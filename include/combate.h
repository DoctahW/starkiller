#ifndef COMBATE_H
#define COMBATE_H

#include "tipos.h"

#define COOLDOWN_LASER   0.15f   /* s entre disparos do jogador */
#define VEL_LASER        600.0f  /* u/s */
#define VIDA_LASER       2.0f    /* s até o laser desativar */
#define DANO_LASER       10.0f
#define CANHAO_LATERAL   3.0f    /* distância de cada canhão ao eixo da nave */
#define CANHAO_FRENTE    5.0f    /* canhões à frente do centro da nave */

/* Raio da esfera de colisão de cada tipo de caça rebelde (chutes; ajustar com os modelos). */
#define RAIO_XWING       8.0f
#define RAIO_YWING       10.0f
#define RAIO_AWING       6.0f

/*
 * Dispara os dois lasers do TIE (canhões esquerdo e direito) na direção da frente.
 * Respeita o cooldown de 0,15 s (jogador.cooldownLaser). Devolve 1 se disparou; 0 se
 * estava em cooldown ou o pool não tem 2 slots livres (nesse caso o cooldown não é gasto).
 */
int dispararLaser(Jogo *g);
/* Diminui o cooldown, move os lasers ativos e desativa os que esgotaram a vida. */
void atualizarLasers(Jogo *g, float dt);

/* Raio da esfera de colisão do caça rebelde de tipo `t`. */
float raioInimigo(TipoNave t);
/*
 * Interseção do segmento a→b com a esfera (centro, raio). Devolve 1 se acerta e grava em
 * *t (se não for NULL) a fração [0, 1] do segmento onde ele entra na esfera (0 se já começa
 * dentro). Considera o caminho todo, então não deixa passar um laser rápido por uma esfera pequena.
 */
int segmentoAcertaEsfera(Vector3 a, Vector3 b, Vector3 centro, float raio, float *t);
/*
 * Colisão laser do jogador × inimigos. Deve rodar logo depois de atualizarLasers(g, dt): o segmento
 * de cada laser vai da posição anterior (pos - vel * dt) à atual. Cada laser acerta no máximo
 * um inimigo (o primeiro que o segmento atravessa) e é desativado. Devolve o número de acertos.
 */
int colidirLasers(Jogo *g, float dt);

#endif
