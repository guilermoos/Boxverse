#include "common.h"

int cmd_exec(int argc, char **argv) {
    if (argc < 3) {
        log_error("Usage: boxverse exec <cmd...>");
        return 1;
    }

    if (get_state() != STATE_RUNNING) {
        log_error("Container is not running.");
        return 1;
    }

    // Construct full command string safely
    char full_cmd[4096] = CMD_EXEC;
    strcat(full_cmd, " "); // Add space separator
    
    size_t current_len = strlen(full_cmd);
    
    for (int i = 2; i < argc; i++) {
        size_t arg_len = strlen(argv[i]);
        if (current_len + arg_len + 1 >= sizeof(full_cmd)) {
            log_error("Command string too long.");
            return 1;
        }
        strcat(full_cmd, argv[i]);
        if (i < argc - 1) strcat(full_cmd, " ");
        current_len += arg_len + 1;
    }

    int fd = ipc_connect_client(SOCK_FILE);
    if (fd < 0) {
        log_error("Failed to connect to container daemon. Is init running?");
        return 1;
    }

    // Send command
    if (write(fd, full_cmd, strlen(full_cmd)) == -1) {
        perror("Socket write error");
        close(fd);
        return 1;
    }

    // Read stream output from container
    char buf[4096];
    ssize_t n;
    while ((n = read(fd, buf, sizeof(buf))) > 0) {
        if (write(STDOUT_FILENO, buf, n) == -1) break;
    }

    close(fd);
    return 0;
}