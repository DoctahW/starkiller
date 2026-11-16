#ifndef INIMIGOS_H
#define INIMIGOS_H

#include "tipos.h"

// Atributos iniciais por tipo
#define ESCUDO_XWING 50.0f
#define CASCO_XWING  100.0f
#define VEL_XWING    60.0f

#define ESCUDO_YWING 80.0f
#define CASCO_YWING  160.0f
#define VEL_YWING    35.0f

#define ESCUDO_AWING 30.0f
#define CASCO_AWING  60.0f
#define VEL_AWING    110.0f

//Monta um inimigo com os atributos do tipo, escudo e casco cheios
//O id vem de quem chama: Jogo.proximoId++
Inimigo criarInimigo(TipoNave tipo, Vector3 pos, int id);

//Insere no FIM em O(1), atualizando inicio, fim e tamanho
//Se a lista estava vazia, o novo nó vira o alvo
void inserirInimigo(ListaInimigos *l, Inimigo in);

//Libera todos os nós e zera a lista
void liberarListaInimigos(ListaInimigos *l);

#endif