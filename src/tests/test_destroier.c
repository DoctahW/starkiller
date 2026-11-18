#include <math.h>

#include "destroier.h"
#include "jogo.h"
#include "teste.h"

#define QUASE(a, b) (fabsf((a) - (b)) < 1e-3f)

static void testarDano(void)
{
    Jogo g = { 0 };

    iniciarDestroier(&g);
    VERIFICA(g.cascoDestroier == CASCO_MAX_DESTROIER);
    VERIFICA(g.escudoDestroier == ESCUDO_MAX_DESTROIER);
    VERIFICA(g.esperaEscudoDestroier == 0.0f);

    danificarDestroier(&g, 20.0f);
    VERIFICA(g.escudoDestroier == ESCUDO_MAX_DESTROIER - 20.0f);
    VERIFICA(g.cascoDestroier == CASCO_MAX_DESTROIER);
    VERIFICA(g.esperaEscudoDestroier == ATRASO_ESCUDO_DESTROIER);

    /* Um acerto que acaba exatamente com o escudo não tira casco. */
    danificarDestroier(&g, g.escudoDestroier);
    VERIFICA(g.escudoDestroier == 0.0f);
    VERIFICA(g.cascoDestroier == CASCO_MAX_DESTROIER);
    danificarDestroier(&g, 10.0f);
    VERIFICA(g.cascoDestroier == CASCO_MAX_DESTROIER - 10.0f);

    /* O excedente atravessa o escudo; nenhum valor fica negativo. */
    iniciarDestroier(&g);
    danificarDestroier(&g, ESCUDO_MAX_DESTROIER + 25.0f);
    VERIFICA(g.escudoDestroier == 0.0f);
    VERIFICA(g.cascoDestroier == CASCO_MAX_DESTROIER - 25.0f);
    danificarDestroier(&g, CASCO_MAX_DESTROIER * 2.0f);
    VERIFICA(g.cascoDestroier == 0.0f);
    VERIFICA(g.escudoDestroier == 0.0f);
    atualizarEscudoDestroier(&g, 100.0f);
    VERIFICA(g.cascoDestroier == 0.0f);
    VERIFICA(g.escudoDestroier == 0.0f);

    /* Nova partida restaura os dois valores e cancela a espera anterior. */
    iniciarDestroier(&g);
    VERIFICA(g.cascoDestroier == CASCO_MAX_DESTROIER);
    VERIFICA(g.escudoDestroier == ESCUDO_MAX_DESTROIER);
    VERIFICA(g.esperaEscudoDestroier == 0.0f);
}

static void testarRecarga(void)
{
    Jogo g = { 0 };

    iniciarDestroier(&g);
    danificarDestroier(&g, ESCUDO_MAX_DESTROIER + 10.0f);
    atualizarEscudoDestroier(&g, ATRASO_ESCUDO_DESTROIER - 0.5f);
    VERIFICA(g.escudoDestroier == 0.0f);
    VERIFICA(QUASE(g.esperaEscudoDestroier, 0.5f));
    atualizarEscudoDestroier(&g, 0.5f);
    VERIFICA(g.escudoDestroier == 0.0f);
    VERIFICA(g.esperaEscudoDestroier == 0.0f);
    atualizarEscudoDestroier(&g, 1.0f);
    VERIFICA(QUASE(g.escudoDestroier, RECARGA_ESCUDO_DESTROIER));
    VERIFICA(g.cascoDestroier == CASCO_MAX_DESTROIER - 10.0f);

    /* Mesmo absorvido pelo escudo, um novo acerto interrompe a recarga. */
    danificarDestroier(&g, 1.0f);
    atualizarEscudoDestroier(&g, ATRASO_ESCUDO_DESTROIER - 0.5f);
    VERIFICA(QUASE(g.escudoDestroier, RECARGA_ESCUDO_DESTROIER - 1.0f));
    /* Só os 0,5 s que ultrapassam a espera entram na regeneração. */
    atualizarEscudoDestroier(&g, 1.0f);
    VERIFICA(QUASE(g.escudoDestroier, RECARGA_ESCUDO_DESTROIER * 1.5f - 1.0f));
    VERIFICA(g.esperaEscudoDestroier == 0.0f);
    atualizarEscudoDestroier(&g, 100.0f);
    VERIFICA(g.escudoDestroier == ESCUDO_MAX_DESTROIER);
    VERIFICA(g.cascoDestroier == CASCO_MAX_DESTROIER - 10.0f);

    /* Dano contínuo ao casco também reinicia a espera. */
    danificarDestroier(&g, ESCUDO_MAX_DESTROIER);
    for (int i = 0; i < 3; i++) {
        atualizarEscudoDestroier(&g, ATRASO_ESCUDO_DESTROIER - 0.5f);
        danificarDestroier(&g, 1.0f);
        VERIFICA(g.escudoDestroier == 0.0f);
        VERIFICA(g.esperaEscudoDestroier == ATRASO_ESCUDO_DESTROIER);
    }
    VERIFICA(g.cascoDestroier == CASCO_MAX_DESTROIER - 13.0f);

    /* Valores sem efeito não curam, causam dano ou reiniciam a espera. */
    atualizarEscudoDestroier(&g, 0.5f);
    danificarDestroier(&g, 0.0f);
    danificarDestroier(&g, -10.0f);
    atualizarEscudoDestroier(&g, 0.0f);
    atualizarEscudoDestroier(&g, -1.0f);
    VERIFICA(g.esperaEscudoDestroier == ATRASO_ESCUDO_DESTROIER - 0.5f);
    VERIFICA(g.escudoDestroier == 0.0f);
    VERIFICA(g.cascoDestroier == CASCO_MAX_DESTROIER - 13.0f);
}

static void testarRecargaPorPassos(void)
{
    Jogo g = { 0 }, outro = { 0 };
    int passos = (int)((ATRASO_ESCUDO_DESTROIER + 2.0f) * PASSOS_POR_SEGUNDO);

    iniciarDestroier(&g);
    iniciarDestroier(&outro);
    danificarDestroier(&g, ESCUDO_MAX_DESTROIER);
    danificarDestroier(&outro, ESCUDO_MAX_DESTROIER);
    for (int i = 0; i < passos; i++)
        atualizarEscudoDestroier(&g, PASSO_FIXO);
    atualizarEscudoDestroier(&outro, passos * PASSO_FIXO);
    VERIFICA(fabsf(g.escudoDestroier - 2.0f * RECARGA_ESCUDO_DESTROIER) < 1e-2f);
    VERIFICA(fabsf(g.escudoDestroier - outro.escudoDestroier) < 1e-2f);
    VERIFICA(g.cascoDestroier == CASCO_MAX_DESTROIER);
}

int main(void)
{
    testarDano();
    testarRecarga();
    testarRecargaPorPassos();
    return RESULTADO_TESTES();
}
