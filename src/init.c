#include "common.h"
#include <limits.h>

// URLs das imagens hospedadas no GitHub Releases
#define URL_ALPINE "https://github.com/guilermoos/Boxverse/releases/download/images/alpine.tar.xz"
#define URL_UBUNTU_NOBLE "https://github.com/guilermoos/Boxverse/releases/download/images/ubuntu-noble.tar.xz"

// Função interna para localizar o binário do init
static char* find_guest_init() {
    static char path[PATH_MAX];
    
    if (access("./boxverse-init", F_OK) == 0) return "./boxverse-init";
    if (access("/usr/local/bin/boxverse-init", F_OK) == 0) return "/usr/local/bin/boxverse-init";

    char self_path[PATH_MAX];
    ssize_t len = readlink("/proc/self/exe", self_path, sizeof(self_path)-1);
    
    if (len != -1) {
        self_path[len] = '\0';
        char *dir = strrchr(self_path, '/');
        if (dir) {
            *dir = '\0';
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

// Função genérica para baixar, extrair e limpar a imagem
static int download_and_extract(const char *url, const char *distro_name) {
    char cmd[1024];
    char tarball_path[256];
    
    // Define nome temporário do arquivo
    snprintf(tarball_path, sizeof(tarball_path), "%s_image.tar.xz", distro_name);

    log_info("Baixando imagem base para %s...", distro_name);
    log_info("URL: %s", url);

    // 1. Download (Tenta wget, fallback para curl)
    if (system("which wget > /dev/null 2>&1") == 0) {
        snprintf(cmd, sizeof(cmd), "wget -q --show-progress -O %s %s", tarball_path, url);
    } else if (system("which curl > /dev/null 2>&1") == 0) {
        snprintf(cmd, sizeof(cmd), "curl -L -o %s %s", tarball_path, url);
    } else {
        log_error("Erro: É necessário ter 'wget' ou 'curl' instalado para baixar a imagem.");
        return 1;
    }

    if (system(cmd) != 0) {
        log_error("Falha no download. Verifique sua conexão ou a URL.");
        unlink(tarball_path);
        return 1;
    }

    // 2. Extração
    log_info("Extraindo sistema de arquivos...");
    // A flag 'J' é para .xz
    snprintf(cmd, sizeof(cmd), "tar -xJf %s -C %s", tarball_path, ROOTFS_DIR);
    
    if (system(cmd) != 0) {
        log_error("Falha ao extrair o arquivo. O download pode estar corrompido.");
        unlink(tarball_path); // Limpa o arquivo corrompido
        return 1;
    }

    // 3. Limpeza (Deleta o arquivo baixado)
    log_info("Limpando arquivos temporários...");
    if (unlink(tarball_path) != 0) {
        log_error("Aviso: Não foi possível deletar o arquivo temporário %s", tarball_path);
    }

    return 0;
}

int cmd_init(int argc, char **argv) {
    if (get_state() != STATE_EMPTY) {
        log_error("Boxverse já inicializado neste diretório. (Use 'destroy' primeiro)");
        return 1;
    }

    if (argc < 3) {
        log_error("Uso: boxverse init <distro> [--net] [--ssh]");
        log_error("Distros disponíveis:");
        log_error("  alpine   (Alpine Linux - Super Leve)");
        log_error("  noble    (Ubuntu 24.04 LTS)");
        return 1;
    }

    const char *distro = argv[2];
    Config cfg = {0};
    strncpy(cfg.distro, distro, sizeof(cfg.distro) - 1);

    // Parse flags
    for (int i = 3; i < argc; i++) {
        if (strcmp(argv[i], "--net") == 0) cfg.net_enabled = true;
        if (strcmp(argv[i], "--ssh") == 0) cfg.ssh_enabled = true;
    }

    char *guest_bin = find_guest_init();
    if (!guest_bin) {
        log_error("CRÍTICO: Binário 'boxverse-init' não encontrado.");
        log_error("Rode 'sudo make install' ou verifique o diretório local.");
        return 1;
    }

    log_info("Inicializando ambiente Boxverse...");
    
    // Cria diretórios base
    if (mkdir(BOX_DIR, 0755) != 0 && errno != EEXIST) { perror("mkdir .boxverse"); return 1; }
    if (mkdir(ROOTFS_DIR, 0755) != 0 && errno != EEXIST) { perror("mkdir rootfs"); return 1; }

    // Roteamento de Distros (Agora todas usam download)
    int ret = 1;
    if (strcmp(distro, "alpine") == 0) {
        ret = download_and_extract(URL_ALPINE, "alpine");
    } 
    else if (strcmp(distro, "noble") == 0) {
        ret = download_and_extract(URL_UBUNTU_NOBLE, "noble");
    } 
    else {
        log_error("Distro '%s' desconhecida.", distro);
        log_error("Use 'alpine' ou 'noble'.");
        system("rm -rf .boxverse rootfs");
        return 1;
    }

    if (ret != 0) {
        // Se falhou, limpa as pastas criadas
        system("rm -rf .boxverse rootfs");
        return 1;
    }

    // Instalação do Binário do Guest (Init System)
    log_info("Instalando Init System do Boxverse...");
    
    char cmd[1024];
    // Garante que a pasta sbin exista (Alpine e Ubuntu usam estruturas diferentes as vezes)
    snprintf(cmd, sizeof(cmd), "mkdir -p %s/sbin", ROOTFS_DIR);
    system(cmd);

    snprintf(cmd, sizeof(cmd), "cp %s %s/sbin/boxverse-init", guest_bin, ROOTFS_DIR);
    if (system(cmd) != 0) {
        log_error("Falha ao copiar boxverse-init para rootfs.");
        return 1;
    }
    
    snprintf(cmd, sizeof(cmd), "chmod +x %s/sbin/boxverse-init", ROOTFS_DIR);
    system(cmd);

    // Configuração básica de DNS para garantir conectividade imediata
    snprintf(cmd, sizeof(cmd), "echo 'nameserver 8.8.8.8' > %s/etc/resolv.conf", ROOTFS_DIR);
    system(cmd);

    save_config(&cfg);
    set_state(STATE_INITIALIZED);

    log_info("Instalação concluída com sucesso!");
    log_info("Distro: %s", distro);
    
    if (strcmp(distro, "alpine") == 0) {
        log_info("Dica: No Alpine use 'apk' para gerenciar pacotes.");
    } else {
        log_info("Dica: No Ubuntu use 'apt' para gerenciar pacotes.");
    }
    
    return 0;
}