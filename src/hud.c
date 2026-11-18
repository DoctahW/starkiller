#include "hud.h"

#include <math.h>
#include <stddef.h>

#include "raymath.h"
#include "combate.h"
#include "jogo.h"
#include "nave.h"
#include "pontuacao.h"

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

/* Retícula: anel central com quatro traços. */
#define RETICULA_RAIO   14.0f
#define RETICULA_VAO    22
#define RETICULA_TRACO  18

/* Tablets dos lados do painel (centro x, topo y) e coluna central. */
#define TABLET_X        360.0f
#define TABLET_Y        505.0f
#define TABLET_MEIA     115.0f  /* meia largura */
#define TABLET_ALTURA   210.0f
#define COLUNA_TOPO     470.0f
#define VISOR_Y         632.0f  /* centro do visor redondo da coluna */
#define VISOR_RAIO      78.0f

#define BARRA_SEGMENTOS 10

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
#define COR_DESTROIER (Color){ 70, 170, 255, 255 }
#define COR_ALERTA    (Color){ 240, 200, 40, 255 }
#define COR_CRITICA   (Color){ 235, 50, 40, 255 }
#define COR_ALVO      (Color){ 255, 120, 60, 255 }

/*
 * Alvo travado. No vidro: colchetes em volta do caça, com tipo e distância, e o indicador de tiro
 * adiantado (onde mirar para o laser encontrar o caça); fora do vidro, uma seta em volta da retícula.
 * No visor: escopo com a direção do alvo (centro = à frente, anel do meio = 90°, borda = atrás).
 */
#define MARCA_MIN       14.0f   /* meia largura mínima dos colchetes (px) */
#define MARCA_ESCALA    1.6f    /* colchetes um pouco maiores que o caça */
#define MARGEM_VIDRO    20.0f   /* px; mais perto da borda da tela, vira seta */
#define RAIO_SETA       200.0f  /* círculo em torno da retícula onde fica a seta */
#define PIPPER_RAIO     7.0f
#define ESCOPO_RAIO     40.0f
#define ESCOPO_Y        (VISOR_Y - 14.0f)

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

static void desenharReticula(void)
{
    int cx = LARGURA_TELA / 2;
    int cy = ALTURA_TELA / 2;
    Color cor = Fade(COR_HUD, 0.85f);

    DrawCircleLines(cx, cy, RETICULA_RAIO, cor);
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

/* Projeta `p` na tela; 0 se está atrás da nave ou fora do vidro (acima do painel). */
static int noVidro(const Jogador *j, Vector3 p, Vector2 *tela)
{
    if (noEspacoDaNave(j, p).z < 1.0f)
        return 0;
    *tela = GetWorldToScreen(p, cameraDoJogador(j));
    return tela->x >= MARGEM_VIDRO && tela->x <= LARGURA_TELA - MARGEM_VIDRO
        && tela->y >= MARGEM_VIDRO && tela->y <= COLUNA_TOPO - MARGEM_VIDRO;
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
 * chegar, se ele voar reto; com o indicador dentro da retícula, o tiro acerta (fica verde).
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

    if (!noVidro(j, alvo->pos, &tela)) {
        setaParaOAlvo(j, alvo, dist);
        return;
    }

    focal = (ALTURA_TELA * 0.5f) / tanf(FOV_JOGADOR * 0.5f * DEG2RAD);
    m = fmaxf(MARCA_MIN, raioInimigo(alvo->tipo) * MARCA_ESCALA * focal / noEspacoDaNave(j, alvo->pos).z);
    colchetes(tela, m, COR_ALVO);
    texto(TextFormat("%s  %d u", nomeDoTipo(alvo->tipo), (int)lroundf(dist)), (int)tela.x, (int)(tela.y + m + 4.0f),
          14, 0.5f, COR_ALVO);

    if (dist > VEL_LASER * VIDA_LASER)
        return;
    adiantado = pontoDeInterceptacao(j->pos, VEL_LASER, alvo->pos, alvo->vel);
    if (noVidro(j, adiantado, &pipper)) {
        Vector2 centro = { LARGURA_TELA / 2.0f, ALTURA_TELA / 2.0f };
        int naMira = Vector2Distance(pipper, centro) <= RETICULA_RAIO;
        Color cor = naMira ? COR_HUD : COR_ALVO;

        DrawLineEx(tela, pipper, 1.0f, Fade(cor, 0.5f));
        DrawCircleLinesV(pipper, PIPPER_RAIO, cor);
        if (naMira)
            DrawCircleV(pipper, PIPPER_RAIO - 3.0f, cor);
    }
}

/* Escopo no visor: direção do alvo travado; o ponto pisca quando o alvo está atrás. */
static void desenharEscopo(const Jogador *j, const Inimigo *alvo)
{
    Vector2 c = { JANELA_X, ESCOPO_Y };
    Vector3 local = Vector3Normalize(noEspacoDaNave(j, alvo->pos));
    float ang = acosf(Clamp(local.z, -1.0f, 1.0f));
    Vector2 dir = { local.x, -local.y };
    Vector2 ponto;

    dir = Vector2Length(dir) > 1e-3f ? Vector2Normalize(dir) : (Vector2){ 0.0f, 0.0f };
    ponto = Vector2Add(c, Vector2Scale(dir, ang / PI * ESCOPO_RAIO));

    DrawCircleLinesV(c, ESCOPO_RAIO, Fade(COR_NEON, 0.8f));
    DrawCircleLinesV(c, ESCOPO_RAIO * 0.5f, Fade(COR_NEON, 0.4f));
    DrawLineV((Vector2){ c.x - ESCOPO_RAIO, c.y }, (Vector2){ c.x + ESCOPO_RAIO, c.y }, Fade(COR_NEON, 0.3f));
    DrawLineV((Vector2){ c.x, c.y - ESCOPO_RAIO }, (Vector2){ c.x, c.y + ESCOPO_RAIO }, Fade(COR_NEON, 0.3f));
    if (ang <= PI * 0.5f || piscaAceso())
        DrawCircleV(ponto, 4.0f, COR_ALVO);
}

/* Barra horizontal em segmentos que esvazia da direita para a esquerda. */
static void barra(Rectangle r, float f, Color cor)
{
    float largura = (r.width - 4.0f) / BARRA_SEGMENTOS;
    int acesos = (int)ceilf(f * BARRA_SEGMENTOS);
    int i;

    for (i = 0; i < BARRA_SEGMENTOS; i++) {
        Rectangle s = { r.x + 2.0f + largura * i, r.y, largura - 3.0f, r.height };

        DrawRectangleRec(s, i < acesos ? cor : Fade(COR_ROTULO, 0.15f));
    }
}

/*
 * Barra contínua com marcas a cada 10%. O casco do Destróier tem 300 pontos e cada laser tira
 * 2,4, então em 10 segmentos (30 pontos cada) a barra quase não se movia.
 */
static void barraContinua(Rectangle r, float f, Color cor)
{
    int i;

    DrawRectangleRec(r, Fade(COR_ROTULO, 0.15f));
    DrawRectangleRec((Rectangle){ r.x, r.y, r.width * f, r.height }, cor);
    for (i = 1; i < 10; i++) {
        float x = r.x + r.width * i / 10.0f;

        DrawLineV((Vector2){ x, r.y }, (Vector2){ x, r.y + r.height }, Fade(BLACK, 0.35f));
    }
}

/*
 * Tablet hexagonal com a ponta estreita para cima, preso ao painel por um braço,
 * moldura escura e neon azul com o canto externo vermelho.
 */
static void tablet(float cx, int ladoExterno)
{
    float t = TABLET_Y;
    float m = TABLET_MEIA;
    const Vector2 corpo[] = {
        { cx - m * 0.62f, t }, { cx + m * 0.62f, t }, { cx + m, t + 62.0f },
        { cx + m, t + TABLET_ALTURA }, { cx - m, t + TABLET_ALTURA }, { cx - m, t + 62.0f },
    };
    Vector2 vidro[6];
    int i;

    DrawRectangleRec((Rectangle){ cx - 22.0f, t + TABLET_ALTURA - 10.0f, 44.0f, ALTURA_TELA }, COR_PAINEL);
    poligono(corpo, 6, COR_PAINEL);
    contorno(corpo, 6, 2.0f, COR_BRILHO);
    for (i = 0; i < 6; i++)
        vidro[i] = Vector2MoveTowards(corpo[i], (Vector2){ cx, t + TABLET_ALTURA * 0.55f }, 12.0f);
    poligono(vidro, 6, COR_TELA);
    contorno(vidro, 6, 2.0f, COR_NEON);
    /* Neon vermelho no lado de fora, como nos painéis da referência. */
    DrawLineEx(corpo[ladoExterno > 0 ? 1 : 0], corpo[ladoExterno > 0 ? 2 : 5], 3.0f, COR_NEON_VERMELHO);
    DrawLineEx(corpo[ladoExterno > 0 ? 2 : 5], corpo[ladoExterno > 0 ? 3 : 4], 3.0f, COR_NEON_VERMELHO);
}

static const char *textoMultiplicador(float mult)
{
    return mult >= MULTIPLICADOR_X2 ? "x2" : (mult >= MULTIPLICADOR_X1_5 ? "x1,5" : "x1");
}

static Color corMultiplicador(float mult)
{
    return mult >= MULTIPLICADOR_X2 ? COR_CRITICA : (mult >= MULTIPLICADOR_X1_5 ? COR_ALERTA : COR_ROTULO);
}

/* Tablet esquerdo: tempo, pontos, abates e multiplicador de sequência. */
static void desenharTabletPartida(const Jogo *g)
{
    float cx = TABLET_X;
    int esq = (int)(cx - TABLET_MEIA) + 22;
    int dir = (int)(cx + TABLET_MEIA) - 22;
    int y = (int)TABLET_Y;
    /* Arredonda para cima: só mostra 0:00 no fim. */
    int segundos = g->tempoRestante > 0.0f ? (int)ceilf(g->tempoRestante) : 0;
    Color corTempo = segundos <= TEMPO_CRITICO && piscaAceso() ? COR_CRITICA : COR_VALOR;
    float mult = multiplicadorAtual(g->sequenciaSemDano);

    tablet(cx, -1);
    texto("TEMPO", (int)cx, y + 18, 14, 0.5f, COR_ROTULO);
    texto(TextFormat("%d:%02d", segundos / 60, segundos % 60), (int)cx, y + 32, 40, 0.5f, corTempo);
    DrawLineEx((Vector2){ (float)esq, y + 82.0f }, (Vector2){ (float)dir, y + 82.0f }, 1.0f, Fade(COR_NEON, 0.6f));

    texto("PONTOS", esq, y + 96, 14, 0.0f, COR_ROTULO);
    texto(TextFormat("%d", g->pontos), dir, y + 90, 26, 1.0f, COR_VALOR);
    texto("ABATES", esq, y + 128, 14, 0.0f, COR_ROTULO);
    texto(TextFormat("%d", g->abates), dir, y + 124, 22, 1.0f, COR_VALOR);
    texto(TextFormat("SEQ %d", g->sequenciaSemDano), esq, y + 162, 14, 0.0f, COR_ROTULO);
    texto(textoMultiplicador(mult), dir, y + 154, 26, 1.0f, corMultiplicador(mult));
}

/* Tablet direito: casco do TIE e do Destróier, distância à borda da arena. */
static void desenharTabletNave(const Jogo *g)
{
    float cx = LARGURA_TELA - TABLET_X;
    int esq = (int)(cx - TABLET_MEIA) + 22;
    int dir = (int)(cx + TABLET_MEIA) - 22;
    int y = (int)TABLET_Y;
    float casco = fracao(g->jogador.casco, CASCO_JOGADOR);
    float destroier = fracao(g->cascoDestroier, CASCO_MAX_DESTROIER);
    float borda = RAIO_ARENA - Vector3Length(g->jogador.pos);
    Color corCasco = corDoNivel(casco, COR_TIE);
    Color corDestroier = corDoNivel(destroier, COR_DESTROIER);

    if (borda < 0.0f)
        borda = 0.0f;

    /* As linhas começam abaixo do topo inclinado do tablet. */
    tablet(cx, 1);
    texto("CASCO", esq, y + 46, 14, 0.0f, COR_ROTULO);
    texto(TextFormat("%d%%", (int)ceilf(casco * 100.0f)), dir, y + 40, 22, 1.0f, corCasco);
    barra((Rectangle){ (float)esq, y + 68.0f, (float)(dir - esq), 14.0f }, casco, corCasco);

    texto("DESTRÓIER", esq, y + 98, 14, 0.0f, COR_ROTULO);
    texto(TextFormat("%d%%", (int)ceilf(destroier * 100.0f)), dir, y + 92, 22, 1.0f, corDestroier);
    barraContinua((Rectangle){ (float)esq, y + 120.0f, (float)(dir - esq), 14.0f }, destroier, corDestroier);

    DrawLineEx((Vector2){ (float)esq, y + 148.0f }, (Vector2){ (float)dir, y + 148.0f }, 1.0f, Fade(COR_NEON, 0.6f));
    texto("BORDA", esq, y + 164, 14, 0.0f, COR_ROTULO);
    texto(TextFormat("%d u", (int)lroundf(borda)), dir, y + 158, 22, 1.0f,
          borda < RAIO_ARENA * BORDA_PERTO ? COR_CRITICA : COR_VALOR);
}

/* Silhueta do TIE desenhada em linhas. */
static void silhuetaTie(int cx, int cy, Color cor)
{
    int lado;

    for (lado = -1; lado <= 1; lado += 2) {
        float x = (float)(cx + lado * 28);
        const Vector2 asa[] = {
            { x, cy - 36.0f }, { x + 6.0f, cy - 22.0f }, { x + 6.0f, cy + 22.0f },
            { x, cy + 36.0f }, { x - 6.0f, cy + 22.0f }, { x - 6.0f, cy - 22.0f },
        };

        contorno(asa, 6, 2.0f, cor);
        DrawLineEx((Vector2){ x - lado * 6.0f, (float)cy }, (Vector2){ cx + lado * 11.0f, (float)cy }, 3.0f, cor);
    }
    DrawCircleLinesV((Vector2){ (float)cx, (float)cy }, 11.0f, cor);
    DrawCircleLinesV((Vector2){ (float)cx, (float)cy }, 5.0f, cor);
}

/* Coluna central: velocidade, acelerador e luz da arena no topo; visor redondo com o estado. */
static void desenharColuna(const Jogo *g, int foraDaArena)
{
    const Vector2 corpo[] = { { 520, COLUNA_TOPO }, { 760, COLUNA_TOPO }, { 785, ALTURA_TELA }, { 495, ALTURA_TELA } };
    Rectangle vel = { 532, COLUNA_TOPO + 10, 54, 28 };
    Rectangle acel = { 592, COLUNA_TOPO + 10, 96, 28 };
    Rectangle luz = { 694, COLUNA_TOPO + 10, 54, 28 };
    Vector2 visor = { JANELA_X, VISOR_Y };
    const Inimigo *alvo = g->inimigos.alvo != NULL ? &g->inimigos.alvo->dado : NULL;
    float casco = fracao(g->jogador.casco, CASCO_JOGADOR);
    float f = fracao(g->jogador.velocidade - VEL_MIN, VEL_MAX - VEL_MIN);
    int aceso = foraDaArena && piscaAceso();
    const char *estado = "Em combate";
    Color corEstado = COR_HUD;

    if (foraDaArena) {
        estado = "Volte à arena";
        corEstado = COR_CRITICA;
    } else if (casco < FRACAO_CRITICA) {
        estado = "Casco crítico";
        corEstado = COR_CRITICA;
    } else if (g->cascoDestroier < CASCO_MAX_DESTROIER * FRACAO_CRITICA) {
        estado = "Destróier!";
        corEstado = COR_CRITICA;
    }

    poligono(corpo, 4, COR_PAINEL);
    contorno(corpo, 4, 2.0f, COR_BRILHO);

    DrawRectangleRec(vel, COR_TELA);
    DrawRectangleLinesEx(vel, 2.0f, COR_NEON);
    texto(TextFormat("%d", (int)lroundf(g->jogador.velocidade)), (int)(vel.x + vel.width / 2), (int)vel.y + 4, 20, 0.5f, COR_VALOR);

    DrawRectangleRec(acel, COR_TELA);
    DrawRectangleRec((Rectangle){ acel.x + 4, acel.y + 9, (acel.width - 8) * f, acel.height - 18 }, COR_NEON);
    DrawRectangleLinesEx(acel, 2.0f, COR_NEON);

    DrawRectangleRec(luz, aceso ? COR_CRITICA : COR_TELA);
    DrawRectangleLinesEx(luz, 2.0f, foraDaArena ? COR_CRITICA : COR_NEON);
    texto("ARENA", (int)(luz.x + luz.width / 2), (int)luz.y + 6, 15, 0.5f, aceso ? WHITE : Fade(COR_CRITICA, 0.6f));

    DrawRectangleRec((Rectangle){ 560, COLUNA_TOPO + 46, 160, 26 }, COR_TELA);
    if (alvo != NULL) {
        /* Alvo travado: tipo, distância e integridade (casco em verde, escudo em azul). */
        float total = alvo->escudoMax + alvo->cascoMax;
        float largura = 156.0f;
        float escudo = largura * alvo->escudo / total;
        float cascoAlvo = largura * alvo->casco / total;
        float base = COLUNA_TOPO + 46 + 23;

        texto(TextFormat("%s  %d u", nomeDoTipo(alvo->tipo), (int)lroundf(Vector3Distance(alvo->pos, g->jogador.pos))),
              (int)JANELA_X, (int)COLUNA_TOPO + 49, 17, 0.5f, COR_ALVO);
        DrawRectangleRec((Rectangle){ 562, base, cascoAlvo, 2 }, COR_TIE);
        DrawRectangleRec((Rectangle){ 562 + cascoAlvo, base, escudo, 2 }, COR_DESTROIER);
    } else {
        texto("SEM ALVO", (int)JANELA_X, (int)COLUNA_TOPO + 48, 20, 0.5f, Fade(COR_ROTULO, 0.6f));
    }

    DrawCircleV(visor, VISOR_RAIO + 6.0f, COR_BRILHO);
    DrawCircleV(visor, VISOR_RAIO, COR_TELA);
    DrawCircleLinesV(visor, VISOR_RAIO - 6.0f, Fade(COR_NEON, 0.7f));
    if (alvo != NULL)
        desenharEscopo(&g->jogador, alvo);
    else
        silhuetaTie((int)visor.x, (int)visor.y - 18, corDoNivel(casco, COR_ALERTA));
    if (corEstado.r != COR_CRITICA.r || piscaAceso())
        texto(estado, (int)visor.x, (int)visor.y + 30, 15, 0.5f, corEstado);
}

/* Aviso piscando no vidro central enquanto a nave está presa na borda da arena. */
static void desenharAvisoArena(void)
{
    Rectangle r = { JANELA_X - 170.0f, 150.0f, 340.0f, 60.0f };
    float alfa = piscaAceso() ? 0.95f : 0.35f;

    DrawRectangleRec(r, Fade(BLACK, 0.6f));
    DrawRectangleLinesEx(r, 2.0f, Fade(COR_CRITICA, alfa));
    texto("LIMITE DA ARENA", (int)JANELA_X, (int)r.y + 4, 28, 0.5f, Fade(COR_CRITICA, alfa));
    texto("volte para a área de combate", (int)JANELA_X, (int)r.y + 36, 16, 0.5f, COR_VALOR);
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

void hud_desenhar(const Jogo *g, int foraDaArena)
{
    desenharFlashDano(g);
    desenharEstrutura();
    desenharReticula();
    if (foraDaArena)
        desenharAvisoArena();
    desenharAlvoNoVidro(g);
    desenharTabletPartida(g);
    desenharTabletNave(g);
    desenharColuna(g, foraDaArena);
}
