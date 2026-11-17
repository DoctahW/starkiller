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

static float comprimento(Vector3 v)
{
    return sqrtf(v.x * v.x + v.y * v.y + v.z * v.z);
}

static float distancia(Vector3 a, Vector3 b)
{
    return comprimento((Vector3){ a.x - b.x, a.y - b.y, a.z - b.z });
}

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
    Inimigo in = novoInimigo(XWING, (Vector3){ 0.0f, 0.0f, 1000.0f }, IA_ATACAR_DESTROIER);
    float antes = comprimento(in.pos);

    iniciarJogador(&g.jogador);

    ia_atualizar(&in, &g, DT);
    VERIFICA(comprimento(in.vel) > 0.0f);
    VERIFICA(comprimento(in.vel) < VEL_XWING);   /* um passo não chega na velocidade máxima */
    VERIFICA(in.vel.z < 0.0f);                    /* Destróier na origem: -z */
    VERIFICA(comprimento(in.pos) < antes);

    simular(&in, &g, 300);
    VERIFICA(comprimento(in.vel) <= VEL_XWING + 1e-3f);
    VERIFICA(comprimento(in.vel) > 0.99f * VEL_XWING);
    VERIFICA(in.vel.z < -0.99f * VEL_XWING);
    VERIFICA(comprimento(in.pos) < antes);
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
        Inimigo in = novoInimigo(casos[i].tipo, (Vector3){ 0.0f, 0.0f, 1500.0f }, IA_ATACAR_DESTROIER);

        simular(&in, &g, 300);
        VERIFICA(comprimento(in.vel) <= casos[i].velMax + 1e-3f);
        VERIFICA(comprimento(in.vel) > 0.99f * casos[i].velMax);
    }
}

/* IA-01: o estado IA_ATACAR_JOGADOR persegue a posição do jogador. */
static void testarPersegueJogador(void)
{
    Jogo g = { 0 };
    Inimigo in = novoInimigo(AWING, (Vector3){ 0.0f, 0.0f, 100.0f }, IA_ATACAR_JOGADOR);
    float antes;

    iniciarJogador(&g.jogador);   /* jogador em (0, 0, 300) */

    antes = distancia(in.pos, g.jogador.pos);
    simular(&in, &g, 120);
    VERIFICA(distancia(in.pos, g.jogador.pos) < antes);
    VERIFICA(in.vel.z > 0.0f);
}

/* IA_FUGIR: o caça se afasta do jogador. */
static void testarFogeDoJogador(void)
{
    Jogo g = { 0 };
    Inimigo in = novoInimigo(XWING, (Vector3){ 0.0f, 0.0f, 400.0f }, IA_FUGIR);
    float antes;

    iniciarJogador(&g.jogador);

    antes = distancia(in.pos, g.jogador.pos);
    simular(&in, &g, 120);
    VERIFICA(distancia(in.pos, g.jogador.pos) > antes);
    VERIFICA(in.pos.z > 400.0f);
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
    VERIFICA(QUASE(comprimento(in.vel), 0.0f));
    VERIFICA(QUASE(comprimento(in.pos), 0.0f));
}

/* Seção 3.4, passo 4: atualizarInimigos delega o movimento à IA. */
static void testarAtualizarInimigosMove(void)
{
    Jogo g = { 0 };
    Vector3 antes = { 0.0f, 0.0f, 1000.0f };
    Inimigo in = novoInimigo(XWING, antes, IA_ATACAR_DESTROIER);

    iniciarJogador(&g.jogador);
    in.id = g.proximoId++;
    VERIFICA(inserirInimigo(&g.inimigos, in) == 1);

    atualizarInimigos(&g, DT);

    VERIFICA(g.inimigos.inicio != NULL);
    VERIFICA(g.inimigos.tamanho == 1);
    VERIFICA(g.inimigos.inicio->dado.pos.z < antes.z);
    VERIFICA(comprimento(g.inimigos.inicio->dado.vel) > 0.0f);
    VERIFICA(comprimento(g.inimigos.inicio->dado.vel) <= VEL_XWING);
    /* a distância do passo é medida antes do movimento, como na seção 3.4 */
    VERIFICA(QUASE(g.inimigos.inicio->dado.distancia, distancia(antes, g.jogador.pos)));

    liberarListaInimigos(&g.inimigos);
}

int main(void)
{
    testarAproximaDoDestroier();
    testarVelocidadeMaximaPorTipo();
    testarPersegueJogador();
    testarFogeDoJogador();
    testarAlvoNaMesmaPosicao();
    testarAtualizarInimigosMove();

    return RESULTADO_TESTES();
}
