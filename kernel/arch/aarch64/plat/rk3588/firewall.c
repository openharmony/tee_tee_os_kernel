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

#define FIREWALL_DDR_MST_CORE0 11
#define FIREWALL_DDR_MST_CORE1 32
#define FIREWALL_DDR_MST_CORE2 2
#define FIREWALL_DDR_MST_CORE0_40BIT 20

#define FIREWALL_DDR_MST_CORE_MASK 0xffff0000U
#define FIREWALL_DDR_MST_40BIT_MASK 0x0000ffffU

#define FIREWALL_CMA_REGION_BASE 8
#define FIREWALL_CMA_REGION_COUNT 4

struct npu_ddr_master {
    int core_index;
    u32 master_index;
    u32 access_mask;
};

static const struct npu_ddr_master npu_ddr_masters[] = {
    {0, FIREWALL_DDR_MST_CORE0, FIREWALL_DDR_MST_CORE_MASK},
    {1, FIREWALL_DDR_MST_CORE1, FIREWALL_DDR_MST_CORE_MASK},
    {2, FIREWALL_DDR_MST_CORE2, FIREWALL_DDR_MST_CORE_MASK},
    {0, FIREWALL_DDR_MST_CORE0_40BIT,
     FIREWALL_DDR_MST_40BIT_MASK},
};

static inline void set32(vaddr_t addr, u32 set_mask)
{
    put32(addr, get32(addr) | set_mask);
}
static inline void clr32(vaddr_t addr, u32 set_mask)
{
    put32(addr, get32(addr) & ~set_mask);
}

void switch_firewall_ddr(int secure, int core_index)
{
    vaddr_t firewall_ddr_vaddr = phys_to_virt(FIREWALL_DDR_BASE);
    unsigned int i;

    for (i = 0; i < sizeof(npu_ddr_masters)
                    / sizeof(npu_ddr_masters[0]);
         i++) {
        const struct npu_ddr_master *master =
            &npu_ddr_masters[i];
        vaddr_t addr;

        if (master->core_index != core_index)
            continue;

        addr = firewall_ddr_vaddr + FIREWALL_DDR_MST(master->master_index);
        if (secure)
            clr32(addr, master->access_mask);
        else
            set32(addr, master->access_mask);
    }
}

/* unit: Mb */
int dsu_fw_rgn_alter(unsigned long base_mb, unsigned long top_mb, int rgn_id)
{
    vaddr_t firewall_dsu_base = phys_to_virt(FIREWALL_DSU_BASE);

	if (rgn_id >= FIREWALL_DSU_RGN_CNT || rgn_id < 0) {
        return -1;
	}

	put32(firewall_dsu_base + FIREWALL_DSU_RGN(rgn_id),
		      RG_MAP_SECURE(top_mb, base_mb));

    return 0;
}

int dsu_fw_rgn_enable(int rgn_id, bool enable)
{
	int i;
    vaddr_t firewall_dsu_base = phys_to_virt(FIREWALL_DSU_BASE);

	if (rgn_id >= FIREWALL_DSU_RGN_CNT || rgn_id < 0) {
        return -1;
	}

    for (i = 0; i < DDR_CHN_CNT; i++) {
        if (enable) {
            set32(firewall_dsu_base + FIREWALL_DSU_CON(i), BIT(rgn_id));
        } else {
            clr32(firewall_dsu_base + FIREWALL_DSU_CON(i), BIT(rgn_id));
        }
    }

    return 0;
}

/* unit: Mb */
int ddr_fw_rgn_alter(unsigned long base_mb, unsigned long top_mb, int rgn_id)
{
    vaddr_t firewall_ddr_base = phys_to_virt(FIREWALL_DDR_BASE);

    if (rgn_id >= FIREWALL_DDR_RGN_CNT || rgn_id < 0) {
        return -1;
    }

    put32(firewall_ddr_base + FIREWALL_DDR_RGN(rgn_id),
                RG_MAP_SECURE(top_mb, base_mb));

    return 0;
}

int ddr_fw_rgn_enable(int rgn_id, bool enable) {
    vaddr_t firewall_ddr_base = phys_to_virt(FIREWALL_DDR_BASE);
    if (rgn_id >= FIREWALL_DDR_RGN_CNT || rgn_id < 0) {
        return -1;
    }

    if (enable) {
        /* enable region */
        set32(firewall_ddr_base + FIREWALL_DDR_CON, BIT(rgn_id));
    } else {
        clr32(firewall_ddr_base + FIREWALL_DDR_CON, BIT(rgn_id));
    }

    return 0;
}

void firewall_ddr_cma_rgn_init(void) {
    for (int rgn = 0; rgn < FIREWALL_CMA_REGION_COUNT; rgn++) {
        int rgn_id = rgn + FIREWALL_CMA_REGION_BASE;

        dsu_fw_rgn_alter(0, 0, rgn_id);
        dsu_fw_rgn_enable(rgn_id, true);
        ddr_fw_rgn_alter(0, 0, rgn_id);
        ddr_fw_rgn_enable(rgn_id, true);
    }
}
