#include "common.h"

void usage() {
    printf("\n");
    printf("   \033[1mBoxverse Container Runtime\033[0m\n");
    printf("  -------------------------------------------\n");
    printf("  Gerenciador de containers persistentes e minimalistas.\n");
    printf("\n");
    printf("  \033[1mUso:\033[0m\n");
    printf("    sudo boxverse <comando> [argumentos...]\n");
    printf("\n");
    printf("  \033[1mCiclo de Vida:\033[0m\n");
    printf("    \033[1minit\033[0m <distro>    Baixa o sistema base (debootstrap) e prepara a pasta.\n");
    printf("                     \033[36m--net\033[0m   Habilita stack de rede (veth/bridge).\n");
    printf("                     \033[36m--ssh\033[0m   Prepara configuração para SSH (opcional).\n");
    printf("    \033[1mstart\033[0m           Inicia o container (PID 1 persistente) e abre um shell.\n");
    printf("                     Se o container já estiver rodando, apenas conecta (attach).\n");
    printf("    \033[1mend\033[0m             Envia sinal de desligamento gracioso (SHUTDOWN) ao init.\n");
    printf("    \033[1mdestroy\033[0m         Para (se necessário) e apaga todos os arquivos e metadados.\n");
    printf("\n");
    printf("  \033[1mInteração:\033[0m\n");
    printf("    \033[1mexec\033[0m <cmd...>    Executa um comando isolado dentro do container rodando.\n");
    printf("                     Ex: boxverse exec python3 app.py\n");
    printf("\n");
    printf("  \033[1mExemplos:\033[0m\n");
    printf("    sudo boxverse init noble --net\n");
    printf("    sudo boxverse start\n");
    printf("    sudo boxverse exec ps aux\n");
    printf("    sudo boxverse end\n");
    printf("\n");
    exit(1);
}

int main(int argc, char **argv) {
    // 1. Verificação de Segurança (Root é obrigatório para namespaces)
    if (geteuid() != 0) {
        fprintf(stderr, "\033[31m[ERRO] Boxverse precisa de privilégios de root (sudo).\033[0m\n");
        return 1;
    }

    // 2. Sem argumentos ou help
    if (argc < 2) {
        usage();
    }
    
    if (strcmp(argv[1], "-h") == 0 || strcmp(argv[1], "--help") == 0) {
        usage();
    }

    const char *cmd = argv[1];

    // 3. Roteamento de Comandos
    if (strcmp(cmd, "init") == 0)    return cmd_init(argc, argv);
    if (strcmp(cmd, "start") == 0)   return cmd_start(argc, argv);
    if (strcmp(cmd, "exec") == 0)    return cmd_exec(argc, argv);
    if (strcmp(cmd, "end") == 0)     return cmd_end(argc, argv);
    if (strcmp(cmd, "destroy") == 0) return cmd_destroy(argc, argv);

    // 4. Comando desconhecido
    fprintf(stderr, "\033[31m[ERRO] Comando desconhecido: '%s'\033[0m\n", cmd);
    fprintf(stderr, "Use 'boxverse --help' para ver a lista de comandos.\n");
    return 1;
}