#ifndef NAVE_H
#define NAVE_H

#include "tipos.h"

#define FOV_JOGADOR 70.0f   /* graus */

/* Velocidades angulares máximas (rad/s), atingidas com o eixo em ±1. */
#define VEL_ARFAGEM 1.5f
#define VEL_GUINADA 1.0f
#define VEL_ROLAGEM 2.0f

/* Jogador perto do Destróier (na origem), olhando para fora da arena. */
void iniciarJogador(Jogador *j);
/* Limites de velocidade (u/s) e aceleração com o eixo em ±1 (u/s²). */
#define VEL_MIN     40.0f
#define VEL_MAX     120.0f
#define ACELERACAO  40.0f

/* Raio da arena, centrada na origem. */
#define RAIO_ARENA  2000.0f

/*
 * Gira a nave no espaço local conforme os eixos de cmd (arfagem > 0 = nariz para cima,
 * guinada > 0 = nariz para a direita, rolagem > 0 = asa direita para baixo),
 * renormaliza rot e recalcula frente/cima/direita. O eixo aceleracao acelera/freia
 * a velocidade, limitada a [VEL_MIN, VEL_MAX], e a posição avança frente * velocidade * dt.
 * Se a posição passar de RAIO_ARENA, a nave é devolvida à borda da esfera e a função
 * devolve 1 (aviso de limite da arena para o HUD); caso contrário, 0.
 */
int atualizarJogador(Jogador *j, const Comandos *cmd, float dt);
/* Câmera de cabine: position = pos, target = pos + frente, up = cima. */
Camera3D cameraDoJogador(const Jogador *j);

#endif
