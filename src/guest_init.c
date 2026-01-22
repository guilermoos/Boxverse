// src/guest_init.c
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <string.h>
#include <fcntl.h>
// Incluímos o common.h para ter acesso às funções IPC
#include "common.h"

// Redefinimos apenas o que o guest precisa se não quisermos linkar util.c inteiro
// Mas para simplificar, usaremos as definicoes do common.h

#define MAX_CLIENTS 10

volatile int keep_running = 1;

void handle_sigterm(int sig) {
    (void)sig;
    keep_running = 0;
}

void reap_zombies() {
    while (waitpid(-1, NULL, WNOHANG) > 0);
}

int interface_exists(const char *ifname) {
    char path[128];
    snprintf(path, sizeof(path), "/sys/class/net/%s", ifname);
    return access(path, F_OK) == 0;
}

void setup_guest_network() {
    int logfd = open("/dev/null", O_WRONLY);
    if (logfd >= 0) {
        dup2(logfd, STDOUT_FILENO);
        dup2(logfd, STDERR_FILENO);
    }

    system("ip link set lo up");

    if (interface_exists("veth1")) system("ip link set veth1 name eth0");

    if (interface_exists("eth0")) {
        system("ip addr add 10.10.10.2/24 dev eth0");
        system("ip link set eth0 up");
        system("ip route add default via 10.10.10.1");
    } 

    unlink("/etc/resolv.conf");
    FILE *f = fopen("/etc/resolv.conf", "w");
    if (f) {
        fprintf(f, "nameserver 8.8.8.8\n");
        fprintf(f, "nameserver 1.1.1.1\n");
        fclose(f);
    }
    if (logfd >= 0) close(logfd);
}

void wait_and_configure_network() {
    for (int i = 0; i < 30; i++) {
        if (interface_exists("veth1") || interface_exists("eth0")) {
            setup_guest_network();
            return;
        }
        usleep(100000);
    }
}

void handle_client(int client_fd) {
    char buf[4096];
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
            dup2(client_fd, STDOUT_FILENO);
            dup2(client_fd, STDERR_FILENO);
            close(client_fd);
            execl("/bin/sh", "sh", "-c", cmd, NULL);
            perror("execl");
            exit(127);
        } 
    }
}

int main() {
    signal(SIGINT, SIG_IGN);
    signal(SIGTERM, handle_sigterm);
    
    wait_and_configure_network();

    // REUTILIZAÇÃO: Cria o servidor usando a função do ipc.c
    int server_fd = ipc_create_server(GUEST_SOCK_PATH, MAX_CLIENTS);
    if (server_fd < 0) exit(1);

    while (keep_running) {
        reap_zombies();

        fd_set readfds;
        FD_ZERO(&readfds);
        FD_SET(server_fd, &readfds);
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

    close(server_fd);
    unlink(GUEST_SOCK_PATH);
    
    kill(-1, SIGTERM);
    sleep(1);
    kill(-1, SIGKILL);
    
    return 0;
}