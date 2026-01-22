#include "common.h"
#include <sched.h>
#include <sys/wait.h>
#include <sys/mount.h>

int container_entrypoint(void *arg) {
    (void)arg;

    sethostname("boxverse", 8);

    if (setup_mounts(ROOTFS_DIR) != 0) return 1;

    char *args[] = {"/sbin/boxverse-init", NULL};
    char *env[] = {"PATH=/bin:/sbin:/usr/bin:/usr/sbin", "TERM=xterm", NULL};
    
    execve(args[0], args, env);
    perror("Falha crítica: execve init");
    return 1;
}

int attach_to_container(void) {
    int pid = get_box_pid();
    log_info("Container rodando (PID %d). Anexando shell...", pid);
    
    char pid_str[16];
    sprintf(pid_str, "%d", pid);

    char path[256];
    char *ns_list[] = {"ipc", "uts", "net", "pid", "mnt"};
    
    for(int i=0; i<5; i++) {
        snprintf(path, sizeof(path), "/proc/%s/ns/%s", pid_str, ns_list[i]);
        int fd = open(path, O_RDONLY);
        if (fd == -1) continue;
        if (setns(fd, 0) == -1) perror("setns");
        close(fd);
    }

    if (fork() == 0) {
        execl("/bin/bash", "bash", NULL);
        execl("/bin/sh", "sh", NULL); 
        perror("Falha ao abrir shell");
        exit(1);
    }
    
    wait(NULL);
    return 0;
}

int cmd_start(int argc, char **argv) {
    (void)argc; (void)argv;
    if (get_state() == STATE_RUNNING) {
        if (get_box_pid() > 0) return attach_to_container();
        set_state(STATE_STOPPED);
    }

    Config cfg = load_config();
    int lock_fd = acquire_lock();
    if (lock_fd < 0) return 1;

    log_info("Iniciando container...");

    char *stack = malloc(1024 * 1024);
    if (!stack) return 1;
    char *stack_top = stack + 1024 * 1024;

    int flags = CLONE_NEWNS | CLONE_NEWPID | CLONE_NEWIPC | CLONE_NEWUTS | SIGCHLD;
    if (cfg.net_enabled) flags |= CLONE_NEWNET;

    pid_t child_pid = clone(container_entrypoint, stack_top, flags, NULL);
    if (child_pid == -1) {
        perror("clone");
        free(stack);
        release_lock(lock_fd);
        return 1;
    }

    write_file(PID_FILE, "");
    char pid_str[16];
    sprintf(pid_str, "%d", child_pid);
    write_file(PID_FILE, pid_str);

    if (setup_cgroup(child_pid) != 0) {
        log_error("Falha ao configurar cgroup");
        kill(child_pid, SIGKILL);
        free(stack);
        release_lock(lock_fd);
        return 1;
    }
    if (cfg.net_enabled && setup_network(child_pid) != 0) {
        log_error("Falha ao configurar rede");
        kill(child_pid, SIGKILL);
        free(stack);
        release_lock(lock_fd);
        return 1;
    }

    set_state(STATE_RUNNING);
    release_lock(lock_fd);

    log_info("Container iniciado em background (PID %d).", child_pid);

    usleep(200000);
    return attach_to_container();
}