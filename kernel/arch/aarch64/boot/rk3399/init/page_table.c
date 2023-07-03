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
#include <common/macro.h>
#include <arch/mmu.h>
#include <arch/mm/page_table.h>
#include <mm/mm.h>
#include <machine.h>

#define IS_VALID       (0x1UL << 0)
#define IS_TABLE       (0x1UL << 1)
#define UXN            (0x1UL << 54)
#define ACCESSED       (0x1UL << 10)
#define NG             (0x1UL << 11)
#define INNER_SHARABLE (0x3UL << 8)
#define NORMAL         (0x4UL << 2)
#define DEVICE         (0x0UL << 2)
#define SECURE         (0x0UL << 5)

ptp_t boot_ttbr0_l0 ALIGN(PAGE_SIZE);
ptp_t boot_ttbr0_l1 ALIGN(PAGE_SIZE);

ptp_t boot_ttbr1_l0 ALIGN(PAGE_SIZE);
ptp_t boot_ttbr1_l1 ALIGN(PAGE_SIZE);

void init_boot_page_table(void)
{
    vaddr_t vaddr;
    paddr_t paddr;

    /* TTBR0_EL1: Low memory */
    vaddr = 0;
    boot_ttbr0_l0.ent[GET_L0_INDEX(vaddr)].pte = ((paddr_t)&boot_ttbr0_l1)
                                                 | IS_TABLE | IS_VALID;

    /* Blindly map 0~1G as normal memory */
    vaddr = paddr = 0;
    boot_ttbr0_l1.ent[GET_L1_INDEX(vaddr)].pte = paddr | UXN | ACCESSED | NG
                                                 | SECURE | INNER_SHARABLE
                                                 | NORMAL | IS_VALID;

    /* Blindly map 3~4G as device memory */
    vaddr = paddr = 3 * (1UL << L1_INDEX_SHIFT);
    boot_ttbr0_l1.ent[GET_L1_INDEX(vaddr)].pte = paddr | UXN | ACCESSED | NG
                                                 | DEVICE | SECURE | IS_VALID;

    /* TTBR1_EL1: High memory */
    vaddr = phys_to_virt(0);
    boot_ttbr1_l0.ent[GET_L0_INDEX(vaddr)].pte = ((paddr_t)&boot_ttbr1_l1)
                                                 | IS_TABLE | IS_VALID;

    /* Blindly map 0~1G as normal memory */
    paddr = 0;
    vaddr = phys_to_virt(paddr);
    boot_ttbr1_l1.ent[GET_L1_INDEX(vaddr)].pte =
        paddr | UXN | ACCESSED | SECURE | INNER_SHARABLE | NORMAL | IS_VALID;

    /* Blindly map 3~4G as device memory */
    paddr = 3 * (1UL << L1_INDEX_SHIFT);
    vaddr = phys_to_virt(paddr);
    boot_ttbr1_l1.ent[GET_L1_INDEX(vaddr)].pte = paddr | UXN | ACCESSED | DEVICE
                                                 | SECURE | IS_VALID;
}
