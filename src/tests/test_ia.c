#include <math.h>
#include <string.h>

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

/* Valores da tabela 2.2 e do criterio da IA-02. */
#define BOMBA_DANO    24.0f
#define BOMBA_ALCANCE (685.0f * ESCALA_DESTROIER)
#define ORBITA_RAIO   (550.0f * ESCALA_DESTROIER)

static Inimigo novoInimigo(TipoNave tipo, Vector3 pos, EstadoIA estado)
{
    Inimigo in = criarInimigo(tipo, pos, 1);

    in.estadoIA = estado;
    return in;
}

static void simular(Inimigo *in, Jogo *g, int passos)
{
    for (int i = 0; i < passos; i++)
        ia_atualizar(in, g, DT);
}

/* IA-01: o caça acelera para o alvo, sem inverter o rumo no mesmo frame. */
static void testarAproximaDoDestroier(void)
{
    Jogo g = { 0 };
    Inimigo in = novoInimigo(XWING, (Vector3){ 0.0f, 0.0f, 8200.0f }, IA_ATACAR_DESTROIER);
    float antes = Vector3Length(in.pos);

    iniciarJogador(&g.jogador);

    ia_atualizar(&in, &g, DT);
    VERIFICA(Vector3Length(in.vel) > 0.0f);
    VERIFICA(Vector3Length(in.vel) < VEL_XWING);   /* um passo não chega na velocidade máxima */
    VERIFICA(in.vel.z < 0.0f);                    /* Destróier na origem: -z */
    VERIFICA(Vector3Length(in.pos) < antes);

    simular(&in, &g, 300);
    VERIFICA(Vector3Length(in.vel) <= VEL_XWING + 1e-3f);
    VERIFICA(Vector3Length(in.vel) > 0.99f * VEL_XWING);
    VERIFICA(in.vel.z < -0.99f * VEL_XWING);
    VERIFICA(Vector3Length(in.pos) < antes);
}

/* Tabela 2.2: cada tipo respeita a própria velocidade máxima. */
static void testarVelocidadeMaximaPorTipo(void)
{
    Jogo g = { 0 };
    const struct { TipoNave tipo; float velMax; } casos[] = {
        { XWING, VEL_XWING },
        { YWING, VEL_YWING },
        { AWING, VEL_AWING },
    };

    iniciarJogador(&g.jogador);

    for (int i = 0; i < (int)(sizeof casos / sizeof casos[0]); i++) {
        Inimigo in = novoInimigo(casos[i].tipo, (Vector3){ 0.0f, 0.0f, 8200.0f }, IA_ATACAR_DESTROIER);

        simular(&in, &g, 300);
        VERIFICA(Vector3Length(in.vel) <= casos[i].velMax + 1e-3f);
        VERIFICA(Vector3Length(in.vel) > 0.99f * casos[i].velMax);
    }
}

/* IA-01: o estado IA_ATACAR_JOGADOR persegue a posição do jogador. */
static void testarPersegueJogador(void)
{
    Jogo g = { 0 };
    Inimigo in;
    float antes;

    iniciarJogador(&g.jogador);
    in = novoInimigo(AWING, Vector3Add(g.jogador.pos, (Vector3){ 0.0f, 0.0f, 1130.0f }), IA_ATACAR_JOGADOR);

    antes = Vector3Distance(in.pos, g.jogador.pos);
    simular(&in, &g, 120);
    VERIFICA(Vector3Distance(in.pos, g.jogador.pos) < antes);
    VERIFICA(in.vel.z < 0.0f);
}

/* IA_FUGIR: o caça se afasta do jogador, que é o alvo do seu estado de ataque. */
static void testarFogeDoJogador(void)
{
    Jogo g = { 0 };
    Inimigo in;
    float antes, z0;

    iniciarJogador(&g.jogador);
    in = novoInimigo(AWING, Vector3Add(g.jogador.pos, (Vector3){ 0.0f, 0.0f, 500.0f }), IA_FUGIR);

    antes = Vector3Distance(in.pos, g.jogador.pos);
    z0 = in.pos.z;
    simular(&in, &g, 120);
    VERIFICA(Vector3Distance(in.pos, g.jogador.pos) > antes);
    VERIFICA(in.pos.z > z0);

    /* X-Wing: a referência da fuga é o Destróier (estado de ataque no tempo 0). */
    {
        Jogo gx = { 0 };

        iniciarJogador(&gx.jogador);
        in = novoInimigo(XWING, (Vector3){ 0.0f, 2200.0f, 0.0f }, IA_FUGIR);
        antes = Vector3Length(in.pos);
        simular(&in, &gx, 120);
        VERIFICA(Vector3Length(in.pos) > antes);
    }
}

/*
 * Alvo na mesma posição não pode gerar NaN (divisão por zero no rumo). Na origem,
 * o caça está dentro do casco e o desvio da IA-04 precisa tirá-lo de lá.
 */
static void testarAlvoNaMesmaPosicao(void)
{
    Jogo g = { 0 };
    Inimigo naOrigem = novoInimigo(XWING, (Vector3){ 0.0f, 0.0f, 0.0f }, IA_ATACAR_DESTROIER);
    Inimigo noJogador;

    iniciarJogador(&g.jogador);
    /* A-Wing porque o X-Wing tem o estado sobrescrito pela IA-03 antes de mirar. */
    noJogador = novoInimigo(AWING, (Vector3){ 0.0f, 0.0f, 5000.0f }, IA_ATACAR_JOGADOR);
    g.jogador.pos = noJogador.pos;

    simular(&naOrigem, &g, 10);
    VERIFICA(isfinite(naOrigem.vel.x) && isfinite(naOrigem.vel.y) && isfinite(naOrigem.vel.z));
    VERIFICA(isfinite(naOrigem.pos.x) && isfinite(naOrigem.pos.y) && isfinite(naOrigem.pos.z));
    VERIFICA(!tocaDestroier(naOrigem.pos, raioInimigo(XWING)));

    simular(&noJogador, &g, 10);
    VERIFICA(isfinite(noJogador.vel.x) && isfinite(noJogador.vel.y) && isfinite(noJogador.vel.z));
    VERIFICA(isfinite(noJogador.pos.x) && isfinite(noJogador.pos.y) && isfinite(noJogador.pos.z));
    VERIFICA(QUASE(Vector3Length(noJogador.vel), 0.0f));
    VERIFICA(QUASE(noJogador.pos.z, 5000.0f));
}

/* Seção 3.4, passo 4: atualizarInimigos delega o movimento à IA. */
static void testarAtualizarInimigosMove(void)
{
    Jogo g = { 0 };
    Vector3 antes = { 0.0f, 0.0f, 4600.0f };
    Inimigo in = novoInimigo(XWING, antes, IA_ATACAR_DESTROIER);

    iniciarJogador(&g.jogador);
    in.id = g.proximoId++;
    VERIFICA(inserirInimigo(&g.inimigos, in) == 1);

    atualizarInimigos(&g, DT);

    VERIFICA(g.inimigos.inicio != NULL);
    VERIFICA(g.inimigos.tamanho == 1);
    VERIFICA(g.inimigos.inicio->dado.pos.z < antes.z);
    VERIFICA(Vector3Length(g.inimigos.inicio->dado.vel) > 0.0f);
    VERIFICA(Vector3Length(g.inimigos.inicio->dado.vel) <= VEL_XWING);
    /* a distância do passo é medida antes do movimento, como na seção 3.4 */
    VERIFICA(QUASE(g.inimigos.inicio->dado.distancia, Vector3Distance(antes, g.jogador.pos)));

    liberarListaInimigos(&g.inimigos);
}

/* IA-02: a menos de BOMBA_ALCANCE do Destróier, o Y-Wing causa 24 de dano/s no casco. */
static void testarYwingBombardeia(void)
{
    Jogo g = { 0 };
    Inimigo in = novoInimigo(YWING, (Vector3){ 0.0f, 0.0f, 460.0f }, IA_ATACAR_DESTROIER);

    iniciarJogador(&g.jogador);
    g.cascoDestroier = 2000.0f;

    ia_atualizar(&in, &g, DT);
    VERIFICA(QUASE(g.cascoDestroier, 2000.0f - BOMBA_DANO * DT));

    simular(&in, &g, 59);
    /* 1 s de bombardeio; a tolerância cobre o arredondamento do float a cada passo */
    VERIFICA(fabsf(g.cascoDestroier - (2000.0f - BOMBA_DANO)) < 0.01f);
}

/* IA-02: fora do alcance não há dano; o Y-Wing orbita no plano z = 0. */
static void testarYwingForaDeAlcance(void)
{
    Jogo g = { 0 };
    Inimigo longe = novoInimigo(YWING, (Vector3){ 0.0f, 0.0f, BOMBA_ALCANCE + 900.0f }, IA_ATACAR_DESTROIER);
    Inimigo orbita = novoInimigo(YWING, (Vector3){ ORBITA_RAIO, 0.0f, 0.0f }, IA_ATACAR_DESTROIER);
    float antes;

    iniciarJogador(&g.jogador);
    g.cascoDestroier = 2000.0f;

    antes = Vector3Length(longe.pos);
    ia_atualizar(&longe, &g, DT);
    VERIFICA(g.cascoDestroier == 2000.0f);
    VERIFICA(Vector3Length(longe.pos) < antes);

    simular(&longe, &g, 600);
    VERIFICA(Vector3Length(longe.pos) > ORBITA_RAIO - 185.0f * ESCALA_DESTROIER);
    VERIFICA(Vector3Length(longe.pos) < BOMBA_ALCANCE + 45.0f * ESCALA_DESTROIER);
    VERIFICA(g.cascoDestroier < 2000.0f);

    simular(&orbita, &g, 600);
    /* fica no anel, no plano z = 0, e continua bombardeando */
    VERIFICA(Vector3Length(orbita.pos) > ORBITA_RAIO - 185.0f * ESCALA_DESTROIER);
    VERIFICA(Vector3Length(orbita.pos) < BOMBA_ALCANCE + 45.0f * ESCALA_DESTROIER);
    VERIFICA(fabsf(orbita.pos.z) < 50.0f);
    VERIFICA(fabsf(Vector3DotProduct(Vector3Normalize(orbita.pos), orbita.vel)) < 45.0f);
    VERIFICA(Vector3Length(orbita.vel) > 90.0f);
    VERIFICA(g.cascoDestroier < 2000.0f - BOMBA_DANO);
}

/* IA-04: um caça dentro do casco é empurrado para fora já no primeiro passo. */
static void testarCascoEmpurraParaFora(void)
{
    Jogo g = { 0 };
    Inimigo proa = novoInimigo(XWING, (Vector3){ 0.0f, 0.0f, 810.0f }, IA_ATACAR_DESTROIER);
    Inimigo centro = novoInimigo(AWING, (Vector3){ 0.0f, 0.0f, 0.0f }, IA_ATACAR_DESTROIER);

    iniciarJogador(&g.jogador);

    ia_atualizar(&proa, &g, DT);
    VERIFICA(!tocaDestroier(proa.pos, raioInimigo(XWING)));
    VERIFICA(Vector3Length(proa.pos) > 810.0f - 1.0f);   /* empurrado para fora, não para dentro */

    ia_atualizar(&centro, &g, DT);
    VERIFICA(!tocaDestroier(centro.pos, raioInimigo(AWING)));
    VERIFICA(Vector3Length(centro.pos) > 0.0f);
    VERIFICA(isfinite(centro.pos.x) && isfinite(centro.pos.y) && isfinite(centro.pos.z));
}

/*
 * IA-04: nenhum tipo atravessa o casco, por mais passos que voe contra ele.
 * Cobre aproximação no eixo, lateral, diagonal e o caso que travava: dentro do casco no eixo.
 */
static void testarNaoAtravessaOCasco(void)
{
    const TipoNave tipos[] = { XWING, YWING, AWING };
    const Vector3 partidas[] = {
        { 0.0f, 0.0f, 4600.0f },
        { 1500.0f, 0.0f, 0.0f },
        { 900.0f, 400.0f, -1500.0f },
        { 0.0f, 0.0f, 0.0f },
        { 0.0f, 0.0f, -90.0f },
        { 0.0f, 0.0f, 90.0f },
    };
    int t, p;

    for (t = 0; t < (int)(sizeof tipos / sizeof tipos[0]); t++) {
        for (p = 0; p < (int)(sizeof partidas / sizeof partidas[0]); p++) {
            Jogo g = { 0 };
            Inimigo in = novoInimigo(tipos[t], partidas[p], IA_ATACAR_DESTROIER);
            int i, dentro = 0;

            iniciarJogador(&g.jogador);

            for (i = 0; i < 1800; i++) {
                ia_atualizar(&in, &g, DT);
                dentro |= tocaDestroier(in.pos, raioInimigo(in.tipo));
            }

            VERIFICA(dentro == 0);
            VERIFICA(isfinite(in.pos.x) && isfinite(in.pos.y) && isfinite(in.pos.z));
        }
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
    VERIFICA(QUASE(fatorVelocidadeIA(2.0f * DURACAO_PARTIDA), FATOR_VELOCIDADE_MAX));

    VERIFICA(QUASE(fatorCooldownIA(0.0f), 1.0f));
    VERIFICA(QUASE(fatorCooldownIA(meio), 0.85f));
    VERIFICA(QUASE(fatorCooldownIA(DURACAO_PARTIDA), FATOR_COOLDOWN_MIN));
    VERIFICA(QUASE(fatorCooldownIA(2.0f * DURACAO_PARTIDA), FATOR_COOLDOWN_MIN));

    /* o cooldown encolhe com o tempo; a velocidade cresce */
    VERIFICA(fatorCooldownIA(DURACAO_PARTIDA) < fatorCooldownIA(0.0f));
    VERIFICA(fatorVelocidadeIA(DURACAO_PARTIDA) > fatorVelocidadeIA(0.0f));
}

/* IA-05: no fim da partida o caça chega a FATOR_VELOCIDADE_MAX vezes a velocidade do tipo. */
static void testarVelocidadeCresceComOTempo(void)
{
    Jogo g = { 0 };
    Inimigo in = novoInimigo(XWING, (Vector3){ 0.0f, 0.0f, 8200.0f }, IA_ATACAR_DESTROIER);
    float velMax = VEL_XWING * FATOR_VELOCIDADE_MAX;

    iniciarJogador(&g.jogador);
    g.tempoDecorrido = DURACAO_PARTIDA;

    simular(&in, &g, 300);
    VERIFICA(Vector3Length(in.vel) <= velMax + 1e-3f);
    VERIFICA(Vector3Length(in.vel) > 0.99f * velMax);
}

/* IA-05: a aceleração escala com o quadrado do fator, então o raio de curva não abre. */
static void testarAceleracaoAcompanhaFator(void)
{
    Jogo g = { 0 };
    Inimigo inicio;
    Inimigo fim;

    iniciarJogador(&g.jogador);

    g.tempoDecorrido = 0.0f;
    inicio = novoInimigo(AWING, (Vector3){ 0.0f, 0.0f, 8200.0f }, IA_ATACAR_DESTROIER);
    ia_atualizar(&inicio, &g, DT);

    g.tempoDecorrido = DURACAO_PARTIDA;
    fim = novoInimigo(AWING, (Vector3){ 0.0f, 0.0f, 8200.0f }, IA_ATACAR_DESTROIER);
    ia_atualizar(&fim, &g, DT);

    VERIFICA(Vector3Length(inicio.vel) > 0.0f);
    VERIFICA(fabsf(Vector3Length(fim.vel) / Vector3Length(inicio.vel) - 1.69f) < 0.05f);
}

/* Zera o jogo e põe o jogador na posição padrão de nascimento (fora do casco). */
static void iniciarJogoVazio(Jogo *g)
{
    memset(g, 0, sizeof *g);
    iniciarJogador(&g->jogador);
}

static int lasersAtivos(const Jogo *g)
{
    int i, n = 0;

    for (i = 0; i < MAX_LASERS; i++)
        n += g->lasers[i].ativo != 0;
    return n;
}

/*
 * IA-03: o X-Wing troca de alvo com período de 8–12 s. A fase vem do relógio da partida
 * (a struct Inimigo é congelada e não tem tempo de spawn), então a primeira troca de quem
 * nasce no meio do ciclo pode ser menor; da segunda em diante o intervalo fica em 8–12 s.
 * O caça fica parado longe dos dois alvos: perto deles a passagem de ataque o faria fugir.
 */
static void testarAlternanciaXWing(void)
{
    const int ids[] = { 1, 6 };   /* 1: primeira troca em 8 s; 6: em 3 s */
    int k;

    for (k = 0; k < (int)(sizeof ids / sizeof ids[0]); k++) {
        Jogo g;
        Inimigo in;
        EstadoIA estado;
        float ultimaTroca = 0.0f;
        int trocas = 0;
        int i, primeira = 1;

        iniciarJogoVazio(&g);
        in = novoInimigo(XWING, (Vector3){ 0.0f, 0.0f, 6000.0f }, IA_ATACAR_DESTROIER);
        in.id = ids[k];
        estado = in.estadoIA;

        for (i = 1; i <= 60 * 60; i++) {
            g.tempoDecorrido = i * DT;
            in.pos = (Vector3){ 0.0f, 0.0f, 6000.0f };
            in.vel = (Vector3){ 0.0f, 0.0f, 0.0f };
            ia_atualizar(&in, &g, DT);
            if (in.estadoIA != estado) {
                float intervalo = g.tempoDecorrido - ultimaTroca;

                if (primeira) {
                    VERIFICA(intervalo > 0.0f && intervalo <= TROCA_XWING_MIN + TROCA_XWING_VARIACAO + DT);
                    primeira = 0;
                } else {
                    VERIFICA(intervalo >= TROCA_XWING_MIN - DT);
                    VERIFICA(intervalo <= TROCA_XWING_MIN + TROCA_XWING_VARIACAO - 1 + DT);
                }
                ultimaTroca = g.tempoDecorrido;
                estado = in.estadoIA;
                trocas++;
            }
        }
        VERIFICA(trocas >= 4);
    }
}

/* IA-03: X-Wing e A-Wing atiram no cone de 15° e respeitam o cooldown. */
static void testarTiroNoCone(void)
{
    Jogo g;
    Inimigo in;

    /* A-Wing dentro do cone e do alcance: um laser inimigo e cooldown recarregado. */
    iniciarJogoVazio(&g);
    in = novoInimigo(AWING, Vector3Add(g.jogador.pos, (Vector3){ 0.0f, 0.0f, 1130.0f }), IA_ATACAR_JOGADOR);
    in.vel = (Vector3){ 0.0f, 0.0f, -VEL_AWING };
    ia_atualizar(&in, &g, DT);
    VERIFICA(lasersAtivos(&g) == 1);
    VERIFICA(g.lasers[0].doJogador == 0);
    VERIFICA(QUASE(g.lasers[0].dano, DANO_LASER_INIMIGO));
    VERIFICA(QUASE(in.cooldownTiro, COOLDOWN_TIRO_INIMIGO));

    /* Ainda em cooldown: não dispara de novo. */
    ia_atualizar(&in, &g, DT);
    VERIFICA(lasersAtivos(&g) == 1);

    /* Passado o cooldown, dispara outra vez (no mesmo ponto, longe de romper a passagem). */
    for (int i = 0; i < (int)(COOLDOWN_TIRO_INIMIGO / DT) + 1; i++) {
        in.pos = Vector3Add(g.jogador.pos, (Vector3){ 0.0f, 0.0f, 1130.0f });
        in.vel = (Vector3){ 0.0f, 0.0f, -VEL_AWING };
        ia_atualizar(&in, &g, DT);
    }
    VERIFICA(lasersAtivos(&g) == 2);

    /* Fora do cone (voando para longe do alvo): não dispara. */
    iniciarJogoVazio(&g);
    in = novoInimigo(AWING, Vector3Add(g.jogador.pos, (Vector3){ 0.0f, 0.0f, 1130.0f }), IA_ATACAR_JOGADOR);
    in.vel = (Vector3){ 0.0f, 0.0f, VEL_AWING };
    ia_atualizar(&in, &g, DT);
    VERIFICA(lasersAtivos(&g) == 0);

    /* Fora do alcance: não dispara. */
    iniciarJogoVazio(&g);
    in = novoInimigo(AWING, Vector3Add(g.jogador.pos, (Vector3){ 0.0f, 0.0f, 2000.0f }), IA_ATACAR_JOGADOR);
    in.vel = (Vector3){ 0.0f, 0.0f, -VEL_AWING };
    ia_atualizar(&in, &g, DT);
    VERIFICA(lasersAtivos(&g) == 0);

    /* Y-Wing bombardeia, não atira. */
    iniciarJogoVazio(&g);
    in = novoInimigo(YWING, (Vector3){ 0.0f, 0.0f, 1500.0f }, IA_ATACAR_DESTROIER);
    in.vel = (Vector3){ 0.0f, 0.0f, -VEL_YWING };
    ia_atualizar(&in, &g, DT);
    VERIFICA(lasersAtivos(&g) == 0);

    /* X-Wing no estado de atacar o jogador também atira (id 0, t = 9 -> jogador). */
    iniciarJogoVazio(&g);
    g.tempoDecorrido = 9.0f;
    in = novoInimigo(XWING, Vector3Add(g.jogador.pos, (Vector3){ 0.0f, 0.0f, 1130.0f }), IA_ATACAR_DESTROIER);
    in.id = 0;
    in.vel = (Vector3){ 0.0f, 0.0f, -VEL_XWING };
    ia_atualizar(&in, &g, DT);
    VERIFICA(in.estadoIA == IA_ATACAR_JOGADOR);
    VERIFICA(lasersAtivos(&g) == 1);

    /* No fim da partida o cooldown recarregado usa o fator da IA-05. */
    iniciarJogoVazio(&g);
    g.tempoDecorrido = DURACAO_PARTIDA;
    in = novoInimigo(AWING, Vector3Add(g.jogador.pos, (Vector3){ 0.0f, 0.0f, 1130.0f }), IA_ATACAR_JOGADOR);
    in.vel = (Vector3){ 0.0f, 0.0f, -VEL_AWING };
    ia_atualizar(&in, &g, DT);
    VERIFICA(lasersAtivos(&g) == 1);
    VERIFICA(QUASE(in.cooldownTiro, COOLDOWN_TIRO_INIMIGO * FATOR_COOLDOWN_MIN));
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

/*
 * Passagem de ataque no Destróier: o X-Wing atira de dentro do alcance e rompe a
 * DIST_ROMPER_DESTROIER da origem, sem encostar nem parar no casco.
 * id 1: os primeiros 8 s são de atacar o Destróier (IA-03).
 */
static void testarPassagemNoDestroier(void)
{
    Jogo g;
    Inimigo in;
    float menor = 1e9f;
    int fugiu = 0, tocou = 0;

    iniciarJogoVazio(&g);
    g.jogador.pos = (Vector3){ 0.0f, 0.0f, -8000.0f };
    in = novoInimigo(XWING, (Vector3){ 0.0f, 0.0f, 5000.0f }, IA_ATACAR_DESTROIER);

    for (int i = 1; i < 8 * 60; i++) {
        g.tempoDecorrido = i * DT;
        ia_atualizar(&in, &g, DT);
        menor = fminf(menor, Vector3Length(in.pos));
        fugiu |= in.estadoIA == IA_FUGIR;
        tocou |= tocaDestroier(in.pos, raioInimigo(XWING));
    }

    VERIFICA(fugiu);
    VERIFICA(!tocou);
    VERIFICA(menor > DIST_ROMPER_DESTROIER - 100.0f);
    VERIFICA(Vector3Length(in.vel) > 0.5f * VEL_XWING);
    VERIFICA(lasersAtivos(&g) > 0);
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
    testarAproximaDoDestroier();
    testarVelocidadeMaximaPorTipo();
    testarPersegueJogador();
    testarFogeDoJogador();
    testarAlvoNaMesmaPosicao();
    testarAtualizarInimigosMove();
    testarYwingBombardeia();
    testarYwingForaDeAlcance();
    testarCascoEmpurraParaFora();
    testarNaoAtravessaOCasco();
    testarFatoresDeDificuldade();
    testarVelocidadeCresceComOTempo();
    testarAceleracaoAcompanhaFator();
    testarAlternanciaXWing();
    testarTiroNoCone();
    testarPassagemNoJogador();
    testarPassagemNoDestroier();
    testarSeparacao();

    return RESULTADO_TESTES();
}
