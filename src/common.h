#ifndef COMMON_H
#define COMMON_H

// Nota: _GNU_SOURCE é definido pelo Makefile (CFLAGS)
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <stdbool.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include <signal.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <stdarg.h>

// Paths and Constants
#define BOX_DIR ".boxverse"
#define ROOTFS_DIR "rootfs"
#define CONFIG_FILE ".boxverse/config.toml"
#define STATE_FILE ".boxverse/state"
#define PID_FILE ".boxverse/pid"
#define LOCK_FILE ".boxverse/lock"
#define SOCK_FILE ".boxverse/control.sock"
#define GUEST_SOCK_PATH "/.boxverse/control.sock"

// Commands
#define CMD_SHUTDOWN "SHUTDOWN"
#define CMD_EXEC "EXEC"

// States
typedef enum {
    STATE_EMPTY,
    STATE_INITIALIZED,
    STATE_RUNNING,
    STATE_STOPPED
} BoxState;

// Configuration
typedef struct {
    char distro[64];
    bool net_enabled;
    bool ssh_enabled;
} Config;

// Command Handlers
// MUDANÇA: cmd_init -> cmd_install e cmd_destroy -> cmd_uninstall
int cmd_install(int argc, char **argv);
int cmd_start(int argc, char **argv);
int cmd_exec(int argc, char **argv);
int cmd_end(int argc, char **argv);
int cmd_uninstall(int argc, char **argv);

// Setup / Infrastructure
int setup_mounts(const char *rootfs);
int setup_cgroup(pid_t pid);
int cleanup_cgroup(void);
int setup_network(pid_t pid);
int cleanup_network(void);

// Image Operations
int download_image(const char *url, const char *distro_name, char *out_path, size_t size);
int extract_rootfs(const char *tarball_path, const char *dest_dir);
char* find_guest_init_binary(void);

// Utils
void log_info(const char *fmt, ...);
void log_error(const char *fmt, ...);
void log_warn(const char *fmt, ...);
int write_file(const char *path, const char *content);
char* read_file(const char *path);
int get_box_pid(void);
void set_state(BoxState state);
BoxState get_state(void);
void save_config(Config *cfg);
Config load_config(void);
int acquire_lock(void);
void release_lock(int fd);

// IPC
int ipc_connect_client(const char *socket_path);
int ipc_create_server(const char *socket_path, int max_clients);

#endif