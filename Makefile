# Makefile para compilar o Boxverse
CC = gcc
CFLAGS = -Wall -Wextra -D_GNU_SOURCE -static
HOST_TARGET = boxverse
GUEST_TARGET = boxverse-init

# Diretórios de instalação
PREFIX ?= /usr/local
BINDIR = $(PREFIX)/bin

SRC_DIR = src
OBJ_DIR = obj

# Fontes do Host (CLI) - Adicionado ipc.c
HOST_SRCS = $(SRC_DIR)/main.c $(SRC_DIR)/init.c $(SRC_DIR)/start.c \
            $(SRC_DIR)/exec.c $(SRC_DIR)/end.c $(SRC_DIR)/destroy.c \
            $(SRC_DIR)/mounts.c $(SRC_DIR)/cgroup.c $(SRC_DIR)/network.c \
            $(SRC_DIR)/util.c $(SRC_DIR)/ipc.c

# Fontes do Guest (Init interno) - Adicionado ipc.c
GUEST_SRCS = $(SRC_DIR)/guest_init.c $(SRC_DIR)/ipc.c

HOST_OBJS = $(patsubst $(SRC_DIR)/%.c, $(OBJ_DIR)/%.o, $(HOST_SRCS))

# Declaração de alvos que não são arquivos
.PHONY: help here install uninstall clean

# --- ALVO PADRÃO (Help) ---
help:
	@echo ""
	@echo "  \033[1;34m Boxverse Build System\033[0m"
	@echo "  -----------------------"
	@echo "  Por favor, escolha uma opção:"
	@echo ""
	@echo "  \033[1;32msudo make here\033[0m       Compila o projeto localmente."
	@echo "  \033[1;32msudo make install\033[0m    Compila e instala em $(BINDIR)."
	@echo "  \033[1;33msudo make uninstall\033[0m  Remove os binários."
	@echo "  \033[1;31msudo make clean\033[0m      Limpa arquivos temporários."
	@echo ""

# --- COMPILAÇÃO LOCAL (Make Here) ---
here: $(HOST_TARGET) $(GUEST_TARGET)
	@echo "\n\033[1;32mCompilação concluída!\033[0m"
	@echo "Binários gerados: './$(HOST_TARGET)' e './$(GUEST_TARGET)'"

# Compila CLI do Host
$(HOST_TARGET): $(HOST_OBJS)
	$(CC) $(CFLAGS) -o $@ $^

# Compila Init do Guest (Estático)
# Nota: Usamos $^ para incluir tanto guest_init.c quanto ipc.c
$(GUEST_TARGET): $(GUEST_SRCS)
	$(CC) $(CFLAGS) -o $@ $^

# Regra genérica para objetos
$(OBJ_DIR)/%.o: $(SRC_DIR)/%.c | $(OBJ_DIR)
	$(CC) $(CFLAGS) -c -o $@ $<

$(OBJ_DIR):
	mkdir -p $(OBJ_DIR)

# --- INSTALAÇÃO ---
install: here
	@echo "Instalando binários em $(BINDIR)..."
	@install -m 755 $(HOST_TARGET) $(BINDIR)/$(HOST_TARGET)
	@install -m 755 $(GUEST_TARGET) $(BINDIR)/$(GUEST_TARGET)
	@echo "Instalação concluída! Pode rodar 'boxverse'."

# --- DESINSTALAÇÃO ---
uninstall:
	@echo "Removendo binários..."
	@rm -f $(BINDIR)/$(HOST_TARGET)
	@rm -f $(BINDIR)/$(GUEST_TARGET)
	@echo "Desinstalado com sucesso."

# --- LIMPEZA ---
clean:
	rm -rf $(OBJ_DIR) $(HOST_TARGET) $(GUEST_TARGET)
	@echo "Limpeza concluída!"