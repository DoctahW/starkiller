#ifndef ENTRADA_H
#define ENTRADA_H

#include "tipos.h"

#define CAMINHO_CONFIG "dados/config.txt"

/* Faixas válidas de ConfigEntrada; valores fora delas no arquivo voltam ao padrão. */
#define SENSIBILIDADE_MIN 0.5f
#define SENSIBILIDADE_MAX 2.0f
#define ZONA_MORTA_MIN    0.0f
#define ZONA_MORTA_MAX    0.5f

/* Padrões sobrescritos pelo que houver em CAMINHO_CONFIG. */
void iniciarEntrada(ConfigEntrada *cfg);
/* Campos ausentes ou inválidos ficam com o valor atual de cfg; devolve 1 se o arquivo foi lido. */
int carregarConfigEntrada(ConfigEntrada *cfg, const char *caminho);
/* Cria a pasta do arquivo se não existir; devolve 1 se salvou. */
int salvarConfigEntrada(const ConfigEntrada *cfg, const char *caminho);

/* Eixos de rotação multiplicados por cfg->sensibilidade e limitados a [-1, 1]; arfagem invertida se pedido. */
Comandos lerComandos(const ConfigEntrada *cfg);
/* Vibra o controle inteiro (intensidade 0–1, duração em segundos, até 2 s) se cfg->vibracao estiver ligado. */
void vibrar(const ConfigEntrada *cfg, float intensidade, float duracao);

#endif
