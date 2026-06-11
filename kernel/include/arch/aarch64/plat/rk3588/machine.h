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

#define GICD_BASE (KBASE + 0xfe600000) //0xfe600000
#define GICR_BASE (KBASE + 0xfe680000) //0xfe680000

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
#define GICD_ISENABLER(n)	(GICD_BASE+0x100+(n)*4)
#define GICD_ICENABLER(n)	(GICD_BASE+0x180+(n)*4)
#define GICD_ISENABLER_OFFSET (0x100)
#define GICD_ICENABLER_OFFSET (0x180)
#define GICD_ISPENDR(n)	(GICD_BASE+0x200+(n)*4)
#define GICD_ICPENDR(n)	(GICD_BASE+0x280+(n)*4)
#define GICD_ISACTIVER	(GICD_BASE+0x300)
#define GICD_ICACTIVER	(GICD_BASE+0x380)
#define GICD_IPRIORITYR(n)	(GICD_BASE+0x400+(n)*4)
#define GICD_ITARGETSR(n)	(GICD_BASE+0x800+(n)*4)
#define GICD_ICFGR(n)	(GICD_BASE+0xC00+(n)*4)
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

#define SIZE_K(n)		((n) * 1024)
#define SIZE_M(n)		((n) * 1024UL * 1024UL)
#define WITH_16BITS_WMSK(bits)	(0xffff0000 | (bits))
#define RK_BL31_PLAT_PARAM_VAL	0x0f1e2d3c4b5a6978ULL

#define RKNPU_IOMMU_LOW_PT_BASE	0x20000000UL
#define RKNPU_IOMMU_LOW_PT_SIZE	SIZE_M(64)
#define RKNPU_IOMMU_LOW_PT_END \
	(RKNPU_IOMMU_LOW_PT_BASE + RKNPU_IOMMU_LOW_PT_SIZE)
#define PMU_PWR_OFFSET           0x14cU
#define PMU_STATUS_OFFSET        0x180U
#define PMU_REQ_OFFSET           0x10cU
#define PMU_IDLE_OFFSET          0x120U
#define PMU_ACK_OFFSET           0x118U
#define PMU_MEM_PWR_OFFSET       0x1a0U
#define PMU_CHAIN_STATUS_OFFSET  0x1f0U
#define PMU_MEM_STATUS_OFFSET    0x1f8U
#define PMU_REPAIR_STATUS_OFFSET 0x290U

#define CRU_CLKGATE_CON(id) (0x800U + (id) * 0x4U)
#define CRU_GATE_ENABLE(bit) BIT((bit) + 16U)
#define CRU_GATE_DISABLE(bit) (BIT((bit) + 16U) | BIT(bit))

#define PMU_WRITE_MASK(mask) ((mask) << 16)

#define UMCTL0_BASE		0xf7000000
#define UMCTL1_BASE		0xf8000000
#define UMCTL2_BASE		0xf9000000
#define UMCTL3_BASE		0xfa000000
#define GIC600_BASE		0xfe600000
#define GIC600_SIZE		SIZE_K(64)
#define DAPLITE_BASE		0xfd100000
#define PMU0SGRF_BASE		0xfd580000
#define PMU1SGRF_BASE		0xfd582000
#define BUSSGRF_BASE		0xfd586000
#define DSUSGRF_BASE		0xfd587000
#define PMU0GRF_BASE		0xfd588000
#define PMU1GRF_BASE		0xfd58a000
#define SYSGRF_BASE		0xfd58c000
#define BIGCORE0GRF_BASE	0xfd590000
#define BIGCORE1GRF_BASE	0xfd592000
#define LITCOREGRF_BASE		0xfd594000
#define DSUGRF_BASE		0xfd598000
#define DDR01GRF_BASE		0xfd59c000
#define DDR23GRF_BASE		0xfd59d000
#define CENTERGRF_BASE		0xfd59e000
#define GPUGRF_BASE		0xfd5a0000
#define NPUGRF_BASE		0xfd5a2000
#define USBGRF_BASE		0xfd5ac000
#define PHPGRF_BASE		0xfd5b0000
#define PCIE3PHYGRF_BASE	0xfd5b8000
#define USB2PHY0_GRF_BASE	0xfd5d0000
#define USB2PHY1_GRF_BASE	0xfd5d4000
#define USB2PHY2_GRF_BASE	0xfd5d8000
#define USB2PHY3_GRF_BASE	0xfd5dc000
#define PMU0IOC_BASE		0xfd5f0000
#define PMU1IOC_BASE		0xfd5f4000
#define BUSIOC_BASE		0xfd5f8000
#define VCCIO1_4_IOC_BASE	0xfd5f9000
#define VCCIO3_5_IOC_BASE	0xfd5fa000
#define VCCIO2_IOC_BASE		0xfd5fb000
#define VCCIO6_IOC_BASE		0xfd5fc000
#define SRAM_BASE		0xff000000
#define PMUSRAM_BASE		0xff100000
#define PMUSRAM_SIZE		SIZE_K(128)
#define PMUSRAM_RSIZE		SIZE_K(64)
#define CRU_BASE		0xfd7c0000
#define PHP_CRU_BASE		0xfd7c8000
#define SCRU_BASE		0xfd7d0000
#define BUSSCRU_BASE		0xfd7d8000
#define PMU1SCRU_BASE		0xfd7e0000
#define PMU1CRU_BASE		0xfd7f0000
#define DDR0CRU_BASE		0xfd800000
#define DDR1CRU_BASE		0xfd804000
#define DDR2CRU_BASE		0xfd808000
#define DDR3CRU_BASE		0xfd80c000
#define BIGCORE0CRU_BASE	0xfd810000
#define BIGCORE1CRU_BASE	0xfd812000
#define LITCRU_BASE		0xfd814000
#define DSUCRU_BASE		0xfd818000
#define I2C0_BASE		0xfd880000
#define UART0_BASE		0xfd890000
#define GPIO0_BASE		0xfd8a0000
#define PWM0_BASE		0xfd8b0000
#define PMUPVTM_BASE		0xfd8c0000
#define TIMER_HP_BASE		0xfd8c8000
#define PMU0_BASE		0xfd8d0000
#define PMU1_BASE		0xfd8d4000
#define PMU2_BASE		0xfd8d8000
#define PMU_BASE		PMU0_BASE
#define PMUWDT_BASE		0xfd8e0000
#define PMUTIMER_BASE		0xfd8f0000
#define OSC_CHK_BASE		0xfd9b0000
#define VOP_BASE		0xfdd90000
#define HDMIRX_BASE		0xfdee0000
#define MSCH0_BASE		0xfe000000
#define MSCH1_BASE		0xfe002000
#define MSCH2_BASE		0xfe004000
#define MSCH3_BASE		0xfe006000
#define FIREWALL_DSU_BASE	0xfe010000
#define FIREWALL_DDR_BASE	0xfe030000
#define FIREWALL_SYSMEM_BASE	0xfe038000
#define DDRPHY0_BASE		0xfe0c0000
#define DDRPHY1_BASE		0xfe0d0000
#define DDRPHY2_BASE		0xfe0e0000
#define DDRPHY3_BASE		0xfe0f0000
#define TIMER_DDR_BASE		0xfe118000
#define KEYLADDER_BASE		0xfe380000
#define CRYPTO_S_BASE		0xfe390000
#define OTP_S_BASE		0xfe3a0000
#define DCF_BASE		0xfe3c0000
#define STIMER0_BASE		0xfe3d0000
#define WDT_S_BASE		0xfe3e0000
#define CRYPTO_S_BY_KEYLAD_BASE	0xfe420000
#define NSTIMER0_BASE		0xfeae0000
#define NSTIMER1_BASE		0xfeae8000
#define WDT_NS_BASE		0xfeaf0000
#define UART1_BASE		0xfeb40000
#define UART2_BASE		0xfeb50000
#define UART3_BASE		0xfeb60000
#define UART4_BASE		0xfeb70000
#define UART5_BASE		0xfeb80000
#define UART6_BASE		0xfeb90000
#define UART7_BASE		0xfeba0000
#define UART8_BASE		0xfebb0000
#define UART9_BASE		0xfebc0000
#define GPIO1_BASE		0xfec20000
#define GPIO2_BASE		0xfec30000
#define GPIO3_BASE		0xfec40000
#define GPIO4_BASE		0xfec50000
#define MAILBOX1_BASE		0xfec70000
#define OTP_NS_BASE		0xfecc0000
#define INTMUX0_DDR_BASE	0Xfecf8000
#define INTMUX1_DDR_BASE	0Xfecfc000
#define STIMER1_BASE		0xfed30000
#define SRAM_ENTRY_BASE		SRAM_BASE
#define SRAM_PMUM0_SHMEM_BASE	(SRAM_ENTRY_BASE + SIZE_K(3))
#define SRAM_LD_BASE		(SRAM_ENTRY_BASE + SIZE_K(4))
#define SRAM_LD_SIZE		SIZE_K(64)
#define SRAM_LD_SP		(SRAM_LD_BASE + SRAM_LD_SIZE -\
				 128)
#define DDR_SHARE_MEM		SIZE_K(1024)
#define DDR_SHARE_SIZE		SIZE_K(64)
#define SHARE_MEM_BASE		DDR_SHARE_MEM
#define SHARE_MEM_PAGE_NUM	15
#define SHARE_MEM_SIZE		SIZE_K(SHARE_MEM_PAGE_NUM * 4)
#define	SCMI_SHARE_MEM_BASE	(SHARE_MEM_BASE + SHARE_MEM_SIZE)
#define	SCMI_SHARE_MEM_SIZE	SIZE_K(4)
#define SMT_BUFFER_BASE		SCMI_SHARE_MEM_BASE
#define SMT_BUFFER0_BASE	SMT_BUFFER_BASE
#define RK_DBG_UART_BASE		UART2_BASE
#define RK_DBG_UART_BAUDRATE		1500000
#define RK_DBG_UART_CLOCK		24000000
#define SYS_COUNTER_FREQ_IN_TICKS	24000000
#define SYS_COUNTER_FREQ_IN_MHZ		24
#define PLAT_GICD_BASE			GIC600_BASE
#define PLAT_GICC_BASE			0
#define PLAT_GICR_BASE			(GIC600_BASE + 0x80000)
#define PLAT_GICITS0_BASE		0xfe640000
#define PLAT_GICITS1_BASE		0xfe660000
#define RK_IRQ_SEC_SGI_0		8
#define RK_IRQ_SEC_SGI_1		9
#define RK_IRQ_SEC_SGI_2		10
#define RK_IRQ_SEC_SGI_3		11
#define RK_IRQ_SEC_SGI_4		12
#define RK_IRQ_SEC_SGI_5		13
#define RK_IRQ_SEC_SGI_6		14
#define RK_IRQ_SEC_SGI_7		15
#define RK_IRQ_SEC_PHY_TIMER		29
#define PLAT_RK_GICV3_G1S_IRQS						\
	INTR_PROP_DESC(RK_IRQ_SEC_PHY_TIMER, GIC_HIGHEST_SEC_PRIORITY,	\
		       INTR_GROUP1S, GIC_INTR_CFG_LEVEL)
#define PLAT_RK_GICV3_G0_IRQS						\
	INTR_PROP_DESC(RK_IRQ_SEC_SGI_6, GIC_HIGHEST_SEC_PRIORITY,	\
		       INTR_GROUP0, GIC_INTR_CFG_LEVEL)
#define ROCKCHIP_PM_REG_REGION_MEM_SIZE		SIZE_K(4)
#define DSU_SGRF_SOC_CON(i)		((i) * 4)
#define DSUSGRF_SOC_CON(i)		((i) * 4)
#define DSUSGRF_SOC_CON_CNT		13
#define DSUSGRF_DDR_HASH_CON(i)		(0x240 + (i) * 4)
#define DSUSGRF_DDR_HASH_CON_CNT	8
#define PMU1SGRF_SOC_CON(n)		((n) * 4)
#define SGRF_SOC_CON(i)			((i) * 4)
#define SGRF_FIREWALL_CON(i)		(0x240 + (i) * 4)
#define SGRF_FIREWALL_CON_CNT		32
#define FIREWALL_DDR_RGN(i)		((i) * 0x4)
#define FIREWALL_DDR_RGN_CNT		16
#define FIREWALL_DDR_MST(i)		(0x40 + (i) * 0x4)
#define FIREWALL_DDR_MST_CNT		42
#define FIREWALL_DDR_CON		0xf0
#define FIREWALL_SYSMEM_RGN(i)		((i) * 0x4)
#define FIREWALL_SYSMEM_RGN_CNT		8
#define FIREWALL_SYSMEM_MST(i)		(0x40 + (i) * 0x4)
#define FIREWALL_SYSMEM_MST_CNT		43
#define FIREWALL_SYSMEM_CON		0xf0
#define FIREWALL_DSU_RGN(i)		((i) * 0x4)
#define FIREWALL_DSU_RGN_CNT		16
#define FIREWALL_DSU_MST(i)		(0x40 + (i) * 0x4)
#define FIREWALL_DSU_MST_CNT		2
#define FIREWALL_DSU_CON(i)		(0xf0 + (i) * 4)
#define FIREWALL_DSU_CON_CNT		4
#define PLAT_MAX_DDR_CAPACITY_MB	0x8000	/* for 32Gb */
#define RG_MAP_SECURE(top, base)	\
	(((((top) - 1) & 0x7fff) << 16) | ((base) & 0x7fff))
#define RG_MAP_SRAM_SECURE(top_kb, base_kb)	\
	(((((top_kb) / 4 - 1) & 0xff) << 16) | ((base_kb) / 4 & 0xff))
#define DDRGRF_CHA_CON(i)		((i) * 4)
#define DDRGRF_CHB_CON(i)		(0x30 + (i) * 4)
#define DDR_CHN_CNT			4

#if defined(CHCORE_LLM)
#define LLM_TEE_TZ_SIZE  0x7C00000
#endif
