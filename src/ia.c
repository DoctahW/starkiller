#include <math.h>

#include "combate.h"
#include "destroier.h"
#include "ia.h"
#include "inimigos.h"
#include "jogo.h"
#include "raymath.h"

/* Distância mínima para normalizar o rumo; evita divisão por zero com o alvo em cima. */
#define DIST_MINIMA   1e-4f

/* IA-02: o Y-Wing bombardeia a menos de 685 do Destróier e orbita a ~550. */
#define ALCANCE_BOMBA_YWING 685.0f
#define DANO_BOMBA_YWING    8.0f
#define RAIO_ORBITA_YWING   550.0f
#define AVANCO_ORBITA       185.0f

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
 * Alvo do estado atual, na convenção da seção 3.5: o Destróier fica fixo na origem
 * (tabela 2.2) e o jogador é o ponto perseguido ou a referência para fugir.
 */
static Vector3 alvoDoEstado(const Inimigo *in, const Jogo *g)
{
    switch (in->estadoIA) {
    case IA_ATACAR_JOGADOR:
        return g->jogador.pos;
    case IA_FUGIR:
        return Vector3Subtract(Vector3Scale(in->pos, 2.0f), g->jogador.pos);
    case IA_ATACAR_DESTROIER:
    default:
        return (Vector3){ 0.0f, 0.0f, 0.0f };
    }
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

void ia_atualizar(Inimigo *in, Jogo *g, float dt)
{
    Vector3 alvo = in->tipo == YWING && in->estadoIA == IA_ATACAR_DESTROIER
                       ? alvoOrbita(in)
                       : alvoDoEstado(in, g);
    Vector3 rumo = Vector3Subtract(alvo, in->pos);
    float dist = Vector3Length(rumo);
    float fatorVel = fatorVelocidadeIA(g->tempoDecorrido);
    Vector3 desejada = { 0.0f, 0.0f, 0.0f };

    if (dist > DIST_MINIMA)
        desejada = Vector3Scale(rumo, velocidadeMaxima(in->tipo) * fatorVel / dist);

    in->vel = Vector3MoveTowards(in->vel, desejada, ACELERACAO_IA * fatorVel * dt);
    in->pos = Vector3Add(in->pos, Vector3Scale(in->vel, dt));
    desviarCasco(in);
    bombardearDestroier(in, g, dt);
}
