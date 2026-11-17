#ifndef IA_H
#define IA_H

#include "tipos.h"

/* Aceleração com que o rumo dos caças é corrigido: sem virar no mesmo frame (IA-01). */
#define ACELERACAO_IA 550.0f

/*
 * Seção 3.4, passo 4: direciona `vel` suavemente para o alvo do estado atual,
 * com a velocidade máxima da tabela 2.2 escalada pela dificuldade (IA-05); a aceleração
 * acompanha o quadrado do fator, então o raio de curva (v²/a) fica constante. Avança `pos` com `vel`.
 * Y-Wing em IA_ATACAR_DESTROIER orbita o Destróier e bombardeia a menos de 685.
 * O caça nunca fica dentro do casco: usa afastarDoDestroier (IA-04).
 * IA-03: o X-Wing alterna entre atacar o Destróier e o jogador com período de 8–12 s;
 * a primeira troca pode ser menor se o caça nasce no meio do ciclo. O A-Wing persegue o
 * jogador. X-Wing e A-Wing atiram no cone de 15° a menos de ALCANCE_TIRO_INIMIGO,
 * respeitando `cooldownTiro`.
 */
void ia_atualizar(Inimigo *in, Jogo *g, float dt);

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
