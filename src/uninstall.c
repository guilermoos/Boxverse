#include "common.h"

int cmd_uninstall(int argc, char **argv) {
    (void)argc; (void)argv;

    if (get_state() == STATE_RUNNING) {
        int pid = get_box_pid();
        if (pid > 0 && kill(pid, 0) == 0) {
            log_error("Container is running (PID %d). Use 'boxverse end' first.", pid);
            return 1;
        }
        log_warn("Status was RUNNING but process is gone. Proceeding with cleanup.");
    }

    log_info("Uninstalling environment...");

    cleanup_network();
    
    // Remove metadata
    system("rm -rf .boxverse");
    
    // Remove filesystem
    if (access(ROOTFS_DIR, F_OK) == 0) {
        system("rm -rf rootfs");
    }
        
    log_info("Environment successfully uninstalled.");
    return 0;
}