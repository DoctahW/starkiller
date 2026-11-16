#include <math.h>

#include "nave.h"
#include "teste.h"

#define EPS 1e-5f

static int proximo(float a, float b)
{
    return fabsf(a - b) < EPS;
}

#define PI_F 3.14159265358979f

/* Aplica cmd por `angulo / vel` segundos em 600 passos, para girar exatamente `angulo` rad. */
static void girar(Jogador *j, Comandos cmd, float vel, float angulo)
{
    int n = 600;
    float dt = (angulo / vel) / (float)n;
    int i;

    for (i = 0; i < n; i++)
        atualizarJogador(j, &cmd, dt);
}

static float tam(Vector3 v)
{
    return sqrtf(v.x * v.x + v.y * v.y + v.z * v.z);
}

static float dot(Vector3 a, Vector3 b)
{
    return a.x * b.x + a.y * b.y + a.z * b.z;
}

/* frente/cima/direita unitários e mutuamente ortogonais; rot unitário. */
static int baseValida(const Jogador *j)
{
    float q = j->rot.x * j->rot.x + j->rot.y * j->rot.y + j->rot.z * j->rot.z + j->rot.w * j->rot.w;

    return fabsf(q - 1.0f) < 1e-4f
        && fabsf(tam(j->frente) - 1.0f) < 1e-4f && fabsf(tam(j->cima) - 1.0f) < 1e-4f
        && fabsf(tam(j->direita) - 1.0f) < 1e-4f
        && fabsf(dot(j->frente, j->cima)) < 1e-4f && fabsf(dot(j->frente, j->direita)) < 1e-4f
        && fabsf(dot(j->cima, j->direita)) < 1e-4f;
}

static void testarRotacao(void)
{
    Jogador j;
    Comandos c = { 0 };

    /* Sinais: arfagem > 0 sobe o nariz, guinada > 0 vira à direita, rolagem > 0 inclina à direita. */
    iniciarJogador(&j);
    c.arfagem = 1.0f;
    girar(&j, c, VEL_ARFAGEM, 0.3f);
    VERIFICA(j.frente.y > 0.2f && proximo(j.frente.x, 0.0f));

    iniciarJogador(&j);
    c = (Comandos){ .guinada = 1.0f };
    girar(&j, c, VEL_GUINADA, 0.3f);
    VERIFICA(j.frente.x < -0.2f && proximo(j.frente.y, 0.0f));

    iniciarJogador(&j);
    c = (Comandos){ .rolagem = 1.0f };
    girar(&j, c, VEL_ROLAGEM, 0.3f);
    VERIFICA(j.cima.x < -0.2f && proximo(j.frente.x, 0.0f) && proximo(j.frente.y, 0.0f));

    /* Proporcional ao analógico: metade do eixo, metade do ângulo. */
    iniciarJogador(&j);
    c = (Comandos){ .arfagem = 0.5f };
    girar(&j, c, VEL_ARFAGEM * 0.5f, 0.4f);
    VERIFICA(fabsf(j.frente.y - sinf(0.4f)) < 1e-3f);

    /* Espaço local: depois de rolar 90° à direita, arfagem para cima move o nariz para a direita do mundo (-X). */
    iniciarJogador(&j);
    c = (Comandos){ .rolagem = 1.0f };
    girar(&j, c, VEL_ROLAGEM, PI_F / 2.0f);
    c = (Comandos){ .arfagem = 1.0f };
    girar(&j, c, VEL_ARFAGEM, 0.3f);
    VERIFICA(j.frente.x < -0.2f && fabsf(j.frente.y) < 1e-3f);
}

static void testarSemGimbalLock(void)
{
    Jogador j;
    Comandos c = { 0 };

    /* Loop completo de arfagem volta à orientação inicial. */
    iniciarJogador(&j);
    c.arfagem = 1.0f;
    girar(&j, c, VEL_ARFAGEM, 2.0f * PI_F);
    VERIFICA(fabsf(j.frente.x) < 1e-3f && fabsf(j.frente.y) < 1e-3f && fabsf(j.frente.z - 1.0f) < 1e-3f);
    VERIFICA(fabsf(j.cima.y - 1.0f) < 1e-3f);

    /* A 90° de arfagem (nariz a pino) a guinada ainda gira a nave: sem gimbal lock. */
    iniciarJogador(&j);
    c = (Comandos){ .arfagem = 1.0f };
    girar(&j, c, VEL_ARFAGEM, PI_F / 2.0f);
    VERIFICA(fabsf(j.frente.y - 1.0f) < 1e-3f);
    c = (Comandos){ .guinada = 1.0f };
    girar(&j, c, VEL_GUINADA, PI_F / 2.0f);
    VERIFICA(fabsf(j.frente.y) < 1e-3f && fabsf(fabsf(j.frente.x) - 1.0f) < 1e-3f);
    VERIFICA(baseValida(&j));
}

static void testarRenormalizacao(void)
{
    Jogador j;
    Comandos c = { .arfagem = 0.7f, .guinada = -0.4f, .rolagem = 1.0f };
    int i;

    iniciarJogador(&j);
    for (i = 0; i < 100000; i++)
        atualizarJogador(&j, &c, 1.0f / 60.0f);
    VERIFICA(baseValida(&j));

    /* Sem comandos, a orientação não muda. */
    c = (Comandos){ 0 };
    {
        Vector3 antes = j.frente;
        atualizarJogador(&j, &c, 1.0f / 60.0f);
        VERIFICA(proximo(j.frente.x, antes.x) && proximo(j.frente.y, antes.y) && proximo(j.frente.z, antes.z));
    }
}

static void testarAceleracao(void)
{
    Jogador j;
    Comandos c = { 0 };
    int i;

    /* Sem comando, mantém a velocidade e avança frente * v * dt. */
    iniciarJogador(&j);
    j.pos = (Vector3){ 0.0f, 0.0f, 0.0f };
    atualizarJogador(&j, &c, 0.5f);
    VERIFICA(proximo(j.velocidade, VEL_MIN));
    VERIFICA(proximo(j.pos.z, VEL_MIN * 0.5f) && proximo(j.pos.x, 0.0f) && proximo(j.pos.y, 0.0f));

    /* Acelerar: sobe a ACELERACAO u/s² e para em VEL_MAX. */
    c.aceleracao = 1.0f;
    atualizarJogador(&j, &c, 1.0f);
    VERIFICA(proximo(j.velocidade, VEL_MIN + ACELERACAO));
    for (i = 0; i < 1000; i++)
        atualizarJogador(&j, &c, 1.0f / 60.0f);
    VERIFICA(proximo(j.velocidade, VEL_MAX));

    /* Frear: desce e para em VEL_MIN, sem ficar parada nem andar de ré. */
    c.aceleracao = -1.0f;
    for (i = 0; i < 1000; i++)
        atualizarJogador(&j, &c, 1.0f / 60.0f);
    VERIFICA(proximo(j.velocidade, VEL_MIN));

    /* Proporcional ao analógico. */
    iniciarJogador(&j);
    c.aceleracao = 0.5f;
    atualizarJogador(&j, &c, 1.0f);
    VERIFICA(proximo(j.velocidade, VEL_MIN + 0.5f * ACELERACAO));

    /* O avanço segue a frente atual: depois de guinar 90° à direita, anda para -X. */
    iniciarJogador(&j);
    j.pos = (Vector3){ 0.0f, 0.0f, 0.0f };
    c = (Comandos){ .guinada = 1.0f };
    girar(&j, c, VEL_GUINADA, PI_F / 2.0f);
    j.pos = (Vector3){ 0.0f, 0.0f, 0.0f };
    c = (Comandos){ 0 };
    atualizarJogador(&j, &c, 1.0f);
    VERIFICA(fabsf(j.pos.x + j.velocidade) < 1e-2f && fabsf(j.pos.z) < 1e-2f);
}

static void testarArena(void)
{
    Jogador j;
    Comandos c = { 0 };
    int i, avisou = 0, saiu = 0;

    /* Dentro da arena: sem aviso. */
    iniciarJogador(&j);
    VERIFICA(atualizarJogador(&j, &c, 1.0f / 60.0f) == 0);

    /* Voando para fora na velocidade máxima: nunca passa do raio, e o aviso dispara. */
    c.aceleracao = 1.0f;
    for (i = 0; i < 6000; i++) {
        avisou |= atualizarJogador(&j, &c, 1.0f / 60.0f);
        if (tam(j.pos) > RAIO_ARENA + 1e-2f)
            saiu = 1;
    }
    VERIFICA(avisou == 1);
    VERIFICA(saiu == 0);
    VERIFICA(atualizarJogador(&j, &c, 1.0f / 60.0f) == 1);
    VERIFICA(fabsf(tam(j.pos) - RAIO_ARENA) < 1e-2f);

    /* Passo grande (dt exagerado) também fica dentro. */
    iniciarJogador(&j);
    j.velocidade = VEL_MAX;
    VERIFICA(atualizarJogador(&j, &c, 100.0f) == 1);
    VERIFICA(tam(j.pos) <= RAIO_ARENA + 1e-2f);

    /* Virando 180° na borda, a nave volta para dentro e o aviso some. */
    c = (Comandos){ .arfagem = 1.0f };
    girar(&j, c, VEL_ARFAGEM, PI_F);
    c = (Comandos){ 0 };
    atualizarJogador(&j, &c, 1.0f);
    VERIFICA(tam(j.pos) < RAIO_ARENA - 1.0f);
    VERIFICA(atualizarJogador(&j, &c, 1.0f / 60.0f) == 0);
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

    testarRotacao();
    testarSemGimbalLock();
    testarRenormalizacao();
    testarAceleracao();
    testarArena();

    return RESULTADO_TESTES();
}
