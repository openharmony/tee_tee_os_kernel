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

#if defined(CHCORE_LLM)
#define LLM_TEE_LOW_MEM_START     0x02e00000UL
#define LLM_TEE_LOW_MEM_END       0x08000000UL
#define LLM_RKNPU_MEM_END         0x50000000UL
#define LLM_TEE_HIGH_MEM_START    0x60000000UL
#define LLM_TEE_HIGH_MEM_END      0xC0000000UL
#define LLM_TEE_ABOVE_16G_MEM_START 0x400000000UL
#define LLM_TEE_ABOVE_16G_MEM_END   0x700000000UL
#define LLM_PHYSMEM_MAP_NUM       5
#endif /* CHCORE_LLM */

static int rgn_id = 0;

static bool secure_ddr_rgn_used[FIREWALL_DDR_RGN_CNT];
static bool secure_ddr_rgn_dynamic[FIREWALL_DDR_RGN_CNT];
static paddr_t secure_ddr_rgn_start[FIREWALL_DDR_RGN_CNT];
static paddr_t secure_ddr_rgn_end[FIREWALL_DDR_RGN_CNT];
static int secure_ddr_rgn_used_count;
static bool secure_ddr_rgn_inited;

static void secure_ddr_rgn_init_once(void)
{
    if (secure_ddr_rgn_inited)
        return;

    /*
     * Keep rgn 0 reserved. Existing static mappings start from rgn 1 and
     * dynamic allocations will consume the remaining hardware regions.
     */
    secure_ddr_rgn_used[0] = true;
    secure_ddr_rgn_used_count = 1;
    secure_ddr_rgn_inited = true;
}

static int secure_ddr_rgn_check(int rgn)
{
    if (rgn < 0 || rgn >= FIREWALL_DDR_RGN_CNT || rgn >= FIREWALL_DSU_RGN_CNT)
        return -EINVAL;

    return 0;
}

static int secure_ddr_rgn_range(paddr_t st,
                                size_t sz,
                                paddr_t *rgn_start,
                                paddr_t *rgn_end)
{
    paddr_t ed;
    paddr_t fw_start;
    paddr_t fw_end;

    if (sz == 0 || rgn_start == NULL || rgn_end == NULL)
        return -EINVAL;

    ed = st + sz;
    if (ed <= st)
        return -ERANGE;

    if (ed > (~(paddr_t)0 - (SIZE_M(1) - 1)))
        return -ERANGE;

    fw_start = st / SIZE_M(1) * SIZE_M(1);
    fw_end = (ed + SIZE_M(1) - 1) / SIZE_M(1) * SIZE_M(1);
    if (fw_end <= fw_start)
        return -ERANGE;

    *rgn_start = fw_start;
    *rgn_end = fw_end;
    return 0;
}

static bool secure_ddr_rgn_overlaps(paddr_t st, paddr_t ed, int skip_rgn)
{
    int rgn;

    for (rgn = 0; rgn < FIREWALL_DDR_RGN_CNT; rgn++) {
        if (rgn == skip_rgn || !secure_ddr_rgn_used[rgn])
            continue;
        if (st < secure_ddr_rgn_end[rgn] && secure_ddr_rgn_start[rgn] < ed)
            return true;
    }

    return false;
}

static int secure_ddr_rgn_mark(int rgn,
                               paddr_t st,
                               paddr_t ed,
                               bool dynamic)
{
    int ret;

    ret = secure_ddr_rgn_check(rgn);
    if (ret != 0)
        return ret;
    if (ed <= st)
        return -EINVAL;

    secure_ddr_rgn_init_once();

    if (secure_ddr_rgn_used[rgn]) {
        if (secure_ddr_rgn_dynamic[rgn] != dynamic)
            return -EBUSY;

        if (secure_ddr_rgn_start[rgn] == 0 || st < secure_ddr_rgn_start[rgn])
            secure_ddr_rgn_start[rgn] = st;
        if (ed > secure_ddr_rgn_end[rgn])
            secure_ddr_rgn_end[rgn] = ed;
        return 0;
    }

    secure_ddr_rgn_used[rgn] = true;
    secure_ddr_rgn_dynamic[rgn] = dynamic;
    secure_ddr_rgn_start[rgn] = st;
    secure_ddr_rgn_end[rgn] = ed;
    secure_ddr_rgn_used_count++;

    return 0;
}

static int secure_ddr_region_apply(int rgn, paddr_t rgn_start, paddr_t rgn_end)
{
    vaddr_t fw_ddr_base = phys_to_virt(FIREWALL_DDR_BASE);
    vaddr_t fw_dsu_base = phys_to_virt(FIREWALL_DSU_BASE);

    int i;
    paddr_t st_mb = 0;
    paddr_t ed_mb = 0;

    if (rgn < 0 || rgn >= FIREWALL_DDR_RGN_CNT || rgn >= FIREWALL_DSU_RGN_CNT) {
		return -1;
	}
    st_mb = rgn_start / SIZE_M(1);
    ed_mb = (rgn_end - 1) / SIZE_M(1);

    put32(fw_ddr_base + FIREWALL_DDR_RGN(rgn), RG_MAP_SECURE(ed_mb, st_mb));
    put32(fw_dsu_base + FIREWALL_DSU_RGN(rgn), RG_MAP_SECURE(ed_mb, st_mb));

    for (i = 0; i < DDR_CHN_CNT; i++) {
        put32(fw_dsu_base + FIREWALL_DSU_CON(i),
               BIT(rgn) | get32(fw_dsu_base + FIREWALL_DSU_CON(i)));
    }
    put32(fw_ddr_base + FIREWALL_DDR_CON,
           BIT(rgn) | get32(fw_ddr_base + FIREWALL_DDR_CON));

    return 0;
}

static int secure_ddr_region(int rgn, paddr_t st, size_t sz)
{
    paddr_t rgn_start;
    paddr_t rgn_end;
    int ret;

    ret = secure_ddr_rgn_check(rgn);
    if (ret != 0)
        return ret;

    ret = secure_ddr_rgn_range(st, sz, &rgn_start, &rgn_end);
    if (ret != 0)
        return ret;

    ret = secure_ddr_region_apply(rgn, rgn_start, rgn_end);
    if (ret != 0)
        return ret;

    return secure_ddr_rgn_mark(rgn, rgn_start, rgn_end, false);
}

int secure_ddr_region_alloc(paddr_t st, size_t sz, int *rgn_out)
{
    int rgn;
    int ret;
    paddr_t rgn_start;
    paddr_t rgn_end;

    if (rgn_out == NULL)
        return -EINVAL;

    secure_ddr_rgn_init_once();

    ret = secure_ddr_rgn_range(st, sz, &rgn_start, &rgn_end);
    if (ret != 0)
        return ret;

    if (secure_ddr_rgn_overlaps(rgn_start, rgn_end, -1)) {
        printk("secure_ddr_region_alloc: reject overlap paddr=%lx size=%lu "
               "range=[%lx,%lx)\n",
               st,
               sz,
               rgn_start,
               rgn_end);
        return -EBUSY;
    }

    for (rgn = 0; rgn < FIREWALL_DDR_RGN_CNT; rgn++) {
        if (secure_ddr_rgn_used[rgn])
            continue;

        ret = secure_ddr_region_apply(rgn, rgn_start, rgn_end);
        if (ret != 0)
            return ret;

        ret = secure_ddr_rgn_mark(rgn, rgn_start, rgn_end, true);
        if (ret != 0)
            return ret;

        *rgn_out = rgn;
        return 0;
    }

    return -ENOSPC;
}

int secure_ddr_region_free(int rgn)
{
    vaddr_t fw_ddr_base = phys_to_virt(FIREWALL_DDR_BASE);
    vaddr_t fw_dsu_base = phys_to_virt(FIREWALL_DSU_BASE);
    int ret;
    int i;

    ret = secure_ddr_rgn_check(rgn);
    if (ret != 0)
        return ret;

    secure_ddr_rgn_init_once();

    if (!secure_ddr_rgn_used[rgn])
        return -ENOENT;
    if (!secure_ddr_rgn_dynamic[rgn])
        return -EPERM;

    /*
     * Disable DDR firewall region.
     */
    put32(fw_ddr_base + FIREWALL_DDR_CON,
          get32(fw_ddr_base + FIREWALL_DDR_CON) & ~BIT(rgn));

    /*
     * Clear DDR region map.
     */
    put32(fw_ddr_base + FIREWALL_DDR_RGN(rgn), 0);

    /*
     * secure_ddr_region() writes FIREWALL_DSU_RGN(i) for each DDR channel i,
     * so free must clear FIREWALL_DSU_RGN(i) in the same loop.
     */
    for (i = 0; i < DDR_CHN_CNT; i++) {
        put32(fw_dsu_base + FIREWALL_DSU_CON(i),
              get32(fw_dsu_base + FIREWALL_DSU_CON(i)) & ~BIT(rgn));
    }
    put32(fw_dsu_base + FIREWALL_DSU_RGN(rgn), 0);

    secure_ddr_rgn_used[rgn] = false;
    secure_ddr_rgn_dynamic[rgn] = false;
    secure_ddr_rgn_start[rgn] = 0;
    secure_ddr_rgn_end[rgn] = 0;
    if (secure_ddr_rgn_used_count > 0)
        secure_ddr_rgn_used_count--;

    printk("secure_ddr_region_free: freed rgn %d\n", rgn);

    return 0;
}

int secure_ddr_region_total(void)
{
    secure_ddr_rgn_init_once();
    return FIREWALL_DDR_RGN_CNT;
}

int secure_ddr_region_used(void)
{
    secure_ddr_rgn_init_once();
    return secure_ddr_rgn_used_count;
}

void parse_mem_map(void *info)
{
    extern char img_end;

	paddr_t tz_start = get_tzdram_start();
	paddr_t tz_end = get_tzdram_end();
	size_t tz_size = tz_end - tz_start;

    secure_ddr_region(1, tz_start, tz_size);
#if defined(CHCORE_LLM)
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
#if defined(CHCORE_LLM)
    physmem_map_num = LLM_PHYSMEM_MAP_NUM;
#endif
    physmem_map[0][0] = ROUND_UP((paddr_t)&img_end, PAGE_SIZE);
    physmem_map[0][1] = ROUND_DOWN(get_tzdram_end(), PAGE_SIZE);
#if defined(CHCORE_LLM)
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
