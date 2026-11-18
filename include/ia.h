#ifndef IA_H
#define IA_H

#include "destroier.h"
#include "tipos.h"

/* Aceleração com que o rumo dos caças é corrigido: sem virar no mesmo frame (IA-01). */
#define ACELERACAO_IA 550.0f

/*
 * Seção 3.4, passo 4: direciona `vel` suavemente para o alvo do estado atual, com a velocidade
 * máxima da tabela 2.2 escalada pela dificuldade; a aceleração acompanha o quadrado do fator.
 * O caça nunca fica dentro do casco: usa afastarDoDestroier, com uma antecipação de rumo.
 *
 * Rebeldes:
 * - Y-Wing: faz passagens de bombardeio. Reagrupa na frente da proa do Destróier (~1500 u),
 *   acelera para YWING_VEL_RUN e segue reto, soltando até duas bombas em parábola. Depois
 *   dispersa por YWING_TEMPO_DISPERSAO e volta a se posicionar. Fora da run, defende-se com
 *   laser balístico a 20 tiros/min.
 * - X-Wing: prioriza caças TIE (jogador e aliados). Um X-Wing cobre até dois Y-Wings
 *   (escolta); fora da escolta, só troca o foco para o Destróier quando o escudo dele zera,
 *   com 10% de chance por janela de decisão.
 * - A-Wing: sempre prioriza caças TIE.
 * - Todos os tipos se afastam de outro caça a menos de DIST_SEPARACAO.
 */
void ia_atualizar(Inimigo *in, Jogo *g, float dt);

/* Atribui os X-Wings de escolta aos pares de Y-Wings. Roda uma vez por frame em atualizarInimigos. */
void ia_atribuirEscolta(Jogo *g);

/* Passagens de bombardeio. O Y-Wing se alinha a ~3000 u da proa, desce para uma corrida
 * baixa sobre o casco (termina antes da torre de comando) e solta até duas bombas. */
#define YWING_ALTURA_ESTAGIO     1400.0f  /* ponto de alinhamento, acima do casco */
#define YWING_ALTURA_RUN          700.0f  /* corrida baixa, logo acima do convés */
#define YWING_DIST_ESTAGIO       3000.0f  /* da proa do Destróier até o ponto de alinhamento */
#define YWING_FIM_RUN_Z          -900.0f  /* antes da torre de comando (topo ~1140) */
#define YWING_VEL_RUN             450.0f  /* velocidade da corrida reta */
#define YWING_RAIO_ESTAGIO        600.0f
#define YWING_ESPERA_GRUPO          8.0f
#define YWING_TEMPO_DISPERSAO      10.0f
#define YWING_TEMPO_RUN            25.0f
#define YWING_DIST_SOLTURA        900.0f  /* distância horizontal em que a bomba já cai */
#define YWING_INTERVALO_BOMBAS      1.2f
#define YWING_MAX_BOMBAS              2
#define ALCANCE_BOMBA_YWING        2000.0f
/* 1 na aproximação de uma passagem, dentro do alcance de lançamento. */
int ywingBombardeando(const Inimigo *in);
/* Ponto de alinhamento da run, à frente da proa e acima do casco; variado pelo id. */
Vector3 ywingPontoEstagio(int id);

/* Cadência de tiro dos rebeldes, em segundos por disparo. */
#define COOLDOWN_TIRO_XWING   0.20f     /* 300 tiros/min */
#define COOLDOWN_TIRO_AWING   (1.0f / 6.0f) /* 360 tiros/min */
#define COOLDOWN_TIRO_YWING   3.0f      /* 20 tiros/min, só fora da run */

/* Passagem de ataque dos caças. O Destróier rompe a 1100 da origem: fora do casco e dentro do alcance. */
#define DIST_ROMPER_JOGADOR    600.0f
#define DIST_RETOMAR_JOGADOR   1600.0f
#define DIST_ROMPER_DESTROIER  (850.0f * ESCALA_DESTROIER)
#define DIST_RETOMAR_DESTROIER (1750.0f * ESCALA_DESTROIER)
/* Na fuga, a menos disto da borda da arena o rumo perde a componente para fora. */
#define MARGEM_BORDA_FUGA      500.0f
/* Separação entre caças: alcance (u) e peso do empurrão no rumo unitário, no máximo PESO por vizinho. */
#define DIST_SEPARACAO 90.0f
#define PESO_SEPARACAO 1.5f

/* Alcance de seleção de caças TIE e distância em que um X-Wing acompanha o Y-Wing escoltado. */
#define ALCANCE_ALVO_TIE       8000.0f
#define ALCANCE_AMEACA_ESCOLTA 1800.0f
#define DIST_ESCOLTA           450.0f

/* IA-03: alvo no cone de 15° e a menos de 1840 (400 × 4,6, a escala do mundo). */
#define ALCANCE_TIRO_INIMIGO  1840.0f
#define COSSENO_CONE_TIRO     0.9659258f   /* cos(15°) */

/* IA-05: a dificuldade cresce ao longo da partida, com os extremos em constantes. */
#define FATOR_VELOCIDADE_MAX 1.3f
#define FATOR_COOLDOWN_MIN   0.7f

/* Progresso da partida em [0, 1] sobre DURACAO_PARTIDA; fora do intervalo, fica nos extremos. */
float progressoDificuldade(float tempoDecorrido);
/* Multiplicador da velocidade dos caças: 1,0 no início, FATOR_VELOCIDADE_MAX no fim. */
float fatorVelocidadeIA(float tempoDecorrido);
/* Multiplicador de cooldown (mantido para compatibilidade; a cadência nova usa valores fixos). */
float fatorCooldownIA(float tempoDecorrido);

#endif
