#include "common.h"

int cmd_exec(int argc, char **argv) {
    if (argc < 3) {
        log_error("Uso: boxverse exec <comando...>");
        return 1;
    }

    if (get_state() != STATE_RUNNING) {
        log_error("Container não está rodando.");
        return 1;
    }

    // Constrói o comando
    char full_cmd[4096] = "EXEC ";
    size_t len = 5;
    for (int i = 2; i < argc && len < sizeof(full_cmd) - 2; i++) {
        size_t arg_len = strlen(argv[i]);
        if (len + arg_len + 1 >= sizeof(full_cmd)) {
            log_error("Comando muito longo.");
            return 1;
        }
        memcpy(full_cmd + len, argv[i], arg_len);
        len += arg_len;
        full_cmd[len++] = ' ';
    }
    full_cmd[len - 1] = '\0';

    // REUTILIZAÇÃO: Conecta usando a função do ipc.c
    int fd = ipc_connect_client(SOCK_FILE);
    if (fd < 0) {
        perror("Falha ao conectar no container (verifique se ele está rodando)");
        return 1;
    }

    // Envia comando
    if (write(fd, full_cmd, strlen(full_cmd)) == -1) {
        perror("Falha ao enviar comando");
        close(fd);
        return 1;
    }

    // Loop de leitura (streaming)
    char buf[4096];
    ssize_t n;
    while ((n = read(fd, buf, sizeof(buf))) > 0) {
        if (write(STDOUT_FILENO, buf, n) == -1) break;
    }

    close(fd);
    return 0;
}