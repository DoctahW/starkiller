#include "render.h"

#include "espaco.h"

void carregarAssets(void)
{
    iniciarEspaco();
}

void descarregarAssets(void)
{
    encerrarEspaco();
}

void render_cena(const Jogo *g, Camera3D cam)
{
    desenharFundo(&g->jogador);

    BeginMode3D(cam);
    /* A poeira não grava profundidade: vem depois dos objetos opacos da arena. */
    desenharPoeira(&g->jogador);
    EndMode3D();
}
