#include "hud.h"

#include <math.h>
#include <stddef.h>
#include <stdio.h>

#include "raymath.h"
#include "combate.h"
#include "aliados.h"
#include "destroier.h"
#include "ia.h"
#include "jogo.h"
#include "nave.h"
#include "pontuacao.h"
#include "render.h"

#define FONTE_HUD     "assets/fonts/ChakraPetch-Bold.ttf"
#define TAM_BASE_HUD  32      /* tamanho rasterizado; os textos menores escalam a partir dele */
#define ESPACO_FONTE  1.0f

/*
 * Cabine do TIE em 2D sobre a cena, com o vidro ocupando quase a tela toda: janela central
 * octogonal (lados de cima e de baixo retos) e uma escora escura de cada vértice até a parede
 * circular, que só aparece nos cantos. Os instrumentos ficam no painel, abaixo da janela central.
 * A retícula fica no centro da tela (eixo da câmera), dentro do vidro.
 */
#define JANELA_X      640.0f
#define JANELA_Y      330.0f
#define RAIO_PAREDE   700.0f   /* borda interna da parede; a tela inteira cabe em ~750 */
#define RAIO_VIDRO    240.0f   /* vértices da janela central */
#define LADOS_VIDRO   8
#define RAIO_FORA     1000.0f  /* cobre os cantos da tela */

#define LARGURA_ESCORA 22.0f
#define LARGURA_ARO    18.0f

/* Retícula: anel central com quatro traços. O raio do anel é o ângulo TOLERANCIA_MIRA na tela. */
#define RETICULA_VAO    22
#define RETICULA_TRACO  18

/*
 * Instrumentos montados na parte de baixo do cockpit. As bordas externas são trapézios e os
 * painéis têm uma face lateral extruda, para a HUD 2D parecer presa ao painel em perspectiva.
 */
#define CENTRO_TOPO     398.0f  /* topo do console central; acima disso fica o vidro */
#define COMBATE_X       640.0f
#define COMBATE_Y       596.0f
#define COMBATE_RAIO     90.0f

#define SENSORES_CX     290.0f
#define SENSORES_CY     528.0f
#define RADAR_RAIO       76.0f

#define STATUS_CX       990.0f
#define STATUS_CY       525.0f

#define FRACAO_ALERTA   0.5f   /* abaixo: amarelo */
#define FRACAO_CRITICA  0.25f  /* abaixo: vermelho piscando */
#define BORDA_PERTO     0.1f   /* fração do raio da arena a partir da qual a distância à borda fica vermelha */
#define TEMPO_CRITICO   30     /* s finais com o relógio em vermelho */
#define PISCA_PERIODO   0.5    /* s de um ciclo aceso/apagado */

/* Estrutura da cabine: metal escuro, com fio de luz numa borda. */
#define COR_PAREDE   (Color){ 20, 21, 25, 255 }
#define COR_ESCORA   (Color){ 27, 28, 33, 255 }
#define COR_BRILHO   (Color){ 72, 75, 84, 255 }
#define COR_PAINEL   (Color){ 18, 19, 23, 255 }
#define COR_TELA     (Color){ 10, 14, 32, 255 }
#define COR_NEON     (Color){ 70, 100, 235, 255 }
#define COR_NEON_VERMELHO (Color){ 170, 30, 40, 255 }

/* Instrumentos. */
#define COR_ROTULO    (Color){ 100, 150, 255, 255 }
#define COR_VALOR     (Color){ 215, 230, 255, 255 }
#define COR_HUD       (Color){ 120, 230, 140, 255 }
#define COR_TIE       (Color){ 60, 220, 90, 255 }
#define COR_ESCUDO    (Color){ 120, 225, 255, 255 }
#define COR_ALERTA    (Color){ 240, 200, 40, 255 }
#define COR_CRITICA   (Color){ 235, 50, 40, 255 }
#define COR_ALVO      (Color){ 255, 120, 60, 255 }
#define COR_MODELO    (Color){ 200, 228, 255, 255 }

/* Fundo dos instrumentos: vidro azul-escuro, com metal em volta. */
#define COR_VIDRO     (Color){ 8, 13, 30, 248 }
#define COR_VIDRO_2   (Color){ 14, 26, 56, 255 }
#define COR_METAL     (Color){ 24, 26, 34, 255 }
#define COR_METAL_2   (Color){ 17, 20, 29, 255 }

/*
 * Alvo travado. No vidro: colchetes em volta do caça, com tipo e distância, e o indicador de tiro
 * adiantado (onde mirar para o laser encontrar o caça); fora do vidro, uma seta em volta da retícula.
 */
#define MARCA_MIN       14.0f   /* meia largura mínima dos colchetes (px) */
#define MARCA_ESCALA    1.6f    /* colchetes um pouco maiores que o caça */
#define MARGEM_VIDRO    20.0f   /* px; mais perto da borda da tela, vira seta */
#define RAIO_SETA       200.0f  /* círculo em torno da retícula onde fica a seta */
#define PIPPER_RAIO     7.0f

static Font fonteHud;

void hud_carregar(void)
{
    static int pontos[0x5F + 0x60]; /* ASCII e Latin-1 (acentos) */
    int quantidade = 0;
    int c;

    for (c = 0x20; c <= 0x7E; c++)
        pontos[quantidade++] = c;
    for (c = 0xA0; c <= 0xFF; c++)
        if (c != 0xB5) /* micro sign não existe na Chakra Petch */
            pontos[quantidade++] = c;

    fonteHud = LoadFontEx(FONTE_HUD, TAM_BASE_HUD, pontos, quantidade);
    if (fonteHud.texture.id != 0)
        SetTextureFilter(fonteHud.texture, TEXTURE_FILTER_BILINEAR);
}

void hud_descarregar(void)
{
    if (fonteHud.texture.id != 0)
        UnloadFont(fonteHud);
    fonteHud = (Font){ 0 };
}

static Font fonte(void)
{
    return fonteHud.texture.id != 0 ? fonteHud : GetFontDefault();
}

/* `ancora` em [0, 1]: 0 alinha à esquerda de x, 0.5 centraliza, 1 alinha à direita. */
static void texto(const char *t, int x, int y, int tam, float ancora, Color cor)
{
    float largura = MeasureTextEx(fonte(), t, (float)tam, ESPACO_FONTE).x;
    Vector2 p = { (float)x - largura * ancora, (float)y };

    DrawTextEx(fonte(), t, (Vector2){ p.x + 1.0f, p.y + 1.0f }, (float)tam, ESPACO_FONTE, Fade(BLACK, 0.8f));
    DrawTextEx(fonte(), t, p, (float)tam, ESPACO_FONTE, cor);
}

/* Texto que diminui até caber numa largura; evita avisos longos estourarem o painel. */
static void textoAjustado(const char *t, int x, int y, int tamMax, int larguraMax, float ancora, Color cor)
{
    int tam = tamMax;

    while (tam > 9 && MeasureTextEx(fonte(), t, (float)tam, ESPACO_FONTE).x > larguraMax)
        tam--;
    texto(t, x, y, tam, ancora, cor);
}

static float fracao(float valor, float maximo)
{
    float f = valor / maximo;

    return f < 0.0f ? 0.0f : (f > 1.0f ? 1.0f : f);
}

static int piscaAceso(void)
{
    return fmod(GetTime(), PISCA_PERIODO) < PISCA_PERIODO / 2.0;
}

/* Cor cheia, amarelo abaixo de FRACAO_ALERTA e vermelho piscando abaixo de FRACAO_CRITICA. */
static Color corDoNivel(float f, Color cheia)
{
    if (f < FRACAO_CRITICA)
        return piscaAceso() ? COR_CRITICA : Fade(COR_CRITICA, 0.4f);
    return f < FRACAO_ALERTA ? COR_ALERTA : cheia;
}

/* Com o eixo y para baixo, a raylib só desenha triângulos com produto vetorial negativo. */
static void triangulo(Vector2 a, Vector2 b, Vector2 c, Color cor)
{
    float giro = (b.x - a.x) * (c.y - a.y) - (b.y - a.y) * (c.x - a.x);

    if (giro > 0.0f)
        DrawTriangle(a, c, b, cor);
    else
        DrawTriangle(a, b, c, cor);
}

/* Polígono convexo em leque a partir do primeiro vértice. */
static void poligono(const Vector2 *p, int n, Color cor)
{
    int i;

    for (i = 1; i + 1 < n; i++)
        triangulo(p[0], p[i], p[i + 1], cor);
}

static void contorno(const Vector2 *p, int n, float espessura, Color cor)
{
    int i;

    for (i = 0; i < n; i++)
        DrawLineEx(p[i], p[(i + 1) % n], espessura, cor);
}

static Vector2 verticeVidro(int i)
{
    float ang = (-112.5f + 360.0f * i / LADOS_VIDRO) * DEG2RAD;

    return (Vector2){ JANELA_X + cosf(ang) * RAIO_VIDRO, JANELA_Y + sinf(ang) * RAIO_VIDRO };
}

/* Escora escura com um fio de luz do lado de cima. */
static void desenharEscora(Vector2 a, Vector2 b)
{
    Vector2 dir = Vector2Normalize(Vector2Subtract(b, a));
    Vector2 normal = { -dir.y, dir.x };
    Vector2 luz = Vector2Scale(normal, normal.y < 0.0f ? 8.0f : -8.0f);

    DrawLineEx(a, b, LARGURA_ESCORA, COR_ESCORA);
    DrawLineEx(Vector2Add(a, luz), Vector2Add(b, luz), 2.0f, COR_BRILHO);
}

static void desenharEstrutura(void)
{
    Vector2 centro = { JANELA_X, JANELA_Y };
    Vector2 v[LADOS_VIDRO];
    int i;

    DrawRing(centro, RAIO_PAREDE, RAIO_FORA, 0.0f, 360.0f, 128, COR_PAREDE);
    DrawRing(centro, RAIO_PAREDE, RAIO_PAREDE + 3.0f, 0.0f, 360.0f, 128, COR_BRILHO);

    for (i = 0; i < LADOS_VIDRO; i++)
        v[i] = verticeVidro(i);

    for (i = 0; i < LADOS_VIDRO; i++) {
        Vector2 dir = Vector2Normalize(Vector2Subtract(v[i], centro));

        desenharEscora(v[i], Vector2Add(centro, Vector2Scale(dir, RAIO_PAREDE + 10.0f)));
    }

    contorno(v, LADOS_VIDRO, LARGURA_ARO, COR_ESCORA);
    for (i = 0; i < LADOS_VIDRO; i++) {
        Vector2 a = Vector2MoveTowards(v[i], centro, 8.0f);
        Vector2 b = Vector2MoveTowards(v[(i + 1) % LADOS_VIDRO], centro, 8.0f);

        DrawLineEx(a, b, 2.0f, COR_BRILHO);
        DrawCircleV(v[i], 12.0f, COR_ESCORA);
    }
}

/* Pixels por unidade a 1 u de distância, no centro da tela. */
static float distanciaFocal(void)
{
    return (ALTURA_TELA * 0.5f) / tanf(FOV_JOGADOR * 0.5f * DEG2RAD);
}

static void desenharReticula(void)
{
    int cx = LARGURA_TELA / 2;
    int cy = ALTURA_TELA / 2;
    Color cor = Fade(COR_HUD, 0.85f);

    DrawCircleLines(cx, cy, distanciaFocal() * TOLERANCIA_MIRA, cor);
    DrawCircle(cx, cy, 2.0f, cor);
    DrawLine(cx - RETICULA_VAO - RETICULA_TRACO, cy, cx - RETICULA_VAO, cy, cor);
    DrawLine(cx + RETICULA_VAO, cy, cx + RETICULA_VAO + RETICULA_TRACO, cy, cor);
    DrawLine(cx, cy - RETICULA_VAO - RETICULA_TRACO / 2, cx, cy - RETICULA_VAO, cor);
    DrawLine(cx, cy + RETICULA_VAO, cx, cy + RETICULA_VAO + RETICULA_TRACO / 2, cor);
}

static const char *nomeDoTipo(TipoNave t)
{
    switch (t) {
    case YWING: return "Y-WING";
    case AWING: return "A-WING";
    case XWING:
    default:    return "X-WING";
    }
}

/* Ponto no espaço da nave: x para a direita, y para cima, z para a frente. */
static Vector3 noEspacoDaNave(const Jogador *j, Vector3 p)
{
    Vector3 rel = Vector3Subtract(p, j->pos);

    return (Vector3){ Vector3DotProduct(rel, j->direita), Vector3DotProduct(rel, j->cima),
                      Vector3DotProduct(rel, j->frente) };
}

/* Projeta `p` na tela; 0 se está atrás da nave ou fora dos limites da tela. */
static int naTela(const Jogo *g, Vector3 p, Vector2 *tela)
{
    const Jogador *j = &g->jogador;

    if (noEspacoDaNave(j, p).z < 1.0f)
        return 0;
    *tela = GetWorldToScreenEx(p, cameraComTremor(g), LARGURA_TELA, ALTURA_TELA);
    return tela->x >= MARGEM_VIDRO && tela->x <= LARGURA_TELA - MARGEM_VIDRO
        && tela->y >= MARGEM_VIDRO && tela->y <= ALTURA_TELA - MARGEM_VIDRO;
}

static void colchetes(Vector2 c, float m, Color cor)
{
    float traco = m * 0.5f;

    for (int sx = -1; sx <= 1; sx += 2)
        for (int sy = -1; sy <= 1; sy += 2) {
            Vector2 canto = { c.x + sx * m, c.y + sy * m };

            DrawLineEx(canto, (Vector2){ canto.x - sx * traco, canto.y }, 2.0f, cor);
            DrawLineEx(canto, (Vector2){ canto.x, canto.y - sy * traco }, 2.0f, cor);
        }
}

/* Seta em volta da retícula, apontando para o alvo fora do vidro, com a distância. */
static void setaParaOAlvo(const Jogador *j, const Inimigo *alvo, float dist)
{
    Vector3 local = noEspacoDaNave(j, alvo->pos);
    Vector2 dir = { local.x, -local.y };
    Vector2 centro = { LARGURA_TELA / 2.0f, ALTURA_TELA / 2.0f };
    Vector2 p, lado;

    dir = Vector2Length(dir) > 1e-3f ? Vector2Normalize(dir) : (Vector2){ 0.0f, 1.0f };
    p = Vector2Add(centro, Vector2Scale(dir, RAIO_SETA));
    lado = (Vector2){ -dir.y * 9.0f, dir.x * 9.0f };
    triangulo(Vector2Add(p, Vector2Scale(dir, 12.0f)),
              Vector2Add(Vector2Subtract(p, Vector2Scale(dir, 6.0f)), lado),
              Vector2Subtract(Vector2Subtract(p, Vector2Scale(dir, 6.0f)), lado), COR_ALVO);
    texto(TextFormat("%d u", (int)lroundf(dist)), (int)(p.x - dir.x * 26.0f), (int)(p.y - dir.y * 26.0f) - 8,
          14, 0.5f, COR_ALVO);
}

/*
 * Alvo travado no vidro. O indicador de tiro adiantado fica onde o caça estará quando o laser
 * chegar, se ele voar reto; fica verde quando tiroAcerta, o mesmo teste da colisão do laser.
 */
static void desenharAlvoNoVidro(const Jogo *g)
{
    const Jogador *j = &g->jogador;
    const Inimigo *alvo;
    Vector2 tela, pipper;
    float dist, focal, m;
    Vector3 adiantado;

    if (g->inimigos.alvo == NULL)
        return;
    alvo = &g->inimigos.alvo->dado;
    dist = Vector3Distance(alvo->pos, j->pos);

    if (!naTela(g, alvo->pos, &tela)) {
        setaParaOAlvo(j, alvo, dist);
        return;
    }

    focal = distanciaFocal();
    m = fmaxf(MARCA_MIN, raioInimigo(alvo->tipo) * MARCA_ESCALA * focal / noEspacoDaNave(j, alvo->pos).z);
    colchetes(tela, m, COR_ALVO);
    texto(TextFormat("%s  %d u", nomeDoTipo(alvo->tipo), (int)lroundf(dist)), (int)tela.x, (int)(tela.y + m + 4.0f),
          14, 0.5f, COR_ALVO);

    if (dist > VEL_LASER * VIDA_LASER)
        return;
    adiantado = pontoDeInterceptacao(j->pos, VEL_LASER, alvo->pos, alvo->vel);
    if (naTela(g, adiantado, &pipper)) {
        int naMira = tiroAcerta(j, alvo);
        Color cor = naMira ? COR_HUD : COR_ALVO;

        DrawLineEx(tela, pipper, 1.0f, Fade(cor, 0.5f));
        DrawCircleLinesV(pipper, PIPPER_RAIO, cor);
        if (naMira)
            DrawCircleV(pipper, PIPPER_RAIO - 3.0f, cor);
    }
}

static Color corDoTipo(TipoNave t)
{
    switch (t) {
    case YWING: return COR_YWING;
    case AWING: return COR_AWING;
    case XWING:
    default:    return COR_XWING;
    }
}

/* --------------------------------------------------------------------------------------------
 * Painéis com face lateral extruda (2D simulando 3D)
 * ----------------------------------------------------------------------------------------- */

static Vector2 centroDoPoligono(const Vector2 *p, int n)
{
    Vector2 c = { 0.0f, 0.0f };
    int i;

    for (i = 0; i < n; i++)
        c = Vector2Add(c, p[i]);
    return Vector2Scale(c, 1.0f / (float)n);
}

static void poligonoDeslocado(const Vector2 *p, int n, float dx, float dy, Color cor)
{
    Vector2 q[8];
    int i;

    for (i = 0; i < n; i++)
        q[i] = (Vector2){ p[i].x + dx, p[i].y + dy };
    poligono(q, n, cor);
}

/*
 * Face frontal escura, bisel neon e uma lateral mais escura deslocada para baixo/direita.
 * `ladoAcento`: -1 destaca as bordas esquerdas, +1 as direitas, 2 nenhum destaque.
 */
static void desenharPainel3D(const Vector2 *p, int n, Color borda, int ladoAcento, float dx, float dy)
{
    Vector2 centro = centroDoPoligono(p, n);
    Vector2 dentro[8];
    int i;

    /* Sombra e lateral do painel. */
    poligonoDeslocado(p, n, dx + 5.0f, dy + 6.0f, Fade(BLACK, 0.55f));
    poligonoDeslocado(p, n, dx, dy, COR_METAL);
    poligonoDeslocado(p, n, dx, dy - 2.0f, (Color){ 38, 41, 52, 255 });

    /* Face frontal. */
    contorno(p, n, 12.0f, Fade(borda, 0.16f));
    poligono(p, n, COR_METAL_2);
    contorno(p, n, 2.5f, (Color){ 52, 57, 70, 255 });
    for (i = 0; i < n; i++)
        dentro[i] = Vector2MoveTowards(p[i], centro, 8.0f);
    poligono(dentro, n, COR_VIDRO);
    contorno(dentro, n, 2.2f, borda);

    /* Fio de luz na aresta de cima. */
    DrawLineEx(p[0], p[1], 1.0f, Fade(WHITE, 0.22f));

    /* Fio vermelho no lado externo, como nos instrumentos da referência. */
    if (ladoAcento < 0) {
        DrawLineEx(p[n - 1], p[0], 3.0f, COR_NEON_VERMELHO);
        DrawLineEx(p[n - 2], p[n - 1], 3.0f, COR_NEON_VERMELHO);
    } else if (ladoAcento > 0) {
        DrawLineEx(p[1], p[2], 3.0f, COR_NEON_VERMELHO);
        DrawLineEx(p[2], p[3], 3.0f, COR_NEON_VERMELHO);
    }

    if (n == 6) {
        DrawCircleV(p[0], 2.5f, COR_BRILHO);
        DrawCircleV(p[1], 2.5f, COR_BRILHO);
    }
}

/* Braço de metal que prende o painel lateral ao assoalho do cockpit. */
static void desenharSuporte(Vector2 a, Vector2 b)
{
    DrawLineEx(Vector2Add(a, (Vector2){ 4.0f, 5.0f }), Vector2Add(b, (Vector2){ 4.0f, 5.0f }),
               15.0f, Fade(BLACK, 0.4f));
    DrawLineEx(a, b, 13.0f, COR_METAL);
    DrawLineEx(Vector2Add(a, (Vector2){ -3.0f, 0.0f }), Vector2Add(b, (Vector2){ -3.0f, 0.0f }),
               2.0f, Fade(COR_BRILHO, 0.8f));
}

/* Antenas curtas no topo do console central. */
static void desenharAntenas(void)
{
    const float x[] = { 562.0f, 588.0f, 614.0f, 668.0f, 694.0f, 720.0f };
    const float h[] = { 15.0f, 25.0f, 10.0f, 19.0f, 27.0f, 13.0f };
    int i;

    for (i = 0; i < 6; i++) {
        DrawLineEx((Vector2){ x[i], CENTRO_TOPO }, (Vector2){ x[i], CENTRO_TOPO - h[i] },
                   4.0f, COR_ESCORA);
        DrawLineEx((Vector2){ x[i] - 1.0f, CENTRO_TOPO }, (Vector2){ x[i] - 1.0f, CENTRO_TOPO - h[i] },
                   1.0f, COR_BRILHO);
    }
}

/* --------------------------------------------------------------------------------------------
 * Sensores: radar redondo no painel triangular
 * ----------------------------------------------------------------------------------------- */

/* Posição de `p` no radar; `atras` = 1 se ele está a mais de 90° da frente da nave. */
static Vector2 noRadar(const Jogador *j, Vector3 p, int *atras, Vector2 centro, float raio)
{
    Vector3 local = Vector3Normalize(noEspacoDaNave(j, p));
    float ang = acosf(Clamp(local.z, -1.0f, 1.0f));
    Vector2 dir = { local.x, -local.y };

    /* Bem à frente o raio é ~0; bem atrás, a direção some e o ponto vai para a borda de baixo. */
    dir = Vector2Length(dir) > 1e-3f ? Vector2Normalize(dir) : (Vector2){ 0.0f, 1.0f };
    *atras = ang > PI * 0.5f;
    return Vector2Add(centro, Vector2Scale(dir, ang / PI * raio));
}

static void desenharRadar(const Jogo *g, Vector2 c, float raio)
{
    const Jogador *j = &g->jogador;
    const NoInimigo *alvo = g->inimigos.alvo;
    float varredura = fmodf((float)GetTime() * 1.4f, 2.0f * PI);
    int atras;
    int i;

    DrawCircleV(c, raio + 4.0f, COR_METAL);
    DrawCircleGradient(c, raio, COR_VIDRO_2, (Color){ 4, 7, 16, 255 });
    DrawCircleLinesV(c, raio, Fade(COR_NEON, 0.9f));

    /* Anéis e cruz discretos. */
    DrawCircleLinesV(c, raio * 0.66f, Fade(COR_NEON, 0.22f));
    DrawCircleLinesV(c, raio * 0.33f, Fade(COR_NEON, 0.22f));
    DrawLineV((Vector2){ c.x - raio, c.y }, (Vector2){ c.x + raio, c.y }, Fade(COR_NEON, 0.20f));
    DrawLineV((Vector2){ c.x, c.y - raio }, (Vector2){ c.x, c.y + raio }, Fade(COR_NEON, 0.20f));

    /* Graduação curta na borda. */
    for (i = 0; i < 24; i++) {
        float ang = i * 15.0f * DEG2RAD;
        float r1 = raio - ((i % 2 == 0) ? 6.0f : 3.0f);
        Vector2 a = { c.x + cosf(ang) * r1, c.y + sinf(ang) * r1 };
        Vector2 b = { c.x + cosf(ang) * raio, c.y + sinf(ang) * raio };

        DrawLineV(a, b, Fade(COR_NEON, (i % 2 == 0) ? 0.55f : 0.25f));
    }

    /* Varredura fina, com um rastro curto. */
    for (i = 0; i < 4; i++) {
        float ang = varredura - i * 0.07f;
        Vector2 fim = { c.x + cosf(ang) * (raio - 3.0f), c.y + sinf(ang) * (raio - 3.0f) };

        DrawLineEx(c, fim, (i == 0) ? 1.5f : 1.0f, Fade(COR_ESCUDO, 0.55f - i * 0.12f));
    }

    DrawCircleV(c, 2.0f, COR_HUD);

    /* Aliados são cruzes: verde na defesa, ciano na interceptação. */
    for (i = 0; i < MAX_ALIADOS; i++) {
        const Aliado *a = &g->aliados[i];
        Vector2 p;
        Color cor;

        if (!a->ativo)
            continue;
        p = noRadar(j, a->pos, &atras, c, raio - 6.0f);
        cor = Fade(a->papel == ALIADO_DEFENDER ? COR_TIE : COR_ESCUDO, atras ? 0.45f : 1.0f);
        DrawLineV((Vector2){ p.x - 3, p.y }, (Vector2){ p.x + 3, p.y }, cor);
        DrawLineV((Vector2){ p.x, p.y - 3 }, (Vector2){ p.x, p.y + 3 }, cor);
    }

    for (const NoInimigo *no = g->inimigos.inicio; no != NULL; no = no->prox) {
        Vector2 p;

        if (no == alvo)
            continue;
        p = noRadar(j, no->dado.pos, &atras, c, raio - 6.0f);
        DrawCircleV(p, 2.5f, Fade(corDoTipo(no->dado.tipo), atras ? 0.45f : 1.0f));
    }

    if (alvo != NULL) {
        Vector2 p = noRadar(j, alvo->dado.pos, &atras, c, raio - 6.0f);

        if (!atras || piscaAceso()) {
            DrawCircleV(p, 3.5f, corDoTipo(alvo->dado.tipo));
            DrawRing(p, 5.0f, 7.0f, 0.0f, 360.0f, 16, COR_ALVO);
        }
    }
}

static void desenharSensores(const Jogo *g)
{
    const Vector2 corpo[6] = {
        { 185.0f, 410.0f }, { 380.0f, 395.0f }, { 395.0f, 480.0f },
        { 370.0f, 645.0f }, { 210.0f, 655.0f }, { 185.0f, 480.0f },
    };

    desenharPainel3D(corpo, 6, COR_NEON, -1, 8.0f, 10.0f);
    desenharRadar(g, (Vector2){ SENSORES_CX, SENSORES_CY }, RADAR_RAIO);
    DrawCircleV((Vector2){ 367.0f, 626.0f }, 3.0f, COR_ALERTA);
    DrawCircleV((Vector2){ 354.0f, 638.0f }, 3.0f, Fade(COR_NEON_VERMELHO, 0.95f));
}

/* --------------------------------------------------------------------------------------------
 * Velocidade / throttle
 * ----------------------------------------------------------------------------------------- */

static void desenharVelocidade(const Jogo *g)
{
    const Vector2 corpo[4] = {
        { 432.0f, 500.0f }, { 500.0f, 492.0f }, { 506.0f, 710.0f }, { 442.0f, 718.0f },
    };
    float f = fracao(g->jogador.velocidade - VEL_MIN, VEL_MAX - VEL_MIN);
    Rectangle cont = { 454.0f, 524.0f, 17.0f, 108.0f };
    Rectangle seg = { 477.0f, 524.0f, 13.0f, 108.0f };
    int acesos = (int)ceilf(f * 12.0f);
    float altura = (cont.height - 4.0f) * f;
    int i;

    desenharPainel3D(corpo, 4, COR_NEON, -1, 7.0f, 9.0f);
    texto("VEL", 471, 507, 10, 0.5f, Fade(COR_ROTULO, 0.8f));

    /* Barra contínua, enchendo de baixo para cima. */
    DrawRectangleRec(cont, (Color){ 5, 9, 22, 255 });
    DrawRectangleLinesEx(cont, 1.0f, Fade(COR_NEON, 0.45f));
    if (altura > 0.0f) {
        Rectangle cheio = { cont.x + 2.0f, cont.y + cont.height - 2.0f - altura, cont.width - 4.0f, altura };

        DrawRectangleGradientV((int)cheio.x, (int)cheio.y, (int)cheio.width, (int)cheio.height,
                               COR_ESCUDO, COR_NEON);
        DrawLineEx((Vector2){ cheio.x, cheio.y }, (Vector2){ cheio.x + cheio.width, cheio.y }, 2.0f, WHITE);
    }
    for (i = 1; i < 8; i++) {
        float y = cont.y + cont.height * i / 8.0f;

        DrawLineV((Vector2){ cont.x, y }, (Vector2){ cont.x + cont.width, y }, Fade(BLACK, 0.35f));
    }

    /* Pista segmentada à direita, como na referência. */
    DrawRectangleRec(seg, (Color){ 5, 9, 22, 255 });
    DrawRectangleLinesEx(seg, 1.0f, Fade(COR_NEON, 0.45f));
    for (i = 0; i < 12; i++) {
        float passo = (seg.height - 4.0f) / 12.0f;
        Rectangle s = { seg.x + 2.0f, seg.y + 2.0f + passo * (11 - i), seg.width - 4.0f, passo - 2.0f };

        DrawRectangleRec(s, i < acesos ? COR_ESCUDO : (Color){ 25, 48, 95, 170 });
    }

    textoAjustado(TextFormat("%d", (int)lroundf(g->jogador.velocidade)), 471, 644, 30, 64, 0.5f, COR_ESCUDO);
}

/* --------------------------------------------------------------------------------------------
 * Modelos em line art
 * ----------------------------------------------------------------------------------------- */

/* TIE bem simples: bola no meio e dois traços verticais nas laterais, ligados por dois tirantes. */
static void desenharModeloTie(Vector2 c, float e, Color cor)
{
    float x = 34.0f * e;
    float y = 30.0f * e;
    float bola = 14.0f * e;
    int lado;

    for (lado = -1; lado <= 1; lado += 2) {
        DrawLineEx((Vector2){ c.x + lado * x, c.y - y }, (Vector2){ c.x + lado * x, c.y + y },
                   7.0f * e, Fade(cor, 0.16f));
        DrawLineEx((Vector2){ c.x + lado * x, c.y - y }, (Vector2){ c.x + lado * x, c.y + y },
                   4.0f * e, cor);

        DrawLineEx((Vector2){ c.x + lado * bola, c.y - 5.0f * e },
                   (Vector2){ c.x + lado * x, c.y - 5.0f * e }, 2.0f * e, cor);
        DrawLineEx((Vector2){ c.x + lado * bola, c.y + 5.0f * e },
                   (Vector2){ c.x + lado * x, c.y + 5.0f * e }, 2.0f * e, cor);
    }

    DrawCircleV(c, bola - 2.0f * e, COR_TELA);
    DrawRing(c, bola - 3.5f * e, bola, 0.0f, 360.0f, 40, cor);
    DrawRing(c, bola, bola + 2.5f * e, 0.0f, 360.0f, 40, Fade(cor, 0.16f));
}

/* Silhueta do alvo: o X-Wing é literalmente um X, como na referência. */
static void desenharModeloAlvo(TipoNave tipo, Vector2 c, float e, Color cor)
{
    if (tipo == YWING) {
        /* Fuselagem comprida, cabine à frente e dois motores laterais. */
        DrawLineEx((Vector2){ c.x, c.y - 32.0f * e }, (Vector2){ c.x, c.y + 30.0f * e }, 3.0f * e, cor);
        DrawCircleLinesV((Vector2){ c.x, c.y - 17.0f * e }, 5.0f * e, cor);
        DrawCircleV((Vector2){ c.x, c.y - 17.0f * e }, 1.5f * e, cor);
        for (int lado = -1; lado <= 1; lado += 2) {
            float x = c.x + lado * 21.0f * e;

            DrawLineEx((Vector2){ x, c.y - 12.0f * e }, (Vector2){ x, c.y + 27.0f * e }, 3.0f * e, cor);
            DrawLineEx((Vector2){ x - 4.0f * e, c.y - 12.0f * e },
                       (Vector2){ x + 4.0f * e, c.y - 12.0f * e }, 1.5f * e, cor);
            DrawLineEx((Vector2){ c.x, c.y + 2.0f * e }, (Vector2){ x, c.y + 8.0f * e }, 1.4f * e, cor);
        }
    } else if (tipo == AWING) {
        Vector2 a = { c.x, c.y - 31.0f * e };
        Vector2 b = { c.x + 28.0f * e, c.y + 24.0f * e };
        Vector2 d = { c.x - 28.0f * e, c.y + 24.0f * e };
        const Vector2 tri[3] = { a, b, d };

        contorno(tri, 3, 2.0f * e, cor);
        DrawLineEx(a, (Vector2){ c.x, c.y + 10.0f * e }, 1.5f * e, cor);
    } else {
        Vector2 a = { c.x - 30.0f * e, c.y - 22.0f * e };
        Vector2 b = { c.x + 30.0f * e, c.y + 22.0f * e };
        Vector2 d = { c.x - 30.0f * e, c.y + 22.0f * e };
        Vector2 f = { c.x + 30.0f * e, c.y - 22.0f * e };

        DrawLineEx(a, b, 8.0f * e, Fade(cor, 0.14f));
        DrawLineEx(d, f, 8.0f * e, Fade(cor, 0.14f));
        DrawLineEx(a, b, 3.5f * e, cor);
        DrawLineEx(d, f, 3.5f * e, cor);
        DrawCircleV(a, 2.2f * e, cor);
        DrawCircleV(b, 2.2f * e, cor);
        DrawCircleV(d, 2.2f * e, cor);
        DrawCircleV(f, 2.2f * e, cor);
        DrawCircleV(c, 3.0f * e, Fade(cor, 0.9f));
    }
}

/* --------------------------------------------------------------------------------------------
 * Console central: barras do Destróier, aviso de Y-Wing e visor de combate
 * ----------------------------------------------------------------------------------------- */

static void desenharHousingCombate(void)
{
    const Vector2 corpo[4] = {
        { 550.0f, CENTRO_TOPO }, { 730.0f, CENTRO_TOPO },
        { 800.0f, ALTURA_TELA }, { 480.0f, ALTURA_TELA },
    };

    desenharPainel3D(corpo, 4, COR_NEON, 0, 7.0f, 9.0f);

    /* Fio vermelho acompanhando as laterais inclinadas do console. */
    DrawLineEx((Vector2){ 542.0f, 450.0f }, (Vector2){ 490.0f, 690.0f }, 2.5f, Fade(COR_NEON_VERMELHO, 0.9f));
    DrawLineEx((Vector2){ 738.0f, 450.0f }, (Vector2){ 790.0f, 690.0f }, 2.5f, Fade(COR_NEON_VERMELHO, 0.9f));

    /* Pequenas luzes no rodapé. */
    for (int i = 0; i < 3; i++) {
        DrawCircleV((Vector2){ 508.0f + i * 11.0f, 704.0f }, 2.5f, Fade(COR_NEON_VERMELHO, 0.85f));
        DrawCircleV((Vector2){ 740.0f + i * 11.0f, 704.0f }, 2.5f, Fade(COR_ROTULO, 0.7f));
    }
}

/*
 * O topo mostra o escudo do Destróier como barra sólida azul. Atrás dela fica o casco em
 * segmentos vermelhos: conforme o escudo cai, os segmentos vermelhos aparecem.
 */
static void barraDestroier(const Jogo *g)
{
    float escudo = fracao(g->escudoDestroier, ESCUDO_MAX_DESTROIER);
    float casco = fracao(g->cascoDestroier, CASCO_MAX_DESTROIER);
    int recarregando = g->cascoDestroier > 0.0f && g->esperaEscudoDestroier <= 0.0f && escudo < 1.0f;
    Rectangle bisel = { 552.0f, 404.0f, 176.0f, 30.0f };
    Rectangle r = { 558.0f, 410.0f, 164.0f, 18.0f };
    int segmentos = 22;
    int acesos = (int)ceilf(casco * segmentos);
    int i;

    DrawRectangleRec(bisel, (Color){ 10, 12, 20, 255 });
    DrawRectangleLinesEx(bisel, 1.5f, Fade(COR_NEON_VERMELHO, 0.85f));

    /* Casco: segmentos vermelhos com espaçamento. */
    for (i = 0; i < segmentos; i++) {
        float passo = r.width / segmentos;
        Rectangle s = { r.x + passo * i + 1.0f, r.y + 2.0f, passo - 2.4f, r.height - 4.0f };

        DrawRectangleRec(s, i < acesos ? COR_CRITICA : (Color){ 70, 20, 22, 170 });
    }

    /* Escudo sólido por cima; quando ele recua, descobre o casco vermelho. */
    if (escudo > 0.0f) {
        Rectangle cheio = { r.x, r.y, r.width * escudo, r.height };

        DrawRectangleRec(cheio, (Color){ 60, 130, 255, 255 });
        DrawRectangleGradientV((int)cheio.x, (int)cheio.y, (int)cheio.width, (int)cheio.height,
                               (Color){ 150, 210, 255, 255 }, (Color){ 45, 105, 240, 255 });
        DrawLineEx((Vector2){ cheio.x, cheio.y }, (Vector2){ cheio.x + cheio.width, cheio.y },
                   1.5f, WHITE);
    }

    DrawRectangleLinesEx(r, 1.0f, Fade(COR_ROTULO, 0.6f));
    if (recarregando && piscaAceso())
        DrawRectangleLinesEx((Rectangle){ bisel.x - 2.0f, bisel.y - 2.0f, bisel.width + 4.0f, bisel.height + 4.0f },
                             1.5f, Fade(COR_ESCUDO, 0.9f));
}

/* Só existe conteúdo nessa faixa quando um Y-Wing está atacando o Destróier. */
static void desenharAvisoYwing(const Jogo *g)
{
    int n = 0;
    Color cor;

    for (const NoInimigo *no = g->inimigos.inicio; no != NULL; no = no->prox)
        n += ywingBombardeando(&no->dado);
    if (n == 0)
        return;

    cor = piscaAceso() ? COR_ALERTA : Fade(COR_ALERTA, 0.45f);
    if (n == 1)
        textoAjustado("Y-WING ATACANDO O DESTRÓIER", (int)COMBATE_X, 450, 13, 195, 0.5f, cor);
    else
        textoAjustado(TextFormat("%d Y-WINGS ATACANDO O DESTRÓIER", n), (int)COMBATE_X, 450, 13, 195, 0.5f, cor);
    DrawLineEx((Vector2){ COMBATE_X - 55.0f, 467.0f }, (Vector2){ COMBATE_X + 55.0f, 467.0f },
               1.0f, Fade(cor, 0.55f));
}

static void iconeHp(Vector2 c, Color cor)
{
    const Vector2 d[4] = {
        { c.x, c.y - 6.0f }, { c.x + 6.0f, c.y }, { c.x, c.y + 6.0f }, { c.x - 6.0f, c.y },
    };

    contorno(d, 4, 1.2f, cor);
    DrawCircleV(c, 1.6f, cor);
}

static void iconeDistancia(Vector2 c, Color cor)
{
    DrawCircleV(c, 2.5f, cor);
    DrawLineEx((Vector2){ c.x - 7.0f, c.y - 3.0f }, (Vector2){ c.x - 3.0f, c.y }, 1.2f, cor);
    DrawLineEx((Vector2){ c.x - 7.0f, c.y + 3.0f }, (Vector2){ c.x - 3.0f, c.y }, 1.2f, cor);
    DrawLineEx((Vector2){ c.x + 7.0f, c.y - 3.0f }, (Vector2){ c.x + 3.0f, c.y }, 1.2f, cor);
    DrawLineEx((Vector2){ c.x + 7.0f, c.y + 3.0f }, (Vector2){ c.x + 3.0f, c.y }, 1.2f, cor);
}

static void desenharCombate(const Jogo *g)
{
    const Inimigo *alvo = g->inimigos.alvo != NULL ? &g->inimigos.alvo->dado : NULL;
    Vector2 c = { COMBATE_X, COMBATE_Y };
    Rectangle faixa = { COMBATE_X - 90.0f, COMBATE_Y - COMBATE_RAIO - 30.0f, 180.0f, 28.0f };
    int i;

    /* Faixa com o nome da nave, logo acima do visor. */
    DrawRectangleRec(faixa, (Color){ 92, 16, 22, 235 });
    DrawRectangleLinesEx(faixa, 1.0f, Fade(COR_CRITICA, 0.8f));
    if (alvo != NULL)
        texto(TextFormat("%s %02d", nomeDoTipo(alvo->tipo), alvo->id), (int)COMBATE_X, (int)faixa.y + 5,
              14, 0.5f, (Color){ 255, 82, 72, 255 });
    else
        texto("SEM ALVO", (int)COMBATE_X, (int)faixa.y + 5, 14, 0.5f, Fade(COR_ROTULO, 0.65f));

    /* Visor redondo com anel segmentado. */
    DrawCircleV(c, COMBATE_RAIO + 7.0f, COR_METAL);
    DrawCircleGradient(c, COMBATE_RAIO, COR_VIDRO_2, (Color){ 4, 7, 16, 255 });
    DrawCircleLinesV(c, COMBATE_RAIO, Fade(COR_NEON, 0.8f));
    DrawRing(c, COMBATE_RAIO - 1.0f, COMBATE_RAIO + 2.0f, 0.0f, 360.0f, 96, Fade(COR_NEON, 0.18f));

    for (i = 0; i < 72; i++) {
        float ang = i * (360.0f / 72.0f) * DEG2RAD;
        float r1 = COMBATE_RAIO - ((i % 6 == 0) ? 11.0f : 7.0f);
        Vector2 a = { c.x + cosf(ang) * r1, c.y + sinf(ang) * r1 };
        Vector2 b = { c.x + cosf(ang) * (COMBATE_RAIO - 3.0f), c.y + sinf(ang) * (COMBATE_RAIO - 3.0f) };

        DrawLineEx(a, b, 2.2f, Fade((i % 6 == 0) ? COR_ESCUDO : COR_NEON, 0.95f));
    }
    DrawCircleLinesV(c, COMBATE_RAIO - 13.0f, Fade(COR_NEON, 0.18f));

    if (alvo == NULL) {
        DrawCircleLinesV(c, 20.0f, Fade(COR_ROTULO, 0.25f));
        DrawLineV((Vector2){ c.x - 28, c.y }, (Vector2){ c.x + 28, c.y }, Fade(COR_ROTULO, 0.25f));
        DrawLineV((Vector2){ c.x, c.y - 28 }, (Vector2){ c.x, c.y + 28 }, Fade(COR_ROTULO, 0.25f));
        texto("AGUARDANDO TRAVA", (int)c.x, (int)c.y + 44, 11, 0.5f, Fade(COR_ROTULO, 0.6f));
        return;
    }

    {
        float total = alvo->escudoMax + alvo->cascoMax;
        float restante = alvo->escudo + alvo->casco;
        float dist = Vector3Distance(alvo->pos, g->jogador.pos);
        Color corHp;

        if (total <= 0.0f)
            total = 1.0f;
        corHp = corDoNivel(fracao(restante, total), COR_VALOR);

        desenharModeloAlvo(alvo->tipo, (Vector2){ c.x, c.y - 8.0f }, 1.35f, COR_MODELO);

        /* HP e distância alinhados simetricamente no rodapé do visor. */
        iconeHp((Vector2){ c.x - 40.0f, c.y + 44.0f }, corHp);
        texto(TextFormat("%d", (int)ceilf(fracao(restante, total) * 100.0f)),
              (int)(c.x - 40.0f), (int)(c.y + 56.0f), 22, 0.5f, corHp);

        iconeDistancia((Vector2){ c.x + 40.0f, c.y + 44.0f }, COR_CRITICA);
        textoAjustado(TextFormat("%d", (int)lroundf(dist)), (int)(c.x + 40.0f), (int)(c.y + 56.0f),
                      22, 66, 0.5f, (Color){ 255, 82, 72, 255 });
    }
}

/* --------------------------------------------------------------------------------------------
 * Status da nave: reparo, vidas, TIE e HP
 * ----------------------------------------------------------------------------------------- */

static void botaoHabilidade(Vector2 c, float r, Color cor, int tipo, int vidas)
{
    const Vector2 hex[6] = {
        { c.x - r * 0.55f, c.y - r }, { c.x + r * 0.55f, c.y - r }, { c.x + r, c.y },
        { c.x + r * 0.55f, c.y + r }, { c.x - r * 0.55f, c.y + r }, { c.x - r, c.y },
    };

    poligonoDeslocado(hex, 6, 3.0f, 4.0f, Fade(BLACK, 0.5f));
    poligono(hex, 6, (Color){ 9, 13, 26, 255 });
    contorno(hex, 6, 2.0f, cor);
    DrawCircleLinesV(c, r - 4.0f, Fade(cor, 0.22f));

    if (tipo == 0) {
        /* Cruz de reparo. */
        DrawRectangleRec((Rectangle){ c.x - 2.5f, c.y - 9.0f, 5.0f, 18.0f }, cor);
        DrawRectangleRec((Rectangle){ c.x - 9.0f, c.y - 2.5f, 18.0f, 5.0f }, cor);
    } else {
        texto(TextFormat("%d", vidas), (int)c.x, (int)c.y - 9, 16, 0.5f, cor);
    }
}

static void desenharStatusNave(const Jogo *g)
{
    const Vector2 corpo[6] = {
        { 900.0f, 395.0f }, { 1095.0f, 410.0f }, { 1095.0f, 480.0f },
        { 1070.0f, 645.0f }, { 910.0f, 655.0f }, { 885.0f, 480.0f },
    };
    float casco = fracao(g->jogador.casco, CASCO_JOGADOR);
    Color corCasco = corDoNivel(casco, COR_TIE);
    int reparoPronto = g->jogador.cooldownReparo <= 0.0f;
    int vidas = g->jogador.respawnsRestantes;

    if (vidas < 0)
        vidas = 0;

    desenharPainel3D(corpo, 6, COR_NEON, 1, 8.0f, 10.0f);
    DrawLineEx((Vector2){ 922.0f, 478.0f }, (Vector2){ 1060.0f, 478.0f }, 1.0f, Fade(COR_NEON, 0.22f));

    botaoHabilidade((Vector2){ 946.0f, 450.0f }, 17.0f,
                    reparoPronto ? (Color){ 90, 255, 140, 255 } : COR_ROTULO, 0, 0);
    botaoHabilidade((Vector2){ 1034.0f, 450.0f }, 17.0f,
                    vidas > 1 ? COR_VALOR : COR_CRITICA, 1, vidas);

    desenharModeloTie((Vector2){ STATUS_CX, STATUS_CY }, 1.25f, corCasco);

    texto(TextFormat("%d", (int)ceilf(casco * 100.0f)), (int)STATUS_CX, 585, 34, 0.5f, corCasco);
    DrawCircleV((Vector2){ 1067.0f, 626.0f }, 3.0f, COR_ALERTA);
    DrawCircleV((Vector2){ 1080.0f, 638.0f }, 3.0f, Fade(COR_NEON_VERMELHO, 0.95f));
}

/* --------------------------------------------------------------------------------------------
 * Informações da partida no canto superior direito
 * ----------------------------------------------------------------------------------------- */

static const char *textoMultiplicador(float mult)
{
    return mult >= MULTIPLICADOR_X2 ? "x2" : (mult >= MULTIPLICADOR_X1_5 ? "x1,5" : "x1");
}

static Color corMultiplicador(float mult)
{
    return mult >= MULTIPLICADOR_X2 ? COR_CRITICA : (mult >= MULTIPLICADOR_X1_5 ? COR_ALERTA : COR_ROTULO);
}

static void desenharInfoPartida(const Jogo *g)
{
    const Vector2 corpo[4] = {
        { 1012.0f, 14.0f }, { 1264.0f, 14.0f }, { 1264.0f, 150.0f }, { 1012.0f, 150.0f },
    };
    int segundos = g->tempoRestante > 0.0f ? (int)ceilf(g->tempoRestante) : 0;
    Color corTempo = segundos <= TEMPO_CRITICO && piscaAceso() ? COR_CRITICA : COR_VALOR;
    float mult = multiplicadorAtual(g->sequenciaSemDano);
    int defesa = 0, interceptacao = 0;
    float borda = RAIO_ARENA - Vector3Length(g->jogador.pos);

    if (borda < 0.0f)
        borda = 0.0f;
    for (int i = 0; i < MAX_ALIADOS; i++) {
        if (!g->aliados[i].ativo)
            continue;
        defesa += g->aliados[i].papel == ALIADO_DEFENDER;
        interceptacao += g->aliados[i].papel == ALIADO_INTERCEPTAR;
    }

    desenharPainel3D(corpo, 4, COR_NEON, 2, 6.0f, 8.0f);
    DrawLineEx((Vector2){ corpo[0].x, corpo[0].y }, (Vector2){ corpo[1].x, corpo[1].y },
               2.0f, Fade(COR_NEON_VERMELHO, 0.85f));

    texto("TEMPO", 1026, 24, 10, 0.0f, COR_ROTULO);
    texto(TextFormat("%d:%02d", segundos / 60, segundos % 60), 1024, 36, 26, 0.0f, corTempo);
    texto("PONTOS", 1250, 24, 10, 1.0f, COR_ROTULO);
    texto(TextFormat("%d", g->pontos), 1250, 38, 18, 1.0f, COR_VALOR);

    DrawLineEx((Vector2){ 1026.0f, 72.0f }, (Vector2){ 1250.0f, 72.0f }, 1.0f, Fade(COR_NEON, 0.35f));
    texto("ABATES", 1026, 80, 11, 0.0f, COR_ROTULO);
    texto(TextFormat("%d", g->abates), 1102, 77, 15, 0.0f, COR_VALOR);
    texto("SEQ", 1168, 80, 11, 0.0f, COR_ROTULO);
    texto(TextFormat("%d", g->sequenciaSemDano), 1250, 77, 15, 1.0f, COR_VALOR);
    texto(textoMultiplicador(mult), 1250, 94, 15, 1.0f, corMultiplicador(mult));

    DrawLineEx((Vector2){ 1026.0f, 114.0f }, (Vector2){ 1250.0f, 114.0f }, 1.0f, Fade(COR_NEON, 0.35f));
    texto(TextFormat("ALIADOS  DEF %d  INT %d", defesa, interceptacao), 1026, 120, 10, 0.0f, COR_HUD);
    texto("BORDA", 1026, 135, 10, 0.0f, COR_ROTULO);
    texto(TextFormat("%d u", (int)lroundf(borda)), 1250, 132, 13, 1.0f,
          borda < RAIO_ARENA * BORDA_PERTO ? COR_CRITICA : COR_VALOR);
}

/* --------------------------------------------------------------------------------------------
 * Avisos de tela
 * ----------------------------------------------------------------------------------------- */

/* Moldura vermelha piscando e aviso compacto enquanto a nave está presa na borda da arena. */
static void desenharAvisoArena(void)
{
    float alfa = piscaAceso() ? 0.85f : 0.30f;

    DrawRectangleLinesEx((Rectangle){ 3.0f, 3.0f, LARGURA_TELA - 6.0f, ALTURA_TELA - 6.0f },
                         4.0f, Fade(COR_CRITICA, alfa));
    texto("LIMITE DA ARENA  ·  VOLTE PARA A ÁREA DE COMBATE", (int)JANELA_X, 104, 18, 0.5f,
          Fade(COR_CRITICA, alfa));
}

/* Flash vermelho no vidro inteiro enquanto g->tempoDano > 0; a opacidade cai junto com o tremor. */
static void desenharFlashDano(const Jogo *g)
{
    float fator = g->tempoDano / DURACAO_FEEDBACK_DANO;

    if (g->tempoDano <= 0.0f)
        return;
    if (fator > 1.0f)
        fator = 1.0f;
    DrawRectangle(0, 0, LARGURA_TELA, ALTURA_TELA, Fade(COR_CRITICA, 0.45f * fator));
}

/* Aviso central de morte e respawn. */
static void desenharMorte(const Jogo *g)
{
    Rectangle r = { JANELA_X - 260.0f, 170.0f, 520.0f, 92.0f };
    char tempo[48];

    if (!g->jogador.morto)
        return;

    DrawRectangleRec(r, Fade(BLACK, 0.72f));
    DrawRectangleLinesEx(r, 2.0f, Fade(COR_CRITICA, piscaAceso() ? 0.95f : 0.35f));
    texto("TIE ABATIDO", (int)JANELA_X, (int)r.y + 8, 30, 0.5f, COR_CRITICA);
    if (g->jogador.respawnsRestantes > 0) {
        int segundos = g->jogador.tempoRespawn > 0.0f ? (int)ceilf(g->jogador.tempoRespawn) : 0;

        if (g->jogador.explodiu)
            snprintf(tempo, sizeof tempo, "RESPAWN EM %d s", segundos);
        else
            snprintf(tempo, sizeof tempo, "PREPARANDO RESPAWN");
        texto(tempo, (int)JANELA_X, (int)r.y + 48, 18, 0.5f, COR_VALOR);
        texto(TextFormat("RESPAWNS RESTANTES: %d", g->jogador.respawnsRestantes),
              (int)JANELA_X, (int)r.y + 70, 14, 0.5f, COR_ROTULO);
    } else {
        texto("SEM RESPAWNS", (int)JANELA_X, (int)r.y + 48, 20, 0.5f, COR_VALOR);
    }
}

void hud_desenhar(const Jogo *g, int foraDaArena)
{
    /* Durante o giro da queda o cockpit continua; depois da explosão fica só o contador de
     * respawn sobre a câmera de terceira pessoa. O HUD completo volta no respawn. */
    if (g->jogador.explodiu) {
        desenharMorte(g);
        return;
    }

    desenharFlashDano(g);
    desenharEstrutura();

    /* Suportes atrás dos instrumentos, como se estivessem presos ao assoalho. */
    desenharSuporte((Vector2){ 238.0f, 650.0f }, (Vector2){ 270.0f, 720.0f });
    desenharSuporte((Vector2){ 358.0f, 645.0f }, (Vector2){ 392.0f, 720.0f });
    desenharSuporte((Vector2){ 1042.0f, 650.0f }, (Vector2){ 1010.0f, 720.0f });
    desenharSuporte((Vector2){ 922.0f, 645.0f }, (Vector2){ 888.0f, 720.0f });

    /* Console central primeiro; a velocidade fica por cima da borda dele. */
    desenharAntenas();
    desenharHousingCombate();
    barraDestroier(g);
    desenharAvisoYwing(g);
    desenharCombate(g);

    desenharSensores(g);
    desenharStatusNave(g);
    desenharVelocidade(g);
    desenharInfoPartida(g);

    /* Mira e alvo por último: o painel do cockpit nunca cobre a HUD de mira. */
    desenharReticula();
    desenharAlvoNoVidro(g);

    if (foraDaArena)
        desenharAvisoArena();
}
