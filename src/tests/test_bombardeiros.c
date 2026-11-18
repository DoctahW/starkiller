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
#include "reforcos.h"
#include "teste.h"

#define DT PASSO_FIXO
#define QUASE(a, b) (fabsf((a) - (b)) < 1e-3f)

static void iniciar(Jogo *g)
{
    memset(g, 0, sizeof *g);
    iniciarJogador(&g->jogador);
    iniciarDestroier(g);
    g->jogador.pos = (Vector3){ 8000, 0, 0 };
    g->aliados[0].ativo = 0;
}

static Inimigo emPassagem(float z)
{
    Inimigo in = criarInimigo(YWING, (Vector3){ 0, YWING_ALTURA_RUN, z }, 1);

    in.faseBombardeio = BOMBA_APROXIMAR;
    in.vel = (Vector3){ 0, 0, -YWING_VEL_RUN };
    return in;
}

static int torpedos(const Jogo *g)
{
    int n = 0;
    for (int i = 0; i < MAX_LASERS; i++)
        n += g->lasers[i].ativo && g->lasers[i].torpedo;
    return n;
}

static void testarDuasBombasParabolicasERetirada(void)
{
    Jogo g;
    Inimigo in = emPassagem(850.0f);
    int segundo = 0;
    Vector3 velTorpedo = { 0 };

    iniciar(&g);
    ia_atualizar(&in, &g, DT);
    VERIFICA(in.bombasLancadas == 1 && torpedos(&g) == 1);
    VERIFICA(g.escudoDestroier == ESCUDO_MAX_DESTROIER); /* lançar ainda não é acertar */
    {
        for (int i = 0; i < MAX_LASERS; i++)
            if (g.lasers[i].ativo && g.lasers[i].torpedo)
                velTorpedo = g.lasers[i].vel;
    }
    VERIFICA(velTorpedo.y < 0.0f);          /* a parábola já começa indo para baixo */
    VERIFICA(g.lasers[0].gravidade > 0.0f);

    for (int i = 1; i < 180; i++) {
        ia_atualizar(&in, &g, DT);
        if (!segundo && in.bombasLancadas == 2) segundo = i;
    }
    VERIFICA(segundo * DT >= YWING_INTERVALO_BOMBAS - DT);
    VERIFICA(segundo * DT <= YWING_INTERVALO_BOMBAS + DT);
    VERIFICA(torpedos(&g) == 2);
    VERIFICA(in.bombasLancadas == 2);
    VERIFICA(in.faseBombardeio == BOMBA_RETIRAR);
    VERIFICA(!ywingBombardeando(&in));
}

static void testarDanoSomenteNoImpacto(void)
{
    Jogo g;
    Inimigo in = emPassagem(800.0f);
    float dano = DANO_TORPEDO_YWING * FRACAO_DANO_TORPEDO_DESTROIER;

    iniciar(&g);
    VERIFICA(dispararTorpedoYwing(&g, &in));
    VERIFICA(g.escudoDestroier == ESCUDO_MAX_DESTROIER);
    for (int i = 0; i < 5 * PASSOS_POR_SEGUNDO; i++) {
        atualizarLasers(&g, DT);
        colidirLasersInimigos(&g, DT);
    }
    VERIFICA(torpedos(&g) == 0);
    VERIFICA(QUASE(g.escudoDestroier, ESCUDO_MAX_DESTROIER - dano));
    VERIFICA(g.cascoDestroier == CASCO_MAX_DESTROIER);
    VERIFICA(g.esperaEscudoDestroier == ATRASO_ESCUDO_DESTROIER);

    /* O torpedo é mais forte que um laser de caça no Destróier (fração própria de 0,4). */
    VERIFICA(DANO_TORPEDO_YWING * FRACAO_DANO_TORPEDO_DESTROIER
             > DANO_LASER_INIMIGO * FRACAO_DANO_DESTROIER);
}

static void testarRestricoesDeLancamento(void)
{
    Jogo g;
    Inimigo in;

    iniciar(&g);
    in = emPassagem(850.0f);
    in.faseBombardeio = BOMBA_REAGRUPAR;
    ia_atualizar(&in, &g, DT);
    VERIFICA(torpedos(&g) == 0);
    in.faseBombardeio = BOMBA_RETIRAR;
    ia_atualizar(&in, &g, DT);
    VERIFICA(torpedos(&g) == 0);
    in = emPassagem(3000.0f);
    ia_atualizar(&in, &g, DT);
    VERIFICA(torpedos(&g) == 0); /* longe demais: espera chegar perto */

    /* Pool cheio não gasta a munição da passagem nem o cooldown. */
    for (int i = 0; i < MAX_LASERS; i++) g.lasers[i].ativo = 1;
    in = emPassagem(800.0f);
    ia_atualizar(&in, &g, DT);
    VERIFICA(in.bombasLancadas == 0 && in.cooldownTiro == 0.0f);
    g.lasers[0].ativo = 0;
    ia_atualizar(&in, &g, DT);
    VERIFICA(in.bombasLancadas == 1 && g.lasers[0].torpedo);
}

static void testarAlinhamentoEGrupo(void)
{
    Jogo g;
    int ciclos[2] = { 0 }, tiros[2] = { 0 };
    int maximo = 0, dentro = 0, semTiros = 0;

    iniciar(&g);
    for (int i = 0; i < 2; i++) {
        Inimigo in = criarInimigo(YWING, ywingPontoEstagio(i + 1), i + 1);

        in.grupoBombardeio = 7;
        in.faseBombardeio = BOMBA_REAGRUPAR;
        VERIFICA(inserirInimigo(&g.inimigos, in));
    }
    atualizarInimigos(&g, DT);
    VERIFICA(g.inimigos.inicio->dado.faseBombardeio == BOMBA_APROXIMAR);
    VERIFICA(g.inimigos.fim->dado.faseBombardeio == BOMBA_APROXIMAR);

    for (int passo = 0; passo < 120 * PASSOS_POR_SEGUNDO; passo++) {
        FaseBombardeio fases[2] = { g.inimigos.inicio->dado.faseBombardeio,
                                    g.inimigos.fim->dado.faseBombardeio };
        g.tempoDecorrido = passo * DT;
        atualizarInimigos(&g, DT);
        int i = 0;
        for (NoInimigo *no = g.inimigos.inicio; no; no = no->prox, i++) {
            Inimigo *in = &no->dado;
            if (in->bombasLancadas > maximo) maximo = in->bombasLancadas;
            if (in->bombasLancadas > tiros[i]) tiros[i] = in->bombasLancadas;
            if (fases[i] == BOMBA_APROXIMAR && in->faseBombardeio == BOMBA_RETIRAR) {
                ciclos[i]++;
                semTiros |= tiros[i] < 1;
                tiros[i] = 0;
            }
            dentro |= tocaDestroier(in->pos, RAIO_YWING);
        }
        atualizarLasers(&g, DT);
        colidirLasersInimigos(&g, DT);
    }
    VERIFICA(ciclos[0] >= 1 && ciclos[1] >= 1);
    VERIFICA(maximo == YWING_MAX_BOMBAS && !semTiros);
    VERIFICA(!dentro);
    VERIFICA(g.escudoDestroier < ESCUDO_MAX_DESTROIER);
    liberarListaInimigos(&g.inimigos);
}

static void testarEsperaLimitada(void)
{
    Jogo g;
    Inimigo in;

    iniciar(&g);
    in = criarInimigo(YWING, ywingPontoEstagio(1), 1);
    in.faseBombardeio = BOMBA_REAGRUPAR;
    in.grupoBombardeio = 1;
    for (int i = 0; i < (int)(YWING_ESPERA_GRUPO / DT) - 1; i++)
        ia_atualizar(&in, &g, DT);
    VERIFICA(in.faseBombardeio == BOMBA_REAGRUPAR);
    for (int i = 0; i < 3; i++) ia_atualizar(&in, &g, DT);
    VERIFICA(in.faseBombardeio == BOMBA_APROXIMAR);

    /* Um companheiro distante não bloqueia o atacante indefinidamente. */
    in = criarInimigo(YWING, ywingPontoEstagio(1), 1);
    in.faseBombardeio = BOMBA_REAGRUPAR;
    in.grupoBombardeio = 2;
    VERIFICA(inserirInimigo(&g.inimigos, criarInimigo(YWING, (Vector3){ 0, 0, 8200 }, 2)));
    g.inimigos.fim->dado.faseBombardeio = BOMBA_REAGRUPAR;
    g.inimigos.fim->dado.grupoBombardeio = 2;
    for (int i = 0; i < (int)(YWING_ESPERA_GRUPO / DT) + 3; i++)
        ia_atualizar(&in, &g, DT);
    VERIFICA(in.faseBombardeio == BOMBA_APROXIMAR);
    VERIFICA(g.inimigos.inicio->dado.faseBombardeio == BOMBA_REAGRUPAR);
    liberarListaInimigos(&g.inimigos);
}

static void testarDefesaComLaserParabolico(void)
{
    Jogo g;
    Inimigo in = criarInimigo(YWING, (Vector3){ 0, 1600, 3000 }, 1);
    int achou = 0;

    iniciar(&g);
    g.jogador.pos = (Vector3){ 0, 1600, 2000 };
    g.jogador.frente = (Vector3){ 0, 0, -1 };
    in.faseBombardeio = BOMBA_REAGRUPAR;
    ia_atualizar(&in, &g, DT);
    for (int i = 0; i < MAX_LASERS; i++)
        if (g.lasers[i].ativo && !g.lasers[i].doJogador && !g.lasers[i].doAliado) {
            achou = 1;
            VERIFICA(g.lasers[i].tipoOrigem == YWING);
            VERIFICA(g.lasers[i].gravidade > 0.0f);
        }
    VERIFICA(achou);
    VERIFICA(QUASE(in.cooldownTiro, COOLDOWN_TIRO_YWING));
}

int main(void)
{
    testarDuasBombasParabolicasERetirada();
    testarDanoSomenteNoImpacto();
    testarRestricoesDeLancamento();
    testarAlinhamentoEGrupo();
    testarEsperaLimitada();
    testarDefesaComLaserParabolico();
    return RESULTADO_TESTES();
}
