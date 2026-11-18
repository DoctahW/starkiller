#ifndef TELAS_H
#define TELAS_H

#include "tipos.h"

/* Cada tela desenha um frame (entre BeginDrawing/EndDrawing do jogo) e devolve o próximo estado. */
EstadoJogo telaMenu(Jogo *g, const Comandos *cmd);
EstadoJogo telaFim(Jogo *g, const Comandos *cmd);
EstadoJogo telaPlacar(Jogo *g, const Comandos *cmd);
/* Tela de Opções (abas Controles e Áudio). A assinatura é a mesma da antiga tela de Controles. */
EstadoJogo telaControles(ConfigEntrada *cfg, const Comandos *cmd);

/* Origem da tela de Opções: EST_MENU ou EST_PAUSA (padrão EST_MENU). */
void telasOpcoesDefinirOrigem(EstadoJogo origem);

/* Itens do menu de pausa, na ordem exibida. */
enum {
    PAUSA_CONTINUAR = 0,
    PAUSA_OPCOES,
    PAUSA_VOLTAR_MENU,
    PAUSA_QTD
};

/* Desenha o menu de pausa sobre a cena congelada; a entrada fica em jogo.c. */
void telasDesenharPausa(int selecionada);

/* Libera a fonte e a textura da interface; chamar antes de CloseWindow. */
void descarregarTelas(void);

#endif
