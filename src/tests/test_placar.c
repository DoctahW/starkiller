#include <string.h>

#include "placar.h"
#include "teste.h"

/* Preenche um vetor e verifica se ele está em ordem decrescente. */
static int estaDecrescente(const NoPlacar *v, int n)
{
    int i;

    for (i = 1; i < n; i++)
        if (v[i - 1].pontos < v[i].pontos)
            return 0;
    return 1;
}

/* Monta uma árvore inserindo as pontuações na ordem do vetor. */
static NoPlacar *construir(const int *pontos, int n)
{
    NoPlacar *raiz = NULL;
    char nome[16];
    int i;

    for (i = 0; i < n; i++) {
        snprintf(nome, sizeof nome, "P%d", i);
        raiz = inserirPontuacao(raiz, nome, pontos[i]);
    }
    return raiz;
}

int main(void)
{
    NoPlacar ranking[12];
    NoPlacar todos[25];
    NoPlacar *raiz;

    /* Árvore vazia */
    memset(ranking, 0x7f, sizeof ranking);
    VERIFICA(exibirRankingEmOrdem(NULL, ranking, 10) == 0);
    VERIFICA(ranking[0].pontos == 0x7f7f7f7f);
    liberarPlacar(NULL);

    /* Um nó */
    raiz = inserirPontuacao(NULL, "Ana", 1500);
    VERIFICA(raiz != NULL);
    VERIFICA(raiz->pontos == 1500);
    VERIFICA(strcmp(raiz->nome, "Ana") == 0);
    VERIFICA(raiz->esq == NULL && raiz->dir == NULL);
    VERIFICA(exibirRankingEmOrdem(raiz, ranking, 10) == 1);
    VERIFICA(ranking[0].pontos == 1500);
    VERIFICA(strcmp(ranking[0].nome, "Ana") == 0);
    VERIFICA(ranking[0].esq == NULL && ranking[0].dir == NULL);
    liberarPlacar(raiz);

    /* Empates: todos vão para a esquerda; o mais antigo fica à frente */
    raiz = inserirPontuacao(NULL, "Ariel", 100);
    raiz = inserirPontuacao(raiz, "Vinicius", 100);
    raiz = inserirPontuacao(raiz, "Joao", 100);
    VERIFICA(raiz->pontos == 100 && strcmp(raiz->nome, "Ariel") == 0);
    VERIFICA(raiz->dir == NULL);
    VERIFICA(raiz->esq != NULL && strcmp(raiz->esq->nome, "Vinicius") == 0);
    VERIFICA(raiz->esq->esq != NULL && strcmp(raiz->esq->esq->nome, "Joao") == 0);
    VERIFICA(exibirRankingEmOrdem(raiz, ranking, 3) == 3);
    VERIFICA(strcmp(ranking[0].nome, "Ariel") == 0);
    VERIFICA(strcmp(ranking[1].nome, "Vinicius") == 0);
    VERIFICA(strcmp(ranking[2].nome, "Joao") == 0);
    liberarPlacar(raiz);

    /* Inserção já ordenada (crescente): árvore vira cadeia à direita */
    {
        const int crescente[] = { 10, 20, 30, 40, 50, 60 };

        raiz = construir(crescente, 6);
        VERIFICA(raiz->pontos == 10);
        VERIFICA(raiz->esq == NULL);
        VERIFICA(raiz->dir != NULL && raiz->dir->pontos == 20);
        VERIFICA(exibirRankingEmOrdem(raiz, ranking, 6) == 6);
        VERIFICA(estaDecrescente(ranking, 6));
        VERIFICA(ranking[0].pontos == 60 && ranking[5].pontos == 10);
        liberarPlacar(raiz);
    }

    /* Inserção já ordenada (decrescente): árvore vira cadeia à esquerda */
    {
        const int decrescente[] = { 60, 50, 40, 30, 20, 10 };

        raiz = construir(decrescente, 6);
        VERIFICA(raiz->pontos == 60);
        VERIFICA(raiz->dir == NULL);
        VERIFICA(raiz->esq != NULL && raiz->esq->pontos == 50);
        VERIFICA(exibirRankingEmOrdem(raiz, ranking, 6) == 6);
        VERIFICA(estaDecrescente(ranking, 6));
        VERIFICA(ranking[0].pontos == 60 && ranking[5].pontos == 10);
        liberarPlacar(raiz);
    }

    /* Top 10 com mais de 10 nós (15 espalhados, com repetições de faixa) */
    {
        const int pontos[15] = { 70, 120, 40, 150, 90, 30, 110, 60,
                                 140, 20, 100, 50, 130, 80, 10 };

        raiz = construir(pontos, 15);
        memset(ranking, 0x7f, sizeof ranking);
        VERIFICA(exibirRankingEmOrdem(raiz, ranking, 10) == 10);
        VERIFICA(estaDecrescente(ranking, 10));
        VERIFICA(ranking[0].pontos == 150);
        VERIFICA(ranking[9].pontos == 60);
        VERIFICA(ranking[10].pontos == 0x7f7f7f7f);
        liberarPlacar(raiz);
    }

    /* Top 10 com 25 nós e limites de max */
    {
        int pontos[25];
        int i;

        for (i = 0; i < 25; i++)
            pontos[i] = (i + 1) * 10;

        raiz = construir(pontos, 25);
        memset(ranking, 0x7f, sizeof ranking);
        VERIFICA(exibirRankingEmOrdem(raiz, ranking, 10) == 10);
        VERIFICA(ranking[0].pontos == 250 && ranking[9].pontos == 160);
        VERIFICA(ranking[10].pontos == 0x7f7f7f7f);
        VERIFICA(exibirRankingEmOrdem(raiz, todos, 25) == 25);
        VERIFICA(exibirRankingEmOrdem(raiz, ranking, 0) == 0);
        VERIFICA(exibirRankingEmOrdem(raiz, ranking, -1) == 0);
        VERIFICA(exibirRankingEmOrdem(raiz, NULL, 10) == 0);
        liberarPlacar(raiz);
    }

    /* Nome: trunca em 15, neutraliza ';' e aceita NULL */
    raiz = inserirPontuacao(NULL, "ABCDEFGHIJKLMNOPQRST", 1);
    VERIFICA(strlen(raiz->nome) == PLACAR_MAX_NOME);
    VERIFICA(strcmp(raiz->nome, "ABCDEFGHIJKLMNO") == 0);
    liberarPlacar(raiz);

    raiz = inserirPontuacao(NULL, "COM;PONTO", 1);
    VERIFICA(strcmp(raiz->nome, "COM_PONTO") == 0);
    liberarPlacar(raiz);

    raiz = inserirPontuacao(NULL, NULL, 1);
    VERIFICA(raiz != NULL && raiz->nome[0] == '\0');
    liberarPlacar(raiz);

    return RESULTADO_TESTES();
}
