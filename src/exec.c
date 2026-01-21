#include "common.h"
#include <sys/socket.h>
#include <sys/un.h>

int cmd_exec(int argc, char **argv) {
    if (argc < 3) {
        log_error("Uso: boxverse exec <comando...>");
        return 1;
    }

    if (get_state() != STATE_RUNNING) {
        log_error("Container não está rodando.");
        return 1;
    }

    // Monta string de comando
    char full_cmd[1024] = "EXEC ";
    for (int i = 2; i < argc; i++) {
        strcat(full_cmd, argv[i]);
        strcat(full_cmd, " ");
    }

    // Conecta no Socket
    int fd = socket(AF_UNIX, SOCK_STREAM, 0);
    struct sockaddr_un addr = {0};
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, SOCK_FILE, sizeof(addr.sun_path)-1);

    if (connect(fd, (struct sockaddr*)&addr, sizeof(addr)) == -1) {
        perror("Falha ao conectar no container");
        return 1;
    }

    // Envia e Espera
    write(fd, full_cmd, strlen(full_cmd));
    
    char buf[16];
    int len = read(fd, buf, sizeof(buf)-1);
    close(fd);

    if (len > 0) {
        buf[len] = '\0';
        int exit_code = atoi(buf);
        return exit_code;
    }

    return 1;
}