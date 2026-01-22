// src/network.c
#include "common.h"

void apply_iptables_rule(const char *rule) {
    char cmd[256];
    snprintf(cmd, sizeof(cmd), "iptables %s", rule);
    system(cmd);
}

int setup_network(pid_t pid) {
    log_info("Configurando rede para PID %d...", pid);

    if (system("ip link add veth0 type veth peer name veth1") != 0) {
        log_error("Falha ao criar par veth");
        return 1;
    }
    
    system("ip addr add 10.10.10.1/24 dev veth0");
    system("ip link set veth0 up");
    
    char cmd[128];
    snprintf(cmd, sizeof(cmd), "ip link set veth1 netns %d", pid);
    if (system(cmd) != 0) {
        log_error("Falha ao mover interface para o guest");
        return 1;
    }

    system("echo 1 > /proc/sys/net/ipv4/ip_forward");
    
    apply_iptables_rule("-t nat -D POSTROUTING -s 10.10.10.0/24 -j MASQUERADE 2>/dev/null");
    apply_iptables_rule("-t nat -A POSTROUTING -s 10.10.10.0/24 -j MASQUERADE");
    
    apply_iptables_rule("-D FORWARD -i veth0 -j ACCEPT 2>/dev/null");
    apply_iptables_rule("-A FORWARD -i veth0 -j ACCEPT");
    apply_iptables_rule("-D FORWARD -o veth0 -j ACCEPT 2>/dev/null");
    apply_iptables_rule("-A FORWARD -o veth0 -j ACCEPT");

    // Adicionar regras de NAT para redirecionamento de porta 5000
    apply_iptables_rule("-t nat -A OUTPUT -p tcp -d 127.0.0.1 --dport 5000 -j DNAT --to-destination 10.10.10.2:5000");
    apply_iptables_rule("-t nat -A PREROUTING -p tcp --dport 5000 -j DNAT --to-destination 10.10.10.2:5000");
    

    return 0;
}

int cleanup_network(void) {
    // Remove as regras de NAT e Forwarding criadas
    apply_iptables_rule("-t nat -D POSTROUTING -s 10.10.10.0/24 -j MASQUERADE 2>/dev/null");
    apply_iptables_rule("-D FORWARD -i veth0 -j ACCEPT 2>/dev/null");
    apply_iptables_rule("-D FORWARD -o veth0 -j ACCEPT 2>/dev/null");
    
    // Remover regras de NAT para redirecionamento de porta 5000
    apply_iptables_rule("-t nat -D OUTPUT -p tcp -d 127.0.0.1 --dport 5000 -j DNAT --to-destination 10.10.10.2:5000 2>/dev/null");
    apply_iptables_rule("-t nat -D PREROUTING -p tcp --dport 5000 -j DNAT --to-destination 10.10.10.2:5000 2>/dev/null");
    
    // Opcional: Remover a interface (o kernel remove sozinho quando o namespace morre, mas ajuda na limpeza)
    system("ip link delete veth0 2>/dev/null");
    return 0;
}