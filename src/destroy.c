#include "common.h"

int cmd_destroy(int argc, char **argv) {
    (void)argc; (void)argv;

    // Verifica se está rodando usando a abstração correta
    if (get_state() == STATE_RUNNING) {
        // Double check com PID para ter certeza
        int pid = get_box_pid();
        if (pid > 0 && kill(pid, 0) == 0) {
            log_error("Erro: Container rodando (PID %d). Use 'boxverse end' primeiro.", pid);
            return 1;
        }
        // Se o estado é RUNNING mas não tem processo, é estado inconsistente. Permitimos destruir.
        log_info("Aviso: Estado era RUNNING mas processo morreu. Procedendo com limpeza.");
    }

    log_info("Destruindo ambiente boxverse...");

    // Remove metadados
    system("rm -rf .boxverse");
    
    // Remove rootfs (opcional, conforme sua política de limpeza total)
    if (access(ROOTFS_DIR, F_OK) == 0) {
        system("rm -rf rootfs");
    }
    
    // Limpa estado na memória (conceitual)
    // Como apagamos .boxverse, set_state falharia ao tentar abrir o arquivo,
    // então apenas logamos.
    
    log_info("Ambiente destruído com sucesso.");
    return 0;
}