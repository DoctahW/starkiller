#include "entrada.h"

void iniciarEntrada(ConfigEntrada *cfg)
{
    cfg->sensibilidade = 1.0f;
    cfg->zonaMorta = 0.15f;
    cfg->inverterArfagem = 0;
    cfg->vibracao = 1;
}

/* Teclado, só navegação de menus e pausa; voo, mira e controle entram em INP-01/INP-02. */
Comandos lerComandos(const ConfigEntrada *cfg)
{
    (void)cfg;
    Comandos c = {0};
    c.menuCima = IsKeyPressed(KEY_UP);
    c.menuBaixo = IsKeyPressed(KEY_DOWN);
    c.menuEsquerda = IsKeyPressed(KEY_LEFT);
    c.menuDireita = IsKeyPressed(KEY_RIGHT);
    c.confirmar = IsKeyPressed(KEY_ENTER);
    c.voltar = IsKeyPressed(KEY_ESCAPE);
    c.pausa = IsKeyPressed(KEY_ESCAPE);
    return c;
}
