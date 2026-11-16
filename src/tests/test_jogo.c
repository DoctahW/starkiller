#include <math.h>
#include <string.h>

#include "combate.h"
#include "inimigos.h"
#include "jogo.h"
#include "nave.h"
#include "teste.h"

#define QUASE(a, b) (fabsf((a) - (b)) < 1e-4f)

static Jogo jogo;
static RelogioCombate relogio;
static const Comandos parado = { 0 };

/* Partida em andamento com cascos cheios e `passos` passos já jogados. */
static void novaPartida(int passos)
{
    memset(&jogo, 0, sizeof jogo);
    iniciarJogador(&jogo.jogador);
    jogo.cascoDestroier = CASCO_MAX_DESTROIER;
    jogo.tempoDecorrido = passos * PASSO_FIXO;
    jogo.tempoRestante = (PASSOS_PARTIDA - passos) * PASSO_FIXO;
    jogo.vitoria = -1;
    relogio = (RelogioCombate){ .acumulador = 0.0f, .passos = passos };
}

static EstadoJogo simular(float dtFrame)
{
    return simularCombate(&jogo, &relogio, &parado, dtFrame);
}

int main(void)
{
    /* roda só os passos inteiros que cabem e guarda a sobra para o próximo frame */
    novaPartida(0);
    VERIFICA(simular(3.5f * PASSO_FIXO) == EST_COMBATE);
    VERIFICA(relogio.passos == 3);
    VERIFICA(QUASE(jogo.tempoDecorrido, 3.0f * PASSO_FIXO));
    VERIFICA(QUASE(jogo.tempoRestante, DURACAO_PARTIDA - 3.0f * PASSO_FIXO));
    VERIFICA(QUASE(relogio.acumulador, 0.5f * PASSO_FIXO));

    /* frame menor que um passo não simula; a sobra completa o passo no frame seguinte */
    novaPartida(0);
    simular(0.6f * PASSO_FIXO);
    VERIFICA(relogio.passos == 0);
    simular(0.6f * PASSO_FIXO);
    VERIFICA(relogio.passos == 1);

    /* frame longo (janela arrastada, breakpoint) é limitado a DT_MAXIMO */
    novaPartida(0);
    simular(10.0f);
    VERIFICA(relogio.passos == (int)(DT_MAXIMO * PASSOS_POR_SEGUNDO)
             || relogio.passos == (int)(DT_MAXIMO * PASSOS_POR_SEGUNDO) - 1);

    /* a partida inteira dura exatamente PASSOS_PARTIDA passos, sem deriva do float */
    novaPartida(0);
    EstadoJogo estado = EST_COMBATE;
    for (int frame = 0; frame < 2 * PASSOS_PARTIDA && estado == EST_COMBATE; frame++)
        estado = simular(PASSO_FIXO);
    VERIFICA(estado == EST_FIM);
    VERIFICA(relogio.passos == PASSOS_PARTIDA);
    VERIFICA(QUASE(jogo.tempoDecorrido, DURACAO_PARTIDA));

    /* tempo esgotado é vitória, com o tempo restante exatamente 0 */
    novaPartida(PASSOS_PARTIDA - 1);
    VERIFICA(simular(PASSO_FIXO) == EST_FIM);
    VERIFICA(jogo.vitoria == 1);
    VERIFICA(jogo.tempoRestante == 0.0f);

    /* para no passo em que a partida acabou, sem consumir o resto do frame */
    novaPartida(PASSOS_PARTIDA - 1);
    VERIFICA(simular(DT_MAXIMO) == EST_FIM);
    VERIFICA(relogio.passos == PASSOS_PARTIDA);
    VERIFICA(relogio.acumulador > DT_MAXIMO - 2.0f * PASSO_FIXO);

    /* casco do TIE em 0 é derrota */
    novaPartida(600);
    jogo.jogador.casco = 0.0f;
    VERIFICA(simular(PASSO_FIXO) == EST_FIM);
    VERIFICA(jogo.vitoria == 0);

    /* casco do Destróier abaixo de 0 é derrota */
    novaPartida(600);
    jogo.cascoDestroier = -5.0f;
    VERIFICA(simular(PASSO_FIXO) == EST_FIM);
    VERIFICA(jogo.vitoria == 0);

    /* casco zerado no mesmo passo em que o tempo acaba: derrota tem precedência */
    novaPartida(PASSOS_PARTIDA - 1);
    jogo.jogador.casco = 0.0f;
    VERIFICA(simular(PASSO_FIXO) == EST_FIM);
    VERIFICA(jogo.vitoria == 0);

    /* casco positivo e tempo sobrando: a partida continua */
    novaPartida(600);
    jogo.jogador.casco = 0.1f;
    VERIFICA(simular(PASSO_FIXO) == EST_COMBATE);
    VERIFICA(jogo.vitoria == -1);

    /* sem disparar, nenhum laser nasce */
    novaPartida(0);
    simular(5.0f * PASSO_FIXO);
    {
        int ativos = 0;

        for (int i = 0; i < MAX_LASERS; i++)
            ativos += jogo.lasers[i].ativo;
        VERIFICA(ativos == 0);
    }

    /* segurando o disparo, o passo fixo atira, acerta, abate o X-Wing à frente e pontua */
    {
        Comandos atirando = { .disparar = 1 };
        Vector3 alvo;

        novaPartida(0);
        alvo = (Vector3){ jogo.jogador.pos.x + jogo.jogador.frente.x * 150.0f,
                          jogo.jogador.pos.y + jogo.jogador.frente.y * 150.0f,
                          jogo.jogador.pos.z + jogo.jogador.frente.z * 150.0f };
        VERIFICA(inserirInimigo(&jogo.inimigos, criarInimigo(XWING, alvo, jogo.proximoId++)) == 1);
        for (int i = 0; i < 200 && jogo.inimigos.tamanho > 0; i++)
            simularCombate(&jogo, &relogio, &atirando, PASSO_FIXO);
        VERIFICA(jogo.inimigos.tamanho == 0);
        VERIFICA(jogo.inimigos.alvo == NULL);
        VERIFICA(jogo.pontos > 0);
        liberarListaInimigos(&jogo.inimigos);
    }

    return RESULTADO_TESTES();
}
