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

static Texture2D fundoMenu;
static int fundoMenuCarregado;
static Font fonteMenu;
static int fonteMenuCarregada;

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

EstadoJogo telaFim(Jogo *g, const Comandos *cmd)
{
    textoCentralizado(g->vitoria ? "VITÓRIA" : "DERROTA", 200, TAM_TITULO, g->vitoria ? GREEN : RED);
    textoCentralizado(TextFormat("Abates: %d   Pontos: %d", g->abates, g->pontos), 300, TAM_TEXTO, RAYWHITE);
    textoCentralizado("Enter: continuar", 400, TAM_TEXTO, GRAY);
    return cmd->confirmar ? EST_PLACAR : EST_FIM;
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
