// src/network.c
#include "common.h"

int setup_network(pid_t pid) {
    // 1. Criar par veth
    system("ip link add veth0 type veth peer name veth1");
    
    // 2. Ativar lado host
    system("ip link set veth0 up");
    // Em produção: system("ip link set veth0 master br0");
    
    // 3. Mover veth1 para guest
    char cmd[128];
    snprintf(cmd, sizeof(cmd), "ip link set veth1 netns %d", pid);
    system(cmd);
    
    // Nota: Configuração de IP dentro do guest deve ser feita pelo init 
    // ou manualmente via 'boxverse exec' pois o init deste MVP é mínimo.
    return 0;
}