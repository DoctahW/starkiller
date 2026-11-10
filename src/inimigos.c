#include <stdlib.h>

#include "inimigos.h"

void liberarListaInimigos(ListaInimigos *l)
{
    NoInimigo *no = l->inicio;
    while (no != NULL) {
        NoInimigo *prox = no->prox;
        free(no);
        no = prox;
    }
    l->inicio = l->fim = l->alvo = NULL;
    l->tamanho = 0;
}
