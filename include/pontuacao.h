#ifndef PONTUACAO_H
#define PONTUACAO_H

#include "tipos.h"

#define PONTOS_XWING 100
#define PONTOS_YWING 150
#define PONTOS_AWING 200

#define SEQUENCIA_X1_5 3 /* a partir de 3 abates sem dano: x1,5 */
#define SEQUENCIA_X2   6 /* a partir de 6 abates sem dano: x2   */

#define CASCO_MAX_DESTROIER 2000.0f
#define BONUS_POR_PORCENTO  50

void registrarAbate(Jogo *g, TipoNave tipo);

void registrarDanoJogador(Jogo *g);

int calcularPontuacao(const Jogo *g);

#endif
