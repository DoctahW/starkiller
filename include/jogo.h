#ifndef JOGO_H
#define JOGO_H

#include "tipos.h"

#define LARGURA_TELA 1280
#define ALTURA_TELA  720

#define PASSOS_POR_SEGUNDO 60
#define DURACAO_ONDAS     300.0f   /* janela de geração das ondas (20 ondas) */
#define DURACAO_PARTIDA   320.0f   /* +20 s para a última onda durar mais */
#define PASSOS_PARTIDA     ((int)DURACAO_PARTIDA * PASSOS_POR_SEGUNDO)
#define PASSO_FIXO         (1.0f / PASSOS_POR_SEGUNDO)
#define DT_MAXIMO          0.25f

/* Casco máximo do Destróier na escala usada pela partida (0 a 1200). */
#define CASCO_MAX_DESTROIER 1200.0f

/* O tempo da partida vem de passos inteiros, sem somar PASSO_FIXO em float a cada passo. */
typedef struct {
    float acumulador;
    int passos;
} RelogioCombate;

/* Abre a janela, roda o jogo até o jogador sair e libera tudo. */
void jogo_executar(void);

/*
 * Um frame de combate na ordem da seção 3.4: aplica as ações de mira de cmd uma vez (são de borda),
 * depois roda os passos fixos que cabem no acumulador (nave, disparo, lasers, colisões, fim).
 * Devolve EST_FIM com g->vitoria preenchido ao fim da partida. A pausa é tratada fora daqui.
 */
EstadoJogo simularCombate(Jogo *g, RelogioCombate *r, const Comandos *cmd, float dtFrame);

#endif
