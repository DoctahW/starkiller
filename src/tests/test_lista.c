#include <math.h>
#include <string.h>

#include "inimigos.h"
#include "teste.h"

#define EPS 1e-5f

static Jogo jogo;

//compara float com tolerancia
static int proximo(float a, float b){
    return fabsf(a - b) < EPS;
}

static int vetorNulo(Vector3 v){
    return proximo(v.x, 0.0f) && proximo(v.y, 0.0f) && proximo(v.z, 0.0f);
}

//lista com ids 1..qtd, na ordem
static void montarLista(ListaInimigos *l, int qtd){
    for(int i = 1; i <= qtd; i++){
        Vector3 pos = {(float) i, 0.0f, (float) i * 10.0f};
        inserirInimigo(l, criarInimigo(XWING, pos, i));
    }
}

//busca so pra montar caso de teste; a API nao expoe busca
static NoInimigo *buscarPorId(ListaInimigos *l, int id){
    for(NoInimigo *no = l->inicio; no != NULL; no = no->prox){
        if(no->dado.id == id){
            return no;
        }
    }
    return NULL;
}

//confere tamanho, encadeamento duplo e as invariantes do alvo
static void conferirLista(const ListaInimigos *l, int esperado){
    int frente = 0;
    for(const NoInimigo *no = l->inicio; no != NULL; no = no->prox){
        if(no->prox != NULL && no->prox->ant != no){
            break; //ant corrompido
        }
        frente++;
    }

    int tras = 0;
    for(const NoInimigo *no = l->fim; no != NULL; no = no->ant){
        if(no->ant != NULL && no->ant->prox != no){
            break; //prox corrompido
        }
        tras++;
    }

    VERIFICA(frente == esperado);
    VERIFICA(tras == esperado);
    VERIFICA(l->tamanho == esperado);

    if(esperado == 0){
        VERIFICA(l->inicio == NULL);
        VERIFICA(l->fim == NULL);
        VERIFICA(l->alvo == NULL); //esvaziou, alvo tem que ser NULL
        return;
    }

    VERIFICA(l->inicio != NULL && l->inicio->ant == NULL);
    VERIFICA(l->fim != NULL && l->fim->prox == NULL);
    VERIFICA(l->alvo != NULL); //lista com no tem alvo

    //alvo tem que estar dentro da lista
    int alvoVisivel = 0;
    for(const NoInimigo *no = l->inicio; no != NULL; no = no->prox){
        if(no == l->alvo){
            alvoVisivel = 1;
        }
    }
    VERIFICA(alvoVisivel == 1);
}

// ==================== LST-01: criarInimigo ====================

static void testarXWing(){
    Inimigo in = criarInimigo(XWING, (Vector3){1.0f, 2.0f, 3.0f}, 7);

    VERIFICA(in.id == 7);
    VERIFICA(in.tipo == XWING);
    VERIFICA(proximo(in.pos.x, 1.0f));
    VERIFICA(proximo(in.pos.y, 2.0f));
    VERIFICA(proximo(in.pos.z, 3.0f));

    //atributos da tabela 2.2
    VERIFICA(proximo(in.escudoMax, 50.0f));
    VERIFICA(proximo(in.cascoMax, 100.0f));

    //nasce cheio
    VERIFICA(proximo(in.escudo, in.escudoMax));
    VERIFICA(proximo(in.casco, in.cascoMax));

    //a IA-01 assume o movimento, LST-07 recalcula a distancia
    VERIFICA(vetorNulo(in.vel));
    VERIFICA(proximo(in.distancia, 0.0f));
    VERIFICA(proximo(in.cooldownTiro, 0.0f));
    VERIFICA(in.estadoIA == IA_ATACAR_DESTROIER);
    VERIFICA(proximo(VEL_XWING, 60.0f));
}

static void testarYWing(){
    Inimigo in = criarInimigo(YWING, (Vector3){0.0f, 0.0f, 1800.0f}, 8);

    VERIFICA(in.id == 8);
    VERIFICA(in.tipo == YWING);
    VERIFICA(proximo(in.escudoMax, 80.0f));
    VERIFICA(proximo(in.cascoMax, 160.0f));
    VERIFICA(proximo(in.escudo, in.escudoMax));
    VERIFICA(proximo(in.casco, in.cascoMax));
    VERIFICA(in.estadoIA == IA_ATACAR_DESTROIER); //vai direto no Destróier
    VERIFICA(proximo(VEL_YWING, 35.0f));
}

static void testarAWing(){
    Inimigo in = criarInimigo(AWING, (Vector3){-1800.0f, 0.0f, 0.0f}, 9);

    VERIFICA(in.id == 9);
    VERIFICA(in.tipo == AWING);
    VERIFICA(proximo(in.escudoMax, 30.0f));
    VERIFICA(proximo(in.cascoMax, 60.0f));
    VERIFICA(proximo(in.escudo, in.escudoMax));
    VERIFICA(proximo(in.casco, in.cascoMax));
    VERIFICA(in.estadoIA == IA_ATACAR_JOGADOR); //persegue o jogador
    VERIFICA(proximo(VEL_AWING, 110.0f));
}

//o id vem do chamador, via Jogo.proximoId++
static void testarIdsDoJogo(){
    memset(&jogo, 0, sizeof(jogo));
    jogo.proximoId = 1;

    Inimigo a = criarInimigo(XWING, (Vector3){0.0f, 0.0f, 1800.0f}, jogo.proximoId++);
    Inimigo b = criarInimigo(AWING, (Vector3){1800.0f, 0.0f, 0.0f}, jogo.proximoId++);

    VERIFICA(a.id == 1);
    VERIFICA(b.id == 2);
    VERIFICA(a.id != b.id);
    VERIFICA(jogo.proximoId == 3);
}

// ==================== LST-02: inserirInimigo ====================

static void testarInsercaoEmListaVazia(){
    ListaInimigos lista;
    memset(&lista, 0, sizeof(lista));

    inserirInimigo(&lista, criarInimigo(XWING, (Vector3){1.0f, 2.0f, 3.0f}, 1));

    VERIFICA(lista.tamanho == 1);
    VERIFICA(lista.inicio == lista.fim);
    VERIFICA(lista.alvo == lista.inicio); //estava vazia, entao o no virou o alvo
    VERIFICA(lista.inicio->dado.id == 1);
    VERIFICA(proximo(lista.inicio->dado.escudo, 50.0f)); //copiou o struct inteiro

    conferirLista(&lista, 1);
}

//insere no fim sem andar na lista, e nunca mexe no alvo
static void testarInsercaoSempreNoFim(){
    ListaInimigos lista;
    memset(&lista, 0, sizeof(lista));

    inserirInimigo(&lista, criarInimigo(XWING, (Vector3){0.0f, 0.0f, 1.0f}, 1));
    NoInimigo *primeiro = lista.inicio;

    inserirInimigo(&lista, criarInimigo(YWING, (Vector3){0.0f, 0.0f, 2.0f}, 2));
    inserirInimigo(&lista, criarInimigo(AWING, (Vector3){0.0f, 0.0f, 3.0f}, 3));

    conferirLista(&lista, 3);
    VERIFICA(lista.inicio == primeiro); //entra no fim, inicio intacto
    VERIFICA(lista.fim->dado.id == 3);
    VERIFICA(lista.alvo == primeiro); //alvo nao muda em lista nao vazia

    //ordem de chegada
    VERIFICA(lista.inicio->dado.id == 1);
    VERIFICA(lista.inicio->prox->dado.id == 2);
    VERIFICA(lista.fim->ant->dado.id == 2);
}

static void testarLiberarListaVazia(){
    ListaInimigos lista;
    memset(&lista, 0, sizeof(lista));

    liberarListaInimigos(&lista); //nao pode quebrar

    VERIFICA(lista.inicio == NULL);
    VERIFICA(lista.fim == NULL);
    VERIFICA(lista.alvo == NULL);
    VERIFICA(lista.tamanho == 0);
}

static void testarLiberarComNos(){
    ListaInimigos lista;
    memset(&lista, 0, sizeof(lista));
    montarLista(&lista, 4);

    liberarListaInimigos(&lista);

    //libera tudo e zera a lista
    VERIFICA(lista.tamanho == 0);
    VERIFICA(lista.inicio == NULL);
    VERIFICA(lista.fim == NULL);
    VERIFICA(lista.alvo == NULL);

    //reusavel depois de liberar
    montarLista(&lista, 2);
    conferirLista(&lista, 2);
    liberarListaInimigos(&lista);
}

// ==================== LST-03: removerInimigo ====================

static void testarRemoverNoUnico(){
    ListaInimigos lista;
    memset(&lista, 0, sizeof(lista));
    montarLista(&lista, 1);

    removerInimigo(&lista, lista.inicio);

    VERIFICA(lista.tamanho == 0);
    VERIFICA(lista.inicio == NULL);
    VERIFICA(lista.fim == NULL);
    VERIFICA(lista.alvo == NULL); //sem vizinho, alvo vira NULL
}

static void testarRemoverInicio(){
    ListaInimigos lista;
    memset(&lista, 0, sizeof(lista));
    montarLista(&lista, 3);

    removerInimigo(&lista, lista.inicio);

    conferirLista(&lista, 2);
    VERIFICA(lista.inicio->dado.id == 2);
    VERIFICA(lista.fim->dado.id == 3);
}

static void testarRemoverMeio(){
    ListaInimigos lista;
    memset(&lista, 0, sizeof(lista));
    montarLista(&lista, 3);

    removerInimigo(&lista, buscarPorId(&lista, 2));

    conferirLista(&lista, 2);
    VERIFICA(lista.inicio->dado.id == 1);
    VERIFICA(lista.fim->dado.id == 3);
    VERIFICA(lista.inicio->prox == lista.fim); //1 cola direto no 3
    VERIFICA(lista.fim->ant == lista.inicio);
}

static void testarRemoverFim(){
    ListaInimigos lista;
    memset(&lista, 0, sizeof(lista));
    montarLista(&lista, 3);

    removerInimigo(&lista, lista.fim);

    conferirLista(&lista, 2);
    VERIFICA(lista.inicio->dado.id == 1);
    VERIFICA(lista.fim->dado.id == 2);
}

//alvo era o inicio: passa pro prox
static void testarAlvoMudaProProx(){
    ListaInimigos lista;
    memset(&lista, 0, sizeof(lista));
    montarLista(&lista, 3);

    VERIFICA(lista.alvo->dado.id == 1);

    removerInimigo(&lista, lista.inicio);

    conferirLista(&lista, 2);
    VERIFICA(lista.alvo->dado.id == 2);
}

//alvo era o fim: nao tem prox, cai no ant
static void testarAlvoNoFimVaiProAnt(){
    ListaInimigos lista;
    memset(&lista, 0, sizeof(lista));
    montarLista(&lista, 3);
    lista.alvo = lista.fim;

    NoInimigo *esperado = lista.fim->ant;

    removerInimigo(&lista, lista.fim);

    conferirLista(&lista, 2);
    VERIFICA(lista.alvo == esperado);
    VERIFICA(lista.alvo->dado.id == 2);
    VERIFICA(lista.alvo == lista.fim);
}

//remove o alvo do meio, depois sobra ele mesmo sem vizinhos
static void testarAlvoAteSobrarSoEle(){
    ListaInimigos lista;
    memset(&lista, 0, sizeof(lista));
    montarLista(&lista, 3);

    lista.alvo = buscarPorId(&lista, 2);
    NoInimigo *meio = lista.alvo;

    removerInimigo(&lista, buscarPorId(&lista, 1));
    removerInimigo(&lista, buscarPorId(&lista, 3));

    VERIFICA(lista.alvo == meio); //mesmo no, nada mudou
    conferirLista(&lista, 1);

    removerInimigo(&lista, meio);

    VERIFICA(lista.alvo == NULL);
    conferirLista(&lista, 0);
}

static void testarRemoverNullNaoFazNada(){
    ListaInimigos lista;
    memset(&lista, 0, sizeof(lista));
    montarLista(&lista, 3);
    NoInimigo *alvoAntes = lista.alvo;

    removerInimigo(&lista, NULL);

    VERIFICA(lista.tamanho == 3);
    VERIFICA(lista.alvo == alvoAntes);
    conferirLista(&lista, 3);
}

//padrao da secao 3.5: guarda o prox antes de processar o no
static void testarRemocaoComIteracaoSegura(){
    ListaInimigos lista;
    memset(&lista, 0, sizeof(lista));
    montarLista(&lista, 5);

    int removidos = 0;
    NoInimigo *no = lista.inicio;
    while(no != NULL){
        NoInimigo *prox = no->prox;
        removerInimigo(&lista, no);
        removidos++;
        no = prox;
    }

    VERIFICA(removidos == 5);
    conferirLista(&lista, 0);
}

int main(void){
    testarXWing();
    testarYWing();
    testarAWing();
    testarIdsDoJogo();

    testarInsercaoEmListaVazia();
    testarInsercaoSempreNoFim();
    testarLiberarListaVazia();
    testarLiberarComNos();

    testarRemoverNoUnico();
    testarRemoverInicio();
    testarRemoverMeio();
    testarRemoverFim();
    testarAlvoMudaProProx();
    testarAlvoNoFimVaiProAnt();
    testarAlvoAteSobrarSoEle();
    testarRemoverNullNaoFazNada();
    testarRemocaoComIteracaoSegura();

    return RESULTADO_TESTES();
}