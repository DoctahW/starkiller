#include <math.h>
#include <string.h>

#include <SDL3/SDL.h>

#include "jogo.h"
#include "audio.h"
#include "aliados.h"
#include "combate.h"
#include "destroier.h"
#include "entrada.h"
#include "hud.h"
#include "inimigos.h"
#include "nave.h"
#include "placar.h"
#include "raymath.h"
#include "reforcos.h"
#include "render.h"
#include "telas.h"

static RelogioCombate relogio;
static int avisoArena;       /* 1 enquanto a nave está além do RAIO_ARENA; o HUD mostra o aviso */
static int pausaSelecionada; /* item do menu de pausa */

/* Libera as estruturas da partida em andamento; seguro de chamar mais de uma vez. */
static void encerrarPartida(Jogo *g)
{
    liberarListaInimigos(&g->inimigos);
    liberarFila(&g->reforcos);
    limparQuedasInimigas();
}

/* Zera o Jogo preservando o placar, gera os reforços e posiciona o jogador. */
static void iniciarPartida(Jogo *g)
{
    NoPlacar *placar = g->placar;

    encerrarPartida(g);
    memset(g, 0, sizeof *g);
    g->placar = placar;
    g->tempoRestante = DURACAO_PARTIDA;
    iniciarDestroier(g);
    g->proximoId = 1;
    gerarReforcos(&g->reforcos, DURACAO_ONDAS);
    iniciarJogador(&g->jogador);
    iniciarAliados(g);
    relogio = (RelogioCombate){ 0 };
    avisoArena = 0;
}

/*
 * Derrota tem precedência sobre o tempo esgotado. Com o casco do TIE em 0, a nave primeiro
 * rodopia (iniciarQuedaJogador + SPIN) e só depois explode (BOOM), como pede a AUD-01.
 * Se ainda há respawns, a partida continua e o jogador volta em TEMPO_RESPAWN_JOGADOR;
 * na última vida, a explosão encerra a partida.
 */
static int verificarFim(Jogo *g)
{
    if (g->jogador.casco <= 0.0f) {
        int fase = faseQuedaJogador();

        if (fase == 0) {
            iniciarQuedaJogador();
            audioSpinJogador();
            g->jogador.morto = 1;
            g->jogador.explodiu = 0;
            g->jogador.tempoRespawn = 0.0f;
            g->pontos = g->pontos > PENALIDADE_MORTE_JOGADOR
                ? g->pontos - PENALIDADE_MORTE_JOGADOR : 0;
            g->sequenciaSemDano = 0;
            return 0;
        }
        if (fase == 1)   /* ainda rodopiando */
            return 0;
        if (!g->jogador.explodiu) {
            criarExplosao(g, g->jogador.pos, RAIO_EXPLOSAO);
            audioBoomJogador(g->jogador.pos);
            g->jogador.explodiu = 1;
            g->jogador.tempoRespawn = TEMPO_RESPAWN_JOGADOR;
            g->jogador.tempoExplosao = 0.0f;
        }
        if (g->jogador.respawnsRestantes <= 0) {
            /* Deixa o zoom out da câmera mostrar a explosão antes do fim de jogo. */
            if (g->jogador.tempoExplosao < 1.8f)
                return 0;
            g->vitoria = 0;
            return 1;
        }
        return 0;
    }
    if (g->cascoDestroier <= 0.0f) {
        g->vitoria = 0;
        return 1;
    }
    if (g->tempoRestante <= 0.0f) {
        g->vitoria = 1;
        return 1;
    }
    return 0;
}

/* Trava a mira no caça mais próximo; a lista fica em ordem de distância. */
static void mirarMaisProximo(Jogo *g)
{
    ordenarPorDistancia(&g->inimigos);
    g->inimigos.alvo = g->inimigos.inicio;
}

/*
 * R/LT: trava o caça que está sob a retícula, não o mais próximo em geral.
 * Escolhe o menor desvio angular em relação à frente da nave, dentro de um cone curto;
 * se nenhum estiver na mira, mantém o alvo atual.
 */
static void mirarNaReticula(Jogo *g)
{
    const Jogador *j = &g->jogador;
    NoInimigo *melhor = NULL;
    float melhorCosseno = COSSENO_CONE_MIRA;
    float melhorDist = 1e30f;

    for (NoInimigo *no = g->inimigos.inicio; no != NULL; no = no->prox) {
        Vector3 rel = Vector3Subtract(no->dado.pos, j->pos);
        float dist = Vector3Length(rel);
        float cosseno;

        if (dist < 1e-4f) {
            melhor = no;
            melhorCosseno = 1.0f;
            melhorDist = 0.0f;
            continue;
        }
        cosseno = Vector3DotProduct(Vector3Scale(rel, 1.0f / dist), j->frente);
        if (cosseno < COSSENO_CONE_MIRA)
            continue;
        if (melhor == NULL || cosseno > melhorCosseno + 1e-5f
            || (fabsf(cosseno - melhorCosseno) <= 1e-5f && dist < melhorDist)) {
            melhor = no;
            melhorCosseno = cosseno;
            melhorDist = dist;
        }
    }
    if (melhor != NULL)
        g->inimigos.alvo = melhor;
}

/* Seção 3.4, passos 2 a 5: nave → reforços → inimigos → combate. */
static void passoCombate(Jogo *g, RelogioCombate *r, const Comandos *cmd)
{
    int tinhaAlvo, idAlvo;

    /* Antes de qualquer dano do passo: colisão e laser armam o feedback inteiro (DURACAO_FEEDBACK_DANO). */
    if (g->tempoDano > 0.0f) {
        g->tempoDano -= PASSO_FIXO;
        if (g->tempoDano < 0.0f)
            g->tempoDano = 0.0f;
    }
    atualizarEscudoDestroier(g, PASSO_FIXO);

    /* Morto e explodido: o mundo continua, mas o TIE do jogador só volta no respawn. */
    if (g->jogador.explodiu) {
        g->jogador.tempoExplosao += PASSO_FIXO;
        if (g->jogador.morto && g->jogador.respawnsRestantes > 0) {
            g->jogador.tempoRespawn -= PASSO_FIXO;
            if (g->jogador.tempoRespawn <= 0.0f) {
                respawnJogador(g);
                g->tempoDano = 0.0f;
                avisoArena = 0;
            }
        }
    }

    if (!g->jogador.morto) {
        avisoArena = atualizarJogador(&g->jogador, cmd, PASSO_FIXO);
        colidirDestroier(g, PASSO_FIXO);
        if (cmd->reparar)
            repararJogador(g);
    } else if (faseQuedaJogador() == 1) {
        /* A queda precisa continuar girando para terminar e liberar a explosão/respawn. */
        avisoArena = atualizarJogador(&g->jogador, cmd, PASSO_FIXO);
    }

    /* O tempo do passo entra antes dos reforços: o grupo de t = 3 s nasce no passo 180. */
    r->passos++;
    g->tempoDecorrido = r->passos * PASSO_FIXO;
    g->tempoRestante = (PASSOS_PARTIDA - r->passos) * PASSO_FIXO;

    processarReforcos(g);               /* seção 3.4, passo 3 */
    atualizarInimigos(g, PASSO_FIXO);   /* seção 3.4, passo 4 */
    atualizarAliados(g, PASSO_FIXO);

    /* disparar vale enquanto segurado; o cooldown é contado em passos, então o disparo fica no passo.
     * resolverLasers roda logo depois de atualizarLasers: o segmento do laser é pos - vel * dt → pos. */
    if (cmd->disparar && !g->jogador.morto)
        dispararLaser(g);
    atualizarLasers(g, PASSO_FIXO);
    tinhaAlvo = g->inimigos.alvo != NULL;
    idAlvo = tinhaAlvo ? g->inimigos.alvo->dado.id : 0;
    resolverLasers(g, PASSO_FIXO);
    /* Alvo abatido: removerInimigo passa a mira ao vizinho da lista; o mais próximo é mais útil. */
    if (tinhaAlvo && (g->inimigos.alvo == NULL || g->inimigos.alvo->dado.id != idAlvo)) {
        mirarMaisProximo(g);
        if (g->inimigos.alvo != NULL)
            audioTrocaAlvo();
    }
    colidirLasersInimigos(g, PASSO_FIXO);
    atualizarQuedasInimigas(g, PASSO_FIXO);
    atualizarExplosoes(g, PASSO_FIXO);
}

/*
 * Seção 3.4, passo 1: ações de borda valem uma vez por frame, mesmo em frames sem passo ou com vários.
 * Próximo e anterior andam na lista em ordem de distância: do alvo atual para o seguinte mais longe
 * ou mais perto, nunca para um caça qualquer do outro lado da arena.
 */
static void tratarMira(Jogo *g, const Comandos *cmd)
{
    NoInimigo *antes = g->inimigos.alvo;

    if (cmd->proximoAlvo || cmd->alvoAnterior)
        ordenarPorDistancia(&g->inimigos);
    if (cmd->proximoAlvo)
        proximoAlvo(&g->inimigos);
    if (cmd->alvoAnterior)
        alvoAnterior(&g->inimigos);
    if (cmd->alvoNaMira)
        mirarNaReticula(g);
    if (g->inimigos.alvo != antes && g->inimigos.alvo != NULL)
        audioTrocaAlvo();
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

static EstadoJogo combate(Jogo *g, const ConfigEntrada *cfg, const Comandos *cmd, int controleCaiu)
{
    EstadoJogo proximo = EST_PAUSA;

    if (!cmd->pausa && !controleCaiu) {
        float vidaAntes = g->jogador.casco;
        int abatesAntes = g->abates;

        proximo = simularCombate(g, &relogio, cmd, GetFrameTime());
        /* Dano ao jogador pesa mais que o abate; o abate só vibra se não houve dano no frame. */
        if (g->jogador.casco < vidaAntes)
            vibrar(cfg, 0.8f, 0.4f);
        else if (g->abates > abatesAntes)
            vibrar(cfg, 0.6f, 0.3f);
    }

    render_cena(g, cameraComTremor(g));   /* também no frame que entra em pausa: sem piscar preto */
    hud_desenhar(g, avisoArena);
    return proximo;
}

/* A cena e a cabine ficam congeladas e escurecidas atrás do menu. */
static EstadoJogo pausa(const Jogo *g, const Comandos *cmd)
{
    int antes = pausaSelecionada;

    if (cmd->menuCima)
        pausaSelecionada = (pausaSelecionada + PAUSA_QTD - 1) % PAUSA_QTD;
    if (cmd->menuBaixo)
        pausaSelecionada = (pausaSelecionada + 1) % PAUSA_QTD;
    if (pausaSelecionada != antes)
        audioUIMover();

    render_cena(g, cameraComTremor(g));
    hud_desenhar(g, avisoArena);
    telasDesenharPausa(pausaSelecionada);

    if (cmd->pausa || cmd->voltar) {
        audioUIVoltar();
        return EST_COMBATE;
    }
    if (cmd->confirmar) {
        audioUISelecionar();
        if (pausaSelecionada == PAUSA_OPCOES) {
            telasOpcoesDefinirOrigem(EST_PAUSA);
            return EST_CONTROLES;
        }
        if (pausaSelecionada == PAUSA_VOLTAR_MENU)
            return EST_MENU;
        return EST_COMBATE;
    }
    return EST_PAUSA;
}

/*
 * Transições:
 *   MENU      -> COMBATE (inicia partida) | PLACAR | CONTROLES (Opções) | SAIR
 *   COMBATE   -> PAUSA (Esc/Start ou controle desconectado) | FIM
 *   PAUSA     -> COMBATE | CONTROLES (Opções) | MENU (libera a partida)
 *   CONTROLES -> MENU (origem menu) | PAUSA (origem pausa)
 *   FIM       -> PLACAR
 *   PLACAR    -> MENU
 */
static EstadoJogo atualizarEstado(Jogo *g, ConfigEntrada *cfg, const Comandos *cmd, int controleCaiu)
{
    switch (g->estado) {
    case EST_MENU:      return telaMenu(g, cmd);
    case EST_COMBATE:   return combate(g, cfg, cmd, controleCaiu);
    case EST_PAUSA:     return pausa(g, cmd);
    case EST_FIM:       return telaFim(g, cmd);
    case EST_PLACAR:    return telaPlacar(g, cmd);
    case EST_CONTROLES: return telaControles(cfg, cmd);
    case EST_SAIR:      break;
    }
    return EST_SAIR;
}

/* Faixa de fundo de cada estado; Opções herda o contexto de quem a abriu. */
static void definirMusica(const Jogo *g, EstadoJogo anterior, EstadoJogo novo)
{
    switch (novo) {
    case EST_MENU:
        audioMusicaTocar(AUDIO_MUSICA_MENU, 1.0f);
        break;
    case EST_COMBATE:
        audioMusicaTocar(AUDIO_MUSICA_COMBATE, 1.0f);
        break;
    case EST_PAUSA:
        audioMusicaTocar(AUDIO_MUSICA_COMBATE, AUDIO_FATOR_PAUSA);
        break;
    case EST_CONTROLES:
        if (anterior == EST_PAUSA)
            audioMusicaTocar(AUDIO_MUSICA_COMBATE, AUDIO_FATOR_PAUSA);
        else
            audioMusicaTocar(AUDIO_MUSICA_MENU, 1.0f);
        break;
    case EST_FIM:
    case EST_PLACAR:
        /* Derrota fica em silêncio; vitória leva a fanfarra também para o placar. */
        if (g->vitoria == 1)
            audioMusicaTocar(AUDIO_MUSICA_VITORIA, 1.0f);
        else
            audioMusicaTocar(AUDIO_MUSICA_NENHUMA, 0.0f);
        break;
    case EST_SAIR:
        audioMusicaTocar(AUDIO_MUSICA_NENHUMA, 0.0f);
        break;
    }
}

static void trocarEstado(Jogo *g, EstadoJogo novo)
{
    EstadoJogo anterior = g->estado;

    if (anterior == EST_MENU && novo == EST_COMBATE)
        iniciarPartida(g);
    else if (novo == EST_MENU)
        encerrarPartida(g);

    if (novo == EST_PAUSA && anterior == EST_COMBATE)
        pausaSelecionada = 0;

    g->estado = novo;
    definirMusica(g, anterior, novo);
}

void jogo_executar(void)
{
    static Jogo jogo;
    ConfigEntrada cfg;
    int controleAntes = 0;

    /*
     * O backend SDL da raylib não pede profundidade e a SDL cria o framebuffer com 16 bits: as
     * camadas quase coplanares do casco do Destróier brigam no z-buffer e serrilham em movimento.
     * O atributo vale para a janela criada pelo InitWindow; o SDL_Quit do CloseWindow encerra o vídeo.
     */
    if (!SDL_InitSubSystem(SDL_INIT_VIDEO) || !SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24))
        TraceLog(LOG_WARNING, "JOGO: sem profundidade de 24 bits: %s", SDL_GetError());
    SetConfigFlags(FLAG_WINDOW_RESIZABLE);
    InitWindow(LARGURA_TELA, ALTURA_TELA, "TIE Fighter: Esquadrão Imperial");
    SetWindowMinSize(960, 540);
    SetWindowState(FLAG_WINDOW_MAXIMIZED);   /* abre grande, mas ainda em janela */
    ChangeDirectory(GetApplicationDirectory());
    jogo.placar = carregarPlacar(CAMINHO_PLACAR);
    SetExitKey(KEY_NULL);   /* Esc é pausa/voltar, não fecha a janela */
    SetTargetFPS(60);

    /* A cena e a HUD são desenhadas em 1280×720 e escaladas para a janela, sem esticar. */
    RenderTexture2D tela = LoadRenderTexture(LARGURA_TELA, ALTURA_TELA);
    SetTextureFilter(tela.texture, TEXTURE_FILTER_BILINEAR);

    iniciarEntrada(&cfg);
    carregarAssets();
    hud_carregar();
    iniciarAudio();
    jogo.estado = EST_MENU;
    audioMusicaTocar(AUDIO_MUSICA_MENU, 1.0f);

    while (jogo.estado != EST_SAIR && !WindowShouldClose()) {
        Comandos cmd = lerComandos(&cfg);
        int controleCaiu = controleAntes && !cmd.controleConectado;
        int larguraTela, alturaTela;
        float escala, larguraDestino, alturaDestino;
        Rectangle origem = { 0.0f, 0.0f, (float)tela.texture.width, -(float)tela.texture.height };
        Rectangle destino;

        controleAntes = cmd.controleConectado;
        if (IsKeyPressed(KEY_F11))
            ToggleBorderlessWindowed();   /* tela cheia sem modo exclusivo */

        BeginTextureMode(tela);
        ClearBackground(BLACK);
        EstadoJogo proximo = atualizarEstado(&jogo, &cfg, &cmd, controleCaiu);
        EndTextureMode();

        audioAtualizar(&jogo, &cmd, jogo.estado == EST_COMBATE, GetFrameTime());

        larguraTela = GetScreenWidth();
        alturaTela = GetScreenHeight();
        escala = fminf((float)larguraTela / LARGURA_TELA, (float)alturaTela / ALTURA_TELA);
        larguraDestino = LARGURA_TELA * escala;
        alturaDestino = ALTURA_TELA * escala;
        destino = (Rectangle){ ((float)larguraTela - larguraDestino) * 0.5f,
                               ((float)alturaTela - alturaDestino) * 0.5f,
                               larguraDestino, alturaDestino };

        BeginDrawing();
        ClearBackground(BLACK);
        DrawTexturePro(tela.texture, origem, destino, (Vector2){ 0.0f, 0.0f }, 0.0f, WHITE);
        EndDrawing();

        if (proximo != jogo.estado)
            trocarEstado(&jogo, proximo);
    }

    UnloadRenderTexture(tela);
    encerrarPartida(&jogo);
    descarregarAssets();
    liberarPlacar(jogo.placar);
    jogo.placar = NULL;
    descarregarTelas();
    hud_descarregar();
    encerrarAudio();
    CloseWindow();
}
