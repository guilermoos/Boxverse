#include "common.h"
#include <limits.h>

// Versão do Alpine para download
#define ALPINE_VERSION "3.21.0"
#define ALPINE_MAJOR "v3.21"
#define ARCH "x86_64" 

// URL base para o Alpine Mini Root Filesystem
// Ex: https://dl-cdn.alpinelinux.org/alpine/v3.21/releases/x86_64/alpine-minirootfs-3.21.0-x86_64.tar.gz

// Função interna para localizar o binário do init (mantida igual)
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

// Configura sources.list para Ubuntu/Debian
static void configure_debian_sources(const char *distro) {
    char path[256];
    snprintf(path, sizeof(path), "%s/etc/apt/sources.list", ROOTFS_DIR);

    FILE *f = fopen(path, "w");
    if (!f) return;

    // Detecta se é Debian ou Ubuntu baseado no nome da distro (simplificado)
    // Se for 'stable', 'bookworm', etc assume Debian. Se 'noble', 'jammy', assume Ubuntu.
    // Como padrão assumiremos estrutura Ubuntu, mas para 'alpine' essa função nem é chamada.
    
    const char *archive_url = "http://archive.ubuntu.com/ubuntu/";
    const char *security_url = "http://security.ubuntu.com/ubuntu/";
    fprintf(f, "deb %s %s main restricted universe multiverse\n", archive_url, distro);
    fprintf(f, "deb %s %s-updates main restricted universe multiverse\n", archive_url, distro);
    fprintf(f, "deb %s %s-security main restricted universe multiverse\n", security_url, distro);

    fclose(f);
}

// --- INSTALAÇÃO DEBIAN/UBUNTU (Via debootstrap) ---
static int install_debian(const char *distro) {
    char cmd[1024];
    log_info("Detectado sistema base Debian/Ubuntu. Usando debootstrap...");
    
    // Verifica se debootstrap existe
    if (system("which debootstrap > /dev/null 2>&1") != 0) {
        log_error("Erro: 'debootstrap' não instalado.");
        log_error("Instale com: sudo apt-get install debootstrap");
        return 1;
    }

    snprintf(cmd, sizeof(cmd), "debootstrap --components=main,universe,multiverse %s %s", distro, ROOTFS_DIR);
    
    if (system(cmd) != 0) {
        log_error("Falha no debootstrap.");
        return 1;
    }

    configure_debian_sources(distro);
    return 0;
}

// --- INSTALAÇÃO ALPINE (Via Tarball) ---
static int install_alpine(void) {
    char url[512];
    char cmd[1024];
    char tarball_path[] = "alpine-rootfs.tar.gz";

    log_info("Detectado Alpine Linux. Baixando Mini Root Filesystem...");

    // Constrói URL
    snprintf(url, sizeof(url), 
             "https://dl-cdn.alpinelinux.org/alpine/%s/releases/%s/alpine-minirootfs-%s-%s.tar.gz",
             ALPINE_MAJOR, ARCH, ALPINE_VERSION, ARCH);

    log_info("URL: %s", url);

    // Tenta baixar com wget ou curl
    if (system("which wget > /dev/null 2>&1") == 0) {
        snprintf(cmd, sizeof(cmd), "wget -q -O %s %s", tarball_path, url);
    } else if (system("which curl > /dev/null 2>&1") == 0) {
        snprintf(cmd, sizeof(cmd), "curl -L -o %s %s", tarball_path, url);
    } else {
        log_error("Erro: Nem 'wget' nem 'curl' encontrados para baixar o rootfs.");
        return 1;
    }

    if (system(cmd) != 0) {
        log_error("Falha ao baixar o Alpine rootfs.");
        return 1;
    }

    log_info("Extraindo sistema de arquivos...");
    snprintf(cmd, sizeof(cmd), "tar -xzf %s -C %s", tarball_path, ROOTFS_DIR);
    
    if (system(cmd) != 0) {
        log_error("Falha ao extrair o tarball.");
        unlink(tarball_path); // Limpa
        return 1;
    }

    unlink(tarball_path); // Remove o arquivo baixado

    // Configuração de DNS inicial (o guest_init reescreve, mas é bom garantir)
    snprintf(cmd, sizeof(cmd), "echo 'nameserver 8.8.8.8' > %s/etc/resolv.conf", ROOTFS_DIR);
    system(cmd);

    return 0;
}

int cmd_init(int argc, char **argv) {
    if (get_state() != STATE_EMPTY) {
        log_error("Boxverse já inicializado neste diretório. (Use 'destroy' primeiro)");
        return 1;
    }

    if (argc < 3) {
        log_error("Uso: boxverse init <distro> [--net] [--ssh]");
        log_error("Exemplos: ");
        log_error("  boxverse init noble    (Ubuntu 24.04)");
        log_error("  boxverse init alpine   (Alpine Linux %s)", ALPINE_MAJOR);
        return 1;
    }

    const char *distro = argv[2];
    Config cfg = {0};
    strncpy(cfg.distro, distro, sizeof(cfg.distro) - 1);

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

    log_info("Inicializando ambiente...");
    
    // Cria diretórios base
    if (mkdir(BOX_DIR, 0755) != 0 && errno != EEXIST) { perror("mkdir .boxverse"); return 1; }
    if (mkdir(ROOTFS_DIR, 0755) != 0 && errno != EEXIST) { perror("mkdir rootfs"); return 1; }

    // SELEÇÃO DA ESTRATÉGIA DE INSTALAÇÃO
    int ret = 0;
    if (strcmp(distro, "alpine") == 0) {
        ret = install_alpine();
    } else {
        ret = install_debian(distro);
    }

    if (ret != 0) {
        // Limpeza em caso de falha
        system("rm -rf .boxverse rootfs");
        return 1;
    }

    // Instalação do Binário do Guest
    log_info("Instalando Init System do Boxverse...");
    
    char cmd[1024];
    // No Alpine, /sbin existe e é o local padrão.
    // No Debian/Ubuntu moderno, /sbin é link para /usr/sbin ou existe.
    snprintf(cmd, sizeof(cmd), "cp %s %s/sbin/boxverse-init", guest_bin, ROOTFS_DIR);
    
    if (system(cmd) != 0) {
        // Fallback: Tenta criar pasta se não existir (algumas distros minimalistas podem variar)
        snprintf(cmd, sizeof(cmd), "mkdir -p %s/sbin", ROOTFS_DIR);
        system(cmd);
        snprintf(cmd, sizeof(cmd), "cp %s %s/sbin/boxverse-init", guest_bin, ROOTFS_DIR);
        
        if (system(cmd) != 0) {
            log_error("Falha ao copiar boxverse-init para rootfs.");
            return 1;
        }
    }
    
    snprintf(cmd, sizeof(cmd), "chmod +x %s/sbin/boxverse-init", ROOTFS_DIR);
    system(cmd);

    save_config(&cfg);
    set_state(STATE_INITIALIZED);

    log_info("Instalação concluída!");
    log_info("Distro: %s", distro);
    if (strcmp(distro, "alpine") == 0) {
        log_info("Dica: No Alpine, use 'apk add' em vez de 'apt-get'.");
    }
    return 0;
}