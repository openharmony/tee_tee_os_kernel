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

#include <common/types.h>

#define PLAT_CPU_NUM 1

void teeos_cfg_init(paddr_t start_pa);

paddr_t get_tzdram_start(void);

paddr_t get_tzdram_end(void);

paddr_t get_gicd_base(void);

paddr_t get_uart_base(void);

#define GICD_BASE (KBASE + 0xfd400000)
#define GICR_BASE (KBASE + 0xfd460000)

#define SIZE_128K	0x20000
#define GICR_PER_CPU_BASE(CPUID) (GICR_BASE + SIZE_128K * CPUID)

#define GICH_HCR	0x0
#define GICH_VTR	0x4
#define GICH_VMCR	0x8
#define GICH_MISR	0x10
#define GICH_EISR0	0x20
#define GICH_EISR1	0x24
#define GICH_ELRSR0	0x30
#define GICH_ELRSR1	0x34
#define GICH_APR	0xf0
#define GICH_LR0	0x100

/* GICD Registers */
#define GICD_CTLR	(GICD_BASE+0x000)
#define GICD_TYPER	(GICD_BASE+0x004)
#define GICD_IIDR	(GICD_BASE+0x008)
#define GICD_IGROUPR(n)	(GICD_BASE+0x080+(n)*4)
#define GICD_ISENABLER	(GICD_BASE+0x100)
#define GICD_ICENABLER	(GICD_BASE+0x180)
#define GICD_ISENABLER_OFFSET (0x100)
#define GICD_ICENABLER_OFFSET (0x180)
#define GICD_ISPENDR(n)	(GICD_BASE+0x200+(n)*4)
#define GICD_ICPENDR(n)	(GICD_BASE+0x280+(n)*4)
#define GICD_ISACTIVER	(GICD_BASE+0x300)
#define GICD_ICACTIVER	(GICD_BASE+0x380)
#define GICD_IPRIORITYR	(GICD_BASE+0x400)
#define GICD_ITARGETSR	(GICD_BASE+0x800)
#define GICD_ICFGR	(GICD_BASE+0xC00)
#define GICD_IGRPMODR(n)    (GICD_BASE+0xD00+(n)*4)
#define GICD_SGIR	(GICD_BASE+0xF00)
#define GICD_SGIR_CLRPEND	(GICD_BASE+0xF10)
#define GICD_SGIR_SETPEND	(GICD_BASE+0xF20)
#define GICD_IROUTER		(GICD_BASE+0x6000)

#define GICD_ENABLE			0x1
#define GICD_DISABLE			0x0
#define GICD_INT_ACTLOW_LVLTRIG		0x0
#define GICD_INT_EN_CLR_X32		0xffffffff
#define GICD_INT_EN_SET_SGI		0x0000ffff
#define GICD_INT_EN_CLR_PPI		0xffff0000
#define GICD_INT_DEF_PRI		0xa0
#define GICD_INT_DEF_PRI_X4		((GICD_INT_DEF_PRI << 24) |\
					(GICD_INT_DEF_PRI << 16) |\
					(GICD_INT_DEF_PRI << 8) |\
					GICD_INT_DEF_PRI)

#define GICD_CTLR_RWP			(1U << 31)
#define GICD_CTLR_DS			(1U << 6)
#define GICD_CTLR_ARE_S		    (1U << 4)
#define GICD_CTLR_ENABLE_G1_S	(1U << 2)
#define GICD_CTLR_ENABLE_G0		(1U << 0)

#define GICD_TYPER_RSS			(1U << 26)
#define GICD_TYPER_LPIS			(1U << 17)
#define GICD_TYPER_MBIS			(1U << 16)

/* GICD Register bits */
#define GICD_CTL_ENABLE 0x1
#define GICD_TYPE_LINES 0x01F
#define GICD_TYPE_CPUS_SHIFT 5
#define GICD_TYPE_CPUS 0x0E0
#define GICD_TYPE_SEC 0x400

/* GICD_SGIR defination */
#define GICD_SGIR_SGIINTID_SHIFT	0
#define GICD_SGIR_CPULIST_SHIFT		16
#define GICD_SGIR_LISTFILTER_SHIFT	24
#define GICD_SGIR_VAL(listfilter, cpulist, sgi)		\
	(((listfilter) << GICD_SGIR_LISTFILTER_SHIFT) |	\
	 ((cpulist) << GICD_SGIR_CPULIST_SHIFT) |	\
	 ((sgi) << GICD_SGIR_SGIINTID_SHIFT))

#define GIC_INTID_PPI			16
#define GIC_INTID_SPI			32
#define GIC_INTID_SPI_UART6		(GIC_INTID_SPI + 79)
#define GIC_INTID_PPI_EL1_PHYS_TIMER	(GIC_INTID_PPI + 14)
#define GIC_INTID_PPI_EL1_VIRT_TIMER	(GIC_INTID_PPI + 11)
#define GIC_INTID_PPI_NS_EL2_PHYS_TIMER	(GIC_INTID_PPI + 10)
#define GIC_INTID_PPI_EL3_PHYS_TIMER	(GIC_INTID_PPI + 13)

#define GICD_TYPER_ID_BITS(typer)	((((typer) >> 19) & 0x1f) + 1)
#define GICD_TYPER_NUM_LPIS(typer)	((((typer) >> 11) & 0x1f) + 1)
#define GICD_TYPER_IRQS(typer)		(((((typer) & 0x1f) + 1) << 5) - 1)

/* GICR Registers */
/* Redistributor - RD_BASE */
#define GICR_CTLR_OFFSET		0x00000000U
#define GICR_IIDR_OFFSET		0x00000004U
#define GICR_TYPER_OFFSET		0x00000008U
#define GICR_STATUSR_OFFSET		0x00000010U
#define GICR_WAKER_OFFSET		0x00000014U
#define GICR_MPAMIDR_OFFSET		0x00000018U
#define GICR_PARTIDR_OFFSET		0x0000001CU
#define GICR_SETLPIR_OFFSET		0x00000040U
#define GICR_CLRLPIR_OFFSET		0x00000048U
#define GICR_PROPBASER_OFFSET		0x00000070U
#define GICR_PENDBASER_OFFSET		0x00000078U

#define GICR_WAKER(CPUID)		(GICR_PER_CPU_BASE(CPUID) + GICR_WAKER_OFFSET)
#define GICR_WAKER_ProcessorSleep	(1U << 1)
#define GICR_WAKER_ChildrenAsleep	(1U << 2)
#define GICR_IGROUPR0(CPUID)		(GICR_PER_CPU_BASE(CPUID) + SIZE_64K + 0x080)

/* Redistributor - SGI_BASE */
#define GICR_SGI_BASE_OFFSET		0x10000U
#define GICR_IGROUPR0_OFFSET		0x00000080U
#define GICR_IGROUPR_E_OFFSET		0x00000084U
#define GICR_ISENABLER0_OFFSET		0x00000100U
#define GICR_ISENABLER_E_OFFSET		0x00000104U
#define GICR_ICENABLER0_OFFSET		0x00000180U
#define GICR_ICENABLER_E_OFFSET		0x00000184U
#define GICR_ISPENDR0_OFFSET		0x00000200U
#define GICR_ISPENDR_E_OFFSET		0x00000204U
#define GICR_ICPENDR0_OFFSET		0x00000280U
#define GICR_ICPENDR_E_OFFSET		0x00000284U
#define GICR_ISACTIVER0_OFFSET		0x00000300U
#define GICR_ISACTIVER_E_OFFSET		0x00000304U
#define GICR_ICACTIVER0_OFFSET		0x00000380U
#define GICR_ICACTIVER_E_OFFSET		0x00000384U
#define GICR_IPRIORITYR_OFFSET		0x00000400U
#define GICR_IPRIORITYR_E_OFFSET	0x00000420U
#define GICR_ICFGR0_OFFSET		0x00000C00U
#define GICR_ICFGR1_OFFSET		0x00000C04U
#define GICR_ICFGR_E_OFFSET		0x00000C08U
#define GICR_IGRPMODR0_OFFSET		0x00000D00U
#define GICR_IGRPMODR_E_OFFSET		0x00000D04U
#define GICR_NSACR_OFFSET		0x00000E00U

/* Default priority value. */
#define DEFAULT_PMR_VALUE	0xF0

#define GICD_ICACTIVER_DEFAULT_MASK	0xffffffff
#define GICD_ICENABLER_DEFAULT_MASK	0xffffffff
#define GICD_ICPENDR_DEFAULT_MASK	0xffffffff
#define GICD_IGROUPR_DEFAULT_MASK	0xffffffff
#define GICD_IGRPMODR_DEFAULT_MASK	0xffffffff

#define GICR_ICACTIVER0_DEFAULT_MASK	0xffffffff
#define GICR_ICENABLER0_DEFAULT_MASK	0xffff0000
#define GICR_ISENABLER0_DEFAULT_MASK	0x0000ffff
#define GICR_IGROUPR0_DEFAULT_MASK	0xffffffff

/* Number of interrupts in one register */
#define NUM_INTS_PER_REG	32

/*
 * GICv3 cpu interface registers.
 * Refers to GICv3 Architecture Manual
 */

/* ICC_CTLR_EL1 */
#define ICC_CTLR_EL1_EOImode_SHIFT   1
#define ICC_CTLR_EL1_CBPR_SHIFT      0
#define ICC_CTLR_EL1_PRIbits_SHIFT   8
#define ICC_CTLR_EL1_PRIbits_MASK    0x700

/* ICC_SRE_EL1 */
#define ICC_SRE_EL1_SRE              (1U << 0)

/* SGI registers */
#define ICC_SGI1R_SGI_ID_SHIFT         24
#define ICC_SGI1R_AFFINITY_1_SHIFT     16
#define ICC_SGI1R_AFFINITY_2_SHIFT     32
#define ICC_SGI1R_AFFINITY_3_SHIFT     48
#define ICC_SGI1R_RS_SHIFT             44

#define stringify(x)    #x
#define ICC_IGRPEN1_EL1 stringify(S3_0_C12_C12_7)
#define ICC_SRE_EL1     stringify(S3_0_C12_C12_5)
#define ICC_CTLR_EL1    stringify(S3_0_C12_C12_4)
#define ICC_BPR1_EL1    stringify(S3_0_C12_C12_3)
#define ICC_EOIR1_EL1   stringify(S3_0_C12_C12_1)
#define ICC_IAR1_EL1    stringify(S3_0_C12_C12_0)
#define ICC_SGI1R_EL1   stringify(S3_0_C12_C11_5)
#define ICC_DIR_EL1     stringify(S3_0_C12_C11_1)
#define ICC_AP1R0_EL1   stringify(S3_0_C12_C9_0)
#define ICC_AP1R1_EL1   stringify(S3_0_C12_C9_1)
#define ICC_AP1R2_EL1   stringify(S3_0_C12_C9_2)
#define ICC_AP1R3_EL1   stringify(S3_0_C12_C9_3)
#define ICC_AP0R0_EL1   stringify(S3_0_C12_C8_4)
#define ICC_AP0R1_EL1   stringify(S3_0_C12_C8_5)
#define ICC_AP0R2_EL1   stringify(S3_0_C12_C8_6)
#define ICC_AP0R3_EL1   stringify(S3_0_C12_C8_7)
#define ICC_PMR_EL1     stringify(S3_0_C4_C6_0)

#define read_sys_reg(sys_reg)                                              \
	({                                                                 \
		u32 val;                                                   \
		asm volatile("mrs %0, " sys_reg : "=r"(val) : : "memory"); \
		val;                                                       \
	})

#define write_sys_reg(sys_reg, val) \
	asm volatile("msr " sys_reg ", %0" : : "r"((val)) : "memory")

#define MPIDR_AFFINITY_LEVEL(mpidr, level) \
	((mpidr >> (level * 8)) & 0xFF)

#define MPIDR_TO_SGI_AFFINITY(cluster_id, level) \
	(MPIDR_AFFINITY_LEVEL(cluster_id, level) \
	 << ICC_SGI1R_AFFINITY_##level##_SHIFT)

#define MPIDR_RS(mpidr)                (((mpidr)&0xF0UL) >> 4)
#define MPIDR_TO_SGI_RS(mpidr)         (MPIDR_RS(mpidr) << ICC_SGI1R_RS_SHIFT)
#define MPIDR_TO_SGI_CLUSTER_ID(mpidr) ((mpidr) & ~0xFUL)
