#include "render.h"

#include <math.h>

#include "destroier.h"
#include "espaco.h"
#include "raymath.h"
#include "rlgl.h"

/* Luz das naves da arena: a mesma no shader casco e nas primitivas. */
static const Vector3 COR_LUZ  = { 0.95f, 0.93f, 0.88f };
static const Vector3 AMBIENTE = { 0.14f, 0.15f, 0.19f };

/* Plano perto e longe da câmera: nada chega a menos de 1 u dela, e a arena tem 9000 de raio. */
#define PLANO_PERTO 1.0
#define PLANO_LONGE 20000.0

/*
 * Destróier em primitivas, quando falta o destroier.glb: a cunha de destroier.h e a
 * superestrutura e os motores abaixo, aproximando o modelo nas mesmas medidas.
 */
typedef struct {
    Vector3 min, max;
} Caixa;

#define NUM_CAIXAS 5
static const Caixa CAIXAS_DESTROIER[NUM_CAIXAS] = {
    { { -183.0f,   0.0f, -750.0f }, { 183.0f, 128.0f, -320.0f } },   /* terraços */
    { { -137.0f,   0.0f, -741.0f }, { 137.0f, 165.0f, -434.0f } },
    { {  -91.0f,   0.0f, -731.0f }, {  91.0f, 201.0f, -526.0f } },
    { {  -27.0f, 201.0f, -686.0f }, {  27.0f, 283.0f, -631.0f } },   /* pescoço da torre */
    { { -128.0f, 283.0f, -695.0f }, { 128.0f, 325.0f, -622.0f } },   /* ponte de comando */
};

typedef struct {
    float x, y, raio;
} Motor;

/* Bocais de Z_POPA_DESTROIER (a boca) até a face da popa do casco, FUNDO_MOTOR à frente. */
#define NUM_MOTORES 5
static const Motor MOTORES_DESTROIER[NUM_MOTORES] = {
    { -110.0f, 0.0f, 50.0f }, { 110.0f, 0.0f, 50.0f }, { 0.0f, 0.0f, 32.0f }, { -210.0f, 0.0f, 32.0f }, { 210.0f, 0.0f, 32.0f },
};
#define FUNDO_MOTOR  27.0f
#define Z_POPA_CASCO (Z_POPA_DESTROIER + FUNDO_MOTOR)
#define LADOS_MOTOR  16

static const Color COR_CASCO = { 150, 152, 158, 255 };
static const Color COR_MOTOR = { 20, 20, 23, 255 };
static const Color COR_BOCAL = { 150, 190, 255, 255 };

static Model destroier;
static Shader sombraCasco;
static int temDestroier, temSombraCasco;
static Vector3 sol;

int carregarShader(const char *nome, Shader *s)
{
    char vs[128], fs[128];

    TextCopy(vs, TextFormat(PASTA_SHADERS "%s.vs", nome));
    TextCopy(fs, TextFormat(PASTA_SHADERS "%s.fs", nome));
    if (!FileExists(vs) || !FileExists(fs)) {
        TraceLog(LOG_WARNING, "RENDER: shader %s ausente", nome);
        return 0;
    }
    *s = LoadShader(vs, fs);
    if (s->id == rlGetShaderIdDefault()) {
        TraceLog(LOG_WARNING, "RENDER: shader %s não compilou", nome);
        return 0;
    }
    return 1;
}

void suavizarTexturas(Model *m)
{
    for (int i = 0; i < m->materialCount; i++) {
        Texture2D *t = &m->materials[i].maps[MATERIAL_MAP_ALBEDO].texture;

        if (t->id != 0 && t->id != rlGetTextureIdDefault()) {
            GenTextureMipmaps(t);
            SetTextureFilter(*t, TEXTURE_FILTER_TRILINEAR);
        }
    }
}

void descarregarModelo(Model m)
{
    for (int i = 0; i < m.materialCount; i++)
        for (int k = 0; k <= MATERIAL_MAP_BRDF; k++) {
            Texture2D t = m.materials[i].maps[k].texture;

            if (t.id != 0 && t.id != rlGetTextureIdDefault())
                UnloadTexture(t);
        }
    UnloadModel(m);
}

static void carregarDestroier(void)
{
    temDestroier = temSombraCasco = 0;
    if (!FileExists(ARQUIVO_DESTROIER)) {
        TraceLog(LOG_WARNING, "RENDER: %s ausente, Destróier em primitivas", ARQUIVO_DESTROIER);
        return;
    }
    destroier = LoadModel(ARQUIVO_DESTROIER);
    if (!IsModelValid(destroier))
        return;
    temDestroier = 1;

    /*
     * Sem o shader o modelo sai sem luz, mas aparece. Materiais emissivos (motores e luzes; o
     * conversor dá a eles uma textura emissiva e a cor do emissivo como cor base) ficam no
     * shader padrão: brilham sem depender do sol.
     */
    if (carregarShader("casco", &sombraCasco)) {
        temSombraCasco = 1;
        SetShaderValue(sombraCasco, GetShaderLocation(sombraCasco, "dirLuz"), &sol, SHADER_UNIFORM_VEC3);
        SetShaderValue(sombraCasco, GetShaderLocation(sombraCasco, "corLuz"), &COR_LUZ, SHADER_UNIFORM_VEC3);
        SetShaderValue(sombraCasco, GetShaderLocation(sombraCasco, "ambiente"), &AMBIENTE, SHADER_UNIFORM_VEC3);
        for (int i = 0; i < destroier.materialCount; i++)
            if (destroier.materials[i].maps[MATERIAL_MAP_EMISSION].texture.id == 0)
                destroier.materials[i].shader = sombraCasco;
    }
    suavizarTexturas(&destroier);
}

void carregarAssets(void)
{
    /*
     * Plano perto em 1 u (a raylib usa 0,05): a precisão do z-buffer cai com distância²/perto, e
     * nada da cena chega a menos de 1 u da câmera (canhões a 5 u, casco do TIE com raio 6).
     * Longe em PLANO_LONGE (a raylib usa 4000): o Destróier fica visível de qualquer ponto da arena.
     */
    rlSetClipPlanes(PLANO_PERTO, PLANO_LONGE);
    iniciarEspaco();
    sol = direcaoSol();
    carregarDestroier();
}

void descarregarAssets(void)
{
    if (temSombraCasco)
        UnloadShader(sombraCasco);
    if (temDestroier)
        descarregarModelo(destroier);
    temDestroier = temSombraCasco = 0;
    encerrarEspaco();
}

/* Cor da face com a normal `n` sob a luz do shader casco. */
static Color iluminar(Color base, Vector3 n)
{
    float l = fmaxf(Vector3DotProduct(n, sol), 0.0f);

    return (Color){ (unsigned char)fminf(base.r * (AMBIENTE.x + COR_LUZ.x * l), 255.0f),
                    (unsigned char)fminf(base.g * (AMBIENTE.y + COR_LUZ.y * l), 255.0f),
                    (unsigned char)fminf(base.b * (AMBIENTE.z + COR_LUZ.z * l), 255.0f), 255 };
}

/*
 * Polígono convexo plano com a frente para fora de `dentro` (ponto interno da peça), em leque.
 * Iluminado pelo sol se `acesa` = 0; com cor própria se `acesa` = 1.
 */
static void poligono(const Vector3 *v, int n, Vector3 dentro, Color cor, int acesa)
{
    Vector3 normal = Vector3Normalize(Vector3CrossProduct(Vector3Subtract(v[1], v[0]), Vector3Subtract(v[2], v[0])));
    int invertido = Vector3DotProduct(normal, Vector3Subtract(v[0], dentro)) < 0.0f;
    Color c;

    if (invertido)
        normal = Vector3Negate(normal);
    c = acesa ? cor : iluminar(cor, normal);
    /* A raylib descarta as faces de trás: anti-horário visto de fora. */
    for (int i = 1; i + 1 < n; i++) {
        if (invertido)
            DrawTriangle3D(v[0], v[i + 1], v[i], c);
        else
            DrawTriangle3D(v[0], v[i], v[i + 1], c);
    }
}

static void caixa(const Caixa *c)
{
    Vector3 a = c->min, b = c->max;
    Vector3 dentro = Vector3Scale(Vector3Add(a, b), 0.5f);
    Vector3 faces[6][4] = {
        { { a.x, a.y, a.z }, { b.x, a.y, a.z }, { b.x, b.y, a.z }, { a.x, b.y, a.z } },
        { { a.x, a.y, b.z }, { b.x, a.y, b.z }, { b.x, b.y, b.z }, { a.x, b.y, b.z } },
        { { a.x, a.y, a.z }, { a.x, b.y, a.z }, { a.x, b.y, b.z }, { a.x, a.y, b.z } },
        { { b.x, a.y, a.z }, { b.x, b.y, a.z }, { b.x, b.y, b.z }, { b.x, a.y, b.z } },
        { { a.x, a.y, a.z }, { b.x, a.y, a.z }, { b.x, a.y, b.z }, { a.x, a.y, b.z } },
        { { a.x, b.y, a.z }, { b.x, b.y, a.z }, { b.x, b.y, b.z }, { a.x, b.y, b.z } },
    };

    for (int i = 0; i < 6; i++)
        poligono(faces[i], 4, dentro, COR_CASCO, 0);
}

/* Anel do bocal na boca (z0) e entrando 1 u no casco (z1), sem fresta. */
static void anelMotor(const Motor *m, Vector3 *anel, float z)
{
    for (int k = 0; k < LADOS_MOTOR; k++) {
        float a = 2.0f * PI * k / LADOS_MOTOR;

        anel[k] = (Vector3){ m->x + m->raio * cosf(a), m->y + m->raio * sinf(a), z };
    }
}

static void desenharPrimitivas(void)
{
    const float z0 = Z_POPA_DESTROIER, z1 = Z_POPA_CASCO + 1.0f;
    /* Seção da popa: crista, bordas da parede lateral e quilha. */
    Vector3 popa[6] = {
        { 0.0f, Y_CRISTA_DESTROIER, Z_POPA_CASCO },
        { MEIA_BOCA_DESTROIER, Y_BORDA_DESTROIER, Z_POPA_CASCO },
        { MEIA_BOCA_DESTROIER, -Y_BORDA_DESTROIER, Z_POPA_CASCO },
        { 0.0f, Y_QUILHA_DESTROIER, Z_POPA_CASCO },
        { -MEIA_BOCA_DESTROIER, -Y_BORDA_DESTROIER, Z_POPA_CASCO },
        { -MEIA_BOCA_DESTROIER, Y_BORDA_DESTROIER, Z_POPA_CASCO },
    };
    Vector3 proa = { 0.0f, 0.0f, Z_PROA_DESTROIER };
    Vector3 dentro = { 0.0f, 0.0f, 0.0f };

    for (int i = 0; i < 6; i++) {
        Vector3 t[3] = { popa[i], popa[(i + 1) % 6], proa };

        poligono(t, 3, dentro, COR_CASCO, 0);
    }
    poligono(popa, 6, dentro, COR_CASCO, 0);

    for (int i = 0; i < NUM_CAIXAS; i++)
        caixa(&CAIXAS_DESTROIER[i]);

    for (int i = 0; i < NUM_MOTORES; i++) {
        const Motor *m = &MOTORES_DESTROIER[i];
        Vector3 boca[LADOS_MOTOR], fundo[LADOS_MOTOR];
        Vector3 eixo = { m->x, m->y, (z0 + z1) * 0.5f };

        anelMotor(m, boca, z0);
        anelMotor(m, fundo, z1);
        for (int k = 0; k < LADOS_MOTOR; k++) {
            int p = (k + 1) % LADOS_MOTOR;
            Vector3 q[4] = { boca[k], boca[p], fundo[p], fundo[k] };

            poligono(q, 4, eixo, COR_MOTOR, 0);
        }
        /* A boca brilha sem luz e tampa o cilindro. */
        poligono(boca, LADOS_MOTOR, (Vector3){ m->x, m->y, z0 + 1.0f }, COR_BOCAL, 1);
    }
}

static void desenharDestroier(void)
{
    if (temDestroier)
        DrawModel(destroier, (Vector3){ 0.0f, 0.0f, 0.0f }, 1.0f, WHITE);
    else
        desenharPrimitivas();
}

void render_cena(const Jogo *g, Camera3D cam)
{
    desenharFundo(&g->jogador);

    BeginMode3D(cam);
    desenharDestroier();
    /* A poeira não grava profundidade: vem depois dos objetos opacos da arena. */
    desenharPoeira(&g->jogador);
    EndMode3D();
}
