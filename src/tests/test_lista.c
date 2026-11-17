#include <math.h>
#include <string.h>

#include "inimigos.h"
#include "nave.h"
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

//seta a distancia de cada no, ja que o criarInimigo deixa em zero
static void setarDistancias(ListaInimigos *l, const float *vals, int qtd){
    int i = 0;
    for(NoInimigo *no = l->inicio; no != NULL && i < qtd; no = no->prox){
        no->dado.distancia = vals[i];
        i++;
    }
}

static int estaCrescente(const ListaInimigos *l){
    for(const NoInimigo *no = l->inicio; no != NULL && no->prox != NULL; no = no->prox){
        if(no->dado.distancia > no->prox->dado.distancia){
            return 0;
        }
    }
    return 1;
}

static void porNaFrente(ListaInimigos *l, int id, float z){
    Vector3 pos = {0.0f, 0.0f, z};
    inserirInimigo(l, criarInimigo(XWING, pos, id));
}

// ==================== LST-01: criarInimigo ====================

static void testarXWing(void){
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
}

static void testarYWing(void){
    Inimigo in = criarInimigo(YWING, (Vector3){0.0f, 0.0f, 1800.0f}, 8);

    VERIFICA(in.id == 8);
    VERIFICA(in.tipo == YWING);
    VERIFICA(proximo(in.escudoMax, 80.0f));
    VERIFICA(proximo(in.cascoMax, 160.0f));
    VERIFICA(proximo(in.escudo, in.escudoMax));
    VERIFICA(proximo(in.casco, in.cascoMax));
    VERIFICA(in.estadoIA == IA_ATACAR_DESTROIER); //vai direto no Destróier
}

static void testarAWing(void){
    Inimigo in = criarInimigo(AWING, (Vector3){-1800.0f, 0.0f, 0.0f}, 9);

    VERIFICA(in.id == 9);
    VERIFICA(in.tipo == AWING);
    VERIFICA(proximo(in.escudoMax, 30.0f));
    VERIFICA(proximo(in.cascoMax, 60.0f));
    VERIFICA(proximo(in.escudo, in.escudoMax));
    VERIFICA(proximo(in.casco, in.cascoMax));
    VERIFICA(in.estadoIA == IA_ATACAR_JOGADOR); //persegue o jogador
}

//velocidades da tabela 2.2 (x4,6 com a escala do mundo); quem consome esses valores e a IA-01 (#49)
static void testarVelocidadesDaTabela(void){
    //A-Wing e o mais rapido, Y-Wing o mais lento
    VERIFICA(VEL_AWING > VEL_XWING);
    VERIFICA(VEL_XWING > VEL_YWING);
}

//o id vem do chamador, via Jogo.proximoId++
static void testarIdsDoJogo(void){
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

static void testarInsercaoEmListaVazia(void){
    ListaInimigos lista;
    memset(&lista, 0, sizeof(lista));

    VERIFICA(inserirInimigo(&lista, criarInimigo(XWING, (Vector3){1.0f, 2.0f, 3.0f}, 1)) == 1);

    VERIFICA(lista.tamanho == 1);
    VERIFICA(lista.inicio == lista.fim);
    VERIFICA(lista.alvo == lista.inicio); //estava vazia, entao o no virou o alvo
    VERIFICA(lista.inicio->dado.id == 1);
    VERIFICA(proximo(lista.inicio->dado.escudo, 50.0f)); //copiou o struct inteiro

    conferirLista(&lista, 1);

    liberarListaInimigos(&lista);
}

//insere no fim sem andar na lista, e nunca mexe no alvo
static void testarInsercaoSempreNoFim(void){
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

    liberarListaInimigos(&lista);
}

static void testarLiberarListaVazia(void){
    ListaInimigos lista;
    memset(&lista, 0, sizeof(lista));

    liberarListaInimigos(&lista); //nao pode quebrar

    VERIFICA(lista.inicio == NULL);
    VERIFICA(lista.fim == NULL);
    VERIFICA(lista.alvo == NULL);
    VERIFICA(lista.tamanho == 0);
}

static void testarLiberarComNos(void){
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

static void testarRemoverNoUnico(void){
    ListaInimigos lista;
    memset(&lista, 0, sizeof(lista));
    montarLista(&lista, 1);

    removerInimigo(&lista, lista.inicio);

    VERIFICA(lista.tamanho == 0);
    VERIFICA(lista.inicio == NULL);
    VERIFICA(lista.fim == NULL);
    VERIFICA(lista.alvo == NULL); //sem vizinho, alvo vira NULL
}

static void testarRemoverInicio(void){
    ListaInimigos lista;
    memset(&lista, 0, sizeof(lista));
    montarLista(&lista, 3);

    removerInimigo(&lista, lista.inicio);

    conferirLista(&lista, 2);
    VERIFICA(lista.inicio->dado.id == 2);
    VERIFICA(lista.fim->dado.id == 3);

    liberarListaInimigos(&lista);
}

static void testarRemoverMeio(void){
    ListaInimigos lista;
    memset(&lista, 0, sizeof(lista));
    montarLista(&lista, 3);

    removerInimigo(&lista, buscarPorId(&lista, 2));

    conferirLista(&lista, 2);
    VERIFICA(lista.inicio->dado.id == 1);
    VERIFICA(lista.fim->dado.id == 3);
    VERIFICA(lista.inicio->prox == lista.fim); //1 cola direto no 3
    VERIFICA(lista.fim->ant == lista.inicio);

    liberarListaInimigos(&lista);
}

static void testarRemoverFim(void){
    ListaInimigos lista;
    memset(&lista, 0, sizeof(lista));
    montarLista(&lista, 3);

    removerInimigo(&lista, lista.fim);

    conferirLista(&lista, 2);
    VERIFICA(lista.inicio->dado.id == 1);
    VERIFICA(lista.fim->dado.id == 2);

    liberarListaInimigos(&lista);
}

//alvo era o inicio: passa pro prox
static void testarAlvoMudaProProx(void){
    ListaInimigos lista;
    memset(&lista, 0, sizeof(lista));
    montarLista(&lista, 3);

    VERIFICA(lista.alvo->dado.id == 1);

    removerInimigo(&lista, lista.inicio);

    conferirLista(&lista, 2);
    VERIFICA(lista.alvo->dado.id == 2);

    liberarListaInimigos(&lista);
}

//alvo era o fim: nao tem prox, cai no ant
static void testarAlvoNoFimVaiProAnt(void){
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

    liberarListaInimigos(&lista);
}

//remove o alvo do meio, depois sobra ele mesmo sem vizinhos
static void testarAlvoAteSobrarSoEle(void){
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

static void testarRemoverNullNaoFazNada(void){
    ListaInimigos lista;
    memset(&lista, 0, sizeof(lista));
    montarLista(&lista, 3);
    NoInimigo *alvoAntes = lista.alvo;

    removerInimigo(&lista, NULL);

    VERIFICA(lista.tamanho == 3);
    VERIFICA(lista.alvo == alvoAntes);
    conferirLista(&lista, 3);

    liberarListaInimigos(&lista);
}

//padrao da secao 3.5: guarda o prox antes de processar o no
static void testarRemocaoComIteracaoSegura(void){
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

static void testarMiraEmListaVazia(void){
    ListaInimigos lista;
    memset(&lista, 0, sizeof(lista));

    proximoAlvo(&lista);
    VERIFICA(lista.alvo == NULL);

    alvoAnterior(&lista);
    VERIFICA(lista.alvo == NULL);
    VERIFICA(lista.tamanho == 0);
}

static void testarCicloProximo(void){
    ListaInimigos lista;
    memset(&lista, 0, sizeof(lista));
    montarLista(&lista, 3);

    VERIFICA(lista.alvo->dado.id == 1);

    proximoAlvo(&lista);
    VERIFICA(lista.alvo->dado.id == 2);

    proximoAlvo(&lista);
    VERIFICA(lista.alvo->dado.id == 3);

    proximoAlvo(&lista);
    VERIFICA(lista.alvo->dado.id == 1); //tava no fim, deu a volta

    NoInimigo *antes = lista.alvo;
    proximoAlvo(&lista);
    proximoAlvo(&lista);
    proximoAlvo(&lista);
    VERIFICA(lista.alvo == antes);

    liberarListaInimigos(&lista);
}

static void testarCicloAnterior(void){
    ListaInimigos lista;
    memset(&lista, 0, sizeof(lista));
    montarLista(&lista, 3);

    alvoAnterior(&lista);
    VERIFICA(lista.alvo->dado.id == 3); //tava no comeco, voltou pro fim

    alvoAnterior(&lista);
    VERIFICA(lista.alvo->dado.id == 2);

    alvoAnterior(&lista);
    VERIFICA(lista.alvo->dado.id == 1);

    alvoAnterior(&lista);
    VERIFICA(lista.alvo->dado.id == 3);

    liberarListaInimigos(&lista);
}

static void testarUmNoFicaNeleMesmo(void){
    ListaInimigos lista;
    memset(&lista, 0, sizeof(lista));
    montarLista(&lista, 1);
    NoInimigo *unico = lista.alvo;

    proximoAlvo(&lista);
    VERIFICA(lista.alvo == unico);

    alvoAnterior(&lista);
    VERIFICA(lista.alvo == unico);

    liberarListaInimigos(&lista);
}

static void testarIdaEVolta(void){
    ListaInimigos lista;
    memset(&lista, 0, sizeof(lista));
    montarLista(&lista, 5);
    NoInimigo *antes = lista.alvo;

    for(int i = 0; i < 5; i++){
        proximoAlvo(&lista);
    }
    VERIFICA(lista.alvo == antes);

    for(int i = 0; i < 5; i++){
        alvoAnterior(&lista);
    }
    VERIFICA(lista.alvo == antes);

    liberarListaInimigos(&lista);
}

static void testarMiraAposInsercaoERemocao(void){
    ListaInimigos lista;
    memset(&lista, 0, sizeof(lista));
    montarLista(&lista, 3);

    proximoAlvo(&lista);
    NoInimigo *alvo = lista.alvo;
    VERIFICA(alvo->dado.id == 2);

    //spawn de reforco nao rouba a mira
    inserirInimigo(&lista, criarInimigo(XWING, (Vector3){0.0f, 0.0f, 9.0f}, 4));
    VERIFICA(lista.alvo == alvo);
    conferirLista(&lista, 4);

    //abater outro no tambem nao
    removerInimigo(&lista, buscarPorId(&lista, 3));
    VERIFICA(lista.alvo == alvo);
    conferirLista(&lista, 3);

    //abater o proprio alvo segue a secao 3.5
    removerInimigo(&lista, alvo);
    VERIFICA(lista.alvo->dado.id == 4);
    conferirLista(&lista, 2);

    liberarListaInimigos(&lista);
}

static void testarOrdenarListaVazia(void){
    ListaInimigos lista;
    memset(&lista, 0, sizeof(lista));

    ordenarPorDistancia(&lista);

    VERIFICA(lista.inicio == NULL);
    VERIFICA(lista.fim == NULL);
    VERIFICA(lista.alvo == NULL);
    VERIFICA(lista.tamanho == 0);
}

static void testarOrdenarUmNo(void){
    ListaInimigos lista;
    memset(&lista, 0, sizeof(lista));
    montarLista(&lista, 1);
    float v[] = {42.0f};
    setarDistancias(&lista, v, 1);
    NoInimigo *unico = lista.inicio;

    ordenarPorDistancia(&lista);

    VERIFICA(lista.inicio == unico);
    VERIFICA(lista.fim == unico);
    VERIFICA(lista.alvo == unico);
    conferirLista(&lista, 1);

    liberarListaInimigos(&lista);
}

static void testarOrdenarDesordenada(void){
    ListaInimigos lista;
    memset(&lista, 0, sizeof(lista));
    montarLista(&lista, 3);
    float v[] = {30.0f, 10.0f, 20.0f};
    setarDistancias(&lista, v, 3);

    ordenarPorDistancia(&lista);

    conferirLista(&lista, 3);
    VERIFICA(estaCrescente(&lista) == 1);
    VERIFICA(lista.inicio->dado.id == 2); //o mais perto vira o inicio
    VERIFICA(lista.inicio->prox->dado.id == 3);
    VERIFICA(lista.fim->dado.id == 1); //o mais longe vira o fim
    VERIFICA(proximo(lista.inicio->dado.distancia, 10.0f));
    VERIFICA(proximo(lista.fim->dado.distancia, 30.0f));

    liberarListaInimigos(&lista);
}

static void testarOrdenarJaOrdenada(void){
    ListaInimigos lista;
    memset(&lista, 0, sizeof(lista));
    montarLista(&lista, 3);
    float v[] = {10.0f, 20.0f, 30.0f};
    setarDistancias(&lista, v, 3);

    ordenarPorDistancia(&lista);

    conferirLista(&lista, 3);
    VERIFICA(estaCrescente(&lista) == 1);
    VERIFICA(lista.inicio->dado.id == 1); //nada saiu do lugar
    VERIFICA(lista.inicio->prox->dado.id == 2);
    VERIFICA(lista.fim->dado.id == 3);

    liberarListaInimigos(&lista);
}

static void testarOrdenarInvertida(void){
    ListaInimigos lista;
    memset(&lista, 0, sizeof(lista));
    montarLista(&lista, 4);
    float v[] = {40.0f, 30.0f, 20.0f, 10.0f};
    setarDistancias(&lista, v, 4);

    ordenarPorDistancia(&lista);

    conferirLista(&lista, 4);
    VERIFICA(estaCrescente(&lista) == 1);
    VERIFICA(lista.inicio->dado.id == 4);
    VERIFICA(lista.inicio->prox->dado.id == 3);
    VERIFICA(lista.fim->dado.id == 1);

    liberarListaInimigos(&lista);
}

//o criterio 2: alvo continua sendo o mesmo no, nao o mais proximo
static void testarAlvoContinuaOMesmoNo(void){
    ListaInimigos lista;
    memset(&lista, 0, sizeof(lista));
    montarLista(&lista, 3);
    float v[] = {30.0f, 10.0f, 20.0f};
    setarDistancias(&lista, v, 3);

    lista.alvo = buscarPorId(&lista, 3);
    NoInimigo *alvo = lista.alvo;

    ordenarPorDistancia(&lista);

    conferirLista(&lista, 3);
    VERIFICA(lista.alvo == alvo); //mesmo ponteiro
    VERIFICA(lista.alvo->dado.id == 3); //mesmo inimigo
    VERIFICA(lista.alvo != lista.inicio); //mas deixou de ser o inicio
    VERIFICA(lista.alvo == lista.inicio->prox); //esta no meio agora

    liberarListaInimigos(&lista);
}

//empatados mantem a ordem de chegada
static void testarOrdenacaoEstavel(void){
    ListaInimigos lista;
    memset(&lista, 0, sizeof(lista));
    montarLista(&lista, 4);
    float v[] = {20.0f, 10.0f, 20.0f, 5.0f};
    setarDistancias(&lista, v, 4);

    ordenarPorDistancia(&lista);

    conferirLista(&lista, 4);
    VERIFICA(estaCrescente(&lista) == 1);
    VERIFICA(lista.inicio->dado.id == 4); //5
    VERIFICA(lista.inicio->prox->dado.id == 2); //10
    VERIFICA(lista.inicio->prox->prox->dado.id == 1); //20, chegou antes do id 3
    VERIFICA(lista.fim->dado.id == 3); //20 empatado, ficou depois

    liberarListaInimigos(&lista);
}

static void testarAtualizarRecalculaDistancia(void){
    memset(&jogo, 0, sizeof(jogo));
    jogo.jogador.pos = (Vector3){0.0f, 0.0f, 0.0f};
    porNaFrente(&jogo.inimigos, 1, 100.0f);
    porNaFrente(&jogo.inimigos, 2, 200.0f);
    porNaFrente(&jogo.inimigos, 3, 300.0f);

    atualizarInimigos(&jogo, 1.0f / 60.0f);

    VERIFICA(proximo(jogo.inimigos.inicio->dado.distancia, 100.0f));
    VERIFICA(proximo(jogo.inimigos.inicio->prox->dado.distancia, 200.0f));
    VERIFICA(proximo(jogo.inimigos.fim->dado.distancia, 300.0f));
    VERIFICA(jogo.inimigos.tamanho == 3);

    liberarListaInimigos(&jogo.inimigos);
}

static void testarAtualizarListaVazia(void){
    memset(&jogo, 0, sizeof(jogo));

    atualizarInimigos(&jogo, 1.0f / 60.0f);

    VERIFICA(jogo.inimigos.inicio == NULL);
    VERIFICA(jogo.inimigos.fim == NULL);
    VERIFICA(jogo.inimigos.alvo == NULL);
    VERIFICA(jogo.inimigos.tamanho == 0);
}

static void testarDespawnForaDaArena(void){
    int i, ativas = 0;

    memset(&jogo, 0, sizeof(jogo));
    porNaFrente(&jogo.inimigos, 1, 500.0f);
    porNaFrente(&jogo.inimigos, 2, RAIO_ARENA + 500.0f);
    porNaFrente(&jogo.inimigos, 3, 600.0f);

    atualizarInimigos(&jogo, 1.0f / 60.0f);

    conferirLista(&jogo.inimigos, 2);
    VERIFICA(jogo.inimigos.inicio->dado.id == 1);
    VERIFICA(jogo.inimigos.fim->dado.id == 3);
    VERIFICA(jogo.inimigos.inicio->prox == jogo.inimigos.fim);


    VERIFICA(jogo.abates == 0);
    for(i = 0; i < MAX_EXPLOSOES; i++)
        ativas += jogo.explosoes[i].ativa != 0;
    VERIFICA(ativas == 0);

    liberarListaInimigos(&jogo.inimigos);
}

static void testarDespawnDeVariosSeguidos(void){
    memset(&jogo, 0, sizeof(jogo));
    porNaFrente(&jogo.inimigos, 1, 500.0f);
    porNaFrente(&jogo.inimigos, 2, RAIO_ARENA + 100.0f);
    porNaFrente(&jogo.inimigos, 3, RAIO_ARENA + 200.0f);
    porNaFrente(&jogo.inimigos, 4, 600.0f);

    atualizarInimigos(&jogo, 1.0f / 60.0f);

    conferirLista(&jogo.inimigos, 2);
    VERIFICA(jogo.inimigos.inicio->dado.id == 1);
    VERIFICA(jogo.inimigos.fim->dado.id == 4);

    liberarListaInimigos(&jogo.inimigos);
}

static void testarAlvoDespawnado(void){
    memset(&jogo, 0, sizeof(jogo));
    porNaFrente(&jogo.inimigos, 1, 500.0f);
    porNaFrente(&jogo.inimigos, 2, 600.0f);
    porNaFrente(&jogo.inimigos, 3, RAIO_ARENA + 500.0f);
    jogo.inimigos.alvo = buscarPorId(&jogo.inimigos, 3);

    atualizarInimigos(&jogo, 1.0f / 60.0f);

    conferirLista(&jogo.inimigos, 2);
    VERIFICA(jogo.inimigos.alvo != NULL);
    VERIFICA(jogo.inimigos.alvo->dado.id == 2);
    VERIFICA(jogo.inimigos.alvo == jogo.inimigos.fim);

    liberarListaInimigos(&jogo.inimigos);
}

static void testarArenaNaoEADistanciaDoJogador(void){
    memset(&jogo, 0, sizeof(jogo));
    jogo.jogador.pos = (Vector3){0.0f, 0.0f, -500.0f};
    porNaFrente(&jogo.inimigos, 1, 1900.0f);

    atualizarInimigos(&jogo, 1.0f / 60.0f);

    VERIFICA(jogo.inimigos.tamanho == 1);
    VERIFICA(jogo.inimigos.inicio->dado.id == 1);
    VERIFICA(proximo(jogo.inimigos.inicio->dado.distancia, 2400.0f));

    liberarListaInimigos(&jogo.inimigos);
}

static void testarAtualizarMovePelaIA(void){
    memset(&jogo, 0, sizeof(jogo));
    porNaFrente(&jogo.inimigos, 1, 1000.0f);

    atualizarInimigos(&jogo, 1.0f / 60.0f);

    //IA-01 (#49): o X-Wing anda na direcao do Destroier, na origem
    VERIFICA(jogo.inimigos.inicio->dado.pos.z < 1000.0f);
    VERIFICA(!vetorNulo(jogo.inimigos.inicio->dado.vel));

    liberarListaInimigos(&jogo.inimigos);
}

int main(void){
    testarXWing();
    testarYWing();
    testarAWing();
    testarIdsDoJogo();
    testarVelocidadesDaTabela();

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

    testarMiraEmListaVazia();
    testarCicloProximo();
    testarCicloAnterior();
    testarUmNoFicaNeleMesmo();
    testarIdaEVolta();
    testarMiraAposInsercaoERemocao();

    testarOrdenarListaVazia();
    testarOrdenarUmNo();
    testarOrdenarDesordenada();
    testarOrdenarJaOrdenada();
    testarOrdenarInvertida();
    testarAlvoContinuaOMesmoNo();
    testarOrdenacaoEstavel();

    testarAtualizarRecalculaDistancia();
    testarAtualizarListaVazia();
    testarDespawnForaDaArena();
    testarDespawnDeVariosSeguidos();
    testarAlvoDespawnado();
    testarArenaNaoEADistanciaDoJogador();
    testarAtualizarMovePelaIA();

    return RESULTADO_TESTES();
}
