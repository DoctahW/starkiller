/*
 * Contrato compartilhado entre todos os módulos.
 * Congelado: qualquer mudança exige issue `tipo:contrato` aprovada.
 */
#ifndef TIPOS_H
#define TIPOS_H

#include "raylib.h"

#define MAX_LASERS    256
#define MAX_EXPLOSOES 64
#define MAX_ALIADOS   8   /* 8 TIEs aliados + o jogador */

typedef enum { XWING, YWING, AWING } TipoNave;

typedef enum {
    EST_MENU,
    EST_COMBATE,
    EST_PAUSA,
    EST_FIM,
    EST_PLACAR,
    EST_CONTROLES,
    EST_SAIR
} EstadoJogo;

typedef enum { IA_ATACAR_DESTROIER, IA_ATACAR_JOGADOR, IA_FUGIR, IA_ATACAR_ALIADO, IA_ESCOLTAR, IA_PATRULHAR } EstadoIA;
typedef enum { BOMBA_REAGRUPAR, BOMBA_APROXIMAR, BOMBA_RETIRAR } FaseBombardeio;
typedef enum { ALIADO_DEFENDER, ALIADO_INTERCEPTAR } PapelAliado;

typedef struct {
    Vector3 pos, vel;
    float casco, cooldownTiro;
    float tempoRespawn;           /* s até voltar, quando inativo */
    int ativo, alvoId, fugindo;
    int alvoYwing;                /* id do Y-Wing interceptado, ou -1 */
    PapelAliado papel;
} Aliado;

typedef struct {
    int id;
    TipoNave tipo;
    Vector3 pos, vel;
    float escudo, escudoMax, casco, cascoMax;
    float distancia;              /* até o jogador, recalculada a cada frame */
    float cooldownTiro;
    EstadoIA estadoIA;
    EstadoIA estadoAtaque;        /* estado para o qual volta depois de IA_FUGIR */
    int alvoAliado;               /* índice em Jogo.aliados, ou -1 */
    int escoltaYwing, escoltaYwing2; /* ids dos Y-Wings cobertos, ou -1 */
    int focoDestroier;            /* X-Wing livre atacando o Destróier (escudo em 0) */
    FaseBombardeio faseBombardeio;
    int grupoBombardeio, bombasLancadas;
    float tempoManobra;           /* espera no grupo, duração da run ou da dispersão */
    Vector3 pontoReagrupamento;
} Inimigo;

typedef struct NoInimigo {
    Inimigo dado;
    struct NoInimigo *ant, *prox;
} NoInimigo;

typedef struct {
    NoInimigo *inicio, *fim, *alvo;
    int tamanho;
} ListaInimigos;

typedef struct NoReforco {
    float tempoChegada;
    int qtdX, qtdY, qtdA;
    struct NoReforco *prox;
} NoReforco;

typedef struct {
    NoReforco *inicio, *fim;
    int tamanho;
} FilaReforcos;

typedef struct NoPlacar {
    char nome[16];
    int pontos;
    struct NoPlacar *esq, *dir;
} NoPlacar;

typedef struct {
    Vector3 pos, frente, cima, direita;
    Quaternion rot;
    float velocidade, casco, cooldownLaser, cooldownReparo;
    int morto, explodiu, respawnsRestantes;
    float tempoRespawn;
    float tempoExplosao;          /* s desde a explosão, para o zoom out da câmera */
} Jogador;

typedef struct {
    Vector3 pos, vel;
    float vida;
    int ativo, doJogador;
    int doAliado, torpedo;        /* origem e tipo; zerados a cada reutilização do slot */
    int tipoOrigem;               /* TipoNave que disparou (cor do laser) */
    float dano, gravidade;        /* gravidade (u/s²) da trajetória parabólica; 0 = reta */
} Laser;

typedef struct {
    Vector3 pos;
    float tempo, raioMax;
    int ativa;
} Explosao;

typedef struct {
    /* eixos analógicos normalizados em [-1, 1] (teclado gera -1, 0 ou 1) */
    float arfagem, guinada, rolagem, aceleracao;
    /* disparar vale enquanto segurado; as demais só no frame em que foram pressionadas */
    int disparar, proximoAlvo, alvoAnterior, alvoNaMira, pausa, reparar;
    /* navegação de menus, só no frame em que foram pressionadas */
    int menuCima, menuBaixo, menuEsquerda, menuDireita, confirmar, voltar;
    int usandoControle;           /* 1 se o último dispositivo usado foi o controle */
    int controleConectado;
} Comandos;

typedef struct {
    float sensibilidade;          /* multiplicador dos eixos de rotação, 0.5–2.0 */
    float zonaMorta;              /* padrão 0.15 */
    int inverterArfagem;
    int vibracao;
} ConfigEntrada;

typedef struct {
    Jogador jogador;
    ListaInimigos inimigos;
    Aliado aliados[MAX_ALIADOS];
    FilaReforcos reforcos;
    NoPlacar *placar;
    Laser lasers[MAX_LASERS];
    Explosao explosoes[MAX_EXPLOSOES];
    float cascoDestroier, tempoRestante, tempoDecorrido;
    float escudoDestroier, esperaEscudoDestroier; /* escudo e s até a recarga (destroier.h) */
    int aliadosIniciados;      /* 1 depois de iniciarAliados; evita ativar aliados em testes vazios */
    float tempoDano;   /* s restantes do feedback de dano (tremor e flash); armado por registrarDanoJogador */
    int pontos, sequenciaSemDano, abates, proximoId;
    EstadoJogo estado;
    int vitoria;
} Jogo;

#endif
