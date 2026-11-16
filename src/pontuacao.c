#include <math.h>

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
    }
    return 0;
}

static float multiplicadorSequencia(int sequenciaSemDano)
{
    if (sequenciaSemDano >= SEQUENCIA_X2)
        return 2.0f;
    if (sequenciaSemDano >= SEQUENCIA_X1_5)
        return 1.5f;
    return 1.0f;
}

void registrarAbate(Jogo *g, TipoNave tipo)
{
    int base = pontosDoTipo(tipo);

    g->pontos += (int)lroundf((float)base * multiplicadorSequencia(g->sequenciaSemDano));
    g->sequenciaSemDano++;
    g->abates++;
}

void registrarDanoJogador(Jogo *g)
{
    g->sequenciaSemDano = 0;
}

int calcularPontuacao(const Jogo *g)
{
    int total = g->pontos;

    if (g->vitoria) {
        float porcentagem = 100.0f * g->cascoDestroier / CASCO_MAX_DESTROIER;

        if (porcentagem < 0.0f)
            porcentagem = 0.0f;
        if (porcentagem > 100.0f)
            porcentagem = 100.0f;
        total += (int)lroundf(porcentagem * BONUS_POR_PORCENTO);
    }
    return total;
}
