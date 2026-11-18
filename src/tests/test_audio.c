#include <math.h>
#include <string.h>

#include "audio.h"
#include "jogo.h"
#include "nave.h"
#include "teste.h"

static Jogo jogo;
static const Comandos parado = { 0 };

/* A AUD-01 exige que som ausente não trave o jogo: sem iniciarAudio tudo vira no-op. */
static void testarSemDispositivo(void)
{
    int i;

    memset(&jogo, 0, sizeof jogo);
    jogo.jogador.casco = CASCO_JOGADOR;
    jogo.cascoDestroier = CASCO_MAX_DESTROIER;

    VERIFICA(audioDisponivel() == 0);

    audioTiroJogador();
    audioTiroInimigo(XWING, (Vector3){ 0.0f, 0.0f, 100.0f });
    audioDanoJogador();
    audioColisaoJogador();
    audioTrocaAlvo();
    audioSpinJogador();
    audioBoomJogador((Vector3){ 0.0f, 0.0f, 0.0f });
    audioPararSpins();
    audioDestroierDestruido();
    audioEscudoDestroier(1);
    audioEscudoDestroier(0);
    audioUIMover();
    audioUISelecionar();
    audioUIVoltar();
    audioUITecla();
    audioMusicaTocar(AUDIO_MUSICA_MENU, 1.0f);
    audioMusicaTocar(AUDIO_MUSICA_COMBATE, AUDIO_FATOR_PAUSA);
    audioMusicaTocar(AUDIO_MUSICA_RANKING, 1.0f);
    audioMusicaTocar(AUDIO_MUSICA_VITORIA, 1.0f);
    audioMusicaTocar(AUDIO_MUSICA_NENHUMA, 0.0f);
    audioAplicarVolumes();
    VERIFICA(audioConfig() != NULL);
    {
        int voz = audioSpinInimigo(AWING, (Vector3){ 1.0f, 2.0f, 3.0f });

        audioBoomInimigo(AWING, (Vector3){ 1.0f, 2.0f, 3.0f }, voz);
    }
    audioMotorRegistrar(AUDIO_MOTOR_ALIADO, 1, XWING, (Vector3){ 0.0f, 0.0f, 100.0f },
                        (Vector3){ 0.0f, 0.0f, -100.0f });

    for (i = 0; i < 120; i++)
        audioAtualizar(&jogo, &parado, 1, PASSO_FIXO);

    VERIFICA(audioDisponivel() == 0);
}

/* Queda do TIE: duração sorteada entre 1 s e a duração do áudio de SPIN (4,75 s). */
static void testarQueda(void)
{
    int i;

    for (i = 0; i < 50; i++) {
        float duracao;

        iniciarJogador(&jogo.jogador);
        VERIFICA(faseQuedaJogador() == 0);
        duracao = iniciarQuedaJogador();
        VERIFICA(duracao > 1.0f && duracao < 4.75f);
        VERIFICA(faseQuedaJogador() == 1);
        VERIFICA(atualizarJogador(&jogo.jogador, &parado, PASSO_FIXO) == 0);
        {
            int passos = 0;

            while (faseQuedaJogador() == 1 && passos < 400) {
                atualizarJogador(&jogo.jogador, &parado, PASSO_FIXO);
                passos++;
            }
            VERIFICA(passos > 60);              /* mais de 1 s girando */
            VERIFICA(faseQuedaJogador() == 2);
        }
    }

    /* iniciarJogador cancela a queda (nova partida) */
    iniciarJogador(&jogo.jogador);
    VERIFICA(faseQuedaJogador() == 0);
}

/* Doppler: aproximação sobe o pitch, afastamento desce, e os extremos ficam limitados. */
static void testarDoppler(void)
{
    Jogo g;
    Comandos cmd = { 0 };
    float aproximando, afastando;

    memset(&g, 0, sizeof g);
    g.jogador.pos = (Vector3){ 0.0f, 0.0f, 0.0f };
    g.jogador.frente = (Vector3){ 0.0f, 0.0f, 1.0f };
    g.jogador.direita = (Vector3){ -1.0f, 0.0f, 0.0f };
    g.jogador.velocidade = 0.0f;

    /* Sem dispositivo, audioAtualizar ainda atualiza o ouvinte usado pelo Doppler. */
    audioAtualizar(&g, &cmd, 0, PASSO_FIXO);

    VERIFICA(audioFatorDoppler((Vector3){ 0.0f, 0.0f, 500.0f }, (Vector3){ 0 }) == 1.0f);
    aproximando = audioFatorDoppler((Vector3){ 0.0f, 0.0f, 500.0f }, (Vector3){ 0.0f, 0.0f, -300.0f });
    afastando = audioFatorDoppler((Vector3){ 0.0f, 0.0f, 500.0f }, (Vector3){ 0.0f, 0.0f, 300.0f });
    VERIFICA(aproximando > 1.0f && aproximando <= 1.35f);
    VERIFICA(afastando < 1.0f && afastando >= 0.72f);

    /* Ouvinte indo na direção da fonte também sobe o pitch. */
    g.jogador.velocidade = 300.0f;
    audioAtualizar(&g, &cmd, 0, PASSO_FIXO);
    VERIFICA(audioFatorDoppler((Vector3){ 0.0f, 0.0f, 500.0f }, (Vector3){ 0 }) > 1.0f);

    /* Velocidades absurdas ficam presas aos limites. */
    g.jogador.velocidade = 0.0f;
    audioAtualizar(&g, &cmd, 0, PASSO_FIXO);
    VERIFICA(audioFatorDoppler((Vector3){ 0.0f, 0.0f, 500.0f },
                               (Vector3){ 0.0f, 0.0f, -10000.0f }) <= 1.35f);
    VERIFICA(audioFatorDoppler((Vector3){ 0.0f, 0.0f, 500.0f },
                               (Vector3){ 0.0f, 0.0f, 10000.0f }) >= 0.72f);

    /* Com o alcance maior, a fonte distante tem o mesmo Doppler atenuado. */
    {
        float perto = audioFatorDoppler((Vector3){ 0.0f, 0.0f, 500.0f },
                                        (Vector3){ 0.0f, 0.0f, -300.0f });
        float longe = audioFatorDoppler((Vector3){ 0.0f, 0.0f, 2000.0f },
                                        (Vector3){ 0.0f, 0.0f, -300.0f });

        VERIFICA(longe > 1.0f);
        VERIFICA(fabsf(longe - 1.0f) < fabsf(perto - 1.0f));
    }
}

int main(void)
{
    testarSemDispositivo();
    testarDoppler();
    testarQueda();
    return RESULTADO_TESTES();
}
