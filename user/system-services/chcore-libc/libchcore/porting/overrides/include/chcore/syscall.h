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

#pragma once

#include <chcore/defs.h>
#include <chcore/type.h>
#include <stdio.h>
#include <chcore/memory.h>

#ifdef __cplusplus
extern "C" {
#endif

void usys_putstr(vaddr_t buffer, size_t size);
char usys_getc(void);

int usys_tee_push_rdr_update_addr(paddr_t addr, size_t size, bool is_cache_mem,
                                  char *chip_type_buff, size_t buff_len);
int usys_debug_rdr_logitem(char *str, size_t str_len);

int usys_set_priority(cap_t thread_cap, unsigned int prio);
int usys_get_pmo_paddr(cap_t pmo_cap, unsigned long *buf);
cap_t usys_create_device_pmo(unsigned long paddr, unsigned long size);
cap_t usys_create_pmo(unsigned long size, unsigned long type);
int usys_map_pmo(cap_t cap_group_cap, cap_t pmo_cap, unsigned long addr,
                 unsigned long perm);
int usys_unmap_pmo(cap_t cap_group_cap, cap_t pmo_cap, unsigned long addr);
int usys_write_pmo(cap_t pmo_cap, unsigned long offset, void *buf,
                   unsigned long size);
int usys_read_pmo(cap_t cap, unsigned long offset, void *buf,
                  unsigned long size);
int usys_get_phys_addr(void *vaddr, unsigned long *paddr);

int usys_revoke_cap(cap_t obj_cap, bool revoke_copy);
int usys_transfer_caps(cap_t dest_group_cap, cap_t *src_caps, int nr_caps,
                       cap_t *dst_caps);

_Noreturn void usys_exit(unsigned long ret);
void usys_yield(void);

cap_t usys_create_thread(unsigned long thread_args_p);
cap_t usys_create_cap_group(unsigned long cap_group_args_p);
int usys_kill_group(int proc_cap);
int usys_register_server(unsigned long ipc_handler, cap_t reigster_cb_cap,
                         unsigned long destructor);
cap_t usys_register_client(cap_t server_cap, unsigned long vm_config_ptr);
int usys_ipc_call(cap_t conn_cap, unsigned long ipc_msg_ptr,
                  unsigned int cap_num);
_Noreturn void usys_ipc_return(unsigned long ret, unsigned long cap_num);
void usys_ipc_register_cb_return(cap_t server_thread_cap,
                                 unsigned long server_thread_exit_routine,
                                 unsigned long server_shm_addr);
_Noreturn void usys_ipc_exit_routine_return(void);
void usys_debug_log(long arg);
int usys_set_affinity(cap_t thread_cap, s32 aff);
s32 usys_get_affinity(cap_t thread_cap);

unsigned long usys_get_free_mem_size(void);
void usys_get_mem_usage_msg(void);
void usys_perf_start(void);
void usys_perf_end(void);
void usys_perf_null(void);
void usys_top(void);

int usys_user_fault_register(cap_t notific_cap, vaddr_t msg_buffer);
int usys_user_fault_map(badge_t client_badge, vaddr_t fault_va,
                        vaddr_t remap_va, bool copy, vmr_prop_t perm);
int usys_map_pmo_with_length(cap_t pmo_cap, vaddr_t addr, unsigned long perm,
                             size_t length);

cap_t usys_irq_register(int irq);
int usys_irq_wait(cap_t irq_cap, bool is_block);
int usys_irq_ack(cap_t irq_cap);
cap_t usys_create_notifc(void);
int usys_wait(cap_t notifc_cap, bool is_block, void *timeout);
int usys_notify(cap_t notifc_cap);
void usys_cache_config(unsigned long option);

#ifdef CHCORE_OH_TEE
cap_t usys_create_ns_pmo(cap_t cap_group, unsigned long paddr,
                         unsigned long size);
int usys_destroy_ns_pmo(cap_t cap_group, cap_t pmo);
int usys_transfer_pmo_owner(cap_t pmo, cap_t cap_group);
cap_t usys_create_tee_shared_pmo(cap_t cap_group, void *uuid,
                                 unsigned long size, cap_t *self_cap);
cap_t usys_get_thread_id(cap_t thread_cap);
int usys_terminate_thread(cap_t thread_cap);
cap_t usys_tee_msg_create_msg_hdl(void);
cap_t usys_tee_msg_create_channel(void);
int usys_tee_msg_stop_channel(cap_t channel_cap);
int usys_tee_msg_receive(cap_t channel_cap, void *recv_buf, size_t recv_len,
                         cap_t msg_hdl_cap, void *info, int timeout);
int usys_tee_msg_call(cap_t channel_cap, void *send_buf, size_t send_len,
                      void *recv_buf, size_t recv_len, void *timeout);
int usys_tee_msg_reply(cap_t msg_hdl_cap, void *reply_buf, size_t reply_len);
int usys_tee_msg_notify(cap_t channel_cap, void *send_buf, size_t send_len);
#endif /* CHCORE_OH_TEE */

int usys_register_recycle_thread(cap_t cap, unsigned long buffer);
int usys_ipc_close_connection(cap_t cap);

int usys_cache_flush(unsigned long start, unsigned long size, int op_type);
unsigned long usys_get_current_tick(void);

void usys_get_pci_device(int dev_class, unsigned long uaddr);

struct smc_registers {
    unsigned long x0;
    unsigned long x1;
    unsigned long x2;
    unsigned long x3;
    unsigned long x4;
};
enum tz_switch_req {
    TZ_SWITCH_REQ_ENTRY_DONE,
    TZ_SWITCH_REQ_STD_REQUEST,
    TZ_SWITCH_REQ_STD_RESPONSE,
    TZ_SWITCH_REQ_NR
};
int usys_tee_wait_switch_req(struct smc_registers *regs);
int usys_tee_switch_req(struct smc_registers *regs);
int usys_tee_create_ns_pmo(unsigned long paddr, unsigned long size);
int usys_tee_pull_kernel_var(unsigned long cmd_buf_addr_buf);
void usys_disable_local_irq(void);
void usys_enable_local_irq(void);
void usys_poweroff(void);

#ifdef __cplusplus
}
#endif
