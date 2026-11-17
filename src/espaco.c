#include "espaco.h"
#include <math.h>

#include "nave.h"
#include "raymath.h"
#include "rlgl.h"

typedef struct {
    Vector3 dir;    /* unitário */
    float tam;      /* lado do quadrado a RAIO_CEU */
    Color cor;
} Estrela;

static Estrela estrelas[NUM_ESTRELAS];
static Vector3 poeira[NUM_POEIRA];   /* posições no mundo, em [0, LADO_POEIRA)^3 */
static unsigned int semente;

/* LCG próprio: o céu é sempre o mesmo e não mexe no rand() dos outros módulos. Devolve [0, 1). */
static float aleatorio(void)
{
    semente = semente * 1664525u + 1013904223u;
    return (float)(semente >> 8) * (1.0f / 16777216.0f);
}

static void sortearEstrelasEPoeira(void)
{
    semente = 0x5EEDu;
    for (int i = 0; i < NUM_ESTRELAS; i++) {
        /* Uniforme na esfera: z uniforme em [-1, 1] e ângulo uniforme. */
        float z = 2.0f * aleatorio() - 1.0f;
        float ang = 2.0f * PI * aleatorio();
        float r = sqrtf(1.0f - z * z);
        float brilho = powf(aleatorio(), 4.0f);   /* maioria fraca, poucas fortes */
        unsigned char v = (unsigned char)(90.0f + 165.0f * brilho);
        float tinta = aleatorio();
        Estrela *e = &estrelas[i];

        e->dir = (Vector3){ r * cosf(ang), r * sinf(ang), z };
        /* A RAIO_CEU, 1 px ~ 1,9 u (FOV 70, 720 px): 1,5-4,5 u dá 1-2,5 px. */
        e->tam = 1.5f + 3.0f * brilho;
        if (tinta < 0.15f)
            e->cor = (Color){ (unsigned char)(v * 0.8f), (unsigned char)(v * 0.9f), v, 255 };   /* azulada */
        else if (tinta < 0.25f)
            e->cor = (Color){ v, (unsigned char)(v * 0.9f), (unsigned char)(v * 0.7f), 255 };   /* amarelada */
        else
            e->cor = (Color){ v, v, v, 255 };
    }
    for (int i = 0; i < NUM_POEIRA; i++)
        poeira[i] = (Vector3){ aleatorio() * LADO_POEIRA, aleatorio() * LADO_POEIRA, aleatorio() * LADO_POEIRA };
}

void iniciarEspaco(void)
{
    sortearEstrelasEPoeira();
}

void encerrarEspaco(void)
{
}

void desenharFundo(const Jogador *j)
{
    Camera3D ceu = { 0 };
    Vector3 d = j->direita, u = j->cima;

    ceu.target = j->frente;   /* position = origem: a translação some */
    ceu.up = j->cima;
    ceu.fovy = FOV_JOGADOR;
    ceu.projection = CAMERA_PERSPECTIVE;

    BeginMode3D(ceu);
    rlDisableDepthMask();

    rlBegin(RL_QUADS);
    for (int i = 0; i < NUM_ESTRELAS; i++) {
        const Estrela *e = &estrelas[i];
        Vector3 c = Vector3Scale(e->dir, RAIO_CEU);
        Vector3 dx = Vector3Scale(d, e->tam * 0.5f);
        Vector3 dy = Vector3Scale(u, e->tam * 0.5f);

        rlColor4ub(e->cor.r, e->cor.g, e->cor.b, e->cor.a);
        /* Anti-horário visto da câmera (direita = frente x cima): passa no culling de faces de trás. */
        rlVertex3f(c.x - dx.x - dy.x, c.y - dx.y - dy.y, c.z - dx.z - dy.z);
        rlVertex3f(c.x + dx.x - dy.x, c.y + dx.y - dy.y, c.z + dx.z - dy.z);
        rlVertex3f(c.x + dx.x + dy.x, c.y + dx.y + dy.y, c.z + dx.z + dy.z);
        rlVertex3f(c.x - dx.x + dy.x, c.y - dx.y + dy.y, c.z - dx.z + dy.z);
    }
    rlEnd();
    /* O rlgl acumula vértices: desenha as estrelas agora, antes da troca de estado. */
    rlDrawRenderBatchActive();

    rlEnableDepthMask();
    EndMode3D();
}

/* Leva x para [-LADO_POEIRA/2, LADO_POEIRA/2): a caixa de poeira se repete em volta da câmera. */
static float repetir(float x)
{
    x = fmodf(x + LADO_POEIRA * 0.5f, LADO_POEIRA);
    if (x < 0.0f)
        x += LADO_POEIRA;
    return x - LADO_POEIRA * 0.5f;
}

void desenharPoeira(const Jogador *j)
{
    Vector3 rastro = Vector3Scale(j->frente, -j->velocidade * RASTRO_POEIRA);

    rlDisableDepthMask();
    rlBegin(RL_LINES);
    for (int i = 0; i < NUM_POEIRA; i++) {
        Vector3 rel = { repetir(poeira[i].x - j->pos.x),
                        repetir(poeira[i].y - j->pos.y),
                        repetir(poeira[i].z - j->pos.z) };
        /* Some perto da borda da caixa: nada "pula" ao dar a volta. */
        float a = 1.0f - Vector3Length(rel) / (LADO_POEIRA * 0.5f);
        Vector3 p, q;

        if (a <= 0.0f)
            continue;
        p = Vector3Add(j->pos, rel);
        q = Vector3Add(p, rastro);
        rlColor4ub(200, 205, 230, (unsigned char)(a * 160.0f));
        rlVertex3f(p.x, p.y, p.z);
        rlVertex3f(q.x, q.y, q.z);
    }
    rlEnd();
    rlDrawRenderBatchActive();   /* antes de religar a escrita de profundidade */
    rlEnableDepthMask();
}
