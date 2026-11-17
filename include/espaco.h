#ifndef ESPACO_H
#define ESPACO_H

#include "tipos.h"

/* Estrelas de fundo, centradas na câmera: nunca se chega perto delas. */
#define NUM_ESTRELAS 2500
#define RAIO_CEU     1000.0f   /* < RL_CULL_DISTANCE_FAR (4000) */

/* Poeira próxima: caixa de LADO_POEIRA repetida em volta da câmera, dá profundidade e velocidade. */
#define NUM_POEIRA    300
#define LADO_POEIRA   200.0f
#define RASTRO_POEIRA 0.05f    /* s: comprimento do risco = velocidade * RASTRO_POEIRA */

/* Sorteia estrelas e poeira (sempre iguais). */
void iniciarEspaco(void);
/* Libera o que iniciarEspaco carregou. Seguro sem iniciarEspaco. */
void encerrarEspaco(void);

/*
 * Céu com a câmera na origem e a orientação do jogador: nada nele se aproxima.
 * Chamar fora de BeginMode3D, antes da cena; não grava profundidade.
 */
void desenharFundo(const Jogador *j);

/* Riscos de poeira na direção do movimento. Chamar dentro do BeginMode3D da cabine, depois do que é opaco. */
void desenharPoeira(const Jogador *j);

#endif
