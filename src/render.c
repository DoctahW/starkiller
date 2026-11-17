#include "render.h"

#include "espaco.h"
#include "nave.h"

void iniciarRender(void)
{
    iniciarEspaco();
}

void encerrarRender(void)
{
    encerrarEspaco();
}

void desenharCena(const Jogo *g)
{
    desenharFundo(&g->jogador);

    BeginMode3D(cameraDoJogador(&g->jogador));
    /* A poeira não grava profundidade: vem depois dos objetos opacos da arena. */
    desenharPoeira(&g->jogador);
    EndMode3D();
}
