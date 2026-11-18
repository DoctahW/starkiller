#include "audio.h"

#include <stdio.h>
#include <string.h>

/*
 * Formato igual ao de entrada.c: uma linha `chave=valor` por campo; linhas desconhecidas
 * são ignoradas. Valores fora de [0, 1] ou malformados mantêm o valor atual.
 */

void audioConfigPadroes(ConfigAudio *cfg)
{
    cfg->geral = AUDIO_VOLUME_GERAL_PADRAO;
    cfg->efeitos = AUDIO_VOLUME_EFEITOS_PADRAO;
    cfg->musica = AUDIO_VOLUME_MUSICA_PADRAO;
}

static int lerVolume(const char *texto, float *saida)
{
    float v;
    char resto;

    if (sscanf(texto, "%f %c", &v, &resto) != 1
        || !(v >= AUDIO_VOLUME_MIN && v <= AUDIO_VOLUME_MAX))
        return 0;
    *saida = v;
    return 1;
}

int audioConfigCarregar(ConfigAudio *cfg, const char *caminho)
{
    FILE *f = fopen(caminho, "r");
    char linha[128];

    if (!f)
        return 0;

    while (fgets(linha, sizeof linha, f)) {
        char *igual = strchr(linha, '=');
        const char *chave, *valor;

        if (!igual)
            continue;
        *igual = '\0';
        chave = linha;
        valor = igual + 1;

        if (strcmp(chave, "volumeGeral") == 0)
            lerVolume(valor, &cfg->geral);
        else if (strcmp(chave, "volumeEfeitos") == 0)
            lerVolume(valor, &cfg->efeitos);
        else if (strcmp(chave, "volumeMusica") == 0)
            lerVolume(valor, &cfg->musica);
    }

    fclose(f);
    return 1;
}

int audioConfigSalvar(const ConfigAudio *cfg, const char *caminho)
{
    const char *pasta = GetDirectoryPath(caminho);
    FILE *f;

    if (pasta[0] != '\0' && !DirectoryExists(pasta) && MakeDirectory(pasta) != 0)
        return 0;

    f = fopen(caminho, "w");
    if (!f)
        return 0;

    fprintf(f, "volumeGeral=%.2f\nvolumeEfeitos=%.2f\nvolumeMusica=%.2f\n",
            cfg->geral, cfg->efeitos, cfg->musica);
    return fclose(f) == 0;
}
