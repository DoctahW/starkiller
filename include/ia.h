#ifndef IA_H
#define IA_H

#include "tipos.h"

/*
 * Seção 3.4, passo 4: direciona `vel` suavemente para o alvo do estado atual,
 * com a velocidade máxima da tabela 2.2 para o tipo, e avança `pos` com `vel`.
 * Y-Wing em IA_ATACAR_DESTROIER orbita o Destróier e bombardeia a menos de 150.
 * Não reescreve o estado: quem alterna entre atacar o Destróier e o jogador é a IA-03.
 */
void ia_atualizar(Inimigo *in, Jogo *g, float dt);

#endif
