#ifndef INIMIGOS_H
#define INIMIGOS_H

#include "tipos.h"

/* Atributos iniciais por tipo (tabela 2.2; BAL-01 ajusta os valores). */
#define ESCUDO_XWING 50.0f
#define CASCO_XWING  100.0f
#define VEL_XWING    60.0f

#define ESCUDO_YWING 80.0f
#define CASCO_YWING  160.0f
#define VEL_YWING    35.0f

#define ESCUDO_AWING 30.0f
#define CASCO_AWING  60.0f
#define VEL_AWING    110.0f

/*
 * Monta um inimigo do tipo `tipo` na posição `pos`, com escudo e casco cheios
 * (escudo = escudoMax, casco = cascoMax) e velocidade inicial zero; a IA-01
 * assume o movimento. `distancia` é zerada e recalculada em LST-07.
 * `id` deve ser único: o chamador passa `Jogo.proximoId++`.
 */
Inimigo criarInimigo(TipoNave tipo, Vector3 pos, int id);

void liberarListaInimigos(ListaInimigos *l);

#endif