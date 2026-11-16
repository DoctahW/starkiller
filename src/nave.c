#include "nave.h"

#define VELOCIDADE_INICIAL 40.0f
#define CASCO_JOGADOR      100.0f

void iniciarJogador(Jogador *j)
{
    j->pos = (Vector3){ 0.0f, 0.0f, 300.0f };
    j->rot = (Quaternion){ 0.0f, 0.0f, 0.0f, 1.0f };
    j->frente = (Vector3){ 0.0f, 0.0f, 1.0f };
    j->cima = (Vector3){ 0.0f, 1.0f, 0.0f };
    j->direita = (Vector3){ -1.0f, 0.0f, 0.0f };
    j->velocidade = VELOCIDADE_INICIAL;
    j->casco = CASCO_JOGADOR;
    j->cooldownLaser = 0.0f;
}

Camera3D cameraDoJogador(const Jogador *j)
{
    Camera3D cam = { 0 };

    cam.position = j->pos;
    cam.target = (Vector3){ j->pos.x + j->frente.x, j->pos.y + j->frente.y, j->pos.z + j->frente.z };
    cam.up = j->cima;
    cam.fovy = FOV_JOGADOR;
    cam.projection = CAMERA_PERSPECTIVE;
    return cam;
}
