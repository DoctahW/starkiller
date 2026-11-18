#include "combate.h"
#include <math.h>
#include <stddef.h>
#include <string.h>

#include "raymath.h"
#include "audio.h"
#include "pontuacao.h"
#include "destroier.h"
#include "nave.h"
#include "inimigos.h"
#include "aliados.h"

static int segmentoEntraNoCasco(Vector3 a, Vector3 b, float *t);

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
    l->doAliado = 0;
    l->torpedo = 0;
    l->tipoOrigem = XWING;
    l->dano = DANO_LASER;
    l->gravidade = 0.0f;
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
    audioTiroJogador();
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
        /* Torpedo e laser do Y-Wing caem em parábola; os demais voam reto (gravidade 0). */
        if (l->gravidade > 0.0f) {
            Vector3 aceleracao = { 0.0f, -l->gravidade, 0.0f };

            l->pos = Vector3Add(l->pos, Vector3Add(Vector3Scale(l->vel, dt),
                                                   Vector3Scale(aceleracao, 0.5f * dt * dt)));
            l->vel = Vector3Add(l->vel, Vector3Scale(aceleracao, dt));
        } else {
            l->pos = Vector3Add(l->pos, Vector3Scale(l->vel, dt));
        }
        l->vida -= dt;
    }
}

/* Posição no início do passo, desfazendo a integração com gravidade (usada na colisão por segmento). */
static Vector3 posicaoAnterior(const Laser *l, float dt)
{
    Vector3 g = { 0.0f, -l->gravidade, 0.0f };

    return Vector3Add(Vector3Subtract(l->pos, Vector3Scale(l->vel, dt)),
                      Vector3Scale(g, 0.5f * dt * dt));
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
    Vector3 anterior = posicaoAnterior(l, dt);
    Vector3 origem = Vector3Subtract(l->pos, Vector3Scale(l->vel, VIDA_LASER - l->vida));
    NoInimigo *no, *primeiro = NULL;
    float melhor = 2.0f;

    for (no = g->inimigos.inicio; no; no = no->prox) {
        float raio = l->doJogador ? raioDeAcerto(no->dado.tipo, Vector3Distance(no->dado.pos, origem))
                                 : raioInimigo(no->dado.tipo);
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

/* Destroços dos caças abatidos: giram na mesma direção e explodem ao fim (SPIN → BOOM). */
static QuedaInimigo quedas[MAX_QUEDAS_INIMIGO];

const QuedaInimigo *quedasInimigas(void)
{
    return quedas;
}

void limparQuedasInimigas(void)
{
    memset(quedas, 0, sizeof quedas);
    audioPararSpins();
}

static float sorteio(float minimo, float maximo)
{
    return minimo + (maximo - minimo) * ((float)GetRandomValue(0, 10000) / 10000.0f);
}

/* Mesma orientação do caça vivo: +Z alinhado à frente, inclusive no caso antiparalelo (-Z). */
static Quaternion rotacaoDaFrente(Vector3 frente)
{
    Vector3 eixo = { -frente.y, frente.x, 0.0f };
    float comprimento = Vector3LengthSqr(eixo);

    if (comprimento < 1e-6f) {
        if (frente.z >= 0.0f)
            return (Quaternion){ 0.0f, 0.0f, 0.0f, 1.0f };
        return QuaternionFromAxisAngle((Vector3){ 0.0f, 1.0f, 0.0f }, PI);
    }
    return QuaternionFromAxisAngle(Vector3Scale(eixo, 1.0f / sqrtf(comprimento)),
                                   acosf(Clamp(frente.z, -1.0f, 1.0f)));
}

/* Cria o destroço no primeiro slot livre. Devolve 0 se o pool estiver cheio. */
static int criarQuedaInimigo(TipoNave tipo, Vector3 pos, Vector3 vel)
{
    int i;

    for (i = 0; i < MAX_QUEDAS_INIMIGO; i++) {
        QuedaInimigo *q = &quedas[i];
        Vector3 eixo;

        if (q->ativo)
            continue;

        q->ativo = 1;
        q->tipo = tipo;
        q->pos = pos;
        q->vel = vel;

        /* A orientação inicial é a mesma do caça vivo: +Z alinhado à velocidade. */
        if (Vector3LengthSqr(vel) > 1e-6f)
            q->rot = rotacaoDaFrente(Vector3Normalize(vel));
        else
            q->rot = (Quaternion){ 0.0f, 0.0f, 0.0f, 1.0f };

        eixo = (Vector3){ sorteio(-1.0f, 1.0f), sorteio(-1.0f, 1.0f), sorteio(-1.0f, 1.0f) };
        if (Vector3LengthSqr(eixo) < 1e-4f)
            eixo = (Vector3){ 1.0f, 0.0f, 0.0f };
        q->eixo = Vector3Normalize(eixo);
        q->giro = sorteio(3.5f, 7.0f);
        q->tempo = 0.0f;
        q->duracao = sorteio(QUEDA_INIMIGO_MIN, QUEDA_INIMIGO_MAX);
        q->vozSpin = audioSpinInimigo(tipo, pos);
        return 1;
    }
    return 0;
}

void atualizarQuedasInimigas(Jogo *g, float dt)
{
    int i;

    for (i = 0; i < MAX_QUEDAS_INIMIGO; i++) {
        QuedaInimigo *q = &quedas[i];

        if (!q->ativo)
            continue;

        q->tempo += dt;
        q->pos = Vector3Add(q->pos, Vector3Scale(q->vel, dt));
        q->rot = QuaternionNormalize(QuaternionMultiply(
            q->rot, QuaternionFromAxisAngle(q->eixo, q->giro * dt)));

        if (q->tempo >= q->duracao) {
            q->ativo = 0;
            criarExplosao(g, q->pos, RAIO_EXPLOSAO);
            audioBoomInimigo(q->tipo, q->pos, q->vozSpin);
            q->vozSpin = -1;
        }
    }
}

/* Abate: pontua só o jogador, cria o destroço girando (explosão/BOOM ao fim) e remove da lista. */
static void abaterInimigo(Jogo *g, NoInimigo *no, int doJogador)
{
    TipoNave tipo = no->dado.tipo;
    Vector3 pos = no->dado.pos;
    Vector3 vel = no->dado.vel;

    if (doJogador)
        registrarAbate(g, tipo);
    if (!criarQuedaInimigo(tipo, pos, vel)) {
        /* Sem slot no pool de destroços: explode na hora, sem o giro visual. */
        criarExplosao(g, pos, RAIO_EXPLOSAO);
        audioBoomInimigo(tipo, pos, -1);
    }
    removerInimigo(&g->inimigos, no);
}

int resolverLasers(Jogo *g, float dt)
{
    int i, acertos = 0;

    for (i = 0; i < MAX_LASERS; i++) {
        Laser *l = &g->lasers[i];
        NoInimigo *no;

        if (!l->ativo || (!l->doJogador && !l->doAliado))
            continue;
        no = primeiroAtingido(g, l, dt);
        if (l->doAliado) {
            Vector3 anterior = posicaoAnterior(l, dt);
            float tCasco, tAlvo = 2.0f;

            if (no)
                segmentoAcertaEsfera(anterior, l->pos, no->dado.pos, raioInimigo(no->dado.tipo), &tAlvo);
            if (segmentoEntraNoCasco(anterior, l->pos, &tCasco) && tCasco <= tAlvo) {
                l->ativo = 0;
                continue;
            }
        }
        if (!no)
            continue;
        l->ativo = 0;
        acertos++;
        if (aplicarDano(&no->dado, l->dano))
            abaterInimigo(g, no, l->doJogador);
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

/*
 * Velocidade inicial para um projétil balístico com componente horizontal `velHorizontal` e
 * gravidade `gravidade` encontrar `alvo`. Devolve 0 sem solução (alvo exatamente em cima/embaixo).
 * A componente vertical pode ser negativa (tiro já caindo) ou positiva (lob), quem chama decide.
 */
static int velocidadeBalistica(Vector3 origem, Vector3 alvo, float velHorizontal, float gravidade,
                               Vector3 *velocidade)
{
    Vector3 rel = Vector3Subtract(alvo, origem);
    float distH = sqrtf(rel.x * rel.x + rel.z * rel.z);
    float t, vy;

    if (distH < 1e-4f || velHorizontal <= 0.0f)
        return 0;
    t = distH / velHorizontal;
    vy = (rel.y + 0.5f * gravidade * t * t) / t;
    *velocidade = (Vector3){ rel.x / t, vy, rel.z / t };
    return 1;
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
    else if (in->estadoIA == IA_ATACAR_ALIADO && in->alvoAliado >= 0 && in->alvoAliado < MAX_ALIADOS
             && g->aliados[in->alvoAliado].ativo)
        alvo = pontoDeInterceptacao(in->pos, VEL_LASER_INIMIGO,
                                    g->aliados[in->alvoAliado].pos, g->aliados[in->alvoAliado].vel);
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
    l->tipoOrigem = in->tipo;
    l->vida = VIDA_LASER_INIMIGO;
    l->dano = DANO_LASER_INIMIGO;
    audioTiroInimigo(in->tipo, in->pos);
    return 1;
}

#define GRAVIDADE_LASER_YWING 500.0f

int dispararLaserYwing(Jogo *g, const Inimigo *in, Vector3 alvo, Vector3 velAlvo)
{
    Vector3 velocidade, previsto = alvo;
    int slot = proximoLivre(g, 0);
    Laser *l;

    if (slot < 0 || in->tipo != YWING)
        return 0;
    /* Duas iterações de antecipação bastam para um alvo que voa reto. */
    for (int i = 0; i < 2; i++) {
        if (!velocidadeBalistica(in->pos, previsto, VEL_LASER_INIMIGO, GRAVIDADE_LASER_YWING,
                                 &velocidade))
            return 0;
        previsto = Vector3Add(alvo, Vector3Scale(velAlvo, Vector3Length(Vector3Subtract(previsto, in->pos))
                                                         / VEL_LASER_INIMIGO));
    }
    if (!velocidadeBalistica(in->pos, previsto, VEL_LASER_INIMIGO, GRAVIDADE_LASER_YWING, &velocidade))
        return 0;
    if (fabsf(velocidade.y) > 2600.0f)
        return 0;
    l = &g->lasers[slot];
    ativarLaser(l, in->pos, velocidade);
    l->doJogador = 0;
    l->tipoOrigem = YWING;
    l->vida = VIDA_LASER_INIMIGO;
    l->dano = DANO_LASER_INIMIGO;
    l->gravidade = GRAVIDADE_LASER_YWING;
    /* O tiro defensivo usa o som de canhão do X-Wing; só o míssil/bomba usa o som do Y-Wing. */
    audioTiroInimigo(XWING, in->pos);
    return 1;
}

int dispararTorpedoYwing(Jogo *g, const Inimigo *in)
{
    int slot = proximoLivre(g, 0);
    Vector3 frente = Vector3Normalize((Vector3){ in->vel.x, 0.0f, in->vel.z });
    Laser *l;

    if (slot < 0 || in->tipo != YWING)
        return 0;
    if (Vector3LengthSqr(frente) < 1e-6f)
        frente = (Vector3){ 0.0f, 0.0f, -1.0f };
    l = &g->lasers[slot];
    ativarLaser(l, in->pos, (Vector3){ frente.x * VEL_TORPEDO_YWING,
                                       -0.8f * VEL_TORPEDO_YWING,
                                       frente.z * VEL_TORPEDO_YWING });
    l->doJogador = 0;
    l->tipoOrigem = YWING;
    l->torpedo = 1;
    l->vida = VIDA_TORPEDO_YWING;
    l->dano = DANO_TORPEDO_YWING;
    l->gravidade = GRAVIDADE_TORPEDO_YWING;
    audioTiroInimigo(YWING, in->pos);
    return 1;
}

int dispararLaserAliado(Jogo *g, const Aliado *aliado, const Inimigo *alvo)
{
    Vector3 ponto = pontoDeInterceptacao(aliado->pos, VEL_LASER, alvo->pos, alvo->vel);
    Vector3 dir = Vector3Subtract(ponto, aliado->pos);
    Vector3 base, lateral;
    int slotA, slotB;
    Laser *a, *b;

    if (!aliado->ativo || Vector3LengthSqr(dir) < 1e-6f)
        return 0;
    slotA = proximoLivre(g, 0);
    slotB = slotA < 0 ? -1 : proximoLivre(g, slotA + 1);
    if (slotB < 0)
        return 0;

    dir = Vector3Normalize(dir);
    base = Vector3Add(aliado->pos, Vector3Scale(dir, RAIO_ALIADO));
    /* Mesma nave do jogador: dois canhões, um de cada lado do eixo. */
    lateral = Vector3CrossProduct(dir, (Vector3){ 0.0f, 1.0f, 0.0f });
    if (Vector3LengthSqr(lateral) < 1e-6f)
        lateral = (Vector3){ 1.0f, 0.0f, 0.0f };
    else
        lateral = Vector3Normalize(lateral);
    lateral = Vector3Scale(lateral, CANHAO_LATERAL);

    a = &g->lasers[slotA];
    b = &g->lasers[slotB];
    ativarLaser(a, Vector3Subtract(base, lateral), Vector3Scale(dir, VEL_LASER));
    ativarLaser(b, Vector3Add(base, lateral), Vector3Scale(dir, VEL_LASER));
    for (int i = 0; i < 2; i++) {
        Laser *l = i == 0 ? a : b;

        l->doJogador = 0;
        l->doAliado = 1;
        l->dano = DANO_LASER_ALIADO;
    }
    audioTiroAliado(aliado->pos);
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
        float tAliado = 2.0f;
        int aliado = -1;

        if (!l->ativo || l->doJogador || l->doAliado)
            continue;

        anterior = posicaoAnterior(l, dt);
        if (segmentoAcertaEsfera(anterior, l->pos, g->jogador.pos, RAIO_JOGADOR, &t))
            tTie = t;
        if (segmentoEntraNoCasco(anterior, l->pos, &t))
            tCasco = t;
        for (int k = 0; k < MAX_ALIADOS; k++)
            if (g->aliados[k].ativo
                && segmentoAcertaEsfera(anterior, l->pos, g->aliados[k].pos, RAIO_ALIADO, &t)
                && t < tAliado) {
                tAliado = t;
                aliado = k;
            }
        if (tTie > 1.0f && tCasco > 1.0f && tAliado > 1.0f)
            continue;

        l->ativo = 0;
        if (tAliado < tTie && tAliado < tCasco) {
            danificarAliado(g, aliado, l->dano);
        } else if (tTie <= tCasco) {
            if (g->jogador.casco > 0.0f) {
                danificarJogador(g, l->dano);
                audioDanoJogador();
            }
            acertosTie++;
        } else {
            float fracao = l->torpedo ? FRACAO_DANO_TORPEDO_DESTROIER : FRACAO_DANO_DESTROIER;

            danificarDestroier(g, l->dano * fracao);
        }
    }
    return acertosTie;
}
