#ifndef JOGO_H
#define JOGO_H

#include "tipos.h"

#define LARGURA_TELA 1280
#define ALTURA_TELA  720

#define DURACAO_PARTIDA  300.0f
#define PASSO_FIXO       (1.0f / 60.0f)
#define DT_MAXIMO        0.25f

/* Abre a janela, roda o jogo até o jogador sair e libera tudo. */
void jogo_executar(void);

/* Roda os passos fixos que cabem no acumulador; devolve EST_FIM com g->vitoria preenchido ao fim da partida. */
EstadoJogo simularCombate(Jogo *g, float *acumulador, float dtFrame);

#endif
