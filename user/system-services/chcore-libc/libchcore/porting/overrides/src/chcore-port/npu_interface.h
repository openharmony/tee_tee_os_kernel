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
#ifndef NPU_INTERFACE_H
#define NPU_INTERFACE_H

#if defined(CHCORE_LLM)

#include <stdint.h>

void* mem_allocate(size_t size, uint64_t *dma_addr, uint64_t *obj, uint32_t flags, uint64_t *handle);
void mem_destroy(void *addr, size_t len, uint64_t handle, uint64_t obj_addr);

int npu_submit(__u64 task_obj_addr, __u32 core_mask);
int npu_submit_multi(__u64 task_obj_addr[], int task_num);
void npu_close(void);

#endif /* CHCORE_LLM */

#endif // NPU_INTERFACE_H
