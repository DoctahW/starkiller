#include <math.h>

#include "ia.h"
#include "inimigos.h"
#include "raymath.h"

/* Aceleração com que o rumo é corrigido: sem virar no mesmo frame, como pede a IA-01. */
#define ACELERACAO_IA 120.0f
#define DIST_MINIMA   1e-4f

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

void ia_atualizar(Inimigo *in, Jogo *g, float dt)
{
    Vector3 rumo = Vector3Subtract(alvoDoEstado(in, g), in->pos);
    float dist = Vector3Length(rumo);
    Vector3 desejada = { 0.0f, 0.0f, 0.0f };

    if (dist > DIST_MINIMA)
        desejada = Vector3Scale(rumo, velocidadeMaxima(in->tipo) / dist);

    in->vel = Vector3MoveTowards(in->vel, desejada, ACELERACAO_IA * dt);
    in->pos = Vector3Add(in->pos, Vector3Scale(in->vel, dt));
}
