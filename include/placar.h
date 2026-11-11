#ifndef PLACAR_H
#define PLACAR_H

#include "tipos.h"

#define PLACAR_MAX_NOME 15

NoPlacar *inserirPontuacao(NoPlacar *raiz, const char *nome, int pontos);

int exibirRankingEmOrdem(const NoPlacar *raiz, NoPlacar *saida, int max);

void liberarPlacar(NoPlacar *raiz);

#endif
