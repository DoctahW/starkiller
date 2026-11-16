#include <math.h>
#include <string.h>

#include "inimigos.h"
#include "teste.h"

#define EPS 1e-5f

static Jogo jogo;

static int proximo(float a, float b)
{
    return fabsf(a - b) < EPS;
}

static int vetorNulo(Vector3 v)
{
    return proximo(v.x, 0.0f) && proximo(v.y, 0.0f) && proximo(v.z, 0.0f);
}

static void testarXWing(void)
{
    Inimigo in = criarInimigo(XWING, (Vector3){ 10.0f, 20.0f, -30.0f }, 7);

    VERIFICA(in.id == 7);
    VERIFICA(in.tipo == XWING);
    VERIFICA(proximo(in.pos.x, 10.0f) && proximo(in.pos.y, 20.0f) && proximo(in.pos.z, -30.0f));

    VERIFICA(proximo(in.escudoMax, 50.0f) && proximo(in.cascoMax, 100.0f));
    VERIFICA(proximo(in.escudo, in.escudoMax) && proximo(in.casco, in.cascoMax));

    VERIFICA(vetorNulo(in.vel));
    VERIFICA(proximo(in.distancia, 0.0f));
    VERIFICA(proximo(in.cooldownTiro, 0.0f));
    VERIFICA(in.estadoIA == IA_ATACAR_DESTROIER);
    VERIFICA(proximo(VEL_XWING, 60.0f));
}

static void testarYWing(void)
{
    Inimigo in = criarInimigo(YWING, (Vector3){ 0.0f, 0.0f, 1800.0f }, 8);

    VERIFICA(in.id == 8);
    VERIFICA(in.tipo == YWING);
    VERIFICA(proximo(in.pos.z, 1800.0f));

    VERIFICA(proximo(in.escudoMax, 80.0f) && proximo(in.cascoMax, 160.0f));
    VERIFICA(proximo(in.escudo, in.escudoMax) && proximo(in.casco, in.cascoMax));

    VERIFICA(vetorNulo(in.vel));
    VERIFICA(proximo(in.distancia, 0.0f));
    VERIFICA(proximo(in.cooldownTiro, 0.0f));
    VERIFICA(in.estadoIA == IA_ATACAR_DESTROIER);
    VERIFICA(proximo(VEL_YWING, 35.0f));
}

static void testarAWing(void)
{
    Inimigo in = criarInimigo(AWING, (Vector3){ -1800.0f, 0.0f, 0.0f }, 9);

    VERIFICA(in.id == 9);
    VERIFICA(in.tipo == AWING);
    VERIFICA(proximo(in.pos.x, -1800.0f));

    VERIFICA(proximo(in.escudoMax, 30.0f) && proximo(in.cascoMax, 60.0f));
    VERIFICA(proximo(in.escudo, in.escudoMax) && proximo(in.casco, in.cascoMax));

    VERIFICA(vetorNulo(in.vel));
    VERIFICA(proximo(in.distancia, 0.0f));
    VERIFICA(proximo(in.cooldownTiro, 0.0f));
    VERIFICA(in.estadoIA == IA_ATACAR_JOGADOR);
    VERIFICA(proximo(VEL_AWING, 110.0f));
}

static void testarIdsDoJogo(void)
{
    Inimigo a, b;

    memset(&jogo, 0, sizeof jogo);
    jogo.proximoId = 1;

    a = criarInimigo(XWING, (Vector3){ 0.0f, 0.0f, 1800.0f }, jogo.proximoId++);
    b = criarInimigo(AWING, (Vector3){ 1800.0f, 0.0f, 0.0f }, jogo.proximoId++);

    VERIFICA(a.id == 1);
    VERIFICA(b.id == 2);
    VERIFICA(a.id != b.id);
    VERIFICA(jogo.proximoId == 3);
}

static void testarInsercaoEmListaVazia(void)
{
    ListaInimigos l = { 0 };

    inserirInimigo(&l, criarInimigo(XWING, (Vector3){ 1.0f, 2.0f, 3.0f }, 1));

    VERIFICA(l.tamanho == 1);
    VERIFICA(l.inicio != NULL && l.inicio == l.fim);
    VERIFICA(l.alvo == l.inicio); // estava vazia: novo nó vira alvo
    VERIFICA(l.inicio->ant == NULL && l.inicio->prox == NULL);
    VERIFICA(l.inicio->dado.id == 1 && l.inicio->dado.tipo == XWING);
    VERIFICA(proximo(l.inicio->dado.escudo, 50.0f));

    liberarListaInimigos(&l); // primeiro malloc do módulo
    VERIFICA(l.inicio == NULL && l.fim == NULL && l.alvo == NULL);
    VERIFICA(l.tamanho == 0);
}

static void testarInsercaoSempreNoFim(void)
{
    ListaInimigos l = { 0 };
    NoInimigo *n, *primeiro;
    int ids[3], i;

    inserirInimigo(&l, criarInimigo(XWING, (Vector3){ 0.0f, 0.0f, 1.0f }, 1));
    primeiro = l.inicio;
    inserirInimigo(&l, criarInimigo(YWING, (Vector3){ 0.0f, 0.0f, 2.0f }, 2));
    inserirInimigo(&l, criarInimigo(AWING, (Vector3){ 0.0f, 0.0f, 3.0f }, 3));

    VERIFICA(l.tamanho == 3);
    VERIFICA(l.inicio == primeiro); // entra no FIM, nunca no início
    VERIFICA(l.fim != NULL && l.fim->dado.id == 3);
    VERIFICA(l.alvo == primeiro); // alvo NÃO muda em lista não vazia

    for (n = l.inicio, i = 0; n != NULL && i < 3; n = n->prox, i++)
        ids[i] = n->dado.id;
    VERIFICA(ids[0] == 1 && ids[1] == 2 && ids[2] == 3);

    VERIFICA(l.inicio->ant == NULL);
    VERIFICA(l.fim->prox == NULL);
    VERIFICA(l.fim->ant != NULL && l.fim->ant->dado.id == 2);
    VERIFICA(l.inicio->prox != NULL && l.inicio->prox->ant == l.inicio); // duplo sentido

    liberarListaInimigos(&l);
    VERIFICA(l.tamanho == 0 && l.inicio == NULL && l.fim == NULL && l.alvo == NULL);
}

int main(void)
{
    testarXWing();
    testarYWing();
    testarAWing();
    testarIdsDoJogo();
    testarInsercaoEmListaVazia();
    testarInsercaoSempreNoFim();

    return RESULTADO_TESTES();
}