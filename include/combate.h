#ifndef COMBATE_H
#define COMBATE_H

#include "tipos.h"

#define COOLDOWN_LASER   0.20f   /* s entre disparos do jogador: 300 tiros/min */
#define VEL_LASER        2750.0f /* u/s */
#define VIDA_LASER       2.0f    /* s até o laser desativar */
#define TOLERANCIA_COOLDOWN 1e-6f /* s; absorve o erro de float de 9 passos de 1/60 s */
#define DANO_LASER       10.0f
#define CANHAO_LATERAL   4.5f    /* distância de cada canhão ao eixo da nave (1,5×) */
#define CANHAO_FRENTE    7.5f    /* canhões à frente do centro da nave (1,5×) */

/* Raio da esfera de colisão de cada tipo de caça rebelde, 1,5× a escala anterior. */
#define RAIO_XWING       12.0f
#define RAIO_YWING       15.0f
#define RAIO_AWING        9.0f

/*
 * Dispara os dois lasers do TIE (canhões esquerdo e direito) na direção da frente.
 * Respeita o cooldown de 0,15 s (jogador.cooldownLaser). Devolve 1 se disparou; 0 se
 * estava em cooldown ou o pool não tem 2 slots livres (nesse caso o cooldown não é gasto).
 * Disparar reinicia a espera da recarga do escudo (ATRASO_ESCUDO).
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
 * Assistência de mira: o laser do jogador acerta o caça que passar a até raioDeAcerto do caminho dele.
 * O raio é o do caça de perto e cresce com a distância percorrida pelo laser (TOLERANCIA_MIRA rad),
 * então a folga na tela é constante: o anel da retícula do HUD tem exatamente esse ângulo. A folga
 * também cobre a curva que o caça faz durante o voo do laser, que a previsão em linha reta não vê.
 */
#define TOLERANCIA_MIRA  0.035f   /* rad (~2°) */
/* R/LT: trava o caça mais próximo da retícula (não precisa estar exatamente no centro). */
#define COSSENO_CONE_MIRA 0.9396926f /* cos(20°) */
float raioDeAcerto(TipoNave t, float distancia);
/*
 * 1 se um disparo agora acerta `alvo` voando reto: o ponto de interceptação está no alcance do laser
 * e o canhão mais perto passa a até raioDeAcerto dele. É o teste do indicador verde do HUD.
 */
int tiroAcerta(const Jogador *j, const Inimigo *alvo);
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
 * Resolve lasers do jogador e de aliados contra inimigos. O casco do Destróier bloqueia tiros
 * aliados sem sofrer dano; a assistência de mira vale só para o jogador. Se o casco inimigo zera:
 * registrarAbate só para tiros do jogador, destroço/explosão e removerInimigo (a mira continua válida).
 * Como cada laser acerta no máximo um inimigo, remover durante a varredura de lasers é seguro.
 * Devolve o número de acertos.
 */
int resolverLasers(Jogo *g, float dt);

/* Laser inimigo (vermelho): mesma escala do mundo (×4,6); 5 s cobrem a arena de 9000 u. */
#define VEL_LASER_INIMIGO   1840.0f  /* u/s */
#define VIDA_LASER_INIMIGO  5.0f
#define DANO_LASER_INIMIGO  6.4f     /* 8 × 0,8: redução de 20% dos tiros rebeldes */
/*
 * cascoDestroier vai de 0 a CASCO_MAX_DESTROIER (1200): um tiro de caça vale uma fração do que
 * vale no TIE. A fração fica em 0,3; com o casco triplicado, o Destróier aguenta bem mais tempo.
 */
#define FRACAO_DANO_DESTROIER 0.3f

/*
 * Onde um projétil de velocidade `velProjetil`, saindo de `origem` agora, encontra um alvo em `alvo`
 * que voa reto com `velAlvo`. Sem solução (alvo mais rápido que o projétil), devolve `alvo`.
 * Serve à mira dos rebeldes e ao indicador de tiro adiantado do HUD.
 */
Vector3 pontoDeInterceptacao(Vector3 origem, float velProjetil, Vector3 alvo, Vector3 velAlvo);

/*
 * Dispara um laser inimigo (doJogador = 0) da posição de `in` rumo ao alvo do seu estadoIA:
 * IA_ATACAR_JOGADOR mira o ponto de interceptação do TIE (pontoDeInterceptacao com a frente * velocidade
 * dele), então acerta quem voa reto; IA_ATACAR_DESTROIER mira a origem (centro do Destróier).
 * IA_ATACAR_ALIADO mira a interceptação do TIE aliado ativo indicado por alvoAliado.
 * IA_FUGIR, IA_ESCOLTAR e IA_PATRULHAR não atiram. Devolve 1 se disparou; 0 se fugindo, com o pool
 * cheio ou sem direção (inimigo exatamente sobre o alvo). O cooldownTiro é da IA: esta função não
 * o lê nem o altera.
 */
int dispararLaserInimigo(Jogo *g, const Inimigo *in);
/* Torpedo antinave do Y-Wing: trajetória parabólica descendente, dano discreto no impacto.
 * A fração própria (0,4) é maior que a dos lasers (0,3): o Y-Wing é sempre a melhor nave
 * contra o Destróier, tanto no escudo quanto no casco. */
#define VEL_TORPEDO_YWING  500.0f   /* componente horizontal, igual à velocidade da run */
#define VIDA_TORPEDO_YWING   8.0f
#define DANO_TORPEDO_YWING  80.0f   /* bomba: mantém o Y-Wing como principal ameaça ao Destróier */
#define GRAVIDADE_TORPEDO_YWING 900.0f
#define FRACAO_DANO_TORPEDO_DESTROIER 0.44f  /* 0,4 + 10% no escudo e no casco do Destróier */
int dispararTorpedoYwing(Jogo *g, const Inimigo *in);
/* Laser defensivo do Y-Wing (20 tiros/min) também segue a parábola; mira balística. */
int dispararLaserYwing(Jogo *g, const Inimigo *in, Vector3 alvo, Vector3 velAlvo);
/* Laser aliado mira a interceptação do inimigo e não gera pontos para o jogador. */
int dispararLaserAliado(Jogo *g, const Aliado *aliado, const Inimigo *alvo);
/*
 * Colisão dos lasers/torpedos inimigos × jogador, × TIEs aliados e × Destróier (casco de destroier.h,
 * amostrando o segmento). Roda logo depois de atualizarLasers, com o mesmo segmento
 * (posição anterior → posição atual, já com a gravidade dos torpedos). Cada laser atinge só o
 * primeiro alvo que atravessa e é desativado.
 * No jogador: danificarJogador (só casco). Nos aliados: danificarAliado. No Destróier:
 * danificarDestroier com dano * fração (torpedo usa FRACAO_DANO_TORPEDO_DESTROIER).
 * Devolve quantos acertos no jogador houve neste passo (quem chama vibra o controle).
 */
int colidirLasersInimigos(Jogo *g, float dt);

#define DURACAO_EXPLOSAO 0.8f    /* s de vida de uma explosão */
#define RAIO_EXPLOSAO    20.0f   /* raio máximo (chute; ajustar com o visual) */

/*
 * Ativa uma explosão em `pos` no primeiro slot livre de g->explosoes (tempo = 0). `tempo` é o tempo
 * decorrido desde o início, até DURACAO_EXPLOSAO. Com o pool cheio, ignora. Devolve 1 se criou.
 */
int criarExplosao(Jogo *g, Vector3 pos, float raioMax);
/* Avança o tempo das explosões ativas e desativa as que passaram de DURACAO_EXPLOSAO. */
void atualizarExplosoes(Jogo *g, float dt);

/*
 * Curva da explosão para o render (VIS-10): progresso em [0, 1] e raio atual, que cresce
 * rápido no começo e desacelera no fim. Explosão inativa devolve 0. Não muda estado,
 * então serve para o `const Jogo *` do render_cena. O fade é 1 - progresso.
 */
float progressoExplosao(const Explosao *e);
float raioExplosao(const Explosao *e);

#define MAX_QUEDAS_INIMIGO 32
#define QUEDA_INIMIGO_MIN  1.2f   /* s; abaixo da duração dos áudios de SPIN */
#define QUEDA_INIMIGO_MAX  2.6f

/*
 * Destroço visível de um caça abatido. Ele sai da lista de inimigos (não é mais alvo nem
 * colide), continua na mesma velocidade que tinha e gira em torno de um eixo aleatório até
 * explodir. O render desenha a partir de `rot`; o áudio toca o SPIN no início e o BOOM no fim.
 * `vozSpin` é a voz de áudio reservada para parar o SPIN quando o BOOM começar.
 */
typedef struct {
    int ativo;
    TipoNave tipo;
    Vector3 pos, vel;
    Quaternion rot;
    Vector3 eixo;
    float giro;
    float tempo, duracao;
    int vozSpin;
} QuedaInimigo;

/* Pool de quedas para o render (iterar até MAX_QUEDAS_INIMIGO checando `ativo`). */
const QuedaInimigo *quedasInimigas(void);
/* Zera as quedas; chamar ao iniciar/encerrar a partida. */
void limparQuedasInimigas(void);
/*
 * Avança posição/rotação de cada queda; ao fim da duração cria a explosão na posição atual,
 * para a voz de SPIN e toca o BOOM. Roda uma vez por passo fixo.
 */
void atualizarQuedasInimigas(Jogo *g, float dt);

#endif
