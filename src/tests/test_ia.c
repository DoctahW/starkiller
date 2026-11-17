#include <math.h>
#include <string.h>

#include "combate.h"
#include "destroier.h"
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
    Inimigo in = novoInimigo(AWING, (Vector3){ 0.0f, 0.0f, 2500.0f }, IA_ATACAR_JOGADOR);
    float antes;

    iniciarJogador(&g.jogador);   /* jogador em (0, 0, 1370) */

    antes = Vector3Distance(in.pos, g.jogador.pos);
    simular(&in, &g, 120);
    VERIFICA(Vector3Distance(in.pos, g.jogador.pos) < antes);
    VERIFICA(in.vel.z < 0.0f);
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

/*
 * Alvo na mesma posição não pode gerar NaN (divisão por zero no rumo). Na origem,
 * o caça está dentro do casco e o desvio da IA-04 precisa tirá-lo de lá.
 */
static void testarAlvoNaMesmaPosicao(void)
{
    Jogo g = { 0 };
    Inimigo naOrigem = novoInimigo(XWING, (Vector3){ 0.0f, 0.0f, 0.0f }, IA_ATACAR_DESTROIER);
    Inimigo noJogador;

    iniciarJogador(&g.jogador);
    noJogador = novoInimigo(XWING, (Vector3){ 0.0f, 0.0f, 5000.0f }, IA_ATACAR_JOGADOR);
    g.jogador.pos = noJogador.pos;

    simular(&naOrigem, &g, 10);
    VERIFICA(isfinite(naOrigem.vel.x) && isfinite(naOrigem.vel.y) && isfinite(naOrigem.vel.z));
    VERIFICA(isfinite(naOrigem.pos.x) && isfinite(naOrigem.pos.y) && isfinite(naOrigem.pos.z));
    VERIFICA(!tocaDestroier(naOrigem.pos, raioInimigo(XWING)));

    simular(&noJogador, &g, 10);
    VERIFICA(isfinite(noJogador.vel.x) && isfinite(noJogador.vel.y) && isfinite(noJogador.vel.z));
    VERIFICA(isfinite(noJogador.pos.x) && isfinite(noJogador.pos.y) && isfinite(noJogador.pos.z));
    VERIFICA(QUASE(Vector3Length(noJogador.vel), 0.0f));
    VERIFICA(QUASE(noJogador.pos.z, 5000.0f));
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

/* IA-02: fora do alcance não há dano; o Y-Wing orbita no plano z = 0. */
static void testarYwingForaDeAlcance(void)
{
    Jogo g = { 0 };
    Inimigo longe = novoInimigo(YWING, (Vector3){ 0.0f, 0.0f, 1830.0f }, IA_ATACAR_DESTROIER);
    Inimigo orbita = novoInimigo(YWING, (Vector3){ ORBITA_RAIO, 0.0f, 0.0f }, IA_ATACAR_DESTROIER);

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
    /* fica no anel, no plano z = 0, e continua bombardeando */
    VERIFICA(Vector3Length(orbita.pos) > 365.0f);
    VERIFICA(Vector3Length(orbita.pos) < BOMBA_ALCANCE + 45.0f);
    VERIFICA(fabsf(orbita.pos.z) < 50.0f);
    VERIFICA(fabsf(Vector3DotProduct(Vector3Normalize(orbita.pos), orbita.vel)) < 45.0f);
    VERIFICA(Vector3Length(orbita.vel) > 90.0f);
    VERIFICA(g.cascoDestroier < 2000.0f - BOMBA_DANO);
}

/* IA-04: um caça dentro do casco é empurrado para fora já no primeiro passo. */
static void testarCascoEmpurraParaFora(void)
{
    Jogo g = { 0 };
    Inimigo proa = novoInimigo(XWING, (Vector3){ 0.0f, 0.0f, 810.0f }, IA_ATACAR_DESTROIER);
    Inimigo centro = novoInimigo(AWING, (Vector3){ 0.0f, 0.0f, 0.0f }, IA_ATACAR_DESTROIER);

    iniciarJogador(&g.jogador);

    ia_atualizar(&proa, &g, DT);
    VERIFICA(!tocaDestroier(proa.pos, raioInimigo(XWING)));
    VERIFICA(Vector3Length(proa.pos) > 810.0f - 1.0f);   /* empurrado para fora, não para dentro */

    ia_atualizar(&centro, &g, DT);
    VERIFICA(!tocaDestroier(centro.pos, raioInimigo(AWING)));
    VERIFICA(Vector3Length(centro.pos) > 0.0f);
    VERIFICA(isfinite(centro.pos.x) && isfinite(centro.pos.y) && isfinite(centro.pos.z));
}

/*
 * IA-04: nenhum tipo atravessa o casco, por mais passos que voe contra ele.
 * Cobre aproximação no eixo, lateral, diagonal e o caso que travava: dentro do casco no eixo.
 */
static void testarNaoAtravessaOCasco(void)
{
    const TipoNave tipos[] = { XWING, YWING, AWING };
    const Vector3 partidas[] = {
        { 0.0f, 0.0f, 4600.0f },
        { 1500.0f, 0.0f, 0.0f },
        { 900.0f, 400.0f, -1500.0f },
        { 0.0f, 0.0f, 0.0f },
        { 0.0f, 0.0f, -90.0f },
        { 0.0f, 0.0f, 90.0f },
    };
    int t, p;

    for (t = 0; t < (int)(sizeof tipos / sizeof tipos[0]); t++) {
        for (p = 0; p < (int)(sizeof partidas / sizeof partidas[0]); p++) {
            Jogo g = { 0 };
            Inimigo in = novoInimigo(tipos[t], partidas[p], IA_ATACAR_DESTROIER);
            int i, dentro = 0;

            iniciarJogador(&g.jogador);

            for (i = 0; i < 1800; i++) {
                ia_atualizar(&in, &g, DT);
                dentro |= tocaDestroier(in.pos, raioInimigo(in.tipo));
            }

            VERIFICA(dentro == 0);
            VERIFICA(isfinite(in.pos.x) && isfinite(in.pos.y) && isfinite(in.pos.z));
        }
    }
}

/* IA-05: os fatores de dificuldade vão de 1,0 aos extremos e saturam nas pontas. */
static void testarFatoresDeDificuldade(void)
{
    float meio = 0.5f * DURACAO_PARTIDA;

    VERIFICA(QUASE(progressoDificuldade(0.0f), 0.0f));
    VERIFICA(QUASE(progressoDificuldade(meio), 0.5f));
    VERIFICA(QUASE(progressoDificuldade(DURACAO_PARTIDA), 1.0f));
    VERIFICA(QUASE(progressoDificuldade(2.0f * DURACAO_PARTIDA), 1.0f));
    VERIFICA(QUASE(progressoDificuldade(-1.0f), 0.0f));

    VERIFICA(QUASE(fatorVelocidadeIA(0.0f), 1.0f));
    VERIFICA(QUASE(fatorVelocidadeIA(meio), 1.15f));
    VERIFICA(QUASE(fatorVelocidadeIA(DURACAO_PARTIDA), FATOR_VELOCIDADE_MAX));
    VERIFICA(QUASE(fatorVelocidadeIA(2.0f * DURACAO_PARTIDA), FATOR_VELOCIDADE_MAX));

    VERIFICA(QUASE(fatorCooldownIA(0.0f), 1.0f));
    VERIFICA(QUASE(fatorCooldownIA(meio), 0.85f));
    VERIFICA(QUASE(fatorCooldownIA(DURACAO_PARTIDA), FATOR_COOLDOWN_MIN));
    VERIFICA(QUASE(fatorCooldownIA(2.0f * DURACAO_PARTIDA), FATOR_COOLDOWN_MIN));

    /* o cooldown encolhe com o tempo; a velocidade cresce */
    VERIFICA(fatorCooldownIA(DURACAO_PARTIDA) < fatorCooldownIA(0.0f));
    VERIFICA(fatorVelocidadeIA(DURACAO_PARTIDA) > fatorVelocidadeIA(0.0f));
}

/* IA-05: no fim da partida o caça chega a FATOR_VELOCIDADE_MAX vezes a velocidade do tipo. */
static void testarVelocidadeCresceComOTempo(void)
{
    Jogo g = { 0 };
    Inimigo in = novoInimigo(XWING, (Vector3){ 0.0f, 0.0f, 5000.0f }, IA_ATACAR_DESTROIER);
    float velMax = VEL_XWING * FATOR_VELOCIDADE_MAX;

    iniciarJogador(&g.jogador);
    g.tempoDecorrido = DURACAO_PARTIDA;

    simular(&in, &g, 300);
    VERIFICA(Vector3Length(in.vel) <= velMax + 1e-3f);
    VERIFICA(Vector3Length(in.vel) > 0.99f * velMax);
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
    testarCascoEmpurraParaFora();
    testarNaoAtravessaOCasco();
    testarFatoresDeDificuldade();
    testarVelocidadeCresceComOTempo();

    return RESULTADO_TESTES();
}
