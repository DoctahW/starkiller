#include "nave.h"
#include <math.h>

#include "raymath.h"
#include "destroier.h"
#include "pontuacao.h"

#define VELOCIDADE_INICIAL VEL_MIN

/* Velocidade angular atual (rad/s) em x = arfagem, y = guinada, z = rolagem. Só existe um jogador. */
static Vector3 velAng;
/* Tempo restante (s) sem novo dano de colisão. Só existe um jogador; iniciarJogador zera. */
static float recargaColisao;

void iniciarJogador(Jogador *j)
{
    velAng = (Vector3){ 0.0f, 0.0f, 0.0f };
    recargaColisao = 0.0f;
    /* ~550 u à frente da proa, já com a escala do Destróier no jogo. */
    j->pos = (Vector3){ 0.0f, 0.0f, Z_PROA_DESTROIER * ESCALA_DESTROIER + 550.0f };
    j->rot = (Quaternion){ 0.0f, 0.0f, 0.0f, 1.0f };
    j->frente = (Vector3){ 0.0f, 0.0f, 1.0f };
    j->cima = (Vector3){ 0.0f, 1.0f, 0.0f };
    j->direita = (Vector3){ -1.0f, 0.0f, 0.0f };
    j->velocidade = VELOCIDADE_INICIAL;
    j->casco = CASCO_JOGADOR;
    j->cooldownLaser = 0.0f;
    j->escudo = ESCUDO_JOGADOR;
    j->esperaEscudo = 0.0f;
}

void danificarJogador(Jogo *g, float dano)
{
    Jogador *j = &g->jogador;

    if (dano <= 0.0f)
        return;
    if (dano <= j->escudo) {
        j->escudo -= dano;
    } else {
        j->casco -= dano - j->escudo;
        j->escudo = 0.0f;
        if (j->casco < 0.0f)
            j->casco = 0.0f;
    }
    j->esperaEscudo = ATRASO_ESCUDO;
    registrarDanoJogador(g);
}

void atualizarEscudo(Jogador *j, float dt)
{
    if (j->esperaEscudo > 0.0f) {
        j->esperaEscudo -= dt;
        return;
    }
    j->escudo += RECARGA_ESCUDO * dt;
    if (j->escudo > ESCUDO_JOGADOR)
        j->escudo = ESCUDO_JOGADOR;
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

Camera3D cameraComTremor(const Jogo *g)
{
    Camera3D cam = cameraDoJogador(&g->jogador);
    float fator, dx, dy;
    Vector3 direita, desloc;

    if (g->tempoDano <= 0.0f)
        return cam;

    fator = g->tempoDano / DURACAO_FEEDBACK_DANO;
    if (fator > 1.0f)
        fator = 1.0f;
    dx = sinf(g->tempoDecorrido * 90.0f) * AMPLITUDE_TREMOR * fator;
    dy = cosf(g->tempoDecorrido * 110.0f) * AMPLITUDE_TREMOR * fator;
    direita = Vector3CrossProduct(g->jogador.frente, g->jogador.cima);
    desloc = Vector3Add(Vector3Scale(direita, dx), Vector3Scale(g->jogador.cima, dy));
    cam.position = Vector3Add(cam.position, desloc);
    cam.target = Vector3Add(cam.target, desloc);
    return cam;
}

int colidirDestroier(Jogo *g, float dt)
{
    int contato;

    if (recargaColisao > 0.0f)
        recargaColisao -= dt;

    contato = afastarDoDestroier(&g->jogador.pos, RAIO_JOGADOR);

    if (contato && recargaColisao <= 0.0f) {
        danificarJogador(g, DANO_COLISAO_DESTROIER);
        recargaColisao = RECARGA_COLISAO;
    }
    return contato;
}
