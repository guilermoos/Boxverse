#include "common.h"
#include <sched.h>
#include <sys/wait.h>
#include <sys/mount.h>

int container_entrypoint(void *arg) {
    (void)arg;

    // Apply isolation
    sethostname("boxverse-dev", 12);
    
    if (setup_mounts(ROOTFS_DIR) != 0) return 1;

    // Execute PID 1
    char *args[] = {"/sbin/boxverse-init", NULL};
    char *env[] = {"PATH=/bin:/sbin:/usr/bin:/usr/sbin", "TERM=xterm", NULL};
    
    execve(args[0], args, env);
    perror("CRITICAL: Failed to exec init system");
    return 1;
}

int attach_to_container(void) {
    int pid = get_box_pid();
    if (pid <= 0) {
        log_error("Invalid PID.");
        return 1;
    }

    log_info("Container active (PID %d). Attaching shell...", pid);
    
    char path[256];
    char *ns_list[] = {"ipc", "uts", "net", "pid", "mnt"};
    
    // Join all namespaces
    for(int i = 0; i < 5; i++) {
        snprintf(path, sizeof(path), "/proc/%d/ns/%s", pid, ns_list[i]);
        int fd = open(path, O_RDONLY);
        if (fd == -1) {
            // "net" might not exist if network was disabled
            continue; 
        }
        if (setns(fd, 0) == -1) {
            perror("setns");
            log_warn("Failed to join namespace: %s", ns_list[i]);
        }
        close(fd);
    }

    // Fork before Exec to properly attach to the new PID namespace
    pid_t child = fork();
    if (child == 0) {
        setenv("PS1", "\\u@\\h:\\w\\$ ", 1);
        execl("/bin/bash", "bash", NULL);
        execl("/bin/sh", "sh", NULL); 
        perror("Failed to spawn shell");
        exit(1);
    } else {
        int status;
        waitpid(child, &status, 0);
    }
    
    return 0;
}

int cmd_start(int argc, char **argv) {
    (void)argc; (void)argv;
    
    if (get_state() == STATE_RUNNING) {
        int pid = get_box_pid();
        if (pid > 0 && kill(pid, 0) == 0) {
            return attach_to_container();
        }
        // If state says running but PID is gone:
        log_warn("State corruption detected (stale PID). Resetting.");
        cleanup_network();
        set_state(STATE_STOPPED);
    }

    Config cfg = load_config();
    int lock_fd = acquire_lock();
    if (lock_fd < 0) return 1;

    log_info("Starting container infrastructure...");

    // Stack for the cloned process
    char *stack = malloc(1024 * 1024);
    if (!stack) {
        log_error("Memory allocation failed.");
        release_lock(lock_fd);
        return 1;
    }
    char *stack_top = stack + 1024 * 1024;

    // Namespaces configuration
    int flags = CLONE_NEWNS | CLONE_NEWPID | CLONE_NEWIPC | CLONE_NEWUTS | SIGCHLD;
    if (cfg.net_enabled) flags |= CLONE_NEWNET;

    pid_t child_pid = clone(container_entrypoint, stack_top, flags, NULL);
    if (child_pid == -1) {
        perror("clone failed");
        free(stack);
        release_lock(lock_fd);
        return 1;
    }

    // Record PID
    char pid_str[16];
    sprintf(pid_str, "%d", child_pid);
    write_file(PID_FILE, pid_str);

    // Apply Limits
    if (setup_cgroup(child_pid) != 0) {
        log_error("Cgroup setup failed. Aborting.");
        kill(child_pid, SIGKILL);
        free(stack);
        release_lock(lock_fd);
        return 1;
    }

    // Configure Network from Host Side
    if (cfg.net_enabled) {
        if (setup_network(child_pid) != 0) {
            log_error("Network setup failed. Aborting.");
            kill(child_pid, SIGKILL);
            free(stack);
            release_lock(lock_fd);
            return 1;
        }
    }

    set_state(STATE_RUNNING);
    release_lock(lock_fd);

    log_info("Container started in background (PID %d).", child_pid);

    // Wait slightly to allow init to boot before attaching
    usleep(200000);
    return attach_to_container();
}