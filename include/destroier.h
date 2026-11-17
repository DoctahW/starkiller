#ifndef DESTROIER_H
#define DESTROIER_H

#include "tipos.h"

/*
 * Destróier Estelar aproximado por esferas, centrado na origem (casco comprido ao longo de Z,
 * proa em +Z). Constante COMPARTILHADA: o render.c desenhará o modelo (VIS-03) nestas dimensões e a colisão
 * da nave (colidirDestroier) usa as mesmas esferas. Não duplicar estes valores em outro lugar.
 * Valores provisórios (chutes): a VIS-03 (#57) ajusta quando o modelo final existir.
 */
#define NUM_ESFERAS_DESTROIER 6

typedef struct {
    Vector3 centro;
    float raio;
} EsferaDestroier;

extern const EsferaDestroier ESFERAS_DESTROIER[NUM_ESFERAS_DESTROIER];

#endif
