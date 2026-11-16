#include <math.h>
#include <string.h>

#include "combate.h"
#include "nave.h"
#include "teste.h"

#define EPS 1e-4f

static Jogo jogo;

static int proximo(float a, float b)
{
    return fabsf(a - b) < EPS;
}

static int contarAtivos(const Jogo *g)
{
    int i, n = 0;

    for (i = 0; i < MAX_LASERS; i++)
        n += g->lasers[i].ativo != 0;
    return n;
}

static void novoJogo(void)
{
    memset(&jogo, 0, sizeof jogo);
    iniciarJogador(&jogo.jogador);
}

static void testarDisparo(void)
{
    int i, achados = 0;
    float xMin = 1e9f, xMax = -1e9f;

    novoJogo();
    VERIFICA(contarAtivos(&jogo) == 0);
    VERIFICA(dispararLaser(&jogo) == 1);
    VERIFICA(contarAtivos(&jogo) == 2);

    /* Dois lasers do jogador, à frente, simétricos em torno do eixo, na velocidade de 600 u/s. */
    for (i = 0; i < MAX_LASERS; i++) {
        const Laser *l = &jogo.lasers[i];

        if (!l->ativo)
            continue;
        achados++;
        VERIFICA(l->doJogador == 1);
        VERIFICA(proximo(l->vida, VIDA_LASER) && proximo(l->dano, DANO_LASER));
        VERIFICA(proximo(l->vel.x, 0.0f) && proximo(l->vel.y, 0.0f) && proximo(l->vel.z, VEL_LASER));
        VERIFICA(proximo(l->pos.z, jogo.jogador.pos.z + CANHAO_FRENTE) && proximo(l->pos.y, jogo.jogador.pos.y));
        if (l->pos.x < xMin) xMin = l->pos.x;
        if (l->pos.x > xMax) xMax = l->pos.x;
    }
    VERIFICA(achados == 2);
    VERIFICA(proximo(xMax, CANHAO_LATERAL) && proximo(xMin, -CANHAO_LATERAL));
}

static void testarCooldown(void)
{
    novoJogo();
    VERIFICA(dispararLaser(&jogo) == 1);
    VERIFICA(proximo(jogo.jogador.cooldownLaser, COOLDOWN_LASER));

    /* Em cooldown não dispara. */
    VERIFICA(dispararLaser(&jogo) == 0);
    VERIFICA(contarAtivos(&jogo) == 2);
    atualizarLasers(&jogo, 0.10f);
    VERIFICA(dispararLaser(&jogo) == 0);
    VERIFICA(contarAtivos(&jogo) == 2);

    /* Passados 0,15 s, dispara de novo. */
    atualizarLasers(&jogo, 0.06f);
    VERIFICA(jogo.jogador.cooldownLaser == 0.0f);
    VERIFICA(dispararLaser(&jogo) == 1);
    VERIFICA(contarAtivos(&jogo) == 4);

    /* Cooldown nunca fica negativo. */
    novoJogo();
    atualizarLasers(&jogo, 5.0f);
    VERIFICA(jogo.jogador.cooldownLaser == 0.0f);
}

static void testarMovimentoEVida(void)
{
    int i;

    novoJogo();
    dispararLaser(&jogo);

    /* Move a 600 u/s. */
    atualizarLasers(&jogo, 0.5f);
    for (i = 0; i < MAX_LASERS; i++)
        if (jogo.lasers[i].ativo)
            VERIFICA(proximo(jogo.lasers[i].pos.z, jogo.jogador.pos.z + CANHAO_FRENTE + 300.0f));
    VERIFICA(contarAtivos(&jogo) == 2);

    /* Desativa depois de 2 s de vida. */
    atualizarLasers(&jogo, 1.4f);
    VERIFICA(contarAtivos(&jogo) == 2);
    atualizarLasers(&jogo, 0.2f);
    VERIFICA(contarAtivos(&jogo) == 0);

    /* O slot liberado é reaproveitado. */
    VERIFICA(dispararLaser(&jogo) == 1);
    VERIFICA(contarAtivos(&jogo) == 2);
}

static void testarOrientacao(void)
{
    Comandos c = { .guinada = 1.0f };
    int i;
    int direita = 0;

    /* Dispara na direção da frente atual, não sempre em +Z. */
    novoJogo();
    for (i = 0; i < 200; i++)
        atualizarJogador(&jogo.jogador, &c, 1.0f / 60.0f);
    jogo.jogador.pos = (Vector3){ 0.0f, 0.0f, 0.0f };
    dispararLaser(&jogo);
    for (i = 0; i < MAX_LASERS; i++) {
        const Laser *l = &jogo.lasers[i];

        if (!l->ativo)
            continue;
        VERIFICA(fabsf(l->vel.x - VEL_LASER * jogo.jogador.frente.x) < 1e-2f);
        VERIFICA(fabsf(l->vel.z - VEL_LASER * jogo.jogador.frente.z) < 1e-2f);
        direita++;
    }
    VERIFICA(direita == 2);
}

static void testarPoolCheio(void)
{
    int i;

    /* Pool fixo: com 1 slot livre, não dispara (precisa de 2) e não gasta cooldown. */
    novoJogo();
    for (i = 0; i < MAX_LASERS - 1; i++)
        jogo.lasers[i].ativo = 1;
    VERIFICA(dispararLaser(&jogo) == 0);
    VERIFICA(jogo.jogador.cooldownLaser == 0.0f);
    VERIFICA(contarAtivos(&jogo) == MAX_LASERS - 1);

    /* Com 2 slots livres (não adjacentes), dispara nos dois. */
    jogo.lasers[10].ativo = 0;
    VERIFICA(dispararLaser(&jogo) == 1);
    VERIFICA(contarAtivos(&jogo) == MAX_LASERS);
    VERIFICA(jogo.lasers[10].ativo == 1 && jogo.lasers[MAX_LASERS - 1].ativo == 1);

    /* Pool totalmente cheio: não dispara. */
    jogo.jogador.cooldownLaser = 0.0f;
    VERIFICA(dispararLaser(&jogo) == 0);
}

static void testarCadenciaContinua(void)
{
    int i, disparos = 0;

    /* Segurando o gatilho por 3 s a 60 fps: 1 disparo a cada 9 ou 10 frames (0,15 s), o pool nunca enche. */
    novoJogo();
    for (i = 0; i < 180; i++) {
        disparos += dispararLaser(&jogo);
        atualizarLasers(&jogo, 1.0f / 60.0f);
    }
    VERIFICA(disparos >= 18 && disparos <= 20);
    VERIFICA(contarAtivos(&jogo) <= 2 * 14);
}

int main(void)
{
    testarDisparo();
    testarCooldown();
    testarMovimentoEVida();
    testarOrientacao();
    testarPoolCheio();
    testarCadenciaContinua();

    return RESULTADO_TESTES();
}
