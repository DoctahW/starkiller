#ifndef PLACAR_H
#define PLACAR_H

#include "tipos.h"

#define PLACAR_MAX_NOME 15 /* NoPlacar.nome é char[16]: 15 caracteres + '\0' */

/*
 * Insere uma partida na BST e devolve a raiz da árvore.
 *
 * - chave = pontos; empate vai para a esquerda, então o mais antigo aparece
 *   primeiro no ranking;
 * - o nome é truncado em PLACAR_MAX_NOME caracteres;
 * - ';', '\n' e '\r' são trocados por '_' (o ';' separa nome e pontos no
 *   arquivo do placar);
 * - se faltar memória, a árvore original é devolvida intacta.
 */
NoPlacar *inserirPontuacao(NoPlacar *raiz, const char *nome, int pontos);

void liberarPlacar(NoPlacar *raiz);

#endif
