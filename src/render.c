#include "render.h"

#include <math.h>
#include <stddef.h>

#include "combate.h"
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

/*
 * Brilho dos motores: os 7 bocais reais do modelo (3 grandes no eixo e 4 pequenos em cima/embaixo),
 * medidos do material Emission1. `raio` é o raio da abertura do bocal, no tamanho canônico do modelo.
 */
typedef struct {
    float x, y, raio;
} BrilhoMotor;

#define NUM_BRILHOS 7
static const BrilhoMotor BRILHOS_MOTORES[NUM_BRILHOS] = {
    { -213.0f,   1.0f, 50.0f }, {  -1.0f,   1.0f, 50.0f }, { 211.0f,   1.0f, 50.0f },
    { -110.0f,  32.0f, 20.0f }, { -110.0f, -32.0f, 20.0f },
    {  110.0f,  32.0f, 20.0f }, {  110.0f, -32.0f, 20.0f },
};
#define TAM_TEXTURA_BRILHO 128
static Texture2D texturaBrilho;
static int temTexturaBrilho;

static const Color COR_CASCO = { 150, 152, 158, 255 };
static const Color COR_MOTOR = { 20, 20, 23, 255 };
static const Color COR_BOCAL = { 150, 190, 255, 255 };

static Model destroier;
static Shader sombraCasco;
static int temDestroier, temSombraCasco;
static Vector3 sol;

/* Modelos dos caças: X-Wing CC (assets/models/xwing.glb) e Y-Wing/A-Wing locais do BF2. */
static Model modeloXwing, modeloYwing, modeloAwing;
static int temModeloXwing, temModeloYwing, temModeloAwing;

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

static void criarTexturaBrilho(void)
{
    Image img = GenImageColor(TAM_TEXTURA_BRILHO, TAM_TEXTURA_BRILHO, BLANK);

    for (int y = 0; y < TAM_TEXTURA_BRILHO; y++) {
        for (int x = 0; x < TAM_TEXTURA_BRILHO; x++) {
            float dx = (x + 0.5f) / TAM_TEXTURA_BRILHO * 2.0f - 1.0f;
            float dy = (y + 0.5f) / TAM_TEXTURA_BRILHO * 2.0f - 1.0f;
            float d = sqrtf(dx * dx + dy * dy);
            float halo = 1.0f - d;
            float nucleo = expf(-d * d * 18.0f);
            float a;

            if (halo < 0.0f)
                halo = 0.0f;
            a = powf(halo, 2.2f) * 0.85f + nucleo;
            if (a > 1.0f)
                a = 1.0f;
            ImageDrawPixel(&img, x, y, (Color){ 255, 255, 255, (unsigned char)(255.0f * a) });
        }
    }

    texturaBrilho = LoadTextureFromImage(img);
    UnloadImage(img);
    SetTextureFilter(texturaBrilho, TEXTURE_FILTER_BILINEAR);
    temTexturaBrilho = 1;
}

/* Disco opaco do propulsor: desenhado como a tampa de um cilindro curto no plano XY da popa
 * (um billboard de 400 u sairia para trás da popa quando visto de lado). */
static void desenharDiscosMotores(void)
{
    for (int i = 0; i < NUM_BRILHOS; i++) {
        const BrilhoMotor *m = &BRILHOS_MOTORES[i];
        float x = m->x * ESCALA_DESTROIER;
        float y = m->y * ESCALA_DESTROIER;
        float r = m->raio * ESCALA_DESTROIER;
        Vector3 boca = { x, y, (Z_POPA_DESTROIER + 8.0f) * ESCALA_DESTROIER };
        Vector3 fundo = { x, y, (Z_POPA_DESTROIER + 20.0f) * ESCALA_DESTROIER };

        DrawCylinderEx(boca, fundo, r, r, 24, (Color){ 45, 115, 255, 255 });
        DrawCylinderEx((Vector3){ x, y, (Z_POPA_DESTROIER + 7.0f) * ESCALA_DESTROIER },
                       (Vector3){ x, y, (Z_POPA_DESTROIER + 9.0f) * ESCALA_DESTROIER },
                       r * 0.55f, r * 0.55f, 24, (Color){ 150, 205, 255, 255 });
    }
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

/* Carrega um GLB de caça; sem o arquivo ou com modelo inválido, cai nas primitivas. */
static int carregarModeloInimigo(const char *caminho, Model *modelo)
{
    if (!FileExists(caminho)) {
        TraceLog(LOG_WARNING, "RENDER: %s ausente, caça em primitivas", caminho);
        return 0;
    }
    *modelo = LoadModel(caminho);
    if (!IsModelValid(*modelo)) {
        TraceLog(LOG_WARNING, "RENDER: %s não carregou, caça em primitivas", caminho);
        return 0;
    }
    suavizarTexturas(modelo);
    return 1;
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
    criarTexturaBrilho();
    temModeloXwing = carregarModeloInimigo("assets/models/xwing.glb", &modeloXwing);
    temModeloYwing = carregarModeloInimigo("assets/models/bf2/ywing.glb", &modeloYwing);
    temModeloAwing = carregarModeloInimigo("assets/models/bf2/awing.glb", &modeloAwing);
}

void descarregarAssets(void)
{
    if (temSombraCasco)
        UnloadShader(sombraCasco);
    if (temTexturaBrilho) {
        UnloadTexture(texturaBrilho);
        temTexturaBrilho = 0;
        texturaBrilho.id = 0;
    }
    if (temDestroier)
        descarregarModelo(destroier);
    if (temModeloXwing)
        descarregarModelo(modeloXwing);
    if (temModeloYwing)
        descarregarModelo(modeloYwing);
    if (temModeloAwing)
        descarregarModelo(modeloAwing);
    temDestroier = temSombraCasco = 0;
    temModeloXwing = temModeloYwing = temModeloAwing = 0;
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

static void desenharGlowMotores(const Camera3D *cam)
{
    if (!temTexturaBrilho)
        return;

    /* Halo em volta do disco, somado à cena (sem gravar profundidade). */
    rlDisableDepthMask();
    BeginBlendMode(BLEND_ADDITIVE);
    for (int i = 0; i < NUM_BRILHOS; i++) {
        const BrilhoMotor *m = &BRILHOS_MOTORES[i];
        float x = m->x * ESCALA_DESTROIER;
        float y = m->y * ESCALA_DESTROIER;
        Vector3 pos;

        /* Núcleo quente logo à frente do disco. */
        pos = (Vector3){ x, y, (Z_POPA_DESTROIER - 1.0f) * ESCALA_DESTROIER };
        DrawBillboard(*cam, texturaBrilho, pos, m->raio * 1.2f * ESCALA_DESTROIER,
                      (Color){ 220, 240, 255, 200 });
        /* Halo próximo, ainda colado no disco. */
        pos.z = (Z_POPA_DESTROIER + 2.0f) * ESCALA_DESTROIER;
        DrawBillboard(*cam, texturaBrilho, pos, m->raio * 3.2f * ESCALA_DESTROIER,
                      (Color){ 85, 155, 255, 150 });
        /* Halo largo: é ele que lava o casco em volta do propulsor. */
        pos.z = (Z_POPA_DESTROIER - 5.0f) * ESCALA_DESTROIER;
        DrawBillboard(*cam, texturaBrilho, pos, m->raio * 5.6f * ESCALA_DESTROIER,
                      (Color){ 35, 95, 255, 95 });
    }
    EndBlendMode();
    rlEnableDepthMask();
}

static void desenharDestroier(const Camera3D *cam)
{
    if (temDestroier) {
        DrawModelEx(destroier, (Vector3){ 0.0f, 0.0f, 0.0f }, (Vector3){ 0.0f, 1.0f, 0.0f }, 0.0f,
                    (Vector3){ ESCALA_DESTROIER, ESCALA_DESTROIER, ESCALA_DESTROIER }, WHITE);
        desenharDiscosMotores();
        desenharGlowMotores(cam);
    } else {
        rlPushMatrix();
        rlScalef(ESCALA_DESTROIER, ESCALA_DESTROIER, ESCALA_DESTROIER);
        desenharPrimitivas();
        rlPopMatrix();
    }
}

/* Primitivas em coordenadas locais: frente +Z, cima +Y e raio próximo de 1.
 * A escala de cada caça acompanha seu raio de colisão. */
static void desenharXWing(void)
{
    Color cor = {235, 105, 80, 255};
    DrawCylinderEx((Vector3){0, 0, -0.5f}, (Vector3){0, 0, 0.8f},
                   0.16f, 0.04f, 8, LIGHTGRAY);
    DrawCube((Vector3){0, 0.13f, 0.05f}, 0.17f, 0.12f, 0.32f, DARKBLUE);
    for (int x = -1; x <= 1; x += 2) {
        for (int y = -1; y <= 1; y += 2) {
            Vector3 ponta = {x * 0.72f, y * 0.35f, -0.2f};
            DrawCylinderEx((Vector3){0, 0, -0.2f}, ponta, 0.09f, 0.06f, 4, cor);
            DrawCylinderEx((Vector3){ponta.x, ponta.y, -0.45f},
                           (Vector3){ponta.x, ponta.y, 0.4f},
                           0.045f, 0.025f, 6, LIGHTGRAY);
        }
    }
}

static void desenharYWing(void)
{
    Color cor = {245, 195, 65, 255};
    DrawCube((Vector3){0, 0, 0.35f}, 0.36f, 0.22f, 0.65f, cor);
    DrawCube((Vector3){0, 0.14f, 0.4f}, 0.22f, 0.1f, 0.25f, DARKBLUE);
    DrawCube((Vector3){0, 0, -0.15f}, 1.1f, 0.12f, 0.18f, LIGHTGRAY);
    for (int lado = -1; lado <= 1; lado += 2) {
        float x = lado * 0.52f;
        DrawCylinderEx((Vector3){x, 0, -0.7f}, (Vector3){x, 0, 0.35f},
                       0.14f, 0.14f, 8, cor);
        DrawSphereEx((Vector3){x, 0, -0.7f}, 0.1f, 4, 8, SKYBLUE);
    }
}

static void desenharAWing(void)
{
    Color cor = {85, 195, 130, 255};
    /* Cone achatado: silhueta de cunha, com a ponta voltada para +Z. */
    rlPushMatrix();
    rlScalef(1.0f, 0.35f, 1.0f);
    DrawCylinderEx((Vector3){0, 0, -0.5f}, (Vector3){0, 0, 0.85f},
                   0.55f, 0.0f, 4, cor);
    rlPopMatrix();
    DrawCube((Vector3){0, 0.12f, -0.05f}, 0.2f, 0.15f, 0.35f, DARKBLUE);
    for (int lado = -1; lado <= 1; lado += 2) {
        float x = lado * 0.4f;
        DrawCylinderEx((Vector3){x, 0, -0.55f}, (Vector3){x, 0, 0.25f},
                       0.07f, 0.04f, 6, LIGHTGRAY);
    }
}

static void desenharInimigo(const Inimigo *inimigo)
{
    Vector3 frente = {0, 0, 1};
    if (Vector3LengthSqr(inimigo->vel) > 0.000001f)
        frente = Vector3Normalize(inimigo->vel);

    /* Rotação de +Z para a velocidade. O eixo Y resolve também a direção -Z;
     * parado, o caça conserva a orientação padrão, sem normalizar vetor zero. */
    Vector3 eixo = {-frente.y, frente.x, 0};
    if (Vector3LengthSqr(eixo) < 0.000001f)
        eixo = (Vector3){0, 1, 0};
    float angulo = acosf(Clamp(frente.z, -1.0f, 1.0f)) * RAD2DEG;
    float raio = raioInimigo(inimigo->tipo);

    rlPushMatrix();
    rlTranslatef(inimigo->pos.x, inimigo->pos.y, inimigo->pos.z);
    rlRotatef(angulo, eixo.x, eixo.y, eixo.z);
    switch (inimigo->tipo) {
        case XWING:
            if (temModeloXwing) {
                DrawModel(modeloXwing, (Vector3){0}, 1.0f, WHITE);
            } else {
                rlScalef(raio, raio, raio);
                desenharXWing();
            }
            break;
        case YWING:
            if (temModeloYwing) {
                DrawModel(modeloYwing, (Vector3){0}, 1.0f, WHITE);
            } else {
                rlScalef(raio, raio, raio);
                desenharYWing();
            }
            break;
        case AWING:
            if (temModeloAwing) {
                DrawModel(modeloAwing, (Vector3){0}, 1.0f, WHITE);
            } else {
                rlScalef(raio, raio, raio);
                desenharAWing();
            }
            break;
    }
    rlPopMatrix();
}

static void desenharLasers(const Jogo *g)
{
    /* A posição da simulação é a ponta do tiro; a cauda fica atrás dela.
     * Comprimento visual fixo, independente da velocidade e do frame. */
    const float comprimento = 12.0f;
    const float raio = 0.22f;
    for (int i = 0; i < MAX_LASERS; i++) {
        const Laser *laser = &g->lasers[i];
        if (!laser->ativo)
            continue;
        Color cor = laser->doJogador ? GREEN : RED; /* VIS-04: TIE imperial verde, rebeldes vermelhos. */
        if (Vector3LengthSqr(laser->vel) < 0.000001f) {
            DrawSphereEx(laser->pos, raio, 4, 6, cor);
            continue;
        }
        Vector3 cauda = Vector3Subtract(laser->pos,
            Vector3Scale(Vector3Normalize(laser->vel), comprimento));
        DrawCylinderEx(cauda, laser->pos, raio, raio, 6, cor);
    }
}

void render_cena(const Jogo *g, Camera3D cam)
{
    desenharFundo(&g->jogador);

    BeginMode3D(cam);
    desenharDestroier(&cam);
    for (const NoInimigo *no = g->inimigos.inicio; no != NULL; no = no->prox)
        desenharInimigo(&no->dado);
    desenharLasers(g);
    /* A poeira não grava profundidade: vem depois dos objetos opacos da arena. */
    desenharPoeira(&g->jogador);
    EndMode3D();
}
