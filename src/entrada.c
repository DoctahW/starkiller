#include "entrada.h"

#include <math.h>

#define CONTROLE          0       /* índice do gamepad lido */
#define LIMIAR_DIGITAL    0.5f    /* gatilho/analógico acima disso conta como pressionado */

/* Ações digitais; cada uma tem até duas teclas e um botão do controle (tabela 2.1). */
typedef enum {
    ACAO_ARFAGEM_BAIXO, ACAO_ARFAGEM_CIMA,
    ACAO_GUINADA_ESQUERDA, ACAO_GUINADA_DIREITA,
    ACAO_ROLAGEM_ESQUERDA, ACAO_ROLAGEM_DIREITA,
    ACAO_ACELERAR, ACAO_FREAR,
    ACAO_DISPARAR, ACAO_PROXIMO_ALVO, ACAO_ALVO_ANTERIOR, ACAO_ALVO_MAIS_PROXIMO,
    ACAO_PAUSA,
    ACAO_MENU_CIMA, ACAO_MENU_BAIXO, ACAO_MENU_ESQUERDA, ACAO_MENU_DIREITA,
    ACAO_CONFIRMAR, ACAO_VOLTAR,
    ACAO_QTD
} Acao;

typedef struct {
    int teclas[2];  /* KEY_NULL = sem tecla */
    int botao;      /* GAMEPAD_BUTTON_UNKNOWN = sem botão (eixo ou gatilho, lido à parte) */
} Vinculo;

static const Vinculo vinculos[ACAO_QTD] = {
    [ACAO_ARFAGEM_BAIXO]     = { { KEY_W },                              GAMEPAD_BUTTON_UNKNOWN },
    [ACAO_ARFAGEM_CIMA]      = { { KEY_S },                              GAMEPAD_BUTTON_UNKNOWN },
    [ACAO_GUINADA_ESQUERDA]  = { { KEY_A },                              GAMEPAD_BUTTON_UNKNOWN },
    [ACAO_GUINADA_DIREITA]   = { { KEY_D },                              GAMEPAD_BUTTON_UNKNOWN },
    [ACAO_ROLAGEM_ESQUERDA]  = { { KEY_Q },                              GAMEPAD_BUTTON_LEFT_TRIGGER_1 },
    [ACAO_ROLAGEM_DIREITA]   = { { KEY_E },                              GAMEPAD_BUTTON_RIGHT_TRIGGER_1 },
    [ACAO_ACELERAR]          = { { KEY_LEFT_SHIFT, KEY_RIGHT_SHIFT },     GAMEPAD_BUTTON_UNKNOWN },
    [ACAO_FREAR]             = { { KEY_LEFT_CONTROL, KEY_RIGHT_CONTROL }, GAMEPAD_BUTTON_UNKNOWN },
    [ACAO_DISPARAR]          = { { KEY_SPACE },                          GAMEPAD_BUTTON_UNKNOWN },
    [ACAO_PROXIMO_ALVO]      = { { KEY_T },                              GAMEPAD_BUTTON_LEFT_FACE_RIGHT },
    [ACAO_ALVO_ANTERIOR]     = { { KEY_Y },                              GAMEPAD_BUTTON_LEFT_FACE_LEFT },
    [ACAO_ALVO_MAIS_PROXIMO] = { { KEY_R },                              GAMEPAD_BUTTON_UNKNOWN },
    [ACAO_PAUSA]             = { { KEY_ESCAPE },                         GAMEPAD_BUTTON_MIDDLE_RIGHT },
    [ACAO_MENU_CIMA]         = { { KEY_UP },                             GAMEPAD_BUTTON_LEFT_FACE_UP },
    [ACAO_MENU_BAIXO]        = { { KEY_DOWN },                           GAMEPAD_BUTTON_LEFT_FACE_DOWN },
    [ACAO_MENU_ESQUERDA]     = { { KEY_LEFT },                           GAMEPAD_BUTTON_LEFT_FACE_LEFT },
    [ACAO_MENU_DIREITA]      = { { KEY_RIGHT },                          GAMEPAD_BUTTON_LEFT_FACE_RIGHT },
    [ACAO_CONFIRMAR]         = { { KEY_ENTER },                          GAMEPAD_BUTTON_RIGHT_FACE_DOWN },
    [ACAO_VOLTAR]            = { { KEY_ESCAPE, KEY_BACKSPACE },          GAMEPAD_BUTTON_RIGHT_FACE_RIGHT },
};

/* Estado entre frames para detecção de borda em entradas analógicas usadas como digitais. */
typedef struct {
    int gatilhoEsquerdo;
    int menuCima, menuBaixo, menuEsquerda, menuDireita;
} BordasControle;

static BordasControle bordas;
static int usandoControle;

void iniciarEntrada(ConfigEntrada *cfg)
{
    cfg->sensibilidade = 1.0f;
    cfg->zonaMorta = 0.15f;
    cfg->inverterArfagem = 0;
    cfg->vibracao = 1;
}

static int teclaSegurada(Acao a)
{
    for (int i = 0; i < 2; i++)
        if (vinculos[a].teclas[i] != KEY_NULL && IsKeyDown(vinculos[a].teclas[i]))
            return 1;
    return 0;
}

static int teclaPressionada(Acao a)
{
    for (int i = 0; i < 2; i++)
        if (vinculos[a].teclas[i] != KEY_NULL && IsKeyPressed(vinculos[a].teclas[i]))
            return 1;
    return 0;
}

static int botaoSegurado(Acao a)
{
    return vinculos[a].botao != GAMEPAD_BUTTON_UNKNOWN && IsGamepadButtonDown(CONTROLE, vinculos[a].botao);
}

static int botaoPressionado(Acao a)
{
    return vinculos[a].botao != GAMEPAD_BUTTON_UNKNOWN && IsGamepadButtonPressed(CONTROLE, vinculos[a].botao);
}

static float eixoTeclado(Acao negativo, Acao positivo)
{
    return (float)(teclaSegurada(positivo) - teclaSegurada(negativo));
}

/* Zerado dentro da zona morta; fora dela, reescalado para que a saída ainda vá de 0 a 1. */
static float aplicarZonaMorta(float v, float zona)
{
    float m = fabsf(v);
    if (m <= zona)
        return 0.0f;
    m = (m - zona) / (1.0f - zona);
    if (m > 1.0f)
        m = 1.0f;
    return v < 0.0f ? -m : m;
}

/* No backend SDL os gatilhos vão de 0 (solto) a 1; antes do primeiro movimento valem -1. */
static int gatilhoPressionado(int eixo)
{
    return GetGamepadAxisMovement(CONTROLE, eixo) > LIMIAR_DIGITAL;
}

/* 1 só no frame em que `agora` passa de 0 para 1. */
static int borda(int agora, int *anterior)
{
    int subiu = agora && !*anterior;
    *anterior = agora;
    return subiu;
}

static float limitar(float v)
{
    return v < -1.0f ? -1.0f : v > 1.0f ? 1.0f : v;
}

static Comandos lerTeclado(void)
{
    Comandos c = {0};
    /* arfagem negativa = nariz para baixo (W) */
    c.arfagem = eixoTeclado(ACAO_ARFAGEM_BAIXO, ACAO_ARFAGEM_CIMA);
    c.guinada = eixoTeclado(ACAO_GUINADA_ESQUERDA, ACAO_GUINADA_DIREITA);
    c.rolagem = eixoTeclado(ACAO_ROLAGEM_ESQUERDA, ACAO_ROLAGEM_DIREITA);
    c.aceleracao = eixoTeclado(ACAO_FREAR, ACAO_ACELERAR);
    c.disparar = teclaSegurada(ACAO_DISPARAR);
    c.proximoAlvo = teclaPressionada(ACAO_PROXIMO_ALVO);
    c.alvoAnterior = teclaPressionada(ACAO_ALVO_ANTERIOR);
    c.alvoMaisProximo = teclaPressionada(ACAO_ALVO_MAIS_PROXIMO);
    c.pausa = teclaPressionada(ACAO_PAUSA);
    c.menuCima = teclaPressionada(ACAO_MENU_CIMA);
    c.menuBaixo = teclaPressionada(ACAO_MENU_BAIXO);
    c.menuEsquerda = teclaPressionada(ACAO_MENU_ESQUERDA);
    c.menuDireita = teclaPressionada(ACAO_MENU_DIREITA);
    c.confirmar = teclaPressionada(ACAO_CONFIRMAR);
    c.voltar = teclaPressionada(ACAO_VOLTAR);
    return c;
}

static Comandos lerControle(const ConfigEntrada *cfg)
{
    Comandos c = {0};
    float bx = GetGamepadAxisMovement(CONTROLE, GAMEPAD_AXIS_LEFT_X);
    float by = GetGamepadAxisMovement(CONTROLE, GAMEPAD_AXIS_LEFT_Y);

    /* analógico esquerdo para cima (Y negativo) = nariz para baixo, como W */
    c.arfagem = aplicarZonaMorta(by, cfg->zonaMorta);
    c.guinada = aplicarZonaMorta(bx, cfg->zonaMorta);
    c.rolagem = (float)(botaoSegurado(ACAO_ROLAGEM_DIREITA) - botaoSegurado(ACAO_ROLAGEM_ESQUERDA));
    c.aceleracao = -aplicarZonaMorta(GetGamepadAxisMovement(CONTROLE, GAMEPAD_AXIS_RIGHT_Y), cfg->zonaMorta);
    c.disparar = gatilhoPressionado(GAMEPAD_AXIS_RIGHT_TRIGGER);
    c.proximoAlvo = botaoPressionado(ACAO_PROXIMO_ALVO);
    c.alvoAnterior = botaoPressionado(ACAO_ALVO_ANTERIOR);
    c.alvoMaisProximo = borda(gatilhoPressionado(GAMEPAD_AXIS_LEFT_TRIGGER), &bordas.gatilhoEsquerdo);
    c.pausa = botaoPressionado(ACAO_PAUSA);
    c.menuCima = botaoPressionado(ACAO_MENU_CIMA) | borda(by < -LIMIAR_DIGITAL, &bordas.menuCima);
    c.menuBaixo = botaoPressionado(ACAO_MENU_BAIXO) | borda(by > LIMIAR_DIGITAL, &bordas.menuBaixo);
    c.menuEsquerda = botaoPressionado(ACAO_MENU_ESQUERDA) | borda(bx < -LIMIAR_DIGITAL, &bordas.menuEsquerda);
    c.menuDireita = botaoPressionado(ACAO_MENU_DIREITA) | borda(bx > LIMIAR_DIGITAL, &bordas.menuDireita);
    c.confirmar = botaoPressionado(ACAO_CONFIRMAR);
    c.voltar = botaoPressionado(ACAO_VOLTAR);
    return c;
}

static int temEntrada(const Comandos *c)
{
    return c->arfagem != 0.0f || c->guinada != 0.0f || c->rolagem != 0.0f || c->aceleracao != 0.0f
        || c->disparar || c->proximoAlvo || c->alvoAnterior || c->alvoMaisProximo || c->pausa
        || c->menuCima || c->menuBaixo || c->menuEsquerda || c->menuDireita || c->confirmar || c->voltar;
}

/* Teclado e controle são lidos juntos: eixos somados e limitados, botões combinados por OU. */
Comandos lerComandos(const ConfigEntrada *cfg)
{
    Comandos t = lerTeclado();
    Comandos g = {0};
    int conectado = IsGamepadAvailable(CONTROLE);

    if (conectado) {
        g = lerControle(cfg);
    } else {
        bordas = (BordasControle){0};
        usandoControle = 0;
    }

    int doTeclado = temEntrada(&t), doControle = temEntrada(&g);
    if (doControle && !doTeclado)
        usandoControle = 1;
    else if (doTeclado && !doControle)
        usandoControle = 0;

    Comandos c;
    c.arfagem = limitar(t.arfagem + g.arfagem);
    c.guinada = limitar(t.guinada + g.guinada);
    c.rolagem = limitar(t.rolagem + g.rolagem);
    c.aceleracao = limitar(t.aceleracao + g.aceleracao);
    c.disparar = t.disparar | g.disparar;
    c.proximoAlvo = t.proximoAlvo | g.proximoAlvo;
    c.alvoAnterior = t.alvoAnterior | g.alvoAnterior;
    c.alvoMaisProximo = t.alvoMaisProximo | g.alvoMaisProximo;
    c.pausa = t.pausa | g.pausa;
    c.menuCima = t.menuCima | g.menuCima;
    c.menuBaixo = t.menuBaixo | g.menuBaixo;
    c.menuEsquerda = t.menuEsquerda | g.menuEsquerda;
    c.menuDireita = t.menuDireita | g.menuDireita;
    c.confirmar = t.confirmar | g.confirmar;
    c.voltar = t.voltar | g.voltar;
    c.usandoControle = usandoControle;
    c.controleConectado = conectado;
    return c;
}

/* SetGamepadVibration (SDL_RumbleGamepad) já limita intensidade a [0, 1] e duração a 2 s. */
void vibrar(const ConfigEntrada *cfg, float intensidade, float duracao)
{
    if (cfg->vibracao && IsGamepadAvailable(CONTROLE))
        SetGamepadVibration(CONTROLE, intensidade, intensidade, duracao);
}
