# Compilacion de los componentes en GNU C del Proyecto 2.
#
#   make              compila todo en bin/
#   make interceptor  compila solo el DNS Interceptor
#   make zonal_cache  compila solo el Zonal Cache
#   make debug        compila con sanitizers (ASan + UBSan)
#   make run-apache   levanta el servidor Apache de origen en Docker
#   make clean        borra los binarios y objetos

CC      := gcc
CFLAGS  := -std=gnu11 -Wall -Wextra -g -Icomun
LDFLAGS :=

BIN     := bin
OBJ     := build

# --- modulos compartidos entre los dos ejecutables ---
COMUN_SRC := $(wildcard comun/*.c)
COMUN_OBJ := $(COMUN_SRC:%.c=$(OBJ)/%.o)

# --- ejecutables ---
INT_SRC := $(wildcard interceptor/*.c)
INT_OBJ := $(INT_SRC:%.c=$(OBJ)/%.o)

ZC_SRC  := $(wildcard zonal_cache/*.c)
ZC_OBJ  := $(ZC_SRC:%.c=$(OBJ)/%.o)

PUERTO_APACHE ?= 8081

.PHONY: all interceptor zonal_cache debug clean run-apache stop-apache

all: $(BIN)/interceptor $(BIN)/zonal_cache

interceptor: $(BIN)/interceptor
zonal_cache: $(BIN)/zonal_cache

$(BIN)/interceptor: $(INT_OBJ) $(COMUN_OBJ) | $(BIN)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

$(BIN)/zonal_cache: $(ZC_OBJ) $(COMUN_OBJ) | $(BIN)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

# Regla generica: cada .c produce su .o dentro de build/, conservando la ruta
$(OBJ)/%.o: %.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

$(BIN):
	@mkdir -p $(BIN)

# Recompila todo con deteccion de errores de memoria en tiempo de ejecucion
debug: CFLAGS  += -fsanitize=address,undefined
debug: LDFLAGS += -fsanitize=address,undefined
debug: clean all

# --- servidor de origen para las pruebas del Zonal Cache ---
run-apache:
	docker run -d --rm --name apache-origen -p $(PUERTO_APACHE):80 httpd:2.4
	@echo "Apache escuchando en http://127.0.0.1:$(PUERTO_APACHE)/"

stop-apache:
	-docker stop apache-origen

clean:
	rm -rf $(BIN) $(OBJ)
