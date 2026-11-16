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
    jogo.jogador.pos = (Vector3){ 0.0f, 0.0f, 0.0f };   /* origem, para contas simples */
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
    VERIFICA(contarAtivos(&jogo) == 2);                /* expirou neste passo: ainda colide */
    atualizarLasers(&jogo, 0.0f);
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
    VERIFICA(disparos >= 19 && disparos <= 21);        /* 0,15 s = 9 frames: 20 disparos em 180 frames */
    VERIFICA(contarAtivos(&jogo) <= 2 * 14);

    /* O erro de float de 9 passos de 1/60 s não pode empurrar o disparo para o 10º frame. */
    novoJogo();
    VERIFICA(dispararLaser(&jogo) == 1);
    for (i = 0; i < 8; i++) {
        atualizarLasers(&jogo, 1.0f / 60.0f);
        VERIFICA(dispararLaser(&jogo) == 0);
    }
    atualizarLasers(&jogo, 1.0f / 60.0f);
    VERIFICA(dispararLaser(&jogo) == 1);
}

static int acerta(Vector3 a, Vector3 b, Vector3 c, float r, float *t)
{
    return segmentoAcertaEsfera(a, b, c, r, t);
}

static void testarSegmentoEsfera(void)
{
    Vector3 o = { 0.0f, 0.0f, 0.0f };
    Vector3 c = { 0.0f, 0.0f, 50.0f };
    float t = -1.0f;

    /* De frente: entra em z = 45 (raio 5), 45% de um segmento de 0 a 100. */
    VERIFICA(acerta(o, (Vector3){ 0.0f, 0.0f, 100.0f }, c, 5.0f, &t));
    VERIFICA(proximo(t, 0.45f));

    /* Túnel: os dois extremos estão fora da esfera, mas o caminho a atravessa. */
    VERIFICA(acerta((Vector3){ 0.0f, 0.0f, 40.0f }, (Vector3){ 0.0f, 0.0f, 60.0f }, c, 5.0f, &t));
    VERIFICA(proximo(t, 0.25f));
    VERIFICA(acerta((Vector3){ 0.0f, 0.0f, 0.0f }, (Vector3){ 0.0f, 0.0f, 1000.0f }, c, 0.5f, NULL));

    /* Passa ao lado, a mais que o raio: erra. */
    VERIFICA(!acerta(o, (Vector3){ 0.0f, 0.0f, 100.0f }, (Vector3){ 6.0f, 0.0f, 50.0f }, 5.0f, NULL));
    /* Rasante dentro do raio: acerta. */
    VERIFICA(acerta(o, (Vector3){ 0.0f, 0.0f, 100.0f }, (Vector3){ 4.0f, 0.0f, 50.0f }, 5.0f, NULL));
    /* Tangente exata conta como acerto. */
    VERIFICA(acerta(o, (Vector3){ 0.0f, 0.0f, 100.0f }, (Vector3){ 5.0f, 0.0f, 50.0f }, 5.0f, NULL));

    /* Segmento que termina antes da esfera erra; esfera atrás da origem erra. */
    VERIFICA(!acerta(o, (Vector3){ 0.0f, 0.0f, 40.0f }, c, 5.0f, NULL));
    VERIFICA(!acerta((Vector3){ 0.0f, 0.0f, 100.0f }, (Vector3){ 0.0f, 0.0f, 200.0f }, c, 5.0f, NULL));
    VERIFICA(!acerta((Vector3){ 0.0f, 0.0f, 60.0f }, (Vector3){ 0.0f, 0.0f, 100.0f }, c, 5.0f, NULL));

    /* Começa dentro da esfera: acerta com t = 0. */
    VERIFICA(acerta((Vector3){ 0.0f, 0.0f, 52.0f }, (Vector3){ 0.0f, 0.0f, 120.0f }, c, 5.0f, &t));
    VERIFICA(t == 0.0f);

    /* Segmento degenerado (a == b): só acerta se estiver dentro. */
    VERIFICA(acerta((Vector3){ 0.0f, 0.0f, 52.0f }, (Vector3){ 0.0f, 0.0f, 52.0f }, c, 5.0f, NULL));
    VERIFICA(!acerta(o, o, c, 5.0f, NULL));

    /* Direção oblíqua. */
    VERIFICA(acerta(o, (Vector3){ 100.0f, 100.0f, 100.0f }, (Vector3){ 50.0f, 50.0f, 50.0f }, 1.0f, NULL));
}

static void testarRaios(void)
{
    VERIFICA(proximo(raioInimigo(XWING), RAIO_XWING));
    VERIFICA(proximo(raioInimigo(YWING), RAIO_YWING));
    VERIFICA(proximo(raioInimigo(AWING), RAIO_AWING));
}

/* Coloca um inimigo de teste numa lista encadeada montada à mão (sem depender de inserirInimigo). */
static void ligar(NoInimigo *nos, int n)
{
    int i;

    jogo.inimigos.inicio = n > 0 ? &nos[0] : NULL;
    jogo.inimigos.fim = n > 0 ? &nos[n - 1] : NULL;
    jogo.inimigos.tamanho = n;
    for (i = 0; i < n; i++) {
        nos[i].ant = i > 0 ? &nos[i - 1] : NULL;
        nos[i].prox = i + 1 < n ? &nos[i + 1] : NULL;
    }
}

static void testarColisaoLasers(void)
{
    NoInimigo nos[2];
    float dt = 1.0f / 60.0f;

    memset(nos, 0, sizeof nos);
    novoJogo();
    nos[0].dado.tipo = XWING;
    nos[0].dado.pos = (Vector3){ -3.0f * CANHAO_LATERAL, 0.0f, 100.0f };
    ligar(nos, 1);

    /* O X-Wing fica em x = -9: só o laser do canhão em x = -3 o atinge (distância 6 < raio 8); o outro (x = +3) passa a 12. */
    dispararLaser(&jogo);
    atualizarLasers(&jogo, dt);
    VERIFICA(colidirLasers(&jogo, dt) == 0);          /* ainda longe: z ~ 15 */
    atualizarLasers(&jogo, 0.15f);
    {
        int acertos = colidirLasers(&jogo, 0.15f);    /* z ~ 105 depois de 0,15 s: segmento 15..105 */

        VERIFICA(acertos == 1);
        VERIFICA(contarAtivos(&jogo) == 1);
    }

    /* Mesmo cenário com um passo grande: o segmento evita o túnel. */
    novoJogo();
    ligar(nos, 1);
    nos[0].dado.pos = (Vector3){ -3.0f * CANHAO_LATERAL, 0.0f, 300.0f };
    dispararLaser(&jogo);
    atualizarLasers(&jogo, 1.0f);                     /* pos.z ~ 605, anterior ~ 5 */
    VERIFICA(colidirLasers(&jogo, 1.0f) == 1);
    VERIFICA(contarAtivos(&jogo) == 1);

    /* Inimigo fora do alcance: ninguém acerta e os lasers continuam ativos. */
    novoJogo();
    ligar(nos, 1);
    nos[0].dado.pos = (Vector3){ 200.0f, 200.0f, 100.0f };
    dispararLaser(&jogo);
    atualizarLasers(&jogo, 0.2f);
    VERIFICA(colidirLasers(&jogo, 0.2f) == 0);
    VERIFICA(contarAtivos(&jogo) == 2);

    /* Lista vazia: sem acertos, sem falhas. */
    novoJogo();
    dispararLaser(&jogo);
    atualizarLasers(&jogo, 0.2f);
    VERIFICA(colidirLasers(&jogo, 0.2f) == 0);
    VERIFICA(contarAtivos(&jogo) == 2);
}

static void testarAcertoNoUltimoPasso(void)
{
    const float dt = 1.0f / 60.0f;   /* o laser anda 10 u por passo */
    NoInimigo no = { 0 };

    /* A vida acaba no mesmo passo em que o segmento cruza a esfera: o acerto não pode se perder. */
    novoJogo();
    no.dado.tipo = XWING;
    no.dado.pos = (Vector3){ 0.0f, 0.0f, 105.0f };
    ligar(&no, 1);
    jogo.lasers[0] = (Laser){ .pos = { 0.0f, 0.0f, 90.0f }, .vel = { 0.0f, 0.0f, 600.0f }, .vida = dt * 0.5f,
                              .ativo = 1, .doJogador = 1 };
    atualizarLasers(&jogo, dt);                        /* z = 100, vida < 0 */
    VERIFICA(jogo.lasers[0].vida <= 0.0f);
    VERIFICA(colidirLasers(&jogo, dt) == 1);
    VERIFICA(!jogo.lasers[0].ativo);

    /* Sem acerto, o laser expirado some no passo seguinte. */
    novoJogo();
    ligar(&no, 0);
    jogo.lasers[0] = (Laser){ .pos = { 0.0f, 0.0f, 90.0f }, .vel = { 0.0f, 0.0f, 600.0f }, .vida = dt * 0.5f,
                              .ativo = 1, .doJogador = 1 };
    atualizarLasers(&jogo, dt);
    VERIFICA(colidirLasers(&jogo, dt) == 0 && jogo.lasers[0].ativo);
    atualizarLasers(&jogo, dt);
    VERIFICA(!jogo.lasers[0].ativo);
}

static void testarUmAcertoPorLaser(void)
{
    NoInimigo nos[2];
    float dt = 0.2f;

    /* Dois inimigos em fila no caminho do laser do canhão em x = -3 (fora do alcance do outro): só o mais próximo é atingido,
     * e o laser some (não passa a acertar o segundo). */
    memset(nos, 0, sizeof nos);
    novoJogo();
    nos[0].dado.tipo = XWING;
    nos[0].dado.pos = (Vector3){ -3.0f * CANHAO_LATERAL, 0.0f, 100.0f };
    nos[1].dado.tipo = XWING;
    nos[1].dado.pos = (Vector3){ -3.0f * CANHAO_LATERAL, 0.0f, 60.0f };   /* mais perto, mas depois na lista */
    ligar(nos, 2);
    dispararLaser(&jogo);
    atualizarLasers(&jogo, dt);
    VERIFICA(colidirLasers(&jogo, dt) == 1);
    VERIFICA(contarAtivos(&jogo) == 1);

    /* Lasers já inativos e lasers inimigos são ignorados. */
    novoJogo();
    ligar(nos, 1);
    nos[0].dado.tipo = YWING;
    nos[0].dado.pos = (Vector3){ 0.0f, 0.0f, 100.0f };
    jogo.lasers[0] = (Laser){ .pos = { 0.0f, 0.0f, 100.0f }, .vel = { 0.0f, 0.0f, 600.0f }, .vida = 1.0f, .ativo = 0, .doJogador = 1 };
    jogo.lasers[1] = (Laser){ .pos = { 0.0f, 0.0f, 100.0f }, .vel = { 0.0f, 0.0f, 600.0f }, .vida = 1.0f, .ativo = 1, .doJogador = 0 };
    VERIFICA(colidirLasers(&jogo, dt) == 0);
    VERIFICA(jogo.lasers[1].ativo == 1);

    /* Dois lasers do jogador no mesmo inimigo: dois acertos, os dois somem. */
    novoJogo();
    ligar(nos, 1);
    nos[0].dado.tipo = AWING;
    nos[0].dado.pos = (Vector3){ 0.0f, 0.0f, 100.0f };
    jogo.lasers[3] = (Laser){ .pos = { 0.0f, 0.0f, 100.0f }, .vel = { 0.0f, 0.0f, 600.0f }, .vida = 1.0f, .ativo = 1, .doJogador = 1 };
    jogo.lasers[7] = jogo.lasers[3];
    VERIFICA(colidirLasers(&jogo, dt) == 2);
    VERIFICA(contarAtivos(&jogo) == 0);
}

int main(void)
{
    testarDisparo();
    testarCooldown();
    testarMovimentoEVida();
    testarOrientacao();
    testarPoolCheio();
    testarCadenciaContinua();
    testarSegmentoEsfera();
    testarRaios();
    testarColisaoLasers();
    testarUmAcertoPorLaser();
    testarAcertoNoUltimoPasso();

    return RESULTADO_TESTES();
}
