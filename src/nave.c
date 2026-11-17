#include "nave.h"
#include <math.h>

#include "raymath.h"
#include "destroier.h"
#include "pontuacao.h"

#define VELOCIDADE_INICIAL VEL_MIN
#define CASCO_JOGADOR      100.0f

/* Velocidade angular atual (rad/s) em x = arfagem, y = guinada, z = rolagem. Só existe um jogador. */
static Vector3 velAng;
/* Tempo restante (s) sem novo dano de colisão. Só existe um jogador; iniciarJogador zera. */
static float recargaColisao;

void iniciarJogador(Jogador *j)
{
    velAng = (Vector3){ 0.0f, 0.0f, 0.0f };
    recargaColisao = 0.0f;
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

int atualizarJogador(Jogador *j, const Comandos *cmd, float dt)
{
    /* Suavização exponencial (independe do dt): a velocidade angular persegue o alvo do comando. */
    Vector3 alvo = { cmd->arfagem * VEL_ARFAGEM, cmd->guinada * VEL_GUINADA, cmd->rolagem * VEL_ROLAGEM };
    Vector3 antes = velAng;
    float k = 1.0f - expf(-dt / TAU_GIRO);
    Vector3 media;

    velAng.x += (alvo.x - velAng.x) * k;
    velAng.y += (alvo.y - velAng.y) * k;
    velAng.z += (alvo.z - velAng.z) * k;
    /* Ângulo do passo pela média das velocidades (trapézio), mais preciso que só a nova. */
    media = Vector3Scale(Vector3Add(antes, velAng), 0.5f);

    /* Rotação incremental no espaço local: rot * delta gira em torno dos eixos da própria nave. */
    Quaternion arf = QuaternionFromAxisAngle((Vector3){ -1.0f, 0.0f, 0.0f }, media.x * dt);
    Quaternion gui = QuaternionFromAxisAngle((Vector3){ 0.0f, -1.0f, 0.0f }, media.y * dt);
    Quaternion rol = QuaternionFromAxisAngle((Vector3){ 0.0f, 0.0f, 1.0f }, media.z * dt);
    Quaternion delta = QuaternionMultiply(QuaternionMultiply(arf, gui), rol);

    j->rot = QuaternionNormalize(QuaternionMultiply(j->rot, delta));
    derivarEixos(j);

    j->velocidade = Clamp(j->velocidade + cmd->aceleracao * ACELERACAO * dt, VEL_MIN, VEL_MAX);
    j->pos = Vector3Add(j->pos, Vector3Scale(j->frente, j->velocidade * dt));

    /* Arena: fora da esfera, empurra de volta para a borda e avisa. */
    {
        float dist = Vector3Length(j->pos);

        if (dist > RAIO_ARENA) {
            j->pos = Vector3Scale(j->pos, RAIO_ARENA / dist);
            return 1;
        }
    }
    return 0;
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

/* Empurra a nave para a superfície da esfera `e` se estiver dentro (centro a centro < raio + RAIO_JOGADOR). */
static int empurrarParaFora(Jogador *j, const EsferaDestroier *e)
{
    Vector3 d = Vector3Subtract(j->pos, e->centro);
    float dist = Vector3Length(d);
    float minimo = e->raio + RAIO_JOGADOR;

    if (dist >= minimo)
        return 0;
    /* No centro exato a normal é indefinida: sai pela proa. */
    d = dist > 1e-6f ? Vector3Scale(d, 1.0f / dist) : (Vector3){ 0.0f, 0.0f, 1.0f };
    j->pos = Vector3Add(e->centro, Vector3Scale(d, minimo));
    return 1;
}

int colidirDestroier(Jogo *g, float dt)
{
    int i, passada, contato = 0;

    if (recargaColisao > 0.0f)
        recargaColisao -= dt;

    /* Empurrar para fora de uma esfera pode entrar noutra (esferas se sobrepõem): repete até assentar. */
    for (passada = 0; passada < NUM_ESFERAS_DESTROIER; passada++) {
        int moveu = 0;

        for (i = 0; i < NUM_ESFERAS_DESTROIER; i++)
            moveu |= empurrarParaFora(&g->jogador, &ESFERAS_DESTROIER[i]);
        if (!moveu)
            break;
        contato = 1;
    }

    if (contato && recargaColisao <= 0.0f) {
        g->jogador.casco -= DANO_COLISAO_DESTROIER;
        if (g->jogador.casco < 0.0f)
            g->jogador.casco = 0.0f;
        registrarDanoJogador(g);
        recargaColisao = RECARGA_COLISAO;
    }
    return contato;
}
