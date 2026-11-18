#include <math.h>
#include <string.h>

#include "audio.h"
#include "entrada.h"
#include "placar.h"
#include "pontuacao.h"
#include "telas.h"

#define TAM_MENU   30
#define TAM_DICA   20

#define MENU_CENTRO_X 0.28f /* fração da largura; cai na área escura da arte */
#define MENU_TOPO_Y   342
#define MENU_ESPACO   42
#define MENU_SOMBRA   2

#define FONTE_MENU   "assets/fonts/ChakraPetch-Bold.ttf"
#define ESPACO_FONTE 1.0f
#define RANKING_MAX  10

#define TAM_FIM_TITULO 48
#define TAM_FIM_TEXTO  26
#define TAM_FIM_NOME   30
#define TAM_FIM_DICA   20
#define LARGURA_NOME   440
#define ALTURA_NOME    52
#define Y_NOME         378
#define Y_LETRAS       462
#define Y_DICA_FIM     540

static Texture2D fundoMenu;
static int fundoMenuCarregado;
static Font fonteMenu;
static int fonteMenuCarregada;

static char nomeFim[PLACAR_MAX_NOME + 1];
static int nomeTam;
static int nomeCursor;
static char letraAtual = 'A';

static char ultimoNome[PLACAR_MAX_NOME + 1];
static int ultimosPontos;
static int destacarUltima;

static EstadoJogo telaAnterior = EST_SAIR;
static int aguardandoTeclasSoltas;
static double fimBloqueadoAte;
static int erroSalvarPlacar;

/* Tela de Opções: de onde ela foi aberta e aviso de que o estado interno deve reiniciar. */
static EstadoJogo origemOpcoes = EST_MENU;
static int opcoesRecemAbertas;

void telasOpcoesDefinirOrigem(EstadoJogo origem)
{
    origemOpcoes = origem == EST_PAUSA ? EST_PAUSA : EST_MENU;
    opcoesRecemAbertas = 1;
}

static int entrouEm(EstadoJogo tela)
{
    if (telaAnterior == tela)
        return 0;
    telaAnterior = tela;
    return 1;
}

/* Teclas que produzem texto (espaço, letras e pontuação) ainda pressionadas. */
static int algumaTeclaDeTextoPressionada(void)
{
    int k;

    for (k = 32; k <= 96; k++)
        if (IsKeyDown(k))
            return 1;
    return 0;
}

/* Translitera Latin-1 para ASCII: o placar guarda no máximo 15 bytes. */
static int letraAscii(int c)
{
    if (c >= 32 && c <= 126)
        return c;
    if (c >= 0xC0 && c <= 0xC5)
        return 'A';
    if (c == 0xC7)
        return 'C';
    if (c >= 0xC8 && c <= 0xCB)
        return 'E';
    if (c >= 0xCC && c <= 0xCF)
        return 'I';
    if (c == 0xD1)
        return 'N';
    if (c >= 0xD2 && c <= 0xD6)
        return 'O';
    if (c >= 0xD9 && c <= 0xDC)
        return 'U';
    if (c == 0xDD)
        return 'Y';
    if (c >= 0xE0 && c <= 0xE5)
        return 'a';
    if (c == 0xE7)
        return 'c';
    if (c >= 0xE8 && c <= 0xEB)
        return 'e';
    if (c >= 0xEC && c <= 0xEF)
        return 'i';
    if (c == 0xF1)
        return 'n';
    if (c >= 0xF2 && c <= 0xF6)
        return 'o';
    if (c >= 0xF9 && c <= 0xFC)
        return 'u';
    if (c == 0xFD || c == 0xFF)
        return 'y';
    return 0;
}

/* Carrega a fonte uma única vez, com ASCII e Latin-1 para os acentos. */
static Font fonteDoMenu(void)
{
    if (!fonteMenuCarregada) {
        static int pontos[0x60 + 0x60 + 4]; /* ASCII, Latin-1 e setas */
        int quantidade = 0;
        int c;

        for (c = 0x20; c <= 0x7E; c++)
            pontos[quantidade++] = c;
        for (c = 0xA0; c <= 0xFF; c++) {
            if (c == 0xB5) /* micro sign não existe na Chakra Petch */
                continue;
            pontos[quantidade++] = c;
        }
        for (c = 0x2190; c <= 0x2193; c++) /* ← ↑ → ↓ */
            pontos[quantidade++] = c;

        fonteMenu = LoadFontEx(FONTE_MENU, 48, pontos, quantidade);
        if (fonteMenu.texture.id != 0)
            SetTextureFilter(fonteMenu.texture, TEXTURE_FILTER_BILINEAR);
        fonteMenuCarregada = 1;
    }

    return fonteMenu.texture.id != 0 ? fonteMenu : GetFontDefault();
}

static void textoFonte(const char *texto, int x, int y, int tamanho, Color cor)
{
    Font fonte = fonteDoMenu();

    DrawTextEx(fonte, texto,
               (Vector2){ (float)(x + MENU_SOMBRA), (float)(y + MENU_SOMBRA) },
               (float)tamanho, ESPACO_FONTE, Fade(BLACK, 0.75f));
    DrawTextEx(fonte, texto, (Vector2){ (float)x, (float)y },
               (float)tamanho, ESPACO_FONTE, cor);
}

static void textoNoMenu(const char *texto, int centroX, int y, int tamanho, Color cor)
{
    float largura = MeasureTextEx(fonteDoMenu(), texto, (float)tamanho, ESPACO_FONTE).x;

    textoFonte(texto, centroX - (int)(largura / 2.0f), y, tamanho, cor);
}

static void textoFonteDireita(const char *texto, int direita, int y, int tamanho, Color cor)
{
    float largura = MeasureTextEx(fonteDoMenu(), texto, (float)tamanho, ESPACO_FONTE).x;

    textoFonte(texto, direita - (int)largura, y, tamanho, cor);
}

static void desenharFundoMenu(void)
{
    if (!fundoMenuCarregado) {
        fundoMenu = LoadTexture("assets/img/Img_menu.png");
        if (fundoMenu.id != 0)
            SetTextureFilter(fundoMenu, TEXTURE_FILTER_BILINEAR);
        fundoMenuCarregado = 1;
    }

    if (fundoMenu.id != 0) {
        DrawTexturePro(fundoMenu,
                       (Rectangle){ 0.0f, 0.0f, (float)fundoMenu.width, (float)fundoMenu.height },
                       (Rectangle){ 0.0f, 0.0f, (float)GetScreenWidth(), (float)GetScreenHeight() },
                       (Vector2){ 0.0f, 0.0f }, 0.0f, WHITE);
    }
}

EstadoJogo telaMenu(Jogo *g, const Comandos *cmd)
{
    static const char *opcoes[] = { "Iniciar Missão", "Placar de Pilotos", "Opções", "Sair" };
    static const EstadoJogo destinos[] = { EST_COMBATE, EST_PLACAR, EST_CONTROLES, EST_SAIR };
    enum { QTD_OPCOES = sizeof opcoes / sizeof opcoes[0] };
    static int selecionada = 0;
    int centroX = (int)(GetScreenWidth() * MENU_CENTRO_X);
    int antes = selecionada;
    int i;

    (void)entrouEm(EST_MENU);
    (void)g;

    if (cmd->menuCima)
        selecionada = (selecionada + QTD_OPCOES - 1) % QTD_OPCOES;
    if (cmd->menuBaixo)
        selecionada = (selecionada + 1) % QTD_OPCOES;
    if (selecionada != antes)
        audioUIMover();

    desenharFundoMenu();

    for (i = 0; i < QTD_OPCOES; i++) {
        int y = MENU_TOPO_Y + i * MENU_ESPACO;

        if (i == selecionada) {
            Font fonte = fonteDoMenu();
            float largura = MeasureTextEx(fonte, opcoes[i], (float)TAM_MENU, ESPACO_FONTE).x;
            float xMarcador = (float)centroX - largura / 2.0f - 26.0f;

            DrawTextEx(fonte, ">",
                       (Vector2){ xMarcador + MENU_SOMBRA, (float)(y + MENU_SOMBRA) },
                       (float)TAM_MENU, ESPACO_FONTE, Fade(BLACK, 0.75f));
            DrawTextEx(fonte, ">", (Vector2){ xMarcador, (float)y },
                       (float)TAM_MENU, ESPACO_FONTE, YELLOW);
        }
        textoNoMenu(opcoes[i], centroX, y, TAM_MENU, i == selecionada ? YELLOW : LIGHTGRAY);
    }

    textoNoMenu("Setas: navegar   Enter/A: selecionar",
                centroX, MENU_TOPO_Y + QTD_OPCOES * MENU_ESPACO + 16, TAM_DICA, LIGHTGRAY);

    if (cmd->confirmar) {
        audioUISelecionar();
        if (destinos[selecionada] == EST_CONTROLES)
            telasOpcoesDefinirOrigem(EST_MENU);
        return destinos[selecionada];
    }
    return EST_MENU;
}

static const char LETRAS_FIM[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789 .-";

static char proximaLetra(char atual)
{
    const char *p = strchr(LETRAS_FIM, atual);

    if (p == NULL || p[1] == '\0')
        return LETRAS_FIM[0];
    return p[1];
}

static char letraAnterior(char atual)
{
    const char *p = strchr(LETRAS_FIM, atual);

    if (p == NULL || p == LETRAS_FIM)
        return LETRAS_FIM[sizeof LETRAS_FIM - 2];
    return p[-1];
}

static void apagarLetraNome(void)
{
    if (nomeCursor <= 0)
        return;

    memmove(&nomeFim[nomeCursor - 1], &nomeFim[nomeCursor],
            (size_t)(nomeTam - nomeCursor + 1));
    nomeTam--;
    nomeCursor--;
}

static void inserirLetraNome(char c)
{
    if (nomeTam >= PLACAR_MAX_NOME)
        return;

    if (nomeCursor < 0)
        nomeCursor = 0;
    if (nomeCursor > nomeTam)
        nomeCursor = nomeTam;

    memmove(&nomeFim[nomeCursor + 1], &nomeFim[nomeCursor],
            (size_t)(nomeTam - nomeCursor + 1));
    nomeFim[nomeCursor] = c;
    nomeTam++;
    nomeCursor++;
}

static const char *nomeFinalFim(void)
{
    static char limpo[PLACAR_MAX_NOME + 1];
    int ini = 0;
    int fim = nomeTam;
    int n;

    while (ini < fim && nomeFim[ini] == ' ')
        ini++;
    while (fim > ini && nomeFim[fim - 1] == ' ')
        fim--;

    n = fim - ini;
    if (n == 0)
        return PLACAR_NOME_ANONIMO;

    memcpy(limpo, &nomeFim[ini], (size_t)n);
    limpo[n] = '\0';
    return limpo;
}

static void atualizarNomeFim(const Comandos *cmd)
{
    int c;
    int cursorAntes = nomeCursor;
    int editou = 0;
    int moveu = 0;
    char letraAntes = letraAtual;

    if (aguardandoTeclasSoltas) {
        while (GetCharPressed() != 0) { }
        if (GetTime() >= fimBloqueadoAte && !algumaTeclaDeTextoPressionada())
            aguardandoTeclasSoltas = 0;
        return;
    }

    while ((c = GetCharPressed()) != 0) {
        int ascii = letraAscii(c);

        if (ascii != 0 && ascii != ';') {
            int antes = nomeTam;

            inserirLetraNome((char)ascii);
            if (nomeTam > antes) {
                audioUITecla();
                editou = 1;
            }
        }
    }

    if (IsKeyPressed(KEY_BACKSPACE) || (cmd->usandoControle && cmd->voltar)) {
        int antes = nomeTam;

        apagarLetraNome();
        if (nomeTam < antes) {
            audioUIVoltar();
            editou = 1;
        }
    }

    if (cmd->menuEsquerda && nomeCursor > 0)
        nomeCursor--;

    if (cmd->menuDireita) {
        if (cmd->usandoControle && nomeCursor == nomeTam) {
            int antes = nomeTam;

            inserirLetraNome(letraAtual);
            if (nomeTam > antes) {
                audioUITecla();
                editou = 1;
            }
        } else if (nomeCursor < nomeTam) {
            nomeCursor++;
        }
    }

    if (cmd->usandoControle) {
        if (cmd->menuCima)
            letraAtual = letraAnterior(letraAtual);
        else if (cmd->menuBaixo)
            letraAtual = proximaLetra(letraAtual);

        if ((cmd->menuCima || cmd->menuBaixo) && nomeCursor < nomeTam)
            nomeFim[nomeCursor] = letraAtual;
    }

    if (nomeCursor != cursorAntes || letraAtual != letraAntes)
        moveu = 1;
    if (moveu && !editou)
        audioUIMover();

    if (nomeCursor < nomeTam)
        letraAtual = nomeFim[nomeCursor];
}

static void desenharSeletorLetras(void)
{
    Font fonte = fonteDoMenu();
    const char *p;
    const float tamanho = 18.0f;
    const float espaco = 6.0f;
    float largura = 0.0f;
    float x;

    for (p = LETRAS_FIM; *p != '\0'; p++) {
        char letra[2] = { *p, '\0' };

        largura += MeasureTextEx(fonte, letra, tamanho, ESPACO_FONTE).x + espaco;
    }

    x = ((float)GetScreenWidth() - largura) / 2.0f;
    for (p = LETRAS_FIM; *p != '\0'; p++) {
        char letra[2] = { *p, '\0' };
        float larguraLetra = MeasureTextEx(fonte, letra, tamanho, ESPACO_FONTE).x;

        if (*p == letraAtual)
            DrawRectangleLines((int)x - 2, Y_LETRAS - 2, (int)larguraLetra + 4, 22, YELLOW);
        DrawTextEx(fonte, letra, (Vector2){ x, (float)Y_LETRAS }, tamanho, ESPACO_FONTE,
                   *p == letraAtual ? YELLOW : GRAY);
        x += larguraLetra + espaco;
    }
}

static void desenharNomeFim(void)
{
    Font fonte = fonteDoMenu();
    int x = (GetScreenWidth() - LARGURA_NOME) / 2;
    int tx = x + 16;
    int ty = Y_NOME + 11;
    char antes[PLACAR_MAX_NOME + 1];
    float xCursor;
    int i;

    DrawRectangleLines(x, Y_NOME, LARGURA_NOME, ALTURA_NOME, GRAY);
    DrawTextEx(fonte, nomeFim, (Vector2){ (float)tx, (float)ty },
               TAM_FIM_NOME, ESPACO_FONTE, RAYWHITE);

    for (i = 0; i < nomeCursor; i++)
        antes[i] = nomeFim[i];
    antes[nomeCursor] = '\0';
    xCursor = (float)tx + MeasureTextEx(fonte, antes, TAM_FIM_NOME, ESPACO_FONTE).x;

    if (((int)(GetTime() * 2.0)) % 2 == 0)
        DrawRectangle((int)xCursor, ty, 2, TAM_FIM_NOME, YELLOW);
}

EstadoJogo telaFim(Jogo *g, const Comandos *cmd)
{
    int centro = GetScreenWidth() / 2;
    int bonus = bonusCasco(g);
    int total = calcularPontuacao(g);

    if (entrouEm(EST_FIM)) {
        while (GetCharPressed() != 0) { } /* descarta o que estava na fila */
        fimBloqueadoAte = GetTime() + 0.45;
        aguardandoTeclasSoltas = 1;
    }

    atualizarNomeFim(cmd);

    textoNoMenu(g->vitoria ? "VITÓRIA" : "DERROTA", centro, 70,
                TAM_FIM_TITULO, g->vitoria ? GREEN : RED);
    textoNoMenu(TextFormat("Abates: %d", g->abates), centro, 160, TAM_FIM_TEXTO, RAYWHITE);
    textoNoMenu(TextFormat("Pontos: %d", g->pontos), centro, 198, TAM_FIM_TEXTO, RAYWHITE);
    textoNoMenu(TextFormat("Bônus: %d", bonus), centro, 236, TAM_FIM_TEXTO, RAYWHITE);
    textoNoMenu(TextFormat("Total: %d", total), centro, 274, TAM_FIM_TEXTO, YELLOW);

    textoNoMenu("Digite seu nome:", centro, 336, 22, LIGHTGRAY);
    desenharNomeFim();

    if (cmd->usandoControle) {
        desenharSeletorLetras();
        textoNoMenu("↑/↓ letra   → adiciona   ← volta   B apaga   A confirma",
                    centro, Y_DICA_FIM, TAM_FIM_DICA, LIGHTGRAY);
    } else {
        textoNoMenu("Digite, Backspace apaga, Enter confirma",
                    centro, Y_NOME + ALTURA_NOME + 40, TAM_FIM_DICA, LIGHTGRAY);
    }

    if (cmd->confirmar && !aguardandoTeclasSoltas) {
        const char *nomeFinal = nomeFinalFim();

        audioUISelecionar();
        g->placar = inserirPontuacao(g->placar, nomeFinal, total);
        erroSalvarPlacar = salvarPlacar(g->placar, CAMINHO_PLACAR) ? 0 : 1;

        memcpy(ultimoNome, nomeFinal, strlen(nomeFinal) + 1);
        ultimosPontos = total;
        destacarUltima = 1;

        nomeTam = 0;
        nomeCursor = 0;
        nomeFim[0] = '\0';
        letraAtual = 'A';

        return EST_PLACAR;
    }

    return EST_FIM;
}

EstadoJogo telaPlacar(Jogo *g, const Comandos *cmd)
{
    NoPlacar ranking[RANKING_MAX];
    int total = exibirRankingEmOrdem(g->placar, ranking, RANKING_MAX);
    int centro = GetScreenWidth() / 2;
    int i;

    (void)entrouEm(EST_PLACAR);
    textoNoMenu("Placar de Pilotos", centro, 40, 40, RAYWHITE);

    if (total == 0) {
        textoNoMenu("Nenhuma partida registrada", centro, 300, 24, GRAY);
    } else {
        textoFonteDireita("#", 400, 140, 20, GRAY);
        textoFonte("Piloto", 440, 140, 20, GRAY);
        textoFonteDireita("Pontos", 900, 140, 20, GRAY);
        DrawLine(360, 172, 920, 172, DARKGRAY);

        for (i = 0; i < total; i++) {
            int y = 190 + i * 38;
            int recente = destacarUltima &&
                          strcmp(ranking[i].nome, ultimoNome) == 0 &&
                          ranking[i].pontos == ultimosPontos;
            Color cor = recente ? YELLOW : LIGHTGRAY;

            if (recente)
                DrawRectangle(350, y - 5, 590, 34, Fade(YELLOW, 0.10f));

            textoFonteDireita(TextFormat("%d", i + 1), 400, y, 26, cor);
            textoFonte(ranking[i].nome, 440, y, 26, cor);
            textoFonteDireita(TextFormat("%d", ranking[i].pontos), 900, y, 26, cor);
        }
    }

    textoNoMenu("Esc/B: voltar", centro, 640, 20, LIGHTGRAY);

    if (erroSalvarPlacar)
        textoNoMenu("Falha ao salvar o placar em disco.", centro, 610, 18, RED);

    if (cmd->voltar || cmd->confirmar) {
        audioUIVoltar();
        destacarUltima = 0;
        erroSalvarPlacar = 0;
        return EST_MENU;
    }
    return EST_PLACAR;
}

typedef struct {
    const char *acao;
    const char *teclado;
    const char *controle;
} LinhaControle;

static const LinhaControle CONTROLES[] = {
    { "Eixo Y (Inclinar)", "W / S", "Analógico esq. ↑ / ↓" },
    { "Eixo X (Virar)", "A / D", "Analógico esq. ← / →" },
    { "Rolagem", "Q / E", "LB / RB" },
    { "Acelerar / frear", "Shift / Ctrl", "Analógico dir. ↑ / ↓" },
    { "Disparar lasers", "Espaço", "RT" },
    { "Próximo / anterior alvo", "T / Y", "D-pad → / ←" },
    { "Alvo mais próximo", "R", "LT" },
    { "Pausa", "Esc", "Start" },
    { "Navegar nos menus", "Setas", "D-pad / analógico esq." },
    { "Confirmar", "Enter", "A" },
    { "Voltar", "Esc / Backspace", "B" },
};

enum { QTD_CONTROLES = (int)(sizeof CONTROLES / sizeof CONTROLES[0]) };

static const char *ROTULOS_CONFIG[] = {
    "Sensibilidade", "Zona morta", "Inverter eixo Y", "Vibração"
};

enum { QTD_CONFIG = (int)(sizeof ROTULOS_CONFIG / sizeof ROTULOS_CONFIG[0]) };

static const char *ROTULOS_AUDIO[] = {
    "Volume geral", "Efeitos", "Música"
};

enum { QTD_AUDIO = (int)(sizeof ROTULOS_AUDIO / sizeof ROTULOS_AUDIO[0]) };

enum { ABA_CONTROLES = 0, ABA_AUDIO, QTD_ABAS };

/* Ajusta uma opção de entrada e devolve 1 se o valor mudou. */
static int ajustarConfig(ConfigEntrada *cfg, int selecionada, int direcao)
{
    float sensibilidadeAntes = cfg->sensibilidade;
    float zonaMortaAntes = cfg->zonaMorta;
    int inverterAntes = cfg->inverterArfagem;
    int vibracaoAntes = cfg->vibracao;

    switch (selecionada) {
    case 0:
        cfg->sensibilidade += direcao * 0.05f;
        if (cfg->sensibilidade < SENSIBILIDADE_MIN)
            cfg->sensibilidade = SENSIBILIDADE_MIN;
        if (cfg->sensibilidade > SENSIBILIDADE_MAX)
            cfg->sensibilidade = SENSIBILIDADE_MAX;
        cfg->sensibilidade = roundf(cfg->sensibilidade * 20.0f) / 20.0f;
        break;
    case 1:
        cfg->zonaMorta += direcao * 0.05f;
        if (cfg->zonaMorta < ZONA_MORTA_MIN)
            cfg->zonaMorta = ZONA_MORTA_MIN;
        if (cfg->zonaMorta > ZONA_MORTA_MAX)
            cfg->zonaMorta = ZONA_MORTA_MAX;
        cfg->zonaMorta = roundf(cfg->zonaMorta * 20.0f) / 20.0f;
        break;
    case 2:
        if (direcao > 0)
            cfg->inverterArfagem = 1;
        else if (direcao < 0)
            cfg->inverterArfagem = 0;
        else
            cfg->inverterArfagem = !cfg->inverterArfagem;
        break;
    default:
        if (direcao > 0)
            cfg->vibracao = 1;
        else if (direcao < 0)
            cfg->vibracao = 0;
        else
            cfg->vibracao = !cfg->vibracao;
        break;
    }

    if (cfg->sensibilidade == sensibilidadeAntes && cfg->zonaMorta == zonaMortaAntes
        && cfg->inverterArfagem == inverterAntes && cfg->vibracao == vibracaoAntes)
        return 0;

    salvarConfigEntrada(cfg, CAMINHO_CONFIG);
    return 1;
}

/* Ajusta um volume em passos de 5% e devolve 1 se mudou. */
static int ajustarVolume(ConfigAudio *cfg, int selecionada, int direcao)
{
    float *valor;
    float novo;

    if (selecionada == 0)
        valor = &cfg->geral;
    else if (selecionada == 1)
        valor = &cfg->efeitos;
    else
        valor = &cfg->musica;

    novo = roundf((*valor + direcao * AUDIO_VOLUME_PASSO) * 20.0f) / 20.0f;
    if (novo < AUDIO_VOLUME_MIN)
        novo = AUDIO_VOLUME_MIN;
    if (novo > AUDIO_VOLUME_MAX)
        novo = AUDIO_VOLUME_MAX;
    if (novo == *valor)
        return 0;

    *valor = novo;
    audioAplicarVolumes();
    audioSalvarConfiguracao();
    return 1;
}

static void desenharBarraVolume(int x, int y, int largura, int altura, float valor)
{
    int preenchida;

    if (valor < 0.0f)
        valor = 0.0f;
    if (valor > 1.0f)
        valor = 1.0f;
    preenchida = (int)(largura * valor);

    DrawRectangle(x, y, largura, altura, Fade(GRAY, 0.25f));
    DrawRectangle(x, y, preenchida, altura, YELLOW);
    DrawRectangleLines(x, y, largura, altura, GRAY);
}

static void desenharAbas(int aba, int foco)
{
    static const char *ABAS[] = { "Controles", "Áudio" };
    int centro = GetScreenWidth() / 2;
    int x[QTD_ABAS];
    int i;

    x[0] = centro - 150;
    x[1] = centro + 150;

    for (i = 0; i < QTD_ABAS; i++) {
        Font fonte = fonteDoMenu();
        float largura = MeasureTextEx(fonte, ABAS[i], 24.0f, ESPACO_FONTE).x;
        Color cor = i == aba ? YELLOW : LIGHTGRAY;

        textoNoMenu(ABAS[i], x[i], 62, 24, cor);
        DrawLine(x[i] - (int)(largura / 2.0f), 92,
                 x[i] + (int)(largura / 2.0f), 92, i == aba ? YELLOW : DARKGRAY);
        if (i == aba && foco == 0)
            DrawRectangleLines(x[i] - (int)(largura / 2.0f) - 12, 54,
                               (int)largura + 24, 42, YELLOW);
    }
}

EstadoJogo telaControles(ConfigEntrada *cfg, const Comandos *cmd)
{
    static int aba = ABA_CONTROLES;
    static int foco = 0;   /* 0 = barra de abas; 1..N = linha da aba atual */
    ConfigAudio *acfg = audioConfig();
    int centro = GetScreenWidth() / 2;
    int nLinhas;
    int antesAba, antesFoco;
    int i;

    if (opcoesRecemAbertas) {
        aba = ABA_CONTROLES;
        foco = 0;
        opcoesRecemAbertas = 0;
    }

    if (cmd->voltar) {
        audioUIVoltar();
        return origemOpcoes;
    }

    nLinhas = aba == ABA_CONTROLES ? QTD_CONFIG : QTD_AUDIO;
    antesAba = aba;
    antesFoco = foco;

    if (foco == 0) {
        if (cmd->menuEsquerda)
            aba = (aba + QTD_ABAS - 1) % QTD_ABAS;
        else if (cmd->menuDireita)
            aba = (aba + 1) % QTD_ABAS;
        else if (cmd->menuBaixo)
            foco = 1;
        else if (cmd->menuCima)
            foco = nLinhas;
        else if (cmd->confirmar) {
            foco = 1;
            audioUISelecionar();
        }

        if (aba != antesAba) {
            foco = 0;
            audioUIMover();
        } else if (foco != antesFoco && foco != 0 && !cmd->confirmar)
            audioUIMover();
    } else {
        if (cmd->menuCima) {
            foco = foco == 1 ? 0 : foco - 1;
            audioUIMover();
        } else if (cmd->menuBaixo) {
            if (foco < nLinhas) {
                foco++;
                audioUIMover();
            }
        } else if (cmd->menuEsquerda || cmd->menuDireita) {
            int direcao = cmd->menuDireita ? 1 : -1;
            int mudou = aba == ABA_CONTROLES
                            ? ajustarConfig(cfg, foco - 1, direcao)
                            : ajustarVolume(acfg, foco - 1, direcao);

            if (mudou)
                audioUIMover();
        } else if (cmd->confirmar && aba == ABA_CONTROLES && foco - 1 >= 2) {
            if (ajustarConfig(cfg, foco - 1, 0))
                audioUISelecionar();
        }
    }

    textoNoMenu("Opções", centro, 16, 34, RAYWHITE);
    desenharAbas(aba, foco);

    if (aba == ABA_CONTROLES) {
        textoFonte("Ação", 120, 108, 18, GRAY);
        textoFonte("Teclado", 520, 108, 18, GRAY);
        textoFonte("Controle Xbox", 760, 108, 18, GRAY);
        DrawLine(110, 134, 1170, 134, DARKGRAY);

        for (i = 0; i < QTD_CONTROLES; i++) {
            int y = 142 + i * 24;

            textoFonte(CONTROLES[i].acao, 120, y, 17, LIGHTGRAY);
            textoFonte(CONTROLES[i].teclado, 520, y, 17, LIGHTGRAY);
            textoFonte(CONTROLES[i].controle, 760, y, 17, LIGHTGRAY);
        }

        DrawLine(110, 414, 1170, 414, DARKGRAY);
        textoNoMenu("Configurações", centro, 424, 22, RAYWHITE);

        for (i = 0; i < QTD_CONFIG; i++) {
            int y = 458 + i * 32;
            const char *valor;
            Color cor = foco == i + 1 ? YELLOW : LIGHTGRAY;

            switch (i) {
            case 0:
                valor = TextFormat("%.2f", cfg->sensibilidade);
                break;
            case 1:
                valor = TextFormat("%.2f", cfg->zonaMorta);
                break;
            case 2:
                valor = cfg->inverterArfagem ? "Sim" : "Não";
                break;
            default:
                valor = cfg->vibracao ? "Ligada" : "Desligada";
                break;
            }

            if (foco == i + 1)
                textoFonte(">", 140, y, 22, YELLOW);
            textoFonte(ROTULOS_CONFIG[i], 170, y, 22, cor);
            textoFonteDireita(valor, 650, y, 22, cor);
        }

        textoNoMenu(foco == 0
                        ? "←/→ troca aba   ↑/↓ ou Enter/A entra"
                        : "↑/↓ seleciona   ←/→ altera   Enter/A alterna   Esc/B volta",
                    centro, 640, 18, LIGHTGRAY);
    } else {
        for (i = 0; i < QTD_AUDIO; i++) {
            int y = 220 + i * 90;
            float valor = i == 0 ? acfg->geral : (i == 1 ? acfg->efeitos : acfg->musica);
            Color cor = foco == i + 1 ? YELLOW : LIGHTGRAY;

            if (foco == i + 1)
                textoFonte(">", 340, y, 26, YELLOW);
            textoFonte(ROTULOS_AUDIO[i], 380, y, 26, cor);
            desenharBarraVolume(700, y + 6, 260, 20, valor);
            textoFonteDireita(TextFormat("%d%%", (int)lroundf(valor * 100.0f)),
                              1040, y, 26, cor);
        }

        textoNoMenu("A música fica abaixo dos efeitos por padrão.",
                    centro, 540, 18, GRAY);
        textoNoMenu(foco == 0
                        ? "←/→ troca aba   ↑/↓ ou Enter/A entra"
                        : "↑/↓ seleciona   ←/→ ajusta 5%   Esc/B volta",
                    centro, 640, 18, LIGHTGRAY);
    }

    return EST_CONTROLES;
}

void telasDesenharPausa(int selecionada)
{
    static const char *ITENS[PAUSA_QTD] = {
        "Continuar", "Opções", "Voltar ao menu principal"
    };
    int centro = GetScreenWidth() / 2;
    int i;

    DrawRectangle(0, 0, GetScreenWidth(), GetScreenHeight(), Fade(BLACK, 0.65f));
    textoNoMenu("PAUSA", centro, 150, 48, RAYWHITE);

    for (i = 0; i < PAUSA_QTD; i++) {
        int y = 280 + i * 54;
        Color cor = i == selecionada ? YELLOW : LIGHTGRAY;

        if (i == selecionada) {
            Font fonte = fonteDoMenu();
            float largura = MeasureTextEx(fonte, ITENS[i], (float)TAM_MENU, ESPACO_FONTE).x;

            textoFonte(">", centro - (int)(largura / 2.0f) - 28, y, TAM_MENU, YELLOW);
        }
        textoNoMenu(ITENS[i], centro, y, TAM_MENU, cor);
    }

    textoNoMenu("Esc/Start: continuar   ↑/↓: navegar   Enter/A: selecionar",
                centro, 460, 18, LIGHTGRAY);
}

/* Libera fonte e textura antes de o contexto OpenGL ser destruído. */
void descarregarTelas(void)
{
    if (fundoMenu.id != 0)
        UnloadTexture(fundoMenu);
    if (fonteMenu.texture.id != 0)
        UnloadFont(fonteMenu);

    fundoMenu = (Texture2D){ 0 };
    fonteMenu = (Font){ 0 };
    fundoMenuCarregado = 0;
    fonteMenuCarregada = 0;
}
