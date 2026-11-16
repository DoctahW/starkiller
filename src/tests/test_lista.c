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

int main(void)
{
    testarXWing();
    testarYWing();
    testarAWing();
    testarIdsDoJogo();

    return RESULTADO_TESTES();
}