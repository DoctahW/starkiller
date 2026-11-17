#include <math.h>

#include "ia.h"
#include "inimigos.h"
#include "raymath.h"

/* Aceleração com que o rumo é corrigido: sem virar no mesmo frame, como pede a IA-01. */
#define ACELERACAO_IA 550.0f
#define DIST_MINIMA   1e-4f

/* IA-02: o Y-Wing bombardeia a menos de 685 do Destróier e orbita a ~550. */
#define ALCANCE_BOMBA_YWING 685.0f
#define DANO_BOMBA_YWING    8.0f
#define RAIO_ORBITA_YWING   550.0f
#define AVANCO_ORBITA       185.0f
#define FOLGA_ORBITA        90.0f   /* além do anel por mais que isto, volta direto para ele */

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
 * Alvo do Y-Wing em bombardeio: um ponto do anel de órbita em volta do Destróier,
 * avançado no sentido tangencial. Longe do anel, volta direto para o raio de órbita.
 */
static Vector3 alvoOrbita(const Inimigo *in)
{
    Vector3 raio = in->pos;
    float dist = Vector3Length(raio);
    Vector3 eixo, tangente;

    if (dist < DIST_MINIMA)
        return (Vector3){ RAIO_ORBITA_YWING, 0.0f, 0.0f };

    raio = Vector3Scale(raio, 1.0f / dist);
    if (dist > RAIO_ORBITA_YWING + FOLGA_ORBITA)
        return Vector3Scale(raio, RAIO_ORBITA_YWING);

    eixo = fabsf(raio.y) > 0.9f ? (Vector3){ 1.0f, 0.0f, 0.0f } : (Vector3){ 0.0f, 1.0f, 0.0f };
    tangente = Vector3Normalize(Vector3CrossProduct(eixo, raio));
    return Vector3Add(Vector3Scale(raio, RAIO_ORBITA_YWING), Vector3Scale(tangente, AVANCO_ORBITA));
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
    Vector3 desejada = { 0.0f, 0.0f, 0.0f };

    if (dist > DIST_MINIMA)
        desejada = Vector3Scale(rumo, velocidadeMaxima(in->tipo) / dist);

    in->vel = Vector3MoveTowards(in->vel, desejada, ACELERACAO_IA * dt);
    in->pos = Vector3Add(in->pos, Vector3Scale(in->vel, dt));
    bombardearDestroier(in, g, dt);
}
