#ifndef REFORCOS_H
#define REFORCOS_H

#include "tipos.h"

void enfileirarReforco(FilaReforcos *f, float t, int x, int y, int a);
int desenfileirarReforco(FilaReforcos *f, NoReforco *saida);
void gerarReforcos(FilaReforcos *f, float duracao);
/*
 * Seção 3.4, passo 3: desenfileira todos os grupos cujo tempoChegada já passou
 * (tempoChegada <= g->tempoDecorrido) e insere os caças de cada grupo na lista de
 * inimigos, juntos, num ponto aleatório da borda da arena e em formação.
 */
void processarReforcos(Jogo *g);
void liberarFila(FilaReforcos *f);

#endif
