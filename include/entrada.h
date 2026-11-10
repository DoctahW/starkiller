#ifndef ENTRADA_H
#define ENTRADA_H

#include "tipos.h"

void iniciarEntrada(ConfigEntrada *cfg);
Comandos lerComandos(const ConfigEntrada *cfg);
/* Vibra o controle inteiro (intensidade 0–1, duração em segundos, até 2 s) se cfg->vibracao estiver ligado. */
void vibrar(const ConfigEntrada *cfg, float intensidade, float duracao);

#endif
