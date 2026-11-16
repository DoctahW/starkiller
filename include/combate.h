#ifndef COMBATE_H
#define COMBATE_H

#include "tipos.h"

#define COOLDOWN_LASER   0.15f   /* s entre disparos do jogador */
#define VEL_LASER        600.0f  /* u/s */
#define VIDA_LASER       2.0f    /* s até o laser desativar */
#define DANO_LASER       10.0f
#define CANHAO_LATERAL   3.0f    /* distância de cada canhão ao eixo da nave */
#define CANHAO_FRENTE    5.0f    /* canhões à frente do centro da nave */

/*
 * Dispara os dois lasers do TIE (canhões esquerdo e direito) na direção da frente.
 * Respeita o cooldown de 0,15 s (jogador.cooldownLaser). Devolve 1 se disparou; 0 se
 * estava em cooldown ou o pool não tem 2 slots livres (nesse caso o cooldown não é gasto).
 */
int dispararLaser(Jogo *g);
/* Diminui o cooldown, move os lasers ativos e desativa os que esgotaram a vida. */
void atualizarLasers(Jogo *g, float dt);

#endif
