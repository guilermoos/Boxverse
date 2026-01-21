#include "common.h"
#include <sys/socket.h>
#include <sys/un.h>

int cmd_end(int argc, char **argv) {
    (void)argc; (void)argv;

    if (get_state() != STATE_RUNNING) {
        log_error("Container não está rodando.");
        return 0;
    }

    log_info("Enviando sinal de desligamento...");

    // Tenta conectar via Socket
    int fd = socket(AF_UNIX, SOCK_STREAM, 0);
    struct sockaddr_un addr = {0};
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, SOCK_FILE, sizeof(addr.sun_path)-1);

    if (connect(fd, (struct sockaddr*)&addr, sizeof(addr)) == 0) {
        // Conexão socket bem sucedida
        write(fd, CMD_SHUTDOWN, strlen(CMD_SHUTDOWN));
        char buf[16];
        read(fd, buf, sizeof(buf)); // Wait OK
        close(fd);
    } else {
        // Fallback: SIGTERM no PID
        int pid = get_box_pid();
        if (pid > 0) {
            log_info("Socket indisponível. Usando SIGTERM no PID %d...", pid);
            kill(pid, SIGTERM);
        } else {
            log_error("Não foi possível encontrar o PID para encerrar.");
        }
    }

    // Aguarda término (polling simples)
    sleep(2);
    
    // Limpeza forçada se ainda existir (SIGKILL)
    int pid = get_box_pid();
    if (pid > 0 && kill(pid, 0) == 0) {
        log_info("Container travado. Forçando SIGKILL...");
        kill(pid, SIGKILL);
    }

    cleanup_cgroup();
    set_state(STATE_STOPPED);
    log_info("Container parado.");
    return 0;
}