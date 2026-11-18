#include <math.h>

#include "inimigos.h"
#include "jogo.h"
#include "nave.h"
#include "raymath.h"
#include "reforcos.h"
#include "teste.h"

/* Fila vazia: desenfileirar devolve 0 e não toca em `saida`. */
static void testarVazia(void)
{
    FilaReforcos f = { 0 };
    NoReforco saida = { -1.0f, -1, -2, -3, NULL };

    VERIFICA(f.inicio == NULL && f.fim == NULL && f.tamanho == 0);
    VERIFICA(desenfileirarReforco(&f, &saida) == 0);
    VERIFICA(saida.tempoChegada == -1.0f && saida.qtdX == -1 && saida.qtdY == -2 && saida.qtdA == -3);
    VERIFICA(saida.prox == NULL);
    VERIFICA(f.inicio == NULL && f.fim == NULL && f.tamanho == 0);

    liberarFila(&f);
    VERIFICA(f.inicio == NULL && f.fim == NULL && f.tamanho == 0);
}

/* Um elemento: é início e fim ao mesmo tempo; sai com os dados copiados e o nó liberado. */
static void testarUmElemento(void)
{
    FilaReforcos f = { 0 };
    NoReforco saida;

    enfileirarReforco(&f, 3.0f, 2, 0, 0);
    VERIFICA(f.tamanho == 1);
    VERIFICA(f.inicio != NULL && f.inicio == f.fim);
    VERIFICA(f.inicio->tempoChegada == 3.0f && f.inicio->qtdX == 2);

    VERIFICA(desenfileirarReforco(&f, &saida) == 1);
    VERIFICA(saida.tempoChegada == 3.0f && saida.qtdX == 2 && saida.qtdY == 0 && saida.qtdA == 0);
    VERIFICA(saida.prox == NULL);
    VERIFICA(f.tamanho == 0 && f.inicio == NULL && f.fim == NULL);

    /* A fila ficou vazia de verdade: a segunda remoção não mexe em `saida`. */
    saida.qtdX = 99;
    VERIFICA(desenfileirarReforco(&f, &saida) == 0);
    VERIFICA(saida.qtdX == 99);

    liberarFila(&f);
}

/* Ordem FIFO preservada; `fim` acompanha o último inserido mesmo depois de esvaziar. */
static void testarOrdemFifo(void)
{
    const float tempos[3] = { 3.0f, 10.0f, 23.0f };
    const int qtdX[3] = { 2, 3, 4 };
    const int qtdY[3] = { 0, 1, 1 };
    const int qtdA[3] = { 0, 0, 1 };
    FilaReforcos f = { 0 };
    NoReforco saida;
    int i;

    for (i = 0; i < 3; i++)
        enfileirarReforco(&f, tempos[i], qtdX[i], qtdY[i], qtdA[i]);

    VERIFICA(f.tamanho == 3);
    VERIFICA(f.inicio->tempoChegada == 3.0f);
    VERIFICA(f.fim->tempoChegada == 23.0f && f.fim->qtdX == 4);

    for (i = 0; i < 3; i++) {
        VERIFICA(desenfileirarReforco(&f, &saida) == 1);
        VERIFICA(saida.tempoChegada == tempos[i]);
        VERIFICA(saida.qtdX == qtdX[i] && saida.qtdY == qtdY[i] && saida.qtdA == qtdA[i]);
        VERIFICA(saida.prox == NULL);
        VERIFICA(f.tamanho == 2 - i);
    }

    VERIFICA(f.inicio == NULL && f.fim == NULL && f.tamanho == 0);

    /* Enfileirar de novo numa fila esvaziada religa início e fim no mesmo nó. */
    enfileirarReforco(&f, 41.0f, 5, 2, 2);
    VERIFICA(f.tamanho == 1 && f.inicio == f.fim);
    VERIFICA(desenfileirarReforco(&f, &saida) == 1 && saida.tempoChegada == 41.0f);

    liberarFila(&f);
}

/* liberarFila libera o que sobrou (o valgrind confirma) e é seguro repetir. */
static void testarLiberar(void)
{
    FilaReforcos f = { 0 };
    NoReforco saida;

    enfileirarReforco(&f, 3.0f, 2, 0, 0);
    enfileirarReforco(&f, 10.0f, 3, 1, 0);
    enfileirarReforco(&f, 23.0f, 4, 1, 1);

    VERIFICA(desenfileirarReforco(&f, &saida) == 1);
    VERIFICA(f.tamanho == 2);

    liberarFila(&f);
    VERIFICA(f.inicio == NULL && f.fim == NULL && f.tamanho == 0);
    VERIFICA(desenfileirarReforco(&f, &saida) == 0);

    liberarFila(&f);
    VERIFICA(f.inicio == NULL && f.fim == NULL && f.tamanho == 0);
}

/* gerarReforcos: onda a cada 15 s, todos os tipos em 3×. */
static void testarGerarReforcos(void)
{
    FilaReforcos f = { 0 };
    NoReforco grupo;
    float t = 3.0f;
    int onda = 0, qtdAAnterior = 0, qtdYAnterior = 0, viuA = 0;

    gerarReforcos(&f, DURACAO_PARTIDA);
    VERIFICA(f.tamanho == 20);

    while (desenfileirarReforco(&f, &grupo) == 1) {
        int totalBase = 2 + (int)(grupo.tempoChegada / 40.0f);
        int qtdYBase = onda == 0 ? 0 : (totalBase >= 3 ? 2 : 1);
        int qtdABase = (int)(totalBase * (grupo.tempoChegada / DURACAO_PARTIDA));
        int x, y, a;

        if (qtdABase > totalBase - 1 - qtdYBase)
            qtdABase = totalBase - 1 - qtdYBase;
        x = (totalBase - qtdYBase - qtdABase) * 3;
        y = qtdYBase * 3;
        a = qtdABase * 3;
        if (onda >= 11) {
            x += (a + 1) / 2;
            y += a / 2;
            a = 0;
        }

        VERIFICA(grupo.tempoChegada == t);
        VERIFICA(grupo.qtdX == x);
        VERIFICA(grupo.qtdY == y);
        VERIFICA(grupo.qtdA == a);
        VERIFICA(grupo.qtdX >= 3);
        viuA |= grupo.qtdA > 0;

        if (onda == 0)
            VERIFICA(grupo.qtdY == 0);
        else
            VERIFICA(grupo.qtdY >= 3);
        if (onda > 0 && onda < 11 && totalBase >= 3)
            VERIFICA(grupo.qtdY == 6);

        if (onda > 0) {
            if (onda < 11)
                VERIFICA(grupo.qtdA >= qtdAAnterior);
            else
                VERIFICA(grupo.qtdA == 0);
            VERIFICA(grupo.qtdY >= qtdYAnterior);
        }

        qtdAAnterior = grupo.qtdA;
        qtdYAnterior = grupo.qtdY;
        t += 15.0f;
        onda++;
    }

    VERIFICA(onda == 20);
    VERIFICA(viuA);
    VERIFICA(f.tamanho == 0 && f.inicio == NULL && f.fim == NULL);

    /* Partida que acaba antes da primeira onda não gera nada. */
    gerarReforcos(&f, 2.9f);
    VERIFICA(f.tamanho == 0);
    gerarReforcos(&f, 3.0f);
    VERIFICA(f.tamanho == 0);

    /* Com 13 s cabe só a onda de t = 3. */
    gerarReforcos(&f, 13.0f);
    VERIFICA(f.tamanho == 1);
    VERIFICA(desenfileirarReforco(&f, &grupo) == 1 && grupo.tempoChegada == 3.0f);

    liberarFila(&f);
    VERIFICA(f.tamanho == 0);
}

static int contarTipo(const ListaInimigos *l, TipoNave tipo)
{
    int n = 0;

    for (const NoInimigo *no = l->inicio; no != NULL; no = no->prox)
        if (no->dado.tipo == tipo)
            n++;
    return n;
}

static float comprimento(Vector3 v)
{
    return sqrtf(v.x * v.x + v.y * v.y + v.z * v.z);
}

static float distancia(Vector3 a, Vector3 b)
{
    return comprimento((Vector3){ a.x - b.x, a.y - b.y, a.z - b.z });
}

/*
 * REF-04: processarReforcos desenfileira o que já venceu e faz o grupo nascer junto,
 * na borda (~8200) e em formação. A distância ao jogador já sai preenchida.
 */
static void testarProcessarReforcos(void)
{
    Jogo jogo = { 0 };
    const Vector3 *primeiro;

    iniciarJogador(&jogo.jogador);
    jogo.proximoId = 1;

    /* Fila vazia: nada acontece. */
    processarReforcos(&jogo);
    VERIFICA(jogo.inimigos.tamanho == 0);

    /* Antes do tempo do grupo, ele continua na fila. */
    enfileirarReforco(&jogo.reforcos, 3.0f, 2, 1, 1);
    jogo.tempoDecorrido = 2.999f;
    processarReforcos(&jogo);
    VERIFICA(jogo.reforcos.tamanho == 1);
    VERIFICA(jogo.inimigos.tamanho == 0);

    /* No tempo, o grupo inteiro sai da fila e nasce de uma vez. */
    jogo.tempoDecorrido = 3.0f;
    processarReforcos(&jogo);
    VERIFICA(jogo.reforcos.tamanho == 0);
    VERIFICA(jogo.inimigos.tamanho == 4);
    VERIFICA(contarTipo(&jogo.inimigos, XWING) == 2);
    VERIFICA(contarTipo(&jogo.inimigos, YWING) == 1);
    VERIFICA(contarTipo(&jogo.inimigos, AWING) == 1);

    /* ids únicos, na ordem de inserção */
    {
        int esperado = 1;

        for (const NoInimigo *no = jogo.inimigos.inicio; no != NULL; no = no->prox, esperado++)
            VERIFICA(no->dado.id == esperado);
    }

    /* todos na borda, perto uns dos outros (formação) e com a distância ao jogador certa */
    primeiro = &jogo.inimigos.inicio->dado.pos;
    VERIFICA(comprimento(*primeiro) > 8100.0f && comprimento(*primeiro) < 8300.0f);
    for (const NoInimigo *no = jogo.inimigos.inicio; no != NULL; no = no->prox) {
        VERIFICA(comprimento(no->dado.pos) > 8100.0f && comprimento(no->dado.pos) < 8300.0f);
        VERIFICA(distancia(no->dado.pos, *primeiro) <= 150.0f);
        VERIFICA(fabsf(no->dado.distancia - distancia(no->dado.pos, jogo.jogador.pos)) < 0.001f);
    }
    VERIFICA(distancia(jogo.inimigos.inicio->dado.pos, jogo.inimigos.inicio->prox->dado.pos) > 1.0f);

    liberarListaInimigos(&jogo.inimigos);

    /* Dois grupos vencidos no mesmo passo saem na ordem da fila, cada um com seus caças. */
    jogo.proximoId = 1;
    enfileirarReforco(&jogo.reforcos, 3.0f, 1, 0, 0);
    enfileirarReforco(&jogo.reforcos, 3.0f, 2, 0, 0);
    jogo.tempoDecorrido = 3.0f;
    processarReforcos(&jogo);
    VERIFICA(jogo.reforcos.tamanho == 0);
    VERIFICA(jogo.inimigos.tamanho == 3);
    VERIFICA(jogo.inimigos.inicio->dado.id == 1 && jogo.inimigos.inicio->dado.tipo == XWING);
    VERIFICA(jogo.inimigos.fim->dado.id == 3 && jogo.inimigos.fim->dado.tipo == XWING);
    liberarListaInimigos(&jogo.inimigos);

    /* Grupo que ainda não venceu não é tocado. */
    enfileirarReforco(&jogo.reforcos, 10.0f, 1, 0, 0);
    jogo.tempoDecorrido = 9.9f;
    processarReforcos(&jogo);
    VERIFICA(jogo.inimigos.tamanho == 0 && jogo.reforcos.tamanho == 1);

    liberarFila(&jogo.reforcos);
    liberarListaInimigos(&jogo.inimigos);
    VERIFICA(jogo.reforcos.tamanho == 0 && jogo.inimigos.tamanho == 0);
}

/*
 * O grupo nasce longe do jogador mesmo com ele na borda da arena, onde a esfera de spawn
 * passa a menos de 1000 dele: nenhum caça a menos de 3000 em 200 grupos seguidos.
 */
static void testarSpawnLongeDoJogador(void)
{
    Jogo jogo = { 0 };
    const Vector3 bordas[] = { { 0.0f, 0.0f, 9000.0f }, { 9000.0f, 0.0f, 0.0f }, { 0.0f, -9000.0f, 0.0f } };
    float menor = 1e9f;

    iniciarJogador(&jogo.jogador);
    jogo.proximoId = 1;
    for (int i = 0; i < 200; i++) {
        jogo.jogador.pos = bordas[i % 3];
        enfileirarReforco(&jogo.reforcos, 0.0f, 2, 1, 1);
        processarReforcos(&jogo);
        for (const NoInimigo *no = jogo.inimigos.inicio; no != NULL; no = no->prox)
            menor = fminf(menor, distancia(no->dado.pos, jogo.jogador.pos));
        liberarListaInimigos(&jogo.inimigos);
    }
    VERIFICA(menor > 3000.0f - 50.0f);   /* a formação abre até ~30 u do centro */
}

int main(void)
{
    testarVazia();
    testarUmElemento();
    testarOrdemFifo();
    testarLiberar();
    testarGerarReforcos();
    testarProcessarReforcos();
    testarSpawnLongeDoJogador();

    return RESULTADO_TESTES();
}
