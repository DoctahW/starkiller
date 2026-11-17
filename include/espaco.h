#ifndef ESPACO_H
#define ESPACO_H

#include "tipos.h"

/* Estrelas de fundo, centradas na câmera: nunca se chega perto delas. */
#define NUM_ESTRELAS 2500
#define RAIO_CEU     1000.0f   /* < RL_CULL_DISTANCE_FAR (4000) */

/*
 * Estrela da Morte no céu (raio do modelo: 10 u). A 900 u com escala 18, ~230 px de diâmetro.
 * Maior que isso estica nas bordas da tela (FOV horizontal ~100°).
 */
#define ARQUIVO_ESTRELA_MORTE   "assets/models/death_star.glb"
#define DISTANCIA_ESTRELA_MORTE 900.0f
#define ESCALA_ESTRELA_MORTE    18.0f

/* Poeira próxima: caixa de LADO_POEIRA repetida em volta da câmera, dá profundidade e velocidade. */
#define NUM_POEIRA    300
#define LADO_POEIRA   200.0f
#define RASTRO_POEIRA 0.05f    /* s: comprimento do risco = velocidade * RASTRO_POEIRA */

/* Shaders em assets/shaders/<nome>.vs e .fs (GLSL 330). */
#define PASTA_SHADERS "assets/shaders/"

/* Sorteia estrelas e poeira (sempre iguais) e carrega modelo e shaders. Exige a janela aberta. */
void iniciarEspaco(void);
/* Libera modelos, texturas e shaders. Seguro sem iniciarEspaco. */
void encerrarEspaco(void);

/*
 * Céu (estrelas e Estrela da Morte) com a câmera na origem e a orientação do jogador:
 * nada nele se aproxima. Chamar fora de BeginMode3D, antes da cena; não grava profundidade.
 */
void desenharFundo(const Jogador *j);

/* Riscos de poeira na direção do movimento. Chamar dentro do BeginMode3D da cabine, depois do que é opaco. */
void desenharPoeira(const Jogador *j);

#endif
