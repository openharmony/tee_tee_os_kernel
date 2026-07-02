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
#include <stdio.h>
#include <assert.h>
#include <usrsyscall_irq.h>
#include <chcore/syscall.h>

enum gicv3_irq_op {
    GICV3_OP_SET_GROUP = 2,
    GICV3_OP_SET_PENDING = 4,
    GICV3_OP_MASK = 5,
};

int32_t enable_local_irq(void)
{
    usys_enable_local_irq();
    return 0;
}

int32_t disable_local_irq(void)
{
    usys_disable_local_irq();
    return 0;
}

/*
 * set_irq_pending() asks the kernel to set a hardware interrupt pending.
 *
 * The caller passes a GIC hardware interrupt number that has already been
 * agreed with the REE side. The kernel validates the interrupt range in
 * sys_irq_op()/gicv3_op(), so this wrapper only keeps the framework code from
 * depending on raw GIC operation numbers.
 */
int32_t set_irq_pending(uint32_t irq)
{
    return usys_irq_op((int)irq, GICV3_OP_SET_PENDING, 1);
}

/*
 * clear_irq_pending() clears pending state before enabling a notify interrupt.
 *
 * This keeps old pending state from generating a spurious notification after
 * the REE handler has been registered.
 */
int32_t clear_irq_pending(uint32_t irq)
{
    return usys_irq_op((int)irq, GICV3_OP_SET_PENDING, 0);
}

/*
 * set_irq_nonsecure() configures an interrupt as non-secure Group 1.
 *
 * Linux receives the notify SPI in the non-secure world, so TEE must not leave
 * the interrupt in a secure group.
 */
int32_t set_irq_nonsecure(uint32_t irq)
{
    return usys_irq_op((int)irq, GICV3_OP_SET_GROUP, 1);
}

/*
 * mask_irq() masks or unmasks a GIC interrupt.
 *
 * The wrapper keeps system services from depending on the raw GIC operation
 * number while still allowing notify initialization to unmask its SPI.
 */
int32_t mask_irq(uint32_t irq, bool mask)
{
    return usys_irq_op((int)irq, GICV3_OP_MASK, mask ? 1 : 0);
}

void init_sysctrl_hdlr(void)
{
    printf("%s not implemented!\n", __func__);
}