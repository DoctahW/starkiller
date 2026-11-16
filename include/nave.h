#ifndef NAVE_H
#define NAVE_H

#include "tipos.h"

#define FOV_JOGADOR 70.0f   /* graus */

/* Jogador perto do Destróier (na origem), olhando para fora da arena. */
void iniciarJogador(Jogador *j);
/* Câmera de cabine: position = pos, target = pos + frente, up = cima. */
Camera3D cameraDoJogador(const Jogador *j);

#endif
