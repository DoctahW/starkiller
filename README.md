<div align="center">

# Starkiller

[Compilar e rodar](#compilar-e-rodar) • [Controles](#controles) • [Estrutura](#estrutura-do-projeto) • [Créditos](#créditos)

![CI](https://github.com/DoctahW/starkiller/actions/workflows/ci.yml/badge.svg)
![C99](https://img.shields.io/badge/C-99-A8B9CC?style=flat-square&logo=c&logoColor=white)
![raylib 6.2](https://img.shields.io/badge/raylib-6.2-black?style=flat-square)
![SDL3](https://img.shields.io/badge/SDL-3-1D4979?style=flat-square)

</div>

---

### Sumário
- [Introdução](#introdução)
- [Recursos](#recursos)
- [Stack](#stack)
- [Compilar e rodar](#compilar-e-rodar)
- [Controles](#controles)
- [Estrutura do projeto](#estrutura-do-projeto)
- [Créditos](#créditos)


# Introdução

Starkiller é um jogo de combate espacial em 3D. Você pilota um caça TIE, com
visão de cabine, e defende um Destróier Estelar contra ondas de X-Wings,
Y-Wings e A-Wings por 5 minutos.

Você vence se o TIE e o Destróier sobreviverem até o fim do tempo, e perde se
um dos dois for destruído.


# Recursos

- **Voo em cabine**: arfagem, guinada, rolagem e controle de velocidade.
- **Combate**: lasers, escudo regenerável, mira adiantada e travamento de alvo.
- **Defesa do Destróier**: 150 pontos de escudo absorvem lasers e bombas antes
  dos 300 pontos de casco. Após 5 segundos sem dano, o escudo regenera 15 pontos
  por segundo; cada acerto reinicia a espera. O casco não regenera. O HUD mostra
  escudo e casco separadamente, com indicação de recarga.
- **Inimigos com IA**: cada caça rebelde tem seu comportamento e chega em ondas
  de reforços cada vez maiores.
- **Passagens dos Y-Wings**: os bombardeiros da mesma onda se reagrupam, aproximam-se
  juntos e lançam até dois torpedos por passagem, com 1,2 segundo entre lançamentos.
  Depois se afastam para preparar outro ataque. A espera pelos companheiros é de
  no máximo 8 segundos após chegar ao ponto de reunião. O dano acontece no impacto.
- **Caças aliados**: seis TIEs começam a partida, três defendendo o Destróier
  (com prioridade para bombardeiros) e três interceptando inimigos no espaço aberto.
  Eles patrulham, atiram e podem ser abatidos. Seus abates não somam pontos ao jogador
  e seus tiros não causam dano amigo. No radar, cruzes verdes indicam defensores e
  cruzes ciano indicam interceptadores; o HUD mostra quantos continuam ativos.
- **HUD**: retícula, medidores, tempo, pontos, radar e alvo travado.
- **Feedback de dano**: tremor de câmera, flash vermelho e vibração do controle.
- **Som 3D**: motores, armas e explosões com volume, pan e Doppler de acordo
  com a posição de cada nave, além de alarmes de casco baixo.
- **Placar**: no fim da partida, você digita seu nome e entra no ranking,
  salvo em `dados/placar.dat`.
- **Teclado e controle**: suporte a controle Xbox, com vibração.


# Stack

- C99
- [raylib](https://www.raylib.com/) 6.2 (3D, áudio e input)
- [SDL3](https://www.libsdl.org/) como backend da raylib (necessária para a vibração do controle)
- `make` para compilar


# Compilar e rodar

O jogo precisa da raylib 6.2 compilada com o backend SDL3. O pacote da raylib
das distros e o instalador oficial usam GLFW, então a raylib é compilada à mão
uma única vez.

### 1. Clonar

Os modelos 3D grandes usam [Git LFS](https://git-lfs.com/). Instale-o antes de
clonar:

```sh
git lfs install
git clone https://github.com/DoctahW/starkiller.git
cd starkiller
```

### 2. Instalar as dependências

<details open>
<summary><b>Linux</b></summary>

1. Instale o compilador, o `make` e a SDL3:

   ```sh
   # Arch
   sudo pacman -S base-devel sdl3

   # Debian 13 / Ubuntu 25.04 ou mais novo
   sudo apt install build-essential libsdl3-dev
   ```

   Se a sua distro não tem a SDL3, compile pelo
   [guia oficial](https://wiki.libsdl.org/SDL3/README-cmake).

2. Baixe e compile a raylib 6.2 em `~/raylib`:

   ```sh
   curl -fsSL https://github.com/raysan5/raylib/archive/refs/tags/6.2.tar.gz | tar xz -C ~
   mv ~/raylib-6.2 ~/raylib
   make -C ~/raylib/src PLATFORM=PLATFORM_DESKTOP_SDL \
     SDL_INCLUDE_PATH="$(pkg-config --variable=includedir sdl3)" \
     SDL_LIBRARY_PATH="$(pkg-config --variable=libdir sdl3)" \
     SDL_LIBRARIES=-lSDL3 CUSTOM_CFLAGS=-DUSING_SDL3_PROJECT
   ```

</details>

<details>
<summary><b>Windows</b></summary>

1. Instale a raylib pelo [instalador oficial](https://www.raylib.com/). Ele
   traz o w64devkit (gcc e make) em `C:\raylib`.
2. Troque a pasta `C:\raylib\raylib` pelo código-fonte da
   [raylib 6.2](https://github.com/raysan5/raylib/archive/refs/tags/6.2.zip).
3. Baixe o `SDL3-devel-<versão>-mingw.zip` nos
   [releases da SDL](https://github.com/libsdl-org/SDL/releases) e extraia
   de forma que exista a pasta `C:\SDL3\x86_64-w64-mingw32`.
4. Abra o terminal do w64devkit (`C:\raylib\w64devkit\w64devkit.exe`) e
   compile a raylib:

   ```sh
   cd C:/raylib/raylib/src
   make PLATFORM=PLATFORM_DESKTOP_SDL \
     SDL_INCLUDE_PATH=C:/SDL3/x86_64-w64-mingw32/include \
     SDL_LIBRARY_PATH=C:/SDL3/x86_64-w64-mingw32/lib \
     SDL_LIBRARIES=-lSDL3 CUSTOM_CFLAGS=-DUSING_SDL3_PROJECT
   ```

Use esse mesmo terminal nos passos seguintes. O `make` copia a `SDL3.dll`
para a pasta do jogo.

</details>

> [!TIP]
> Se a raylib ou a SDL3 estiverem em outro lugar, passe o caminho para o
> `make`: `make RAYLIB_PATH=/caminho/raylib/src` e, no Windows,
> `SDL3_PATH=/caminho/x86_64-w64-mingw32`.

### 3. Compilar e jogar

Na pasta do projeto:

```sh
make run
```

Pronto. Nas próximas vezes, basta rodar `./starkiller` (ou `starkiller.exe`
no Windows).

Outros comandos:

| Comando       | O que faz                         |
| ------------- | --------------------------------- |
| `make`        | Só compila                        |
| `make run`    | Compila e abre o jogo             |
| `make test`   | Roda os testes                    |
| `make clean`  | Apaga os arquivos de build        |


# Controles

| Ação                    | Teclado                  | Controle Xbox            |
| ----------------------- | ------------------------ | ------------------------ |
| Arfagem                 | W / S                    | Analógico esquerdo ↑ / ↓ |
| Guinada                 | A / D                    | Analógico esquerdo ← / → |
| Rolagem                 | Q / E                    | LB / RB                  |
| Acelerar / frear        | Shift / Ctrl             | Analógico direito ↑ / ↓  |
| Disparar                | Espaço                   | RT                       |
| Próximo / anterior alvo | T / Y                    | D-pad → / ←              |
| Alvo na mira            | R                        | LT                       |
| Reparar casco (25)      | F                        | D-pad ↑                  |
| Pausa                   | Esc                      | Start                    |
| Confirmar / voltar      | Enter / Esc ou Backspace | A / B                    |


# Estrutura do projeto

```
starkiller/
├── assets/     # Modelos 3D, sons, shaders, fontes e imagens
├── include/    # Headers de cada módulo
├── src/
│   ├── main.c  # Ponto de entrada
│   ├── jogo.c  # Janela, estados e loop principal
│   ├── ...     # Um arquivo por módulo: nave, combate, ia, hud, telas...
│   └── tests/  # Testes
├── tools/      # Ferramentas de inspeção dos modelos
└── Makefile
```

O jogo cria a pasta `dados/` ao salvar o placar e as configurações.


# Créditos

- Destróier Estelar: ["Imperial-Class Star Destroyer"](https://sketchfab.com/3d-models/star-wars-imperial-class-star-destroyer-fa18d537db4d4020a443c1802ec0f88e), de [Heataker](https://sketchfab.com/Heataker), licença [Free Standard](https://sketchfab.com/licenses).
- Estrela da Morte: adaptada de ["Death Star - Star Wars"](https://sketchfab.com/3d-models/death-star-star-wars-423fb92f677f4448aef407112a1fc032), de [Quiznos323](https://sketchfab.com/quiznos323), licença [CC BY-NC-SA 4.0](http://creativecommons.org/licenses/by-nc-sa/4.0/).
- X-Wing: ["Low Poly X-Wing"](https://skfb.ly/6W8Rs), de [felipelunardelli](https://sketchfab.com/felipelunardelli), licença [CC BY 4.0](http://creativecommons.org/licenses/by/4.0/).
- Y-Wing e A-Wing: adaptados do Battlefront II (2005), © LucasArts/EA.
- Fonte: Chakra Petch, licença [SIL OFL](assets/fonts/ChakraPetch-OFL.txt).
- Efeitos sonoros: fornecidos pelo grupo.
