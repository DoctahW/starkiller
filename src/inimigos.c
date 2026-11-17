#include <stdlib.h>

#include "inimigos.h"
#include "nave.h"
#include "raymath.h"

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
        in.estadoIA = IA_ATACAR_DESTROIER;   // bombardeia o Destróier
        break;
    case AWING:
        in.escudoMax = ESCUDO_AWING;
        in.cascoMax = CASCO_AWING;
        in.estadoIA = IA_ATACAR_JOGADOR;     // persegue o jogador
        break;
    case XWING:
    default:
        in.tipo = XWING;
        in.escudoMax = ESCUDO_XWING;
        in.cascoMax = CASCO_XWING;
        in.estadoIA = IA_ATACAR_DESTROIER;   // alterna em IA-03
        break;
    }

    in.escudo = in.escudoMax;
    in.casco = in.cascoMax;
    return in;
}

int inserirInimigo(ListaInimigos *l, Inimigo in)
{
    NoInimigo *no = malloc(sizeof *no);
    if (no == NULL)
        return 0;

    no->dado = in;
    no->prox = NULL;
    no->ant = l->fim;

    if (l->fim == NULL) {
        // tava vazia: vira início, fim e alvo
        l->inicio = l->fim = l->alvo = no;
    } else {
        l->fim->prox = no;
        l->fim = no;
    }
    l->tamanho++;
    return 1;
}

void removerInimigo(ListaInimigos *l, NoInimigo *no){
    if(no == NULL){
        return;
    }

    //alvo definido antes do free, depois nao da mais pra ler prox e ant
    if(l->alvo == no){
        if(no->prox != NULL){
            l->alvo = no->prox;
        } else if(no->ant != NULL){
            l->alvo = no->ant;
        } else {
            l->alvo = NULL;
        }
    }

    //descola do vizinho da esquerda
    if(no->ant != NULL){
        no->ant->prox = no->prox;
    } else {
        l->inicio = no->prox;
    }

    //descola do vizinho da direita
    if(no->prox != NULL){
        no->prox->ant = no->ant;
    } else {
        l->fim = no->ant;
    }

    free(no);
    l->tamanho--;
}


void atualizarInimigos(Jogo *g, float dt){
    NoInimigo *no = g->inimigos.inicio;

    (void) dt; //so a ia_atualizar usa dt, e ela entra com a IA-01 (#49)

    while(no != NULL){
        NoInimigo *prox = no->prox; //secao 3.5: guardar antes, o despawn libera o no

        no->dado.distancia = Vector3Distance(no->dado.pos, g->jogador.pos);

        //arena e em torno do Destroier na origem, nao do jogador
        if(Vector3Length(no->dado.pos) > RAIO_ARENA){
            removerInimigo(&g->inimigos, no);
        }

        no = prox;
    }
}

void ordenarPorDistancia(ListaInimigos *l){
    NoInimigo *restante = l->inicio;
    NoInimigo *ordenada = NULL;

    //tira o da frente da pilha e encaixa na parcela ja ordenada
    while(restante != NULL){
        NoInimigo *no = restante;
        NoInimigo *ant = NULL;
        NoInimigo *p = ordenada;

        restante = no->prox; //guarda antes de descolar o no

        //<= pra manter a ordem de chegada entre empatados
        while(p != NULL && p->dado.distancia <= no->dado.distancia){
            ant = p;
            p = p->prox;
        }

        no->ant = ant;
        no->prox = p;

        if(ant != NULL){
            ant->prox = no;
        } else {
            ordenada = no; //virou o mais proximo
        }

        if(p != NULL){
            p->ant = no;
        }
    }

    l->inicio = ordenada;

    NoInimigo *ultimo = ordenada;
    while(ultimo != NULL && ultimo->prox != NULL){
        ultimo = ultimo->prox;
    }
    l->fim = ultimo;

    //alvo nao muda: so religuei ponteiro, o no continua o mesmo
}

void proximoAlvo(ListaInimigos *l){
    if(l->alvo == NULL){
        return; //lista vazia
    }

    if(l->alvo->prox != NULL){
        l->alvo = l->alvo->prox;
    } else {
        l->alvo = l->inicio; //tava no fim, volta pro comeco
    }
}

void alvoAnterior(ListaInimigos *l){
    if(l->alvo == NULL){
        return; //lista vazia
    }

    if(l->alvo->ant != NULL){
        l->alvo = l->alvo->ant;
    } else {
        l->alvo = l->fim; //tava no comeco, volta pro fim
    }
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
