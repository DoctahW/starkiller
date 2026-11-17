#ifndef HUD_H
#define HUD_H

#include "tipos.h"

/* Fonte do HUD; chamar depois de InitWindow. Sem o arquivo, usa a fonte padrão da raylib. */
void hud_carregar(void);
/* Libera a fonte; chamar antes de CloseWindow. */
void hud_descarregar(void);

/*
 * Desenha a cabine do TIE em 2D por cima da cena (depois de render_cena, fora do BeginMode3D):
 * moldura da janela, retícula central, medidores de casco do TIE e do Destróier, tempo restante,
 * pontos e multiplicador de sequência, e o console com velocidade, status, missão e navegação.
 * Com alvo travado: colchetes, tipo e distância sobre o caça, indicador de tiro adiantado (verde
 * dentro da retícula) e seta quando ele está fora do vidro; no console, tipo, distância e
 * integridade, e no visor um escopo com a direção dele.
 * `foraDaArena` (retorno de atualizarJogador) liga o aviso de limite da arena. Não altera o jogo.
 */
void hud_desenhar(const Jogo *g, int foraDaArena);

#endif
