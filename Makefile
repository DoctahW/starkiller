CC     ?= cc
CFLAGS := -std=c99 -Wall -Wextra -O2 -g -Iinclude

# raylib 6.2 estática compilada com PLATFORM_DESKTOP_SDL (SDL3); RAYLIB_PATH é a pasta raylib/src dela.
ifeq ($(OS),Windows_NT)
    BIN         := starkiller.exe
    RAYLIB_PATH ?= C:/raylib/raylib/src
    SDL3_PATH   ?= C:/SDL3/x86_64-w64-mingw32
    DLL         := SDL3.dll
    LDLIBS      := -L$(RAYLIB_PATH) -lraylib -L$(SDL3_PATH)/lib -lSDL3 -lopengl32 -lgdi32 -lwinmm
else
    BIN         := starkiller
    RAYLIB_PATH ?= $(HOME)/raylib/src
    LDLIBS      := -L$(RAYLIB_PATH) -lraylib $(shell pkg-config --libs sdl3) -lGL -lm -lpthread -ldl -lrt
endif
CFLAGS += -I$(RAYLIB_PATH)

SRC     := $(wildcard src/*.c)
OBJ     := $(SRC:src/%.c=build/%.o)
MODULOS := $(filter-out build/main.o,$(OBJ))
TESTES  := $(patsubst src/tests/%.c,build/tests/%,$(wildcard src/tests/test_*.c))

.PHONY: all run test valgrind clean

all: $(BIN)

$(BIN): $(OBJ) | $(DLL)
	$(CC) $(LDFLAGS) $^ -o $@ $(LDLIBS)

# Windows: o executável e os testes carregam a SDL3.dll da raiz do projeto.
ifdef DLL
$(DLL): $(SDL3_PATH)/bin/$(DLL)
	cp $< $@
endif

build/%.o: src/%.c | build
	$(CC) $(CFLAGS) -MMD -MP -c $< -o $@

build/tests/%: src/tests/%.c $(MODULOS) | build/tests $(DLL)
	$(CC) $(CFLAGS) $< $(MODULOS) -o $@ $(LDLIBS)

build build/tests:
	mkdir -p $@

run: $(BIN)
	./$(BIN)

test: $(TESTES)
	@for t in $(TESTES); do echo "== $$t"; ./$$t || exit 1; done

valgrind: $(TESTES)
	@for t in $(TESTES); do echo "== $$t"; valgrind --leak-check=full --error-exitcode=1 ./$$t || exit 1; done

clean:
	rm -rf build $(BIN) $(DLL)

-include $(OBJ:.o=.d)
