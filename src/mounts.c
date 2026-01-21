#include "common.h"
#include <sys/mount.h>
#include <sys/syscall.h>

int setup_mounts(const char *rootfs) {
    // 1. Tornar / privado (evita propagação)
    if (mount(NULL, "/", NULL, MS_REC | MS_PRIVATE, NULL) != 0) {
        perror("mount / private");
        return 1;
    }

    // 2. Bind Mount do rootfs sobre si mesmo (requisito pivot_root)
    if (mount(rootfs, rootfs, NULL, MS_BIND | MS_REC, NULL) != 0) {
        perror("mount rootfs bind");
        return 1;
    }

    // 3. Montar .boxverse dentro do rootfs (CRUCIAL PARA SOCKET)
    // O socket é criado pelo init em /.boxverse/control.sock
    char target_box[256];
    snprintf(target_box, sizeof(target_box), "%s/.boxverse", rootfs);
    mkdir(target_box, 0755);
    if (mount(BOX_DIR, target_box, NULL, MS_BIND, NULL) != 0) {
        perror("mount .boxverse");
        // Não falha, mas avisa
    }

    // 4. Mudar diretório atual
    chdir(rootfs);

    // 5. Pivot Root
    mkdir("old_root", 0755);
    if (syscall(SYS_pivot_root, ".", "old_root") != 0) {
        perror("pivot_root");
        return 1;
    }
    chdir("/");

    // 6. Montar Essenciais
    mount("proc", "/proc", "proc", 0, NULL);
    mount("sysfs", "/sys", "sysfs", 0, NULL);
    mount("tmpfs", "/dev", "tmpfs", 0, "mode=755");

    // 7. Limpar old_root
    umount2("/old_root", MNT_DETACH);
    rmdir("/old_root");

    return 0;
}