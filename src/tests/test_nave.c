#include <math.h>

#include "nave.h"
#include "teste.h"

#define EPS 1e-5f

static int proximo(float a, float b)
{
    return fabsf(a - b) < EPS;
}

int main(void)
{
    Jogador j;
    Camera3D cam;

    iniciarJogador(&j);

    /* Início: fora do Destróier (origem), olhando para longe dele. */
    VERIFICA(j.pos.x * j.frente.x + j.pos.y * j.frente.y + j.pos.z * j.frente.z > 0.0f);
    VERIFICA(proximo(j.frente.x * j.frente.x + j.frente.y * j.frente.y + j.frente.z * j.frente.z, 1.0f));

    cam = cameraDoJogador(&j);
    VERIFICA(proximo(cam.position.x, j.pos.x) && proximo(cam.position.y, j.pos.y) && proximo(cam.position.z, j.pos.z));
    VERIFICA(proximo(cam.target.x, j.pos.x + j.frente.x));
    VERIFICA(proximo(cam.target.y, j.pos.y + j.frente.y));
    VERIFICA(proximo(cam.target.z, j.pos.z + j.frente.z));
    VERIFICA(proximo(cam.up.x, j.cima.x) && proximo(cam.up.y, j.cima.y) && proximo(cam.up.z, j.cima.z));
    VERIFICA(proximo(cam.fovy, FOV_JOGADOR));
    VERIFICA(cam.projection == CAMERA_PERSPECTIVE);

    /* A câmera acompanha o jogador. */
    j.pos = (Vector3){ 10.0f, -5.0f, 20.0f };
    j.frente = (Vector3){ 1.0f, 0.0f, 0.0f };
    j.cima = (Vector3){ 0.0f, 0.0f, 1.0f };
    cam = cameraDoJogador(&j);
    VERIFICA(proximo(cam.position.x, 10.0f) && proximo(cam.target.x, 11.0f));
    VERIFICA(proximo(cam.target.y, -5.0f) && proximo(cam.up.z, 1.0f));

    return RESULTADO_TESTES();
}
