#ifndef RENDER_H
#define RENDER_H

#include "tipos.h"

void carregarAssets(void);
void descarregarAssets(void);

/*
 * Desenha a cena 3D vista da cabine (entre BeginDrawing/EndDrawing, antes do HUD):
 * primeiro o fundo do espaço, sem gravar profundidade, depois a arena.
 * `cam` é a câmera da cabine (cameraDoJogador); o fundo ignora a posição dela
 * e usa só a orientação do jogador.
 */
void render_cena(const Jogo *g, Camera3D cam);

#endif
