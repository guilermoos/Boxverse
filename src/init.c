#include "common.h"
#include <limits.h>

// Função interna para localizar o binário do init
static char* find_guest_init() {
    static char path[PATH_MAX];
    
    // Tentativa 1: Pasta local (desenvolvimento)
    if (access("./boxverse-init", F_OK) == 0) {
        return "./boxverse-init";
    }
    
    // Tentativa 2: Pasta de instalação padrão do sistema
    if (access("/usr/local/bin/boxverse-init", F_OK) == 0) {
        return "/usr/local/bin/boxverse-init";
    }

    // Tentativa 3: Mesma pasta do executável boxverse atual
    char self_path[PATH_MAX];
    ssize_t len = readlink("/proc/self/exe", self_path, sizeof(self_path)-1);
    
    if (len != -1) {
        self_path[len] = '\0';
        char *dir = strrchr(self_path, '/');
        if (dir) {
            *dir = '\0'; // Remove o nome do executável, mantém o diretório
            
            // CORREÇÃO DEFINITIVA DO WARNING:
            // Trocamos snprintf por strcpy/strcat dentro de um check explícito.
            // Isso satisfaz o compilador pois remove a heurística de formatação.
            size_t required_len = strlen(self_path) + strlen("/boxverse-init") + 1;
            
            if (required_len <= sizeof(path)) {
                strcpy(path, self_path);
                strcat(path, "/boxverse-init");
                
                if (access(path, F_OK) == 0) return path;
            }
        }
    }

    return NULL;
}

int cmd_init(int argc, char **argv) {
    if (get_state() != STATE_EMPTY) {
        log_error("Boxverse já inicializado neste diretório.");
        return 1;
    }

    if (argc < 3) {
        log_error("Uso: boxverse init <distro> [--net] [--ssh]");
        return 1;
    }

    const char *distro = argv[2];
    Config cfg = {0};
    strncpy(cfg.distro, distro, sizeof(cfg.distro) - 1);

    for (int i = 3; i < argc; i++) {
        if (strcmp(argv[i], "--net") == 0) cfg.net_enabled = true;
        if (strcmp(argv[i], "--ssh") == 0) cfg.ssh_enabled = true;
    }

    // Busca o binário
    char *guest_bin = find_guest_init();
    if (!guest_bin) {
        log_error("CRÍTICO: Binário 'boxverse-init' não encontrado no sistema.");
        log_error("Dica: Rode 'sudo make install' para instalar os binários corretamente.");
        return 1;
    }

    log_info("Localizado binário do guest em: %s", guest_bin);
    log_info("Inicializando boxverse para distro '%s'...", distro);

    mkdir(BOX_DIR, 0755);
    mkdir(ROOTFS_DIR, 0755);

    char cmd[1024];
    snprintf(cmd, sizeof(cmd), "debootstrap %s %s", distro, ROOTFS_DIR);
    log_info("Executando debootstrap (isso pode demorar)...");
    
    if (system(cmd) != 0) {
        log_error("Falha no debootstrap. Verifique sua conexão e se o debootstrap está instalado.");
        system("rm -rf .boxverse rootfs");
        return 1;
    }

    // Copia o binário encontrado para dentro do rootfs
    snprintf(cmd, sizeof(cmd), "cp %s %s/sbin/boxverse-init", guest_bin, ROOTFS_DIR);
    if (system(cmd) != 0) {
        log_error("Falha ao copiar o binário para dentro do container.");
        return 1;
    }
    
    snprintf(cmd, sizeof(cmd), "chmod +x %s/sbin/boxverse-init", ROOTFS_DIR);
    system(cmd);

    save_config(&cfg);
    set_state(STATE_INITIALIZED);

    log_info("Instalação concluída com sucesso.");
    log_info("Use 'sudo boxverse start' para entrar no container.");
    return 0;
}