#include <math.h>
#include <stdlib.h>

#include "inimigos.h"
#include "raymath.h"
#include "reforcos.h"

#define PRIMEIRA_ONDA    3.0f
#define INTERVALO_ONDAS  15.0f

/* Multiplicadores por tipo: todos os caças rebeldes em 3×. */
#define MULT_XWING 3
#define MULT_YWING 3
#define MULT_AWING 3

/* Os reforços nascem numa esfera de raio 8200, dentro da arena (RAIO_ARENA = 9000). */
#define RAIO_REFORCO     8200.0f
#define ESPACO_FORMACAO  45.0f
#define COLUNAS_FORMACAO 4

/* O grupo nasce longe do jogador: fora do alcance de tiro (1840) com folga para ele reagir. */
#define DIST_MIN_JOGADOR 3000.0f
#define TENTATIVAS_CENTRO 16

static unsigned int semente = 0x5EEDu;

/* LCG próprio, como o do céu: não mexe no rand() nem no RNG da raylib. Devolve [0, 1). */
static float aleatorio(void)
{
    semente = semente * 1664525u + 1013904223u;
    return (float)(semente >> 8) * (1.0f / 16777216.0f);
}

void enfileirarReforco(FilaReforcos *f, float t, int x, int y, int a)
{
    NoReforco *no = malloc(sizeof *no);
    if (no == NULL)
        return;
    no->tempoChegada = t;
    no->qtdX = x;
    no->qtdY = y;
    no->qtdA = a;
    no->prox = NULL;
    if (f->fim == NULL)
        f->inicio = no;
    else
        f->fim->prox = no;
    f->fim = no;
    f->tamanho++;
}

/*
 * Remove o grupo do início (O(1)) e copia tempoChegada e quantidades para `saida`.
 * Devolve 1 se desenfileirou e 0 se a fila estava vazia; em fila vazia `saida` não é tocada.
 * O `prox` de `saida` é sempre NULL: os nós pertencem só à fila.
 */
int desenfileirarReforco(FilaReforcos *f, NoReforco *saida)
{
    NoReforco *no = f->inicio;

    if (no == NULL)
        return 0;

    *saida = *no;
    saida->prox = NULL;
    f->inicio = no->prox;
    if (f->inicio == NULL)
        f->fim = NULL;
    f->tamanho--;
    free(no);
    return 1;
}

/*
 * Seção 2.2: primeira onda em 3 s, depois uma a cada 20 s, com tamanho 2 + t/40.
 * A proporção de A-Wings cresce com o tempo. Após a primeira onda, Y-Wings vêm em pares
 * assim que há espaço para dois bombardeiros e um X-Wing; a segunda onda ainda tem um só.
 * Os clamps garantem pelo menos 1 X-Wing por onda e que X + Y + A seja sempre o tamanho da onda.
 */
void gerarReforcos(FilaReforcos *f, float duracao)
{
    int onda;
    float t;

    for (onda = 0, t = PRIMEIRA_ONDA; t < duracao; onda++, t += INTERVALO_ONDAS) {
        int totalBase = 2 + (int)(t / 40.0f);
        int qtdABase = (int)(totalBase * (t / duracao));
        int qtdYBase = onda == 0 ? 0 : (totalBase >= 3 ? 2 : 1);
        int x, y, a;

        if (qtdABase > totalBase - 1 - qtdYBase)
            qtdABase = totalBase - 1 - qtdYBase;
        x = (totalBase - qtdYBase - qtdABase) * MULT_XWING;
        y = qtdYBase * MULT_YWING;
        a = qtdABase * MULT_AWING;

        /* Da 12ª onda em diante: a cota de A-Wings é dividida entre X e Y, depois
         * saem 1 X-Wing e 2 Y-Wings, e ficam fixos 2 A-Wings por onda. */
        if (onda >= 11) {
            x += (a + 1) / 2;
            y += a / 2;
            x -= 1;
            y -= 2;
            a = 2;
        }
        enfileirarReforco(f, t, x, y, a);
    }
}

/* Direção uniforme na esfera unitária. */
static Vector3 sortearDirecao(void)
{
    float z = 2.0f * aleatorio() - 1.0f;
    float ang = 2.0f * PI * aleatorio();
    float r = sqrtf(1.0f - z * z);

    return (Vector3){ r * cosf(ang), z, r * sinf(ang) };
}

/*
 * Ponto uniforme na esfera de raio RAIO_REFORCO a pelo menos DIST_MIN_JOGADOR do jogador e os
 * eixos da formação (perpendiculares ao raio). Se nenhuma tentativa servir, fica a mais distante.
 */
static Vector3 sortearCentro(Vector3 jogador, Vector3 *direita, Vector3 *cima)
{
    Vector3 raio = sortearDirecao();
    float melhor = Vector3Distance(Vector3Scale(raio, RAIO_REFORCO), jogador);
    Vector3 apoio;

    for (int i = 1; i < TENTATIVAS_CENTRO && melhor < DIST_MIN_JOGADOR; i++) {
        Vector3 outro = sortearDirecao();
        float dist = Vector3Distance(Vector3Scale(outro, RAIO_REFORCO), jogador);

        if (dist > melhor) {
            melhor = dist;
            raio = outro;
        }
    }

    apoio = fabsf(raio.y) > 0.9f ? (Vector3){ 0.0f, 0.0f, 1.0f } : (Vector3){ 0.0f, 1.0f, 0.0f };
    *direita = Vector3Normalize(Vector3CrossProduct(raio, apoio));
    *cima = Vector3CrossProduct(*direita, raio);
    return Vector3Scale(raio, RAIO_REFORCO);
}

static TipoNave tipoDoIndice(const NoReforco *grupo, int i)
{
    if (i < grupo->qtdX)
        return XWING;
    if (i < grupo->qtdX + grupo->qtdY)
        return YWING;
    return AWING;
}

/* Um grupo nasce junto: centro sorteado na borda e formação em grade centrada. */
static void criarGrupo(Jogo *g, const NoReforco *grupo)
{
    int total = grupo->qtdX + grupo->qtdY + grupo->qtdA;
    int colunas, linhas;
    Vector3 centro, direita, cima;
    int i;
    int grupoBombardeio = g->proximoId;

    if (total <= 0)
        return;

    colunas = total < COLUNAS_FORMACAO ? total : COLUNAS_FORMACAO;
    linhas = (total + colunas - 1) / colunas;
    centro = sortearCentro(g->jogador.pos, &direita, &cima);

    for (i = 0; i < total; i++) {
        int coluna = i % colunas;
        int linha = i / colunas;
        int naLinha = linha == linhas - 1 ? total - linha * colunas : colunas;
        Vector3 desloc = Vector3Add(
            Vector3Scale(direita, (coluna - (naLinha - 1) * 0.5f) * ESPACO_FORMACAO),
            Vector3Scale(cima, (linha - (linhas - 1) * 0.5f) * ESPACO_FORMACAO));
        Inimigo in = criarInimigo(tipoDoIndice(grupo, i), Vector3Add(centro, desloc), g->proximoId++);

        in.distancia = Vector3Distance(in.pos, g->jogador.pos);
        in.grupoBombardeio = grupoBombardeio;
        inserirInimigo(&g->inimigos, in);
    }
}

void processarReforcos(Jogo *g)
{
    NoReforco grupo;

    while (g->reforcos.inicio != NULL && g->reforcos.inicio->tempoChegada <= g->tempoDecorrido) {
        if (!desenfileirarReforco(&g->reforcos, &grupo))
            break;
        criarGrupo(g, &grupo);
    }
}

void liberarFila(FilaReforcos *f)
{
    NoReforco *no = f->inicio;
    while (no != NULL) {
        NoReforco *prox = no->prox;
        free(no);
        no = prox;
    }
    f->inicio = f->fim = NULL;
    f->tamanho = 0;
}
