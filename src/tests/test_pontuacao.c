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

    /* Dano zera a sequência, mas não os pontos */
    registrarDanoJogador(&g);
    VERIFICA(g.sequenciaSemDano == 0);
    VERIFICA(g.pontos == 950);
    registrarAbate(&g, XWING);
    VERIFICA(g.pontos == 1050); /* voltou ao multiplicador x1 */
    VERIFICA(g.abates == 8);

    /* Bônus de vitória: porcentagem de casco x 50 */
    g = jogoNovo();
    g.pontos = 1000;
    g.vitoria = 1;
    g.cascoDestroier = 2000.0f;
    VERIFICA(calcularPontuacao(&g) == 6000);

    g.cascoDestroier = 1000.0f; /* 50% -> 2500 */
    VERIFICA(calcularPontuacao(&g) == 3500);

    g.cascoDestroier = 1500.0f; /* 75% -> 3750 */
    VERIFICA(calcularPontuacao(&g) == 4750);

    g.cascoDestroier = 1999.0f; /* 99,95% -> 4998 (arredonda) */
    VERIFICA(calcularPontuacao(&g) == 5998);

    /* Sem vitória não há bônus */
    g.vitoria = 0;
    g.cascoDestroier = 2000.0f;
    VERIFICA(calcularPontuacao(&g) == 1000);

    /* Casco fora da faixa é limitado a 0..100% */
    g.vitoria = 1;
    g.cascoDestroier = 2500.0f;
    VERIFICA(calcularPontuacao(&g) == 6000);
    g.cascoDestroier = -100.0f;
    VERIFICA(calcularPontuacao(&g) == 1000);

    /* calcularPontuacao não altera o Jogo */
    g.cascoDestroier = 1200.0f;
    {
        int pontos = g.pontos;
        int sequencia = g.sequenciaSemDano;
        int abates = g.abates;
        int total = calcularPontuacao(&g);

        VERIFICA(total == calcularPontuacao(&g));
        VERIFICA(g.pontos == pontos);
        VERIFICA(g.sequenciaSemDano == sequencia);
        VERIFICA(g.abates == abates);
    }

    return RESULTADO_TESTES();
}
