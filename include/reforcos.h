#ifndef REFORCOS_H
#define REFORCOS_H

#include "tipos.h"

void enfileirarReforco(FilaReforcos *f, float t, int x, int y, int a);
int desenfileirarReforco(FilaReforcos *f, NoReforco *saida);
void gerarReforcos(FilaReforcos *f, float duracao);
void liberarFila(FilaReforcos *f);

#endif
