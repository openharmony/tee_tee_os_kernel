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
#ifndef USRSYSCALL_IRQ_H
#define USRSYSCALL_IRQ_H

#include <stdbool.h>
#include <stdint.h>

int32_t enable_local_irq(void);

int32_t disable_local_irq(void);

/*
 * set_irq_pending() raises a GIC interrupt from a user-space system service.
 *
 * This is used by TEE framework services that need to notify the REE through a
 * non-secure SPI. The irq argument must be a GIC hardware interrupt number, not
 * a Linux virtual IRQ.
 */
int32_t set_irq_pending(uint32_t irq);

/*
 * clear_irq_pending() clears a GIC interrupt pending state.
 *
 * This is used before enabling a notify SPI so stale pending state does not
 * generate a spurious REE wakeup.
 */
int32_t clear_irq_pending(uint32_t irq);

/*
 * set_irq_nonsecure() moves a GIC interrupt into the non-secure group.
 *
 * The notify SPI must be non-secure because Linux owns the IRQ handler that
 * drains the notify shared memory.
 */
int32_t set_irq_nonsecure(uint32_t irq);

/*
 * mask_irq() masks or unmasks a GIC interrupt.
 *
 * Passing true masks the interrupt and passing false unmasks it.
 */
int32_t mask_irq(uint32_t irq, bool mask);

void init_sysctrl_hdlr(void);

#endif