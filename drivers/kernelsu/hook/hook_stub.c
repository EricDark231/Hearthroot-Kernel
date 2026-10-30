// SPDX-License-Identifier: GPL-2.0-only
/*
 * Bridge stubs for the inline syscall-hook architecture.
 *
 * The in-tree SUSFS bridge does not compile hook/<arch>/syscall_hook.c,
 * hook/lsm_hook.c, hook/syscall_hook_manager.c, hook/syscall_event_bridge.c
 * and hook/tp_marker.c, but upstream call sites in core/init.c keep calling
 * their entry points. These no-ops keep those files verbatim and linkable.
 *
 * ksu_app_profile_init() upstream only probes the Pixel-10 seccomp backport
 * (scan_call_to on seccomp_filter_release); this tree's policy/app_profile.c
 * takes the >= 6.6 branch instead, so the probe is not needed here.
 */
#include <linux/errno.h>
#include <linux/init.h>
#include <linux/types.h>

void __init ksu_syscall_hook_init(void);
void __init ksu_lsm_hook_init(void);
void __exit ksu_lsm_hook_exit(void);
void __init ksu_syscall_hook_manager_init(void);
void __exit ksu_syscall_hook_manager_exit(void);
void __init ksu_app_profile_init(void);

void __init ksu_syscall_hook_init(void)
{
}

void __init ksu_lsm_hook_init(void)
{
}

void __exit ksu_lsm_hook_exit(void)
{
}

void __init ksu_syscall_hook_manager_init(void)
{
}

void __exit ksu_syscall_hook_manager_exit(void)
{
}

void __init ksu_app_profile_init(void)
{
}

/*
 * The v4.3.0 manage_mark supercall reaches into the inline-hook task
 * marking (hook/tp_marker.c). That architecture is not compiled here -
 * syscall interception lives in the kernel tree's ksu_handle_* call sites,
 * which have no notion of task marks - so report absence instead of
 * pretending to mark: the manager treats the feature as unsupported.
 */
int ksu_get_task_mark(pid_t pid)
{
    return 0; /* unmarked */
}

int ksu_set_task_mark(pid_t pid, bool mark)
{
    return -EOPNOTSUPP;
}

void ksu_mark_all_process(void)
{
}

void ksu_unmark_all_process(void)
{
}

void ksu_mark_running_process(void)
{
}
