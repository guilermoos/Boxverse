#include "common.h"

void usage() {
    printf("\n");
    printf("   \033[1mBoxverse Container Runtime\033[0m\n");
    printf("  -------------------------------------------\n");
    printf("  A minimalist persistent container manager.\n");
    printf("\n");
    printf("  \033[1mCommands:\033[0m\n");
    printf("    \033[1minstall\033[0m <distro> Download base image and setup rootfs.\n");
    printf("                     \033[36m--net\033[0m   Enable network stack.\n");
    printf("    \033[1mstart\033[0m            Boot container (background) and attach shell.\n");
    printf("    \033[1mexec\033[0m <cmd...>     Execute command inside running container.\n");
    printf("    \033[1mend\033[0m              Shutdown container gracefully.\n");
    printf("    \033[1muninstall\033[0m        Delete environment files and data.\n");
    printf("\n");
    printf("  \033[1mExample:\033[0m\n");
    printf("    sudo boxverse install alpine --net\n");
    printf("    sudo boxverse start\n");
    printf("\n");
    exit(1);
}

int main(int argc, char **argv) {
    // 1. Root Security Check
    if (geteuid() != 0) {
        fprintf(stderr, "\033[31m[ERROR] Root privileges required (sudo).\033[0m\n");
        return 1;
    }

    // 2. Help handling
    if (argc < 2) usage();
    if (strcmp(argv[1], "-h") == 0 || strcmp(argv[1], "--help") == 0) usage();

    const char *cmd = argv[1];

    // 3. Command Routing
    // MUDANÇA: Roteamento atualizado para 'install' e 'uninstall'
    if (strcmp(cmd, "install") == 0)    return cmd_install(argc, argv);
    if (strcmp(cmd, "start") == 0)      return cmd_start(argc, argv);
    if (strcmp(cmd, "exec") == 0)       return cmd_exec(argc, argv);
    if (strcmp(cmd, "end") == 0)        return cmd_end(argc, argv);
    if (strcmp(cmd, "uninstall") == 0)  return cmd_uninstall(argc, argv);

    // 4. Invalid Command
    fprintf(stderr, "\033[31m[ERROR] Unknown command: '%s'\033[0m\n", cmd);
    usage();
    return 1;
}