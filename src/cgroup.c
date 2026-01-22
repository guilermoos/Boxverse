#include "common.h"

#define CGROUP_PATH "/sys/fs/cgroup/boxverse"

int setup_cgroup(pid_t pid) {
    if (mkdir(CGROUP_PATH, 0755) != 0 && errno != EEXIST) {
        perror("mkdir cgroup");
        return 1;
    }

    char path[256];
    
    // Add PID to cgroup
    snprintf(path, sizeof(path), "%s/cgroup.procs", CGROUP_PATH);
    char pid_str[32];
    snprintf(pid_str, sizeof(pid_str), "%d", pid);
    
    if (write_file(path, pid_str) != 0) {
        log_error("Failed to write PID to cgroup.");
        return 1;
    }

    return 0;
}

int cleanup_cgroup(void) {
    if (rmdir(CGROUP_PATH) != 0 && errno != ENOENT) {
        // It's common to fail if tasks are still cleaning up
        return 1; 
    }
    return 0;
}