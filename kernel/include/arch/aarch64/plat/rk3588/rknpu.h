/*
 * Copyright (c) 2026 Institute of Parallel And Distributed Systems (IPADS), Shanghai Jiao Tong University (SJTU)
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

#include <common/types.h>

#define RKNPU_CORE_NR (3)

#define RKNPU_CORE0_BASE_PADDR 0xfdab0000UL
#define RKNPU_CORE1_BASE_PADDR 0xfdac0000UL
#define RKNPU_CORE2_BASE_PADDR 0xfdad0000UL

#define RKNPU_CORE0_IRQ (142)
#define RKNPU_CORE1_IRQ (143)
#define RKNPU_CORE2_IRQ (144)

#define RKNPU_SUBCORE_TASK_NR     5U
#define RKNPU_DEFAULT_TIMEOUT_MS  6000U
#define RKNPU_JOB_PINGPONG        (1U << 2)

#define RKNPU_IOMMU_PROT_READ  (1U << 0)
#define RKNPU_IOMMU_PROT_WRITE (1U << 1)
#define RKNPU_IOMMU_PROT_RW \
    (RKNPU_IOMMU_PROT_READ | RKNPU_IOMMU_PROT_WRITE)

struct rknpu_task {
    u32 flags;
    u32 op_idx;
    u32 enable_mask;
    u32 int_mask;
    u32 int_clear;
    u32 int_status;
    u32 regcfg_amount;
    u32 regcfg_offset;
    u64 regcmd_addr;
} __attribute__((packed));

struct rknpu_subcore_task {
    u32 task_start;
    u32 task_number;
};

struct rknpu_submit {
    u32 flags;
    u32 timeout;
    u32 task_start;
    u32 task_number;
    u32 task_counter;
    s32 priority;
    u64 task_obj_addr;
    u64 regcfg_obj_addr;
    u64 task_base_addr;
    u64 user_data;
    u32 core_mask;
    s32 fence_fd;
    struct rknpu_subcore_task subcore_task[RKNPU_SUBCORE_TASK_NR];
};

void sys_tee_npu_secure_switch(bool secure);
int sys_tee_npu_ree_power_on(void);
int sys_tee_npu_ree_power_off(void);
int sys_tee_npu_submit_start(unsigned long submits_uaddr, int task_num);
int sys_tee_npu_poll_complete(void);
int sys_tee_npu_submit_cancel(void);
int sys_tee_npu_iommu_init(void);
int sys_tee_npu_iommu_map(unsigned long paddr,
                          unsigned long size,
                          unsigned int prot,
                          unsigned long iova_uaddr);
int sys_tee_npu_iommu_unmap(unsigned long iova, unsigned long size);
void sys_tee_npu_iommu_dump(unsigned long data_iova,
                            unsigned long dma_base_iova);
