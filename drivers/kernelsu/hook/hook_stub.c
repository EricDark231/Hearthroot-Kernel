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
#include <linux/init.h>

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
