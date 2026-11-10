#include <stdlib.h>

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

/* Pós-ordem: os filhos são liberados antes do pai. */
void liberarPlacar(NoPlacar *raiz)
{
    if (raiz == NULL)
        return;
    liberarPlacar(raiz->esq);
    liberarPlacar(raiz->dir);
    free(raiz);
}
