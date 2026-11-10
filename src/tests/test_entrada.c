#include "entrada.h"
#include "teste.h"

#define PASTA   "build/tests/dados_teste"
#define ARQUIVO PASTA "/config.txt"

static ConfigEntrada padrao(void)
{
    return (ConfigEntrada){ .sensibilidade = 1.0f, .zonaMorta = 0.15f, .inverterArfagem = 0, .vibracao = 1 };
}

static void escrever(const char *texto)
{
    FILE *f = fopen(ARQUIVO, "w");
    fputs(texto, f);
    fclose(f);
}

int main(void)
{
    remove(ARQUIVO);
    remove(PASTA);

    /* arquivo ausente: mantém os padrões */
    ConfigEntrada cfg = padrao();
    VERIFICA(carregarConfigEntrada(&cfg, ARQUIVO) == 0);
    VERIFICA(cfg.sensibilidade == 1.0f && cfg.zonaMorta == 0.15f && cfg.inverterArfagem == 0 && cfg.vibracao == 1);

    /* salvar cria a pasta e o arquivo; carregar devolve os mesmos valores */
    ConfigEntrada salvo = { .sensibilidade = 1.75f, .zonaMorta = 0.3f, .inverterArfagem = 1, .vibracao = 0 };
    VERIFICA(salvarConfigEntrada(&salvo, ARQUIVO) == 1);
    VERIFICA(DirectoryExists(PASTA));
    cfg = padrao();
    VERIFICA(carregarConfigEntrada(&cfg, ARQUIVO) == 1);
    VERIFICA(cfg.sensibilidade == 1.75f && cfg.zonaMorta == 0.3f && cfg.inverterArfagem == 1 && cfg.vibracao == 0);

    /* limites da faixa são aceitos */
    escrever("sensibilidade=0.5\nzonaMorta=0.5\n");
    cfg = padrao();
    carregarConfigEntrada(&cfg, ARQUIVO);
    VERIFICA(cfg.sensibilidade == SENSIBILIDADE_MIN && cfg.zonaMorta == ZONA_MORTA_MAX);

    /* valores inválidos ou fora da faixa ficam no padrão; os válidos ainda são lidos */
    escrever("sensibilidade=2.01\nzonaMorta=-0.1\ninverterArfagem=2\nvibracao=0\n");
    cfg = padrao();
    carregarConfigEntrada(&cfg, ARQUIVO);
    VERIFICA(cfg.sensibilidade == 1.0f && cfg.zonaMorta == 0.15f && cfg.inverterArfagem == 0 && cfg.vibracao == 0);

    escrever("sensibilidade=1.2abc\nzonaMorta=nan\ninverterArfagem=\nlixo\nsem igual nenhum\nvibracao=1x\n");
    cfg = padrao();
    carregarConfigEntrada(&cfg, ARQUIVO);
    VERIFICA(cfg.sensibilidade == 1.0f && cfg.zonaMorta == 0.15f && cfg.inverterArfagem == 0 && cfg.vibracao == 1);

    return RESULTADO_TESTES();
}
