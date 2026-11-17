#include <math.h>

#include "jogo.h"
#include "pontuacao.h"

static int pontosDoTipo(TipoNave tipo)
{
    switch (tipo) {
    case XWING:
        return PONTOS_XWING;
    case YWING:
        return PONTOS_YWING;
    case AWING:
        return PONTOS_AWING;
    default:
        return 0;
    }
}

float multiplicadorAtual(int sequenciaSemDano)
{
    if (sequenciaSemDano >= SEQUENCIA_X2)
        return MULTIPLICADOR_X2;
    if (sequenciaSemDano >= SEQUENCIA_X1_5)
        return MULTIPLICADOR_X1_5;
    return MULTIPLICADOR_X1;
}

void registrarAbate(Jogo *g, TipoNave tipo)
{
    int base = pontosDoTipo(tipo);

    if (base <= 0)
        return; /* tipo desconhecido não conta como abate */

    g->pontos += (int)lroundf((float)base * multiplicadorAtual(g->sequenciaSemDano));
    g->sequenciaSemDano++;
    g->abates++;
}

void registrarDanoJogador(Jogo *g)
{
    g->sequenciaSemDano = 0;
    g->tempoDano = DURACAO_FEEDBACK_DANO;
}

int bonusCasco(const Jogo *g)
{
    float porcentagem;

    if (g->vitoria != 1)
        return 0;

    porcentagem = PORCENTAGEM_MAX * g->cascoDestroier / CASCO_MAX_DESTROIER;
    if (porcentagem < PORCENTAGEM_MIN)
        porcentagem = PORCENTAGEM_MIN;
    if (porcentagem > PORCENTAGEM_MAX)
        porcentagem = PORCENTAGEM_MAX;

    return (int)lroundf(porcentagem * BONUS_POR_PORCENTO);
}

int calcularPontuacao(const Jogo *g)
{
    return g->pontos + bonusCasco(g);
}
