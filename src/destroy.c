#include "common.h"

int cmd_destroy(int argc, char **argv) {
    (void)argc; (void)argv;

    if (get_state() == STATE_RUNNING) {
        int pid = get_box_pid();
        if (pid > 0 && kill(pid, 0) == 0) {
            log_error("Erro: Container rodando (PID %d). Use 'boxverse end' primeiro.", pid);
            return 1;
        }
        log_info("Aviso: Estado era RUNNING mas processo morreu. Procedendo com limpeza.");
    }

    log_info("Destruindo ambiente boxverse...");

    cleanup_network();

    system("rm -rf .boxverse");
    
    if (access(ROOTFS_DIR, F_OK) == 0) {
        system("rm -rf rootfs");
    }
        
    log_info("Ambiente destruído com sucesso.");
    return 0;
}