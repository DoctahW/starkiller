#ifndef PLACAR_H
#define PLACAR_H

#include "tipos.h"

/* Derivado de NoPlacar.nome: acompanha qualquer mudança em tipos.h. */
#define PLACAR_MAX_NOME ((int)(sizeof(((NoPlacar *)0)->nome) - 1))
#define CAMINHO_PLACAR "dados/placar.dat" /* a pasta dados/ é criada no primeiro salvamento */

NoPlacar *inserirPontuacao(NoPlacar *raiz, const char *nome, int pontos);

int exibirRankingEmOrdem(const NoPlacar *raiz, NoPlacar *saida, int max);

void liberarPlacar(NoPlacar *raiz);

int salvarPlacar(const NoPlacar *raiz, const char *caminho);

NoPlacar *carregarPlacar(const char *caminho);

#endif
