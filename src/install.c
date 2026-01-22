#include "common.h"

// Release URLs
#define URL_ALPINE "https://github.com/guilermoos/Boxverse/releases/download/images/alpine.tar.xz"
#define URL_UBUNTU_NOBLE "https://github.com/guilermoos/Boxverse/releases/download/images/ubuntu-noble.tar.xz"

int cmd_install(int argc, char **argv) {
    if (get_state() != STATE_EMPTY) {
        log_error("Environment already initialized here. Run 'uninstall' first.");
        return 1;
    }

    if (argc < 3) {
        log_error("Usage: boxverse install <distro> [--net] [--ssh]");
        log_error("Available distros: 'alpine', 'noble'");
        return 1;
    }

    const char *distro = argv[2];
    Config cfg = {0};
    strncpy(cfg.distro, distro, sizeof(cfg.distro) - 1);

    // Flag parsing
    for (int i = 3; i < argc; i++) {
        if (strcmp(argv[i], "--net") == 0) cfg.net_enabled = true;
        if (strcmp(argv[i], "--ssh") == 0) cfg.ssh_enabled = true;
    }

    // Resource check
    char *guest_bin = find_guest_init_binary();
    if (!guest_bin) {
        log_error("CRITICAL: 'boxverse-init' binary not found.");
        log_error("Please install the project correctly or build 'boxverse-init'.");
        return 1;
    }

    log_info("Installing Boxverse environment...");
    
    if (mkdir(BOX_DIR, 0755) != 0 && errno != EEXIST) { perror("mkdir .boxverse"); return 1; }
    if (mkdir(ROOTFS_DIR, 0755) != 0 && errno != EEXIST) { perror("mkdir rootfs"); return 1; }

    // Distro Routing
    const char *url = NULL;
    if (strcmp(distro, "alpine") == 0) url = URL_ALPINE;
    else if (strcmp(distro, "noble") == 0) url = URL_UBUNTU_NOBLE;
    else {
        log_error("Unknown distribution: '%s'", distro);
        system("rm -rf .boxverse rootfs");
        return 1;
    }

    // Download & Extract (Image Module)
    char tar_path[256];
    if (download_image(url, distro, tar_path, sizeof(tar_path)) != 0) {
        system("rm -rf .boxverse rootfs");
        return 1;
    }

    int extract_ret = extract_rootfs(tar_path, ROOTFS_DIR);
    unlink(tar_path); // Cleanup artifact

    if (extract_ret != 0) {
        system("rm -rf .boxverse rootfs");
        return 1;
    }

    // Guest Init System Installation
    log_info("Installing Guest Init System...");
    char cmd[1024];
    
    snprintf(cmd, sizeof(cmd), "mkdir -p %s/sbin", ROOTFS_DIR);
    system(cmd);

    snprintf(cmd, sizeof(cmd), "cp %s %s/sbin/boxverse-init", guest_bin, ROOTFS_DIR);
    if (system(cmd) != 0) {
        log_error("Failed to copy boxverse-init to rootfs.");
        return 1;
    }
    
    snprintf(cmd, sizeof(cmd), "chmod +x %s/sbin/boxverse-init", ROOTFS_DIR);
    system(cmd);

    // Essential configuration
    snprintf(cmd, sizeof(cmd), "echo 'nameserver 8.8.8.8' > %s/etc/resolv.conf", ROOTFS_DIR);
    system(cmd);

    save_config(&cfg);
    set_state(STATE_INITIALIZED);

    log_info("Installation complete! Distro: %s", distro);
    if (cfg.net_enabled) log_info("Network: Enabled (veth/bridge)");
    return 0;
}