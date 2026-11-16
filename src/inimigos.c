#include <stdlib.h>

#include "inimigos.h"

Inimigo criarInimigo(TipoNave tipo, Vector3 pos, int id)
{
    Inimigo in = { 0 };

    in.id = id;
    in.tipo = tipo;
    in.pos = pos;
    in.vel = (Vector3){ 0.0f, 0.0f, 0.0f };
    in.distancia = 0.0f;
    in.cooldownTiro = 0.0f;

    switch (tipo) {
    case YWING:
        in.escudoMax = ESCUDO_YWING;
        in.cascoMax = CASCO_YWING;
        in.estadoIA = IA_ATACAR_DESTROIER;   /* bombardeia o Destróier */
        break;
    case AWING:
        in.escudoMax = ESCUDO_AWING;
        in.cascoMax = CASCO_AWING;
        in.estadoIA = IA_ATACAR_JOGADOR;     /* persegue o jogador */
        break;
    case XWING:
    default:
        in.escudoMax = ESCUDO_XWING;
        in.cascoMax = CASCO_XWING;
        in.estadoIA = IA_ATACAR_DESTROIER;   /* alterna em IA-03 */
        break;
    }

    in.escudo = in.escudoMax;
    in.casco = in.cascoMax;
    return in;
}

void liberarListaInimigos(ListaInimigos *l)
{
    NoInimigo *no = l->inicio;
    while (no != NULL) {
        NoInimigo *prox = no->prox;
        free(no);
        no = prox;
    }
    l->inicio = l->fim = l->alvo = NULL;
    l->tamanho = 0;
}
