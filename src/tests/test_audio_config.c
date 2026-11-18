#include <math.h>
#include <stdio.h>

#include "audio.h"
#include "teste.h"

#define PASTA   "build/tests/dados_audio_teste"
#define ARQUIVO PASTA "/audio.txt"

static int quase(float a, float b)
{
    return fabsf(a - b) < 1e-6f;
}

static void escrever(const char *texto)
{
    FILE *f = fopen(ARQUIVO, "w");

    fputs(texto, f);
    fclose(f);
}

int main(void)
{
    ConfigAudio cfg;

    remove(ARQUIVO);
    remove(PASTA);

    /* arquivo ausente: mantém os padrões */
    audioConfigPadroes(&cfg);
    VERIFICA(quase(cfg.geral, AUDIO_VOLUME_GERAL_PADRAO));
    VERIFICA(quase(cfg.efeitos, AUDIO_VOLUME_EFEITOS_PADRAO));
    VERIFICA(quase(cfg.musica, AUDIO_VOLUME_MUSICA_PADRAO));
    VERIFICA(audioConfigCarregar(&cfg, ARQUIVO) == 0);

    /* salvar cria a pasta e o arquivo; carregar devolve os mesmos valores */
    cfg.geral = 0.65f;
    cfg.efeitos = 0.35f;
    cfg.musica = 0.10f;
    VERIFICA(audioConfigSalvar(&cfg, ARQUIVO) == 1);
    VERIFICA(DirectoryExists(PASTA));

    audioConfigPadroes(&cfg);
    VERIFICA(audioConfigCarregar(&cfg, ARQUIVO) == 1);
    VERIFICA(quase(cfg.geral, 0.65f));
    VERIFICA(quase(cfg.efeitos, 0.35f));
    VERIFICA(quase(cfg.musica, 0.10f));

    /* limites aceitos */
    escrever("volumeGeral=0\nvolumeEfeitos=1\nvolumeMusica=0.5\n");
    audioConfigPadroes(&cfg);
    VERIFICA(audioConfigCarregar(&cfg, ARQUIVO) == 1);
    VERIFICA(quase(cfg.geral, 0.0f));
    VERIFICA(quase(cfg.efeitos, 1.0f));
    VERIFICA(quase(cfg.musica, 0.5f));

    /* inválidos ficam no padrão; os válidos ainda são lidos */
    escrever("volumeGeral=1.5\nvolumeEfeitos=-0.1\nvolumeMusica=nan\n");
    audioConfigPadroes(&cfg);
    VERIFICA(audioConfigCarregar(&cfg, ARQUIVO) == 1);
    VERIFICA(quase(cfg.geral, AUDIO_VOLUME_GERAL_PADRAO));
    VERIFICA(quase(cfg.efeitos, AUDIO_VOLUME_EFEITOS_PADRAO));
    VERIFICA(quase(cfg.musica, AUDIO_VOLUME_MUSICA_PADRAO));

    escrever("volumeGeral=0.25abc\nvolumeEfeitos=\nvolumeMusica=1x\nlixo\nsem igual\n");
    audioConfigPadroes(&cfg);
    audioConfigCarregar(&cfg, ARQUIVO);
    VERIFICA(quase(cfg.geral, AUDIO_VOLUME_GERAL_PADRAO));
    VERIFICA(quase(cfg.efeitos, AUDIO_VOLUME_EFEITOS_PADRAO));
    VERIFICA(quase(cfg.musica, AUDIO_VOLUME_MUSICA_PADRAO));

    /* a configuração viva é sempre acessível e audioAplicarVolumes limita tudo */
    VERIFICA(audioConfig() != NULL);
    audioConfig()->geral = 2.0f;
    audioConfig()->efeitos = -1.0f;
    audioConfig()->musica = NAN;
    audioAplicarVolumes();
    VERIFICA(quase(audioConfig()->geral, 1.0f));
    VERIFICA(quase(audioConfig()->efeitos, 0.0f));
    VERIFICA(quase(audioConfig()->musica, 0.0f));

    return RESULTADO_TESTES();
}
