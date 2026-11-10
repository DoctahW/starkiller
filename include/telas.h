#ifndef TELAS_H
#define TELAS_H

#include "tipos.h"

/* Cada tela desenha um frame (entre BeginDrawing/EndDrawing do jogo) e devolve o próximo estado. */
EstadoJogo telaMenu(Jogo *g, const Comandos *cmd);
EstadoJogo telaFim(Jogo *g, const Comandos *cmd);
EstadoJogo telaPlacar(Jogo *g, const Comandos *cmd);
EstadoJogo telaControles(ConfigEntrada *cfg, const Comandos *cmd);

#endif
