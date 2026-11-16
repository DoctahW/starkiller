#include "combate.h"
#include "raymath.h"

/* Índice do primeiro slot livre a partir de `de`, ou -1. */
static int proximoLivre(const Jogo *g, int de)
{
    int i;

    for (i = de; i < MAX_LASERS; i++)
        if (!g->lasers[i].ativo)
            return i;
    return -1;
}

static void ativarLaser(Laser *l, Vector3 pos, Vector3 vel)
{
    l->pos = pos;
    l->vel = vel;
    l->vida = VIDA_LASER;
    l->ativo = 1;
    l->doJogador = 1;
    l->dano = DANO_LASER;
}

int dispararLaser(Jogo *g)
{
    Jogador *j = &g->jogador;
    int a, b;
    Vector3 base, deslocamento, vel;

    if (j->cooldownLaser > 0.0f)
        return 0;

    a = proximoLivre(g, 0);
    b = (a < 0) ? -1 : proximoLivre(g, a + 1);
    if (b < 0)
        return 0;

    base = Vector3Add(j->pos, Vector3Scale(j->frente, CANHAO_FRENTE));
    deslocamento = Vector3Scale(j->direita, CANHAO_LATERAL);
    vel = Vector3Scale(j->frente, VEL_LASER);

    ativarLaser(&g->lasers[a], Vector3Subtract(base, deslocamento), vel);   /* canhão esquerdo */
    ativarLaser(&g->lasers[b], Vector3Add(base, deslocamento), vel);        /* canhão direito */
    j->cooldownLaser = COOLDOWN_LASER;
    return 1;
}

void atualizarLasers(Jogo *g, float dt)
{
    int i;

    g->jogador.cooldownLaser -= dt;
    if (g->jogador.cooldownLaser < 0.0f)
        g->jogador.cooldownLaser = 0.0f;

    for (i = 0; i < MAX_LASERS; i++) {
        Laser *l = &g->lasers[i];

        if (!l->ativo)
            continue;
        l->pos = Vector3Add(l->pos, Vector3Scale(l->vel, dt));
        l->vida -= dt;
        if (l->vida <= 0.0f)
            l->ativo = 0;
    }
}
