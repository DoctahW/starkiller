#ifndef NAVE_H
#define NAVE_H

#include "tipos.h"

#define FOV_JOGADOR 70.0f   /* graus */

/* Casco do TIE: não regenera. O reparo de campo devolve REPARO_JOGADOR a cada uso. */
#define CASCO_JOGADOR 125.0f
#define REPARO_JOGADOR 25.0f
#define COOLDOWN_REPARO 15.0f

/* O jogador começa com uma vida e pode voltar mais RESPAWNS_JOGADOR vezes. */
#define RESPAWNS_JOGADOR 3
#define TEMPO_RESPAWN_JOGADOR 5.0f
#define PENALIDADE_MORTE_JOGADOR 500  /* cinco abates base de X-Wing */

/*
 * Tira `dano` direto do casco (o TIE não tem escudo), sem deixar negativo.
 * Reinicia a sequência sem dano e chama registrarDanoJogador. Dano <= 0 não faz nada.
 */
void danificarJogador(Jogo *g, float dano);
/* Consome o reparo se o casco não está cheio e o cooldown zerou. Devolve 1 se reparou. */
int repararJogador(Jogo *g);
/* Volta o jogador ao ponto de nascimento abaixo do Destróier, gastando um respawn. */
int respawnJogador(Jogo *g);

/* Velocidades angulares máximas (rad/s), atingidas com o eixo em ±1. */
#define VEL_ARFAGEM 1.5f
#define VEL_GUINADA 1.0f
#define VEL_ROLAGEM 2.0f

/* Jogador perto do Destróier (na origem), olhando para fora da arena. Zera a velocidade angular. */
void iniciarJogador(Jogador *j);
/* Ponto de nascimento comum ao jogador e aos TIEs aliados, abaixo do casco do Destróier. */
#define SPAWN_ALA_Y -1200.0f
#define SPAWN_ALA_RAIO 180.0f
/* Constante de tempo (s) da suavização da rotação: ~63% da velocidade angular alvo em TAU_GIRO. */
#define TAU_GIRO    0.1f

/*
 * Queda do TIE: em vez de obedecer aos comandos, a nave gira incontrolavelmente em torno de um
 * eixo aleatório e continua avançando com a velocidade atual. Devolve a duração sorteada do giro,
 * entre 1,0 s e a duração do áudio de SPIN (maior que 1 segundo, como pede a AUD-01).
 * iniciarJogador cancela a queda.
 */
float iniciarQuedaJogador(void);
/* Fase da queda: 0 = não começou, 1 = girando, 2 = giro terminou (pronto para explodir). */
int faseQuedaJogador(void);

/*
 * Limites de velocidade (u/s) e aceleração com o eixo em ±1 (u/s²). Escala do mundo: 1 u = 1 m,
 * com o Destróier no tamanho canônico (1600 m); distâncias e velocidades foram multiplicadas por
 * ~4,6 junto com ele, para o ritmo da partida não mudar. O tamanho dos caças não mudou.
 */
#define VEL_MIN     230.0f
#define VEL_MAX     600.0f
#define ACELERACAO  180.0f

/* Raio da arena, centrada na origem. */
#define RAIO_ARENA  9000.0f

/*
 * Gira a nave no espaço local conforme os eixos de cmd, com a velocidade angular suavizada
 * (aproxima-se do alvo com constante de tempo TAU_GIRO, sem giro instantâneo) (arfagem > 0 = nariz para cima,
 * guinada > 0 = nariz para a direita, rolagem > 0 = asa direita para baixo),
 * renormaliza rot e recalcula frente/cima/direita. O eixo aceleracao acelera/freia
 * a velocidade, limitada a [VEL_MIN, VEL_MAX], e a posição avança frente * velocidade * dt.
 * Se a posição passar de RAIO_ARENA, a nave é devolvida à borda da esfera e a função
 * devolve 1 (aviso de limite da arena para o HUD); caso contrário, 0.
 */
int atualizarJogador(Jogador *j, const Comandos *cmd, float dt);

/* Esfera de colisão do TIE (low_poly_tie_fighter.glb a 3,6×: 1,5× a escala anterior). */
#define RAIO_JOGADOR            9.0f
#define DANO_COLISAO_DESTROIER  10.0f
/* Depois de um dano, os contatos seguintes só empurram a nave durante este tempo (s), sem novo dano. */
#define RECARGA_COLISAO         0.5f

/*
 * Colisão do TIE com o Destróier (casco de destroier.h). Encostar no casco empurra a nave para
 * fora pelo caminho mais curto (afastarDoDestroier) e causa DANO_COLISAO_DESTROIER ao jogador
 * (danificarJogador: escudo, depois casco; zera a sequência sem dano). Durante RECARGA_COLISAO
 * depois de um dano só empurra, para a nave encostada não perder vida a cada passo.
 * Roda depois de atualizarJogador. Devolve 1 se houve contato neste passo; 0 caso contrário.
 */
int colidirDestroier(Jogo *g, float dt);
/* Câmera de cabine: position = pos, target = pos + frente, up = cima. */
Camera3D cameraDoJogador(const Jogador *j);

/* Deslocamento máximo (unidades do mundo) do tremor, no começo do feedback de dano. */
#define AMPLITUDE_TREMOR 1.5f
/*
 * Câmera de cabine com tremor: desloca position e target pelo mesmo vetor (no plano frente-cima), então
 * a vista balança sem girar. A amplitude cai linearmente de AMPLITUDE_TREMOR a 0 em DURACAO_FEEDBACK_DANO.
 * Determinística em (g->tempoDano, g->tempoDecorrido); tempoDano <= 0 devolve cameraDoJogador.
 */
Camera3D cameraComTremor(const Jogo *g);

#endif
