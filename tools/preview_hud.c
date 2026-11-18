/* Prévia da HUD em 1280×720, com um estado de partida fixo. Rodar na raiz: build/preview_hud --capturar */
#include <math.h>
#include <stdio.h>
#include <string.h>

#include "aliados.h"
#include "destroier.h"
#include "hud.h"
#include "inimigos.h"
#include "jogo.h"
#include "nave.h"
#include "render.h"

int main(int argc, char **argv)
{
    int capturar = argc > 1 && strcmp(argv[1], "--capturar") == 0;
    int critico = argc > 2 && strcmp(argv[2], "--critico") == 0;
    int vazio = argc > 2 && strcmp(argv[2], "--vazio") == 0;
    int reparo = argc > 2 && strcmp(argv[2], "--reparo") == 0;
    NoInimigo alvo = { 0 }, ywing = { 0 }, awing = { 0 };
    Jogo g = { 0 };
    int frame = 0, falhou = 0;

    InitWindow(LARGURA_TELA, ALTURA_TELA, "Starkiller / HUD v2");
    if (!IsWindowReady())
        return 1;
    SetTargetFPS(60);
    hud_carregar();

    iniciarJogador(&g.jogador);
    g.jogador.pos = (Vector3){ 0.0f, 15.0f, 5200.0f };
    g.jogador.frente = (Vector3){ 0.0f, 0.0f, 1.0f };
    g.jogador.direita = (Vector3){ -1.0f, 0.0f, 0.0f };
    g.jogador.cima = (Vector3){ 0.0f, 1.0f, 0.0f };
    g.jogador.velocidade = 420.0f;
    g.jogador.casco = critico ? 18.0f : 72.0f;
    g.jogador.cooldownReparo = reparo ? 0.0f : 7.0f;
    g.jogador.respawnsRestantes = 2;

    alvo.dado = criarInimigo(XWING, (Vector3){ 0.0f, 15.0f, 5850.0f }, 1);
    alvo.dado.vel = (Vector3){ 0.0f, 0.0f, -60.0f };
    alvo.dado.casco *= 0.55f;

    ywing.dado = criarInimigo(YWING, (Vector3){ 500.0f, 900.0f, 700.0f }, 2);
    ywing.dado.faseBombardeio = vazio ? BOMBA_RETIRAR : BOMBA_APROXIMAR;
    ywing.dado.bombasLancadas = 0;

    awing.dado = criarInimigo(AWING, (Vector3){ -2200.0f, 400.0f, 1200.0f }, 3);

    alvo.prox = &ywing;
    ywing.prox = &awing;
    ywing.ant = &alvo;
    awing.ant = &ywing;
    g.inimigos = (ListaInimigos){ .inicio = &alvo, .fim = &awing, .alvo = &alvo, .tamanho = 3 };

    g.aliados[0].ativo = 1;
    g.aliados[0].papel = ALIADO_DEFENDER;
    g.aliados[0].pos = (Vector3){ 600.0f, 100.0f, 4600.0f };
    g.aliados[1].ativo = 1;
    g.aliados[1].papel = ALIADO_INTERCEPTAR;
    g.aliados[1].pos = (Vector3){ -900.0f, 200.0f, 4900.0f };
    g.aliados[2].ativo = 1;
    g.aliados[2].papel = ALIADO_DEFENDER;
    g.aliados[2].pos = (Vector3){ 300.0f, -150.0f, 4000.0f };

    g.cascoDestroier = CASCO_MAX_DESTROIER * 0.72f;
    g.escudoDestroier = ESCUDO_MAX_DESTROIER * 0.35f;
    g.tempoRestante = 287.0f;
    g.pontos = 3420;
    g.abates = 12;
    g.sequenciaSemDano = 7;
    g.tempoDano = 0.0f;

    while (!WindowShouldClose() && (!capturar || frame < 4)) {
        BeginDrawing();
        ClearBackground((Color){ 12, 14, 22, 255 });
        hud_desenhar(&g, 0);
        EndDrawing();

        if (capturar && frame == 3) {
            Image imagem = LoadImageFromScreen();
            const char *arquivo = critico ? "build/hud_critico.png"
                               : (vazio ? "build/hud_vazio.png"
                               : (reparo ? "build/hud_reparo.png" : "build/hud.png"));

            falhou = !ExportImage(imagem, arquivo);
            UnloadImage(imagem);
        }
        frame++;
    }

    hud_descarregar();
    CloseWindow();
    return falhou;
}
