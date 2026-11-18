#ifndef AUDIO_H
#define AUDIO_H

#include "tipos.h"

/*
 * Áudio do jogo (AUD-01/AUD-02): efeitos, música de fundo, sons de interface e volumes.
 * Os efeitos ficam em assets/sounds/, as músicas em assets/sounds/musics/ e a interface
 * em assets/sounds/menu/.
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
 * e < 1 ao se afastar. O deslocamento perde força com a distância, para fontes no limite do
 * alcance não competirem com as próximas. Já vem limitado a uma faixa segura. Serve para
 * qualquer nave.
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

/*
 * Configuração de áudio (AUD-02), persistida em CAMINHO_CONFIG_AUDIO.
 * Os volumes vão de 0 a 1; a música é multiplicada por volumeGeral * volumeMusica e os
 * efeitos (inclusive os de interface) por volumeGeral * volumeEfeitos.
 */
#define CAMINHO_CONFIG_AUDIO       "dados/audio.txt"
#define AUDIO_VOLUME_MIN           0.0f
#define AUDIO_VOLUME_MAX           1.0f
#define AUDIO_VOLUME_PASSO         0.05f
#define AUDIO_VOLUME_GERAL_PADRAO  0.80f
#define AUDIO_VOLUME_EFEITOS_PADRAO 0.80f
#define AUDIO_VOLUME_MUSICA_PADRAO 0.40f
/* Multiplicador da música na pausa e nas opções abertas por ela. */
#define AUDIO_FATOR_PAUSA          0.40f

typedef struct {
    float geral;
    float efeitos;
    float musica;
} ConfigAudio;

/* Padrões 0,80 / 0,80 / 0,40. */
void audioConfigPadroes(ConfigAudio *cfg);
/* Mesmo formato chave=valor de entrada.c; valor ausente/inválido mantém o atual. */
int audioConfigCarregar(ConfigAudio *cfg, const char *caminho);
/* Cria a pasta se preciso; devolve 1 se salvou. */
int audioConfigSalvar(const ConfigAudio *cfg, const char *caminho);

/* Configuração viva do jogo; nunca NULL. A tela de opções altera os campos diretamente. */
ConfigAudio *audioConfig(void);
/* Limita os três volumes a [0, 1]; chamar depois de alterar a configuração. */
void audioAplicarVolumes(void);
/* Salva em CAMINHO_CONFIG_AUDIO. */
int audioSalvarConfiguracao(void);

/* Faixa de fundo; a troca é um crossfade curto. */
typedef enum {
    AUDIO_MUSICA_NENHUMA = 0,
    AUDIO_MUSICA_MENU,
    AUDIO_MUSICA_COMBATE,
    AUDIO_MUSICA_RANKING,
    AUDIO_MUSICA_VITORIA
} FaixaMusica;

/*
 * Define a faixa desejada. Pode ser chamada antes de iniciarAudio; a faixa escolhida
 * começa quando o áudio estiver pronto. `fator` 1,0 = normal; AUDIO_FATOR_PAUSA abaixa.
 */
void audioMusicaTocar(FaixaMusica faixa, float fator);

/* Efeitos de interface; seguros antes de iniciarAudio. */
void audioUIMover(void);        /* seleção mudou */
void audioUISelecionar(void);   /* confirmou/alternou */
void audioUIVoltar(void);       /* voltou/backspace */
void audioUITecla(void);        /* uma letra foi inserida */

#endif
