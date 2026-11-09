/*
 * Contrato compartilhado entre todos os módulos.
 * Congelado: qualquer mudança exige issue `tipo:contrato` aprovada.
 */
#ifndef TIPOS_H
#define TIPOS_H

#include "raylib.h"

#define MAX_LASERS    256
#define MAX_EXPLOSOES 64

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

typedef enum { IA_ATACAR_DESTROIER, IA_ATACAR_JOGADOR, IA_FUGIR } EstadoIA;

typedef struct {
    int id;
    TipoNave tipo;
    Vector3 pos, vel;
    float escudo, escudoMax, casco, cascoMax;
    float distancia;              /* até o jogador, recalculada a cada frame */
    float cooldownTiro;
    EstadoIA estadoIA;
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
    float velocidade, casco, cooldownLaser;
} Jogador;

typedef struct {
    Vector3 pos, vel;
    float vida;
    int ativo, doJogador;
    float dano;
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
    int disparar, proximoAlvo, alvoAnterior, alvoMaisProximo, pausa;
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
    FilaReforcos reforcos;
    NoPlacar *placar;
    Laser lasers[MAX_LASERS];
    Explosao explosoes[MAX_EXPLOSOES];
    float cascoDestroier, tempoRestante, tempoDecorrido;
    int pontos, sequenciaSemDano, abates, proximoId;
    EstadoJogo estado;
    int vitoria;
} Jogo;

#endif
