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
#include <machine.h>
#include <common/errno.h>
#include <mm/mm.h>
#include <common/macro.h>
#include <common/types.h>
#include <arch/tools.h>
#include <rk_atags.h>

#ifdef CHCORE_LLM
#define LLM_TEE_LOW_MEM_START     0x02e00000UL
#define LLM_TEE_LOW_MEM_END       0x08000000UL
#define LLM_RKNPU_MEM_END         0x50000000UL
#define LLM_TEE_HIGH_MEM_START    0x60000000UL
#define LLM_TEE_HIGH_MEM_END      0xC0000000UL
#define LLM_TEE_ABOVE_16G_MEM_START 0x400000000UL
#define LLM_TEE_ABOVE_16G_MEM_END   0x700000000UL
#define LLM_PHYSMEM_MAP_NUM       5
#endif /* CHCORE_LLM */

static int secure_ddr_region(int rgn, paddr_t st, size_t sz)
{
    vaddr_t fw_ddr_base = phys_to_virt(FIREWALL_DDR_BASE);
    vaddr_t fw_dsu_base = phys_to_virt(FIREWALL_DSU_BASE);

    int i;
    paddr_t st_mb = st / SIZE_M(1);
    paddr_t ed_mb = (st + sz - 1) / SIZE_M(1);

    if (rgn < 0 || rgn >= FIREWALL_DDR_RGN_CNT || rgn >= FIREWALL_DSU_RGN_CNT) {
		return -1;
	}

    put32(fw_ddr_base + FIREWALL_DDR_RGN(rgn), RG_MAP_SECURE(ed_mb, st_mb));

    for (i = 0; i < DDR_CHN_CNT; i++) {
        put32(fw_dsu_base + FIREWALL_DSU_RGN(i), RG_MAP_SECURE(ed_mb, st_mb));
        put32(fw_dsu_base + FIREWALL_DSU_CON(i),
               BIT(rgn) | get32(fw_dsu_base + FIREWALL_DSU_CON(i)));
    }
    put32(fw_ddr_base + FIREWALL_DDR_CON,
           BIT(rgn) | get32(fw_ddr_base + FIREWALL_DDR_CON));

    return 0;
}

void parse_mem_map(void *info)
{
    extern char img_end;

	paddr_t tz_start = get_tzdram_start();
	paddr_t tz_end = get_tzdram_end();
	size_t tz_size = tz_end - tz_start;

    secure_ddr_region(1, tz_start, tz_size);
#ifdef CHCORE_LLM
    secure_ddr_region(2,
                      RKNPU_IOMMU_LOW_PT_BASE,
                      LLM_RKNPU_MEM_END - RKNPU_IOMMU_LOW_PT_BASE);
    secure_ddr_region(3,
                      LLM_TEE_LOW_MEM_START,
                      LLM_TEE_LOW_MEM_END - LLM_TEE_LOW_MEM_START);
    secure_ddr_region(4,
                      LLM_TEE_HIGH_MEM_START,
                      LLM_TEE_HIGH_MEM_END - LLM_TEE_HIGH_MEM_START);
    secure_ddr_region(5,
                      LLM_TEE_ABOVE_16G_MEM_START,
                      LLM_TEE_ABOVE_16G_MEM_END
                          - LLM_TEE_ABOVE_16G_MEM_START);
#endif

    physmem_map_num = 1;
#ifdef HIGH_SECURE_DEBUG
    physmem_map_num = 4;
#endif
#ifdef CHCORE_LLM
    physmem_map_num = LLM_PHYSMEM_MAP_NUM;
#endif
    physmem_map[0][0] = ROUND_UP((paddr_t)&img_end, PAGE_SIZE);
    physmem_map[0][1] = ROUND_DOWN(get_tzdram_end(), PAGE_SIZE);
#ifdef CHCORE_LLM
    physmem_map[1][0] = ROUND_UP(LLM_TEE_LOW_MEM_START, PAGE_SIZE);
    physmem_map[1][1] = ROUND_DOWN(LLM_TEE_LOW_MEM_END, PAGE_SIZE);
    physmem_map[2][0] = ROUND_UP(RKNPU_IOMMU_LOW_PT_END, PAGE_SIZE);
    physmem_map[2][1] = ROUND_DOWN(LLM_RKNPU_MEM_END, PAGE_SIZE);
    physmem_map[3][0] = ROUND_UP(LLM_TEE_HIGH_MEM_START, PAGE_SIZE);
    physmem_map[3][1] = ROUND_DOWN(LLM_TEE_HIGH_MEM_END, PAGE_SIZE);
    physmem_map[4][0] = ROUND_UP(LLM_TEE_ABOVE_16G_MEM_START, PAGE_SIZE);
    physmem_map[4][1] = ROUND_DOWN(LLM_TEE_ABOVE_16G_MEM_END, PAGE_SIZE);
#endif

    kinfo("[ChCore] physmem_map: [0x%lx, 0x%lx)\n",
          physmem_map[0][0],
          physmem_map[0][1]);
    struct tag_tos_mem tag_tos_mem;
    memset(&tag_tos_mem, 0, sizeof(tag_tos_mem));
    memcpy(tag_tos_mem.tee_mem.name, "tee.mem", 8);
    tag_tos_mem.tee_mem.phy_addr = get_tzdram_start();
    tag_tos_mem.tee_mem.size = tz_size;
    tag_tos_mem.tee_mem.flags = 1;
    memcpy(tag_tos_mem.drm_mem.name, "drm.mem", 8);
    tag_tos_mem.drm_mem.phy_addr = 0;
    tag_tos_mem.drm_mem.size = 0;
    tag_tos_mem.drm_mem.flags = 0;
    tag_tos_mem.version = 65536;
    set_tos_mem_tag(&tag_tos_mem);
}
