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
#include <arch/boot.h>
#include <arch/trustzone/logger.h>
#include <common/kprint.h>
#include <common/vars.h>
#include <common/macro.h>
#include <common/types.h>
#include <common/lock.h>
#include <arch/mm/page_table.h>
#include <arch/mmu.h>
#include <mm/mm.h>
#include <irq/irq.h>
#include <object/thread.h>

#define LOGGER_BASE 0xFFFFFA0000000000
#define LOGGER_SIZE (PAGE_SIZE * 64)

volatile bool using_logger = false;
volatile bool registered = false;
struct tmp_log {
    volatile int pos;
    char buf[];
} * logger;

void logger_init(paddr_t log_paddr)
{
    if (registered) {
        return;
    }
    registered = true;
    kinfo("logger init recv paddr %lx\n", log_paddr);

    map_range_in_pgtbl_kernel((void *)((unsigned long)boot_ttbr1_l0 + KBASE),
                              LOGGER_BASE,
                              log_paddr,
                              LOGGER_SIZE,
                              VMR_READ | VMR_WRITE | VMR_TZ_NS);
    flush_tlb_all();

    logger = (void *)LOGGER_BASE;
    logger->pos = 0;

    kinfo("logger init finish logger addr %p\n", logger);
}

void logger_send(int c)
{
    if (logger->pos + sizeof(logger->pos) >= LOGGER_SIZE) {
        return;
    }
    logger->buf[logger->pos] = c;
    logger->pos++;
}
