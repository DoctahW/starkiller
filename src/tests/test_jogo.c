#include <math.h>
#include <string.h>

#include "jogo.h"
#include "teste.h"

#define QUASE(a, b) (fabsf((a) - (b)) < 1e-4f)

static Jogo jogo;

/* Partida em andamento com cascos cheios e `decorrido` segundos já jogados. */
static void novaPartida(float decorrido)
{
    memset(&jogo, 0, sizeof jogo);
    jogo.jogador.casco = 100.0f;
    jogo.cascoDestroier = 100.0f;
    jogo.tempoDecorrido = decorrido;
    jogo.tempoRestante = DURACAO_PARTIDA - decorrido;
    jogo.vitoria = -1;
}

int main(void)
{
    float acum;

    /* roda só os passos inteiros que cabem e guarda a sobra para o próximo frame */
    novaPartida(0.0f);
    acum = 0.0f;
    VERIFICA(simularCombate(&jogo, &acum, 3.5f * PASSO_FIXO) == EST_COMBATE);
    VERIFICA(QUASE(jogo.tempoDecorrido, 3.0f * PASSO_FIXO));
    VERIFICA(QUASE(jogo.tempoRestante, DURACAO_PARTIDA - 3.0f * PASSO_FIXO));
    VERIFICA(QUASE(acum, 0.5f * PASSO_FIXO));

    /* frame menor que um passo não simula; a sobra completa o passo no frame seguinte */
    novaPartida(0.0f);
    acum = 0.0f;
    simularCombate(&jogo, &acum, 0.6f * PASSO_FIXO);
    VERIFICA(jogo.tempoDecorrido == 0.0f);
    simularCombate(&jogo, &acum, 0.6f * PASSO_FIXO);
    VERIFICA(QUASE(jogo.tempoDecorrido, PASSO_FIXO));

    /* frame longo (janela arrastada, breakpoint) é limitado a DT_MAXIMO */
    novaPartida(0.0f);
    acum = 0.0f;
    simularCombate(&jogo, &acum, 10.0f);
    VERIFICA(jogo.tempoDecorrido <= DT_MAXIMO + 1e-4f);
    VERIFICA(jogo.tempoDecorrido >= DT_MAXIMO - PASSO_FIXO - 1e-4f);

    /* tempo esgotado é vitória, com o tempo restante cravado em 0 */
    novaPartida(DURACAO_PARTIDA - 0.5f * PASSO_FIXO);
    acum = 0.0f;
    VERIFICA(simularCombate(&jogo, &acum, PASSO_FIXO) == EST_FIM);
    VERIFICA(jogo.vitoria == 1);
    VERIFICA(jogo.tempoRestante == 0.0f);

    /* para no passo em que a partida acabou, sem consumir o resto do frame */
    novaPartida(DURACAO_PARTIDA - 0.5f * PASSO_FIXO);
    acum = 0.0f;
    VERIFICA(simularCombate(&jogo, &acum, DT_MAXIMO) == EST_FIM);
    VERIFICA(QUASE(jogo.tempoDecorrido, DURACAO_PARTIDA + 0.5f * PASSO_FIXO));
    VERIFICA(acum > DT_MAXIMO - 2.0f * PASSO_FIXO);

    /* casco do TIE em 0 é derrota */
    novaPartida(10.0f);
    acum = 0.0f;
    jogo.jogador.casco = 0.0f;
    VERIFICA(simularCombate(&jogo, &acum, PASSO_FIXO) == EST_FIM);
    VERIFICA(jogo.vitoria == 0);

    /* casco do Destróier abaixo de 0 é derrota */
    novaPartida(10.0f);
    acum = 0.0f;
    jogo.cascoDestroier = -5.0f;
    VERIFICA(simularCombate(&jogo, &acum, PASSO_FIXO) == EST_FIM);
    VERIFICA(jogo.vitoria == 0);

    /* casco zerado no mesmo passo em que o tempo acaba: derrota tem precedência */
    novaPartida(DURACAO_PARTIDA - 0.5f * PASSO_FIXO);
    acum = 0.0f;
    jogo.jogador.casco = 0.0f;
    VERIFICA(simularCombate(&jogo, &acum, PASSO_FIXO) == EST_FIM);
    VERIFICA(jogo.vitoria == 0);

    /* casco positivo e tempo sobrando: a partida continua */
    novaPartida(10.0f);
    acum = 0.0f;
    jogo.jogador.casco = 0.1f;
    VERIFICA(simularCombate(&jogo, &acum, PASSO_FIXO) == EST_COMBATE);
    VERIFICA(jogo.vitoria == -1);

    return RESULTADO_TESTES();
}
