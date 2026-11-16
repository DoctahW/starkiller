#include <string.h>

#include "jogo.h"
#include "entrada.h"
#include "inimigos.h"
#include "nave.h"
#include "placar.h"
#include "reforcos.h"
#include "telas.h"

#define CASCO_DESTROIER  100.0f

static float acumulador;

/* Libera as estruturas da partida em andamento; seguro de chamar mais de uma vez. */
static void encerrarPartida(Jogo *g)
{
    liberarListaInimigos(&g->inimigos);
    liberarFila(&g->reforcos);
}

/* Zera o Jogo preservando o placar, gera os reforços e posiciona o jogador. */
static void iniciarPartida(Jogo *g)
{
    NoPlacar *placar = g->placar;

    encerrarPartida(g);
    memset(g, 0, sizeof *g);
    g->placar = placar;
    g->tempoRestante = DURACAO_PARTIDA;
    g->cascoDestroier = CASCO_DESTROIER;
    g->proximoId = 1;
    gerarReforcos(&g->reforcos, DURACAO_PARTIDA);
    iniciarJogador(&g->jogador);
    acumulador = 0.0f;
}

/* Derrota tem precedência sobre o tempo esgotado. */
static int verificarFim(Jogo *g)
{
    if (g->jogador.casco <= 0.0f || g->cascoDestroier <= 0.0f) {
        g->vitoria = 0;
        return 1;
    }
    if (g->tempoRestante <= 0.0f) {
        g->tempoRestante = 0.0f;
        g->vitoria = 1;
        return 1;
    }
    return 0;
}

/* Seção 3.4: atualizarNave, processarReforcos, atualizarInimigos e atualizarCombate entram antes do timer. */
static void passoCombate(Jogo *g, float dt)
{
    g->tempoDecorrido += dt;
    g->tempoRestante = DURACAO_PARTIDA - g->tempoDecorrido;
}

EstadoJogo simularCombate(Jogo *g, float *acum, float dtFrame)
{
    *acum += dtFrame < DT_MAXIMO ? dtFrame : DT_MAXIMO;
    while (*acum >= PASSO_FIXO) {
        *acum -= PASSO_FIXO;
        passoCombate(g, PASSO_FIXO);
        if (verificarFim(g))
            return EST_FIM;
    }
    return EST_COMBATE;
}

static EstadoJogo combate(Jogo *g, const Comandos *cmd)
{
    if (cmd->pausa)
        return EST_PAUSA;

    EstadoJogo proximo = simularCombate(g, &acumulador, GetFrameTime());

    DrawText(TextFormat("COMBATE  %3.0f s", g->tempoRestante), 20, 20, 20, RAYWHITE);
    DrawText("Esc: pausa", 20, 50, 20, GRAY);
    return proximo;
}

static EstadoJogo pausa(const Comandos *cmd)
{
    DrawText("PAUSA", 20, 20, 40, RAYWHITE);
    DrawText("Esc: continuar   Enter: voltar ao menu", 20, 70, 20, GRAY);

    if (cmd->pausa)
        return EST_COMBATE;
    if (cmd->confirmar)
        return EST_MENU;
    return EST_PAUSA;
}

/*
 * Transições:
 *   MENU      -> COMBATE (inicia partida) | PLACAR | CONTROLES | SAIR
 *   COMBATE   -> PAUSA | FIM
 *   PAUSA     -> COMBATE | MENU (libera a partida)
 *   FIM       -> PLACAR
 *   PLACAR    -> MENU
 *   CONTROLES -> MENU
 */
static EstadoJogo atualizarEstado(Jogo *g, ConfigEntrada *cfg, const Comandos *cmd)
{
    switch (g->estado) {
    case EST_MENU:      return telaMenu(g, cmd);
    case EST_COMBATE:   return combate(g, cmd);
    case EST_PAUSA:     return pausa(cmd);
    case EST_FIM:       return telaFim(g, cmd);
    case EST_PLACAR:    return telaPlacar(g, cmd);
    case EST_CONTROLES: return telaControles(cfg, cmd);
    case EST_SAIR:      break;
    }
    return EST_SAIR;
}

static void trocarEstado(Jogo *g, EstadoJogo novo)
{
    if (g->estado == EST_MENU && novo == EST_COMBATE)
        iniciarPartida(g);
    else if (novo == EST_MENU)
        encerrarPartida(g);
    g->estado = novo;
}

void jogo_executar(void)
{
    static Jogo jogo;
    ConfigEntrada cfg;

    InitWindow(LARGURA_TELA, ALTURA_TELA, "TIE Fighter: Esquadrão Imperial");
    ChangeDirectory(GetApplicationDirectory());
    SetExitKey(KEY_NULL);   /* Esc é pausa/voltar, não fecha a janela */
    SetTargetFPS(60);

    iniciarEntrada(&cfg);
    jogo.estado = EST_MENU;

    while (jogo.estado != EST_SAIR && !WindowShouldClose()) {
        Comandos cmd = lerComandos(&cfg);

        BeginDrawing();
        ClearBackground(BLACK);
        EstadoJogo proximo = atualizarEstado(&jogo, &cfg, &cmd);
        EndDrawing();

        if (proximo != jogo.estado)
            trocarEstado(&jogo, proximo);
    }

    encerrarPartida(&jogo);
    liberarPlacar(jogo.placar);
    jogo.placar = NULL;
    CloseWindow();
}
