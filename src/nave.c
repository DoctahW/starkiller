#include "nave.h"
#include <math.h>

#include "raymath.h"
#include "audio.h"
#include "destroier.h"
#include "pontuacao.h"

#define VELOCIDADE_INICIAL VEL_MIN

/* Queda: giro incontrolável até a explosão final (AUD-01). */
#define QUEDA_GIRO_MIN       3.5f   /* rad/s */
#define QUEDA_GIRO_MAX       7.0f
#define QUEDA_DURACAO_MIN    1.2f   /* s; abaixo da duração dos áudios de SPIN */
#define QUEDA_DURACAO_MAX    2.6f

/* Velocidade angular atual (rad/s) em x = arfagem, y = guinada, z = rolagem. Só existe um jogador. */
static Vector3 velAng;
/* Tempo restante (s) sem novo dano de colisão. Só existe um jogador; iniciarJogador zera. */
static float recargaColisao;

/* Estado da queda do jogador; só existe um jogador. */
static int faseQueda;
static Vector3 eixoQueda;
static float giroQueda, tempoQueda, duracaoQueda;

void iniciarJogador(Jogador *j)
{
    velAng = (Vector3){ 0.0f, 0.0f, 0.0f };
    recargaColisao = 0.0f;
    faseQueda = 0;
    tempoQueda = duracaoQueda = 0.0f;

    /* Abaixo do casco do Destróier, no mesmo ponto de nascimento dos TIEs aliados. */
    j->pos = (Vector3){ 0.0f, SPAWN_ALA_Y, 0.0f };
    j->rot = (Quaternion){ 0.0f, 0.0f, 0.0f, 1.0f };
    j->frente = (Vector3){ 0.0f, 0.0f, 1.0f };
    j->cima = (Vector3){ 0.0f, 1.0f, 0.0f };
    j->direita = (Vector3){ -1.0f, 0.0f, 0.0f };
    j->velocidade = VELOCIDADE_INICIAL;
    j->casco = CASCO_JOGADOR;
    j->cooldownLaser = 0.0f;
    j->cooldownReparo = 0.0f;
    j->morto = 0;
    j->explodiu = 0;
    j->tempoRespawn = 0.0f;
    j->tempoExplosao = 0.0f;
    j->respawnsRestantes = RESPAWNS_JOGADOR;
}

void danificarJogador(Jogo *g, float dano)
{
    Jogador *j = &g->jogador;

    if (dano <= 0.0f || j->casco <= 0.0f)
        return;
    j->casco -= dano;
    if (j->casco < 0.0f)
        j->casco = 0.0f;
    registrarDanoJogador(g);
}

int repararJogador(Jogo *g)
{
    Jogador *j = &g->jogador;

    if (j->morto || j->casco <= 0.0f || j->cooldownReparo > 0.0f || j->casco >= CASCO_JOGADOR)
        return 0;
    j->casco = fminf(CASCO_JOGADOR, j->casco + REPARO_JOGADOR);
    j->cooldownReparo = COOLDOWN_REPARO;
    audioRepararJogador();
    return 1;
}

int respawnJogador(Jogo *g)
{
    Jogador *j = &g->jogador;
    int restantes;

    if (j->respawnsRestantes <= 0)
        return 0;
    restantes = j->respawnsRestantes - 1;
    iniciarJogador(j);
    j->respawnsRestantes = restantes;
    return 1;
}

/* Deriva os eixos da nave a partir de rot (frente +Z, cima +Y, direita -X no espaço local). */
static void derivarEixos(Jogador *j)
{
    j->frente = Vector3RotateByQuaternion((Vector3){ 0.0f, 0.0f, 1.0f }, j->rot);
    j->cima = Vector3RotateByQuaternion((Vector3){ 0.0f, 1.0f, 0.0f }, j->rot);
    j->direita = Vector3RotateByQuaternion((Vector3){ -1.0f, 0.0f, 0.0f }, j->rot);
}

static float sorteio(float minimo, float maximo)
{
    return minimo + (maximo - minimo) * ((float)GetRandomValue(0, 10000) / 10000.0f);
}

float iniciarQuedaJogador(void)
{
    Vector3 eixo = {
        sorteio(-1.0f, 1.0f),
        sorteio(-1.0f, 1.0f),
        sorteio(-1.0f, 1.0f),
    };

    if (Vector3LengthSqr(eixo) < 1e-4f)
        eixo = (Vector3){ 1.0f, 0.0f, 0.0f };
    eixoQueda = Vector3Normalize(eixo);
    giroQueda = sorteio(QUEDA_GIRO_MIN, QUEDA_GIRO_MAX);
    duracaoQueda = sorteio(QUEDA_DURACAO_MIN, QUEDA_DURACAO_MAX);
    tempoQueda = 0.0f;
    faseQueda = 1;
    return duracaoQueda;
}

int faseQuedaJogador(void)
{
    return faseQueda;
}

/* Queda: gira sem ler os comandos e mantém a velocidade, para a cabine rodopiar até a explosão. */
static void atualizarQueda(Jogador *j, float dt)
{
    Quaternion delta;

    tempoQueda += dt;
    if (tempoQueda >= duracaoQueda)
        faseQueda = 2;

    delta = QuaternionFromAxisAngle(eixoQueda, giroQueda * dt);
    j->rot = QuaternionNormalize(QuaternionMultiply(j->rot, delta));
    derivarEixos(j);
    j->pos = Vector3Add(j->pos, Vector3Scale(j->frente, j->velocidade * dt));

    {
        float dist = Vector3Length(j->pos);

        if (dist > RAIO_ARENA)
            j->pos = Vector3Scale(j->pos, RAIO_ARENA / dist);
    }
}

int atualizarJogador(Jogador *j, const Comandos *cmd, float dt)
{
    /* Suavização exponencial (independe do dt): a velocidade angular persegue o alvo do comando. */
    Vector3 alvo = { cmd->arfagem * VEL_ARFAGEM, cmd->guinada * VEL_GUINADA, cmd->rolagem * VEL_ROLAGEM };
    Vector3 antes = velAng;
    float k = 1.0f - expf(-dt / TAU_GIRO);
    Vector3 media;

    if (j->cooldownReparo > 0.0f) {
        j->cooldownReparo -= dt;
        if (j->cooldownReparo < 0.0f)
            j->cooldownReparo = 0.0f;
    }
    if (j->morto && faseQueda != 1)
        return 0;
    if (faseQueda == 1) {
        atualizarQueda(j, dt);
        return 0;
    }
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

/*
 * Câmera de terceira pessoa depois da explosão: começa junto do TIE e se afasta em ~1,6 s,
 * olhando para o ponto da explosão. Durante o giro da queda ainda é a cabine.
 */
static Camera3D cameraDaMorte(const Jogo *g)
{
    const Jogador *j = &g->jogador;
    Camera3D cam = { 0 };
    Vector3 dir = Vector3Add(Vector3Negate(j->frente), Vector3Scale(j->cima, 0.55f));
    float progresso = 0.0f;
    Vector3 posicao;

    if (j->tempoExplosao > 0.0f)
        progresso = j->tempoExplosao / 1.6f;
    if (progresso < 0.0f)
        progresso = 0.0f;
    if (progresso > 1.0f)
        progresso = 1.0f;
    progresso = progresso * progresso * (3.0f - 2.0f * progresso);   /* suaviza o zoom */

    if (Vector3LengthSqr(dir) < 1e-6f)
        dir = (Vector3){ 0.0f, 0.6f, -1.0f };
    dir = Vector3Normalize(dir);
    posicao = Vector3Add(j->pos, Vector3Scale(dir, 28.0f + 120.0f * progresso));
    afastarDoDestroier(&posicao, 4.0f);   /* se a morte foi junto ao casco, não entra nele */

    cam.position = posicao;
    cam.target = j->pos;
    cam.up = (Vector3){ 0.0f, 1.0f, 0.0f };
    cam.fovy = FOV_JOGADOR;
    cam.projection = CAMERA_PERSPECTIVE;
    return cam;
}

Camera3D cameraComTremor(const Jogo *g)
{
    Camera3D cam = cameraDoJogador(&g->jogador);
    float fator, dx, dy;
    Vector3 direita, desloc;

    if (g->jogador.explodiu)
        return cameraDaMorte(g);
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
        if (g->jogador.casco > 0.0f) {
            danificarJogador(g, DANO_COLISAO_DESTROIER);
            audioColisaoJogador();
        }
        recargaColisao = RECARGA_COLISAO;
    }
    return contato;
}
