#ifndef DESTROIER_H
#define DESTROIER_H

#include "tipos.h"

/*
 * Escala do Destróier no jogo. O modelo e o mapa de colisão continuam no tamanho canônico
 * (1600 u); o jogo desenha e testa a colisão com este fator. Aumentar aqui aumenta os dois
 * juntos, sem regenerar o modelo nem o casco.
 */
#define ESCALA_DESTROIER      4.0f

/*
 * Destróier Estelar fixo na origem: casco em cunha comprido ao longo de Z, proa em +Z, cima +Y.
 * Medidas COMPARTILHADAS: o modelo (assets/models/fontes/destroier.py o escala para Z_POPA..Z_PROA)
 * e as primitivas do render.c (cunha que aproxima o modelo, para quando ele falta).
 * Não duplicar estes valores em outro lugar do jogo.
 */
#define Z_PROA_DESTROIER     823.0f   /* ponta da proa; 1600 u de comprimento, o canônico em metros */
#define Z_POPA_DESTROIER    -777.0f   /* ponto mais à popa: boca dos motores */
#define MEIA_BOCA_DESTROIER  439.0f   /* meia largura na popa */
#define Y_CRISTA_DESTROIER   165.0f   /* linha central, em cima, na popa */
#define Y_QUILHA_DESTROIER  -119.0f   /* linha central, embaixo, na popa */
#define Y_BORDA_DESTROIER     18.0f   /* meia altura da parede lateral na popa */

/*
 * Colisão com o casco: mapa de colunas gerado do modelo (src/casco_destroier.inc, por
 * assets/models/fontes/casco_destroier.py). O plano x-z é dividido em células de 8 u e cada
 * uma guarda até 4 intervalos sólidos de y: o interior fechado do casco fica cheio e os vãos
 * abertos, como o espaço sob as asas da ponte de comando, ficam vazios. As paredes laterais
 * ficam até 8 u para fora do modelo.
 */

/* 1 se a esfera (centro, raio) encosta no casco; raio 0 testa um ponto.
 * Coordenadas e raio do mundo: a consulta é convertida para o tamanho do modelo por ESCALA_DESTROIER. */
int tocaDestroier(Vector3 centro, float raio);

/*
 * Se a esfera (centro, raio) entra no casco, leva `centro` para fora pelo caminho mais curto entre
 * os seis sentidos dos eixos (em cima do convés, para cima; na parede lateral, para o lado) e
 * devolve 1; senão não mexe e devolve 0. Se nenhum eixo conseguir sair (não deveria acontecer),
 * empurra para cima e devolve 0, para não contar como contato.
 */
int afastarDoDestroier(Vector3 *centro, float raio);

#endif
