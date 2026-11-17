#include <stdlib.h>

#include "reforcos.h"

#define PRIMEIRA_ONDA    3.0f
#define INTERVALO_ONDAS  20.0f

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

/*
 * Seção 2.2: primeira onda em 3 s, depois uma a cada 20 s, com tamanho 2 + t/40.
 * A proporção de A-Wings cresce com o tempo e há pelo menos 1 Y-Wing da 2ª onda em diante.
 * Os clamps garantem pelo menos 1 X-Wing por onda e que X + Y + A seja sempre o tamanho da onda.
 */
void gerarReforcos(FilaReforcos *f, float duracao)
{
    int onda;
    float t;

    for (onda = 0, t = PRIMEIRA_ONDA; t < duracao; onda++, t += INTERVALO_ONDAS) {
        int total = 2 + (int)(t / 40.0f);
        int qtdA = (int)(total * (t / duracao));
        int qtdY = onda == 0 ? 0 : 1 + onda / 6;

        if (qtdY > total - qtdA)
            qtdY = total - qtdA;
        if (qtdA > total - 1 - qtdY)
            qtdA = total - 1 - qtdY;
        enfileirarReforco(f, t, total - qtdY - qtdA, qtdY, qtdA);
    }
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
