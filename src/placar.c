#include <stdlib.h>
#include <string.h>

#include "placar.h"

/* Copia o nome com limite de PLACAR_MAX_NOME caracteres e neutraliza os
 * caracteres que quebrariam o formato do arquivo. */
static void copiarNome(char destino[PLACAR_MAX_NOME + 1], const char *origem)
{
    int i = 0;

    if (origem != NULL) {
        while (origem[i] != '\0' && i < PLACAR_MAX_NOME) {
            char c = origem[i];
            destino[i] = (c == ';' || c == '\n' || c == '\r') ? '_' : c;
            i++;
        }
    }
    destino[i] = '\0';
}

NoPlacar *inserirPontuacao(NoPlacar *raiz, const char *nome, int pontos)
{
    NoPlacar **link = &raiz;
    NoPlacar *novo;

    /* Desce até o ponteiro que deve receber o novo nó. A inserção iterativa
     * mantém a árvore consistente mesmo se malloc falhar. */
    while (*link != NULL) {
        if (pontos <= (*link)->pontos)
            link = &(*link)->esq;
        else
            link = &(*link)->dir;
    }

    novo = malloc(sizeof *novo);
    if (novo == NULL)
        return raiz;

    copiarNome(novo->nome, nome);
    novo->pontos = pontos;
    novo->esq = NULL;
    novo->dir = NULL;
    *link = novo;

    return raiz;
}

/* Reverso do percurso em ordem: maior primeiro. Para assim que `qtd == max`. */
static void percorrerRanking(const NoPlacar *no, NoPlacar *saida, int max, int *qtd)
{
    if (no == NULL || *qtd >= max)
        return;

    percorrerRanking(no->dir, saida, max, qtd);

    if (*qtd < max) {
        memcpy(saida[*qtd].nome, no->nome, sizeof saida[*qtd].nome);
        saida[*qtd].pontos = no->pontos;
        saida[*qtd].esq = NULL;
        saida[*qtd].dir = NULL;
        (*qtd)++;
    }

    percorrerRanking(no->esq, saida, max, qtd);
}

int exibirRankingEmOrdem(const NoPlacar *raiz, NoPlacar *saida, int max)
{
    int qtd = 0;

    if (saida == NULL || max <= 0)
        return 0;

    percorrerRanking(raiz, saida, max, &qtd);
    return qtd;
}

/* Pós-ordem: os filhos são liberados antes do pai. */
void liberarPlacar(NoPlacar *raiz)
{
    if (raiz == NULL)
        return;
    liberarPlacar(raiz->esq);
    liberarPlacar(raiz->dir);
    free(raiz);
}
