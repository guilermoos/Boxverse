#include "common.h"
#include <sys/wait.h>

int cmd_end(int argc, char **argv) {
    (void)argc; (void)argv;

    if (get_state() != STATE_RUNNING) {
        log_error("Container não está rodando.");
        return 0;
    }

    log_info("Enviando sinal de desligamento...");

    // REUTILIZAÇÃO: Tenta conectar graciosamente
    int fd = ipc_connect_client(SOCK_FILE);
    
    if (fd >= 0) {
        // Se conectou, envia comando SHUTDOWN
        write(fd, CMD_SHUTDOWN, strlen(CMD_SHUTDOWN));
        char buf[16];
        read(fd, buf, sizeof(buf)); // Aguarda ACK
        close(fd);
    } else {
        // Fallback se o socket não responder
        int pid = get_box_pid();
        if (pid > 0) {
            log_info("Socket indisponível. Usando SIGTERM no PID %d...", pid);
            kill(pid, SIGTERM);
        } else {
            log_error("Não foi possível encontrar o PID para encerrar.");
        }
    }

    // Espera o processo morrer
    int pid = get_box_pid();
    if (pid > 0) {
        int status;
        if (waitpid(pid, &status, WNOHANG) == 0) {
            sleep(2);
        }
        if (kill(pid, 0) == 0) {
            log_info("Container travado. Forçando SIGKILL...");
            kill(pid, SIGKILL);
            waitpid(pid, &status, 0);
        }
    }

    cleanup_cgroup();
    set_state(STATE_STOPPED);
    log_info("Container parado.");
    return 0;
}