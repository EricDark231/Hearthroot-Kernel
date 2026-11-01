#include "linux/printk.h"
#include <linux/module.h>
#include <linux/spinlock.h>
#include <linux/kprobes.h>
#include <linux/tracepoint.h>
#include <asm/syscall.h>
#include <linux/ptrace.h>
#include <linux/slab.h>
#include <trace/events/syscalls.h>

#include <linux/version.h>
#include <linux/sched/task_stack.h>
#include <linux/compat.h>

#include "arch.h"
#include "klog.h" // IWYU pragma: keep
#include "hook/syscall_hook_manager.h"
#include "hook/tp_marker.h"
#include "feature/sucompat.h"
#include "hook/setuid_hook.h"
#include "hook/syscall_hook.h"
#include "hook/syscall_event_bridge.h"
#if defined(__riscv)
#include "hook/riscv64/syscall_regs.h"
#endif

#ifdef MODULE
static bool syscall_redirects_pinned_module;
#endif

bool ksu_syscall_redirect_pin_module(void)
{
#ifdef MODULE
    if (READ_ONCE(syscall_redirects_pinned_module))
        return true;
    if (!try_module_get(THIS_MODULE))
        return false;
    WRITE_ONCE(syscall_redirects_pinned_module, true);
    pr_info("hook_manager: hot unload disabled after first syscall redirect\n");
#endif
    return true;
}

#ifdef CONFIG_KRETPROBES

static struct kretprobe *init_kretprobe(const char *name, kretprobe_handler_t handler)
{
    struct kretprobe *rp = kzalloc(sizeof(struct kretprobe), GFP_KERNEL);
    if (!rp)
        return NULL;
    rp->kp.symbol_name = name;
    rp->handler = handler;
    rp->data_size = 0;
    rp->maxactive = 0;

    int ret = register_kretprobe(rp);
    pr_info("hook_manager: register_%s kretprobe: %d\n", name, ret);
    if (ret) {
        kfree(rp);
        return NULL;
    }

    return rp;
}

static void destroy_kretprobe(struct kretprobe **rp_ptr)
{
    struct kretprobe *rp = *rp_ptr;
    if (!rp)
        return;
    unregister_kretprobe(rp);
    synchronize_rcu();
    kfree(rp);
    *rp_ptr = NULL;
}

static int syscall_regfunc_handler(struct kretprobe_instance *ri, struct pt_regs *regs)
{
    unsigned long flags;
    ksu_tp_marker_lock(&flags);
    if (ksu_tp_marker_reg_count() < 1) {
        // while install our tracepoint, mark our processes
        ksu_mark_running_process_locked();
    } else if (ksu_tp_marker_reg_count() == 1) {
        // while other tracepoint first added, mark all processes
        ksu_mark_all_process();
    }
    ksu_tp_marker_inc_reg_count();
    ksu_tp_marker_unlock(&flags);
    return 0;
}

static int syscall_unregfunc_handler(struct kretprobe_instance *ri, struct pt_regs *regs)
{
    unsigned long flags;
    ksu_tp_marker_lock(&flags);
    ksu_tp_marker_dec_reg_count();
    if (ksu_tp_marker_reg_count() <= 0) {
        // while no tracepoint left, unmark all processes
        ksu_unmark_all_process();
    } else if (ksu_tp_marker_reg_count() == 1) {
        // while just our tracepoint left, unmark disallowed processes
        ksu_mark_running_process_locked();
    }
    ksu_tp_marker_unlock(&flags);
    return 0;
}

static struct kretprobe *syscall_regfunc_rp = NULL;
static struct kretprobe *syscall_unregfunc_rp = NULL;
#endif

#ifdef CONFIG_HAVE_SYSCALL_TRACEPOINTS
// sys_enter handler: redirect hooked syscalls to the dispatcher
static void ksu_sys_enter_handler(void *data, struct pt_regs *regs, long id)
{
#if defined(__x86_64__)
    if (unlikely(in_compat_syscall()))
#elif defined(__aarch64__) || defined(__riscv)
    if (unlikely(is_compat_task()))
#endif
        return;

    if (ksu_dispatcher_nr < 0)
        return;

    if (ksu_has_syscall_hook(id)) {
        struct pt_regs *current_regs = task_pt_regs(current);

        if (!ksu_syscall_redirect_pin_module())
            return;
#if defined(__x86_64__)
        // Stash the original syscall number in ax.
        // We use ax because it currently just holds -ENOSYS and is safe to overwrite.
        current_regs->ax = id;
        current_regs->orig_ax = ksu_dispatcher_nr;
#elif defined(__aarch64__)
        PT_REGS_ORIG_SYSCALL(current_regs) = id;
        current_regs->syscallno = ksu_dispatcher_nr;
#elif defined(__riscv)
        /* orig_a0 retains argument zero; a0 is the -ENOSYS return slot. */
        ksu_riscv_redirect_syscall(current_regs, id, ksu_dispatcher_nr);
#endif
    }
}
#endif

static bool ksu_syscall_hook_manager_started;

int __init ksu_syscall_hook_manager_init(void)
{
    int ret;
    int registered_nr[5];
    int registered_count = 0;

    pr_info("hook_manager: initialization started\n");

    if (!ksu_syscall_table || ksu_dispatcher_nr < 0) {
        pr_err("hook_manager: dispatcher unavailable\n");
        return -ENOENT;
    }

#ifdef CONFIG_KRETPROBES
    syscall_regfunc_rp = init_kretprobe(
        "syscall_regfunc", syscall_regfunc_handler);

    if (!syscall_regfunc_rp) {
        ret = -ENOENT;
        goto fail_probes;
    }

    syscall_unregfunc_rp = init_kretprobe(
        "syscall_unregfunc", syscall_unregfunc_handler);

    if (!syscall_unregfunc_rp) {
        ret = -ENOENT;
        goto fail_probes;
    }
#endif

#ifndef CONFIG_KSU_SUSFS
    ret = ksu_register_syscall_hook(
        __NR_setresuid, ksu_hook_setresuid);
    if (ret)
        goto fail_routes;
    registered_nr[registered_count++] = __NR_setresuid;
#endif

    ret = ksu_register_syscall_hook(
        __NR_execve, ksu_hook_execve);
    if (ret)
        goto fail_routes;
    registered_nr[registered_count++] = __NR_execve;

    ret = ksu_register_syscall_hook(
        __NR_execveat, ksu_hook_execveat);
    if (ret)
        goto fail_routes;
    registered_nr[registered_count++] = __NR_execveat;

    ret = ksu_register_syscall_hook(
        __NR_newfstatat, ksu_hook_newfstatat);
    if (ret)
        goto fail_routes;
    registered_nr[registered_count++] = __NR_newfstatat;

    ret = ksu_register_syscall_hook(
        __NR_faccessat, ksu_hook_faccessat);
    if (ret)
        goto fail_routes;
    registered_nr[registered_count++] = __NR_faccessat;

#ifdef CONFIG_HAVE_SYSCALL_TRACEPOINTS
    ret = register_trace_prio_sys_enter(
        ksu_sys_enter_handler, NULL, INT_MIN);

    if (ret) {
        pr_err("hook_manager: tracepoint failed: %d\n", ret);
        goto fail_routes;
    }

#ifndef CONFIG_KRETPROBES
    ksu_mark_running_process_locked();
#endif

    pr_info("hook_manager: tracepoint registered\n");
#else
    ret = -EOPNOTSUPP;
    goto fail_routes;
#endif

    ksu_setuid_hook_init();
    ksu_sucompat_init();

    ksu_syscall_hook_manager_started = true;

    return 0;

fail_routes:
    while (registered_count > 0)
        ksu_unregister_syscall_hook(
            registered_nr[--registered_count]);

#ifdef CONFIG_KRETPROBES
fail_probes:
    destroy_kretprobe(&syscall_unregfunc_rp);
    destroy_kretprobe(&syscall_regfunc_rp);
#endif

    pr_err("hook_manager: initialization failed: %d\n", ret);

    return ret;
}

void __exit ksu_syscall_hook_manager_exit(void)
{
    pr_info("hook_manager: cleanup started\n");

#ifdef CONFIG_HAVE_SYSCALL_TRACEPOINTS
    if (ksu_syscall_hook_manager_started) {
        unregister_trace_sys_enter(
            ksu_sys_enter_handler, NULL);
        tracepoint_synchronize_unregister();
    }
#endif

#ifdef CONFIG_KRETPROBES
    destroy_kretprobe(&syscall_regfunc_rp);
    destroy_kretprobe(&syscall_unregfunc_rp);
#endif

    if (ksu_syscall_hook_manager_started) {
#ifndef CONFIG_KSU_SUSFS
        ksu_unregister_syscall_hook(__NR_setresuid);
#endif
        ksu_unregister_syscall_hook(__NR_execve);
        ksu_unregister_syscall_hook(__NR_execveat);
        ksu_unregister_syscall_hook(__NR_newfstatat);
        ksu_unregister_syscall_hook(__NR_faccessat);
    }

    ksu_syscall_hook_exit();

    if (ksu_syscall_hook_manager_started) {
        ksu_sucompat_exit();
        ksu_setuid_hook_exit();
    }

    ksu_syscall_hook_manager_started = false;
}
