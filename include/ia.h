#ifndef IA_H
#define IA_H

#include "destroier.h"
#include "tipos.h"

/* Aceleração com que o rumo dos caças é corrigido: sem virar no mesmo frame (IA-01). */
#define ACELERACAO_IA 550.0f

/*
 * Seção 3.4, passo 4: direciona `vel` suavemente para o alvo do estado atual,
 * com a velocidade máxima da tabela 2.2 escalada pela dificuldade (IA-05); a aceleração
 * acompanha o quadrado do fator, então o raio de curva (v²/a) fica constante. Avança `pos` com `vel`.
 * Y-Wing em IA_ATACAR_DESTROIER orbita o Destróier e bombardeia a menos de 685 (no tamanho
 * canônico; a distância acompanha ESCALA_DESTROIER).
 * O caça nunca fica dentro do casco: usa afastarDoDestroier (IA-04).
 * IA-03: o X-Wing alterna entre atacar o Destróier e o jogador com período de 8–12 s;
 * a primeira troca pode ser menor se o caça nasce no meio do ciclo. O A-Wing persegue o
 * jogador. X-Wing e A-Wing atiram no cone de 15° a menos de ALCANCE_TIRO_INIMIGO,
 * respeitando `cooldownTiro`.
 * Passagem de ataque: X-Wing e A-Wing não voam até o ponto do alvo. A menos de DIST_ROMPER_*
 * dele, rompem (IA_FUGIR): afastam-se do alvo do seu estado de ataque, abrindo para o lado se
 * vinham de frente e deslizando na borda da arena. Além de DIST_RETOMAR_*, voltam a atacar.
 * Todos os tipos se afastam de outro caça a menos de DIST_SEPARACAO, para não se empilharem.
 */
void ia_atualizar(Inimigo *in, Jogo *g, float dt);

/* Passagem de ataque. O Destróier rompe a 1100 da origem: fora do casco (até ~900) e dentro do alcance. */
#define DIST_ROMPER_JOGADOR    600.0f
#define DIST_RETOMAR_JOGADOR   1600.0f
/* A escala do Destróier entra aqui: 850 u canônicos ficam logo à frente da proa (823). */
#define DIST_ROMPER_DESTROIER  (850.0f * ESCALA_DESTROIER)
#define DIST_RETOMAR_DESTROIER (1750.0f * ESCALA_DESTROIER)
/* Na fuga, a menos disto da borda da arena o rumo perde a componente para fora. */
#define MARGEM_BORDA_FUGA      500.0f
/* Separação entre caças: alcance (u) e peso do empurrão no rumo unitário, no máximo PESO por vizinho. */
#define DIST_SEPARACAO 60.0f
#define PESO_SEPARACAO 1.5f

/* IA-03: intervalo de troca de alvo do X-Wing; o id do caça varia entre MIN e MAX. */
#define TROCA_XWING_MIN      8.0f
#define TROCA_XWING_VARIACAO 5     /* id % 5 -> 8..12 s por estado */

/* IA-03: tiro inimigo no cone de 15° e a menos de 1840 (400 × 4,6, a escala do mundo). */
#define ALCANCE_TIRO_INIMIGO  1840.0f
#define COSSENO_CONE_TIRO     0.9659258f   /* cos(15°) */
#define COOLDOWN_TIRO_INIMIGO 1.5f

/*
 * IA-05: a dificuldade cresce ao longo da partida, com os extremos em constantes.
 * A velocidade dos caças vai de 1,0 até FATOR_VELOCIDADE_MAX e o cooldown de tiro de
 * 1,0 até FATOR_COOLDOWN_MIN. A IA-03 multiplica o cooldown base por fatorCooldownIA
 * ao recarregar `cooldownTiro`.
 */
#define FATOR_VELOCIDADE_MAX 1.3f
#define FATOR_COOLDOWN_MIN   0.7f

/* Progresso da partida em [0, 1] sobre DURACAO_PARTIDA; fora do intervalo, fica nos extremos. */
float progressoDificuldade(float tempoDecorrido);
/* Multiplicador da velocidade dos caças: 1,0 no início, FATOR_VELOCIDADE_MAX no fim. */
float fatorVelocidadeIA(float tempoDecorrido);
/* Multiplicador do cooldown de tiro: 1,0 no início, FATOR_COOLDOWN_MIN no fim. */
float fatorCooldownIA(float tempoDecorrido);

#endif
