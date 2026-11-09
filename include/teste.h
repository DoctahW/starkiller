/*
 * Verificações para os testes de console (src/tests/test_*.c).
 * Uso:
 *     VERIFICA(lista.tamanho == 0);
 *     ...
 *     return RESULTADO_TESTES();
 * `make test` compila e roda cada src/tests/test_*.c ligado aos módulos de src/.
 */
#ifndef TESTE_H
#define TESTE_H

#include <stdio.h>

static int testes_falhas = 0;
static int testes_total = 0;

#define VERIFICA(cond)                                                    \
    do {                                                                  \
        testes_total++;                                                   \
        if (!(cond)) {                                                    \
            testes_falhas++;                                              \
            fprintf(stderr, "%s:%d: falhou: %s\n", __FILE__, __LINE__, #cond); \
        }                                                                 \
    } while (0)

#define RESULTADO_TESTES()                                                \
    (printf("%d/%d verificações passaram\n", testes_total - testes_falhas, testes_total), \
     testes_falhas != 0)

#endif
