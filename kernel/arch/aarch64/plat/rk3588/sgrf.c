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
#include <arch/tools.h>
#include <arch/mmu.h>
#include <machine.h>
#include <common/kprint.h>

#define SGRF_WRITE_MASK 0xffff0000U
#define NPU_SGRF_FIREWALL_CON 8

#define NPU_CORE_COUNT 3
#define NPU_CORE0_SECURE_MASK 0x3U
#define NPU_CORE1_SECURE_MASK 0x4U
#define NPU_CORE2_SECURE_MASK 0x8U

static const u32 npu_sgrf_secure_mask[NPU_CORE_COUNT] = {
    NPU_CORE0_SECURE_MASK,
    NPU_CORE1_SECURE_MASK,
    NPU_CORE2_SECURE_MASK,
};

static inline void set32(vaddr_t addr, u32 set_mask)
{
    put32(addr, SGRF_WRITE_MASK | (get32(addr) | set_mask));
}
static inline void clr32(vaddr_t addr, u32 set_mask)
{
    put32(addr, SGRF_WRITE_MASK | (get32(addr) & ~set_mask));
}

void switch_secure_device(int secure, int core_index)
{
    vaddr_t bussgrf_vaddr = phys_to_virt(BUSSGRF_BASE);
    vaddr_t npu_sgrf_con =
        bussgrf_vaddr + SGRF_FIREWALL_CON(NPU_SGRF_FIREWALL_CON);
    u32 secure_mask;

    if (core_index < 0 || core_index >= NPU_CORE_COUNT)
        return;

    secure_mask = npu_sgrf_secure_mask[core_index];

    if (secure)
        set32(npu_sgrf_con, secure_mask);
    else
        clr32(npu_sgrf_con, secure_mask);
}
