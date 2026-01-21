#ifndef BOXVERSE_H
#define BOXVERSE_H

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <fcntl.h>
#include <errno.h>
#include <stdbool.h>

// Caminhos e Constantes
#define BOX_DIR ".boxverse"
#define ROOTFS_DIR "rootfs"
#define CONFIG_FILE ".boxverse/config.toml"
#define PID_FILE ".boxverse/pid"
#define SOCK_FILE ".boxverse/control.sock"
#define GUEST_SOCK_DIR "/run/boxverse"
#define GUEST_SOCK_PATH "/run/boxverse/control.sock"

// Comandos do Protocolo Interno (Socket)
#define CMD_EXEC "EXEC"
#define CMD_SHUTDOWN "SHUTDOWN"

// Estrutura de Configuração
typedef struct {
    char distro[32];
    bool net_enabled;
    bool ssh_enabled; // Apenas flag de intenção
} Config;

// Protótipos dos Comandos
int cmd_init(int argc, char **argv);
int cmd_start(int argc, char **argv);
int cmd_exec(int argc, char **argv);
int cmd_end(int argc, char **argv);
int cmd_destroy(int argc, char **argv);

// Helpers de Infraestrutura
int setup_namespaces(bool net_enabled);
int setup_cgroup(void);
int setup_mounts(const char *rootfs_path);
int setup_network_host(pid_t pid);
int cleanup_cgroup(void);

// Utils
void log_info(const char *fmt, ...);
void log_err(const char *fmt, ...);
int write_file(const char *path, const char *content);
int read_pid_file(const char *path);
void save_config(const Config *cfg);
Config load_config(void);

#endif