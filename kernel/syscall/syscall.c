/*
 * Copyright (c) 2023 Institute of Parallel And Distributed Systems (IPADS), Shanghai Jiao Tong University (SJTU)
 * Licensed under the Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 *     http://license.coscl.org.cn/MulanPSL2
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY OR FIT FOR A PARTICULAR
 * PURPOSE.
 * See the Mulan PSL v2 for more details.
 */
#include <common/types.h>
#include <io/uart.h>
#include <mm/uaccess.h>
#include <mm/kmalloc.h>
#include <mm/mm.h>
#include <common/kprint.h>
#include <common/debug.h>
#include <common/lock.h>
#include <object/memory.h>
#include <object/thread.h>
#include <object/cap_group.h>
#include <object/recycle.h>
#include <object/object.h>
#include <object/irq.h>
#include <object/user_fault.h>
#include <sched/sched.h>
#include <ipc/connection.h>
#include <irq/timer.h>
#include <irq/irq.h>
#ifdef CHCORE_OH_TEE
#include <arch/trustzone/smc.h>
#include <arch/trustzone/tlogger.h>
#endif /* CHCORE_OH_TEE */
#include <common/poweroff.h>
#ifdef CHCORE_OH_TEE
#include <ipc/channel.h>
#endif /* CHCORE_OH_TEE */
#ifdef CHCORE_ARCH_X86_64
#include <arch/pci.h>
#endif /* CHCORE_ARCH_X86_64 */

#include "syscall_num.h"

#if ENABLE_HOOKING_SYSCALL == ON
void hook_syscall(long n)
{
    if ((n != SYS_putstr) && (n != SYS_getc) && (n != SYS_yield)
        && (n != SYS_handle_brk))
        kinfo("[SYSCALL TRACING] hook_syscall num: %ld\n", n);
}
#endif

/* Placeholder for system calls that are not implemented */
int sys_null_placeholder(void)
{
    kwarn("Invoke non-implemented syscall\n");
    return -EBADSYSCALL;
}

#if ENABLE_PRINT_LOCK == ON

struct lock global_print_lock = {0};
#endif

void sys_putstr(char *str, size_t len)
{
    if (check_user_addr_range((vaddr_t)str, len) != 0)
        return;

    if (is_tlogger_on()) {
        char log_buf[512];
        if (len >= 512) {
            return;
        }
        if (copy_from_user(log_buf, str, len)) {
            return;
        }
        (void)append_chcore_log(log_buf, len, false);
        return;
    }

#define PRINT_BUFSZ 64
    char buf[PRINT_BUFSZ];
    size_t copy_len;
    size_t i;
    int r;

    do {
        copy_len = (len > PRINT_BUFSZ) ? PRINT_BUFSZ : len;
        r = copy_from_user(buf, str, copy_len);
        if (r)
            return;

#if ENABLE_PRINT_LOCK == ON
        lock(&global_print_lock);
#endif
        for (i = 0; i < copy_len; ++i) {
            uart_send((unsigned int)buf[i]);
            if (graphic_putc)
                graphic_putc(buf[i]);
        }

#if ENABLE_PRINT_LOCK == ON
        unlock(&global_print_lock);
#endif
        len -= copy_len;
        str += copy_len;
    } while (len != 0);
}

char sys_getc(void)
{
    return nb_uart_recv();
}

/* Arch-specific declarations */
void arch_flush_cache(vaddr_t, size_t, int);
u64 plat_get_current_tick(void);

/* Helper system calls for user-level drivers to use. */
int sys_cache_flush(unsigned long start, long len, int op_type)
{
    arch_flush_cache(start, len, op_type);
    return 0;
}

unsigned long sys_get_current_tick(void)
{
    return plat_get_current_tick();
}

/* DELETE */
/* Syscalls for perfromance benchmark */
/* A debugging function which can be used for adding trace points in apps */
void sys_debug_log(long arg)
{
    kinfo("%s: %ld\n", __func__, arg);
}

void sys_perf_start(void)
{
    kdebug("Disable TIMER\n");
    plat_disable_timer();
}

void sys_perf_end(void)
{
    kdebug("Enable TIMER\n");
    plat_enable_timer();
}

void sys_perf_null(void)
{
}
/* DELETE */

void sys_get_pci_device(int class, u64 pci_dev_uaddr)
{
#if defined(CHCORE_ARCH_X86_64)
    arch_get_pci_device(class, pci_dev_uaddr);
#else
    kwarn("PCI is not supported on current architecture.\n");
#endif
}

void sys_poweroff(void)
{
    plat_poweroff();
}

const void *syscall_table[NR_SYSCALL] = {
    [0 ... NR_SYSCALL - 1] = sys_null_placeholder,

    /* Character */
    [SYS_putstr] = sys_putstr,
    [SYS_getc] = sys_getc,

    [SYS_tee_push_rdr_update_addr] = sys_tee_push_rdr_update_addr,
    [SYS_debug_rdr_logitem] = sys_debug_rdr_logitem,

    /* PMO */
    /* - single */
    [SYS_create_pmo] = sys_create_pmo,
    [SYS_create_device_pmo] = sys_create_device_pmo,
    [SYS_map_pmo] = sys_map_pmo,
    [SYS_unmap_pmo] = sys_unmap_pmo,
    [SYS_write_pmo] = sys_write_pmo,
    [SYS_read_pmo] = sys_read_pmo,
#ifdef CHCORE_OH_TEE
    [SYS_create_ns_pmo] = sys_create_ns_pmo,
    [SYS_destroy_ns_pmo] = sys_destroy_ns_pmo,
    [SYS_create_tee_shared_pmo] = sys_create_tee_shared_pmo,
    [SYS_transfer_pmo_owner] = sys_transfer_pmo_owner,
#endif /* CHCORE_OH_TEE */

    /* - address translation */
    [SYS_get_phys_addr] = sys_get_phys_addr,

    /* Capability */
    [SYS_revoke_cap] = sys_revoke_cap,
    [SYS_transfer_caps] = sys_transfer_caps,

    /* Multitask */
    /* - create & exit */
    [SYS_create_cap_group] = sys_create_cap_group,
    [SYS_exit_group] = sys_exit_group,
    [SYS_create_thread] = sys_create_thread,
    [SYS_thread_exit] = sys_thread_exit,
    [SYS_kill_group] = sys_kill_group,
#ifdef CHCORE_OH_TEE
    [SYS_get_thread_id] = sys_get_thread_id,
    [SYS_terminate_thread] = sys_terminate_thread,
    [SYS_disable_local_irq] = sys_disable_local_irq,
    [SYS_enable_local_irq] = sys_enable_local_irq,
#endif /* CHCORE_OH_TEE */
    /* - recycle */
    [SYS_register_recycle] = sys_register_recycle,
    [SYS_cap_group_recycle] = sys_cap_group_recycle,
    [SYS_ipc_close_connection] = sys_ipc_close_connection,
    /* - schedule */
    [SYS_yield] = sys_yield,
    [SYS_set_affinity] = sys_set_affinity,
    [SYS_get_affinity] = sys_get_affinity,
    [SYS_set_prio] = sys_set_prio,
    [SYS_get_prio] = sys_get_prio,
    /* IPC */
    /* - procedure call */
    [SYS_register_server] = sys_register_server,
    [SYS_register_client] = sys_register_client,
    [SYS_ipc_register_cb_return] = sys_ipc_register_cb_return,
    [SYS_ipc_call] = sys_ipc_call,
    [SYS_ipc_return] = sys_ipc_return,
    [SYS_ipc_exit_routine_return] = sys_ipc_exit_routine_return,
    /* - notification */
    [SYS_create_notifc] = sys_create_notifc,
    [SYS_wait] = sys_wait,
    [SYS_notify] = sys_notify,
#ifdef CHCORE_OH_TEE
    /* - oh-tee-ipc */
    [SYS_tee_msg_create_msg_hdl] = sys_tee_msg_create_msg_hdl,
    [SYS_tee_msg_create_channel] = sys_tee_msg_create_channel,
    [SYS_tee_msg_receive] = sys_tee_msg_receive,
    [SYS_tee_msg_call] = sys_tee_msg_call,
    [SYS_tee_msg_reply] = sys_tee_msg_reply,
    [SYS_tee_msg_notify] = sys_tee_msg_notify,
    [SYS_tee_msg_stop_channel] = sys_tee_msg_stop_channel,
#endif /* CHCORE_OH_TEE */

    /* Exception */
    /* - irq */
    [SYS_irq_register] = sys_irq_register,
    [SYS_irq_wait] = sys_irq_wait,
    [SYS_irq_ack] = sys_irq_ack,

#ifdef CHCORE_ENABLE_FMAP
    /* - page fault */
    [SYS_user_fault_register] = sys_user_fault_register,
    [SYS_user_fault_map] = sys_user_fault_map,
#endif

    /* Hardware Access (Privileged Instruction) */
    /* - cache */
    [SYS_cache_flush] = sys_cache_flush,
    /* - timer */
    [SYS_get_current_tick] = sys_get_current_tick,

    /* POSIX */
    /* - time */
    [SYS_clock_gettime] = sys_clock_gettime,
    [SYS_clock_nanosleep] = sys_clock_nanosleep,
    /* - memory */
    [SYS_handle_brk] = sys_handle_brk,
    [SYS_handle_mprotect] = sys_handle_mprotect,

    /* Debug */
    [SYS_debug_log] = sys_debug_log,
    [SYS_top] = sys_top,
    [SYS_get_free_mem_size] = sys_get_free_mem_size,

    /* Performance Benchmark */
    [SYS_perf_start] = sys_perf_start,
    [SYS_perf_end] = sys_perf_end,
    [SYS_perf_null] = sys_perf_null,

    [SYS_get_pci_device] = sys_get_pci_device,
    [SYS_poweroff] = sys_poweroff,

#ifdef CHCORE_OH_TEE
    /* TrustZone */
    [SYS_tee_wait_switch_req] = sys_tee_wait_switch_req,
    [SYS_tee_switch_req] = sys_tee_switch_req,
    [SYS_tee_create_ns_pmo] = sys_tee_create_ns_pmo,
    [SYS_tee_pull_kernel_var] = sys_tee_pull_kernel_var,
#endif /* CHCORE_OH_TEE */
};
