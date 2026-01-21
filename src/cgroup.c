// src/cgroup.c
#include "common.h"

int setup_cgroup(pid_t pid) {
    // Cgroup V2
    mkdir("/sys/fs/cgroup/boxverse", 0755);
    char path[128];
    snprintf(path, sizeof(path), "/sys/fs/cgroup/boxverse/cgroup.procs");
    
    char pid_str[16];
    sprintf(pid_str, "%d", pid);
    
    return write_file(path, pid_str);
}

int cleanup_cgroup(void) {
    return rmdir("/sys/fs/cgroup/boxverse");
}