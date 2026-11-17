#include <stdlib.h>

#include "reforcos.h"

#define PRIMEIRA_ONDA    10.0f
#define INTERVALO_ONDAS  30.0f

void enfileirarReforco(FilaReforcos *f, float t, int x, int y, int a)
{
    NoReforco *no = malloc(sizeof *no);
    if (no == NULL)
        return;
    no->tempoChegada = t;
    no->qtdX = x;
    no->qtdY = y;
    no->qtdA = a;
    no->prox = NULL;
    if (f->fim == NULL)
        f->inicio = no;
    else
        f->fim->prox = no;
    f->fim = no;
    f->tamanho++;
}

/*
 * Remove o grupo do início (O(1)) e copia tempoChegada e quantidades para `saida`.
 * Devolve 1 se desenfileirou e 0 se a fila estava vazia; em fila vazia `saida` não é tocada.
 * O `prox` de `saida` é sempre NULL: os nós pertencem só à fila.
 */
int desenfileirarReforco(FilaReforcos *f, NoReforco *saida)
{
    NoReforco *no = f->inicio;

    if (no == NULL)
        return 0;

    *saida = *no;
    saida->prox = NULL;
    f->inicio = no->prox;
    if (f->inicio == NULL)
        f->fim = NULL;
    f->tamanho--;
    free(no);
    return 1;
}

/* Ondas crescentes em ordem de chegada; composição final da seção 2.2 em REF-02. */
void gerarReforcos(FilaReforcos *f, float duracao)
{
    int onda = 0;
    for (float t = PRIMEIRA_ONDA; t < duracao; t += INTERVALO_ONDAS, onda++)
        enfileirarReforco(f, t, 2 + onda / 2, onda >= 1, onda / 3);
}

void liberarFila(FilaReforcos *f)
{
    NoReforco *no = f->inicio;
    while (no != NULL) {
        NoReforco *prox = no->prox;
        free(no);
        no = prox;
    }
    f->inicio = f->fim = NULL;
    f->tamanho = 0;
}
