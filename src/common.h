#ifndef COMMON_H
#define COMMON_H

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

#define BOX_DIR ".boxverse"
#define ROOTFS_DIR "rootfs"
#define CONFIG_FILE ".boxverse/config.toml"
#define STATE_FILE ".boxverse/state"
#define PID_FILE ".boxverse/pid"
#define LOCK_FILE ".boxverse/lock"
#define SOCK_FILE ".boxverse/control.sock"
#define GUEST_SOCK_PATH "/.boxverse/control.sock"

typedef enum {
    STATE_EMPTY,
    STATE_INITIALIZED,
    STATE_RUNNING,
    STATE_STOPPED
} BoxState;

typedef struct {
    char distro[64];
    bool net_enabled;
    bool ssh_enabled;
    bool gui_enabled;
} Config;

#define CMD_SHUTDOWN "SHUTDOWN"
#define CMD_EXEC "EXEC"

int cmd_init(int argc, char **argv);
int cmd_start(int argc, char **argv);
int cmd_exec(int argc, char **argv);
int cmd_end(int argc, char **argv);
int cmd_destroy(int argc, char **argv);

int setup_mounts(const char *rootfs);
int setup_cgroup(pid_t pid);
int cleanup_cgroup(void);
int setup_network(pid_t pid);

void log_info(const char *fmt, ...);
void log_error(const char *fmt, ...);
int write_file(const char *path, const char *content);
char* read_file(const char *path);
int get_box_pid(void);
void set_state(BoxState state);
BoxState get_state(void);
void save_config(Config *cfg);
Config load_config(void);
int acquire_lock(void);
void release_lock(int fd);

#endif