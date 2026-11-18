#include "jogo.h"
#include "pontuacao.h"
#include "teste.h"

static Jogo jogoNovo(void)
{
    Jogo g = { 0 };

    return g;
}

int main(void)
{
    Jogo g = jogoNovo();

    /* Partida zerada */
    VERIFICA(calcularPontuacao(&g) == 0);

    /* Valor por tipo de nave */
    g = jogoNovo();
    registrarAbate(&g, XWING);
    VERIFICA(g.pontos == 100);
    VERIFICA(g.abates == 1);
    VERIFICA(g.sequenciaSemDano == 1);

    g = jogoNovo();
    registrarAbate(&g, YWING);
    VERIFICA(g.pontos == 150);

    g = jogoNovo();
    registrarAbate(&g, AWING);
    VERIFICA(g.pontos == 200);

    /* Multiplicador por sequência: x1, x1, x1, x1,5, x1,5, x1,5, x2 */
    g = jogoNovo();
    registrarAbate(&g, XWING);
    VERIFICA(g.pontos == 100);
    registrarAbate(&g, XWING);
    VERIFICA(g.pontos == 200);
    registrarAbate(&g, XWING);
    VERIFICA(g.pontos == 300); /* sequência chegou a 3, mas o abate valeu x1 */
    registrarAbate(&g, XWING);
    VERIFICA(g.pontos == 450);
    registrarAbate(&g, XWING);
    VERIFICA(g.pontos == 600);
    registrarAbate(&g, XWING);
    VERIFICA(g.pontos == 750);
    registrarAbate(&g, XWING);
    VERIFICA(g.pontos == 950); /* 7º abate já vale x2 */
    VERIFICA(g.abates == 7);
    VERIFICA(g.sequenciaSemDano == 7);

    /* Limites do multiplicador isoladamente */
    VERIFICA(multiplicadorAtual(0) == MULTIPLICADOR_X1);
    VERIFICA(multiplicadorAtual(2) == MULTIPLICADOR_X1);
    VERIFICA(multiplicadorAtual(3) == MULTIPLICADOR_X1_5);
    VERIFICA(multiplicadorAtual(5) == MULTIPLICADOR_X1_5);
    VERIFICA(multiplicadorAtual(6) == MULTIPLICADOR_X2);
    VERIFICA(multiplicadorAtual(100) == MULTIPLICADOR_X2);

    /* Dano zera a sequência, mas não os pontos */
    registrarDanoJogador(&g);
    VERIFICA(g.sequenciaSemDano == 0);
    VERIFICA(g.tempoDano == DURACAO_FEEDBACK_DANO);
    VERIFICA(g.pontos == 950);
    registrarAbate(&g, XWING);
    VERIFICA(g.pontos == 1050); /* voltou ao multiplicador x1 */
    VERIFICA(g.abates == 8);

    /* Tipo inválido não conta como abate nem soma pontos */
    g = jogoNovo();
    registrarAbate(&g, (TipoNave)99);
    VERIFICA(g.pontos == 0);
    VERIFICA(g.abates == 0);
    VERIFICA(g.sequenciaSemDano == 0);

    /* Bônus de vitória: porcentagem de casco x 50, sobre CASCO_MAX_DESTROIER */
    g = jogoNovo();
    g.pontos = 1000;
    g.vitoria = 1;

    g.cascoDestroier = CASCO_MAX_DESTROIER; /* 100% -> 5000 */
    VERIFICA(bonusCasco(&g) == 5000);
    VERIFICA(calcularPontuacao(&g) == 6000);

    g.cascoDestroier = 0.5f * CASCO_MAX_DESTROIER; /* 50% -> 2500 */
    VERIFICA(bonusCasco(&g) == 2500);
    VERIFICA(calcularPontuacao(&g) == 3500);

    g.cascoDestroier = 0.55f * CASCO_MAX_DESTROIER; /* 55% -> 2750 */
    VERIFICA(bonusCasco(&g) == 2750);
    VERIFICA(calcularPontuacao(&g) == 3750);

    g.cascoDestroier = 0.75f * CASCO_MAX_DESTROIER; /* 75% -> 3750 */
    VERIFICA(bonusCasco(&g) == 3750);

    /* Casco fora da faixa é limitado a 0..100% */
    g.cascoDestroier = 1.2f * CASCO_MAX_DESTROIER;
    VERIFICA(bonusCasco(&g) == 5000);
    g.cascoDestroier = -10.0f;
    VERIFICA(bonusCasco(&g) == 0);

    /* Sem vitória (derrota ou partida em andamento) não há bônus */
    g.vitoria = 0;
    g.cascoDestroier = CASCO_MAX_DESTROIER;
    VERIFICA(bonusCasco(&g) == 0);
    VERIFICA(calcularPontuacao(&g) == 1000);
    g.vitoria = -1;
    VERIFICA(bonusCasco(&g) == 0);

    /* calcularPontuacao não altera o Jogo e é idempotente */
    g.vitoria = 1;
    g.cascoDestroier = 0.6f * CASCO_MAX_DESTROIER; /* 60% -> 3000 */
    {
        int pontos = g.pontos;
        int sequencia = g.sequenciaSemDano;
        int abates = g.abates;
        int total = calcularPontuacao(&g);

        VERIFICA(total == 4000);
        VERIFICA(total == calcularPontuacao(&g));
        VERIFICA(g.pontos == pontos);
        VERIFICA(g.sequenciaSemDano == sequencia);
        VERIFICA(g.abates == abates);
        VERIFICA(g.cascoDestroier == 0.6f * CASCO_MAX_DESTROIER);
    }

    return RESULTADO_TESTES();
}
