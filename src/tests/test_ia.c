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

#define DT (1.0f / 60.0f)
#define QUASE(a, b) (fabsf((a) - (b)) < 1e-4f)

static Inimigo novoInimigo(TipoNave tipo, Vector3 pos, EstadoIA estado)
{
    Inimigo in = criarInimigo(tipo, pos, 1);

    in.estadoIA = in.estadoAtaque = estado;
    return in;
}

static void simular(Inimigo *in, Jogo *g, int passos)
{
    for (int i = 0; i < passos; i++)
        ia_atualizar(in, g, DT);
}

static void iniciarJogoVazio(Jogo *g)
{
    memset(g, 0, sizeof *g);
    iniciarJogador(&g->jogador);
    iniciarDestroier(g);
    g->jogador.pos = (Vector3){ 0, 0, 7000 };
}

static int lasersAtivos(const Jogo *g)
{
    int n = 0;

    for (int i = 0; i < MAX_LASERS; i++)
        n += g->lasers[i].ativo != 0;
    return n;
}

/* Tabela 2.2: cada tipo respeita a própria velocidade máxima. */
static void testarVelocidadeMaximaPorTipo(void)
{
    Jogo g;
    const struct { TipoNave tipo; float velMax; } casos[] = {
        { XWING, VEL_XWING },
        { AWING, VEL_AWING },
    };

    iniciarJogoVazio(&g);
    for (int i = 0; i < (int)(sizeof casos / sizeof casos[0]); i++) {
        Inimigo in = novoInimigo(casos[i].tipo, (Vector3){ 0, 0, 5000 }, IA_ATACAR_JOGADOR);

        simular(&in, &g, 300);
        VERIFICA(Vector3Length(in.vel) <= casos[i].velMax * FATOR_VELOCIDADE_MAX + 1e-3f);
        VERIFICA(Vector3Length(in.vel) > 0.90f * casos[i].velMax);
    }
}

/* IA-01: o caça persegue o TIE do jogador. */
static void testarPersegueJogador(void)
{
    Jogo g;
    Inimigo in;
    float antes;

    iniciarJogoVazio(&g);
    in = novoInimigo(AWING, Vector3Add(g.jogador.pos, (Vector3){ 0.0f, 0.0f, 1130.0f }), IA_ATACAR_JOGADOR);
    antes = Vector3Distance(in.pos, g.jogador.pos);
    simular(&in, &g, 120);
    VERIFICA(Vector3Distance(in.pos, g.jogador.pos) < antes);
    VERIFICA(in.estadoAtaque == IA_ATACAR_JOGADOR);
    VERIFICA(in.vel.z < 0.0f);
}

/* IA_FUGIR: o caça se afasta da referência do estado de ataque. */
static void testarFogeDoJogador(void)
{
    Jogo g;
    Inimigo in;
    float antes, z0;

    iniciarJogoVazio(&g);
    in = novoInimigo(AWING, Vector3Add(g.jogador.pos, (Vector3){ 0.0f, 0.0f, 500.0f }), IA_FUGIR);
    in.estadoAtaque = IA_ATACAR_JOGADOR;
    antes = Vector3Distance(in.pos, g.jogador.pos);
    z0 = in.pos.z;
    simular(&in, &g, 120);
    VERIFICA(Vector3Distance(in.pos, g.jogador.pos) > antes);
    VERIFICA(in.pos.z > z0);
}

/* X-Wing e A-Wing priorizam o TIE mais próximo e nunca miram o Destróier por conta própria. */
static void testarFocoEmCacasTie(void)
{
    Jogo g;
    Inimigo in;

    iniciarJogoVazio(&g);
    g.aliados[0].ativo = 1;
    g.aliados[0].pos = (Vector3){ 0, 0, 6800 };
    g.aliados[0].vel = (Vector3){ 0 };
    g.aliados[0].casco = CASCO_ALIADO;
    in = novoInimigo(XWING, (Vector3){ 0, 0, 6000 }, IA_ATACAR_DESTROIER);
    ia_atualizar(&in, &g, DT);
    VERIFICA(in.estadoIA == IA_ATACAR_ALIADO && in.alvoAliado == 0);

    /* Sem TIEs vivos, o A-Wing patrola; o X-Wing só pode ir ao Destróier pelo sorteio de 10%. */
    g.jogador.morto = 1;
    g.aliados[0].ativo = 0;
    g.escudoDestroier = 0.0f;
    g.tempoDecorrido = 0.0f;
    in = novoInimigo(AWING, (Vector3){ 0, 0, 6000 }, IA_ATACAR_JOGADOR);
    ia_atualizar(&in, &g, DT);
    VERIFICA(in.estadoIA == IA_PATRULHAR);
}

/* Com o escudo do Destróier em 0, alguns X-Wings livres trocam o foco; outros continuam patrol. */
static void testarXwingFocoDestroierDezPorCento(void)
{
    Jogo g;
    int foco = 0, total = 0;

    iniciarJogoVazio(&g);
    g.jogador.morto = 1;
    g.escudoDestroier = 0.0f;
    for (int id = 1; id <= 20; id++)
        for (int janela = 0; janela < 20; janela++) {
            Inimigo in = criarInimigo(XWING, (Vector3){ 0, 0, 6000 }, id);

            g.tempoDecorrido = janela * 5.0f + 0.1f;
            in.focoDestroier = 0;
            ia_atualizar(&in, &g, DT);
            foco += in.estadoAtaque == IA_ATACAR_DESTROIER;
            total++;
        }
    VERIFICA(foco > 0 && foco < total);

    /* Escudo cheio: nenhum deles foca o Destróier. */
    g.escudoDestroier = ESCUDO_MAX_DESTROIER;
    foco = 0;
    for (int id = 1; id <= 20; id++) {
        Inimigo in = criarInimigo(XWING, (Vector3){ 0, 0, 6000 }, id);

        ia_atualizar(&in, &g, DT);
        foco += in.estadoAtaque == IA_ATACAR_DESTROIER;
    }
    VERIFICA(foco == 0);
}

/* Um X-Wing cobre até dois Y-Wings: 3 Y-Wings pedem 2 escoltas. */
static void testarEscoltaUmParaDois(void)
{
    Jogo g;
    int escoltando = 0;

    iniciarJogoVazio(&g);
    for (int i = 0; i < 3; i++)
        VERIFICA(inserirInimigo(&g.inimigos, criarInimigo(YWING, (Vector3){ (float)i * 100, 1600, 4000 }, 10 + i)));
    for (int i = 0; i < 2; i++)
        VERIFICA(inserirInimigo(&g.inimigos, criarInimigo(XWING, (Vector3){ (float)i * 100, 500, 4500 }, 20 + i)));
    ia_atribuirEscolta(&g);
    for (const NoInimigo *no = g.inimigos.inicio; no; no = no->prox) {
        const Inimigo *in = &no->dado;

        if (in->tipo != XWING)
            continue;
        escoltando += in->escoltaYwing >= 0;
        if (in->escoltaYwing >= 0)
            VERIFICA(in->escoltaYwing2 >= 0 || in->escoltaYwing == 12);
    }
    VERIFICA(escoltando == 2);
    liberarListaInimigos(&g.inimigos);
}

/* O X-Wing de escolta intercepta o TIE que travou um dos Y-Wings do seu par. */
static void testarEscoltaInterceptaLocker(void)
{
    Jogo g;
    Inimigo *xwing;
    NoInimigo *ywing;

    iniciarJogoVazio(&g);
    VERIFICA(inserirInimigo(&g.inimigos, criarInimigo(YWING, (Vector3){ 0, 1600, 4000 }, 1)));
    ywing = g.inimigos.inicio;
    VERIFICA(inserirInimigo(&g.inimigos, criarInimigo(XWING, (Vector3){ 0, 1600, 6000 }, 2)));
    xwing = &g.inimigos.fim->dado;
    ia_atribuirEscolta(&g);
    VERIFICA(xwing->escoltaYwing == 1);

    g.inimigos.alvo = ywing;   /* jogador travou o Y-Wing */
    ia_atualizar(xwing, &g, DT);
    VERIFICA(xwing->estadoAtaque == IA_ATACAR_JOGADOR);

    g.inimigos.alvo = NULL;
    g.aliados[0].ativo = 1;
    g.aliados[0].casco = CASCO_ALIADO;
    g.aliados[0].pos = (Vector3){ 0, 1600, 5000 };
    g.aliados[0].alvoId = 1;   /* TIE aliado travou o Y-Wing */
    ia_atribuirEscolta(&g);
    ia_atualizar(xwing, &g, DT);
    VERIFICA(xwing->estadoAtaque == IA_ATACAR_ALIADO && xwing->alvoAliado == 0);
    liberarListaInimigos(&g.inimigos);
}

/* IA-03: X-Wing atira a 480 tiros/min e A-Wing a 360. */
static void testarCadenciaDeTiro(void)
{
    Jogo g;
    Inimigo in;

    iniciarJogoVazio(&g);
    in = novoInimigo(AWING, Vector3Add(g.jogador.pos, (Vector3){ 0.0f, 0.0f, 1130.0f }), IA_ATACAR_JOGADOR);
    in.vel = (Vector3){ 0.0f, 0.0f, -VEL_AWING };
    ia_atualizar(&in, &g, DT);
    VERIFICA(lasersAtivos(&g) == 1);
    VERIFICA(QUASE(in.cooldownTiro, COOLDOWN_TIRO_AWING));
    ia_atualizar(&in, &g, DT);
    VERIFICA(lasersAtivos(&g) == 1);

    iniciarJogoVazio(&g);
    in = novoInimigo(XWING, Vector3Add(g.jogador.pos, (Vector3){ 0.0f, 0.0f, 1130.0f }), IA_ATACAR_JOGADOR);
    in.vel = (Vector3){ 0.0f, 0.0f, -VEL_XWING };
    ia_atualizar(&in, &g, DT);
    VERIFICA(lasersAtivos(&g) == 1);
    VERIFICA(QUASE(in.cooldownTiro, COOLDOWN_TIRO_XWING));

    /* Fora do cone não dispara. */
    iniciarJogoVazio(&g);
    in = novoInimigo(AWING, Vector3Add(g.jogador.pos, (Vector3){ 0.0f, 0.0f, 1130.0f }), IA_ATACAR_JOGADOR);
    in.vel = (Vector3){ 0.0f, 0.0f, VEL_AWING };
    ia_atualizar(&in, &g, DT);
    VERIFICA(lasersAtivos(&g) == 0);
}

/*
 * Passagem de ataque: o A-Wing não voa até o jogador. Rompe antes de DIST_ROMPER_JOGADOR,
 * nunca encosta no TIE, se afasta e volta a atacar várias vezes, atirando nas passagens.
 */
static void testarPassagemNoJogador(void)
{
    Jogo g;
    Inimigo in;
    EstadoIA antes;
    float menor = 1e9f;
    int voltas = 0, fugas = 0;

    iniciarJogoVazio(&g);
    in = novoInimigo(AWING, (Vector3){ 0.0f, 0.0f, 6000.0f }, IA_ATACAR_JOGADOR);
    antes = in.estadoIA;
    for (int i = 0; i < 60 * 60; i++) {
        ia_atualizar(&in, &g, DT);
        menor = fminf(menor, Vector3Distance(in.pos, g.jogador.pos));
        if (in.estadoIA != antes) {
            fugas += in.estadoIA == IA_FUGIR;
            voltas += in.estadoIA == IA_ATACAR_JOGADOR;
            antes = in.estadoIA;
        }
    }
    VERIFICA(menor > 10.0f * (RAIO_AWING + RAIO_JOGADOR));
    VERIFICA(fugas >= 3);
    VERIFICA(voltas >= 3);
    VERIFICA(lasersAtivos(&g) > 0);
}

/* IA-04: o Y-Wing não atravessa o casco nem no caminho para a posição de bombardeio. */
static void testarYwingNaoAtravessaOCasco(void)
{
    const Vector3 partidas[] = {
        { 0.0f, 0.0f, 4600.0f }, { 1500.0f, 0.0f, 0.0f },
        { 900.0f, 400.0f, -1500.0f }, { 0.0f, 0.0f, 0.0f }, { 0.0f, 0.0f, -90.0f },
    };

    for (int p = 0; p < (int)(sizeof partidas / sizeof partidas[0]); p++) {
        Jogo g;
        Inimigo in = novoInimigo(YWING, partidas[p], IA_ATACAR_DESTROIER);
        int dentro = 0;

        iniciarJogoVazio(&g);
        for (int i = 0; i < 1800; i++) {
            ia_atualizar(&in, &g, DT);
            dentro |= tocaDestroier(in.pos, raioInimigo(in.tipo));
        }
        VERIFICA(dentro == 0);
        VERIFICA(isfinite(in.pos.x) && isfinite(in.pos.y) && isfinite(in.pos.z));
    }
}

/* IA-05: os fatores de dificuldade vão de 1,0 aos extremos e saturam nas pontas. */
static void testarFatoresDeDificuldade(void)
{
    float meio = 0.5f * DURACAO_PARTIDA;

    VERIFICA(QUASE(progressoDificuldade(0.0f), 0.0f));
    VERIFICA(QUASE(progressoDificuldade(meio), 0.5f));
    VERIFICA(QUASE(progressoDificuldade(DURACAO_PARTIDA), 1.0f));
    VERIFICA(QUASE(progressoDificuldade(2.0f * DURACAO_PARTIDA), 1.0f));
    VERIFICA(QUASE(progressoDificuldade(-1.0f), 0.0f));
    VERIFICA(QUASE(fatorVelocidadeIA(0.0f), 1.0f));
    VERIFICA(QUASE(fatorVelocidadeIA(meio), 1.15f));
    VERIFICA(QUASE(fatorVelocidadeIA(DURACAO_PARTIDA), FATOR_VELOCIDADE_MAX));
    VERIFICA(QUASE(fatorCooldownIA(0.0f), 1.0f));
    VERIFICA(QUASE(fatorCooldownIA(DURACAO_PARTIDA), FATOR_COOLDOWN_MIN));
}

/* IA-05: no fim da partida o caça chega a FATOR_VELOCIDADE_MAX vezes a velocidade do tipo. */
static void testarVelocidadeCresceComOTempo(void)
{
    Jogo g;
    Inimigo in = novoInimigo(AWING, (Vector3){ 0, 0, 5000 }, IA_ATACAR_JOGADOR);
    float velMax = VEL_AWING * FATOR_VELOCIDADE_MAX;

    iniciarJogoVazio(&g);
    g.tempoDecorrido = DURACAO_PARTIDA;
    simular(&in, &g, 300);
    VERIFICA(Vector3Length(in.vel) <= velMax + 1e-3f);
    VERIFICA(Vector3Length(in.vel) > 0.90f * velMax);
}

static void testarAceleracaoAcompanhaFator(void)
{
    Jogo g;
    Inimigo inicio = novoInimigo(AWING, (Vector3){ 0, 0, 5000 }, IA_ATACAR_JOGADOR);
    Inimigo fim = inicio;

    iniciarJogoVazio(&g);
    g.tempoDecorrido = 0.0f;
    ia_atualizar(&inicio, &g, DT);
    g.tempoDecorrido = DURACAO_PARTIDA;
    ia_atualizar(&fim, &g, DT);
    VERIFICA(Vector3Length(inicio.vel) > 0.0f);
    VERIFICA(fabsf(Vector3Length(fim.vel) / Vector3Length(inicio.vel) - 1.69f) < 0.05f);
}

/* Separação: dois caças no mesmo ponto, com o mesmo alvo, se afastam em vez de se empilhar. */
static void testarSeparacao(void)
{
    Jogo g;
    Inimigo in = novoInimigo(AWING, (Vector3){ 0.0f, 0.0f, 7000.0f }, IA_ATACAR_JOGADOR);

    iniciarJogoVazio(&g);
    VERIFICA(inserirInimigo(&g.inimigos, in) == 1);
    in.id = 2;
    VERIFICA(inserirInimigo(&g.inimigos, in) == 1);
    for (int i = 0; i < 120; i++)
        atualizarInimigos(&g, DT);
    VERIFICA(Vector3Distance(g.inimigos.inicio->dado.pos, g.inimigos.fim->dado.pos) >
             RAIO_AWING + RAIO_AWING);
    liberarListaInimigos(&g.inimigos);
}

int main(void)
{
    testarVelocidadeMaximaPorTipo();
    testarPersegueJogador();
    testarFogeDoJogador();
    testarFocoEmCacasTie();
    testarXwingFocoDestroierDezPorCento();
    testarEscoltaUmParaDois();
    testarEscoltaInterceptaLocker();
    testarCadenciaDeTiro();
    testarPassagemNoJogador();
    testarYwingNaoAtravessaOCasco();
    testarFatoresDeDificuldade();
    testarVelocidadeCresceComOTempo();
    testarAceleracaoAcompanhaFator();
    testarSeparacao();
    return RESULTADO_TESTES();
}
