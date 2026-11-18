#ifndef ALIADOS_H
#define ALIADOS_H

#include "tipos.h"

/* Roster fixo: sempre 11 TIEs aliados, com no máximo 3 em guarda. */
#define ALIADOS_MAX            11
#define ALIADOS_INICIAIS       ALIADOS_MAX
#define ALIADOS_GUARDA_MAX      3   /* 3 guardas; o resto intercepta */
#define ALIADOS_TEMPO_RESPAWN   5.0f
#define CASCO_ALIADO           125.0f
#define RAIO_ALIADO              9.0f  /* esfera do TIE a 1,5× a escala anterior */
#define VEL_ALIADO             550.0f
#define ACELERACAO_ALIADO      650.0f
#define RAIO_DEFESA           5600.0f
#define RAIO_PATRULHA         4000.0f
#define ALCANCE_TIRO_ALIADO   2000.0f
#define COOLDOWN_TIRO_ALIADO   0.20f   /* 300 tiros/min */
#define DANO_LASER_ALIADO      10.0f

/* Ponto de nascimento abaixo do Destróier; o jogador usa a mesma base. */
Vector3 pontoNascimentoAliado(int indice);

/* Onze TIEs aliados desde o início: três defendem o Destróier e oito interceptam. */
void iniciarAliados(Jogo *g);
/* Reforços, respawn, patrulha, escolta dos Y-Wings, voo, separação e disparo. */
void atualizarAliados(Jogo *g, float dt);
/* Desativa o caça abatido, agenda o respawn e cria a explosão; não afeta o score do jogador. */
void danificarAliado(Jogo *g, int indice, float dano);
/* Quantidade de aliados ativos e tamanho atual do roster (fixo em 11). */
int contarAliadosAtivos(const Jogo *g);
int tamanhoRosterAliados(const Jogo *g);

#endif
