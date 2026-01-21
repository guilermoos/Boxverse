#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <errno.h>

#define SOCKET_PATH "/.boxverse/control.sock"
#define MAX_CLIENTS 5

volatile int keep_running = 1;

void handle_sigterm(int sig) {
    (void)sig;
    keep_running = 0;
}

void reap_zombies() {
    while (waitpid(-1, NULL, WNOHANG) > 0);
}

// Protocolo:
// 1. Recebe comando: "EXEC <cmd>" ou "SHUTDOWN"
// 2. EXEC: Fork -> Exec -> Wait -> Send Return Code
// 3. SHUTDOWN: Set running=0 -> Send "OK"
void handle_client(int client_fd) {
    char buf[1024];
    int len = read(client_fd, buf, sizeof(buf) - 1);
    if (len <= 0) return;
    buf[len] = '\0';

    if (strncmp(buf, "SHUTDOWN", 8) == 0) {
        write(client_fd, "OK", 2);
        keep_running = 0;
    } 
    else if (strncmp(buf, "EXEC ", 5) == 0) {
        char *cmd = buf + 5;
        pid_t pid = fork();

        if (pid == 0) {
            // Filho: executa comando
            // Redireciona stdout/stderr se necessário (aqui herdando do init que não tem tty)
            // Em produção, redirecionaria para socket ou log
            execl("/bin/sh", "sh", "-c", cmd, NULL);
            exit(127); // Erro se execl falhar
        } else if (pid > 0) {
            // Pai (Init): espera comando terminar para retornar status
            int status;
            waitpid(pid, &status, 0);
            int exit_code = WIFEXITED(status) ? WEXITSTATUS(status) : 128;
            
            // Responde com o código de saída
            char resp[16];
            snprintf(resp, sizeof(resp), "%d", exit_code);
            write(client_fd, resp, strlen(resp));
        }
    }
}

int main() {
    // 1. Bloquear sinais padrão
    signal(SIGINT, SIG_IGN);
    signal(SIGTERM, handle_sigterm);
    
    // 2. Configurar Socket
    unlink(SOCKET_PATH);
    int server_fd = socket(AF_UNIX, SOCK_STREAM, 0);
    struct sockaddr_un addr = {0};
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, SOCKET_PATH, sizeof(addr.sun_path)-1);

    if (bind(server_fd, (struct sockaddr*)&addr, sizeof(addr)) == -1) {
        perror("init: bind socket failed");
        exit(1);
    }
    listen(server_fd, MAX_CLIENTS);

    // 3. Loop Principal
    while (keep_running) {
        reap_zombies();

        fd_set readfds;
        FD_ZERO(&readfds);
        FD_SET(server_fd, &readfds);

        // Timeout para garantir que reap_zombies rode periodicamente
        struct timeval tv = {1, 0}; 

        int activity = select(server_fd + 1, &readfds, NULL, NULL, &tv);

        if (activity > 0 && FD_ISSET(server_fd, &readfds)) {
            int client_fd = accept(server_fd, NULL, NULL);
            if (client_fd >= 0) {
                handle_client(client_fd);
                close(client_fd);
            }
        }
    }

    // 4. Shutdown Graceful
    printf("Init: shutting down...\n");
    close(server_fd);
    unlink(SOCKET_PATH);
    
    kill(-1, SIGTERM);
    sleep(2);
    kill(-1, SIGKILL);
    
    return 0;
}