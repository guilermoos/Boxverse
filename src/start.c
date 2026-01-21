#include "common.h"
#include <sched.h>
#include <sys/wait.h>
#include <sys/mount.h>

// Função executada pelo processo filho (dentro do namespace)
int container_entrypoint(void *arg) {
    (void)arg;

    // 1. Configurar Hostname
    sethostname("boxverse", 8);

    // 2. Setup Mounts (Pivot Root + Proc/Sys/Dev)
    if (setup_mounts(ROOTFS_DIR) != 0) return 1;

    // 3. Executar Init Persistente
    // O init agora roda como PID 1 dentro do novo root
    char *args[] = {"/sbin/boxverse-init", NULL};
    char *env[] = {"PATH=/bin:/sbin:/usr/bin:/usr/sbin", "TERM=xterm", NULL};
    
    execve(args[0], args, env);
    perror("Falha crítica: execve init");
    return 1;
}

int attach_to_container(void) {
    int pid = get_box_pid();
    log_info("Container rodando (PID %d). Anexando shell...", pid);

    // Comando para entrar nos namespaces (nsenter simplificado via system para MVP)
    // Em C puro usaria setns(), mas exigiria tratar PTY complexo.
    // Usaremos execvp para chamar nsenter ou implementar setns + exec bash
    
    char pid_str[16];
    sprintf(pid_str, "%d", pid);

    // Entrando nos namespaces manualmente
    char path[256];
    char *ns_list[] = {"ipc", "uts", "net", "pid", "mnt"};
    
    for(int i=0; i<5; i++) {
        snprintf(path, sizeof(path), "/proc/%s/ns/%s", pid_str, ns_list[i]);
        int fd = open(path, O_RDONLY);
        if (fd == -1) continue; // Pode não ter net namespace
        if (setns(fd, 0) == -1) perror("setns");
        close(fd);
    }

    // Fork para entrar no PID namespace efetivamente e rodar shell
    if (fork() == 0) {
        execl("/bin/bash", "bash", NULL);
        execl("/bin/sh", "sh", NULL); // Fallback
        perror("Falha ao abrir shell");
        exit(1);
    }
    
    wait(NULL);
    return 0;
}

int cmd_start(int argc, char **argv) {
    (void)argc; (void)argv;

    // Se já roda, anexa (Attach)
    if (get_state() == STATE_RUNNING) {
        if (get_box_pid() > 0) return attach_to_container();
        // Se estado é RUNNING mas sem PID, algo quebrou. Continuar para restart.
        set_state(STATE_STOPPED);
    }

    Config cfg = load_config();
    int lock_fd = acquire_lock();
    if (lock_fd < 0) return 1;

    log_info("Iniciando container...");

    // 1. Stack para clone
    char *stack = malloc(1024 * 1024);
    if (!stack) return 1;
    char *stack_top = stack + 1024 * 1024;

    // 2. Flags de Namespace
    int flags = CLONE_NEWNS | CLONE_NEWPID | CLONE_NEWIPC | CLONE_NEWUTS | SIGCHLD;
    if (cfg.net_enabled) flags |= CLONE_NEWNET;

    // 3. Clone (Fork especial)
    pid_t child_pid = clone(container_entrypoint, stack_top, flags, NULL);
    if (child_pid == -1) {
        perror("clone");
        free(stack);
        release_lock(lock_fd);
        return 1;
    }

    // 4. Configuração Pós-Clone (Lado Host)
    write_file(PID_FILE, ""); // Limpa PID antigo
    char pid_str[16];
    sprintf(pid_str, "%d", child_pid);
    write_file(PID_FILE, pid_str);

    setup_cgroup(child_pid);
    if (cfg.net_enabled) setup_network(child_pid);

    set_state(STATE_RUNNING);
    release_lock(lock_fd);

    log_info("Container iniciado em background (PID %d).", child_pid);

    // 5. Auto-attach imediato
    usleep(200000); // Pequena pausa para init subir
    return attach_to_container();
}