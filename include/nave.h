#ifndef NAVE_H
#define NAVE_H

#include "tipos.h"

#define FOV_JOGADOR 70.0f   /* graus */

/* Velocidades angulares máximas (rad/s), atingidas com o eixo em ±1. */
#define VEL_ARFAGEM 1.5f
#define VEL_GUINADA 1.0f
#define VEL_ROLAGEM 2.0f

/* Jogador perto do Destróier (na origem), olhando para fora da arena. Zera a velocidade angular. */
void iniciarJogador(Jogador *j);
/* Constante de tempo (s) da suavização da rotação: ~63% da velocidade angular alvo em TAU_GIRO. */
#define TAU_GIRO    0.1f

/* Limites de velocidade (u/s) e aceleração com o eixo em ±1 (u/s²). */
#define VEL_MIN     40.0f
#define VEL_MAX     120.0f
#define ACELERACAO  40.0f

/* Raio da arena, centrada na origem. */
#define RAIO_ARENA  2000.0f

/*
 * Gira a nave no espaço local conforme os eixos de cmd, com a velocidade angular suavizada
 * (aproxima-se do alvo com constante de tempo TAU_GIRO, sem giro instantâneo) (arfagem > 0 = nariz para cima,
 * guinada > 0 = nariz para a direita, rolagem > 0 = asa direita para baixo),
 * renormaliza rot e recalcula frente/cima/direita. O eixo aceleracao acelera/freia
 * a velocidade, limitada a [VEL_MIN, VEL_MAX], e a posição avança frente * velocidade * dt.
 * Se a posição passar de RAIO_ARENA, a nave é devolvida à borda da esfera e a função
 * devolve 1 (aviso de limite da arena para o HUD); caso contrário, 0.
 */
int atualizarJogador(Jogador *j, const Comandos *cmd, float dt);

/* Esfera de colisão do TIE (chute; ajustar com o modelo) e dano de um contato com o casco do Destróier. */
#define RAIO_JOGADOR            6.0f
#define DANO_COLISAO_DESTROIER  10.0f
/* Depois de um dano, os contatos seguintes só empurram a nave durante este tempo (s), sem novo dano. */
#define RECARGA_COLISAO         0.5f

/*
 * Colisão do TIE com o Destróier (esferas de destroier.h). Encostar no casco empurra a nave para
 * fora, ao longo da normal esfera → nave, até a superfície, e tira DANO_COLISAO_DESTROIER do casco
 * do jogador chamando registrarDanoJogador (zera a sequência sem dano). Durante RECARGA_COLISAO
 * depois de um dano só empurra, para a nave encostada não perder casco a cada passo.
 * Roda depois de atualizarJogador. Devolve 1 se houve contato neste passo; 0 caso contrário.
 */
int colidirDestroier(Jogo *g, float dt);
/* Câmera de cabine: position = pos, target = pos + frente, up = cima. */
Camera3D cameraDoJogador(const Jogador *j);

#endif
