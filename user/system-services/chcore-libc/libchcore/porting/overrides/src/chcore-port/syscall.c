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

#include <chcore/syscall.h>
#include <chcore/defs.h>
#include <chcore/type.h>
#include <chcore/proc.h>
#include <syscall_arch.h>
#include <raw_syscall.h>
#include <errno.h>

#include <stdio.h>

void usys_putstr(vaddr_t buffer, size_t size)
{
    chcore_syscall2(CHCORE_SYS_putstr, buffer, size);
}

char usys_getc(void)
{
    return chcore_syscall0(CHCORE_SYS_getc);
}

int usys_tee_push_rdr_update_addr(paddr_t addr, size_t size, bool is_cache_mem,
                                  char *chip_type_buff, size_t buff_len)
{
    return chcore_syscall5(CHCORE_SYS_tee_push_rdr_update_addr,
                           addr,
                           size,
                           is_cache_mem,
                           (long)chip_type_buff,
                           buff_len);
}

int usys_debug_rdr_logitem(char *str, size_t str_len)
{
    return chcore_syscall2(CHCORE_SYS_debug_rdr_logitem, (long)str, str_len);
}

_Noreturn void usys_exit(unsigned long ret)
{
    chcore_syscall1(CHCORE_SYS_thread_exit, ret);
    __builtin_unreachable();
}

void usys_yield(void)
{
    chcore_syscall0(CHCORE_SYS_yield);
}

cap_t usys_create_device_pmo(unsigned long paddr, unsigned long size)
{
    return chcore_syscall2(CHCORE_SYS_create_device_pmo, paddr, size);
}

cap_t usys_create_pmo(unsigned long size, unsigned long type)
{
    return chcore_syscall2(CHCORE_SYS_create_pmo, size, type);
}

int usys_map_pmo(cap_t cap_group_cap, cap_t pmo_cap, unsigned long addr,
                 unsigned long rights)
{
    return chcore_syscall5(CHCORE_SYS_map_pmo,
                           cap_group_cap,
                           pmo_cap,
                           addr,
                           rights,
                           -1 /* pmo size */);
}

int usys_unmap_pmo(cap_t cap_group_cap, cap_t pmo_cap, unsigned long addr)
{
    return chcore_syscall3(CHCORE_SYS_unmap_pmo, cap_group_cap, pmo_cap, addr);
}

int usys_revoke_cap(cap_t obj_cap, bool revoke_copy)
{
    return chcore_syscall2(CHCORE_SYS_revoke_cap, obj_cap, revoke_copy);
}

int usys_set_affinity(cap_t thread_cap, s32 aff)
{
    return chcore_syscall2(
        CHCORE_SYS_set_affinity, thread_cap, (unsigned long)aff);
}

s32 usys_get_affinity(cap_t thread_cap)
{
    return chcore_syscall1(CHCORE_SYS_get_affinity, thread_cap);
}

int usys_get_prio(cap_t thread_cap)
{
    return chcore_syscall1(CHCORE_SYS_get_prio, thread_cap);
}

int usys_set_prio(cap_t thread_cap, int prio)
{
    return chcore_syscall2(CHCORE_SYS_set_prio, thread_cap, prio);
}

int usys_get_phys_addr(void *vaddr, unsigned long *paddr)
{
    return chcore_syscall2(
        CHCORE_SYS_get_phys_addr, (unsigned long)vaddr, (unsigned long)paddr);
}

cap_t usys_create_thread(unsigned long thread_args_p)
{
    return chcore_syscall1(CHCORE_SYS_create_thread, thread_args_p);
}

cap_t usys_create_cap_group(unsigned long cap_group_args_p)
{
    return chcore_syscall1(CHCORE_SYS_create_cap_group, cap_group_args_p);
}

int usys_kill_group(int proc_cap)
{
    chcore_syscall1(CHCORE_SYS_kill_group, proc_cap);
}

int usys_register_server(unsigned long callback, cap_t register_thread_cap,
                         unsigned long destructor)
{
    return chcore_syscall3(
        CHCORE_SYS_register_server, callback, register_thread_cap, destructor);
}

cap_t usys_register_client(cap_t server_cap, unsigned long vm_config_ptr)
{
    return chcore_syscall2(
        CHCORE_SYS_register_client, server_cap, vm_config_ptr);
}

int usys_ipc_call(cap_t conn_cap, unsigned long ipc_msg_ptr,
                  unsigned int cap_num)
{
    return chcore_syscall3(CHCORE_SYS_ipc_call, conn_cap, ipc_msg_ptr, cap_num);
}

_Noreturn void usys_ipc_return(unsigned long ret, unsigned long cap_num)
{
    chcore_syscall2(CHCORE_SYS_ipc_return, ret, cap_num);
    __builtin_unreachable();
}

void usys_ipc_register_cb_return(cap_t server_thread_cap,
                                 unsigned long server_thread_exit_routine,
                                 unsigned long server_shm_addr)
{
    chcore_syscall3(CHCORE_SYS_ipc_register_cb_return,
                    server_thread_cap,
                    server_thread_exit_routine,
                    server_shm_addr);
}

_Noreturn void usys_ipc_exit_routine_return(void)
{
    chcore_syscall0(CHCORE_SYS_ipc_exit_routine_return);
    __builtin_unreachable();
}

void usys_debug_log(long arg)
{
    chcore_syscall1(CHCORE_SYS_debug_log, arg);
}

int usys_write_pmo(cap_t cap, unsigned long offset, void *buf,
                   unsigned long size)
{
    return chcore_syscall4(
        CHCORE_SYS_write_pmo, cap, offset, (unsigned long)buf, size);
}

int usys_read_pmo(cap_t cap, unsigned long offset, void *buf,
                  unsigned long size)
{
    return chcore_syscall4(
        CHCORE_SYS_read_pmo, cap, offset, (unsigned long)buf, size);
}

int usys_transfer_caps(cap_t cap_group, cap_t *src_caps, int nr,
                       cap_t *dst_caps)
{
    return chcore_syscall4(CHCORE_SYS_transfer_caps,
                           cap_group,
                           (unsigned long)src_caps,
                           (unsigned long)nr,
                           (unsigned long)dst_caps);
}

void usys_perf_start(void)
{
    chcore_syscall0(CHCORE_SYS_perf_start);
}

void usys_perf_end(void)
{
    chcore_syscall0(CHCORE_SYS_perf_end);
}

void usys_perf_null(void)
{
    chcore_syscall0(CHCORE_SYS_perf_null);
}

void usys_top(void)
{
    chcore_syscall0(CHCORE_SYS_top);
}

int usys_user_fault_register(cap_t notific_cap, vaddr_t msg_buffer)
{
    return chcore_syscall2(
        CHCORE_SYS_user_fault_register, notific_cap, msg_buffer);
}

int usys_user_fault_map(badge_t client_badge, vaddr_t fault_va,
                        vaddr_t remap_va, bool copy, vmr_prop_t perm)
{
    return chcore_syscall5(
        CHCORE_SYS_user_fault_map, client_badge, fault_va, remap_va, copy, perm);
}

int usys_map_pmo_with_length(cap_t pmo_cap, vaddr_t addr, unsigned long perm,
                             size_t length)
{
    return chcore_syscall5(
        CHCORE_SYS_map_pmo, SELF_CAP, pmo_cap, addr, perm, length);
}

cap_t usys_irq_register(int irq)
{
    return chcore_syscall1(CHCORE_SYS_irq_register, irq);
}

int usys_irq_wait(cap_t irq_cap, bool is_block)
{
    return chcore_syscall2(CHCORE_SYS_irq_wait, irq_cap, is_block);
}

int usys_irq_ack(cap_t irq_cap)
{
    return chcore_syscall1(CHCORE_SYS_irq_ack, irq_cap);
}

void usys_cache_config(unsigned long option)
{
    chcore_syscall1(CHCORE_SYS_cache_config, option);
}

cap_t usys_create_notifc(void)
{
    return chcore_syscall0(CHCORE_SYS_create_notifc);
}

int usys_wait(cap_t notifc_cap, bool is_block, void *timeout)
{
    return chcore_syscall3(
        CHCORE_SYS_wait, notifc_cap, is_block, (unsigned long)timeout);
}

int usys_notify(cap_t notifc_cap)
{
    int ret;

    do {
        ret = chcore_syscall1(CHCORE_SYS_notify, notifc_cap);

        if (ret == -EAGAIN) {
            // printf("%s retry\n", __func__);
            usys_yield();
        }
    } while (ret == -EAGAIN);
    return ret;
}

#ifdef CHCORE_OH_TEE

cap_t usys_create_ns_pmo(cap_t cap_group, unsigned long paddr,
                         unsigned long size)
{
    return chcore_syscall3(CHCORE_SYS_create_ns_pmo, cap_group, paddr, size);
}

int usys_destroy_ns_pmo(cap_t cap_group, cap_t pmo)
{
    return chcore_syscall2(CHCORE_SYS_destroy_ns_pmo, cap_group, pmo);
}

int usys_transfer_pmo_owner(cap_t pmo, cap_t cap_group)
{
    return chcore_syscall2(CHCORE_SYS_transfer_pmo_owner, pmo, cap_group);
}

cap_t usys_create_tee_shared_pmo(cap_t cap_group, void *uuid,
                                 unsigned long size, cap_t *self_cap)
{
    return chcore_syscall4(CHCORE_SYS_create_tee_shared_pmo,
                           cap_group,
                           (unsigned long)uuid,
                           size,
                           (unsigned long)self_cap);
}

cap_t usys_get_thread_id(cap_t thread_cap)
{
    return chcore_syscall1(CHCORE_SYS_get_thread_id, thread_cap);
}

int usys_terminate_thread(cap_t thread_cap)
{
    return chcore_syscall1(CHCORE_SYS_terminate_thread, thread_cap);
}

cap_t usys_tee_msg_create_msg_hdl(void)
{
    return chcore_syscall0(CHCORE_SYS_tee_msg_create_msg_hdl);
}

cap_t usys_tee_msg_create_channel(void)
{
    return chcore_syscall0(CHCORE_SYS_tee_msg_create_channel);
}

int usys_tee_msg_stop_channel(cap_t channel_cap)
{
    return chcore_syscall1(CHCORE_SYS_tee_msg_stop_channel, channel_cap);
}

int usys_tee_msg_receive(cap_t channel_cap, void *recv_buf, size_t recv_len,
                         cap_t msg_hdl_cap, void *info, int timeout)
{
    return chcore_syscall6(CHCORE_SYS_tee_msg_receive,
                           channel_cap,
                           (unsigned long)recv_buf,
                           recv_len,
                           msg_hdl_cap,
                           (unsigned long)info,
                           timeout);
}
int usys_tee_msg_call(cap_t channel_cap, void *send_buf, size_t send_len,
                      void *recv_buf, size_t recv_len, void *timeout)
{
    return chcore_syscall6(CHCORE_SYS_tee_msg_call,
                           channel_cap,
                           (unsigned long)send_buf,
                           send_len,
                           (unsigned long)recv_buf,
                           recv_len,
                           (unsigned long)timeout);
}
int usys_tee_msg_reply(cap_t msg_hdl_cap, void *reply_buf, size_t reply_len)
{
    return chcore_syscall3(CHCORE_SYS_tee_msg_reply,
                           msg_hdl_cap,
                           (unsigned long)reply_buf,
                           reply_len);
}

int usys_tee_msg_notify(cap_t channel_cap, void *send_buf, size_t send_len)
{
    return chcore_syscall3(CHCORE_SYS_tee_msg_notify,
                           channel_cap,
                           (unsigned long)send_buf,
                           send_len);
}

#endif /* CHCORE_OH_TEE */

/* Only used for recycle process */
int usys_register_recycle_thread(cap_t cap, unsigned long buffer)
{
    return chcore_syscall2(CHCORE_SYS_register_recycle, cap, buffer);
}

int usys_cap_group_recycle(cap_t cap)
{
    return chcore_syscall1(CHCORE_SYS_cap_group_recycle, cap);
}

int usys_ipc_close_connection(cap_t cap)
{
    return chcore_syscall1(CHCORE_SYS_ipc_close_connection, cap);
}

/* Get the size of free memory */
unsigned long usys_get_free_mem_size(void)
{
    return chcore_syscall0(CHCORE_SYS_get_free_mem_size);
}

void usys_get_mem_usage_msg(void)
{
    chcore_syscall0(CHCORE_SYS_get_mem_usage_msg);
}

int usys_cache_flush(unsigned long start, unsigned long size, int op_type)
{
    return chcore_syscall3(CHCORE_SYS_cache_flush, start, size, op_type);
}

unsigned long usys_get_current_tick(void)
{
    return chcore_syscall0(CHCORE_SYS_get_current_tick);
}

void usys_get_pci_device(int dev_class, unsigned long uaddr)
{
    chcore_syscall2(CHCORE_SYS_get_pci_device, dev_class, uaddr);
}

int usys_tee_wait_switch_req(struct smc_registers *regs)
{
    return chcore_syscall1(CHCORE_SYS_tee_wait_switch_req, (long)regs);
}

int usys_tee_switch_req(struct smc_registers *regs)
{
    return chcore_syscall1(CHCORE_SYS_tee_switch_req, (long)regs);
}

int usys_tee_create_ns_pmo(unsigned long paddr, unsigned long size)
{
    return chcore_syscall2(CHCORE_SYS_tee_create_ns_pmo, paddr, size);
}

int usys_tee_pull_kernel_var(unsigned long cmd_buf_addr_buf)
{
    return chcore_syscall1(CHCORE_SYS_tee_pull_kernel_var, cmd_buf_addr_buf);
}

void usys_disable_local_irq(void)
{
    chcore_syscall0(CHCORE_SYS_disable_local_irq);
}

void usys_enable_local_irq(void)
{
    chcore_syscall0(CHCORE_SYS_enable_local_irq);
}

void usys_poweroff(void)
{
    chcore_syscall0(CHCORE_SYS_poweroff);
}
