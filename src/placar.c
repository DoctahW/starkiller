#include <stdlib.h>

#include "placar.h"

/* Pós-ordem: os filhos são liberados antes do pai. */
void liberarPlacar(NoPlacar *raiz)
{
    if (raiz == NULL)
        return;
    liberarPlacar(raiz->esq);
    liberarPlacar(raiz->dir);
    free(raiz);
}
