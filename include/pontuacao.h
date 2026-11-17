#ifndef PONTUACAO_H
#define PONTUACAO_H

#include "tipos.h"

#define PONTOS_XWING 100
#define PONTOS_YWING 150
#define PONTOS_AWING 200

#define SEQUENCIA_X1_5 3 /* a partir de 3 abates sem dano: x1,5 */
#define SEQUENCIA_X2   6 /* a partir de 6 abates sem dano: x2   */

#define MULTIPLICADOR_X1   1.0f
#define MULTIPLICADOR_X1_5 1.5f
#define MULTIPLICADOR_X2   2.0f

#define BONUS_POR_PORCENTO 50
#define PORCENTAGEM_MIN    0.0f
#define PORCENTAGEM_MAX    100.0f


void registrarAbate(Jogo *g, TipoNave tipo);

/* Duração (s) do tremor de câmera e do flash vermelho depois de um dano ao jogador. */
#define DURACAO_FEEDBACK_DANO 0.2f

/* Zera a sequência sem dano e arma g->tempoDano (tremor e flash); pontos e abates são mantidos. */
void registrarDanoJogador(Jogo *g);

/* Multiplicador para a sequência informada: 0-2 -> x1; 3-5 -> x1,5; 6+ -> x2. */
float multiplicadorAtual(int sequenciaSemDano);

int bonusCasco(const Jogo *g);

/* Pontos finais: g->pontos + bonusCasco(g). Não altera o Jogo. */
int calcularPontuacao(const Jogo *g);

#endif
