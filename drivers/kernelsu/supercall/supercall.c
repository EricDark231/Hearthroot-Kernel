#include <linux/anon_inodes.h>
#include <linux/err.h>
#include <linux/fdtable.h>
#include <linux/file.h>
#include <linux/fs.h>
#include <linux/kprobes.h>
#include <linux/pid.h>
#include <linux/slab.h>
#include <linux/syscalls.h>
#include <linux/task_work.h>
#include <linux/uaccess.h>
#include <linux/version.h>
#include <linux/cred.h>
#include <linux/susfs.h>

#include "uapi/supercall.h"
#include "extension/extension.h"
#include "supercall/internal.h"
#include "arch.h"
#include "util.h"
#include "klog.h" // IWYU pragma: keep

#define KSU_DRIVER_PERMISSION_SU_SESSION (1UL << 0)

struct ksu_driver_context {
    unsigned long permissions;
};

struct ksu_install_fd_tw {
    struct callback_head cb;
    int __user *outp;
};

static int anon_ksu_release(struct inode *inode, struct file *filp)
{
    kfree(filp->private_data);
    pr_info("ksu fd released\n");
    return 0;
}

static long anon_ksu_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
    return ksu_supercall_handle_ioctl(filp, cmd, (void __user *)arg);
}

static const struct file_operations anon_ksu_fops = {
    .owner = THIS_MODULE,
    .unlocked_ioctl = anon_ksu_ioctl,
    .compat_ioctl = anon_ksu_ioctl,
    .release = anon_ksu_release,
};

static int ksu_install_fd_with_permissions(unsigned int fd_flags, unsigned long permissions)
{
    struct ksu_driver_context *context;
    struct file *filp;
    const char *name;
    int fd;

    context = kzalloc(sizeof(*context), GFP_KERNEL);
    if (!context)
        return -ENOMEM;

    context->permissions = permissions;
    name = permissions & KSU_DRIVER_PERMISSION_SU_SESSION ? "[ksu_driver_su]" : "[ksu_driver]";

    fd = get_unused_fd_flags(fd_flags);
    if (fd < 0) {
        pr_err("ksu_install_fd: failed to get unused fd for %s\n", name);
        kfree(context);
        return fd;
    }

    filp = anon_inode_getfile(name, &anon_ksu_fops, context, O_RDWR);
    if (IS_ERR(filp)) {
        pr_err("ksu_install_fd: failed to create anon inode file for %s\n", name);
        put_unused_fd(fd);
        kfree(context);
        return PTR_ERR(filp);
    }

    fd_install(fd, filp);
    pr_info("%s fd installed: %d for pid %d\n", name, fd, current->pid);
    return fd;
}

int ksu_install_fd(void)
{
    return ksu_install_fd_with_permissions(O_CLOEXEC, 0);
}

int ksu_install_su_fd(void)
{
    // This descriptor must be installed after the exec into ksud.
    return ksu_install_fd_with_permissions(O_CLOEXEC, KSU_DRIVER_PERMISSION_SU_SESSION);
}

bool ksu_is_su_session_fd(const struct file *filp)
{
    const struct ksu_driver_context *context = filp->private_data;

    return context && (context->permissions & KSU_DRIVER_PERMISSION_SU_SESSION);
}

static void ksu_install_fd_tw_func(struct callback_head *cb)
{
    struct ksu_install_fd_tw *tw = container_of(cb, struct ksu_install_fd_tw, cb);
    int fd = ksu_install_fd();

    pr_info("[%d] install ksu fd: %d\n", current->pid, fd);
    if (copy_to_user(tw->outp, &fd, sizeof(fd))) {
        pr_err("install ksu fd reply err\n");
        ksu_close_fd(fd);
    }

    kfree(tw);
}

int ksu_supercall_reboot_handler(void __user **arg)
{
    struct ksu_install_fd_tw *tw;

    tw = kzalloc(sizeof(*tw), GFP_KERNEL);
    if (!tw)
        return 0;

    tw->outp = (int __user *)(*arg);
    tw->cb.func = ksu_install_fd_tw_func;

    if (task_work_add(current, &tw->cb, TWA_RESUME)) {
        kfree(tw);
        pr_warn("install fd add task_work failed\n");
    }

    return 0;
}

void __init ksu_supercalls_init(void)
{
    ksu_supercall_dump_commands();
}

void __exit ksu_supercalls_exit(void)
{
    ksu_supercall_cleanup_state();
}

/*
 * Kernel-side SUSFS command hook: kernel/reboot.c routes reboot(2) calls
 * carrying SUSFS_MAGIC (fs configuration commands) or KSU_INSTALL_MAGIC2
 * (supercall fd install) here. Lives in this island file because
 * supercall/dispatch.c tracks upstream v4.3.0, which dropped it.
 */
int ksu_supercall_reboot_handler(void __user **arg);

int ksu_handle_sys_reboot(int magic1, int magic2, unsigned int cmd, void __user **arg)
{
    if (magic1 != KSU_INSTALL_MAGIC1) {
        return -EINVAL; 
    }

    // If magic2 is susfs and current process is root
    if (magic2 == SUSFS_MAGIC && current_uid().val == 0) {
        switch(cmd) {
#ifdef CONFIG_KSU_SUSFS_SUS_PATH
            case CMD_SUSFS_ADD_SUS_PATH:
                susfs_add_sus_path(arg);
                return 0;
            case CMD_SUSFS_ADD_SUS_PATH_LOOP:
                susfs_add_sus_path_loop(arg);
                return 0;
#endif // #ifdef CONFIG_KSU_SUSFS_SUS_PATH
#ifdef CONFIG_KSU_SUSFS_SUS_MOUNT
            case CMD_SUSFS_HIDE_SUS_MNTS_FOR_NON_SU_PROCS:
                susfs_set_hide_sus_mnts_for_non_su_procs(arg);
                return 0;
#endif // #ifdef CONFIG_KSU_SUSFS_SUS_MOUNT
#ifdef CONFIG_KSU_SUSFS_SUS_KSTAT
            case CMD_SUSFS_ADD_SUS_KSTAT:
                susfs_add_sus_kstat(arg);
                return 0;
            case CMD_SUSFS_UPDATE_SUS_KSTAT:
                susfs_update_sus_kstat(arg);
                return 0;
            case CMD_SUSFS_ADD_SUS_KSTAT_STATICALLY:
                susfs_add_sus_kstat(arg);
                return 0;
#endif // #ifdef CONFIG_KSU_SUSFS_SUS_KSTAT
#ifdef CONFIG_KSU_SUSFS_SPOOF_UNAME
            case CMD_SUSFS_SET_UNAME:
                susfs_set_uname(arg);
                return 0;
#endif // #ifdef CONFIG_KSU_SUSFS_SPOOF_UNAME
#ifdef CONFIG_KSU_SUSFS_ENABLE_LOG
            case CMD_SUSFS_ENABLE_LOG:
                susfs_enable_log(arg);
                return 0;
#endif // #ifdef CONFIG_KSU_SUSFS_ENABLE_LOG
#ifdef CONFIG_KSU_SUSFS_SPOOF_CMDLINE_OR_BOOTCONFIG
            case CMD_SUSFS_SET_CMDLINE_OR_BOOTCONFIG:
                susfs_set_cmdline_or_bootconfig(arg);
                return 0;
#endif // #ifdef CONFIG_KSU_SUSFS_SPOOF_CMDLINE_OR_BOOTCONFIG
#ifdef CONFIG_KSU_SUSFS_OPEN_REDIRECT
            case CMD_SUSFS_ADD_OPEN_REDIRECT:
                susfs_add_open_redirect(arg);
                return 0;
#endif // #ifdef CONFIG_KSU_SUSFS_OPEN_REDIRECT
#ifdef CONFIG_KSU_SUSFS_SUS_MAP
            case CMD_SUSFS_ADD_SUS_MAP:
                susfs_add_sus_map(arg);
                return 0;
#endif // #ifdef CONFIG_KSU_SUSFS_SUS_MAP
            case CMD_SUSFS_ENABLE_AVC_LOG_SPOOFING:
                susfs_set_avc_log_spoofing(arg);
                return 0;
            case CMD_SUSFS_SHOW_ENABLED_FEATURES:
                susfs_get_enabled_features(arg);
                return 0;
            case CMD_SUSFS_SHOW_VARIANT:
                susfs_show_variant(arg);
                return 0;
            case CMD_SUSFS_SHOW_VERSION:
                susfs_show_version(arg);
                return 0;
            default:
                return -EINVAL;
        }
    }

    if (magic2 == KSU_INSTALL_MAGIC2)
        return ksu_supercall_reboot_handler(arg);

    return -EINVAL;
}
