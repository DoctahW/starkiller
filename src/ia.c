#include <math.h>
#include <stddef.h>

#include "aliados.h"
#include "combate.h"
#include "destroier.h"
#include "ia.h"
#include "inimigos.h"
#include "jogo.h"
#include "nave.h"
#include "raymath.h"

/* Distância mínima para normalizar o rumo; evita divisão por zero com o alvo em cima. */
#define DIST_MINIMA   1e-4f

/* O Destróier é um alvo de 6400 u: os caças atiram nele de mais longe do que no TIE. */
#define ALCANCE_TIRO_DESTROIER (1840.0f * ESCALA_DESTROIER)

#define MAX_ESCOLTA 64

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

/* IA-05: cooldown de tiro cai de 1,0 para FATOR_COOLDOWN_MIN (mantido para testes). */
float fatorCooldownIA(float tempoDecorrido)
{
    return 1.0f + (FATOR_COOLDOWN_MIN - 1.0f) * progressoDificuldade(tempoDecorrido);
}

Vector3 ywingPontoEstagio(int id)
{
    float lateral = ((float)(id % 3) - 1.0f) * 180.0f;
    float altura = YWING_ALTURA_ESTAGIO + (float)(id % 2) * 60.0f;

    return (Vector3){ lateral, altura,
                      Z_PROA_DESTROIER * ESCALA_DESTROIER + YWING_DIST_ESTAGIO };
}

static Inimigo *inimigoPorId(Jogo *g, int id)
{
    if (id < 0)
        return NULL;
    for (NoInimigo *no = g->inimigos.inicio; no; no = no->prox)
        if (no->dado.id == id)
            return &no->dado;
    return NULL;
}

static int jogadorVivo(const Jogo *g)
{
    return g->jogador.casco > 0.0f && !g->jogador.morto;
}

static Vector3 posicaoPatrulha(const Inimigo *in, const Jogo *g)
{
    float raio = in->tipo == AWING ? 6000.0f : 4500.0f;
    float ang = 2.0f * PI * (float)(in->id % 7) / 7.0f + g->tempoDecorrido * 0.10f;

    return (Vector3){ cosf(ang) * raio, 700.0f + (float)(in->id % 3) * 250.0f, sinf(ang) * raio };
}

static Vector3 pontoEscolta(const Inimigo *in, const Jogo *g)
{
    const Inimigo *y = inimigoPorId((Jogo *)g, in->escoltaYwing);

    if (!y)
        y = inimigoPorId((Jogo *)g, in->escoltaYwing2);
    if (!y)
        return posicaoPatrulha(in, g);
    return Vector3Add(y->pos, (Vector3){ 0.0f, 220.0f, 320.0f });
}

/* Posição do alvo: jogador, TIE aliado, ponto de escolta/patrulha ou Destróier, fixo na origem. */
static Vector3 posicaoDoAlvo(const Inimigo *in, EstadoIA estado, const Jogo *g)
{
    if (estado == IA_ATACAR_JOGADOR)
        return g->jogador.pos;
    if (estado == IA_ATACAR_ALIADO && in->alvoAliado >= 0 && in->alvoAliado < MAX_ALIADOS
        && g->aliados[in->alvoAliado].ativo)
        return g->aliados[in->alvoAliado].pos;
    if (estado == IA_ESCOLTAR)
        return pontoEscolta(in, g);
    if (estado == IA_PATRULHAR)
        return posicaoPatrulha(in, g);
    return (Vector3){ 0.0f, 0.0f, 0.0f };
}

/* Sorteio determinístico de foco no Destróier: 10% das janelas de 5 s por caça. */
static int rolarFocoDestroier(const Inimigo *in, const Jogo *g)
{
    unsigned int janela = (unsigned int)(g->tempoDecorrido / 5.0f);
    unsigned int h = (unsigned int)in->id * 2654435761u + janela * 2246822519u;

    h ^= h >> 13;
    h *= 3266489917u;
    return (h % 100u) < 10u;
}

/*
 * Alvo TIE mais próximo (jogador = -1, aliado = índice). A histerese mantém o alvo atual
 * enquanto ele não estiver muito mais longe que o vizinho, para o caça não trocar a cada frame.
 */
static int alvoTieMaisProximo(const Inimigo *in, const Jogo *g)
{
    int melhor = -2;
    float melhorD2 = ALCANCE_ALVO_TIE * ALCANCE_ALVO_TIE;

    if (jogadorVivo(g)) {
        float d2 = Vector3LengthSqr(Vector3Subtract(in->pos, g->jogador.pos));

        if (d2 < melhorD2) {
            melhorD2 = d2;
            melhor = -1;
        }
    }
    for (int i = 0; i < MAX_ALIADOS; i++) {
        if (!g->aliados[i].ativo)
            continue;
        {
            float d2 = Vector3LengthSqr(Vector3Subtract(in->pos, g->aliados[i].pos));

            if (d2 < melhorD2) {
                melhorD2 = d2;
                melhor = i;
            }
        }
    }
    return melhor;
}

static float distanciaAlvoAtual(const Inimigo *in, const Jogo *g)
{
    if (in->estadoAtaque == IA_ATACAR_JOGADOR && jogadorVivo(g))
        return Vector3Distance(in->pos, g->jogador.pos);
    if (in->estadoAtaque == IA_ATACAR_ALIADO && in->alvoAliado >= 0 && in->alvoAliado < MAX_ALIADOS
        && g->aliados[in->alvoAliado].ativo)
        return Vector3Distance(in->pos, g->aliados[in->alvoAliado].pos);
    return 1e30f;
}

static void definirAlvoTie(Inimigo *in, int alvo)
{
    in->alvoAliado = alvo;
    in->focoDestroier = 0;
    in->estadoIA = in->estadoAtaque = alvo < 0 ? IA_ATACAR_JOGADOR : IA_ATACAR_ALIADO;
}

/* Escolhe TIE mais próximo, ou patrola; X-Wing livre pode trocar para o Destróier com 40%. */
static void escolherAlvoTie(Inimigo *in, Jogo *g)
{
    int proximo = alvoTieMaisProximo(in, g);
    float distAtual = distanciaAlvoAtual(in, g);

    if (proximo == -2) {
        if (in->tipo == XWING && g->escudoDestroier <= 0.0f && rolarFocoDestroier(in, g)) {
            in->alvoAliado = -1;
            in->focoDestroier = 1;
            in->estadoIA = in->estadoAtaque = IA_ATACAR_DESTROIER;
        } else {
            in->alvoAliado = -1;
            in->focoDestroier = 0;
            in->estadoIA = in->estadoAtaque = IA_PATRULHAR;
        }
    } else if (in->estadoAtaque != IA_ATACAR_JOGADOR && in->estadoAtaque != IA_ATACAR_ALIADO) {
        definirAlvoTie(in, proximo);
    } else {
        float distProximo = proximo < 0 ? Vector3Distance(in->pos, g->jogador.pos)
                                        : Vector3Distance(in->pos, g->aliados[proximo].pos);

        if (distAtual > distProximo * 1.35f + 250.0f)
            definirAlvoTie(in, proximo);
        else if (in->estadoAtaque == IA_ATACAR_JOGADOR && !jogadorVivo(g))
            definirAlvoTie(in, proximo);
        else if (in->estadoAtaque == IA_ATACAR_ALIADO
                 && (in->alvoAliado < 0 || in->alvoAliado >= MAX_ALIADOS
                     || !g->aliados[in->alvoAliado].ativo))
            definirAlvoTie(in, proximo);
    }

    /* X-Wing livre só desvia o foco para o Destróier com o escudo zerado e no sorteio. */
    if (in->tipo == XWING && in->estadoAtaque != IA_ATACAR_DESTROIER
        && g->escudoDestroier <= 0.0f && rolarFocoDestroier(in, g)) {
        in->alvoAliado = -1;
        in->focoDestroier = 1;
        in->estadoIA = in->estadoAtaque = IA_ATACAR_DESTROIER;
    }
}

static int aliadoMirandoYwing(const Jogo *g, int id, int *indice)
{
    for (int i = 0; i < MAX_ALIADOS; i++)
        if (g->aliados[i].ativo && g->aliados[i].alvoId == id) {
            if (indice)
                *indice = i;
            return 1;
        }
    return 0;
}

/* Um X-Wing de escolta intercepta quem estiver travando um dos Y-Wings do seu par. */
static void atualizarEscolta(Inimigo *in, Jogo *g)
{
    const Inimigo *ywings[2];
    int lockerAliado = -1;
    int lockerJogador = 0;
    float melhorDist = 1e30f;

    ywings[0] = inimigoPorId(g, in->escoltaYwing);
    ywings[1] = inimigoPorId(g, in->escoltaYwing2);
    if (!ywings[0] && !ywings[1]) {
        in->escoltaYwing = in->escoltaYwing2 = -1;
        escolherAlvoTie(in, g);
        return;
    }

    for (int k = 0; k < 2; k++) {
        const Inimigo *y = ywings[k];
        int indice;

        if (!y)
            continue;
        if (jogadorVivo(g) && g->inimigos.alvo != NULL && g->inimigos.alvo->dado.id == y->id)
            lockerJogador = 1;
        if (aliadoMirandoYwing(g, y->id, &indice)) {
            float d = Vector3Distance(in->pos, g->aliados[indice].pos);

            if (d < melhorDist) {
                melhorDist = d;
                lockerAliado = indice;
            }
        }
    }

    if (lockerJogador) {
        in->alvoAliado = -1;
        in->focoDestroier = 0;
        in->estadoIA = in->estadoAtaque = IA_ATACAR_JOGADOR;
        return;
    }
    if (lockerAliado >= 0) {
        in->alvoAliado = lockerAliado;
        in->focoDestroier = 0;
        in->estadoIA = in->estadoAtaque = IA_ATACAR_ALIADO;
        return;
    }

    /* Sem trava: se um TIE chegar perto do Y-Wing, ele vira a ameaça; senão, acompanha. */
    {
        const Inimigo *y = ywings[0] ? ywings[0] : ywings[1];
        int ameaca = -2;
        float melhor = ALCANCE_AMEACA_ESCOLTA * ALCANCE_AMEACA_ESCOLTA;

        if (jogadorVivo(g)) {
            float d2 = Vector3LengthSqr(Vector3Subtract(g->jogador.pos, y->pos));

            if (d2 < melhor) {
                melhor = d2;
                ameaca = -1;
            }
        }
        for (int i = 0; i < MAX_ALIADOS; i++)
            if (g->aliados[i].ativo) {
                float d2 = Vector3LengthSqr(Vector3Subtract(g->aliados[i].pos, y->pos));

                if (d2 < melhor) {
                    melhor = d2;
                    ameaca = i;
                }
            }
        if (ameaca != -2) {
            definirAlvoTie(in, ameaca);
            return;
        }
    }

    in->alvoAliado = -1;
    in->focoDestroier = 0;
    in->estadoIA = in->estadoAtaque = IA_ESCOLTAR;
}

/*
 * Atribui até ceil(Y/2) X-Wings para cobrir pares de Y-Wings. A varredura é determinística:
 * o primeiro par de Y-Wings recebe o primeiro X-Wing disponível, e assim por diante.
 */
void ia_atribuirEscolta(Jogo *g)
{
    Inimigo *ywings[MAX_ESCOLTA];
    Inimigo *xwings[MAX_ESCOLTA];
    int nY = 0, nX = 0, necessario;

    for (NoInimigo *no = g->inimigos.inicio; no; no = no->prox) {
        Inimigo *in = &no->dado;

        if (in->tipo == YWING && nY < MAX_ESCOLTA)
            ywings[nY++] = in;
        else if (in->tipo == XWING && nX < MAX_ESCOLTA)
            xwings[nX++] = in;
    }

    necessario = (nY + 1) / 2;
    for (int i = 0; i < nX; i++)
        xwings[i]->escoltaYwing = xwings[i]->escoltaYwing2 = -1;
    for (int i = 0; i < necessario && i < nX; i++) {
        xwings[i]->escoltaYwing = ywings[2 * i]->id;
        xwings[i]->escoltaYwing2 = 2 * i + 1 < nY ? ywings[2 * i + 1]->id : -1;
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
 * Passagem de ataque: perto do alvo o caça rompe (IA_FUGIR) em vez de voar até o ponto dele, e
 * longe o bastante volta a atacar. Y-Wings têm o próprio ciclo de bombardeio; escolta e patrulha
 * não rompem.
 */
static void atualizarPassagem(Inimigo *in, const Jogo *g)
{
    if (in->estadoIA == IA_ESCOLTAR || in->estadoIA == IA_PATRULHAR)
        return;

    if (in->estadoIA == IA_FUGIR) {
        EstadoIA ataque = in->estadoAtaque;
        float retomar = ataque == IA_ATACAR_DESTROIER ? DIST_RETOMAR_DESTROIER : DIST_RETOMAR_JOGADOR;

        if (ataque == IA_ESCOLTAR || ataque == IA_PATRULHAR)
            in->estadoIA = ataque;
        else if (Vector3Distance(in->pos, posicaoDoAlvo(in, ataque, g)) > retomar)
            in->estadoIA = ataque;
        return;
    }

    if (in->estadoIA == IA_ATACAR_JOGADOR || in->estadoIA == IA_ATACAR_ALIADO
        || in->estadoIA == IA_ATACAR_DESTROIER) {
        float romper = in->estadoIA != IA_ATACAR_DESTROIER ? DIST_ROMPER_JOGADOR : DIST_ROMPER_DESTROIER;

        if (Vector3Distance(in->pos, posicaoDoAlvo(in, in->estadoIA, g)) < romper) {
            in->estadoAtaque = in->estadoIA;
            in->estadoIA = IA_FUGIR;
        }
    }
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

/* Antecipa o casco: se o rumo atual entra nele, soma a normal de saída ao rumo desejado. */
static Vector3 evitarCasco(const Inimigo *in, Vector3 rumo)
{
    Vector3 frente = Vector3LengthSqr(rumo) > 1e-6f ? Vector3Normalize(rumo)
                                                    : (Vector3){ 0.0f, 0.0f, 1.0f };
    float olhar = 220.0f + Vector3Length(in->vel) * 0.45f;

    for (int i = 1; i <= 2; i++) {
        Vector3 sonda = Vector3Add(in->pos, Vector3Scale(frente, olhar * (float)i * 0.5f));
        Vector3 fora = sonda;

        if (afastarDoDestroier(&fora, raioInimigo(in->tipo))) {
            Vector3 normal = Vector3Normalize(Vector3Subtract(fora, sonda));

            rumo = Vector3Add(rumo, Vector3Scale(normal, 1.6f));
            break;
        }
    }
    return rumo;
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

static float cooldownDoTipo(TipoNave tipo)
{
    switch (tipo) {
    case XWING:
        return COOLDOWN_TIRO_XWING;
    case AWING:
        return COOLDOWN_TIRO_AWING;
    case YWING:
    default:
        return COOLDOWN_TIRO_YWING;
    }
}

static Vector3 pontoDeMira(const Inimigo *in, const Jogo *g)
{
    if (in->estadoIA == IA_ATACAR_JOGADOR)
        return pontoDeInterceptacao(in->pos, VEL_LASER_INIMIGO, g->jogador.pos,
                                    Vector3Scale(g->jogador.frente, g->jogador.velocidade));
    if (in->estadoIA == IA_ATACAR_ALIADO && in->alvoAliado >= 0 && in->alvoAliado < MAX_ALIADOS
        && g->aliados[in->alvoAliado].ativo)
        return pontoDeInterceptacao(in->pos, VEL_LASER_INIMIGO, g->aliados[in->alvoAliado].pos,
                                    g->aliados[in->alvoAliado].vel);
    return (Vector3){ 0.0f, 0.0f, 0.0f };
}

/* X-Wing e A-Wing atiram no cone de 15°, com a cadência própria do tipo. */
static void atirarSeNoCone(Inimigo *in, Jogo *g)
{
    Vector3 alvo, rumo;
    float dist, cosseno;
    float alcance = in->estadoIA == IA_ATACAR_DESTROIER ? ALCANCE_TIRO_DESTROIER : ALCANCE_TIRO_INIMIGO;

    if (in->tipo == YWING || in->estadoIA == IA_FUGIR || in->estadoIA == IA_ESCOLTAR
        || in->estadoIA == IA_PATRULHAR)
        return;
    if (in->cooldownTiro > 0.0f || Vector3LengthSqr(in->vel) < 1e-6f)
        return;

    alvo = pontoDeMira(in, g);
    rumo = Vector3Subtract(alvo, in->pos);
    dist = Vector3Length(rumo);
    if (dist > alcance || dist < 1e-6f)
        return;

    cosseno = Vector3DotProduct(Vector3Normalize(in->vel), Vector3Scale(rumo, 1.0f / dist));
    if (cosseno < COSSENO_CONE_TIRO)
        return;

    if (dispararLaserInimigo(g, in))
        in->cooldownTiro = cooldownDoTipo(in->tipo);
}

/* TIE mais próximo para a defesa do Y-Wing; devolve player = -1, aliado = índice ou -2. */
static int alvoTiePertoYwing(const Inimigo *in, const Jogo *g)
{
    int melhor = -2;
    float melhorD2 = 2500.0f * 2500.0f;

    if (jogadorVivo(g)) {
        float d2 = Vector3LengthSqr(Vector3Subtract(in->pos, g->jogador.pos));

        if (d2 < melhorD2) {
            melhorD2 = d2;
            melhor = -1;
        }
    }
    for (int i = 0; i < MAX_ALIADOS; i++)
        if (g->aliados[i].ativo) {
            float d2 = Vector3LengthSqr(Vector3Subtract(in->pos, g->aliados[i].pos));

            if (d2 < melhorD2) {
                melhorD2 = d2;
                melhor = i;
            }
        }
    return melhor;
}

static void atirarDefesaYwing(Inimigo *in, Jogo *g)
{
    int alvo;
    Vector3 pos, vel;

    if (in->cooldownTiro > TOLERANCIA_COOLDOWN)
        return;
    alvo = alvoTiePertoYwing(in, g);
    if (alvo == -2)
        return;
    if (alvo < 0) {
        pos = g->jogador.pos;
        vel = Vector3Scale(g->jogador.frente, g->jogador.velocidade);
    } else {
        pos = g->aliados[alvo].pos;
        vel = g->aliados[alvo].vel;
    }
    if (dispararLaserYwing(g, in, pos, vel))
        in->cooldownTiro = COOLDOWN_TIRO_YWING;
}

static float distanciaHorizontal(Vector3 p)
{
    return sqrtf(p.x * p.x + p.z * p.z);
}

int ywingBombardeando(const Inimigo *in)
{
    return in->tipo == YWING && in->faseBombardeio == BOMBA_APROXIMAR
        && in->bombasLancadas < YWING_MAX_BOMBAS && distanciaHorizontal(in->pos) <= ALCANCE_BOMBA_YWING;
}

static void bombardearDestroier(Inimigo *in, Jogo *g)
{
    if (!ywingBombardeando(in) || in->cooldownTiro > TOLERANCIA_COOLDOWN)
        return;
    if (distanciaHorizontal(in->pos) > YWING_DIST_SOLTURA)
        return;
    if (dispararTorpedoYwing(g, in)) {
        in->bombasLancadas++;
        in->cooldownTiro = YWING_INTERVALO_BOMBAS;
    }
}

static void iniciarRunYwing(Inimigo *in)
{
    in->faseBombardeio = BOMBA_APROXIMAR;
    in->bombasLancadas = 0;
    in->tempoManobra = 0.0f;
    in->cooldownTiro = 0.0f;
    in->estadoIA = in->estadoAtaque = IA_ATACAR_DESTROIER;
}

static void iniciarRetiradaYwing(Inimigo *in)
{
    in->faseBombardeio = BOMBA_RETIRAR;
    in->tempoManobra = 0.0f;
    in->estadoIA = IA_FUGIR;
    in->estadoAtaque = IA_ATACAR_DESTROIER;
}

/* Separação: soma de empurrões para longe dos outros caças a menos de DIST_SEPARACAO. */
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

/* Movimento comum dos rebeldes: rumo + separação + casco/borda, com aceleração da IA. */
static void moverComRumoVel(Inimigo *in, Jogo *g, Vector3 rumo, float dt, float velocidade)
{
    float fatorVel = fatorVelocidadeIA(g->tempoDecorrido);
    Vector3 desejada;
    float distBorda;

    rumo = Vector3Add(rumo, separacao(in, g));
    rumo = evitarCasco(in, rumo);
    distBorda = Vector3Length(in->pos);
    if (distBorda > RAIO_ARENA - MARGEM_BORDA_FUGA) {
        Vector3 radial = Vector3Scale(in->pos, 1.0f / distBorda);
        float saida = Vector3DotProduct(rumo, radial);

        if (saida > 0.0f)
            rumo = Vector3Subtract(rumo, Vector3Scale(radial, saida));
    }
    if (Vector3Length(rumo) > 1.0f)
        rumo = Vector3Normalize(rumo);
    desejada = Vector3Scale(rumo, velocidade);
    in->vel = Vector3MoveTowards(in->vel, desejada, ACELERACAO_IA * fatorVel * fatorVel * dt);
    in->pos = Vector3Add(in->pos, Vector3Scale(in->vel, dt));
    desviarCasco(in);
}

static void moverComRumo(Inimigo *in, Jogo *g, Vector3 rumo, float dt)
{
    moverComRumoVel(in, g, rumo, dt,
                    velocidadeMaxima(in->tipo) * fatorVelocidadeIA(g->tempoDecorrido));
}

static void atualizarYwing(Inimigo *in, Jogo *g, float dt)
{
    Vector3 estagio = ywingPontoEstagio(in->id);

    in->pontoReagrupamento = estagio;

    if (in->faseBombardeio == BOMBA_REAGRUPAR) {
        in->estadoIA = in->estadoAtaque = IA_ATACAR_DESTROIER;
        atirarDefesaYwing(in, g);
        if (Vector3Distance(in->pos, estagio) <= YWING_RAIO_ESTAGIO) {
            int companheiros = 0, prontos = 0;

            in->tempoManobra += dt;
            for (NoInimigo *no = g->inimigos.inicio; no; no = no->prox) {
                Inimigo *outro = &no->dado;

                if (outro == in || outro->tipo != YWING || outro->grupoBombardeio != in->grupoBombardeio
                    || outro->faseBombardeio != BOMBA_REAGRUPAR)
                    continue;
                companheiros++;
                prontos += Vector3Distance(outro->pos, ywingPontoEstagio(outro->id)) <= YWING_RAIO_ESTAGIO;
            }
            if ((companheiros > 0 && prontos == companheiros) || in->tempoManobra >= YWING_ESPERA_GRUPO) {
                for (NoInimigo *no = g->inimigos.inicio; no; no = no->prox) {
                    Inimigo *outro = &no->dado;

                    if (outro->tipo == YWING && outro->grupoBombardeio == in->grupoBombardeio
                        && outro->faseBombardeio == BOMBA_REAGRUPAR
                        && Vector3Distance(outro->pos, ywingPontoEstagio(outro->id)) <= YWING_RAIO_ESTAGIO)
                        iniciarRunYwing(outro);
                }
                iniciarRunYwing(in);
                return;
            }
        } else {
            in->tempoManobra = 0.0f;
        }
        /* Posicionamento também é velocidade de corrida; fora da run ele volta ao normal. */
        moverComRumoVel(in, g, Vector3Normalize(Vector3Subtract(estagio, in->pos)), dt, YWING_VEL_RUN);
        return;
    }

    if (in->faseBombardeio == BOMBA_APROXIMAR) {
        Vector3 destino = { 0.0f, YWING_ALTURA_RUN, YWING_FIM_RUN_Z };
        Vector3 dir = Vector3Subtract(destino, in->pos);

        in->tempoManobra += dt;
        if (Vector3LengthSqr(dir) < 1e-6f)
            dir = (Vector3){ 0.0f, 0.0f, -1.0f };
        /* A corrida é reta: sem separação, sem desvio de casco e sem frear.
         * O destino baixo faz a nave descer suavemente perto do casco. */
        in->vel = Vector3Scale(Vector3Normalize(dir), YWING_VEL_RUN);
        in->pos = Vector3Add(in->pos, Vector3Scale(in->vel, dt));
        bombardearDestroier(in, g);
        if (in->bombasLancadas >= YWING_MAX_BOMBAS || in->pos.z < YWING_FIM_RUN_Z
            || in->tempoManobra >= YWING_TEMPO_RUN)
            iniciarRetiradaYwing(in);
        return;
    }

    /* BOMBA_RETIRAR: dispersa para longe por 10 s e depois se reposiciona. */
    in->tempoManobra += dt;
    atirarDefesaYwing(in, g);
    if (in->tempoManobra >= YWING_TEMPO_DISPERSAO) {
        in->faseBombardeio = BOMBA_REAGRUPAR;
        in->tempoManobra = 0.0f;
        in->estadoIA = in->estadoAtaque = IA_ATACAR_DESTROIER;
    }
    {
        Vector3 rumo = rumoDeFuga(in, (Vector3){ 0.0f, 0.0f, 0.0f });
        Vector3 extra = perpendicular(rumo, in->id);

        rumo = Vector3Add(rumo, Vector3Scale(extra, 0.55f));
        moverComRumo(in, g, Vector3Normalize(rumo), dt);
    }
}

void ia_atualizar(Inimigo *in, Jogo *g, float dt)
{
    if (in->cooldownTiro > 0.0f) {
        in->cooldownTiro -= dt;
        if (in->cooldownTiro < 0.0f)
            in->cooldownTiro = 0.0f;
    }

    if (in->tipo == YWING) {
        atualizarYwing(in, g, dt);
        return;
    }

    if (in->estadoIA != IA_FUGIR) {
        if (in->tipo == XWING && (in->escoltaYwing >= 0 || in->escoltaYwing2 >= 0))
            atualizarEscolta(in, g);
        else
            escolherAlvoTie(in, g);
    }

    atualizarPassagem(in, g);

    if (in->estadoIA == IA_FUGIR) {
        EstadoIA ataque = in->estadoAtaque;

        if (ataque == IA_ESCOLTAR || ataque == IA_PATRULHAR) {
            in->estadoIA = ataque;
            moverComRumo(in, g, Vector3Normalize(Vector3Subtract(posicaoDoAlvo(in, ataque, g), in->pos)), dt);
        } else {
            Vector3 rumo = rumoDeFuga(in, posicaoDoAlvo(in, ataque, g));

            moverComRumo(in, g, rumo, dt);
        }
    } else {
        Vector3 alvo = posicaoDoAlvo(in, in->estadoIA, g);
        Vector3 ate = Vector3Subtract(alvo, in->pos);
        float dist = Vector3Length(ate);
        Vector3 rumo = dist > DIST_MINIMA ? Vector3Scale(ate, 1.0f / dist) : (Vector3){ 0.0f, 0.0f, 0.0f };

        moverComRumo(in, g, rumo, dt);
    }
    atirarSeNoCone(in, g);
}
