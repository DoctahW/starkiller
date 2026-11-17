/* Inspeção isolada do pipeline GLB, executada a partir da raiz do projeto. */
#include <math.h>
#include <stdio.h>
#include <string.h>

#include "entrada.h"
#include "raymath.h"

int main(int argc, char **argv)
{
    int automatico = argc > 1 && strcmp(argv[1], "--capturar") == 0;
    int falhas = 0;
    if (!FileExists("assets/models/teste_eixos.glb")) {
        fprintf(stderr, "Execute na raiz do projeto, com assets/models/teste_eixos.glb disponível.\n");
        return 1;
    }
    InitWindow(960, 640, "Starkiller / pipeline GLB");
    if (!IsWindowReady()) return 1;
    SetExitKey(KEY_NULL);
    SetTargetFPS(60);
    Model modelo = LoadModel("assets/models/teste_eixos.glb");
    if (!IsModelValid(modelo)) {
        UnloadModel(modelo);
        CloseWindow();
        return 1;
    }
    BoundingBox b = GetModelBoundingBox(modelo);
    Vector3 minimo = {-1,-1,-2}, maximo = {1,3,4};
    if (Vector3Distance(b.min,minimo) > 0.01f || Vector3Distance(b.max,maximo) > 0.01f) falhas++;
    Texture2D textura = {0};
    for (int m = 0; m < modelo.materialCount; m++) {
        Texture2D t = modelo.materials[m].maps[MATERIAL_MAP_ALBEDO].texture;
        if (t.width == 32 && t.height == 32) textura = t;
    }
    if (!textura.id) falhas++;
    ConfigEntrada cfg = {.sensibilidade=1,.zonaMorta=0.15f};
    Camera3D camera = {.position={8,5,8},.target={0,0,1},.up={0,1,0},.fovy=45,.projection=CAMERA_PERSPECTIVE};
    int frame = 0;
    while (!WindowShouldClose() && (!automatico || frame < 120)) {
        Comandos cmd = lerComandos(&cfg);
        if (cmd.voltar || cmd.pausa) break;
        BeginDrawing();
        ClearBackground((Color){8,18,28,255});
        BeginMode3D(camera);
        DrawGrid(12,1);
        DrawLine3D((Vector3){0},(Vector3){5,0,0},BLUE);
        DrawLine3D((Vector3){0},(Vector3){0,5,0},YELLOW);
        DrawLine3D((Vector3){0},(Vector3){0,0,5},RED);
        DrawModel(modelo,(Vector3){0},1,WHITE);
        EndMode3D();
        DrawText("VIS-01 / SONDA GLB",24,24,28,RAYWHITE);
        DrawText("Grade: 1 unidade / vermelho: frente +Z / amarelo: cima +Y",24,66,18,LIGHTGRAY);
        DrawText(cmd.usandoControle ? "B/Start: sair" : "Esc: sair",24,600,18,LIGHTGRAY);
        EndDrawing();
        if (automatico && frame == 30) {
            Image captura = LoadImageFromScreen();
            if (!ExportImage(captura,"build/modelo.png")) falhas++;
            UnloadImage(captura);
        }
        frame++;
    }
    printf("Pipeline GLB: dimensoes, textura e carga; falhas %d\n",falhas);
    if (textura.id) UnloadTexture(textura);
    UnloadModel(modelo);
    CloseWindow();
    return falhas != 0;
}
