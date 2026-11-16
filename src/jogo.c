#include <math.h>
#include <string.h>

#include "jogo.h"
#include "combate.h"
#include "entrada.h"
#include "inimigos.h"
#include "nave.h"
#include "placar.h"
#include "reforcos.h"
#include "render.h"
#include "telas.h"

static RelogioCombate relogio;
static int avisoArena;   /* 1 enquanto a nave está além do RAIO_ARENA; o HUD mostra o aviso */

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
    g->cascoDestroier = CASCO_MAX_DESTROIER;
    g->proximoId = 1;
    gerarReforcos(&g->reforcos, DURACAO_PARTIDA);
    iniciarJogador(&g->jogador);
    relogio = (RelogioCombate){ 0 };
    avisoArena = 0;
}

/* Derrota tem precedência sobre o tempo esgotado. */
static int verificarFim(Jogo *g)
{
    if (g->jogador.casco <= 0.0f || g->cascoDestroier <= 0.0f) {
        g->vitoria = 0;
        return 1;
    }
    if (g->tempoRestante <= 0.0f) {
        g->vitoria = 1;
        return 1;
    }
    return 0;
}

/* Seção 3.4, passos 2 a 5: nave → reforços → inimigos → combate. */
static void passoCombate(Jogo *g, RelogioCombate *r, const Comandos *cmd)
{
    avisoArena = atualizarJogador(&g->jogador, cmd, PASSO_FIXO);
    colidirDestroier(g, PASSO_FIXO);

    /* O tempo do passo entra antes dos reforços: o grupo de t = 3 s nasce no passo 180. */
    r->passos++;
    g->tempoDecorrido = r->passos * PASSO_FIXO;
    g->tempoRestante = (PASSOS_PARTIDA - r->passos) * PASSO_FIXO;

    processarReforcos(g);               /* seção 3.4, passo 3 */
    atualizarInimigos(g, PASSO_FIXO);   /* seção 3.4, passo 4 */

    /* disparar vale enquanto segurado; o cooldown é contado em passos, então o disparo fica no passo.
     * resolverLasers roda logo depois de atualizarLasers: o segmento do laser é pos - vel * dt → pos. */
    if (cmd->disparar)
        dispararLaser(g);
    atualizarLasers(g, PASSO_FIXO);
    resolverLasers(g, PASSO_FIXO);
    colidirLasersInimigos(g, PASSO_FIXO);   /* TODO CMB-04: vibrar(&cfg, 0.8f, 0.4f) no acerto, quando o cfg chegar aqui */
    atualizarExplosoes(g, PASSO_FIXO);
}

/* Seção 3.4, passo 1: ações de borda valem uma vez por frame, mesmo em frames sem passo ou com vários. */
static void tratarMira(Jogo *g, const Comandos *cmd)
{
    if (cmd->proximoAlvo)
        proximoAlvo(&g->inimigos);
    if (cmd->alvoAnterior)
        alvoAnterior(&g->inimigos);
    if (cmd->alvoMaisProximo) {
        ordenarPorDistancia(&g->inimigos);
        g->inimigos.alvo = g->inimigos.inicio;
    }
}

EstadoJogo simularCombate(Jogo *g, RelogioCombate *r, const Comandos *cmd, float dtFrame)
{
    tratarMira(g, cmd);

    r->acumulador += dtFrame < DT_MAXIMO ? dtFrame : DT_MAXIMO;
    while (r->acumulador >= PASSO_FIXO) {
        r->acumulador -= PASSO_FIXO;
        passoCombate(g, r, cmd);
        if (verificarFim(g))   /* seção 3.4, passo 6 */
            return EST_FIM;
    }
    return EST_COMBATE;
}

static EstadoJogo combate(Jogo *g, const Comandos *cmd, int controleCaiu)
{
    EstadoJogo proximo = EST_PAUSA;

    if (!cmd->pausa && !controleCaiu)
        proximo = simularCombate(g, &relogio, cmd, GetFrameTime());

    render_cena(g, cameraDoJogador(&g->jogador));   /* também no frame que entra em pausa: sem piscar preto */
    /* Provisório até o HUD (VIS-06): arredonda para cima, então só mostra 0:00 no fim. */
    int segundos = (int)ceilf(g->tempoRestante);
    DrawText(TextFormat("COMBATE  %d:%02d", segundos / 60, segundos % 60), 20, 20, 20, RAYWHITE);
    DrawText("Esc/Start: pausa", 20, 50, 20, GRAY);
    if (avisoArena)
        DrawText("VOLTE PARA A ARENA", LARGURA_TELA / 2 - 140, 80, 28, RED);
    return proximo;
}

/* A cena fica congelada atrás do aviso. */
static EstadoJogo pausa(const Jogo *g, const Comandos *cmd)
{
    render_cena(g, cameraDoJogador(&g->jogador));
    DrawText("PAUSA", 20, 20, 40, RAYWHITE);
    DrawText("Esc/Start: continuar   Enter/A: voltar ao menu", 20, 70, 20, GRAY);

    if (cmd->pausa)
        return EST_COMBATE;
    if (cmd->confirmar)
        return EST_MENU;
    return EST_PAUSA;
}

/*
 * Transições:
 *   MENU      -> COMBATE (inicia partida) | PLACAR | CONTROLES | SAIR
 *   COMBATE   -> PAUSA (Esc/Start ou controle desconectado) | FIM
 *   PAUSA     -> COMBATE | MENU (libera a partida)
 *   FIM       -> PLACAR
 *   PLACAR    -> MENU
 *   CONTROLES -> MENU
 */
static EstadoJogo atualizarEstado(Jogo *g, ConfigEntrada *cfg, const Comandos *cmd, int controleCaiu)
{
    switch (g->estado) {
    case EST_MENU:      return telaMenu(g, cmd);
    case EST_COMBATE:   return combate(g, cmd, controleCaiu);
    case EST_PAUSA:     return pausa(g, cmd);
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
    int controleAntes = 0;

    InitWindow(LARGURA_TELA, ALTURA_TELA, "TIE Fighter: Esquadrão Imperial");
    ChangeDirectory(GetApplicationDirectory());
    SetExitKey(KEY_NULL);   /* Esc é pausa/voltar, não fecha a janela */
    SetTargetFPS(60);

    iniciarEntrada(&cfg);
    carregarAssets();
    jogo.estado = EST_MENU;

    while (jogo.estado != EST_SAIR && !WindowShouldClose()) {
        Comandos cmd = lerComandos(&cfg);
        int controleCaiu = controleAntes && !cmd.controleConectado;
        controleAntes = cmd.controleConectado;

        BeginDrawing();
        ClearBackground(BLACK);
        EstadoJogo proximo = atualizarEstado(&jogo, &cfg, &cmd, controleCaiu);
        EndDrawing();

        if (proximo != jogo.estado)
            trocarEstado(&jogo, proximo);
    }

    encerrarPartida(&jogo);
    descarregarAssets();
    liberarPlacar(jogo.placar);
    jogo.placar = NULL;
    CloseWindow();
}
