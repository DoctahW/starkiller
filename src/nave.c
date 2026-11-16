#include "nave.h"
#include "raymath.h"

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

/* Deriva os eixos da nave a partir de rot (frente +Z, cima +Y, direita -X no espaço local). */
static void derivarEixos(Jogador *j)
{
    j->frente = Vector3RotateByQuaternion((Vector3){ 0.0f, 0.0f, 1.0f }, j->rot);
    j->cima = Vector3RotateByQuaternion((Vector3){ 0.0f, 1.0f, 0.0f }, j->rot);
    j->direita = Vector3RotateByQuaternion((Vector3){ -1.0f, 0.0f, 0.0f }, j->rot);
}

void atualizarJogador(Jogador *j, const Comandos *cmd, float dt)
{
    /* Rotação incremental no espaço local: rot * delta gira em torno dos eixos da própria nave. */
    Quaternion arf = QuaternionFromAxisAngle((Vector3){ -1.0f, 0.0f, 0.0f }, cmd->arfagem * VEL_ARFAGEM * dt);
    Quaternion gui = QuaternionFromAxisAngle((Vector3){ 0.0f, -1.0f, 0.0f }, cmd->guinada * VEL_GUINADA * dt);
    Quaternion rol = QuaternionFromAxisAngle((Vector3){ 0.0f, 0.0f, 1.0f }, cmd->rolagem * VEL_ROLAGEM * dt);
    Quaternion delta = QuaternionMultiply(QuaternionMultiply(arf, gui), rol);

    j->rot = QuaternionNormalize(QuaternionMultiply(j->rot, delta));
    derivarEixos(j);
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
