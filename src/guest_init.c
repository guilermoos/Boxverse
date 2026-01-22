#include "common.h"
#include <sys/wait.h>

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

void setup_guest_network_internal() {
    int logfd = open("/dev/null", O_WRONLY);
    if (logfd >= 0) {
        dup2(logfd, STDOUT_FILENO);
        dup2(logfd, STDERR_FILENO);
    }

    system("ip link set lo up");

    // Host sends "veth1". We rename to "eth0"
    if (interface_exists("veth1")) system("ip link set veth1 name eth0");

    if (interface_exists("eth0")) {
        system("ip addr add 10.10.10.2/24 dev eth0");
        system("ip link set eth0 up");
        system("ip route add default via 10.10.10.1");
    } 

    // DNS refresh (safety measure)
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
    // Wait for the host to attach the veth interface
    for (int i = 0; i < 50; i++) {
        if (interface_exists("veth1") || interface_exists("eth0")) {
            setup_guest_network_internal();
            return;
        }
        usleep(100000); // 100ms
    }
}

void handle_client(int client_fd) {
    char buf[4096];
    ssize_t len = read(client_fd, buf, sizeof(buf) - 1);
    if (len <= 0) return;
    buf[len] = '\0';

    if (strncmp(buf, CMD_SHUTDOWN, strlen(CMD_SHUTDOWN)) == 0) {
        write(client_fd, "OK", 2);
        keep_running = 0;
    } 
    else if (strncmp(buf, CMD_EXEC, strlen(CMD_EXEC)) == 0) {
        char *cmd = buf + strlen(CMD_EXEC) + 1; // Skip "EXEC "
        pid_t pid = fork();

        if (pid == 0) {
            // Redirect child output to socket
            dup2(client_fd, STDOUT_FILENO);
            dup2(client_fd, STDERR_FILENO);
            close(client_fd);
            
            // Execute shell command
            execl("/bin/sh", "sh", "-c", cmd, NULL);
            perror("execl failed");
            exit(127);
        }
        // Parent does not wait immediately, the main loop handles SIGCHLD
    }
}

int main() {
    // Ignore Ctrl+C inside, handle shutdown explicitly
    signal(SIGINT, SIG_IGN);
    signal(SIGTERM, handle_sigterm);
    
    // Environment
    setenv("PATH", "/usr/local/sbin:/usr/local/bin:/usr/sbin:/usr/bin:/sbin:/bin", 1);
    setenv("HOME", "/root", 1);

    wait_and_configure_network();

    int server_fd = ipc_create_server(GUEST_SOCK_PATH, MAX_CLIENTS);
    if (server_fd < 0) exit(1);

    while (keep_running) {
        reap_zombies();

        fd_set readfds;
        FD_ZERO(&readfds);
        FD_SET(server_fd, &readfds);
        
        struct timeval tv = {1, 0}; // 1 second timeout

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
    
    // Kill all processes
    kill(-1, SIGTERM);
    sleep(1);
    kill(-1, SIGKILL);
    
    return 0;
}