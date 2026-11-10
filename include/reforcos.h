#ifndef REFORCOS_H
#define REFORCOS_H

#include "tipos.h"

void enfileirarReforco(FilaReforcos *f, float t, int x, int y, int a);
void gerarReforcos(FilaReforcos *f, float duracao);
void liberarFila(FilaReforcos *f);

#endif
