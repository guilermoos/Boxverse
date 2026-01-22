#include "common.h"
#include <limits.h>
#include <sys/wait.h>

// Helper to execute silent system commands unless they fail
static int run_command(const char *cmd) {
    int status = system(cmd);
    if (WIFEXITED(status) && WEXITSTATUS(status) == 0) {
        return 0;
    }
    return 1;
}

int download_image(const char *url, const char *distro_name, char *out_path, size_t size) {
    snprintf(out_path, size, "%s_image.tar.xz", distro_name);
    
    log_info("Downloading base image for %s...", distro_name);
    log_info("URL: %s", url);

    char cmd[1024];
    // Prioritize wget, fallback to curl
    if (run_command("which wget > /dev/null 2>&1") == 0) {
        snprintf(cmd, sizeof(cmd), "wget -q --show-progress -O %s %s", out_path, url);
    } else if (run_command("which curl > /dev/null 2>&1") == 0) {
        snprintf(cmd, sizeof(cmd), "curl -L -o %s %s", out_path, url);
    } else {
        log_error("Dependency missing: 'wget' or 'curl' required.");
        return 1;
    }

    if (run_command(cmd) != 0) {
        log_error("Download failed. Please check connectivity or URL.");
        unlink(out_path);
        return 1;
    }

    return 0;
}

int extract_rootfs(const char *tarball_path, const char *dest_dir) {
    log_info("Extracting filesystem to %s...", dest_dir);
    
    char cmd[1024];
    // -x: extract, -J: xz compression, -f: file, -C: target directory
    snprintf(cmd, sizeof(cmd), "tar -xJf %s -C %s", tarball_path, dest_dir);
    
    if (run_command(cmd) != 0) {
        log_error("Extraction failed. Archive may be corrupted.");
        return 1;
    }
    
    return 0;
}

char* find_guest_init_binary(void) {
    static char path[PATH_MAX + 64];
    
    // Priority locations
    const char *locations[] = {
        "./boxverse-init",
        "/usr/local/bin/boxverse-init",
        "/usr/lib/boxverse/boxverse-init",
        NULL
    };

    for (int i = 0; locations[i] != NULL; i++) {
        if (access(locations[i], F_OK) == 0) {
            return (char*)locations[i];
        }
    }

    // Try relative to the current executable path
    char self_path[PATH_MAX];
    ssize_t len = readlink("/proc/self/exe", self_path, sizeof(self_path)-1);
    
    if (len != -1) {
        self_path[len] = '\0';
        char *dir = strrchr(self_path, '/');
        if (dir) {
            *dir = '\0';
            
            // Segurança: verifica se cabe no buffer antes de escrever
            size_t required_len = strlen(self_path) + strlen("/boxverse-init") + 1;
            
            if (required_len <= sizeof(path)) {
                snprintf(path, sizeof(path), "%s/boxverse-init", self_path);
                if (access(path, F_OK) == 0) return path;
            }
        }
    }

    return NULL;
}