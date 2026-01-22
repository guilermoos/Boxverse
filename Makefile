# Makefile for Boxverse
CC = gcc
# Flags:
# -D_GNU_SOURCE: Necessário para funcionalidades avançadas do Linux (namespaces, etc)
# -static: Gera binários portáteis sem dependências de bibliotecas dinâmicas
CFLAGS = -Wall -Wextra -D_GNU_SOURCE -static

# Targets Names
HOST_TARGET = boxverse
GUEST_TARGET = boxverse-init

# Installation Directories
PREFIX ?= /usr/local
BINDIR = $(PREFIX)/bin

# Directories
SRC_DIR = src
OBJ_DIR = obj

# --- SOURCES CONFIGURATION ---

# Host Sources (CLI Tool)
# UPDATED: init.c -> install.c | destroy.c -> uninstall.c
HOST_SRCS = $(SRC_DIR)/main.c \
            $(SRC_DIR)/install.c \
            $(SRC_DIR)/uninstall.c \
            $(SRC_DIR)/start.c \
            $(SRC_DIR)/exec.c \
            $(SRC_DIR)/end.c \
            $(SRC_DIR)/mounts.c \
            $(SRC_DIR)/cgroup.c \
            $(SRC_DIR)/network.c \
            $(SRC_DIR)/util.c \
            $(SRC_DIR)/ipc.c \
            $(SRC_DIR)/image.c

# Guest Sources (Internal PID 1)
# Included ipc.c for communication with the host
GUEST_SRCS = $(SRC_DIR)/guest_init.c \
             $(SRC_DIR)/ipc.c

# Generate Object paths for Host
HOST_OBJS = $(patsubst $(SRC_DIR)/%.c, $(OBJ_DIR)/%.o, $(HOST_SRCS))

# Phony targets
.PHONY: help all here install uninstall clean

# --- DEFAULT TARGET (Help) ---
help:
	@echo ""
	@echo "  \033[1;34m Boxverse Build System\033[0m"
	@echo "  -----------------------"
	@echo "  Usage:"
	@echo "  \033[1;32msudo make here\033[0m       Compile artifacts locally."
	@echo "  \033[1;32msudo make install\033[0m    Compile and install binaries to $(BINDIR)."
	@echo "  \033[1;33msudo make uninstall\033[0m  Remove installed binaries from system."
	@echo "  \033[1;31msudo make clean\033[0m      Remove build artifacts (obj/)."
	@echo ""

# --- LOCAL COMPILATION ---
here: $(HOST_TARGET) $(GUEST_TARGET)
	@echo "\n\033[1;32mBuild complete successfully!\033[0m"
	@echo "Artifacts generated: './$(HOST_TARGET)' and './$(GUEST_TARGET)'"

# Host CLI Compilation
$(HOST_TARGET): $(HOST_OBJS)
	@echo "Linking $(HOST_TARGET)..."
	$(CC) $(CFLAGS) -o $@ $^

# Guest Init Compilation
# Uses source files directly to simplify object management for the small static binary
$(GUEST_TARGET): $(GUEST_SRCS)
	@echo "Compiling $(GUEST_TARGET)..."
	$(CC) $(CFLAGS) -o $@ $^

# Pattern Rule for Object Files
$(OBJ_DIR)/%.o: $(SRC_DIR)/%.c | $(OBJ_DIR)
	@echo "Compiling $<..."
	$(CC) $(CFLAGS) -c -o $@ $<

$(OBJ_DIR):
	mkdir -p $(OBJ_DIR)

# --- SYSTEM INSTALLATION ---
# Note: This installs the binaries, not the container environment.
install: here
	@echo "Installing binaries to $(BINDIR)..."
	@mkdir -p $(BINDIR)
	@install -m 755 $(HOST_TARGET) $(BINDIR)/$(HOST_TARGET)
	@install -m 755 $(GUEST_TARGET) $(BINDIR)/$(GUEST_TARGET)
	@echo "Installation complete. You can now run 'boxverse'."

# --- SYSTEM UNINSTALLATION ---
# Note: This removes the binaries, not the .boxverse data.
uninstall:
	@echo "Removing binaries..."
	@rm -f $(BINDIR)/$(HOST_TARGET)
	@rm -f $(BINDIR)/$(GUEST_TARGET)
	@echo "Uninstallation successful."

# --- CLEANUP ---
clean:
	@echo "Cleaning up build artifacts..."
	rm -rf $(OBJ_DIR) $(HOST_TARGET) $(GUEST_TARGET)
	@echo "Done."