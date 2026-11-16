#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "placar.h"

#define TAM_CAMINHO  512
#define TAM_LINHA    128
#define ARQUIVO_TMP  "placar.tmp"
#define TAM_TMP      (TAM_CAMINHO + sizeof ARQUIVO_TMP)
#define NOME_ANONIMO "anonimo"

/* Copia o nome para o nó, remove caracteres que prejudicam o formato nome;pontos
 * e usa "anonimo" caso o nome seja NULL ou vazio. */
static void copiarNome(NoPlacar *destino, const char *origem)
{
    const int limite = (int)sizeof destino->nome - 1;
    int i = 0;

    if (origem != NULL) {
        while (origem[i] != '\0' && i < limite) {
            char c = origem[i];
            destino->nome[i] = (c == ';' || c == '\n' || c == '\r') ? '_' : c;
            i++;
        }
    }

    if (i == 0)
        memcpy(destino->nome, NOME_ANONIMO, sizeof NOME_ANONIMO);
    else
        destino->nome[i] = '\0';
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

    copiarNome(novo, nome);
    novo->pontos = pontos;
    novo->esq = NULL;
    novo->dir = NULL;
    *link = novo;

    return raiz;
}

/* Percorre a árvore do maior para o menor até atingir a quantidade máxima.
 * Em árvores degeneradas, a recursão pode chegar a O(n), mas isso é aceitável devido ao pequeno tamanho do placar.*/
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

/* GetDirectoryPath devolve buffer estático; copia antes de outras chamadas. */
static void copiarPasta(const char *caminho, char *destino, size_t tam)
{
    snprintf(destino, tam, "%s", GetDirectoryPath(caminho));
}

static int garantirPasta(const char *caminho)
{
    char pasta[TAM_CAMINHO];

    copiarPasta(caminho, pasta, sizeof pasta);
    if (pasta[0] == '\0' || strcmp(pasta, ".") == 0)
        return 1;
    if (DirectoryExists(pasta))
        return 1;
    return MakeDirectory(pasta) == 0;
}

static void caminhoTemporario(const char *caminho, char destino[TAM_TMP])
{
    char pasta[TAM_CAMINHO];

    copiarPasta(caminho, pasta, sizeof pasta);
    if (pasta[0] == '\0' || strcmp(pasta, ".") == 0)
        snprintf(destino, TAM_TMP, "%s", ARQUIVO_TMP);
    else
        snprintf(destino, TAM_TMP, "%s/%s", pasta, ARQUIVO_TMP);
}

/* Pré-ordem: o nó antes das subárvores. Reinserir nessa ordem recria a árvore. */
static void gravarNo(FILE *arquivo, const NoPlacar *no)
{
    if (no == NULL)
        return;
    fprintf(arquivo, "%s;%d\n", no->nome, no->pontos);
    gravarNo(arquivo, no->esq);
    gravarNo(arquivo, no->dir);
}

int salvarPlacar(const NoPlacar *raiz, const char *caminho)
{
    char tmp[TAM_TMP];
    FILE *arquivo;

    if (caminho == NULL)
        caminho = CAMINHO_PLACAR;
    if (!garantirPasta(caminho))
        return 0;

    caminhoTemporario(caminho, tmp);
    arquivo = fopen(tmp, "w");
    if (arquivo == NULL)
        return 0;

    gravarNo(arquivo, raiz);
    if (ferror(arquivo) || fflush(arquivo) != 0) {
        fclose(arquivo);
        remove(tmp);
        return 0;
    }
    if (fclose(arquivo) != 0) {
        remove(tmp);
        return 0;
    }

    /* rename() não sobrescreve no Windows; remove o destino e tenta de novo. */
    if (rename(tmp, caminho) != 0) {
        remove(caminho);
        if (rename(tmp, caminho) != 0) {
            remove(tmp);
            return 0;
        }
    }
    return 1;
}

NoPlacar *carregarPlacar(const char *caminho)
{
    NoPlacar *raiz = NULL;
    char linha[TAM_LINHA];
    FILE *arquivo;

    if (caminho == NULL)
        caminho = CAMINHO_PLACAR;

    arquivo = fopen(caminho, "r");
    if (arquivo == NULL)
        return NULL;

    while (fgets(linha, sizeof linha, arquivo) != NULL) {
        char *separador = strrchr(linha, ';');
        char *fim;
        long valor;

        if (separador == NULL || separador == linha)
            continue;
        *separador = '\0';

        valor = strtol(separador + 1, &fim, 10);
        if (fim == separador + 1)
            continue;
        while (*fim == ' ' || *fim == '\t' || *fim == '\r' || *fim == '\n')
            fim++;
        if (*fim != '\0' || valor < INT_MIN || valor > INT_MAX)
            continue;

        raiz = inserirPontuacao(raiz, linha, (int)valor);
    }

    fclose(arquivo);
    return raiz;
}
