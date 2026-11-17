#include <string.h>

#include "placar.h"
#include "pontuacao.h"
#include "telas.h"

#define TAM_TITULO 40
#define TAM_TEXTO  20
#define TAM_MENU   30
#define TAM_DICA   20

#define MENU_CENTRO_X 0.28f /* fração da largura; cai na área escura da arte */
#define MENU_TOPO_Y   342
#define MENU_ESPACO   42
#define MENU_SOMBRA   2

#define FONTE_MENU   "assets/fonts/ChakraPetch-Bold.ttf"
#define ESPACO_FONTE 1.0f

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

static void textoCentralizado(const char *texto, int y, int tamanho, Color cor)
{
    DrawText(texto, (GetScreenWidth() - MeasureText(texto, tamanho)) / 2, y, tamanho, cor);
}

/* Carrega a fonte uma única vez, com ASCII e Latin-1 para os acentos. */
static Font fonteDoMenu(void)
{
    if (!fonteMenuCarregada) {
        static int pontos[0x60 + 0x60]; /* 0x20-0x7E e 0xA0-0xFF */
        int quantidade = 0;
        int c;

        for (c = 0x20; c <= 0x7E; c++)
            pontos[quantidade++] = c;
        for (c = 0xA0; c <= 0xFF; c++) {
            if (c == 0xB5) /* micro sign não existe na Chakra Petch */
                continue;
            pontos[quantidade++] = c;
        }

        fonteMenu = LoadFontEx(FONTE_MENU, 32, pontos, quantidade);
        fonteMenuCarregada = 1;
    }

    return fonteMenu.texture.id != 0 ? fonteMenu : GetFontDefault();
}

static void textoNoMenu(const char *texto, int centroX, int y, int tamanho, Color cor)
{
    Font fonte = fonteDoMenu();
    float largura = MeasureTextEx(fonte, texto, (float)tamanho, ESPACO_FONTE).x;
    int x = centroX - (int)(largura / 2.0f);

    DrawTextEx(fonte, texto,
               (Vector2){ (float)(x + MENU_SOMBRA), (float)(y + MENU_SOMBRA) },
               (float)tamanho, ESPACO_FONTE, Fade(BLACK, 0.75f));
    DrawTextEx(fonte, texto, (Vector2){ (float)x, (float)y },
               (float)tamanho, ESPACO_FONTE, cor);
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
    static const char *opcoes[] = { "Iniciar Missão", "Placar de Pilotos", "Controles", "Sair" };
    static const EstadoJogo destinos[] = { EST_COMBATE, EST_PLACAR, EST_CONTROLES, EST_SAIR };
    enum { QTD_OPCOES = sizeof opcoes / sizeof opcoes[0] };
    static int selecionada = 0;
    int centroX = (int)(GetScreenWidth() * MENU_CENTRO_X);
    int i;
    (void)g;

    if (cmd->menuCima)
        selecionada = (selecionada + QTD_OPCOES - 1) % QTD_OPCOES;
    if (cmd->menuBaixo)
        selecionada = (selecionada + 1) % QTD_OPCOES;

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

    return cmd->confirmar ? destinos[selecionada] : EST_MENU;
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

static void atualizarNomeFim(const Comandos *cmd)
{
    int c;

    while ((c = GetCharPressed()) != 0) {
        if (c >= 32 && c <= 126)
            inserirLetraNome((char)c);
    }

    if (IsKeyPressed(KEY_BACKSPACE) || (cmd->usandoControle && cmd->voltar))
        apagarLetraNome();

    if (cmd->menuEsquerda && nomeCursor > 0)
        nomeCursor--;

    if (cmd->menuDireita) {
        if (cmd->usandoControle && nomeCursor == nomeTam)
            inserirLetraNome(letraAtual);
        else if (nomeCursor < nomeTam)
            nomeCursor++;
    }

    if (cmd->usandoControle) {
        if (cmd->menuCima)
            letraAtual = letraAnterior(letraAtual);
        else if (cmd->menuBaixo)
            letraAtual = proximaLetra(letraAtual);

        if ((cmd->menuCima || cmd->menuBaixo) && nomeCursor < nomeTam)
            nomeFim[nomeCursor] = letraAtual;
    }

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

static void desenharNomeFim(int usandoControle)
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

    if (usandoControle && nomeCursor == nomeTam && nomeTam < PLACAR_MAX_NOME) {
        char previa[2] = { letraAtual, '\0' };

        DrawTextEx(fonte, previa, (Vector2){ xCursor, (float)ty },
                   TAM_FIM_NOME, ESPACO_FONTE, YELLOW);
    }

    if (((int)(GetTime() * 2.0)) % 2 == 0)
        DrawRectangle((int)xCursor, ty, 2, TAM_FIM_NOME, YELLOW);
}

EstadoJogo telaFim(Jogo *g, const Comandos *cmd)
{
    int centro = GetScreenWidth() / 2;
    int bonus = bonusCasco(g);
    int total = calcularPontuacao(g);

    atualizarNomeFim(cmd);

    textoNoMenu(g->vitoria ? "VITÓRIA" : "DERROTA", centro, 70,
                TAM_FIM_TITULO, g->vitoria ? GREEN : RED);
    textoNoMenu(TextFormat("Abates: %d", g->abates), centro, 160, TAM_FIM_TEXTO, RAYWHITE);
    textoNoMenu(TextFormat("Pontos: %d", g->pontos), centro, 198, TAM_FIM_TEXTO, RAYWHITE);
    textoNoMenu(TextFormat("Bônus: %d", bonus), centro, 236, TAM_FIM_TEXTO, RAYWHITE);
    textoNoMenu(TextFormat("Total: %d", total), centro, 274, TAM_FIM_TEXTO, YELLOW);

    textoNoMenu("Digite seu nome:", centro, 336, 22, LIGHTGRAY);
    desenharNomeFim(cmd->usandoControle);

    if (cmd->usandoControle) {
        desenharSeletorLetras();
        textoNoMenu("↑/↓ letra   → adiciona   ← volta   B apaga   A confirma",
                    centro, Y_DICA_FIM, TAM_FIM_DICA, LIGHTGRAY);
    } else {
        textoNoMenu("Digite, Backspace apaga, Enter confirma",
                    centro, Y_NOME + ALTURA_NOME + 40, TAM_FIM_DICA, LIGHTGRAY);
    }

    if (cmd->confirmar) {
        const char *nomeFinal = nomeTam > 0 ? nomeFim : PLACAR_NOME_ANONIMO;
        size_t len = strlen(nomeFinal);

        g->placar = inserirPontuacao(g->placar, nomeFinal, total);
        salvarPlacar(g->placar, CAMINHO_PLACAR);

        if (len >= sizeof ultimoNome)
            len = sizeof ultimoNome - 1;
        memcpy(ultimoNome, nomeFinal, len);
        ultimoNome[len] = '\0';
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
    (void)g;
    textoCentralizado("Placar de Pilotos", 160, TAM_TITULO, RAYWHITE);
    textoCentralizado("Esc: voltar", 400, TAM_TEXTO, GRAY);
    return cmd->voltar || cmd->confirmar ? EST_MENU : EST_PLACAR;
}

EstadoJogo telaControles(ConfigEntrada *cfg, const Comandos *cmd)
{
    (void)cfg;
    textoCentralizado("Controles", 160, TAM_TITULO, RAYWHITE);
    textoCentralizado("Esc: voltar", 400, TAM_TEXTO, GRAY);
    return cmd->voltar ? EST_MENU : EST_CONTROLES;
}
