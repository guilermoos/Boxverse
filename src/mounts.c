#include "common.h"
#include <sys/mount.h>
#include <sys/syscall.h>
#include <sys/sysmacros.h>

void populate_dev(void) {
    mode_t old_mask = umask(0);

    // Create essential nodes
    if (chdir("/dev") != 0) return;

    mknod("null", S_IFCHR | 0666, makedev(1, 3));
    mknod("zero", S_IFCHR | 0666, makedev(1, 5));
    mknod("full", S_IFCHR | 0666, makedev(1, 7));
    mknod("random", S_IFCHR | 0666, makedev(1, 8));
    mknod("urandom", S_IFCHR | 0666, makedev(1, 9));
    mknod("tty", S_IFCHR | 0666, makedev(5, 0));

    umask(old_mask);

    // IO Symlinks
    symlink("/proc/self/fd", "fd");
    symlink("/proc/self/fd/0", "stdin");
    symlink("/proc/self/fd/1", "stdout");
    symlink("/proc/self/fd/2", "stderr");

    // PTY
    mkdir("pts", 0755);
    mount("devpts", "pts", "devpts", 0, "gid=5,mode=620,ptmxmode=666");
    symlink("pts/ptmx", "ptmx");

    chdir("/");
}

int setup_mounts(const char *rootfs) {
    // Ensure mount propagation is private to avoid polluting host
    if (mount(NULL, "/", NULL, MS_REC | MS_PRIVATE, NULL) != 0) {
        perror("mount / private");
        return 1;
    }

    // Bind mount rootfs to itself
    if (mount(rootfs, rootfs, NULL, MS_BIND | MS_REC, NULL) != 0) {
        perror("mount rootfs bind");
        return 1;
    }

    // Bind mount .boxverse config dir into container (for socket communication)
    char target_box[256];
    snprintf(target_box, sizeof(target_box), "%s/.boxverse", rootfs);
    mkdir(target_box, 0755);
    if (mount(BOX_DIR, target_box, NULL, MS_BIND, NULL) != 0) {
        perror("mount bind .boxverse");
    }

    if (chdir(rootfs) != 0) {
        perror("chdir rootfs");
        return 1;
    }

    // Pivot Root
    mkdir("old_root", 0755);
    if (syscall(SYS_pivot_root, ".", "old_root") != 0) {
        perror("pivot_root failed");
        return 1;
    }
    if (chdir("/") != 0) {
        perror("chdir /");
        return 1;
    }

    // Virtual filesystems
    mount("proc", "/proc", "proc", 0, NULL);
    mount("sysfs", "/sys", "sysfs", 0, NULL);

    if (mount("tmpfs", "/dev", "tmpfs", 0, "mode=755") == 0) {
        populate_dev();
    } else {
        perror("mount dev tmpfs");
    }

    // Cleanup old root
    umount2("/old_root", MNT_DETACH);
    rmdir("/old_root");

    return 0;
}