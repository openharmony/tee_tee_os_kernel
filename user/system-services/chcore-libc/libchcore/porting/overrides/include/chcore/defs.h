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

/* Use -1 instead of 0 (NULL) since 0 is used as the ending mark of envp */
#define ENVP_NONE_PMOS (-1)
#define ENVP_NONE_CAPS (-1)

/* ChCore custom auxiliary vector types */
#define AT_CHCORE_PMO_CNT      0xCC00
#define AT_CHCORE_PMO_MAP_ADDR 0xCC01
#define AT_CHCORE_CAP_CNT      0xCC02
#define AT_CHCORE_CAP_VAL      0xCC03

/* Used for usys_fs_load_cpio */
/* wh_cpio */
#define FSM_CPIO     0
#define TMPFS_CPIO   1
#define RAMDISK_CPIO 2
/* op */
#define QUERY_SIZE 0
#define LOAD_CPIO  1

/*
 * Used as a token which is added at the beginnig of the path name
 * during booting a system server. E.g., "kernel-server/fsm.srv"
 *
 * This is simply for preventing users unintentionally
 * run system servers in Shell. That is meaningless.
 */
#define KERNEL_SERVER "kernel-server"

#define NO_AFF       (-1)
#define NO_ARG       (0)
#define PASSIVE_PRIO (-1)

/* cache operations */
#define CACHE_CLEAN         1
#define CACHE_INVALIDATE    2
#define CACHE_CLEAN_AND_INV 3
#define SYNC_IDCACHE        4

/* virtual memory rights */
#define VM_READ   (1 << 0)
#define VM_WRITE  (1 << 1)
#define VM_EXEC   (1 << 2)
#define VM_FORBID (0)

/* PMO types */
#define PMO_ANONYM       0
#define PMO_DATA         1
#define PMO_FILE         2
#define PMO_SHM          3
#define PMO_DATA_NOCACHE 6
#define PMO_FORBID       10

/* a thread's own cap_group */
#define SELF_CAP 0

#define DEFAULT_PRIO 10


#if __SIZEOF_POINTER__ == 4
#define CHILD_THREAD_STACK_BASE (0x50800000UL)
#define CHILD_THREAD_STACK_SIZE (0x200000UL)
#else
#define CHILD_THREAD_STACK_BASE (0x500000800000UL)
#define CHILD_THREAD_STACK_SIZE (0x800000UL)
#endif
#define CHILD_THREAD_PRIO (DEFAULT_PRIO)

#if __SIZEOF_POINTER__ == 4
#define MAIN_THREAD_STACK_BASE (0x50000000UL)
#define MAIN_THREAD_STACK_SIZE (0x200000UL)
#else
#define MAIN_THREAD_STACK_BASE (0x500000000000UL)
#define MAIN_THREAD_STACK_SIZE (0x800000UL)
#endif
#define MAIN_THREAD_PRIO (DEFAULT_PRIO)

#define IPC_PER_SHM_SIZE (0x1000)

#define ROUND_UP(x, n)   (((x) + (n)-1) & ~((n)-1))
#define ROUND_DOWN(x, n) ((x) & ~((n)-1))

#define unused(x) ((void)(x))

#ifndef PAGE_SIZE
#define PAGE_SIZE 0x1000
#endif

/**
 * read(2):
 * On Linux, read() (and similar system calls) will transfer at most
 * 0x7ffff000 (2,147,479,552) bytes, returning the number of bytes
 * actually transferred.  (This is true on both 32-bit and 64-bit
 * systems.)
 */
#define READ_SIZE_MAX (0x7ffff000)

/* Syscall numbers */

/* Character */
#define CHCORE_SYS_putstr 0
#define CHCORE_SYS_getc   1

#define CHCORE_SYS_tee_push_rdr_update_addr 5
#define CHCORE_SYS_debug_rdr_logitem        6

/* PMO */
/* - single */
#define CHCORE_SYS_create_pmo        10
#define CHCORE_SYS_create_device_pmo 11
#define CHCORE_SYS_map_pmo           12
#define CHCORE_SYS_unmap_pmo         13
#define CHCORE_SYS_write_pmo         14
#define CHCORE_SYS_read_pmo          15
#ifdef CHCORE_OH_TEE
#define CHCORE_SYS_create_ns_pmo         16
#define CHCORE_SYS_destroy_ns_pmo        17
#define CHCORE_SYS_create_tee_shared_pmo 19
#define CHCORE_SYS_transfer_pmo_owner    20
#endif /* CHCORE_OH_TEE */

/* - address translation */
#define CHCORE_SYS_get_phys_addr 31

/* Capability */
#define CHCORE_SYS_revoke_cap    18
#define CHCORE_SYS_transfer_caps 62

/* Multitask */
/* - create & exit */
#define CHCORE_SYS_create_cap_group 80
#define CHCORE_SYS_exit_group       81
#define CHCORE_SYS_create_thread    82
#define CHCORE_SYS_thread_exit      83
#ifdef CHCORE_OH_TEE
#define CHCORE_SYS_get_thread_id    84
#define CHCORE_SYS_terminate_thread 85
#endif /* CHCORE_OH_TEE */
#define CHCORE_SYS_kill_group 86
/* - recycle */
#define CHCORE_SYS_register_recycle     90
#define CHCORE_SYS_cap_group_recycle    91
#define CHCORE_SYS_ipc_close_connection 92
/* - schedule */
#define CHCORE_SYS_yield        100
#define CHCORE_SYS_set_affinity 101
#define CHCORE_SYS_get_affinity 102
#define CHCORE_SYS_set_prio     103
#define CHCORE_SYS_get_prio     104

/* IPC */
/* - procedure call */
#define CHCORE_SYS_register_server         120
#define CHCORE_SYS_register_client         121
#define CHCORE_SYS_ipc_register_cb_return  122
#define CHCORE_SYS_ipc_call                123
#define CHCORE_SYS_ipc_return              124
#define CHCORE_SYS_ipc_exit_routine_return 125
/* - notification */
#define CHCORE_SYS_create_notifc 130
#define CHCORE_SYS_wait          131
#define CHCORE_SYS_notify        132

#ifdef CHCORE_OH_TEE
/* - oh-tee-ipc */
#define CHCORE_SYS_tee_msg_create_msg_hdl 140
#define CHCORE_SYS_tee_msg_create_channel 141
#define CHCORE_SYS_tee_msg_stop_channel   142

#define CHCORE_SYS_tee_msg_receive 143
#define CHCORE_SYS_tee_msg_call    144
#define CHCORE_SYS_tee_msg_reply   145
#define CHCORE_SYS_tee_msg_notify  146
#endif /* CHCORE_OH_TEE */

/* Exception */
/* - irq */
#define CHCORE_SYS_irq_register      150
#define CHCORE_SYS_irq_wait          151
#define CHCORE_SYS_irq_ack           152
#define CHCORE_SYS_cache_config      158
#define CHCORE_SYS_disable_local_irq 159
#define CHCORE_SYS_enable_local_irq  160
#define CHCORE_SYS_disable_irqno     155
#define CHCORE_SYS_enable_irqno      156
#define CHCORE_SYS_irq_op            40
#define CHCORE_SYS_irq_stop          41

/* - page fault */
#define CHCORE_SYS_user_fault_register 165
#define CHCORE_SYS_user_fault_map      166

/* Hardware Access (Privileged Instruction) */
/* - cache */
#define CHCORE_SYS_cache_flush 180
/* - timer */
#define CHCORE_SYS_get_current_tick 185

/* POSIX */
/* - time */
#define CHCORE_SYS_clock_gettime   200
#define CHCORE_SYS_clock_nanosleep 201
/* - memory */
#define CHCORE_SYS_handle_brk      210
#define CHCORE_SYS_handle_mprotect 213

/* CPIO */

#define CHCORE_SYS_fs_load_cpio 215

/* Debug */
#define CHCORE_SYS_debug_log         220
#define CHCORE_SYS_top               221
#define CHCORE_SYS_get_free_mem_size 222
#define CHCORE_SYS_get_mem_usage_msg 223

/* Performance Benchmark */
#define CHCORE_SYS_perf_start 230
#define CHCORE_SYS_perf_end   231
#define CHCORE_SYS_perf_null  232

/* Get PCI devcie information. */
#define CHCORE_SYS_get_pci_device 233

/* poweroff */
#define CHCORE_SYS_poweroff 234

/* Virtualization */
#define CHCORE_SYS_virt_dispatch 240

/* TrustZone */
#define CHCORE_SYS_tee_wait_switch_req 250
#define CHCORE_SYS_tee_switch_req      251
#define CHCORE_SYS_tee_create_ns_pmo   252
#define CHCORE_SYS_tee_pull_kernel_var 253
