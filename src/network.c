#include "common.h"

// Internal helper to simplify rule execution
static void apply_iptables(const char *rule) {
    char cmd[512];
    snprintf(cmd, sizeof(cmd), "iptables %s >> /dev/null 2>&1", rule);
    system(cmd);
}

int setup_network(pid_t pid) {
    log_info("Configuring host network stack for PID %d...", pid);

    // Create pair veth0 <-> veth1
    if (system("ip link add veth0 type veth peer name veth1") != 0) {
        log_error("Failed to create veth pair.");
        return 1;
    }
    
    // Configure Host Side
    system("ip addr add 10.10.10.1/24 dev veth0");
    system("ip link set veth0 up");
    
    // Move Peer to Container
    char cmd[256];
    snprintf(cmd, sizeof(cmd), "ip link set veth1 netns %d", pid);
    if (system(cmd) != 0) {
        log_error("Failed to move interface to guest namespace.");
        return 1;
    }

    // Enable IP Forwarding
    system("echo 1 > /proc/sys/net/ipv4/ip_forward");
    
    // Masquerading (NAT)
    apply_iptables("-t nat -D POSTROUTING -s 10.10.10.0/24 -j MASQUERADE"); // clean old
    apply_iptables("-t nat -A POSTROUTING -s 10.10.10.0/24 -j MASQUERADE");
    
    apply_iptables("-D FORWARD -i veth0 -j ACCEPT");
    apply_iptables("-A FORWARD -i veth0 -j ACCEPT");
    apply_iptables("-D FORWARD -o veth0 -j ACCEPT");
    apply_iptables("-A FORWARD -o veth0 -j ACCEPT");

    // Optional Port Forwarding (Example: 5000 host -> 5000 guest)
    // You could make this dynamic via config later
    /*
    apply_iptables("-t nat -A OUTPUT -p tcp -d 127.0.0.1 --dport 5000 -j DNAT --to-destination 10.10.10.2:5000");
    apply_iptables("-t nat -A PREROUTING -p tcp --dport 5000 -j DNAT --to-destination 10.10.10.2:5000");
    */

    return 0;
}

int cleanup_network(void) {
    apply_iptables("-t nat -D POSTROUTING -s 10.10.10.0/24 -j MASQUERADE");
    apply_iptables("-D FORWARD -i veth0 -j ACCEPT");
    apply_iptables("-D FORWARD -o veth0 -j ACCEPT");
    
    // Kernel automatically deletes veth0 when the namespace dies, 
    // but we can try to delete it explicitly just in case.
    system("ip link delete veth0 >> /dev/null 2>&1");
    return 0;
}