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
#include <arch/tools.h>
#include <arch/mmu.h>
#include <arch/machine/smp.h>
#include <common/types.h>
#include <common/macro.h>
#include <mm/mm.h>
#include <object/irq.h>

static int nr_lines;

enum gic_op_t {
	GICV3_OP_SET_PRIO = 1,
	GICV3_OP_SET_GROUP,
	GICV3_OP_SET_TARGET,
	GICV3_OP_SET_PENDING,
	GICV3_OP_MASK,
};

static inline void set32(vaddr_t addr, u32 set_mask)
{
    put32(addr, get32(addr) | set_mask);
}

static inline void clr32(vaddr_t addr, u32 set_mask)
{
    put32(addr, get32(addr) & ~set_mask);
}

static inline bool gicv3_enable_sre(void)
{
	u32 val;

	val = read_sys_reg(ICC_SRE_EL1);
	if (val & ICC_SRE_EL1_SRE)
		return true;

	val |= ICC_SRE_EL1_SRE;
	write_sys_reg(ICC_SRE_EL1, val);
	isb();
	val = read_sys_reg(ICC_SRE_EL1);

	return !!(val & ICC_SRE_EL1_SRE);
}

static inline void gicv3_set_routing(u64 cpumask, void *rout_reg)
{
	put32((u64)rout_reg, (u32)cpumask);
	put32((u64)(rout_reg + 4), (u32)(cpumask >> 32));
}

/* Wait for completion of a redist change */
static void gic_wait_redist_complete(void)
{
	while (get32(GICR_PER_CPU_BASE(smp_get_cpu_id())) & GICD_CTLR_RWP);
}

/* Wait for completion of a dist change */
static inline void gic_wait_dist_complete(void)
{
	while (get32(GICD_BASE) & GICD_CTLR_RWP);
}

void gicv3_distributor_init(void)
{
	unsigned int gic_type_reg, i, val;
	unsigned long cpumask;

	gic_type_reg = get32(GICD_TYPER);
	nr_lines = GICD_TYPER_IRQS(gic_type_reg);

	kinfo("nr_lines %d\n", nr_lines);

	/* Disable the distributor. */
	put32(GICD_CTLR, 0);
	gic_wait_dist_complete();

	/* Enable distributor with ARE_S, Group1S and Group0 */
	val = get32(GICD_CTLR);
	put32(GICD_CTLR, val | GICD_CTLR_ARE_S | GICD_CTLR_ENABLE_G1_S | GICD_CTLR_ENABLE_G0);
}

static void gicv3_init_cpu_interface(void)
{
	unsigned int val;

	/* Make sure that the SRE bit has been set. */
	if (!gicv3_enable_sre()) {
		BUG("GICv3 fatal error: SRE bit not set (disabled at EL2)\n");
		return;
	}

	val = read_sys_reg(ICC_CTLR_EL1);
	write_sys_reg(ICC_CTLR_EL1, val & ~(1 << ICC_CTLR_EL1_EOImode_SHIFT));
	isb();

	val = read_sys_reg(ICC_IGRPEN1_EL1);
	write_sys_reg(ICC_IGRPEN1_EL1, 1);
	isb();
}

static void gicv3_redistributor_init(void)
{
	unsigned int gic_wake_reg;
	unsigned long redis_base, sgi_base;

	redis_base = GICR_PER_CPU_BASE(smp_get_cpu_id());

	/* Clear processor sleep and wait till childasleep is cleard. */
	gic_wake_reg = get32(redis_base + GICR_WAKER_OFFSET);
	gic_wake_reg &= ~GICR_WAKER_ProcessorSleep;
	put32(redis_base + GICR_WAKER_OFFSET, gic_wake_reg);
	while (get32(redis_base + GICR_WAKER_OFFSET) & GICR_WAKER_ChildrenAsleep);

	gic_wait_redist_complete();

	/* Initialise cpu interface registers. */
	gicv3_init_cpu_interface();
}

int gicv3_set_irq_group(int irq, int ns)
{
	int idx = irq / NUM_INTS_PER_REG;
	u32 mask = 1 << (irq % NUM_INTS_PER_REG);

	if (ns) {
		set32(GICD_IGROUPR(idx), mask);
		clr32(GICD_IGRPMODR(idx), mask);
	} else {
		clr32(GICD_IGROUPR(idx), mask);
		set32(GICD_IGRPMODR(idx), mask);
	}

	gic_wait_dist_complete();
	return 0;
}

void gicv3_enable_irqno(int irq)
{
	int cpu_id;

	if (irq < 32) {
		BUG_ON(1);
	} else {
		/* irq in distributor. */
		put32(GICD_BASE + GICD_ISENABLER_OFFSET + ((irq >> 5) << 2),
		      (1 << (irq % 32)));
		gic_wait_dist_complete();
	}
}

void gicv3_disable_irqno(int irq)
{
	int cpu_id;

	if (irq < 32) {
		BUG_ON(1);
		// irq in redist
		cpu_id = smp_get_cpu_id();
		put32(GICR_PER_CPU_BASE(cpu_id) + GICR_SGI_BASE_OFFSET
		      + GICR_ICENABLER0_OFFSET,
		      (1 << (irq % 32)));
		gic_wait_redist_complete();
	} else {
		// irq in dist
		put32(GICD_BASE + GICD_ICENABLER_OFFSET + ((irq >> 5) << 2),
		      (1 << (irq % 32)));
		gic_wait_dist_complete();
	}
}

int gicv3_set_irq_target(int irq, unsigned int cpuid)
{
	unsigned int mpidr;
	unsigned long cpumask;

	if (cpuid != 0) {
		return -EINVAL;
	}

	asm volatile("mrs %0, mpidr_el1\n\t" : "=r"(mpidr));
	gicv3_disable_irqno(irq);
	cpumask = ((mpidr >> 0) & 0xff) |
		  (((mpidr >> 8) & 0xff) << 8) |
		  (((mpidr >> 16) & 0xff) << 16) |
		  ((((unsigned long)mpidr >> 24) & 0xff) << 32);
	gicv3_set_routing(cpumask, (void *)(GICD_IROUTER + irq * 8));
	gicv3_enable_irqno(irq);
	gic_wait_dist_complete();

	return 0;
}

int gicv3_set_irq_prio(int irq, int prio)
{
	put8(GICD_IPRIORITYR + irq, prio);
	return 0;
}

int gicv3_set_irq_pending(int irq, bool set)
{
	int idx = irq / NUM_INTS_PER_REG;
	u32 mask = 1 << (irq % NUM_INTS_PER_REG);

	if (set) {
		set32(GICD_ISPENDR(idx), mask);
	} else {
		set32(GICD_ICPENDR(idx), mask);
	}

	return 0;
}

void gicv3_ack_irq(int irq)
{
	write_sys_reg(ICC_EOIR1_EL1, irq);
}

void plat_interrupt_init(void)
{
	unsigned int cpuid = smp_get_cpu_id();

	if (cpuid == 0)
		gicv3_distributor_init();

	gicv3_redistributor_init();
}

void plat_send_ipi(u32 cpu, u32 ipi)
{
	BUG("ipi not implemented\n");
}

void plat_enable_irqno(int irq)
{
	gicv3_enable_irqno(irq);
}

void plat_disable_irqno(int irq)
{
	gicv3_disable_irqno(irq);
}

void plat_ack_irq(int irq)
{
}

void plat_handle_irq(void)
{
	unsigned int irqnr = 0;
	unsigned int irqstat = 0;
	int ret;

	irqstat = read_sys_reg(ICC_IAR1_EL1);
	dsb(sy);
	irqnr = irqstat & 0x3ff;

	gicv3_ack_irq(irqnr);

	if (likely(irqnr > 15 && irqnr < 1020)) {
		kinfo("%s: recv irq %d\n", __func__, irqnr);
		user_handle_irq(irqnr);
	} else if (irqnr < 16) {
		BUG("NO SGI in TEE now\n");
	}

	isb();
}

void gic_op_raise_pi(size_t it)
{
	gicv3_set_irq_pending(it, true);
}

int gicv3_irq_mask(int irq, bool mask)
{
	if (mask) {
		gicv3_disable_irqno(irq);
	} else {
		gicv3_enable_irqno(irq);
	}
	return 0;
}

int gicv3_op(int irq, int op, long val)
{
	int ret;

	if (irq < 0 || irq >= MAX_IRQ_NUM) {
		ret = -EINVAL;
		goto out;
	}

	switch (op) {
	case GICV3_OP_SET_PRIO:
		ret = gicv3_set_irq_prio(irq, (int)val);
		break;
	case GICV3_OP_SET_GROUP:
		ret = gicv3_set_irq_group(irq, (int)val);
		break;
	case GICV3_OP_SET_TARGET:
		ret = gicv3_set_irq_target(irq, (int)val);
		break;
	case GICV3_OP_SET_PENDING:
		ret = gicv3_set_irq_pending(irq, (bool)val);
		break;
	case GICV3_OP_MASK:
		ret = gicv3_irq_mask(irq, (bool)val);
		break;
	default:
		ret = -EINVAL;
		break;
	}

out:
	return ret;
}
