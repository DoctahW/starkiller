#include "destroier.h"

#include <math.h>

#include "raymath.h"

typedef struct {
    short primeira, quantidade;   /* colunas x da linha */
    int inicio;                   /* índice da primeira em COLUNAS_CASCO */
} LinhaCasco;

typedef struct {
    short baixo, cima;            /* y ocupado; baixo > cima é coluna vazia */
} ColunaCasco;

#include "casco_destroier.inc"

/* Passo da busca pela saída: menor que qualquer coluna inflada pelo raio da nave, nada é pulado. */
#define PASSO_SAIDA     (LADO_CELULA_CASCO * 0.5f)
#define REFINOS_SAIDA   16
#define BUSCA_MAXIMA    1000.0f   /* u; maior que a meia espessura do casco em qualquer sentido */

/* Intervalo da coluna (ix, iz); 0 se vazia ou fora da grade. */
static int coluna(int ix, int iz, float *baixo, float *cima)
{
    const LinhaCasco *l;
    const ColunaCasco *c;

    if (iz < 0 || iz >= NUM_LINHAS_CASCO)
        return 0;
    l = &LINHAS_CASCO[iz];
    if (ix < l->primeira || ix >= l->primeira + l->quantidade)
        return 0;
    c = &COLUNAS_CASCO[l->inicio + ix - l->primeira];
    if (c->baixo > c->cima)
        return 0;
    *baixo = c->baixo;
    *cima = c->cima;
    return 1;
}

int tocaDestroier(Vector3 centro, float raio)
{
    int ix0 = (int)floorf((centro.x - raio - X0_CASCO) / LADO_CELULA_CASCO);
    int ix1 = (int)floorf((centro.x + raio - X0_CASCO) / LADO_CELULA_CASCO);
    int iz0 = (int)floorf((centro.z - raio - Z0_CASCO) / LADO_CELULA_CASCO);
    int iz1 = (int)floorf((centro.z + raio - Z0_CASCO) / LADO_CELULA_CASCO);

    for (int iz = iz0; iz <= iz1; iz++)
        for (int ix = ix0; ix <= ix1; ix++) {
            float baixo, cima, x0, z0, dx, dz, folga;

            if (!coluna(ix, iz, &baixo, &cima))
                continue;
            /* Distância horizontal do centro ao retângulo da coluna; dentro dele, a esfera inteira conta. */
            x0 = X0_CASCO + ix * LADO_CELULA_CASCO;
            z0 = Z0_CASCO + iz * LADO_CELULA_CASCO;
            dx = fmaxf(fmaxf(x0 - centro.x, centro.x - (x0 + LADO_CELULA_CASCO)), 0.0f);
            dz = fmaxf(fmaxf(z0 - centro.z, centro.z - (z0 + LADO_CELULA_CASCO)), 0.0f);
            if (dx * dx + dz * dz > raio * raio)
                continue;
            folga = sqrtf(raio * raio - dx * dx - dz * dz);
            if (centro.y >= baixo - folga && centro.y <= cima + folga)
                return 1;
        }
    return 0;
}

/* Quanto andar em `sentido` até a esfera sair do casco, no máximo `limite`; devolve > limite se não sai antes. */
static float distanciaAteSair(Vector3 centro, float raio, Vector3 sentido, float limite)
{
    float dentro = 0.0f, fora;

    for (fora = PASSO_SAIDA;; fora += PASSO_SAIDA) {
        if (fora > limite)
            return limite + 1.0f;
        if (!tocaDestroier(Vector3Add(centro, Vector3Scale(sentido, fora)), raio))
            break;
        dentro = fora;
    }
    /* Bissecção entre o último ponto dentro e o primeiro fora; fica com o de fora. */
    for (int i = 0; i < REFINOS_SAIDA; i++) {
        float meio = 0.5f * (dentro + fora);

        if (tocaDestroier(Vector3Add(centro, Vector3Scale(sentido, meio)), raio))
            dentro = meio;
        else
            fora = meio;
    }
    return fora;
}

int afastarDoDestroier(Vector3 *centro, float raio)
{
    static const Vector3 SENTIDOS[6] = {
        { 0.0f, 1.0f, 0.0f }, { 0.0f, -1.0f, 0.0f }, { 1.0f, 0.0f, 0.0f },
        { -1.0f, 0.0f, 0.0f }, { 0.0f, 0.0f, 1.0f }, { 0.0f, 0.0f, -1.0f },
    };
    float melhor = BUSCA_MAXIMA;
    int escolhido = -1;

    if (!tocaDestroier(*centro, raio))
        return 0;
    for (int i = 0; i < 6; i++) {
        float d = distanciaAteSair(*centro, raio, SENTIDOS[i], melhor);

        if (d <= melhor) {
            melhor = d;
            escolhido = i;
        }
    }
    if (escolhido >= 0) {
        *centro = Vector3Add(*centro, Vector3Scale(SENTIDOS[escolhido], melhor));
        return 1;
    }

    /*
     * Nenhum eixo saiu dentro de BUSCA_MAXIMA, o que não deveria acontecer (o casco tem menos
     * de BUSCA_MAXIMA em qualquer sentido). Não conta como contato — evita dano a cada recarga
     * com a nave presa —, mas empurra para cima para não deixar a nave dentro do casco.
     */
    {
        float d = distanciaAteSair(*centro, raio, SENTIDOS[0], 2.0f * BUSCA_MAXIMA);

        if (d <= 2.0f * BUSCA_MAXIMA)
            *centro = Vector3Add(*centro, Vector3Scale(SENTIDOS[0], d));
    }
    return 0;
}
