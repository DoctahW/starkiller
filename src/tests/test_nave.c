#include <math.h>

#include <string.h>

#include "destroier.h"
#include "combate.h"
#include "nave.h"
#include "pontuacao.h"
#include "teste.h"

#define EPS 1e-5f

static int proximo(float a, float b)
{
    return fabsf(a - b) < EPS;
}

#define PI_F 3.14159265358979f

/*
 * Aplica cmd por `angulo / vel` segundos e depois solta o comando até a velocidade angular
 * zerar. Com a suavização o ganho em regime é 1, então o ângulo total é exatamente `angulo`.
 */
static void girar(Jogador *j, Comandos cmd, float vel, float angulo)
{
    int n = 600, i;
    float dt = (angulo / vel) / (float)n;
    Comandos parado = { 0 };
    int resto = (int)(30.0f * TAU_GIRO / dt) + 1;

    for (i = 0; i < n; i++)
        atualizarJogador(j, &cmd, dt);
    for (i = 0; i < resto; i++)
        atualizarJogador(j, &parado, dt);
}

static float tam(Vector3 v)
{
    return sqrtf(v.x * v.x + v.y * v.y + v.z * v.z);
}

static float dot(Vector3 a, Vector3 b)
{
    return a.x * b.x + a.y * b.y + a.z * b.z;
}

/* frente/cima/direita unitários e mutuamente ortogonais; rot unitário. */
static int baseValida(const Jogador *j)
{
    float q = j->rot.x * j->rot.x + j->rot.y * j->rot.y + j->rot.z * j->rot.z + j->rot.w * j->rot.w;

    return fabsf(q - 1.0f) < 1e-4f
        && fabsf(tam(j->frente) - 1.0f) < 1e-4f && fabsf(tam(j->cima) - 1.0f) < 1e-4f
        && fabsf(tam(j->direita) - 1.0f) < 1e-4f
        && fabsf(dot(j->frente, j->cima)) < 1e-4f && fabsf(dot(j->frente, j->direita)) < 1e-4f
        && fabsf(dot(j->cima, j->direita)) < 1e-4f;
}

static void testarRotacao(void)
{
    Jogador j;
    Comandos c = { 0 };

    /* Sinais: arfagem > 0 sobe o nariz, guinada > 0 vira à direita, rolagem > 0 inclina à direita. */
    iniciarJogador(&j);
    c.arfagem = 1.0f;
    girar(&j, c, VEL_ARFAGEM, 0.3f);
    VERIFICA(j.frente.y > 0.2f && proximo(j.frente.x, 0.0f));

    iniciarJogador(&j);
    c = (Comandos){ .guinada = 1.0f };
    girar(&j, c, VEL_GUINADA, 0.3f);
    VERIFICA(j.frente.x < -0.2f && proximo(j.frente.y, 0.0f));

    iniciarJogador(&j);
    c = (Comandos){ .rolagem = 1.0f };
    girar(&j, c, VEL_ROLAGEM, 0.3f);
    VERIFICA(j.cima.x < -0.2f && proximo(j.frente.x, 0.0f) && proximo(j.frente.y, 0.0f));

    /* Proporcional ao analógico: metade do eixo, metade do ângulo. */
    iniciarJogador(&j);
    c = (Comandos){ .arfagem = 0.5f };
    girar(&j, c, VEL_ARFAGEM * 0.5f, 0.4f);
    VERIFICA(fabsf(j.frente.y - sinf(0.4f)) < 1e-3f);

    /* Espaço local: depois de rolar 90° à direita, arfagem para cima move o nariz para a direita do mundo (-X). */
    iniciarJogador(&j);
    c = (Comandos){ .rolagem = 1.0f };
    girar(&j, c, VEL_ROLAGEM, PI_F / 2.0f);
    c = (Comandos){ .arfagem = 1.0f };
    girar(&j, c, VEL_ARFAGEM, 0.3f);
    VERIFICA(j.frente.x < -0.2f && fabsf(j.frente.y) < 1e-3f);
}

static void testarSemGimbalLock(void)
{
    Jogador j;
    Comandos c = { 0 };

    /* Loop completo de arfagem volta à orientação inicial. */
    iniciarJogador(&j);
    c.arfagem = 1.0f;
    girar(&j, c, VEL_ARFAGEM, 2.0f * PI_F);
    VERIFICA(fabsf(j.frente.x) < 1e-3f && fabsf(j.frente.y) < 1e-3f && fabsf(j.frente.z - 1.0f) < 1e-3f);
    VERIFICA(fabsf(j.cima.y - 1.0f) < 1e-3f);

    /* A 90° de arfagem (nariz a pino) a guinada ainda gira a nave: sem gimbal lock. */
    iniciarJogador(&j);
    c = (Comandos){ .arfagem = 1.0f };
    girar(&j, c, VEL_ARFAGEM, PI_F / 2.0f);
    VERIFICA(fabsf(j.frente.y - 1.0f) < 1e-3f);
    c = (Comandos){ .guinada = 1.0f };
    girar(&j, c, VEL_GUINADA, PI_F / 2.0f);
    VERIFICA(fabsf(j.frente.y) < 1e-3f && fabsf(fabsf(j.frente.x) - 1.0f) < 1e-3f);
    VERIFICA(baseValida(&j));
}

static void testarRenormalizacao(void)
{
    Jogador j;
    Comandos c = { .arfagem = 0.7f, .guinada = -0.4f, .rolagem = 1.0f };
    int i;

    iniciarJogador(&j);
    for (i = 0; i < 100000; i++)
        atualizarJogador(&j, &c, 1.0f / 60.0f);
    VERIFICA(baseValida(&j));

    /* Sem comandos, a rotação amortece até parar e a orientação deixa de mudar. */
    c = (Comandos){ 0 };
    for (i = 0; i < 600; i++)
        atualizarJogador(&j, &c, 1.0f / 60.0f);
    {
        Vector3 antes = j.frente;
        atualizarJogador(&j, &c, 1.0f / 60.0f);
        VERIFICA(proximo(j.frente.x, antes.x) && proximo(j.frente.y, antes.y) && proximo(j.frente.z, antes.z));
    }
}

static void testarAceleracao(void)
{
    Jogador j;
    Comandos c = { 0 };
    int i;

    /* Sem comando, mantém a velocidade e avança frente * v * dt. */
    iniciarJogador(&j);
    j.pos = (Vector3){ 0.0f, 0.0f, 0.0f };
    atualizarJogador(&j, &c, 0.5f);
    VERIFICA(proximo(j.velocidade, VEL_MIN));
    VERIFICA(proximo(j.pos.z, VEL_MIN * 0.5f) && proximo(j.pos.x, 0.0f) && proximo(j.pos.y, 0.0f));

    /* Acelerar: sobe a ACELERACAO u/s² e para em VEL_MAX. */
    c.aceleracao = 1.0f;
    atualizarJogador(&j, &c, 1.0f);
    VERIFICA(proximo(j.velocidade, VEL_MIN + ACELERACAO));
    for (i = 0; i < 1000; i++)
        atualizarJogador(&j, &c, 1.0f / 60.0f);
    VERIFICA(proximo(j.velocidade, VEL_MAX));

    /* Frear: desce e para em VEL_MIN, sem ficar parada nem andar de ré. */
    c.aceleracao = -1.0f;
    for (i = 0; i < 1000; i++)
        atualizarJogador(&j, &c, 1.0f / 60.0f);
    VERIFICA(proximo(j.velocidade, VEL_MIN));

    /* Proporcional ao analógico. */
    iniciarJogador(&j);
    c.aceleracao = 0.5f;
    atualizarJogador(&j, &c, 1.0f);
    VERIFICA(proximo(j.velocidade, VEL_MIN + 0.5f * ACELERACAO));

    /* O avanço segue a frente atual: depois de guinar 90° à direita, anda para -X. */
    iniciarJogador(&j);
    j.pos = (Vector3){ 0.0f, 0.0f, 0.0f };
    c = (Comandos){ .guinada = 1.0f };
    girar(&j, c, VEL_GUINADA, PI_F / 2.0f);
    j.pos = (Vector3){ 0.0f, 0.0f, 0.0f };
    c = (Comandos){ 0 };
    atualizarJogador(&j, &c, 1.0f);
    VERIFICA(fabsf(j.pos.x + j.velocidade) < 1e-2f && fabsf(j.pos.z) < 1e-2f);
}

static void testarArena(void)
{
    Jogador j;
    Comandos c = { 0 };
    int i, avisou = 0, saiu = 0;

    /* Dentro da arena: sem aviso. */
    iniciarJogador(&j);
    VERIFICA(atualizarJogador(&j, &c, 1.0f / 60.0f) == 0);

    /* Voando para fora na velocidade máxima: nunca passa do raio, e o aviso dispara. */
    c.aceleracao = 1.0f;
    for (i = 0; i < 6000; i++) {
        avisou |= atualizarJogador(&j, &c, 1.0f / 60.0f);
        if (tam(j.pos) > RAIO_ARENA + 1e-2f)
            saiu = 1;
    }
    VERIFICA(avisou == 1);
    VERIFICA(saiu == 0);
    VERIFICA(atualizarJogador(&j, &c, 1.0f / 60.0f) == 1);
    VERIFICA(fabsf(tam(j.pos) - RAIO_ARENA) < 1e-2f);

    /* Passo grande (dt exagerado) também fica dentro. */
    iniciarJogador(&j);
    j.velocidade = VEL_MAX;
    VERIFICA(atualizarJogador(&j, &c, 100.0f) == 1);
    VERIFICA(tam(j.pos) <= RAIO_ARENA + 1e-2f);

    /* Virando 180° na borda, a nave volta para dentro e o aviso some. */
    c = (Comandos){ .arfagem = 1.0f };
    girar(&j, c, VEL_ARFAGEM, PI_F);
    c = (Comandos){ 0 };
    atualizarJogador(&j, &c, 1.0f);
    VERIFICA(tam(j.pos) < RAIO_ARENA - 1.0f);
    VERIFICA(atualizarJogador(&j, &c, 1.0f / 60.0f) == 0);
}

/* Ângulo de arfagem (rad) girado em um passo, medido pela subida da frente a partir do plano. */
static float passoArfagem(Jogador *j, const Comandos *c, float dt)
{
    Vector3 antes = j->frente;

    atualizarJogador(j, c, dt);
    return asinf(j->frente.y) - asinf(antes.y);
}

static void testarSuavizacao(void)
{
    Jogador j;
    Comandos c = { .arfagem = 1.0f };
    Comandos parado = { 0 };
    float dt = 1.0f / 60.0f, ang;
    int i;

    /* Sem giro instantâneo: no primeiro frame gira bem menos que VEL_ARFAGEM * dt. */
    iniciarJogador(&j);
    ang = passoArfagem(&j, &c, dt);
    VERIFICA(ang > 0.0f && ang < 0.25f * VEL_ARFAGEM * dt);

    /* Acelera passo a passo: cada frame gira mais que o anterior. */
    {
        float anterior = ang, atual = passoArfagem(&j, &c, dt);

        VERIFICA(atual > anterior);
    }

    /* Em regime atinge a velocidade alvo. */
    for (i = 0; i < 120; i++)
        atualizarJogador(&j, &c, dt);
    j.rot = (Quaternion){ 0.0f, 0.0f, 0.0f, 1.0f };
    j.frente = (Vector3){ 0.0f, 0.0f, 1.0f };
    ang = passoArfagem(&j, &c, dt);
    VERIFICA(fabsf(ang - VEL_ARFAGEM * dt) < 0.01f * VEL_ARFAGEM * dt);

    /* Soltar o comando também é gradual: ainda gira no frame seguinte, e termina parando. */
    j.rot = (Quaternion){ 0.0f, 0.0f, 0.0f, 1.0f };
    j.frente = (Vector3){ 0.0f, 0.0f, 1.0f };
    ang = passoArfagem(&j, &parado, dt);
    VERIFICA(ang > 0.5f * VEL_ARFAGEM * dt && ang < VEL_ARFAGEM * dt);
    for (i = 0; i < 300; i++)
        atualizarJogador(&j, &parado, dt);
    j.rot = (Quaternion){ 0.0f, 0.0f, 0.0f, 1.0f };
    j.frente = (Vector3){ 0.0f, 0.0f, 1.0f };
    VERIFICA(fabsf(passoArfagem(&j, &parado, dt)) < 1e-6f);

    /* iniciarJogador zera a velocidade angular acumulada. */
    iniciarJogador(&j);
    for (i = 0; i < 60; i++)
        atualizarJogador(&j, &c, dt);
    iniciarJogador(&j);
    VERIFICA(fabsf(passoArfagem(&j, &parado, dt)) < 1e-9f);

    /* A suavização não depende do dt: 1 s a 30 fps ≈ 1 s a 240 fps. */
    {
        Jogador a, b;
        int n;

        iniciarJogador(&a);
        for (n = 0; n < 30; n++)
            atualizarJogador(&a, &c, 1.0f / 30.0f);
        iniciarJogador(&b);
        for (n = 0; n < 240; n++)
            atualizarJogador(&b, &c, 1.0f / 240.0f);
        VERIFICA(fabsf(a.frente.y - b.frente.y) < 5e-3f);
    }
}

static Jogo jogoDestroier;

/*
 * Partida zerada com o jogador em `pos` (a recarga de colisão volta a zero em iniciarJogador). Sem
 * escudo: o dano da colisão vai direto ao casco; o escudo é testado em testarEscudo.
 */
static void novoJogoEm(Vector3 pos)
{
    memset(&jogoDestroier, 0, sizeof jogoDestroier);
    iniciarJogador(&jogoDestroier.jogador);
    jogoDestroier.jogador.pos = pos;
    jogoDestroier.jogador.escudo = 0.0f;
}

/* Roda `segundos` de atualizarEscudo em passos de 1/60 s. */
static void esperarEscudo(Jogador *j, float segundos)
{
    int i;

    for (i = 0; i < (int)lroundf(segundos * 60.0f); i++)
        atualizarEscudo(j, 1.0f / 60.0f);
}

static void testarEscudo(void)
{
    Jogo *g = &jogoDestroier;
    Jogador *j = &g->jogador;

    memset(g, 0, sizeof *g);
    iniciarJogador(j);
    VERIFICA(proximo(j->escudo, ESCUDO_JOGADOR) && proximo(j->casco, CASCO_JOGADOR));

    /* O escudo absorve primeiro; o que passa dele vai para o casco; nada fica negativo. */
    g->sequenciaSemDano = 3;
    danificarJogador(g, 30.0f);
    VERIFICA(proximo(j->escudo, 20.0f) && proximo(j->casco, CASCO_JOGADOR));
    VERIFICA(g->sequenciaSemDano == 0 && g->tempoDano > 0.0f);
    danificarJogador(g, 30.0f);
    VERIFICA(proximo(j->escudo, 0.0f) && proximo(j->casco, CASCO_JOGADOR - 10.0f));
    danificarJogador(g, 500.0f);
    VERIFICA(proximo(j->escudo, 0.0f) && proximo(j->casco, 0.0f));

    /* Sem dano por ATRASO_ESCUDO: só então recarrega, aos poucos, até o máximo; o casco não volta. */
    j->casco = 40.0f;
    danificarJogador(g, 1.0f);
    esperarEscudo(j, ATRASO_ESCUDO - 0.1f);
    VERIFICA(proximo(j->escudo, 0.0f));
    esperarEscudo(j, 1.1f);
    VERIFICA(j->escudo > 0.0f && j->escudo < ESCUDO_JOGADOR);
    esperarEscudo(j, ESCUDO_JOGADOR / RECARGA_ESCUDO);
    VERIFICA(proximo(j->escudo, ESCUDO_JOGADOR) && proximo(j->casco, 39.0f));

    /* Dano no meio da recarga: ela para na hora e a espera recomeça. */
    danificarJogador(g, 40.0f);
    esperarEscudo(j, ATRASO_ESCUDO + 1.0f);
    {
        float recarregado = j->escudo;

        danificarJogador(g, 1.0f);
        esperarEscudo(j, ATRASO_ESCUDO - 0.1f);
        VERIFICA(proximo(j->escudo, recarregado - 1.0f));
    }

    /* Disparar também conta como combate: a recarga espera de novo. */
    j->escudo = 0.0f;
    j->esperaEscudo = 0.0f;
    j->cooldownLaser = 0.0f;
    VERIFICA(dispararLaser(g) == 1);
    esperarEscudo(j, ATRASO_ESCUDO - 0.1f);
    VERIFICA(proximo(j->escudo, 0.0f));
    esperarEscudo(j, 1.0f);
    VERIFICA(j->escudo > 0.0f);
}

/* Primeira altura, descendo de cima em passos de 0,5 u, em que a nave em (x, ?, z) encosta no casco. */
static float alturaDeContato(float x, float z)
{
    float y;

    for (y = 600.0f; y > -600.0f; y -= 0.5f)
        if (tocaDestroier((Vector3){ x, y, z }, RAIO_JOGADOR))
            return y;
    return -1000.0f;
}

static void testarColisaoDestroier(void)
{
    Jogo *g = &jogoDestroier;
    const float E = ESCALA_DESTROIER;
    float casco0, conves, lado;
    Jogador inicio;

    /* Longe do casco: nada acontece (posição inicial do jogador). */
    iniciarJogador(&inicio);
    novoJogoEm(inicio.pos);
    g->sequenciaSemDano = 5;
    casco0 = g->jogador.casco;
    VERIFICA(colidirDestroier(g, 1.0f / 60.0f) == 0);
    VERIFICA(proximo(g->jogador.pos.z, inicio.pos.z) && proximo(g->jogador.casco, casco0));
    VERIFICA(g->sequenciaSemDano == 5);

    /* Rente ao casco: na proa ele tem ~20 u de meia espessura; 40 u acima do eixo é livre. */
    VERIFICA(tocaDestroier((Vector3){ 0.0f, 0.0f, 700.0f * E }, 0.0f));
    VERIFICA(!tocaDestroier((Vector3){ 0.0f, 40.0f * E, 700.0f * E }, RAIO_JOGADOR));
    VERIFICA(!tocaDestroier((Vector3){ 0.0f, -40.0f * E, 700.0f * E }, RAIO_JOGADOR));

    /* Desceu 2 u para dentro do conves: sobe de volta (x e z iguais), perde casco e zera a sequência. */
    conves = alturaDeContato(0.0f, 200.0f * E);
    VERIFICA(conves > 0.0f && conves < 200.0f * E);
    novoJogoEm((Vector3){ 0.0f, conves - 2.0f, 200.0f * E });
    g->sequenciaSemDano = 5;
    VERIFICA(colidirDestroier(g, 1.0f / 60.0f) == 1);
    VERIFICA(g->jogador.pos.y > conves - 2.0f && g->jogador.pos.y < conves + 0.51f);
    VERIFICA(proximo(g->jogador.pos.x, 0.0f) && proximo(g->jogador.pos.z, 200.0f * E));
    VERIFICA(!tocaDestroier(g->jogador.pos, RAIO_JOGADOR));
    VERIFICA(proximo(g->jogador.casco, casco0 - DANO_COLISAO_DESTROIER));
    VERIFICA(g->sequenciaSemDano == 0);

    /* Assentada na superfície não é contato. */
    VERIFICA(colidirDestroier(g, 1.0f / 60.0f) == 0);

    /* Pela parede lateral da popa: o empurrão é para o lado. */
    for (lado = 600.0f * E; lado > 0.0f && !tocaDestroier((Vector3){ lado, 0.0f, -600.0f * E }, RAIO_JOGADOR); lado -= 0.5f)
        ;
    VERIFICA(lado > (MEIA_BOCA_DESTROIER - 60.0f) * E);
    novoJogoEm((Vector3){ lado - 2.0f, 0.0f, -600.0f * E });
    VERIFICA(colidirDestroier(g, 1.0f / 60.0f) == 1);
    VERIFICA(g->jogador.pos.x > lado - 2.0f && g->jogador.pos.x < lado + 0.51f);
    VERIFICA(proximo(g->jogador.pos.y, 0.0f) && proximo(g->jogador.pos.z, -600.0f * E));

    /* Vão sob as asas da ponte de comando (x = ±140, z = -564 no modelo): o casco embaixo e a
     * asa em cima são sólidos, mas o espaço entre eles é livre. A ponte em si (y 240-275) é sólida. */
    VERIFICA(!tocaDestroier((Vector3){ 140.0f * E, 176.0f * E, -564.0f * E }, RAIO_JOGADOR));
    VERIFICA(!tocaDestroier((Vector3){ -140.0f * E, 176.0f * E, -564.0f * E }, RAIO_JOGADOR));
    VERIFICA(tocaDestroier((Vector3){ 140.0f * E, 100.0f * E, -564.0f * E }, RAIO_JOGADOR));   /* casco abaixo */
    VERIFICA(tocaDestroier((Vector3){ 140.0f * E, 250.0f * E, -564.0f * E }, RAIO_JOGADOR));   /* asa acima */
    VERIFICA(tocaDestroier((Vector3){ 135.0f * E, 250.0f * E, -567.0f * E }, RAIO_JOGADOR));   /* ponte */

    /* Bem no meio do casco: sai mesmo assim, sem ficar dentro. */
    novoJogoEm((Vector3){ 0.0f, 0.0f, 0.0f });
    VERIFICA(colidirDestroier(g, 1.0f / 60.0f) == 1);
    VERIFICA(!tocaDestroier(g->jogador.pos, RAIO_JOGADOR));

    /* Encostada contra o casco: empurra todo passo, mas o dano só volta depois de RECARGA_COLISAO. */
    novoJogoEm((Vector3){ 0.0f, conves - 2.0f, 200.0f * E });
    casco0 = g->jogador.casco;
    colidirDestroier(g, 0.1f);
    g->jogador.pos.y = conves - 2.0f;
    VERIFICA(colidirDestroier(g, 0.1f) == 1);
    VERIFICA(proximo(g->jogador.casco, casco0 - DANO_COLISAO_DESTROIER));
    g->jogador.pos.y = conves - 2.0f;
    VERIFICA(colidirDestroier(g, RECARGA_COLISAO) == 1);
    VERIFICA(proximo(g->jogador.casco, casco0 - 2.0f * DANO_COLISAO_DESTROIER));

    /* iniciarJogador zera a recarga: a partida seguinte não herda o intervalo da anterior. */
    novoJogoEm((Vector3){ 0.0f, conves - 2.0f, 200.0f * E });
    colidirDestroier(g, 1.0f / 60.0f);
    novoJogoEm((Vector3){ 0.0f, conves - 2.0f, 200.0f * E });
    casco0 = g->jogador.casco;
    colidirDestroier(g, 1.0f / 60.0f);
    VERIFICA(proximo(g->jogador.casco, casco0 - DANO_COLISAO_DESTROIER));

    /* Voando de frente contra a proa por 3 s: bate e nunca termina um passo dentro do casco. */
    novoJogoEm((Vector3){ 0.0f, 0.0f, (Z_PROA_DESTROIER + 300.0f) * E });
    g->jogador.rot = (Quaternion){ 0.0f, 1.0f, 0.0f, 0.0f };   /* 180° em Y: olha para -Z, rumo ao Destróier */
    {
        Comandos acelera = { .aceleracao = 1.0f };
        int i, dentro = 0, bateu = 0;

        for (i = 0; i < 180; i++) {
            atualizarJogador(&g->jogador, &acelera, 1.0f / 60.0f);
            bateu |= colidirDestroier(g, 1.0f / 60.0f);
            dentro |= tocaDestroier(g->jogador.pos, RAIO_JOGADOR);
        }
        VERIFICA(bateu == 1);
        VERIFICA(dentro == 0);
    }
}

int main(void)
{
    Jogador j;
    Camera3D cam;

    iniciarJogador(&j);

    /* Início: fora do Destróier (origem), olhando para longe dele. */
    VERIFICA(j.pos.x * j.frente.x + j.pos.y * j.frente.y + j.pos.z * j.frente.z > 0.0f);
    VERIFICA(proximo(j.frente.x * j.frente.x + j.frente.y * j.frente.y + j.frente.z * j.frente.z, 1.0f));

    cam = cameraDoJogador(&j);
    VERIFICA(proximo(cam.position.x, j.pos.x) && proximo(cam.position.y, j.pos.y) && proximo(cam.position.z, j.pos.z));
    VERIFICA(proximo(cam.target.x, j.pos.x + j.frente.x));
    VERIFICA(proximo(cam.target.y, j.pos.y + j.frente.y));
    VERIFICA(proximo(cam.target.z, j.pos.z + j.frente.z));
    VERIFICA(proximo(cam.up.x, j.cima.x) && proximo(cam.up.y, j.cima.y) && proximo(cam.up.z, j.cima.z));
    VERIFICA(proximo(cam.fovy, FOV_JOGADOR));
    VERIFICA(cam.projection == CAMERA_PERSPECTIVE);

    /* A câmera acompanha o jogador. */
    j.pos = (Vector3){ 10.0f, -5.0f, 20.0f };
    j.frente = (Vector3){ 1.0f, 0.0f, 0.0f };
    j.cima = (Vector3){ 0.0f, 0.0f, 1.0f };
    cam = cameraDoJogador(&j);
    VERIFICA(proximo(cam.position.x, 10.0f) && proximo(cam.target.x, 11.0f));
    VERIFICA(proximo(cam.target.y, -5.0f) && proximo(cam.up.z, 1.0f));

    /* Tremor: sem dano é a câmera normal; com dano desloca position e target igual e decai a zero. */
    {
        Jogo g;
        Camera3D tr;
        float dx, dy, dz, cheio, meio;

        memset(&g, 0, sizeof g);
        iniciarJogador(&g.jogador);
        g.tempoDecorrido = 1.0f;
        tr = cameraComTremor(&g);
        cam = cameraDoJogador(&g.jogador);
        VERIFICA(tr.position.x == cam.position.x && tr.position.y == cam.position.y && tr.position.z == cam.position.z);

        g.tempoDano = DURACAO_FEEDBACK_DANO;
        tr = cameraComTremor(&g);
        VERIFICA(!proximo(tr.position.x, cam.position.x) || !proximo(tr.position.y, cam.position.y) || !proximo(tr.position.z, cam.position.z));
        VERIFICA(proximo(tr.target.x - tr.position.x, cam.target.x - cam.position.x));
        VERIFICA(proximo(tr.target.y - tr.position.y, cam.target.y - cam.position.y));
        VERIFICA(proximo(tr.target.z - tr.position.z, cam.target.z - cam.position.z));
        /* direita e cima são unitários e ortogonais, e cada eixo vai até AMPLITUDE_TREMOR: |desloc| <= A·√2. */
        dx = tr.position.x - cam.position.x;
        dy = tr.position.y - cam.position.y;
        dz = tr.position.z - cam.position.z;
        cheio = sqrtf(dx * dx + dy * dy + dz * dz);
        VERIFICA(cheio <= AMPLITUDE_TREMOR * sqrtf(2.0f) + EPS);

        /* Decai linearmente: na metade do tempo, com o mesmo tempoDecorrido, o deslocamento é a metade. */
        g.tempoDano = DURACAO_FEEDBACK_DANO * 0.5f;
        tr = cameraComTremor(&g);
        dx = tr.position.x - cam.position.x;
        dy = tr.position.y - cam.position.y;
        dz = tr.position.z - cam.position.z;
        meio = sqrtf(dx * dx + dy * dy + dz * dz);
        VERIFICA(fabsf(meio - cheio * 0.5f) < 1e-4f);

        g.tempoDano = 0.0f;
        tr = cameraComTremor(&g);
        VERIFICA(tr.position.x == cam.position.x && tr.position.y == cam.position.y && tr.position.z == cam.position.z);
    }

    testarRotacao();
    testarSemGimbalLock();
    testarRenormalizacao();
    testarAceleracao();
    testarArena();
    testarSuavizacao();
    testarColisaoDestroier();
    testarEscudo();

    return RESULTADO_TESTES();
}
