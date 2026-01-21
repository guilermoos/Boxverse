# Makefile para compilar o Boxverse
CC = gcc
CFLAGS = -Wall -Wextra -D_GNU_SOURCE
HOST_TARGET = boxverse
GUEST_TARGET = boxverse-init

# Diretórios de instalação
PREFIX ?= /usr/local
BINDIR = $(PREFIX)/bin

SRC_DIR = src
OBJ_DIR = obj

# Fontes do Host (CLI)
HOST_SRCS = $(SRC_DIR)/main.c $(SRC_DIR)/init.c $(SRC_DIR)/start.c \
            $(SRC_DIR)/exec.c $(SRC_DIR)/end.c $(SRC_DIR)/destroy.c \
            $(SRC_DIR)/mounts.c $(SRC_DIR)/cgroup.c $(SRC_DIR)/network.c \
            $(SRC_DIR)/util.c

# Fontes do Guest (Init interno)
GUEST_SRCS = $(SRC_DIR)/guest_init.c

HOST_OBJS = $(patsubst $(SRC_DIR)/%.c, $(OBJ_DIR)/%.o, $(HOST_SRCS))

all: $(HOST_TARGET) $(GUEST_TARGET)

# Compila CLI do Host
$(HOST_TARGET): $(HOST_OBJS)
	$(CC) $(CFLAGS) -o $@ $^

# Compila Init do Guest (Estático obrigatório)
$(GUEST_TARGET): $(GUEST_SRCS)
	$(CC) $(CFLAGS) -static -o $@ $<

$(OBJ_DIR)/%.o: $(SRC_DIR)/%.c | $(OBJ_DIR)
	$(CC) $(CFLAGS) -c -o $@ $<

$(OBJ_DIR):
	mkdir -p $(OBJ_DIR)

# --- Instalação ---
install: all
	@echo "Instalando binários em $(BINDIR)..."
	@install -m 755 $(HOST_TARGET) $(BINDIR)/$(HOST_TARGET)
	@install -m 755 $(GUEST_TARGET) $(BINDIR)/$(GUEST_TARGET)
	@echo "Instalação concluída! Agora você pode rodar 'boxverse' de qualquer lugar."

uninstall:
	@echo "Removendo binários..."
	@rm -f $(BINDIR)/$(HOST_TARGET)
	@rm -f $(BINDIR)/$(GUEST_TARGET)
	@echo "Desinstalado."

clean:
	rm -rf $(OBJ_DIR) $(HOST_TARGET) $(GUEST_TARGET)
	@echo "Limpeza concluída com sucesso!"