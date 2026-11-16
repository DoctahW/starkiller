#ifndef COMBATE_H
#define COMBATE_H

#include "tipos.h"

#define COOLDOWN_LASER   0.15f   /* s entre disparos do jogador */
#define VEL_LASER        600.0f  /* u/s */
#define VIDA_LASER       2.0f    /* s até o laser desativar */
#define TOLERANCIA_COOLDOWN 1e-6f /* s; absorve o erro de float de 9 passos de 1/60 s */
#define DANO_LASER       10.0f
#define CANHAO_LATERAL   3.0f    /* distância de cada canhão ao eixo da nave */
#define CANHAO_FRENTE    5.0f    /* canhões à frente do centro da nave */

/* Raio da esfera de colisão de cada tipo de caça rebelde (chutes; ajustar com os modelos). */
#define RAIO_XWING       8.0f
#define RAIO_YWING       10.0f
#define RAIO_AWING       6.0f

/*
 * Dispara os dois lasers do TIE (canhões esquerdo e direito) na direção da frente.
 * Respeita o cooldown de 0,15 s (jogador.cooldownLaser). Devolve 1 se disparou; 0 se
 * estava em cooldown ou o pool não tem 2 slots livres (nesse caso o cooldown não é gasto).
 */
int dispararLaser(Jogo *g);
/*
 * Diminui o cooldown e move os lasers ativos. Um laser que esgota a vida neste passo continua ativo
 * até o próximo atualizarLasers, para colidirLasers ainda testar o último segmento dele.
 */
void atualizarLasers(Jogo *g, float dt);

/* Raio da esfera de colisão do caça rebelde de tipo `t`. */
float raioInimigo(TipoNave t);
/*
 * Interseção do segmento a→b com a esfera (centro, raio). Devolve 1 se acerta e grava em
 * *t (se não for NULL) a fração [0, 1] do segmento onde ele entra na esfera (0 se já começa
 * dentro). Considera o caminho todo, então não deixa passar um laser rápido por uma esfera pequena.
 */
int segmentoAcertaEsfera(Vector3 a, Vector3 b, Vector3 centro, float raio, float *t);
/*
 * Colisão laser do jogador × inimigos. Deve rodar logo depois de atualizarLasers(g, dt): o segmento
 * de cada laser vai da posição anterior (pos - vel * dt) à atual. Cada laser acerta no máximo
 * um inimigo (o primeiro que o segmento atravessa) e é desativado. Devolve o número de acertos.
 */
int colidirLasers(Jogo *g, float dt);

/*
 * Aplica `dano` ao inimigo: consome o escudo primeiro e o que sobrar vai para o casco. Escudo e casco
 * nunca ficam negativos. Dano <= 0 não faz nada. Devolve 1 se o casco chegou a 0 (inimigo abatido).
 * Não mexe na lista nem na pontuação: quem chama decide o abate.
 */
int aplicarDano(Inimigo *in, float dano);

/*
 * Como colidirLasers, mas aplica o dano do laser (aplicarDano) ao inimigo atingido. Se o casco zera:
 * registrarAbate, explosão na posição do caça e removerInimigo (o alvo da mira continua válido).
 * Como cada laser acerta no máximo um inimigo, remover durante a varredura de lasers é seguro.
 * Devolve o número de acertos.
 */
int resolverLasers(Jogo *g, float dt);

#define DURACAO_EXPLOSAO 0.6f    /* s de vida de uma explosão */
#define RAIO_EXPLOSAO    20.0f   /* raio máximo (chute; ajustar com o visual) */

/*
 * Ativa uma explosão em `pos` no primeiro slot livre de g->explosoes (tempo = 0). `tempo` é o tempo
 * decorrido desde o início, até DURACAO_EXPLOSAO. Com o pool cheio, ignora. Devolve 1 se criou.
 */
int criarExplosao(Jogo *g, Vector3 pos, float raioMax);
/* Avança o tempo das explosões ativas e desativa as que passaram de DURACAO_EXPLOSAO. */
void atualizarExplosoes(Jogo *g, float dt);

#endif
