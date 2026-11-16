#include "combate.h"
#include <math.h>
#include <stddef.h>

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

    if (j->cooldownLaser > TOLERANCIA_COOLDOWN)   /* sobra de float (~1e-8) não pode custar um frame */
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
        /* Expirado no passo anterior: só agora é desativado, para colidirLasers testar o último passo. */
        if (l->vida <= 0.0f) {
            l->ativo = 0;
            continue;
        }
        l->pos = Vector3Add(l->pos, Vector3Scale(l->vel, dt));
        l->vida -= dt;
    }
}

float raioInimigo(TipoNave t)
{
    switch (t) {
    case XWING: return RAIO_XWING;
    case YWING: return RAIO_YWING;
    case AWING: return RAIO_AWING;
    }
    return RAIO_XWING;
}

int segmentoAcertaEsfera(Vector3 a, Vector3 b, Vector3 centro, float raio, float *t)
{
    Vector3 d = Vector3Subtract(b, a);
    Vector3 f = Vector3Subtract(a, centro);
    float c = Vector3DotProduct(f, f) - raio * raio;
    float qa, qb, disc, entrada;

    if (t)
        *t = 0.0f;
    if (c <= 0.0f)              /* começa dentro da esfera */
        return 1;

    qa = Vector3DotProduct(d, d);
    if (qa == 0.0f)             /* segmento degenerado, fora da esfera */
        return 0;
    qb = 2.0f * Vector3DotProduct(f, d);
    disc = qb * qb - 4.0f * qa * c;
    if (disc < 0.0f)
        return 0;

    entrada = (-qb - sqrtf(disc)) / (2.0f * qa);
    if (entrada < 0.0f || entrada > 1.0f)
        return 0;
    if (t)
        *t = entrada;
    return 1;
}

int colidirLasers(Jogo *g, float dt)
{
    int i, acertos = 0;

    for (i = 0; i < MAX_LASERS; i++) {
        Laser *l = &g->lasers[i];
        Vector3 anterior;
        NoInimigo *no, *primeiro = NULL;
        float melhor = 2.0f;

        if (!l->ativo || !l->doJogador)
            continue;

        anterior = Vector3Subtract(l->pos, Vector3Scale(l->vel, dt));
        for (no = g->inimigos.inicio; no; no = no->prox) {
            float t;

            if (segmentoAcertaEsfera(anterior, l->pos, no->dado.pos, raioInimigo(no->dado.tipo), &t)
                && t < melhor) {
                melhor = t;
                primeiro = no;
            }
        }
        if (primeiro) {
            l->ativo = 0;
            acertos++;
        }
    }
    return acertos;
}

int aplicarDano(Inimigo *in, float dano)
{
    if (dano > 0.0f) {
        if (dano <= in->escudo) {
            in->escudo -= dano;
        } else {
            in->casco -= dano - in->escudo;
            in->escudo = 0.0f;
        }
        if (in->casco < 0.0f)
            in->casco = 0.0f;
    }
    return in->casco <= 0.0f;
}

int criarExplosao(Jogo *g, Vector3 pos, float raioMax)
{
    int i;

    for (i = 0; i < MAX_EXPLOSOES; i++) {
        if (!g->explosoes[i].ativa) {
            g->explosoes[i].pos = pos;
            g->explosoes[i].tempo = 0.0f;
            g->explosoes[i].raioMax = raioMax;
            g->explosoes[i].ativa = 1;
            return 1;
        }
    }
    return 0;
}

void atualizarExplosoes(Jogo *g, float dt)
{
    int i;

    for (i = 0; i < MAX_EXPLOSOES; i++) {
        if (!g->explosoes[i].ativa)
            continue;
        g->explosoes[i].tempo += dt;
        if (g->explosoes[i].tempo >= DURACAO_EXPLOSAO)
            g->explosoes[i].ativa = 0;
    }
}
