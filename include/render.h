#ifndef RENDER_H
#define RENDER_H

#include "tipos.h"

/* Shaders em assets/shaders/<nome>.vs e .fs (GLSL 330). */
#define PASTA_SHADERS "assets/shaders/"

/* Destróier Estelar (medidas em destroier.h); sem o arquivo, é desenhado com primitivas. */
#define ARQUIVO_DESTROIER "assets/models/destroier.glb"

/* Cores dos caças rebeldes (VIS-04): primitivas da cena e pontos do radar do HUD. */
#define COR_XWING (Color){ 235, 105, 80, 255 }
#define COR_YWING (Color){ 245, 195, 65, 255 }
#define COR_AWING (Color){ 85, 195, 130, 255 }

void carregarAssets(void);
void descarregarAssets(void);

/*
 * Desenha a cena 3D vista da cabine (entre BeginDrawing/EndDrawing, antes do HUD):
 * primeiro o fundo do espaço, sem gravar profundidade, depois a arena.
 * `cam` é a câmera da cabine (cameraDoJogador); o fundo ignora a posição dela
 * e usa só a orientação do jogador.
 */
void render_cena(const Jogo *g, Camera3D cam);

/* Carrega PASTA_SHADERS<nome>.vs/.fs. Devolve 0 se faltar arquivo ou não compilar (a raylib devolveria o padrão). */
int carregarShader(const char *nome, Shader *s);

/* Mipmaps e filtro trilinear nas texturas dos materiais: sem eles a textura cintila ao longe e ao girar. */
void suavizarTexturas(Model *m);

/* UnloadModel não libera as texturas dos materiais; esta libera as que não são a padrão e depois o modelo. */
void descarregarModelo(Model m);

#endif
