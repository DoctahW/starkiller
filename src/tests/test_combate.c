#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "combate.h"
#include "raymath.h"
#include "nave.h"
#include "pontuacao.h"
#include "destroier.h"
#include "inimigos.h"
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
    liberarListaInimigos(&jogo.inimigos);
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

/* Insere um inimigo de teste na lista pela API do módulo (inserirInimigo). */
static void adicionar(TipoNave tipo, Vector3 pos)
{
    VERIFICA(inserirInimigo(&jogo.inimigos, criarInimigo(tipo, pos, jogo.proximoId++)) == 1);
}

static void testarColisaoLasers(void)
{
    float dt = 1.0f / 60.0f;

    novoJogo();
    adicionar(XWING, (Vector3){ -3.0f * CANHAO_LATERAL, 0.0f, 100.0f });

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
    adicionar(XWING, (Vector3){ -3.0f * CANHAO_LATERAL, 0.0f, 300.0f });
    dispararLaser(&jogo);
    atualizarLasers(&jogo, 1.0f);                     /* pos.z ~ 605, anterior ~ 5 */
    VERIFICA(colidirLasers(&jogo, 1.0f) == 1);
    VERIFICA(contarAtivos(&jogo) == 1);

    /* Inimigo fora do alcance: ninguém acerta e os lasers continuam ativos. */
    novoJogo();
    adicionar(XWING, (Vector3){ 200.0f, 200.0f, 100.0f });
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

    /* A vida acaba no mesmo passo em que o segmento cruza a esfera: o acerto não pode se perder. */
    novoJogo();
    adicionar(XWING, (Vector3){ 0.0f, 0.0f, 105.0f });
    jogo.lasers[0] = (Laser){ .pos = { 0.0f, 0.0f, 90.0f }, .vel = { 0.0f, 0.0f, 600.0f }, .vida = dt * 0.5f,
                              .ativo = 1, .doJogador = 1 };
    atualizarLasers(&jogo, dt);                        /* z = 100, vida < 0 */
    VERIFICA(jogo.lasers[0].vida <= 0.0f);
    VERIFICA(colidirLasers(&jogo, dt) == 1);
    VERIFICA(!jogo.lasers[0].ativo);

    /* Sem acerto, o laser expirado some no passo seguinte. */
    novoJogo();
    jogo.lasers[0] = (Laser){ .pos = { 0.0f, 0.0f, 90.0f }, .vel = { 0.0f, 0.0f, 600.0f }, .vida = dt * 0.5f,
                              .ativo = 1, .doJogador = 1 };
    atualizarLasers(&jogo, dt);
    VERIFICA(colidirLasers(&jogo, dt) == 0 && jogo.lasers[0].ativo);
    atualizarLasers(&jogo, dt);
    VERIFICA(!jogo.lasers[0].ativo);
}

static void testarUmAcertoPorLaser(void)
{
    float dt = 0.2f;

    /* Dois inimigos em fila no caminho do laser do canhão em x = -3 (fora do alcance do outro): só o mais próximo é atingido,
     * e o laser some (não passa a acertar o segundo). */
    novoJogo();
    adicionar(XWING, (Vector3){ -3.0f * CANHAO_LATERAL, 0.0f, 100.0f });
    adicionar(XWING, (Vector3){ -3.0f * CANHAO_LATERAL, 0.0f, 60.0f });   /* mais perto, mas depois na lista */
    dispararLaser(&jogo);
    atualizarLasers(&jogo, dt);
    VERIFICA(colidirLasers(&jogo, dt) == 1);
    VERIFICA(contarAtivos(&jogo) == 1);

    /* Lasers já inativos e lasers inimigos são ignorados. */
    novoJogo();
    adicionar(YWING, (Vector3){ 0.0f, 0.0f, 100.0f });
    jogo.lasers[0] = (Laser){ .pos = { 0.0f, 0.0f, 100.0f }, .vel = { 0.0f, 0.0f, 600.0f }, .vida = 1.0f, .ativo = 0, .doJogador = 1 };
    jogo.lasers[1] = (Laser){ .pos = { 0.0f, 0.0f, 100.0f }, .vel = { 0.0f, 0.0f, 600.0f }, .vida = 1.0f, .ativo = 1, .doJogador = 0 };
    VERIFICA(colidirLasers(&jogo, dt) == 0);
    VERIFICA(jogo.lasers[1].ativo == 1);

    /* Dois lasers do jogador no mesmo inimigo: dois acertos, os dois somem. */
    novoJogo();
    adicionar(AWING, (Vector3){ 0.0f, 0.0f, 100.0f });
    jogo.lasers[3] = (Laser){ .pos = { 0.0f, 0.0f, 100.0f }, .vel = { 0.0f, 0.0f, 600.0f }, .vida = 1.0f, .ativo = 1, .doJogador = 1 };
    jogo.lasers[7] = jogo.lasers[3];
    VERIFICA(colidirLasers(&jogo, dt) == 2);
    VERIFICA(contarAtivos(&jogo) == 0);
}

static void testarAplicarDano(void)
{
    Inimigo in = { .escudo = 20.0f, .escudoMax = 20.0f, .casco = 30.0f, .cascoMax = 30.0f };

    /* Escudo absorve primeiro; casco intacto. */
    VERIFICA(aplicarDano(&in, 15.0f) == 0);
    VERIFICA(proximo(in.escudo, 5.0f) && proximo(in.casco, 30.0f));

    /* Dano que passa do escudo: o resto vai para o casco. */
    VERIFICA(aplicarDano(&in, 15.0f) == 0);
    VERIFICA(proximo(in.escudo, 0.0f) && proximo(in.casco, 20.0f));

    /* Sem escudo, direto no casco. */
    VERIFICA(aplicarDano(&in, 10.0f) == 0);
    VERIFICA(proximo(in.casco, 10.0f));

    /* Dano <= 0 não altera nada. */
    VERIFICA(aplicarDano(&in, 0.0f) == 0);
    VERIFICA(aplicarDano(&in, -5.0f) == 0);
    VERIFICA(proximo(in.escudo, 0.0f) && proximo(in.casco, 10.0f));

    /* Casco exato em 0 abate; excesso não deixa casco negativo. */
    VERIFICA(aplicarDano(&in, 10.0f) == 1);
    VERIFICA(proximo(in.casco, 0.0f));
    in.casco = 5.0f;
    VERIFICA(aplicarDano(&in, 100.0f) == 1);
    VERIFICA(proximo(in.casco, 0.0f) && proximo(in.escudo, 0.0f));

    /* Um golpe só atravessa escudo e casco. */
    in = (Inimigo){ .escudo = 20.0f, .casco = 30.0f };
    VERIFICA(aplicarDano(&in, 50.0f) == 1);
    VERIFICA(proximo(in.escudo, 0.0f) && proximo(in.casco, 0.0f));

    /* Dano exato do escudo: escudo zera, casco intacto, sem abate. */
    in = (Inimigo){ .escudo = 20.0f, .casco = 30.0f };
    VERIFICA(aplicarDano(&in, 20.0f) == 0);
    VERIFICA(proximo(in.escudo, 0.0f) && proximo(in.casco, 30.0f));
}

static int contarExplosoes(const Jogo *g)
{
    int i, n = 0;

    for (i = 0; i < MAX_EXPLOSOES; i++)
        n += g->explosoes[i].ativa != 0;
    return n;
}

static void testarExplosoes(void)
{
    int i;

    novoJogo();
    VERIFICA(criarExplosao(&jogo, (Vector3){ 1.0f, 2.0f, 3.0f }, 15.0f) == 1);
    VERIFICA(contarExplosoes(&jogo) == 1);
    VERIFICA(jogo.explosoes[0].ativa && proximo(jogo.explosoes[0].tempo, 0.0f));
    VERIFICA(proximo(jogo.explosoes[0].pos.z, 3.0f) && proximo(jogo.explosoes[0].raioMax, 15.0f));

    /* Avança sem expirar, depois expira. */
    atualizarExplosoes(&jogo, DURACAO_EXPLOSAO * 0.5f);
    VERIFICA(jogo.explosoes[0].ativa && proximo(jogo.explosoes[0].tempo, DURACAO_EXPLOSAO * 0.5f));
    atualizarExplosoes(&jogo, DURACAO_EXPLOSAO * 0.5f);
    VERIFICA(contarExplosoes(&jogo) == 0);

    /* Slot liberado é reaproveitado. */
    VERIFICA(criarExplosao(&jogo, (Vector3){ 0 }, RAIO_EXPLOSAO) == 1);
    VERIFICA(jogo.explosoes[0].ativa);

    /* Pool cheio: ignora, sem crash. */
    novoJogo();
    for (i = 0; i < MAX_EXPLOSOES; i++)
        VERIFICA(criarExplosao(&jogo, (Vector3){ 0 }, RAIO_EXPLOSAO) == 1);
    VERIFICA(criarExplosao(&jogo, (Vector3){ 0 }, RAIO_EXPLOSAO) == 0);
    VERIFICA(contarExplosoes(&jogo) == MAX_EXPLOSOES);
}

/* Lista de n inimigos em heap (removerInimigo libera o nó), x = 0, z = 100, 200, ... */
static void listaHeap(int n, float escudo, float casco)
{
    int i;
    NoInimigo *ant = NULL;

    memset(&jogo.inimigos, 0, sizeof jogo.inimigos);
    for (i = 0; i < n; i++) {
        NoInimigo *no = calloc(1, sizeof *no);

        no->dado.tipo = XWING;
        no->dado.id = i;
        no->dado.pos = (Vector3){ 0.0f, 0.0f, 100.0f * (i + 1) };
        no->dado.escudo = no->dado.escudoMax = escudo;
        no->dado.casco = no->dado.cascoMax = casco;
        no->ant = ant;
        if (ant) ant->prox = no; else jogo.inimigos.inicio = no;
        jogo.inimigos.fim = no;
        ant = no;
    }
    jogo.inimigos.tamanho = n;
}

static void limparHeap(void)
{
    while (jogo.inimigos.inicio)
        removerInimigo(&jogo.inimigos, jogo.inimigos.inicio);
}

static void laserEm(int slot, float z, float dano)
{
    jogo.lasers[slot] = (Laser){ .pos = { 0.0f, 0.0f, z }, .vel = { 0.0f, 0.0f, 600.0f }, .vida = 1.0f,
                                 .ativo = 1, .doJogador = 1, .dano = dano };
}

static void testarAbate(void)
{
    const float dt = 1.0f / 60.0f;

    /* Escudo absorve: não abate, não pontua, não explode. */
    novoJogo();
    listaHeap(1, 50.0f, 30.0f);
    laserEm(0, 100.0f, 10.0f);
    VERIFICA(resolverLasers(&jogo, dt) == 1);
    VERIFICA(!jogo.lasers[0].ativo);
    VERIFICA(jogo.inimigos.tamanho == 1 && proximo(jogo.inimigos.inicio->dado.escudo, 40.0f));
    VERIFICA(jogo.abates == 0 && jogo.pontos == 0 && contarExplosoes(&jogo) == 0);
    limparHeap();

    /* Casco zera: pontua, explode na posição, sai da lista. */
    novoJogo();
    listaHeap(1, 0.0f, 10.0f);
    laserEm(0, 100.0f, 10.0f);
    VERIFICA(resolverLasers(&jogo, dt) == 1);
    VERIFICA(jogo.inimigos.tamanho == 0 && jogo.inimigos.inicio == NULL && jogo.inimigos.fim == NULL);
    VERIFICA(jogo.abates == 1 && jogo.pontos == PONTOS_XWING);
    VERIFICA(contarExplosoes(&jogo) == 1 && proximo(jogo.explosoes[0].pos.z, 100.0f));

    /* O alvo da mira continua válido após o abate do próprio alvo (meio, ponta, único). */
    novoJogo();
    listaHeap(3, 0.0f, 10.0f);
    jogo.inimigos.alvo = jogo.inimigos.inicio->prox;       /* o do meio (z = 200) */
    laserEm(0, 200.0f, 10.0f);
    VERIFICA(resolverLasers(&jogo, dt) == 1);
    VERIFICA(jogo.inimigos.tamanho == 2);
    VERIFICA(jogo.inimigos.alvo != NULL && jogo.inimigos.alvo->dado.id != 1);
    VERIFICA(jogo.inimigos.alvo == jogo.inimigos.inicio || jogo.inimigos.alvo == jogo.inimigos.fim);
    jogo.inimigos.alvo = jogo.inimigos.fim;                /* ponta (z = 300) */
    laserEm(1, 300.0f, 10.0f);
    VERIFICA(resolverLasers(&jogo, dt) == 1);
    VERIFICA(jogo.inimigos.alvo == jogo.inimigos.inicio && jogo.inimigos.tamanho == 1);
    laserEm(2, 100.0f, 10.0f);                             /* único */
    VERIFICA(resolverLasers(&jogo, dt) == 1);
    VERIFICA(jogo.inimigos.alvo == NULL && jogo.inimigos.tamanho == 0);
    VERIFICA(jogo.abates == 3);

    /* Dois lasers no mesmo frame, dois inimigos: nenhum acesso a nó removido. */
    novoJogo();
    listaHeap(2, 0.0f, 10.0f);
    laserEm(0, 100.0f, 10.0f);
    laserEm(1, 200.0f, 10.0f);
    VERIFICA(resolverLasers(&jogo, dt) == 2);
    VERIFICA(jogo.inimigos.tamanho == 0 && jogo.abates == 2 && contarExplosoes(&jogo) == 2);

    /* Dois lasers no mesmo inimigo, o primeiro abate: o segundo não acha mais nada. */
    novoJogo();
    listaHeap(1, 0.0f, 10.0f);
    laserEm(0, 100.0f, 10.0f);
    laserEm(1, 100.0f, 10.0f);
    VERIFICA(resolverLasers(&jogo, dt) == 1);
    VERIFICA(jogo.abates == 1 && jogo.lasers[1].ativo == 1);

    /* Lasers inimigos não ferem os caças. */
    novoJogo();
    listaHeap(1, 0.0f, 10.0f);
    laserEm(0, 100.0f, 10.0f);
    jogo.lasers[0].doJogador = 0;
    VERIFICA(resolverLasers(&jogo, dt) == 0 && jogo.inimigos.tamanho == 1);
    limparHeap();
}

/* Inimigo X-Wing em `pos` com o estado de IA dado (só para os testes dos lasers inimigos). */
static Inimigo inimigoEm(Vector3 pos, EstadoIA ia)
{
    Inimigo in = criarInimigo(XWING, pos, 1);

    in.estadoIA = ia;
    return in;
}

static int primeiroLaserInimigo(void)
{
    int i;

    for (i = 0; i < MAX_LASERS; i++)
        if (jogo.lasers[i].ativo && !jogo.lasers[i].doJogador)
            return i;
    return -1;
}

static void testarDisparoInimigo(void)
{
    Inimigo in;
    Laser *l;
    int i;

    /* Mira o jogador: laser vermelho na direção dele, a VEL_LASER_INIMIGO, saindo do inimigo. */
    novoJogo();
    jogo.jogador.pos = (Vector3){ 0.0f, 0.0f, 300.0f };
    in = inimigoEm((Vector3){ 300.0f, 0.0f, 700.0f }, IA_ATACAR_JOGADOR);
    VERIFICA(dispararLaserInimigo(&jogo, &in) == 1);
    VERIFICA(primeiroLaserInimigo() >= 0);
    l = &jogo.lasers[primeiroLaserInimigo()];
    VERIFICA(l->doJogador == 0 && proximo(l->dano, DANO_LASER_INIMIGO) && proximo(l->vida, VIDA_LASER_INIMIGO));
    VERIFICA(proximo(l->pos.x, 300.0f) && proximo(l->pos.z, 700.0f));
    VERIFICA(proximo(Vector3Length(l->vel), VEL_LASER_INIMIGO));
    VERIFICA(proximo(Vector3DotProduct(Vector3Normalize(l->vel),
                                        Vector3Normalize(Vector3Subtract(jogo.jogador.pos, in.pos))), 1.0f));

    /* Mira o Destróier: direção da origem. */
    novoJogo();
    jogo.jogador.pos = (Vector3){ 900.0f, 0.0f, 0.0f };
    in = inimigoEm((Vector3){ 0.0f, 0.0f, 1000.0f }, IA_ATACAR_DESTROIER);
    VERIFICA(dispararLaserInimigo(&jogo, &in) == 1);
    l = &jogo.lasers[primeiroLaserInimigo()];
    VERIFICA(proximo(l->vel.x, 0.0f) && proximo(l->vel.y, 0.0f) && proximo(l->vel.z, -VEL_LASER_INIMIGO));

    /* Fugindo não atira; sobre o alvo não há direção; pool cheio não atira. */
    novoJogo();
    in = inimigoEm((Vector3){ 0.0f, 0.0f, 1000.0f }, IA_FUGIR);
    VERIFICA(dispararLaserInimigo(&jogo, &in) == 0 && contarAtivos(&jogo) == 0);
    in = inimigoEm((Vector3){ 0.0f, 0.0f, 0.0f }, IA_ATACAR_DESTROIER);
    VERIFICA(dispararLaserInimigo(&jogo, &in) == 0 && contarAtivos(&jogo) == 0);
    in = inimigoEm((Vector3){ 0.0f, 0.0f, 1000.0f }, IA_ATACAR_DESTROIER);
    for (i = 0; i < MAX_LASERS; i++)
        VERIFICA(dispararLaserInimigo(&jogo, &in) == 1);
    VERIFICA(dispararLaserInimigo(&jogo, &in) == 0);
    VERIFICA(contarAtivos(&jogo) == MAX_LASERS);

    /* O disparo não mexe no cooldownTiro (é da IA). */
    novoJogo();
    in = inimigoEm((Vector3){ 0.0f, 0.0f, 1000.0f }, IA_ATACAR_DESTROIER);
    in.cooldownTiro = 0.7f;
    dispararLaserInimigo(&jogo, &in);
    VERIFICA(proximo(in.cooldownTiro, 0.7f));
}

/* Anda o pool até o laser inimigo acertar ou até `max` passos. Devolve os acertos no TIE. */
static int voarLasersInimigos(float dt, int max, int *passos)
{
    int n, acertos = 0;

    for (n = 0; n < max && !acertos && primeiroLaserInimigo() >= 0; n++) {
        atualizarLasers(&jogo, dt);
        acertos += colidirLasersInimigos(&jogo, dt);
    }
    *passos = n;
    return acertos;
}

static void testarColisaoLasersInimigos(void)
{
    Inimigo in;
    float dt = 1.0f / 60.0f;
    int passos;

    /* Acerto no TIE: casco cai, sequência zera, laser some. */
    novoJogo();
    jogo.jogador.pos = (Vector3){ 0.0f, 0.0f, 300.0f };
    jogo.sequenciaSemDano = 4;
    in = inimigoEm((Vector3){ 0.0f, 0.0f, 900.0f }, IA_ATACAR_JOGADOR);
    dispararLaserInimigo(&jogo, &in);
    VERIFICA(voarLasersInimigos(dt, 600, &passos) == 1);
    VERIFICA(proximo(jogo.jogador.casco, 100.0f - DANO_LASER_INIMIGO));
    VERIFICA(jogo.sequenciaSemDano == 0);
    VERIFICA(contarAtivos(&jogo) == 0);
    VERIFICA(proximo(jogo.cascoDestroier, 0.0f));   /* Jogo zerado: o Destróier não foi tocado */

    /* Acerto no Destróier (proa): cascoDestroier cai, o casco do TIE não. */
    novoJogo();
    jogo.cascoDestroier = 100.0f;
    jogo.jogador.pos = (Vector3){ 900.0f, 0.0f, 0.0f };
    in = inimigoEm((Vector3){ 0.0f, 0.0f, 1000.0f }, IA_ATACAR_DESTROIER);
    dispararLaserInimigo(&jogo, &in);
    voarLasersInimigos(dt, 600, &passos);
    VERIFICA(proximo(jogo.cascoDestroier, 100.0f - DANO_LASER_INIMIGO));
    VERIFICA(proximo(jogo.jogador.casco, 100.0f));
    VERIFICA(contarAtivos(&jogo) == 0);

    /* Passo grande: o segmento evita o túnel pelo TIE (esfera pequena). */
    novoJogo();
    jogo.jogador.pos = (Vector3){ 0.0f, 0.0f, 300.0f };
    in = inimigoEm((Vector3){ 0.0f, 0.0f, 600.0f }, IA_ATACAR_JOGADOR);
    dispararLaserInimigo(&jogo, &in);
    atualizarLasers(&jogo, 1.0f);                     /* anda 400 u: de 600 a 200, atravessando o TIE em 300 */
    VERIFICA(colidirLasersInimigos(&jogo, 1.0f) == 1);

    /* Segmento que atravessa o TIE e depois o Destróier: só o primeiro leva o dano. */
    novoJogo();
    jogo.cascoDestroier = 100.0f;
    jogo.jogador.pos = (Vector3){ 0.0f, 0.0f, 300.0f };
    jogo.lasers[0] = (Laser){ .pos = { 0.0f, 0.0f, 100.0f }, .vel = { 0.0f, 0.0f, -400.0f }, .vida = 1.0f,
                              .ativo = 1, .doJogador = 0, .dano = 5.0f };
    VERIFICA(colidirLasersInimigos(&jogo, 1.0f) == 1);   /* segmento de z = 500 a 100 */
    VERIFICA(proximo(jogo.jogador.casco, 95.0f));
    VERIFICA(proximo(jogo.cascoDestroier, 100.0f));
    VERIFICA(contarAtivos(&jogo) == 0);

    /* Segmento curto que só cruza o Destróier: o TIE não leva dano. */
    novoJogo();
    jogo.cascoDestroier = 100.0f;
    jogo.jogador.pos = (Vector3){ 0.0f, 0.0f, 300.0f };
    jogo.lasers[0] = (Laser){ .pos = { 0.0f, 0.0f, 100.0f }, .vel = { 0.0f, 0.0f, -400.0f }, .vida = 1.0f,
                              .ativo = 1, .doJogador = 0, .dano = 5.0f };
    VERIFICA(colidirLasersInimigos(&jogo, 0.25f) == 0);  /* segmento de z = 200 a 100 */
    VERIFICA(proximo(jogo.jogador.casco, 100.0f));
    VERIFICA(proximo(jogo.cascoDestroier, 95.0f));
    VERIFICA(contarAtivos(&jogo) == 0);

    /* Erra os dois: continua ativo e sem dano. Lasers do jogador e inativos são ignorados. */
    novoJogo();
    jogo.cascoDestroier = 100.0f;
    jogo.jogador.pos = (Vector3){ 900.0f, 0.0f, 0.0f };
    jogo.lasers[0] = (Laser){ .pos = { 500.0f, 500.0f, 0.0f }, .vel = { 0.0f, 0.0f, -400.0f }, .vida = 1.0f,
                              .ativo = 1, .doJogador = 0, .dano = 5.0f };
    jogo.lasers[1] = (Laser){ .pos = { 0.0f, 0.0f, 0.0f }, .vel = { 0.0f, 0.0f, 600.0f }, .vida = 1.0f,
                              .ativo = 1, .doJogador = 1, .dano = 5.0f };   /* do jogador, dentro do Destróier */
    jogo.lasers[2] = (Laser){ .pos = { 900.0f, 0.0f, 0.0f }, .vel = { 0 }, .vida = 1.0f,
                              .ativo = 0, .doJogador = 0, .dano = 5.0f };   /* inativo, sobre o TIE */
    VERIFICA(colidirLasersInimigos(&jogo, dt) == 0);
    VERIFICA(jogo.lasers[0].ativo && jogo.lasers[1].ativo);
    VERIFICA(proximo(jogo.jogador.casco, 100.0f));
    VERIFICA(proximo(jogo.cascoDestroier, 100.0f));
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
    testarAplicarDano();
    testarExplosoes();
    testarAbate();
    testarDisparoInimigo();
    testarColisaoLasersInimigos();
    liberarListaInimigos(&jogo.inimigos);

    return RESULTADO_TESTES();
}
