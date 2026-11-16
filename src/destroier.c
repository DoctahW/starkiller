#include "destroier.h"

/* Popa larga em -Z afinando até a proa em +Z; a ponta fica a ~180 u da origem (o jogador nasce em z = 300). */
const EsferaDestroier ESFERAS_DESTROIER[NUM_ESFERAS_DESTROIER] = {
    { { 0.0f, 0.0f, -120.0f }, 50.0f },
    { { 0.0f, 0.0f,  -60.0f }, 45.0f },
    { { 0.0f, 0.0f,    0.0f }, 38.0f },
    { { 0.0f, 0.0f,   60.0f }, 28.0f },
    { { 0.0f, 0.0f,  120.0f }, 18.0f },
    { { 0.0f, 0.0f,  170.0f }, 10.0f },
};
