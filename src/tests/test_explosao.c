#include <math.h>
#include <string.h>

#include "combate.h"
#include "teste.h"

#define EPS 1e-4f

static Jogo jogo;

static int proximo(float a, float b){
    return fabsf(a - b) < EPS;
}

static int contarAtivas(void){
    int i, n = 0;

    for(i = 0; i < MAX_EXPLOSOES; i++)
        n += jogo.explosoes[i].ativa != 0;
    return n;
}

static void novaPartida(void){
    memset(&jogo, 0, sizeof(jogo));
}

//criterio 1: a vida e ~0,8 s
static void testarDuracao(void){
    novaPartida();

    VERIFICA(proximo(DURACAO_EXPLOSAO, 0.8f));
    VERIFICA(criarExplosao(&jogo, (Vector3){0.0f, 0.0f, 0.0f}, RAIO_EXPLOSAO) == 1);

    atualizarExplosoes(&jogo, 0.4f);
    VERIFICA(contarAtivas() == 1);

    atualizarExplosoes(&jogo, 0.4f);
    VERIFICA(contarAtivas() == 0);
}

//a 60 Hz vive ~0,8 s: viva aos 46 passos, morta aos 50 (tolera o erro de float)
static void testarDuracaoEmPassosDe60Hz(void){
    int i;

    novaPartida();
    criarExplosao(&jogo, (Vector3){0.0f, 0.0f, 0.0f}, RAIO_EXPLOSAO);

    //ainda viva bem antes do fim (46 passos = 0,767 s)
    for(i = 0; i < 46; i++)
        atualizarExplosoes(&jogo, 1.0f / 60.0f);
    VERIFICA(contarAtivas() == 1);

    //morta logo depois (mais 4 passos = 50 passos = 0,833 s)
    for(i = 0; i < 4; i++)
        atualizarExplosoes(&jogo, 1.0f / 60.0f);
    VERIFICA(contarAtivas() == 0);
}

//criterio 2: o render recebe pos, tempo e raio sem inventar a curva
static void testarCurvaDoRaio(void){
    Explosao *e;

    novaPartida();
    criarExplosao(&jogo, (Vector3){1.0f, 2.0f, 3.0f}, 20.0f);
    e = &jogo.explosoes[0];

    VERIFICA(proximo(e->pos.z, 3.0f));
    VERIFICA(proximo(progressoExplosao(e), 0.0f));
    VERIFICA(proximo(raioExplosao(e), 0.0f));

    atualizarExplosoes(&jogo, 0.2f); //p = 0.25
    VERIFICA(proximo(progressoExplosao(e), 0.25f));
    VERIFICA(proximo(raioExplosao(e), 8.75f));

    atualizarExplosoes(&jogo, 0.2f); //p = 0.5
    VERIFICA(proximo(raioExplosao(e), 15.0f));

    atualizarExplosoes(&jogo, 0.2f); //p = 0.75
    VERIFICA(proximo(raioExplosao(e), 18.75f));
    VERIFICA(raioExplosao(e) < e->raioMax);
}

//curva monotona e concava: abre rapido e desacelera ate o raio cheio
static void testarRaioCresceEDesacelera(void){
    Explosao *e;
    float anterior = 0.0f, atual;
    int i;

    novaPartida();
    criarExplosao(&jogo, (Vector3){0.0f, 0.0f, 0.0f}, 20.0f);
    e = &jogo.explosoes[0];

    //7 passos de 0.1: no oitavo o tempo bate 0.8 e a explosao expira
    for(i = 0; i < 7; i++){
        atualizarExplosoes(&jogo, 0.1f);
        atual = raioExplosao(e);
        VERIFICA(atual >= anterior);
        anterior = atual;
    }
    VERIFICA(e->ativa == 1);
    VERIFICA(proximo(raioExplosao(e), 19.6875f));

    //o fim da curva com a explosao ainda viva: raio cheio, sem estourar
    e->tempo = DURACAO_EXPLOSAO;
    VERIFICA(proximo(progressoExplosao(e), 1.0f));
    VERIFICA(proximo(raioExplosao(e), 20.0f));
}

static void testarInativaEClamp(void){
    Explosao *e;

    novaPartida();
    e = &jogo.explosoes[0];

    //slot nunca usado
    VERIFICA(proximo(progressoExplosao(e), 0.0f));
    VERIFICA(proximo(raioExplosao(e), 0.0f));

    //tempo passado do fim nao estoura o raio
    criarExplosao(&jogo, (Vector3){0.0f, 0.0f, 0.0f}, 20.0f);
    e->tempo = DURACAO_EXPLOSAO * 2.0f;
    VERIFICA(proximo(progressoExplosao(e), 1.0f));
    VERIFICA(proximo(raioExplosao(e), 20.0f));

    //expirada devolve 0
    atualizarExplosoes(&jogo, 0.0f);
    VERIFICA(e->ativa == 0);
    VERIFICA(proximo(raioExplosao(e), 0.0f));
}

//duas explosoes nao compartilham curva
static void testarExplosoesIndependentes(void){
    novaPartida();
    criarExplosao(&jogo, (Vector3){0.0f, 0.0f, 0.0f}, 10.0f);
    criarExplosao(&jogo, (Vector3){5.0f, 0.0f, 0.0f}, 40.0f);

    atualizarExplosoes(&jogo, 0.4f);

    VERIFICA(contarAtivas() == 2);
    VERIFICA(proximo(raioExplosao(&jogo.explosoes[0]), 7.5f));
    VERIFICA(proximo(raioExplosao(&jogo.explosoes[1]), 30.0f));
}

int main(void){
    testarDuracao();
    testarDuracaoEmPassosDe60Hz();
    testarCurvaDoRaio();
    testarRaioCresceEDesacelera();
    testarInativaEClamp();
    testarExplosoesIndependentes();

    return RESULTADO_TESTES();
}
