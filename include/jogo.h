#ifndef JOGO_H
#define JOGO_H

#include "tipos.h"

#define LARGURA_TELA 1280
#define ALTURA_TELA  720

#define PASSOS_POR_SEGUNDO 60
#define DURACAO_PARTIDA    300.0f
#define PASSOS_PARTIDA     ((int)DURACAO_PARTIDA * PASSOS_POR_SEGUNDO)
#define PASSO_FIXO         (1.0f / PASSOS_POR_SEGUNDO)
#define DT_MAXIMO          0.25f

/* Casco máximo do Destróier na escala usada pela partida (0-100). */
#define CASCO_MAX_DESTROIER 100.0f

/* O tempo da partida vem de passos inteiros, sem somar PASSO_FIXO em float a cada passo. */
typedef struct {
    float acumulador;
    int passos;
} RelogioCombate;

/* Abre a janela, roda o jogo até o jogador sair e libera tudo. */
void jogo_executar(void);

/*
 * Roda os passos fixos que cabem no acumulador; devolve EST_FIM com g->vitoria preenchido ao fim da partida.
 * Os campos de borda de cmd (pausa, mira) são tratados uma vez por frame, fora daqui.
 */
EstadoJogo simularCombate(Jogo *g, RelogioCombate *r, const Comandos *cmd, float dtFrame);

#endif
