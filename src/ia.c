#include <math.h>
#include <stddef.h>

#include "combate.h"
#include "destroier.h"
#include "ia.h"
#include "inimigos.h"
#include "jogo.h"
#include "nave.h"
#include "raymath.h"

/* Distância mínima para normalizar o rumo; evita divisão por zero com o alvo em cima. */
#define DIST_MINIMA   1e-4f

/* IA-02: o Y-Wing bombardeia a menos de 685 do Destróier (no tamanho canônico) e orbita a ~550.
 * As duas distâncias acompanham a escala do Destróier no jogo (ESCALA_DESTROIER). */
#define ALCANCE_BOMBA_YWING (685.0f * ESCALA_DESTROIER)
#define DANO_BOMBA_YWING    24.0f   /* 24/s em 300 HP mantém os ~12,5 s do desenho de 100 HP */
#define RAIO_ORBITA_YWING   (550.0f * ESCALA_DESTROIER)
#define AVANCO_ORBITA       (185.0f * ESCALA_DESTROIER)
/* O Destróier é um alvo de 6400 u: os caças atiram nele de mais longe do que no TIE. */
#define ALCANCE_TIRO_DESTROIER (1840.0f * ESCALA_DESTROIER)

static float velocidadeMaxima(TipoNave tipo)
{
    switch (tipo) {
    case YWING:
        return VEL_YWING;
    case AWING:
        return VEL_AWING;
    case XWING:
    default:
        return VEL_XWING;
    }
}

/* IA-05: progresso da partida em [0, 1]; fora do intervalo, fica nos extremos. */
float progressoDificuldade(float tempoDecorrido)
{
    float progresso = tempoDecorrido / DURACAO_PARTIDA;

    if (progresso < 0.0f)
        return 0.0f;
    if (progresso > 1.0f)
        return 1.0f;
    return progresso;
}

/* IA-05: velocidade dos caças sobe de 1,0 para FATOR_VELOCIDADE_MAX. */
float fatorVelocidadeIA(float tempoDecorrido)
{
    return 1.0f + (FATOR_VELOCIDADE_MAX - 1.0f) * progressoDificuldade(tempoDecorrido);
}

/* IA-05: cooldown de tiro cai de 1,0 para FATOR_COOLDOWN_MIN. */
float fatorCooldownIA(float tempoDecorrido)
{
    return 1.0f + (FATOR_COOLDOWN_MIN - 1.0f) * progressoDificuldade(tempoDecorrido);
}

/*
 * IA-03: estado do X-Wing derivado do tempo da partida e do id (sem campo novo no Inimigo).
 * O período de cada estado é 8–12 s; o deslocamento por id evita que todos troquem juntos.
 * A fase vem do relógio da partida (a struct não guarda o tempo de spawn), então a primeira
 * troca de um caça que nasce no meio do ciclo pode ser menor que 8 s; daí em diante o
 * intervalo é sempre o período.
 */
static EstadoIA estadoDoXWing(const Inimigo *in, const Jogo *g)
{
    float metade = TROCA_XWING_MIN + (float)(in->id % TROCA_XWING_VARIACAO);
    float fase = fmodf(g->tempoDecorrido + (float)(in->id % 7), 2.0f * metade);

    return fase < metade ? IA_ATACAR_DESTROIER : IA_ATACAR_JOGADOR;
}

/* Posição do alvo de um estado de ataque: o jogador ou o Destróier, fixo na origem (tabela 2.2). */
static Vector3 posicaoDoAlvo(EstadoIA estado, const Jogo *g)
{
    if (estado == IA_ATACAR_JOGADOR)
        return g->jogador.pos;
    return (Vector3){ 0.0f, 0.0f, 0.0f };
}

/* Estado de ataque a que o caça volta depois de fugir: o X-Wing segue a alternância da IA-03. */
static EstadoIA estadoDeAtaque(const Inimigo *in, const Jogo *g)
{
    switch (in->tipo) {
    case XWING:
        return estadoDoXWing(in, g);
    case AWING:
        return IA_ATACAR_JOGADOR;
    case YWING:
    default:
        return IA_ATACAR_DESTROIER;
    }
}

/*
 * Passagem de ataque: perto do alvo o caça rompe (IA_FUGIR) em vez de voar até o ponto dele, e
 * longe o bastante volta a atacar. O Y-Wing em bombardeio orbita e não rompe. Sem campo novo no
 * Inimigo: a referência da fuga é o alvo do estado de ataque do tipo.
 */
static void atualizarPassagem(Inimigo *in, const Jogo *g)
{
    if (in->estadoIA == IA_FUGIR) {
        EstadoIA ataque = estadoDeAtaque(in, g);
        float retomar = ataque == IA_ATACAR_JOGADOR ? DIST_RETOMAR_JOGADOR : DIST_RETOMAR_DESTROIER;

        if (Vector3Distance(in->pos, posicaoDoAlvo(ataque, g)) > retomar)
            in->estadoIA = ataque;
        return;
    }

    if (in->tipo == XWING)
        in->estadoIA = estadoDoXWing(in, g);
    if (in->tipo == YWING && in->estadoIA == IA_ATACAR_DESTROIER)
        return;
    {
        float romper = in->estadoIA == IA_ATACAR_JOGADOR ? DIST_ROMPER_JOGADOR : DIST_ROMPER_DESTROIER;

        if (Vector3Distance(in->pos, posicaoDoAlvo(in->estadoIA, g)) < romper)
            in->estadoIA = IA_FUGIR;
    }
}

/* Unitário perpendicular a `eixo`; o lado depende do id para os caças não romperem todos juntos. */
static Vector3 perpendicular(Vector3 eixo, int id)
{
    Vector3 apoio = fabsf(eixo.y) < 0.9f ? (Vector3){ 0.0f, 1.0f, 0.0f } : (Vector3){ 1.0f, 0.0f, 0.0f };
    Vector3 lado = Vector3Normalize(Vector3CrossProduct(eixo, apoio));

    return id % 2 ? Vector3Negate(lado) : lado;
}

/*
 * Rumo unitário da fuga, ou zero sem direção (caça exatamente sobre a referência). Para longe da
 * referência; vindo de frente, abre também para o lado, então curva em vez de frear contra o
 * alvo. Perto da borda da arena, não aponta para fora: desliza ao longo dela.
 */
static Vector3 rumoDeFuga(const Inimigo *in, Vector3 referencia)
{
    Vector3 longe = Vector3Subtract(in->pos, referencia);
    float distBorda = Vector3Length(in->pos);
    Vector3 rumo;

    if (Vector3Length(longe) < DIST_MINIMA)
        return (Vector3){ 0.0f, 0.0f, 0.0f };
    longe = Vector3Normalize(longe);
    rumo = longe;

    if (Vector3Length(in->vel) > DIST_MINIMA) {
        Vector3 frente = Vector3Normalize(in->vel);
        float entrada = -Vector3DotProduct(frente, longe);

        if (entrada > 0.0f) {
            Vector3 lado = Vector3Add(frente, Vector3Scale(longe, entrada));

            lado = Vector3Length(lado) > 0.1f ? Vector3Normalize(lado) : perpendicular(longe, in->id);
            rumo = Vector3Add(rumo, Vector3Scale(lado, entrada));
        }
    }

    if (distBorda > RAIO_ARENA - MARGEM_BORDA_FUGA) {
        Vector3 radial = Vector3Scale(in->pos, 1.0f / distBorda);
        float saida = Vector3DotProduct(rumo, radial);

        if (saida > 0.0f)
            rumo = Vector3Subtract(rumo, Vector3Scale(radial, saida));
        if (Vector3Length(rumo) < 1e-3f)
            rumo = perpendicular(radial, in->id);
    }
    return Vector3Normalize(rumo);
}

/*
 * Alvo do Y-Wing em bombardeio: um ponto do anel no plano z = 0, perpendicular ao eixo
 * do casco, deslocado no sentido tangencial. Nesse plano o anel fica fora do casco em
 * qualquer ângulo; longe da altura do anel, o caça sobe ou desce até ele.
 */
static Vector3 alvoOrbita(const Inimigo *in)
{
    Vector3 radial = { in->pos.x, in->pos.y, 0.0f };
    float dist = Vector3Length(radial);
    const Vector3 eixo = { 0.0f, 0.0f, 1.0f };
    Vector3 tangente;

    if (dist < DIST_MINIMA)
        radial = (Vector3){ 1.0f, 0.0f, 0.0f };
    else
        radial = Vector3Scale(radial, 1.0f / dist);

    tangente = Vector3CrossProduct(eixo, radial);
    return Vector3Add(Vector3Scale(radial, RAIO_ORBITA_YWING), Vector3Scale(tangente, AVANCO_ORBITA));
}

/*
 * IA-04: o caça não fica dentro do casco. A API é a mesma da colisão do jogador
 * (afastarDoDestroier, de destroier.h). O empurrão é a normal aproximada da superfície,
 * então tira de `vel` a componente que aponta para dentro e o caça desliza no casco.
 */
static void desviarCasco(Inimigo *in)
{
    Vector3 antes = in->pos;
    Vector3 empurrao;
    float dist;

    if (!afastarDoDestroier(&in->pos, raioInimigo(in->tipo)))
        return;

    empurrao = Vector3Subtract(in->pos, antes);
    dist = Vector3Length(empurrao);
    if (dist > 1e-6f) {
        Vector3 normal = Vector3Scale(empurrao, 1.0f / dist);
        float entrada = Vector3DotProduct(in->vel, normal);

        if (entrada < 0.0f)
            in->vel = Vector3Subtract(in->vel, Vector3Scale(normal, entrada));
    }
}

/*
 * IA-03: X-Wing e A-Wing atiram quando o alvo está a menos de ALCANCE_TIRO_INIMIGO e dentro
 * do cone de 15° à frente, respeitando `cooldownTiro` (base × fator de dificuldade da IA-05).
 * O alvo é o do estado atual; fugir não atira.
 */
static void atirarSeNoCone(Inimigo *in, Jogo *g, float dt)
{
    Vector3 alvo, rumo;
    float dist, cosseno;
    float alcance = in->estadoIA == IA_ATACAR_DESTROIER ? ALCANCE_TIRO_DESTROIER : ALCANCE_TIRO_INIMIGO;

    if (in->cooldownTiro > 0.0f) {
        in->cooldownTiro -= dt;
        if (in->cooldownTiro < 0.0f)
            in->cooldownTiro = 0.0f;
    }
    if (in->tipo == YWING || in->estadoIA == IA_FUGIR)
        return;
    if (in->cooldownTiro > 0.0f || Vector3LengthSqr(in->vel) < 1e-6f)
        return;

    if (in->estadoIA == IA_ATACAR_JOGADOR)
        alvo = g->jogador.pos;
    else
        alvo = (Vector3){ 0.0f, 0.0f, 0.0f };

    rumo = Vector3Subtract(alvo, in->pos);
    dist = Vector3Length(rumo);
    if (dist > alcance || dist < 1e-6f)
        return;

    cosseno = Vector3DotProduct(Vector3Normalize(in->vel), Vector3Scale(rumo, 1.0f / dist));
    if (cosseno < COSSENO_CONE_TIRO)
        return;

    if (dispararLaserInimigo(g, in))
        in->cooldownTiro = COOLDOWN_TIRO_INIMIGO * fatorCooldownIA(g->tempoDecorrido);
}

/* IA-02: 8 de dano/s no casco do Destróier enquanto o Y-Wing estiver no alcance. */
static void bombardearDestroier(Inimigo *in, Jogo *g, float dt)
{
    if (in->tipo != YWING || in->estadoIA != IA_ATACAR_DESTROIER)
        return;
    if (Vector3Length(in->pos) >= ALCANCE_BOMBA_YWING)
        return;

    g->cascoDestroier -= DANO_BOMBA_YWING * dt;
    if (g->cascoDestroier < 0.0f)
        g->cascoDestroier = 0.0f;
}

/*
 * Separação: soma de empurrões para longe dos outros caças a menos de DIST_SEPARACAO, mais
 * forte quanto mais perto. Sem ela, quem persegue o mesmo alvo ou orbita o mesmo anel se empilha.
 */
static Vector3 separacao(const Inimigo *in, const Jogo *g)
{
    Vector3 soma = { 0.0f, 0.0f, 0.0f };

    for (const NoInimigo *no = g->inimigos.inicio; no != NULL; no = no->prox) {
        Vector3 longe;
        float dist;

        if (&no->dado == in)
            continue;
        longe = Vector3Subtract(in->pos, no->dado.pos);
        dist = Vector3Length(longe);
        if (dist >= DIST_SEPARACAO)
            continue;
        if (dist < DIST_MINIMA)
            longe = perpendicular((Vector3){ 0.0f, 0.0f, 1.0f }, in->id);
        else
            longe = Vector3Scale(longe, 1.0f / dist);
        soma = Vector3Add(soma, Vector3Scale(longe, PESO_SEPARACAO * (1.0f - dist / DIST_SEPARACAO)));
    }
    return soma;
}

void ia_atualizar(Inimigo *in, Jogo *g, float dt)
{
    float fatorVel = fatorVelocidadeIA(g->tempoDecorrido);
    Vector3 rumo = { 0.0f, 0.0f, 0.0f };
    Vector3 desejada;

    atualizarPassagem(in, g);

    if (in->estadoIA == IA_FUGIR) {
        rumo = rumoDeFuga(in, posicaoDoAlvo(estadoDeAtaque(in, g), g));
    } else {
        Vector3 alvo = in->tipo == YWING && in->estadoIA == IA_ATACAR_DESTROIER
                           ? alvoOrbita(in)
                           : posicaoDoAlvo(in->estadoIA, g);
        Vector3 ate = Vector3Subtract(alvo, in->pos);
        float dist = Vector3Length(ate);

        if (dist > DIST_MINIMA)
            rumo = Vector3Scale(ate, 1.0f / dist);
    }
    rumo = Vector3Add(rumo, separacao(in, g));
    if (Vector3Length(rumo) > 1.0f)
        rumo = Vector3Normalize(rumo);
    desejada = Vector3Scale(rumo, velocidadeMaxima(in->tipo) * fatorVel);

    /* O quadrado do fator mantém o raio de curva (v²/a) constante ao longo da partida. */
    in->vel = Vector3MoveTowards(in->vel, desejada, ACELERACAO_IA * fatorVel * fatorVel * dt);
    in->pos = Vector3Add(in->pos, Vector3Scale(in->vel, dt));
    desviarCasco(in);
    atirarSeNoCone(in, g, dt);
    bombardearDestroier(in, g, dt);
}
