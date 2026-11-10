#include "telas.h"

#define TAM_TITULO 40
#define TAM_TEXTO  20

static void textoCentralizado(const char *texto, int y, int tamanho, Color cor)
{
    DrawText(texto, (GetScreenWidth() - MeasureText(texto, tamanho)) / 2, y, tamanho, cor);
}

EstadoJogo telaMenu(Jogo *g, const Comandos *cmd)
{
    static const char *opcoes[] = { "Iniciar Missão", "Placar de Pilotos", "Controles", "Sair" };
    static const EstadoJogo destinos[] = { EST_COMBATE, EST_PLACAR, EST_CONTROLES, EST_SAIR };
    enum { QTD_OPCOES = sizeof opcoes / sizeof opcoes[0] };
    static int selecionada = 0;
    (void)g;

    if (cmd->menuCima)
        selecionada = (selecionada + QTD_OPCOES - 1) % QTD_OPCOES;
    if (cmd->menuBaixo)
        selecionada = (selecionada + 1) % QTD_OPCOES;

    textoCentralizado("Starkiller", 160, TAM_TITULO, RAYWHITE);
    for (int i = 0; i < QTD_OPCOES; i++)
        textoCentralizado(opcoes[i], 300 + i * 40, TAM_TEXTO, i == selecionada ? YELLOW : GRAY);

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
