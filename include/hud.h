#ifndef HUD_H
#define HUD_H

#include "tipos.h"

/* Fonte do HUD; chamar depois de InitWindow. Sem o arquivo, usa a fonte padrão da raylib. */
void hud_carregar(void);
/* Libera a fonte; chamar antes de CloseWindow. */
void hud_descarregar(void);

/*
 * Desenha a cabine do TIE em 2D por cima da cena (depois de render_cena, fora do BeginMode3D),
 * com os instrumentos montados na parte de baixo do cockpit, em painéis com face lateral extruda
 * para simular perspectiva:
 * - esquerda: painel de sensores com o radar redondo (inimigos, aliados e alvo travado);
 * - centro: console com a barra do Destróier (escudo sólido azul sobre o casco em segmentos
 *   vermelhos), faixa vazia de aviso que só aparece quando um Y-Wing bombardeia e o visor de
 *   combate com o modelo do alvo, nome acima, HP à esquerda e distância à direita;
 * - direita: reparo (cruz azul em cooldown / verde quando pronto), contador de vidas, modelo
 *   simples do TIE na cor do casco (verde/amarelo/vermelho piscando) e o HP atual;
 * - canto superior direito: tempo, pontos, abates, sequência, aliados e distância à borda;
 * - entre os painéis: a velocidade em barra vertical com o número do acelerador.
 * Mantém a retícula central, os colchetes/seta do alvo no vidro e o flash de dano.
 * `foraDaArena` (retorno de atualizarJogador) liga o aviso de limite da arena. Não altera o jogo.
 */
void hud_desenhar(const Jogo *g, int foraDaArena);

#endif
