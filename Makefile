CC     ?= cc
CFLAGS := -std=c99 -Wall -Wextra -O2 -g -Iinclude

ifeq ($(OS),Windows_NT)
    BIN         := starkiller.exe
    RAYLIB_PATH ?= C:/raylib/raylib/src
    CFLAGS      += -I$(RAYLIB_PATH)
    LDLIBS      := -L$(RAYLIB_PATH) -lraylib -lopengl32 -lgdi32 -lwinmm
else
    BIN    := starkiller
    CFLAGS += $(shell pkg-config --cflags raylib 2>/dev/null)
    LDLIBS := $(shell pkg-config --libs raylib 2>/dev/null || echo -lraylib -lGL -lpthread -ldl -lrt -lX11) -lm
endif

SRC     := $(wildcard src/*.c)
OBJ     := $(SRC:src/%.c=build/%.o)
MODULOS := $(filter-out build/main.o,$(OBJ))
TESTES  := $(patsubst src/tests/%.c,build/tests/%,$(wildcard src/tests/test_*.c))

.PHONY: all run test valgrind clean

all: $(BIN)

$(BIN): $(OBJ)
	$(CC) $(LDFLAGS) $^ -o $@ $(LDLIBS)

build/%.o: src/%.c | build
	$(CC) $(CFLAGS) -MMD -MP -c $< -o $@

build/tests/%: src/tests/%.c $(MODULOS) | build/tests
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
	rm -rf build $(BIN)

-include $(OBJ:.o=.d)
