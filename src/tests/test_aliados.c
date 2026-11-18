#include <math.h>
#include <string.h>

#include "aliados.h"
#include "combate.h"
#include "destroier.h"
#include "ia.h"
#include "inimigos.h"
#include "jogo.h"
#include "nave.h"
#include "raymath.h"
#include "teste.h"

#define DT PASSO_FIXO
#define QUASE(a, b) (fabsf((a) - (b)) < 1e-3f)

static void iniciar(Jogo *g)
{
    memset(g, 0, sizeof *g);
    iniciarJogador(&g->jogador);
    iniciarDestroier(g);
    g->jogador.pos = (Vector3){ 0, 7000, 0 };
}

static Aliado aliadoEm(Vector3 pos, PapelAliado papel)
{
    return (Aliado){ .pos = pos, .vel = { VEL_ALIADO, 0, 0 }, .casco = CASCO_ALIADO,
                     .ativo = 1, .alvoId = -1, .alvoYwing = -1, .papel = papel };
}

static int ativos(const Jogo *g)
{
    int n = 0;

    for (int i = 0; i < MAX_ALIADOS; i++)
        n += g->aliados[i].ativo;
    return n;
}

static void testarInicioReforcosERespawn(void)
{
    Jogo g;
    int defensores = 0, interceptadores = 0, abaixo = 0;

    iniciar(&g);
    iniciarAliados(&g);
    VERIFICA(ativos(&g) == ALIADOS_INICIAIS);
    for (int i = 0; i < MAX_ALIADOS; i++) {
        if (!g.aliados[i].ativo)
            continue;
        VERIFICA(g.aliados[i].casco == CASCO_ALIADO);
        VERIFICA(g.aliados[i].alvoId == -1 && g.aliados[i].alvoYwing == -1);
        defensores += g.aliados[i].papel == ALIADO_DEFENDER;
        interceptadores += g.aliados[i].papel == ALIADO_INTERCEPTAR;
        abaixo += g.aliados[i].pos.y < SPAWN_ALA_Y + 1.0f;
    }
    VERIFICA(defensores == ALIADOS_GUARDA_MAX
             && interceptadores == ALIADOS_MAX - ALIADOS_GUARDA_MAX
             && abaixo == ALIADOS_MAX);

    /* O roster é fixo: já começa cheio e não cresce com o tempo. */
    g.tempoDecorrido = 20.0f;
    atualizarAliados(&g, DT);
    VERIFICA(ativos(&g) == ALIADOS_MAX);
    g.tempoDecorrido = 400.0f;
    atualizarAliados(&g, DT);
    VERIFICA(ativos(&g) == ALIADOS_MAX);
    VERIFICA(tamanhoRosterAliados(&g) == ALIADOS_MAX);
    {
        int guardas = 0, interceptores = 0;

        for (int i = 0; i < MAX_ALIADOS; i++)
            if (g.aliados[i].ativo) {
                guardas += g.aliados[i].papel == ALIADO_DEFENDER;
                interceptores += g.aliados[i].papel == ALIADO_INTERCEPTAR;
            }
        VERIFICA(guardas == ALIADOS_GUARDA_MAX);
        VERIFICA(interceptores == ALIADOS_MAX - ALIADOS_GUARDA_MAX);
    }

    /* Abatido, o aliado volta em 5 s; se todos caírem, um volta imediatamente. */
    for (int i = 0; i < MAX_ALIADOS; i++)
        if (g.aliados[i].ativo)
            danificarAliado(&g, i, CASCO_ALIADO);
    VERIFICA(ativos(&g) == 0);
    atualizarAliados(&g, DT);
    VERIFICA(ativos(&g) >= 1);
    g.tempoDecorrido = 400.0f + 5.5f;
    for (int i = 0; i < 6 * 60; i++)
        atualizarAliados(&g, DT);
    VERIFICA(ativos(&g) >= ALIADOS_MAX - 1);
}

static void testarPatrulhaEObstaculos(void)
{
    Jogo g;
    int colisoes = 0, fora = 0, velocidadeInvalida = 0;

    iniciar(&g);
    iniciarAliados(&g);
    for (int passo = 0; passo < 20 * PASSOS_POR_SEGUNDO; passo++) {
        g.tempoDecorrido = passo * DT;
        atualizarAliados(&g, DT);
        for (int i = 0; i < MAX_ALIADOS; i++) {
            if (!g.aliados[i].ativo)
                continue;
            colisoes |= tocaDestroier(g.aliados[i].pos, RAIO_ALIADO);
            fora |= Vector3Length(g.aliados[i].pos) > RAIO_ARENA + 0.01f;
            velocidadeInvalida |= Vector3Length(g.aliados[i].vel) > VEL_ALIADO + 0.01f;
        }
    }
    VERIFICA(!colisoes && !fora && !velocidadeInvalida);

    g.aliados[0].pos = (Vector3){ 0 };
    g.aliados[1].pos = (Vector3){ 12000, 0, 0 };
    atualizarAliados(&g, DT);
    VERIFICA(!tocaDestroier(g.aliados[0].pos, RAIO_ALIADO));
    VERIFICA(Vector3Length(g.aliados[1].pos) <= RAIO_ARENA + 0.01f);
}

static void testarPapeisEAlvoRemovido(void)
{
    Jogo g;
    iniciar(&g);
    g.aliados[0] = aliadoEm((Vector3){ 4000, 1000, 0 }, ALIADO_DEFENDER);
    g.aliados[1] = aliadoEm((Vector3){ 4100, 1000, 0 }, ALIADO_INTERCEPTAR);
    VERIFICA(inserirInimigo(&g.inimigos, criarInimigo(XWING, (Vector3){ 4500, 1000, 0 }, 1)));
    VERIFICA(inserirInimigo(&g.inimigos, criarInimigo(YWING, (Vector3){ 5000, 1000, 0 }, 2)));
    g.inimigos.inicio->prox->dado.faseBombardeio = BOMBA_APROXIMAR;
    VERIFICA(inserirInimigo(&g.inimigos, criarInimigo(AWING, (Vector3){ 7000, 1000, 0 }, 3)));
    atualizarAliados(&g, DT);
    VERIFICA(g.aliados[0].alvoId == 2); /* bombardeiro, mesmo com um X-Wing mais perto */
    VERIFICA(g.aliados[1].alvoId == 3); /* caça no espaço aberto */
    removerInimigo(&g.inimigos, g.inimigos.inicio->prox);
    ordenarPorDistancia(&g.inimigos);
    atualizarAliados(&g, DT);
    VERIFICA(g.aliados[0].alvoId == 1);
    removerInimigo(&g.inimigos, g.inimigos.inicio);
    atualizarAliados(&g, DT);
    VERIFICA(g.aliados[0].alvoId == -1); /* não abandona a defesa para ir ao inimigo distante */
    VERIFICA(g.aliados[1].alvoId == 3);
    liberarListaInimigos(&g.inimigos);
    atualizarAliados(&g, DT);
    VERIFICA(g.aliados[1].alvoId == -1);
}

static void testarInterceptacaoNaRazaoDeUmParaDois(void)
{
    Jogo g;
    int interceptando = 0;

    iniciar(&g);
    iniciarAliados(&g);
    VERIFICA(inserirInimigo(&g.inimigos, criarInimigo(YWING, (Vector3){ 0, 1600, 4300 }, 1)));
    VERIFICA(inserirInimigo(&g.inimigos, criarInimigo(YWING, (Vector3){ 200, 1600, 4300 }, 2)));
    g.inimigos.inicio->dado.faseBombardeio = BOMBA_APROXIMAR;
    g.inimigos.fim->dado.faseBombardeio = BOMBA_APROXIMAR;
    atualizarAliados(&g, DT);
    for (int i = 0; i < MAX_ALIADOS; i++)
        interceptando += g.aliados[i].ativo && g.aliados[i].alvoYwing >= 0;
    VERIFICA(interceptando == 1);
    VERIFICA(g.aliados[0].alvoId == g.aliados[0].alvoYwing);

    /* Quatro Y-Wings: dois caças, sempre independente do jogador estar ajudando. */
    VERIFICA(inserirInimigo(&g.inimigos, criarInimigo(YWING, (Vector3){ 400, 1600, 4300 }, 3)));
    VERIFICA(inserirInimigo(&g.inimigos, criarInimigo(YWING, (Vector3){ 600, 1600, 4300 }, 4)));
    for (NoInimigo *no = g.inimigos.inicio; no; no = no->prox)
        no->dado.faseBombardeio = BOMBA_APROXIMAR;
    g.inimigos.alvo = g.inimigos.inicio; /* o jogador também está travando um Y-Wing */
    interceptando = 0;
    atualizarAliados(&g, DT);
    for (int i = 0; i < MAX_ALIADOS; i++)
        interceptando += g.aliados[i].ativo && g.aliados[i].alvoYwing >= 0;
    VERIFICA(interceptando == 2);
    liberarListaInimigos(&g.inimigos);
}

static void testarDisparoEAbateSemPontos(void)
{
    Jogo g;
    Inimigo in = criarInimigo(XWING, (Vector3){ 5000, 1000, 0 }, 1);
    iniciar(&g);
    g.aliados[0] = aliadoEm((Vector3){ 4000, 1000, 0 }, ALIADO_DEFENDER);
    VERIFICA(inserirInimigo(&g.inimigos, in));
    atualizarAliados(&g, DT);
    VERIFICA(g.lasers[0].ativo && g.lasers[0].doAliado && !g.lasers[0].doJogador);
    VERIFICA(g.lasers[1].ativo && g.lasers[1].doAliado && !g.lasers[1].doJogador);
    VERIFICA(QUASE(g.aliados[0].cooldownTiro, COOLDOWN_TIRO_ALIADO));
    atualizarAliados(&g, DT);
    VERIFICA(g.lasers[2].ativo == 0);
    for (int i = 0; i < 60; i++) {
        atualizarLasers(&g, DT);
        resolverLasers(&g, DT);
    }
    VERIFICA(QUASE(g.inimigos.inicio->dado.escudo, ESCUDO_XWING - 2.0f * DANO_LASER_ALIADO));
    g.inimigos.inicio->dado.escudo = 0;
    g.inimigos.inicio->dado.casco = DANO_LASER_ALIADO;
    VERIFICA(dispararLaserAliado(&g, &g.aliados[0], &g.inimigos.inicio->dado));
    for (int i = 0; i < 60; i++) {
        atualizarLasers(&g, DT);
        resolverLasers(&g, DT);
    }
    VERIFICA(g.inimigos.tamanho == 0 && g.inimigos.alvo == NULL);
    VERIFICA(g.pontos == 0 && g.abates == 0 && g.sequenciaSemDano == 0);
    limparQuedasInimigas();
}

static void testarInimigosAtacamAliados(void)
{
    Jogo g;
    Inimigo in = criarInimigo(AWING, (Vector3){ 5000, 1000, 0 }, 1);
    iniciar(&g);
    g.aliados[0] = aliadoEm((Vector3){ 4000, 1000, 0 }, ALIADO_DEFENDER);
    g.aliados[0].vel = (Vector3){ 0 };
    in.vel = (Vector3){ -VEL_AWING, 0, 0 };
    ia_atualizar(&in, &g, DT);
    VERIFICA(in.estadoIA == IA_ATACAR_ALIADO && in.alvoAliado == 0);
    VERIFICA(g.lasers[0].ativo && !g.lasers[0].doAliado && !g.lasers[0].doJogador);
    VERIFICA(QUASE(in.cooldownTiro, COOLDOWN_TIRO_AWING));
    for (int i = 0; i < 60; i++) {
        atualizarLasers(&g, DT);
        colidirLasersInimigos(&g, DT);
    }
    VERIFICA(QUASE(g.aliados[0].casco, CASCO_ALIADO - DANO_LASER_INIMIGO));
    VERIFICA(g.jogador.casco == CASCO_JOGADOR);
    danificarAliado(&g, 0, 0.0f);
    danificarAliado(&g, 0, -10.0f);
    VERIFICA(QUASE(g.aliados[0].casco, CASCO_ALIADO - DANO_LASER_INIMIGO));
    danificarAliado(&g, 0, CASCO_ALIADO * 2);
    VERIFICA(!g.aliados[0].ativo && g.aliados[0].casco == 0.0f);
    VERIFICA(QUASE(g.aliados[0].tempoRespawn, ALIADOS_TEMPO_RESPAWN));
    VERIFICA(g.explosoes[0].ativa);
    danificarAliado(&g, 0, CASCO_ALIADO);
    VERIFICA(!g.explosoes[1].ativa);
    ia_atualizar(&in, &g, DT);
    VERIFICA(in.alvoAliado == -1 && in.estadoIA == IA_ATACAR_JOGADOR);
}

static void testarFogoAmigoEObstaculos(void)
{
    Jogo g;
    Inimigo in = criarInimigo(XWING, (Vector3){ 5000, 1000, 0 }, 1);
    iniciar(&g);
    g.jogador.pos = (Vector3){ 4500, 1000, 0 };
    g.aliados[0] = aliadoEm((Vector3){ 4000, 1000, 0 }, ALIADO_DEFENDER);
    g.aliados[1] = aliadoEm((Vector3){ 4750, 1000, 0 }, ALIADO_DEFENDER);
    VERIFICA(inserirInimigo(&g.inimigos, in));
    VERIFICA(dispararLaserAliado(&g, &g.aliados[0], &in));
    for (int i = 0; i < 60; i++) {
        atualizarLasers(&g, DT);
        resolverLasers(&g, DT);
        colidirLasersInimigos(&g, DT);
    }
    VERIFICA(g.aliados[1].casco == CASCO_ALIADO);
    VERIFICA(g.jogador.casco == CASCO_JOGADOR);
    VERIFICA(g.inimigos.inicio->dado.escudo < ESCUDO_XWING);
    liberarListaInimigos(&g.inimigos);

    /* O casco bloqueia o tiro aliado sem receber dano. */
    iniciar(&g);
    g.aliados[0] = aliadoEm((Vector3){ 0, 0, 3800 }, ALIADO_DEFENDER);
    in = criarInimigo(XWING, (Vector3){ 0, 0, -4000 }, 1);
    VERIFICA(inserirInimigo(&g.inimigos, in));
    VERIFICA(dispararLaserAliado(&g, &g.aliados[0], &in));
    for (int i = 0; i < 120; i++) {
        atualizarLasers(&g, DT);
        resolverLasers(&g, DT);
        colidirLasersInimigos(&g, DT);
    }
    VERIFICA(!g.lasers[0].ativo);
    VERIFICA(g.inimigos.inicio->dado.escudo == ESCUDO_XWING);
    VERIFICA(g.cascoDestroier == CASCO_MAX_DESTROIER && g.escudoDestroier == ESCUDO_MAX_DESTROIER);
    liberarListaInimigos(&g.inimigos);

    /* A colisão inimiga escolhe o aliado antes do jogador que está atrás dele. */
    iniciar(&g);
    g.jogador.pos = (Vector3){ 4000, 1000, 0 };
    g.aliados[0] = aliadoEm((Vector3){ 4500, 1000, 0 }, ALIADO_DEFENDER);
    g.lasers[0] = (Laser){ .pos = { 3900, 1000, 0 }, .vel = { -2000, 0, 0 }, .ativo = 1, .vida = 1, .dano = 10 };
    VERIFICA(colidirLasersInimigos(&g, 1.0f) == 0);
    VERIFICA(g.aliados[0].casco == CASCO_ALIADO - 10.0f);
    VERIFICA(g.jogador.casco == CASCO_JOGADOR);

    /* Reutilizar um slot apaga a origem aliada e o tipo torpedo. */
    g.lasers[0].torpedo = g.lasers[0].doAliado = 1;
    VERIFICA(dispararLaser(&g));
    VERIFICA(g.lasers[0].doJogador && !g.lasers[0].doAliado && !g.lasers[0].torpedo);
}

int main(void)
{
    testarInicioReforcosERespawn();
    testarPatrulhaEObstaculos();
    testarPapeisEAlvoRemovido();
    testarInterceptacaoNaRazaoDeUmParaDois();
    testarDisparoEAbateSemPontos();
    testarInimigosAtacamAliados();
    testarFogoAmigoEObstaculos();
    return RESULTADO_TESTES();
}
