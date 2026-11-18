#ifndef AUDIO_H
#define AUDIO_H

#include "tipos.h"

/*
 * Efeitos sonoros do jogo (AUD-01). Os arquivos ficam em assets/sounds/ (OGG Vorbis).
 *
 * O módulo é tolerante a falhas: sem dispositivo de áudio ou sem um arquivo, a função
 * correspondente vira no-op e o jogo continua. As funções de evento também são seguras antes de
 * iniciarAudio, então os testes de console rodam sem dispositivo.
 */

/* Abre o dispositivo e carrega os bancos de som; chamar depois de InitWindow. */
void iniciarAudio(void);
/* Para loops, libera os sons e fecha o dispositivo; chamar antes de CloseWindow. */
void encerrarAudio(void);
/* 1 se o dispositivo abriu e os bancos foram carregados. */
int audioDisponivel(void);

/*
 * Atualiza as vozes contínuas e os temporizadores aleatórios. Chamar uma vez por frame.
 * `emCombate` liga o áudio da cabine e dos motores; fora do combate os loops são silenciados
 * (os one-shots já disparados continuam até o fim). `cmd` pode ser NULL: sem ele não há
 * detecção de aceleração/frenagem.
 */
void audioAtualizar(const Jogo *g, const Comandos *cmd, int emCombate, float dt);

/*
 * Grupos de fontes móveis de motor. O grupo separa os ids de listas diferentes (inimigos
 * hoje, aliados no futuro) para que não colidam na mesma voz.
 */
typedef enum {
    AUDIO_MOTOR_INIMIGO = 0,
    AUDIO_MOTOR_ALIADO  = 1
} GrupoMotor;

/*
 * Fator de pitch do efeito Doppler para uma fonte móvel em `pos` com velocidade `vel`,
 * relativa ao ouvinte atual (o TIE do jogador). 1,0 = sem deslocamento; > 1 ao se aproximar
 * e < 1 ao se afastar. Já vem limitado a uma faixa segura. Serve para qualquer nave.
 */
float audioFatorDoppler(Vector3 pos, Vector3 vel);

/*
 * Registra uma fonte de motor móvel neste frame. `id` é único dentro de `grupo`. Inimigos e
 * futuros aliados usam a mesma voz genérica: as fontes mais próximas de cada tipo ocupam as
 * vozes disponíveis, com volume por distância, pan e Doppler. Fonte que não for registrada
 * em um frame desvanece. Chamar antes de audioAtualizar, que consome os registros.
 */
void audioMotorRegistrar(GrupoMotor grupo, int id, TipoNave tipo, Vector3 pos, Vector3 vel);

/* Eventos pontuais do combate. */
void audioTiroJogador(void);                         /* TIE WEAPON, alterna os dois sons */
void audioTiroInimigo(TipoNave tipo, Vector3 pos);   /* WEAPON do caça rebelde */
void audioDanoJogador(void);                         /* SHOT_HIT: tiro inimigo acertou o TIE */
void audioColisaoJogador(void);                      /* COLLISION_HIT: TIE bateu no casco */
void audioTrocaAlvo(void);                           /* TARGET ACQUISITION */
/*
 * Destruição de um caça inimigo: o jogo controla o tempo (o destroço gira) e avisa o áudio.
 * `audioSpinInimigo` devolve a voz usada (>= 0) ou -1; `audioBoomInimigo` para essa voz de
 * SPIN e toca o BOOM no fim do giro.
 */
int audioSpinInimigo(TipoNave tipo, Vector3 pos);
void audioBoomInimigo(TipoNave tipo, Vector3 pos, int vozSpin);
void audioSpinJogador(void);                         /* início da queda do TIE */
void audioBoomJogador(Vector3 pos);                  /* para o SPIN e toca a explosão final do TIE */
/* Para todos os SPINs (inimigos e jogador); usar ao limpar uma partida. */
void audioPararSpins(void);
void audioDestroierDestruido(void);                  /* DESTROYED do Destróier */
void audioEscudoDestroier(int ligado);               /* SHIELD enable/disable (uso futuro) */

#endif
