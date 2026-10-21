#ifdef __aarch64__

#include "../syscall_hook.h"

#include <linux/kallsyms.h>
#include <linux/mutex.h>
#include <asm/cacheflush.h>
#include "infra/symbol_resolver.h"
#include "../patch_memory.h"
#include "arch.h"
#include "klog.h" // IWYU pragma: keep

syscall_fn_t *ksu_syscall_table = NULL;
int ksu_dispatcher_nr = -1;

// Hook registration table — read with READ_ONCE from tracepoint/dispatcher
// context, written with WRITE_ONCE from init/exit context.
static ksu_syscall_hook_fn syscall_hooks[__NR_syscalls];

// Track all hooked syscall entries for restoration.
// Protected by hooked_entries_lock.
struct syscall_hook_entry {
    int nr;
    syscall_fn_t orig;
};

static DEFINE_MUTEX(hooked_entries_lock);
static struct syscall_hook_entry hooked_entries[16];
static int hooked_count = 0;

static int patch_syscall_table(int nr, syscall_fn_t fn)
{
    if (ksu_syscall_table == NULL)
        return -ENOENT;
    if (nr < 0 || nr >= __NR_syscalls)
        return -EINVAL;

    pr_info("patch syscall %d, 0x%lx -> 0x%lx\n", nr, (unsigned long)READ_ONCE(ksu_syscall_table[nr]),
            (unsigned long)fn);

    if (ksu_patch_text(&ksu_syscall_table[nr], &fn, sizeof(fn),
                       KSU_PATCH_TEXT_FLUSH_DCACHE)) {
        pr_err("patch syscall %d failed\n", nr);
        return -EIO;
    }

    if (READ_ONCE(ksu_syscall_table[nr]) != fn) {
        pr_err("patch syscall %d verification failed\n", nr);
        return -EIO;
    }

    return 0;
}

// Direct syscall table patching: overwrite syscall_table[nr] with fn,
// save original to *old, and record for restoration at module exit.
int ksu_syscall_table_hook_checked(int nr, syscall_fn_t fn,
                                          syscall_fn_t *old)
{
    syscall_fn_t orig;
    int i, ret;
    bool found = false;

    if (!ksu_syscall_table)
        return -ENOENT;

    if (nr < 0 || nr >= __NR_syscalls)
        return -EINVAL;

    mutex_lock(&hooked_entries_lock);

    orig = READ_ONCE(ksu_syscall_table[nr]);

    for (i = 0; i < hooked_count; i++) {
        if (hooked_entries[i].nr == nr) {
            found = true;
            break;
        }
    }

    if (found) {
        ret = -EEXIST;
        goto out;
    }

    if (hooked_count >= ARRAY_SIZE(hooked_entries)) {
        ret = -ENOSPC;
        goto out;
    }

    /*
     * Publish the original handler before installing the replacement.
     * Another CPU may enter the hook immediately after the table changes.
     */
    if (old)
        WRITE_ONCE(*old, orig);

    ret = patch_syscall_table(nr, fn);
    if (ret) {
        if (READ_ONCE(ksu_syscall_table[nr]) == orig)
            goto out;

        pr_err("syscall %d installation failed; attempting rollback\n",
               nr);

        patch_syscall_table(nr, orig);

        if (READ_ONCE(ksu_syscall_table[nr]) == orig)
            goto out;

        pr_err("syscall %d rollback unverified; tracking for restoration\n",
               nr);
    }

    hooked_entries[hooked_count].nr = nr;
    hooked_entries[hooked_count].orig = orig;
    hooked_count++;

out:
    mutex_unlock(&hooked_entries_lock);
    return ret;
}

void ksu_syscall_table_hook(int nr, syscall_fn_t fn, syscall_fn_t *old)
{
    int ret = ksu_syscall_table_hook_checked(nr, fn, old);

    if (ret)
        pr_err("cannot install syscall hook %d: %d\n", nr, ret);
}

// Restore syscall_table[nr] to its original value and remove from tracking list.
void ksu_syscall_table_unhook(int nr)
{
    int i;

    if (ksu_syscall_table == NULL)
        return;
    if (nr < 0 || nr >= __NR_syscalls)
        return;

    mutex_lock(&hooked_entries_lock);

    for (i = 0; i < hooked_count; i++) {
        if (hooked_entries[i].nr == nr) {
            patch_syscall_table(nr, hooked_entries[i].orig);

            if (READ_ONCE(ksu_syscall_table[nr]) !=
                hooked_entries[i].orig) {
                pr_err("failed to verify unhook of syscall %d; keeping tracking entry\n",
                       nr);
                mutex_unlock(&hooked_entries_lock);
                return;
            }
            // Remove entry by swapping with last
            hooked_entries[i] = hooked_entries[--hooked_count];
            mutex_unlock(&hooked_entries_lock);
            pr_info("unhooked syscall %d\n", nr);
            return;
        }
    }

    mutex_unlock(&hooked_entries_lock);
    pr_warn("syscall %d not found in hooked entries\n", nr);
}

static int __init ksu_find_ni_syscall_slots(int *out_slots, int max_slots)
{
    unsigned long ni_syscall;
    int i, count = 0;

    if (!ksu_syscall_table || max_slots <= 0)
        return 0;

    ni_syscall = (unsigned long)ksu_resolve_symbol_for_functable_hook("__arm64_sys_ni_syscall");

    pr_info("sys_ni_syscall: 0x%lx\n", ni_syscall);

    if (!ni_syscall)
        return 0;

    for (i = 0; i < __NR_syscalls && count < max_slots; i++) {
        if ((unsigned long)ksu_syscall_table[i] == ni_syscall) {
            out_slots[count++] = i;
            pr_info("ni_syscall %d: %d\n", count, i);
        }
    }

    return count;
}

// Unified dispatcher: reads original NR from x8, dispatches to handler.
// Validates that syscallno matches our dispatcher slot (i.e. we redirected it),
// otherwise it's a spurious call — return -ENOSYS.
static long __nocfi ksu_syscall_dispatcher(const struct pt_regs *regs)
{
    if (regs->syscallno != ksu_dispatcher_nr)
        return -ENOSYS;

    int orig_nr = (int)PT_REGS_ORIG_SYSCALL(regs);

    if (regs->syscallno == orig_nr)
        return -ENOSYS;

    // Restore registers to original state before dispatching
    ((struct pt_regs *)regs)->syscallno = orig_nr;
    PT_REGS_ORIG_SYSCALL((struct pt_regs *)regs) = orig_nr;

    if (likely(orig_nr >= 0 && orig_nr < __NR_syscalls)) {
        ksu_syscall_hook_fn fn = READ_ONCE(syscall_hooks[orig_nr]);
        if (likely(fn))
            return fn(orig_nr, regs);
    }

    return -ENOSYS;
}

// Register a handler into the dispatcher's routing table.
// Does not modify the syscall table — the dispatcher slot is shared by all hooks.
int ksu_register_syscall_hook(int nr, ksu_syscall_hook_fn fn)
{
    if (nr < 0 || nr >= __NR_syscalls)
        return -EINVAL;
    if (READ_ONCE(syscall_hooks[nr])) {
        pr_warn("syscall hook for nr=%d already registered, skip\n", nr);
        return -EEXIST;
    }
    WRITE_ONCE(syscall_hooks[nr], fn);
    pr_info("registered syscall hook for nr=%d\n", nr);
    return 0;
}

// Remove a handler from the dispatcher's routing table.
// The syscall table is not touched — only the dispatcher stops routing this nr.
void ksu_unregister_syscall_hook(int nr)
{
    if (nr < 0 || nr >= __NR_syscalls)
        return;
    WRITE_ONCE(syscall_hooks[nr], NULL);
    pr_info("unregistered syscall hook for nr=%d\n", nr);
}

bool ksu_has_syscall_hook(int nr)
{
    if (nr < 0 || nr >= __NR_syscalls)
        return false;
    return READ_ONCE(syscall_hooks[nr]) != NULL;
}

void __init ksu_syscall_hook_init(void)
{
    int ni_slot;

    memset(syscall_hooks, 0, sizeof(syscall_hooks));

    ksu_syscall_table = (syscall_fn_t *)ksu_resolve_symbol_for_functable_hook("sys_call_table");
    pr_info("sys_call_table=0x%lx", (unsigned long)ksu_syscall_table);

    if (!ksu_syscall_table)
        return;

    // Find one ni_syscall slot for the dispatcher
    if (ksu_find_ni_syscall_slots(&ni_slot, 1) < 1) {
        pr_err("failed to find ni_syscall slot for dispatcher\n");
        return;
    }

    if (ksu_syscall_table_hook_checked(
            ni_slot, (syscall_fn_t)ksu_syscall_dispatcher, NULL)) {
        pr_err("failed to install syscall dispatcher at slot %d\n", ni_slot);
        return;
    }

    WRITE_ONCE(ksu_dispatcher_nr, ni_slot);
    pr_info("dispatcher installed at slot %d\n", ni_slot);
}

void __exit ksu_syscall_hook_exit(void)
{
    int i;
    int pending = 0;

    if (!ksu_syscall_table) {
        pr_err("cannot restore syscall hooks: syscall table unavailable\n");
        return;
    }

    mutex_lock(&hooked_entries_lock);

    for (i = 0; i < hooked_count; i++) {
        int nr = hooked_entries[i].nr;
        syscall_fn_t orig = hooked_entries[i].orig;
        int ret;

        ret = patch_syscall_table(nr, orig);

        if (READ_ONCE(ksu_syscall_table[nr]) == orig)
            continue;

        pr_err("failed to restore syscall %d: %d\n", nr, ret);
        hooked_entries[pending++] = hooked_entries[i];
    }

    hooked_count = pending;
    mutex_unlock(&hooked_entries_lock);

    if (pending) {
        pr_err("%d syscall hooks remain unrestored; preserving state\n",
               pending);
        return;
    }

    memset(syscall_hooks, 0, sizeof(syscall_hooks));
    WRITE_ONCE(ksu_dispatcher_nr, -1);

    pr_info("all syscall hooks restored\n");
}

#endif /* __aarch64__ */
