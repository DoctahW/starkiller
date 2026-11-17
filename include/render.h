#ifndef RENDER_H
#define RENDER_H

#include "tipos.h"

void iniciarRender(void);
void encerrarRender(void);

/*
 * Desenha a cena 3D vista da cabine (entre BeginDrawing/EndDrawing, antes do HUD):
 * primeiro o fundo do espaço, sem gravar profundidade, depois a arena.
 */
void desenharCena(const Jogo *g);

#endif
