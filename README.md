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
assets/          models/, textures/, sounds/
include/         headers públicos de cada módulo
  tipos.h        contrato compartilhado entre os módulos
  teste.h        verificações dos testes de console
src/
  main.c         ponto de entrada
  jogo.c         janela, máquina de estados e loop de combate
  entrada.c      teclado e controle -> Comandos
  nave.c         voo e câmera de cabine
  combate.c      lasers, colisões, dano e explosões
  inimigos.c     lista duplamente encadeada e computador de mira
  reforcos.c     fila de reforços
  ia.c           comportamento dos caças rebeldes
  render.c       cena 3D
  hud.c          HUD da cabine
  placar.c       árvore binária de busca do placar
  pontuacao.c    pontuação
  telas.c        menu, fim de partida, ranking e controles
  audio.c        efeitos e música
  tests/         testes de console das estruturas de dados (test_*.c)
```

Cada módulo tem um `include/<modulo>.h` e um `src/<modulo>.c`, criados pelo dono na primeira story do módulo. A pasta `dados/` (placar e configuração) é criada pelo jogo no primeiro salvamento e não é versionada.

## Estruturas de dados


| Estrutura                  | Uso                                      | Justificativa                                                                   |
| -------------------------- | ---------------------------------------- | ------------------------------------------------------------------------------- |
| Lista duplamente encadeada | Inimigos em combate e computador de mira | Inserção O(1) no fim, remoção O(1) no meio, navegação próximo/anterior          |
| Fila encadeada (FIFO)      | Reforços rebeldes                        | Chegam em ordem cronológica; só o início é consultado                           |
| Árvore binária de busca    | Placar de pilotos                        | Inserção O(log n) no caso médio; percurso em ordem gera o ranking sem reordenar |
