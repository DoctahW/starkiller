#include <math.h>
#include <string.h>

#include "ia.h"
#include "inimigos.h"
#include "jogo.h"
#include "nave.h"
#include "raymath.h"
#include "teste.h"

#define DT (1.0f / 60.0f)
#define QUASE(a, b) (fabsf((a) - (b)) < 1e-4f)

/* Valores da tabela 2.2 e do criterio da IA-02. */
#define BOMBA_DANO    8.0f
#define BOMBA_ALCANCE 685.0f
#define ORBITA_RAIO   550.0f

static Inimigo novoInimigo(TipoNave tipo, Vector3 pos, EstadoIA estado)
{
    Inimigo in = criarInimigo(tipo, pos, 1);

    in.estadoIA = estado;
    return in;
}

static void simular(Inimigo *in, Jogo *g, int passos)
{
    for (int i = 0; i < passos; i++)
        ia_atualizar(in, g, DT);
}

/* IA-01: o caça acelera para o alvo, sem inverter o rumo no mesmo frame. */
static void testarAproximaDoDestroier(void)
{
    Jogo g = { 0 };
    Inimigo in = novoInimigo(XWING, (Vector3){ 0.0f, 0.0f, 4600.0f }, IA_ATACAR_DESTROIER);
    float antes = Vector3Length(in.pos);

    iniciarJogador(&g.jogador);

    ia_atualizar(&in, &g, DT);
    VERIFICA(Vector3Length(in.vel) > 0.0f);
    VERIFICA(Vector3Length(in.vel) < VEL_XWING);   /* um passo não chega na velocidade máxima */
    VERIFICA(in.vel.z < 0.0f);                    /* Destróier na origem: -z */
    VERIFICA(Vector3Length(in.pos) < antes);

    simular(&in, &g, 300);
    VERIFICA(Vector3Length(in.vel) <= VEL_XWING + 1e-3f);
    VERIFICA(Vector3Length(in.vel) > 0.99f * VEL_XWING);
    VERIFICA(in.vel.z < -0.99f * VEL_XWING);
    VERIFICA(Vector3Length(in.pos) < antes);
}

/* Tabela 2.2: cada tipo respeita a própria velocidade máxima. */
static void testarVelocidadeMaximaPorTipo(void)
{
    Jogo g = { 0 };
    const struct { TipoNave tipo; float velMax; } casos[] = {
        { XWING, VEL_XWING },
        { YWING, VEL_YWING },
        { AWING, VEL_AWING },
    };

    iniciarJogador(&g.jogador);

    for (int i = 0; i < (int)(sizeof casos / sizeof casos[0]); i++) {
        Inimigo in = novoInimigo(casos[i].tipo, (Vector3){ 0.0f, 0.0f, 6900.0f }, IA_ATACAR_DESTROIER);

        simular(&in, &g, 300);
        VERIFICA(Vector3Length(in.vel) <= casos[i].velMax + 1e-3f);
        VERIFICA(Vector3Length(in.vel) > 0.99f * casos[i].velMax);
    }
}

/* IA-01: o estado IA_ATACAR_JOGADOR persegue a posição do jogador. */
static void testarPersegueJogador(void)
{
    Jogo g = { 0 };
    Inimigo in = novoInimigo(AWING, (Vector3){ 0.0f, 0.0f, 460.0f }, IA_ATACAR_JOGADOR);
    float antes;

    iniciarJogador(&g.jogador);   /* jogador em (0, 0, 1370) */

    antes = Vector3Distance(in.pos, g.jogador.pos);
    simular(&in, &g, 120);
    VERIFICA(Vector3Distance(in.pos, g.jogador.pos) < antes);
    VERIFICA(in.vel.z > 0.0f);
}

/* IA_FUGIR: o caça se afasta do jogador. */
static void testarFogeDoJogador(void)
{
    Jogo g = { 0 };
    Inimigo in = novoInimigo(XWING, (Vector3){ 0.0f, 0.0f, 1830.0f }, IA_FUGIR);
    float antes;

    iniciarJogador(&g.jogador);

    antes = Vector3Distance(in.pos, g.jogador.pos);
    simular(&in, &g, 120);
    VERIFICA(Vector3Distance(in.pos, g.jogador.pos) > antes);
    VERIFICA(in.pos.z > 1830.0f);
}

/* Alvo na mesma posição não pode gerar NaN (divisão por zero no rumo). */
static void testarAlvoNaMesmaPosicao(void)
{
    Jogo g = { 0 };
    Inimigo in = novoInimigo(XWING, (Vector3){ 0.0f, 0.0f, 0.0f }, IA_ATACAR_DESTROIER);

    iniciarJogador(&g.jogador);

    simular(&in, &g, 10);
    VERIFICA(isfinite(in.vel.x) && isfinite(in.vel.y) && isfinite(in.vel.z));
    VERIFICA(isfinite(in.pos.x) && isfinite(in.pos.y) && isfinite(in.pos.z));
    VERIFICA(QUASE(Vector3Length(in.vel), 0.0f));
    VERIFICA(QUASE(Vector3Length(in.pos), 0.0f));
}

/* Seção 3.4, passo 4: atualizarInimigos delega o movimento à IA. */
static void testarAtualizarInimigosMove(void)
{
    Jogo g = { 0 };
    Vector3 antes = { 0.0f, 0.0f, 4600.0f };
    Inimigo in = novoInimigo(XWING, antes, IA_ATACAR_DESTROIER);

    iniciarJogador(&g.jogador);
    in.id = g.proximoId++;
    VERIFICA(inserirInimigo(&g.inimigos, in) == 1);

    atualizarInimigos(&g, DT);

    VERIFICA(g.inimigos.inicio != NULL);
    VERIFICA(g.inimigos.tamanho == 1);
    VERIFICA(g.inimigos.inicio->dado.pos.z < antes.z);
    VERIFICA(Vector3Length(g.inimigos.inicio->dado.vel) > 0.0f);
    VERIFICA(Vector3Length(g.inimigos.inicio->dado.vel) <= VEL_XWING);
    /* a distância do passo é medida antes do movimento, como na seção 3.4 */
    VERIFICA(QUASE(g.inimigos.inicio->dado.distancia, Vector3Distance(antes, g.jogador.pos)));

    liberarListaInimigos(&g.inimigos);
}

/* IA-02: a menos de BOMBA_ALCANCE do Destróier, o Y-Wing causa 8 de dano/s no casco. */
static void testarYwingBombardeia(void)
{
    Jogo g = { 0 };
    Inimigo in = novoInimigo(YWING, (Vector3){ 0.0f, 0.0f, 460.0f }, IA_ATACAR_DESTROIER);

    iniciarJogador(&g.jogador);
    g.cascoDestroier = 2000.0f;

    ia_atualizar(&in, &g, DT);
    VERIFICA(QUASE(g.cascoDestroier, 2000.0f - BOMBA_DANO * DT));

    simular(&in, &g, 59);
    /* 1 s de bombardeio; a tolerância cobre o arredondamento do float a cada passo */
    VERIFICA(fabsf(g.cascoDestroier - (2000.0f - BOMBA_DANO)) < 0.01f);
}

/* IA-02: fora do alcance não há dano; o Y-Wing orbita em volta do Destróier. */
static void testarYwingForaDeAlcance(void)
{
    Jogo g = { 0 };
    Inimigo longe = novoInimigo(YWING, (Vector3){ 0.0f, 0.0f, 1830.0f }, IA_ATACAR_DESTROIER);
    Inimigo orbita = novoInimigo(YWING, (Vector3){ 0.0f, 0.0f, ORBITA_RAIO }, IA_ATACAR_DESTROIER);
    float antes = Vector3Length(orbita.pos);

    iniciarJogador(&g.jogador);
    g.cascoDestroier = 2000.0f;

    ia_atualizar(&longe, &g, DT);
    VERIFICA(g.cascoDestroier == 2000.0f);
    VERIFICA(Vector3Length(longe.pos) < 1830.0f);

    simular(&longe, &g, 600);
    VERIFICA(Vector3Length(longe.pos) > ORBITA_RAIO - 185.0f);
    VERIFICA(Vector3Length(longe.pos) < BOMBA_ALCANCE + 45.0f);
    VERIFICA(g.cascoDestroier < 2000.0f);

    simular(&orbita, &g, 600);
    /* fica no anel e continua bombardeando */
    VERIFICA(Vector3Length(orbita.pos) > 365.0f);
    VERIFICA(Vector3Length(orbita.pos) < BOMBA_ALCANCE + 45.0f);
    VERIFICA(Vector3Length(orbita.pos) > antes - 275.0f);
    VERIFICA(fabsf(Vector3DotProduct(Vector3Normalize(orbita.pos), orbita.vel)) < 45.0f);
    VERIFICA(Vector3Length(orbita.vel) > 90.0f);
    VERIFICA(g.cascoDestroier < 2000.0f - BOMBA_DANO);
}

int main(void)
{
    testarAproximaDoDestroier();
    testarVelocidadeMaximaPorTipo();
    testarPersegueJogador();
    testarFogeDoJogador();
    testarAlvoNaMesmaPosicao();
    testarAtualizarInimigosMove();
    testarYwingBombardeia();
    testarYwingForaDeAlcance();

    return RESULTADO_TESTES();
}
