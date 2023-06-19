/*
 * Copyright (c) 2023 Institute of Parallel And Distributed Systems (IPADS)
 * Licensed under the Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 *     http://license.coscl.org.cn/MulanPSL2
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY OR FIT FOR A PARTICULAR
 * PURPOSE.
 * See the Mulan PSL v2 for more details.
 */
#ifndef KERNEL_SYSCALL_SYSCALL_NUM_H
#define KERNEL_SYSCALL_SYSCALL_NUM_H

#define NR_SYSCALL 256

/* Character */
#define SYS_putstr 0
#define SYS_getc   1

/* PMO */
/* - single */
#define SYS_create_pmo        10
#define SYS_create_device_pmo 11
#define SYS_map_pmo           12
#define SYS_unmap_pmo         13
#define SYS_write_pmo         14
#define SYS_read_pmo          15
#ifdef CHCORE_OH_TEE
#define SYS_create_ns_pmo         16
#define SYS_destroy_ns_pmo        17
#define SYS_create_tee_shared_pmo 19
#endif /* CHCORE_OH_TEE */

/* - address translation */
#define SYS_get_pmo_paddr 30
#define SYS_get_phys_addr 31

/* Capability */
#define SYS_revoke_cap    18
#define SYS_transfer_caps 62

/* Multitask */
/* - create & exit */
#define SYS_create_cap_group 80
#define SYS_exit_group       81
#define SYS_create_thread    82
#define SYS_thread_exit      83
#ifdef CHCORE_OH_TEE
#define SYS_get_thread_id    84
#define SYS_terminate_thread 85
#endif /* CHCORE_OH_TEE */
/* - recycle */
#define SYS_register_recycle     90
#define SYS_cap_group_recycle    91
#define SYS_ipc_close_connection 92
/* - schedule */
#define SYS_yield        100
#define SYS_set_affinity 101
#define SYS_get_affinity 102
#define SYS_set_priority 103

/* IPC */
/* - procedure call */
#define SYS_register_server         120
#define SYS_register_client         121
#define SYS_ipc_register_cb_return  122
#define SYS_ipc_call                123
#define SYS_ipc_return              124
#define SYS_ipc_exit_routine_return 125
/* - notification */
#define SYS_create_notifc 130
#define SYS_wait          131
#define SYS_notify        132

#ifdef CHCORE_OH_TEE
/* - oh-tee-ipc */
#define SYS_tee_msg_create_msg_hdl 140
#define SYS_tee_msg_create_channel 141
#define SYS_tee_msg_stop_channel   142

#define SYS_tee_msg_receive 143
#define SYS_tee_msg_call    144
#define SYS_tee_msg_reply   145
#define SYS_tee_msg_notify  146
#endif /* CHCORE_OH_TEE */

/* Exception */
/* - irq */
#define SYS_irq_register      150
#define SYS_irq_wait          151
#define SYS_irq_ack           152
#define SYS_disable_irq       153
#define SYS_enable_irq        154
#define SYS_disable_irqno     155
#define SYS_enable_irqno      156
#define SYS_clear_irqno       157
#define SYS_cache_config      158
#define SYS_disable_local_irq 159
#define SYS_enable_local_irq  160
/* - page fault */
#define SYS_user_fault_register 165
#define SYS_user_fault_map      166

/* Hardware Access (Privileged Instruction) */
/* - cache */
#define SYS_cache_flush 180
/* - timer */
#define SYS_get_current_tick 185

/* POSIX */
/* - time */
#define SYS_clock_gettime   200
#define SYS_clock_nanosleep 201
/* - memory */
#define SYS_handle_brk      210
#define SYS_handle_mprotect 213

/* Debug */
#define SYS_debug_log         220
#define SYS_top               221
#define SYS_get_free_mem_size 222
#define SYS_get_mem_usage_msg 223

/* Performance Benchmark */
#define SYS_perf_start 230
#define SYS_perf_end   231
#define SYS_perf_null  232

/* Get PCI devcie information. */
#define SYS_get_pci_device 233

/* poweroff */
#define SYS_poweroff 234

/* Virtualization */
#define SYS_virt_dispatch 240

/* TrustZone */
#define SYS_tee_wait_switch_req 250
#define SYS_tee_switch_req      251
#define SYS_tee_create_ns_pmo   252
#define SYS_tee_pull_kernel_var 253
#endif /* KERNEL_SYSCALL_SYSCALL_NUM_H */
