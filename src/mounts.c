// src/mounts.c
#include "common.h"
#include <sys/mount.h>
#include <sys/syscall.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/sysmacros.h>

void populate_dev(void) {
    mode_t old_mask = umask(0);

    chdir("/dev");

    mknod("null", S_IFCHR | 0666, makedev(1, 3));
    mknod("zero", S_IFCHR | 0666, makedev(1, 5));
    mknod("full", S_IFCHR | 0666, makedev(1, 7));
    mknod("random", S_IFCHR | 0666, makedev(1, 8));
    mknod("urandom", S_IFCHR | 0666, makedev(1, 9));
    mknod("tty", S_IFCHR | 0666, makedev(5, 0));

    umask(old_mask);

    symlink("/proc/self/fd", "fd");
    symlink("/proc/self/fd/0", "stdin");
    symlink("/proc/self/fd/1", "stdout");
    symlink("/proc/self/fd/2", "stderr");

    mkdir("pts", 0755);
    mount("devpts", "pts", "devpts", 0, "gid=5,mode=620,ptmxmode=666");
    symlink("pts/ptmx", "ptmx");

    chdir("/");
}

int setup_mounts(const char *rootfs) {
    if (mount(NULL, "/", NULL, MS_REC | MS_PRIVATE, NULL) != 0) {
        perror("mount / private");
        return 1;
    }

    if (mount(rootfs, rootfs, NULL, MS_BIND | MS_REC, NULL) != 0) {
        perror("mount rootfs bind");
        return 1;
    }

    char target_box[256];
    snprintf(target_box, sizeof(target_box), "%s/.boxverse", rootfs);
    mkdir(target_box, 0755);
    mount(BOX_DIR, target_box, NULL, MS_BIND, NULL);

    chdir(rootfs);

    mkdir("old_root", 0755);
    if (syscall(SYS_pivot_root, ".", "old_root") != 0) {
        perror("pivot_root");
        return 1;
    }
    chdir("/");

    mount("proc", "/proc", "proc", 0, NULL);
    mount("sysfs", "/sys", "sysfs", 0, NULL);

    if (mount("tmpfs", "/dev", "tmpfs", 0, "mode=755") == 0) {
        populate_dev();
    } else {
        perror("mount dev tmpfs");
    }

    umount2("/old_root", MNT_DETACH);
    rmdir("/old_root");

    return 0;
}