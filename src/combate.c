#include "combate.h"
#include <math.h>
#include <stddef.h>

#include "raymath.h"
#include "pontuacao.h"
#include "destroier.h"
#include "nave.h"
#include "inimigos.h"

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
    j->esperaEscudo = ATRASO_ESCUDO;   /* disparar é estar em combate: o escudo não recarrega */
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

float raioDeAcerto(TipoNave t, float distancia)
{
    return fmaxf(raioInimigo(t), distancia * TOLERANCIA_MIRA);
}

int tiroAcerta(const Jogador *j, const Inimigo *alvo)
{
    Vector3 rel = Vector3Subtract(pontoDeInterceptacao(j->pos, VEL_LASER, alvo->pos, alvo->vel), j->pos);
    float aFrente = Vector3DotProduct(rel, j->frente);
    float r;

    if (aFrente <= 0.0f || aFrente > VEL_LASER * VIDA_LASER)
        return 0;
    /* Os canhões ficam a ±CANHAO_LATERAL do eixo: no pior caso o mais perto passa a sqrt(erro² + lateral²). */
    rel = Vector3Subtract(rel, Vector3Scale(j->frente, aFrente));
    r = raioDeAcerto(alvo->tipo, aFrente);
    return Vector3DotProduct(rel, rel) + CANHAO_LATERAL * CANHAO_LATERAL <= r * r;
}

/*
 * Inimigo de menor t que o segmento do laser (pos - vel * dt → pos) atravessa, ou NULL. O raio de
 * cada caça é raioDeAcerto pela distância dele à boca do canhão, de onde o laser saiu (pela vida gasta).
 */
static NoInimigo *primeiroAtingido(const Jogo *g, const Laser *l, float dt)
{
    Vector3 anterior = Vector3Subtract(l->pos, Vector3Scale(l->vel, dt));
    Vector3 origem = Vector3Subtract(l->pos, Vector3Scale(l->vel, VIDA_LASER - l->vida));
    NoInimigo *no, *primeiro = NULL;
    float melhor = 2.0f;

    for (no = g->inimigos.inicio; no; no = no->prox) {
        float raio = raioDeAcerto(no->dado.tipo, Vector3Distance(no->dado.pos, origem));
        float t;

        if (segmentoAcertaEsfera(anterior, l->pos, no->dado.pos, raio, &t) && t < melhor) {
            melhor = t;
            primeiro = no;
        }
    }
    return primeiro;
}

int colidirLasers(Jogo *g, float dt)
{
    int i, acertos = 0;

    for (i = 0; i < MAX_LASERS; i++) {
        Laser *l = &g->lasers[i];

        if (!l->ativo || !l->doJogador)
            continue;
        if (primeiroAtingido(g, l, dt)) {
            l->ativo = 0;
            acertos++;
        }
    }
    return acertos;
}

/* Abate: pontua, cria a explosão na posição do caça e o tira da lista (que mantém o alvo válido). */
static void abaterInimigo(Jogo *g, NoInimigo *no)
{
    TipoNave tipo = no->dado.tipo;
    Vector3 pos = no->dado.pos;

    registrarAbate(g, tipo);
    criarExplosao(g, pos, RAIO_EXPLOSAO);
    removerInimigo(&g->inimigos, no);
}

int resolverLasers(Jogo *g, float dt)
{
    int i, acertos = 0;

    for (i = 0; i < MAX_LASERS; i++) {
        Laser *l = &g->lasers[i];
        NoInimigo *no;

        if (!l->ativo || !l->doJogador)
            continue;
        no = primeiroAtingido(g, l, dt);
        if (!no)
            continue;
        l->ativo = 0;
        acertos++;
        if (aplicarDano(&no->dado, l->dano))
            abaterInimigo(g, no);
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


float progressoExplosao(const Explosao *e)
{
    float p;

    if (!e->ativa)
        return 0.0f;

    p = e->tempo / DURACAO_EXPLOSAO;
    if (p < 0.0f)
        p = 0.0f;
    if (p > 1.0f)
        p = 1.0f;
    return p;
}

float raioExplosao(const Explosao *e)
{
    float p = progressoExplosao(e);

    return e->raioMax * p * (2.0f - p);   /* ease-out: abre rápido, desacelera no fim */
}

Vector3 pontoDeInterceptacao(Vector3 origem, float velProjetil, Vector3 alvo, Vector3 velAlvo)
{
    /* |rel + v t| = s t; com s > |v| há uma raiz positiva. */
    Vector3 rel = Vector3Subtract(alvo, origem);
    float a = Vector3DotProduct(velAlvo, velAlvo) - velProjetil * velProjetil;
    float b = 2.0f * Vector3DotProduct(rel, velAlvo);
    float c = Vector3DotProduct(rel, rel);
    float disc = b * b - 4.0f * a * c;
    float t;

    if (a >= 0.0f || disc < 0.0f)
        return alvo;
    t = (-b - sqrtf(disc)) / (2.0f * a);
    if (t <= 0.0f)
        return alvo;
    return Vector3Add(alvo, Vector3Scale(velAlvo, t));
}

int dispararLaserInimigo(Jogo *g, const Inimigo *in)
{
    Vector3 alvo, dir;
    int slot;
    Laser *l;

    if (in->estadoIA == IA_ATACAR_JOGADOR)
        alvo = pontoDeInterceptacao(in->pos, VEL_LASER_INIMIGO, g->jogador.pos,
                                    Vector3Scale(g->jogador.frente, g->jogador.velocidade));
    else if (in->estadoIA == IA_ATACAR_DESTROIER)
        alvo = (Vector3){ 0.0f, 0.0f, 0.0f };
    else
        return 0;

    dir = Vector3Subtract(alvo, in->pos);
    if (Vector3Length(dir) < 1e-6f)
        return 0;
    slot = proximoLivre(g, 0);
    if (slot < 0)
        return 0;

    l = &g->lasers[slot];
    ativarLaser(l, in->pos, Vector3Scale(Vector3Normalize(dir), VEL_LASER_INIMIGO));
    l->doJogador = 0;
    l->vida = VIDA_LASER_INIMIGO;
    l->dano = DANO_LASER_INIMIGO;
    return 1;
}

/* Passo de amostragem do segmento no casco: menor que meia célula (8 u), nada é pulado. */
#define PASSO_CASCO 4.0f

/* Fração [0, 1] onde o segmento entra no casco; 0 se já começa dentro. Devolve 0 se não entra. */
static int segmentoEntraNoCasco(Vector3 a, Vector3 b, float *t)
{
    Vector3 d = Vector3Subtract(b, a);
    float comprimento = Vector3Length(d);
    int passos, i;

    if (tocaDestroier(a, 0.0f)) {
        *t = 0.0f;
        return 1;
    }
    if (comprimento < 1e-6f)
        return 0;
    passos = (int)ceilf(comprimento / PASSO_CASCO);

    for (i = 1; i <= passos; i++) {
        float f = (float)i / (float)passos;

        if (tocaDestroier(Vector3Add(a, Vector3Scale(d, f)), 0.0f)) {
            *t = f;
            return 1;
        }
    }
    return 0;
}

int colidirLasersInimigos(Jogo *g, float dt)
{
    int i, acertosTie = 0;

    for (i = 0; i < MAX_LASERS; i++) {
        Laser *l = &g->lasers[i];
        Vector3 anterior;
        float tTie = 2.0f, tCasco = 2.0f, t;

        if (!l->ativo || l->doJogador)
            continue;

        anterior = Vector3Subtract(l->pos, Vector3Scale(l->vel, dt));
        if (segmentoAcertaEsfera(anterior, l->pos, g->jogador.pos, RAIO_JOGADOR, &t))
            tTie = t;
        if (segmentoEntraNoCasco(anterior, l->pos, &t))
            tCasco = t;
        if (tTie > 1.0f && tCasco > 1.0f)
            continue;

        l->ativo = 0;
        if (tTie <= tCasco) {
            danificarJogador(g, l->dano);
            acertosTie++;
        } else {
            g->cascoDestroier -= l->dano * FRACAO_DANO_DESTROIER;
            if (g->cascoDestroier < 0.0f)
                g->cascoDestroier = 0.0f;
        }
    }
    return acertosTie;
}
