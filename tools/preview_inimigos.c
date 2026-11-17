/* Inspeção das primitivas usando o mesmo render da partida. Rodar na raiz. */
#include <math.h>
#include <string.h>

#include "inimigos.h"
#include "nave.h"
#include "render.h"
#include "raymath.h"

int main(int argc, char **argv)
{
    /* A cena fica longe do Destróier (que agora ocupa a origem) para as naves não nascerem dentro do casco. */
    const float cenaZ = 3000.0f;
    int capturar = argc > 1 && strcmp(argv[1], "--capturar") == 0;
    InitWindow(1000, 640, "Starkiller / inimigos e lasers");
    if (!IsWindowReady()) return 1;
    SetTargetFPS(60);

    Jogo g = {0};
    NoInimigo nos[3] = {0};
    const char *nomes[] = {"X-WING", "Y-WING", "A-WING"};
    iniciarJogador(&g.jogador);
    g.jogador.pos = (Vector3){0, 12, cenaZ - 65.0f};
    g.jogador.frente = (Vector3){0, 0, 1};
    g.jogador.direita = Vector3Normalize(Vector3CrossProduct(g.jogador.frente, (Vector3){0, 1, 0}));
    g.jogador.cima = Vector3CrossProduct(g.jogador.direita, g.jogador.frente);
    Camera3D cam = cameraDoJogador(&g.jogador);
    for (int i = 0; i < 3; i++) {
        /* A câmera olha para +Z: o lado esquerdo da tela corresponde a +X. */
        nos[i].dado = criarInimigo((TipoNave)i, (Vector3){(1-i)*24.0f, 0, cenaZ}, i);
        nos[i].prox = i < 2 ? &nos[i+1] : NULL;
        nos[i].ant = i > 0 ? &nos[i-1] : NULL;
    }
    g.inimigos = (ListaInimigos){.inicio=&nos[0], .fim=&nos[2], .tamanho=3};
    carregarAssets();
    int frame = 0, falhou = 0;
    while (!WindowShouldClose() && (!capturar || frame < 3)) {
        float angulo = capturar ? 0.6f : (float)GetTime()*0.5f;
        for (int i = 0; i < 3; i++)
            nos[i].dado.vel = (Vector3){sinf(angulo), 0, -cosf(angulo)};
        float fase = capturar ? 0.5f : fmodf((float)GetTime()*0.4f, 1.0f);
        g.lasers[0] = (Laser){.pos={25-50*fase, -19, cenaZ}, .vel={-1,0,0}, .ativo=1, .doJogador=1};
        g.lasers[1] = (Laser){.pos={-25+50*fase, -24, cenaZ}, .vel={1,0,0}, .ativo=1, .doJogador=0};
        BeginDrawing();
        ClearBackground(BLACK);
        render_cena(&g, cam);
        DrawText("PREVIEW DOS INIMIGOS E LASERS", 24, 24, 26, RAYWHITE);
        DrawText("Tres naves proximas da camera, girando para inspecao. Esc: sair", 24, 60, 18, LIGHTGRAY);
        for (int i = 0; i < 3; i++) {
            Vector3 p = Vector3Add(nos[i].dado.pos, (Vector3){0, -13, 0});
            Vector2 tela = GetWorldToScreen(p, cam);
            DrawText(nomes[i], (int)tela.x - MeasureText(nomes[i], 20)/2, (int)tela.y, 20, RAYWHITE);
        }
        DrawText("Verde: jogador", 24, 584, 20, GREEN);
        DrawText("Vermelho: inimigos", 24, 610, 20, RED);
        EndDrawing();
        if (capturar && frame == 2) {
            Image imagem = LoadImageFromScreen();
            falhou = !ExportImage(imagem, "build/inimigos.png");
            UnloadImage(imagem);
        }
        frame++;
    }
    descarregarAssets();
    CloseWindow();
    return falhou;
}
