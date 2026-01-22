#include "common.h"
#include <sys/wait.h>

int cmd_end(int argc, char **argv) {
    (void)argc; (void)argv;

    if (get_state() != STATE_RUNNING) {
        log_error("Container is not running.");
        return 0;
    }

    log_info("Sending shutdown signal...");

    // 1. Try IPC Soft Shutdown
    int fd = ipc_connect_client(SOCK_FILE);
    if (fd >= 0) {
        write(fd, CMD_SHUTDOWN, strlen(CMD_SHUTDOWN));
        char buf[16];
        read(fd, buf, sizeof(buf)); // Wait for ACK
        close(fd);
    } else {
        // 2. Fallback: SIGTERM to Init
        int pid = get_box_pid();
        if (pid > 0) {
            log_warn("Socket unreachable. Using SIGTERM on PID %d...", pid);
            kill(pid, SIGTERM);
        }
    }

    // 3. Wait for process exit
    int pid = get_box_pid();
    if (pid > 0) {
        int status;
        int i = 0;
        // Poll for 5 seconds
        while (i < 50 && kill(pid, 0) == 0) {
            usleep(100000);
            waitpid(pid, &status, WNOHANG);
            i++;
        }
        
        // Force kill if stuck
        if (kill(pid, 0) == 0) {
            log_warn("Container stuck. Forcing SIGKILL...");
            kill(pid, SIGKILL);
            waitpid(pid, &status, 0);
        }
    }

    cleanup_cgroup();
    cleanup_network();
    set_state(STATE_STOPPED);
    log_info("Container stopped.");
    return 0;
}