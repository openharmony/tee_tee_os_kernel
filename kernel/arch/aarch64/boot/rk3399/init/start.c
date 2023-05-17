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
#include "boot.h"
#include <machine.h>
#include <common/macro.h>
#include <common/types.h>
#include <common/vars.h>

char boot_stacks[PLAT_CPU_NUM][BOOT_STACK_SIZE] ALIGN(STACK_ALIGNMENT);

static void clear_bss(void)
{
    u64 *ptr;

    for (ptr = &_bss_start; ptr < &_bss_end; ptr++) {
        *ptr = 0;
    }
}

void start_c(paddr_t start_pa)
{
    clear_bss();

    init_boot_page_table();

    el1_mmu_activate();

    start_kernel(start_pa);
}
