#include "reforcos.h"
#include "teste.h"

/* Fila vazia: desenfileirar devolve 0 e não toca em `saida`. */
static void testarVazia(void)
{
    FilaReforcos f = { 0 };
    NoReforco saida = { -1.0f, -1, -2, -3, NULL };

    VERIFICA(f.inicio == NULL && f.fim == NULL && f.tamanho == 0);
    VERIFICA(desenfileirarReforco(&f, &saida) == 0);
    VERIFICA(saida.tempoChegada == -1.0f && saida.qtdX == -1 && saida.qtdY == -2 && saida.qtdA == -3);
    VERIFICA(saida.prox == NULL);
    VERIFICA(f.inicio == NULL && f.fim == NULL && f.tamanho == 0);

    liberarFila(&f);
    VERIFICA(f.inicio == NULL && f.fim == NULL && f.tamanho == 0);
}

/* Um elemento: é início e fim ao mesmo tempo; sai com os dados copiados e o nó liberado. */
static void testarUmElemento(void)
{
    FilaReforcos f = { 0 };
    NoReforco saida;

    enfileirarReforco(&f, 3.0f, 2, 0, 0);
    VERIFICA(f.tamanho == 1);
    VERIFICA(f.inicio != NULL && f.inicio == f.fim);
    VERIFICA(f.inicio->tempoChegada == 3.0f && f.inicio->qtdX == 2);

    VERIFICA(desenfileirarReforco(&f, &saida) == 1);
    VERIFICA(saida.tempoChegada == 3.0f && saida.qtdX == 2 && saida.qtdY == 0 && saida.qtdA == 0);
    VERIFICA(saida.prox == NULL);
    VERIFICA(f.tamanho == 0 && f.inicio == NULL && f.fim == NULL);

    /* A fila ficou vazia de verdade: a segunda remoção não mexe em `saida`. */
    saida.qtdX = 99;
    VERIFICA(desenfileirarReforco(&f, &saida) == 0);
    VERIFICA(saida.qtdX == 99);

    liberarFila(&f);
}

/* Ordem FIFO preservada; `fim` acompanha o último inserido mesmo depois de esvaziar. */
static void testarOrdemFifo(void)
{
    const float tempos[3] = { 3.0f, 10.0f, 23.0f };
    const int qtdX[3] = { 2, 3, 4 };
    const int qtdY[3] = { 0, 1, 1 };
    const int qtdA[3] = { 0, 0, 1 };
    FilaReforcos f = { 0 };
    NoReforco saida;
    int i;

    for (i = 0; i < 3; i++)
        enfileirarReforco(&f, tempos[i], qtdX[i], qtdY[i], qtdA[i]);

    VERIFICA(f.tamanho == 3);
    VERIFICA(f.inicio->tempoChegada == 3.0f);
    VERIFICA(f.fim->tempoChegada == 23.0f && f.fim->qtdX == 4);

    for (i = 0; i < 3; i++) {
        VERIFICA(desenfileirarReforco(&f, &saida) == 1);
        VERIFICA(saida.tempoChegada == tempos[i]);
        VERIFICA(saida.qtdX == qtdX[i] && saida.qtdY == qtdY[i] && saida.qtdA == qtdA[i]);
        VERIFICA(saida.prox == NULL);
        VERIFICA(f.tamanho == 2 - i);
    }

    VERIFICA(f.inicio == NULL && f.fim == NULL && f.tamanho == 0);

    /* Enfileirar de novo numa fila esvaziada religa início e fim no mesmo nó. */
    enfileirarReforco(&f, 41.0f, 5, 2, 2);
    VERIFICA(f.tamanho == 1 && f.inicio == f.fim);
    VERIFICA(desenfileirarReforco(&f, &saida) == 1 && saida.tempoChegada == 41.0f);

    liberarFila(&f);
}

/* liberarFila libera o que sobrou (o valgrind confirma) e é seguro repetir. */
static void testarLiberar(void)
{
    FilaReforcos f = { 0 };
    NoReforco saida;

    enfileirarReforco(&f, 3.0f, 2, 0, 0);
    enfileirarReforco(&f, 10.0f, 3, 1, 0);
    enfileirarReforco(&f, 23.0f, 4, 1, 1);

    VERIFICA(desenfileirarReforco(&f, &saida) == 1);
    VERIFICA(f.tamanho == 2);

    liberarFila(&f);
    VERIFICA(f.inicio == NULL && f.fim == NULL && f.tamanho == 0);
    VERIFICA(desenfileirarReforco(&f, &saida) == 0);

    liberarFila(&f);
    VERIFICA(f.inicio == NULL && f.fim == NULL && f.tamanho == 0);
}

int main(void)
{
    testarVazia();
    testarUmElemento();
    testarOrdemFifo();
    testarLiberar();

    return RESULTADO_TESTES();
}
