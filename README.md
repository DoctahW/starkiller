# Starkiller

Jogo de combate espacial em C99 com raylib (3D). O jogador pilota um caça TIE, com visão de cabine, e defende um Destróier Estelar contra ondas crescentes de X-Wings, Y-Wings e A-Wings por 5 minutos.

O planejamento (stories, epics, milestones e responsáveis) está nas **Issues** e **Milestones** do GitHub.

## Requisitos

- Compilador C (gcc ou clang) e `make`
- SDL3
- raylib 6.2 compilada como biblioteca estática com o backend SDL3 (`PLATFORM_DESKTOP_SDL`). Os pacotes e o instalador oficial da raylib usam GLFW, que não suporta vibração do controle.

O `Makefile` procura a raylib em `RAYLIB_PATH` (a pasta `raylib/src`, com `raylib.h` e `libraylib.a`); padrão `~/raylib/src` no Linux e `C:/raylib/raylib/src` no Windows.

### Linux

1. SDL3:
   - Arch: `sudo pacman -S sdl3`
   - Debian 13 / Ubuntu 25.04 ou mais novo: `sudo apt install libsdl3-dev`
   - Versões sem o pacote: compilar a SDL3 a partir do código-fonte, conforme o [guia oficial](https://wiki.libsdl.org/SDL3/README-cmake), e rodar `sudo cmake --install`.
2. raylib:

   ```sh
   curl -fsSL https://github.com/raysan5/raylib/archive/refs/tags/6.2.tar.gz | tar xz -C ~
   mv ~/raylib-6.2 ~/raylib
   make -C ~/raylib/src PLATFORM=PLATFORM_DESKTOP_SDL \
     SDL_INCLUDE_PATH="$(pkg-config --variable=includedir sdl3)" \
     SDL_LIBRARY_PATH="$(pkg-config --variable=libdir sdl3)" \
     SDL_LIBRARIES=-lSDL3 CUSTOM_CFLAGS=-DUSING_SDL3_PROJECT
   ```

   O pacote `raylib` do sistema (GLFW) pode continuar instalado; o jogo usa a de `RAYLIB_PATH`.

### Windows

1. Instalar a raylib pelo instalador oficial ([raylib.com](https://www.raylib.com/)), que inclui o w64devkit (gcc e make) em `C:\raylib`.
2. Substituir `C:\raylib\raylib` pelo código-fonte da [raylib 6.2](https://github.com/raysan5/raylib/archive/refs/tags/6.2.zip).
3. Baixar o `SDL3-devel-<versão>-mingw` nos [releases da SDL](https://github.com/libsdl-org/SDL/releases) e extrair de forma que exista `C:\SDL3\x86_64-w64-mingw32`. Se ficar em outro lugar, definir `SDL3_PATH` apontando para essa pasta `x86_64-w64-mingw32`.
4. No terminal do w64devkit (`C:\raylib\w64devkit\w64devkit.exe`):

   ```sh
   cd C:/raylib/raylib/src
   make PLATFORM=PLATFORM_DESKTOP_SDL \
     SDL_INCLUDE_PATH=C:/SDL3/x86_64-w64-mingw32/include \
     SDL_LIBRARY_PATH=C:/SDL3/x86_64-w64-mingw32/lib \
     SDL_LIBRARIES=-lSDL3 CUSTOM_CFLAGS=-DUSING_SDL3_PROJECT
   ```

5. Rodar os mesmos comandos `make` abaixo; o `make` copia a `SDL3.dll` para a raiz do projeto. Se a raylib estiver em outro lugar, definir `RAYLIB_PATH` apontando para a pasta `raylib\src`.

## Compilar e rodar


| Comando         | O que faz                                                   |
| --------------- | ----------------------------------------------------------- |
| `make`          | Compila o jogo (`./starkiller` ou `starkiller.exe`)         |
| `make run`      | Compila e roda                                              |
| `make test`     | Compila e roda os testes de console em `src/tests/test_*.c` |
| `make valgrind` | Roda os testes sob `valgrind --leak-check=full` (Linux)     |
| `make clean`    | Remove os artefatos de build                                |


O `.clangd` já configura o clangd para encontrar os headers em `include/`.

A CI (`.github/workflows/ci.yml`) roda `make` e `make test` em `ubuntu-latest` contra a raylib 6.2 com SDL3 (as duas compiladas na CI e guardadas em cache) em cada PR e push na `main`. O check `build e testes` é obrigatório para o merge na `main`.

## Controles


| Ação                    | Teclado      | Controle Xbox            |
| ----------------------- | ------------ | ------------------------ |
| Arfagem                 | W / S        | Analógico esquerdo ↑ / ↓ |
| Guinada                 | A / D        | Analógico esquerdo ← / → |
| Rolagem                 | Q / E        | LB / RB                  |
| Acelerar / frear        | Shift / Ctrl | Analógico direito ↑ / ↓  |
| Disparar                | Espaço       | RT                       |
| Próximo / anterior alvo | T / Y        | D-pad → / ←              |
| Alvo mais próximo       | R            | LT                       |
| Pausa                   | Esc          | Start                    |
| Confirmar / voltar      | Enter / Esc ou Backspace | A / B           |


## Estrutura

```
assets/          models/ (fontes/: scripts do Blender), shaders/ (GLSL 330), textures/, sounds/ (efeitos em OGG Vorbis), fonts/, img/
include/         headers públicos de cada módulo
  tipos.h        contrato compartilhado entre os módulos
  teste.h        verificações dos testes de console
src/
  main.c         ponto de entrada
  destroier.c    medidas do Destróier e colisão com o casco (mapa de colunas em casco_destroier.inc)
  jogo.c         janela, máquina de estados e loop de combate
  entrada.c      teclado e controle -> Comandos
  nave.c         voo, câmera de cabine e queda do TIE
  combate.c      lasers, colisões, dano, explosões e destroços girando
  inimigos.c     lista duplamente encadeada e computador de mira
  reforcos.c     fila de reforços
  ia.c           comportamento dos caças rebeldes
  render.c       cena 3D: ordem de desenho, Destróier e carga de modelos e shaders
  espaco.c       fundo: estrelas, nebulosa, Estrela da Morte e poeira
  hud.c          cabine do TIE: moldura, retícula, medidores, tempo, pontos, console, radar, aviso de arena e alvo travado
  placar.c       árvore binária de busca do placar
  pontuacao.c    pontuação
  telas.c        menu, fim de partida, ranking e controles
  audio.c        efeitos sonoros, loops e espacialização (AUD-01)
  tests/         testes de console das estruturas de dados e do loop de combate (test_*.c)
```

Cada módulo tem um `include/<modulo>.h` e um `src/<modulo>.c`, criados pelo dono na primeira story do módulo. A pasta `dados/` (placar e configuração) é criada pelo jogo no primeiro salvamento e não é versionada.

## Áudio

Os efeitos da AUD-01 ficam em `assets/sounds/`, em OGG Vorbis (os WAV originais somavam ~85 MB; a conversão deixou o repositório em ~8 MB). `iniciarAudio` e `encerrarAudio` entram no ciclo do jogo; arquivo ou dispositivo ausente só gera aviso e a partida segue sem som. Os loops usam `Music` (loop nativo da raylib) e os one-shots usam `Sound`, com volume e pan calculados pela distância e pela direção do TIE.

| Pasta | Conteúdo |
| --- | --- |
| `isd/` | motor (5 variações, uma sorteada na inicialização), pós-combustor, alarme de casco < 25%, `destroyed` e `shield_enable`/`shield_disable` (uso futuro) |
| `rebel/` | motor de cada caça (X-Wing = idle + thrusters), armas (A-Wing tem som próprio; Y-Wing usa mísseis), SPIN e BOOM de cada tipo |
| `tie/` | armas, motor em 5 setores de velocidade, cabine em 2 setores (Noise Scream Low/Med), ACCEL/DECCEL, troca de alvo, dano de tiro/colisão, aviso e ruídos de casco baixo, SPIN e BOOM do jogador; `telemetry_*` fica reservado para uso futuro |

O X-Wing toca idle e thrusters juntos, com o thrusters um pouco mais baixo. O motor dos caças rebeldes só é audível até 1850 u e cresce ao se aproximar; as fontes móveis recebem Doppler (o pitch sobe na aproximação e desce no afastamento) por um registro genérico de vozes — inimigos hoje e aliados no futuro usam a mesma API. O motor do TIE faz crossfade linear entre setores vizinhos (transição suave) e a cabine faz o mesmo em 2 setores, com os dois Noise Scream. O alarme do Destróier entra em loop a 25% do casco; o aviso de casco baixo do TIE toca a cada 0,75 s e os ruídos aleatórios só nessa faixa. Ao zerar o casco do TIE, a cabine gira incontrolavelmente por 1,2-2,6 s (abaixo da duração do áudio de SPIN) e só então explode; o BOOM marca o fim da partida. Nos caças rebeldes, o abatido vira um destroço que continua no mesmo vetor de velocidade e gira por 1,2-2,6 s; no fim aparece a explosão do pool (VIS-10) e toca o BOOM. O SPIN é interrompido no instante em que o BOOM começa, tanto no TIE quanto nos caças.

As armas são disparadas por evento: cada tiro que realmente sai chama o som correspondente, então mudar o cooldown do jogador ou da IA muda o ritmo automaticamente. O `TELEMETRY_RANDOM` está desligado por enquanto; os arquivos continuam em `assets/sounds/tie/telemetry_*.ogg` para reutilização. Para os futuros caças aliados, basta registrar cada nave com `audioMotorRegistrar(AUDIO_MOTOR_ALIADO, id, tipo, pos, vel)` antes de `audioAtualizar`: ela entra no mesmo sistema de vozes, distância, pan e Doppler dos inimigos.

## Modelos 3D

Pipeline: Blender → `.glb` (glTF binário) → `LoadModel` da raylib. Os modelos ficam em `assets/models/`; scripts que geram ou adaptam um modelo, ou dados derivados dele, ficam em `assets/models/fontes/`.

| Item              | Convenção                                                                                                             |
| ----------------- | --------------------------------------------------------------------------------------------------------------------- |
| Unidade           | 1 m do Blender = 1 u do jogo = 1 m canônico (TIE ~6 m, Destróier 1600 m). O modelo já vem no tamanho final: o jogo desenha com escala 1 e sem rotação |
| Eixos no jogo     | Frente +Z, cima +Y, direita -X (os mesmos do TIE em `nave.c`)                                                         |
| Eixos no Blender  | Frente -Y (o modelo olha para a vista Front, tecla 1), cima +Z. O exportador glTF com "+Y Up" leva isso para o jogo    |
| Origem            | Centro da nave; o Destróier fica com a origem na origem do mundo                                                      |
| Escala do mundo   | Com o Destróier em 1600 u, arena (raio 9000), velocidades e distâncias de IA e reforços foram multiplicadas por ~4,6; o tamanho dos caças não mudou |
| Orçamento         | 2–5k triângulos por caça; o Destróier, peça única sempre em cena, até ~60k                                             |
| Exportação        | Formato glTF Binary, "+Y Up", "Apply Modifiers", transformações aplicadas, só os objetos da nave, sem animação         |
| Malhas            | Até 65.535 vértices por malha: a raylib converte índices de 32 bits para 16 e quebra malhas maiores                    |
| Texturas          | Embutidas no `.glb` em PNG (a raylib padrão não lê JPEG), só o primeiro mapa UV. A raylib usa a cor base e a textura de cor |
| Emissivo          | A raylib só lê o emissivo com textura: material emissivo leva cor base = cor do emissivo e uma textura emissiva 1x1, e o jogo o desenha sem luz |
| Colisão           | Destróier: mapa de colunas gerado do modelo (`src/casco_destroier.inc`), com até 4 intervalos sólidos por coluna para deixar vazios os vãos abertos (como o da ponte de comando); mudou o modelo, rodar `python3 assets/models/fontes/casco_destroier.py` |

O `assets/models/destroier.glb` versionado é o modelo detalhado do Imperial-class Star Destroyer (**1,35 mi de triângulos**, ~71 MB), de [Heataker](https://sketchfab.com/Heataker) no Sketchfab, licença **Free Standard**, já ajustado para 1600 u de comprimento (o canônico), proa em +Z e com as texturas/emissivos da tabela. O GLB é versionado via **Git LFS**. O pipeline antigo de `assets/models/fontes/destroier.py` (modelo do Sketchfab com Decimate 235k → 60k) continua no repositório como referência, mas não é o que gera o arquivo atual. Depois, `python3 assets/models/fontes/casco_destroier.py` gera o casco de colisão em grade de 8 u a partir do GLB atual; o script amostra a malha, então modelos muito densos demoram. Sem o `.glb`, o jogo desenha uma cunha de primitivas nas mesmas medidas.

No jogo, o modelo e o mapa de colisão continuam canônicos (1600 u), mas são desenhados e testados na escala `ESCALA_DESTROIER` (`include/destroier.h`, atualmente **4×**, 6400 u). Mudar essa constante muda o tamanho visual e a colisão juntos, sem regenerar o modelo nem o casco.

Modelos de teste: o X-Wing usa `assets/models/xwing.glb` ("Low Poly X-Wing", de felipelunardelli, CC BY 4.0; ajustado para 12,5 m e centrado; **25.873 triângulos**, acima do orçamento de 2–5k por decisão temporária do time). O Y-Wing e o A-Wing usam modelos do Battlefront II (2005) em `assets/models/bf2/` (`ywing.glb`, `awing.glb`), com o trem de pouso removido. Eles estão versionados **temporariamente** para o grupo testar e devem ser removidos antes de uma distribuição pública — são assets de terceiros (LucasArts/EA).

## Estruturas de dados


| Estrutura                  | Uso                                      | Justificativa                                                                   |
| -------------------------- | ---------------------------------------- | ------------------------------------------------------------------------------- |
| Lista duplamente encadeada | Inimigos em combate e computador de mira | Inserção O(1) no fim, remoção O(1) no meio, navegação próximo/anterior          |
| Fila encadeada (FIFO)      | Reforços rebeldes                        | Chegam em ordem cronológica; só o início é consultado                           |
| Árvore binária de busca    | Placar de pilotos                        | Inserção O(log n) no caso médio; percurso em ordem gera o ranking sem reordenar |

## Créditos

- `assets/models/destroier.glb`: ["Imperial-Class Star Destroyer"](https://sketchfab.com/3d-models/star-wars-imperial-class-star-destroyer-fa18d537db4d4020a443c1802ec0f88e), de [Heataker](https://sketchfab.com/Heataker), licença [Free Standard](https://sketchfab.com/licenses). Alterações: escala para 1600 u, proa em +Z, texturas em PNG, materiais emissivos adaptados à raylib e malhas divididas para os índices de 16 bits da raylib. Versionado via Git LFS.
- `assets/models/fontes/destroier.py`: pipeline antigo do ["Star Wars: Imperial II Star Destroyer"](https://sketchfab.com/3d-models/star-wars-imperial-ii-star-destroyer-b8bd2d35f7604670ab85242c06c6d280), de [Daniel](https://sketchfab.com/DanielAndersson), licença [CC BY 4.0](http://creativecommons.org/licenses/by/4.0/). Mantido como referência do modelo reduzido anterior.
- `assets/models/death_star.glb`: adaptado de ["Death Star - Star Wars"](https://sketchfab.com/3d-models/death-star-star-wars-423fb92f677f4448aef407112a1fc032), de [Quiznos323](https://sketchfab.com/quiznos323), licença [CC BY-NC-SA 4.0](http://creativecommons.org/licenses/by-nc-sa/4.0/). Alterações: superlaser e esqueleto removidos, mapas de normal e emissivo removidos, casco com metade dos polígonos e texturas com metade da resolução. A versão adaptada é distribuída sob a mesma licença.
- `assets/models/xwing.glb`: ["Low Poly X-Wing"](https://skfb.ly/6W8Rs), de [felipelunardelli](https://sketchfab.com/felipelunardelli), licença [CC BY 4.0](http://creativecommons.org/licenses/by/4.0/). Alterações: escala para 12,5 m de comprimento, origem no centro e exportação em GLB único (sem decimate).
- `assets/models/bf2/ywing.glb` e `assets/models/bf2/awing.glb`: adaptados do Battlefront II (2005), © LucasArts/EA. Uso **temporário** para os testes do grupo; remover antes de qualquer distribuição pública.
- `assets/sounds/`: efeitos sonoros do projeto (AUD-01), fornecidos pelo grupo.

## Pipeline de modelos - VIS-01 (#55)

Uma unidade equivale a uma unidade de mundo (metro). Frente **+Z**, cima **+Y**, origem no centro. A direita da câmera inicial é **−X**, conforme `nave.c`. Essas convenções seguem o código de voo existente; o alinhamento explícito com a responsável pelo voo ainda precisa ser registrado na issue.

Os modelos ficam em `assets/models/`, em GLB com materiais e texturas embutidos. O orçamento de referência é **2–5 mil triângulos por nave**, com teto de 5 mil. A sonda de teste usa menos triângulos e não representa uma nave final.

`teste_eixos.glb` foi exportado pelo Blender 4.5.3 LTS. Tem corpo de 2 × 2 × 4 unidades, grade UV embutida de 32 × 32 pixels, haste vermelha em +Z e amarela em +Y. Seus limites são X=[−1,1], Y=[−1,3] e Z=[−2,4]. O exportador aplica transformações e converte Z-up do Blender para Y-up do glTF.

```sh
blender --background --factory-startup --python tools/exportar_modelos.py
make preview-modelo
# Teste com saída automática e captura em build/modelo.png:
./build/preview_modelo --capturar
```

A inspeção carrega o GLB com `LoadModel`, verifica suas dimensões e textura embutida e o desenha junto aos eixos do mundo. Esc/Start encerra a janela. O manifesto em `assets/models/manifesto.json` registra a contagem de triângulos e o raio.

Para inspecionar as formas provisórias de X-Wing, Y-Wing e A-Wing, `make preview-inimigos` usa o mesmo desenho da partida, com as três naves e amostras de lasers em movimento — verdes para o TIE do jogador e vermelhos para os rebeldes, conforme VIS-04. Esc encerra a janela; `./build/preview_inimigos --capturar` salva `build/inimigos.png`.
