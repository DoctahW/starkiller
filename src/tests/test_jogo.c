#include <math.h>
#include <string.h>

#include "combate.h"
#include "inimigos.h"
#include "jogo.h"
#include "nave.h"
#include "raymath.h"
#include "teste.h"

#define QUASE(a, b) (fabsf((a) - (b)) < 1e-4f)

static Jogo jogo;
static RelogioCombate relogio;
static const Comandos parado = { 0 };

/* Partida em andamento com cascos cheios e `passos` passos já jogados. */
static void novaPartida(int passos)
{
    liberarListaInimigos(&jogo.inimigos);
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

static EstadoJogo simularCom(const Comandos *cmd, float dtFrame)
{
    return simularCombate(&jogo, &relogio, cmd, dtFrame);
}

static void adicionarInimigo(Vector3 pos, float distancia)
{
    Inimigo in = criarInimigo(XWING, pos, jogo.proximoId++);
    in.distancia = distancia;
    VERIFICA(inserirInimigo(&jogo.inimigos, in) == 1);
}

static int lasersAtivos(void)
{
    int n = 0;
    for (int i = 0; i < MAX_LASERS; i++)
        n += jogo.lasers[i].ativo;
    return n;
}

/* Seção 3.4, passo 1: mira uma vez por frame; disparo e colisão dentro dos passos. */
static void testarComandosDeCombate(void)
{
    Comandos cmd = { 0 };
    Vector3 longe = { 0.0f, 1000.0f, 0.0f };

    /* trocar de alvo vale uma vez por frame, com vários passos ou nenhum */
    novaPartida(0);
    adicionarInimigo(longe, 30.0f);
    adicionarInimigo(longe, 10.0f);
    adicionarInimigo(longe, 20.0f);
    NoInimigo *primeiro = jogo.inimigos.inicio;
    cmd.proximoAlvo = 1;
    simularCom(&cmd, 3.0f * PASSO_FIXO);
    VERIFICA(jogo.inimigos.alvo == primeiro->prox);
    simularCom(&cmd, 0.0f);
    VERIFICA(jogo.inimigos.alvo == primeiro->prox->prox);
    cmd.proximoAlvo = 0;
    cmd.alvoAnterior = 1;
    simularCom(&cmd, 3.0f * PASSO_FIXO);
    VERIFICA(jogo.inimigos.alvo == primeiro->prox);

    /* alvo mais próximo: ordena a lista e mira o início */
    cmd.alvoAnterior = 0;
    cmd.alvoMaisProximo = 1;
    simularCom(&cmd, 0.0f);
    VERIFICA(jogo.inimigos.alvo == jogo.inimigos.inicio);
    VERIFICA(jogo.inimigos.alvo->dado.distancia == 10.0f);
    VERIFICA(jogo.inimigos.fim->dado.distancia == 30.0f);

    /* disparar segurado: dispara só nos passos, respeitando o cooldown de 9 passos */
    novaPartida(0);
    cmd = (Comandos){ .disparar = 1 };
    simularCom(&cmd, 0.0f);
    VERIFICA(lasersAtivos() == 0);
    simularCom(&cmd, PASSO_FIXO);
    VERIFICA(lasersAtivos() == 2);
    simularCom(&cmd, 9.5f * PASSO_FIXO);   /* passos 2 a 10; o 10º já sai do cooldown */
    VERIFICA(lasersAtivos() == 4);

    /* lasers que acertam um inimigo à frente são desativados pela colisão do passo */
    novaPartida(0);
    adicionarInimigo(Vector3Add(jogo.jogador.pos, Vector3Scale(jogo.jogador.frente, 150.0f)), 150.0f);
    simularCom(&cmd, PASSO_FIXO);
    VERIFICA(lasersAtivos() == 2);
    for (int i = 0; i < 6; i++)   /* 60 passos em frames abaixo de DT_MAXIMO */
        simular(10.0f * PASSO_FIXO);
    VERIFICA(lasersAtivos() == 0);

    /* sem inimigos no caminho, os mesmos lasers continuam ativos */
    novaPartida(0);
    simularCom(&cmd, PASSO_FIXO);
    for (int i = 0; i < 6; i++)
        simular(10.0f * PASSO_FIXO);
    VERIFICA(lasersAtivos() == 2);

    liberarListaInimigos(&jogo.inimigos);
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

    testarComandosDeCombate();

    return RESULTADO_TESTES();
}
