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
