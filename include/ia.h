#ifndef IA_H
#define IA_H

#include "tipos.h"

/* Aceleração com que o rumo dos caças é corrigido: sem virar no mesmo frame (IA-01). */
#define ACELERACAO_IA 550.0f

/*
 * Seção 3.4, passo 4: direciona `vel` suavemente para o alvo do estado atual,
 * com a velocidade máxima da tabela 2.2 escalada pela dificuldade (IA-05); a aceleração
 * acompanha o fator para o raio de curva não abrir no fim da partida. Avança `pos` com `vel`.
 * Y-Wing em IA_ATACAR_DESTROIER orbita o Destróier e bombardeia a menos de 685.
 * O caça nunca fica dentro do casco: usa afastarDoDestroier (IA-04).
 * Não reescreve o estado: quem alterna entre atacar o Destróier e o jogador é a IA-03.
 */
void ia_atualizar(Inimigo *in, Jogo *g, float dt);

/*
 * IA-05: a dificuldade cresce ao longo da partida, com os extremos em constantes.
 * A velocidade dos caças vai de 1,0 até FATOR_VELOCIDADE_MAX e o cooldown de tiro de
 * 1,0 até FATOR_COOLDOWN_MIN. A IA-03 (#51) multiplica o cooldown base por
 * fatorCooldownIA ao recarregar `cooldownTiro`; até lá o fator só aparece nos testes.
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
