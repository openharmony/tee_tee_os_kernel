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
#include <arch/mmu.h>
#include <arch/mm/cache.h>
#include <arch/sync.h>
#include <arch/tools.h>
#include <common/errno.h>
#include <common/kprint.h>
#include <common/lock.h>
#include <common/macro.h>
#include <common/types.h>
#include <common/util.h>
#include <machine.h>
#include <mm/kmalloc.h>
#include <mm/uaccess.h>
#include <rknpu.h>

#define RKNPU_IOMMU_MMIO_SIZE 0x100UL

#define RKNPU_IOMMU_BIT(nr) (1UL << (nr))
#define RKNPU_IOMMU_GENMASK_ULL(h, l) \
    (((~0ULL) >> (63U - (h))) & (~0ULL << (l)))

#define RKNPU_IOMMU_REG_DTE_ADDR        0x00U
#define RKNPU_IOMMU_REG_STATUS          0x04U
#define RKNPU_IOMMU_REG_COMMAND         0x08U
#define RKNPU_IOMMU_REG_PAGE_FAULT_ADDR 0x0CU
#define RKNPU_IOMMU_REG_INT_RAWSTAT     0x14U
#define RKNPU_IOMMU_REG_INT_CLEAR       0x18U
#define RKNPU_IOMMU_REG_INT_MASK        0x1CU
#define RKNPU_IOMMU_REG_INT_STATUS      0x20U

#define RKNPU_IOMMU_CMD_ENABLE_PAGING   0U
#define RKNPU_IOMMU_CMD_DISABLE_PAGING  1U
#define RKNPU_IOMMU_CMD_ENABLE_STALL    2U
#define RKNPU_IOMMU_CMD_DISABLE_STALL   3U
#define RKNPU_IOMMU_CMD_ZAP_CACHE       4U
#define RKNPU_IOMMU_CMD_PAGE_FAULT_DONE 5U

#define RKNPU_IOMMU_STATUS_PAGING_ENABLED    RKNPU_IOMMU_BIT(0U)
#define RKNPU_IOMMU_STATUS_PAGE_FAULT_ACTIVE RKNPU_IOMMU_BIT(1U)
#define RKNPU_IOMMU_STATUS_STALL_ACTIVE      RKNPU_IOMMU_BIT(2U)

#define RKNPU_IOMMU_IRQ_PAGE_FAULT RKNPU_IOMMU_BIT(0U)
#define RKNPU_IOMMU_IRQ_BUS_ERROR  RKNPU_IOMMU_BIT(1U)
#define RKNPU_IOMMU_IRQ_MASK \
    (RKNPU_IOMMU_IRQ_PAGE_FAULT | RKNPU_IOMMU_IRQ_BUS_ERROR)

#define RKNPU_IOMMU_PAGE_SHIFT 12UL
#define RKNPU_IOMMU_PAGE_SIZE  (1UL << RKNPU_IOMMU_PAGE_SHIFT)
#define RKNPU_IOMMU_PAGE_MASK  (RKNPU_IOMMU_PAGE_SIZE - 1UL)

#define RKNPU_IOMMU_DTE_ENTRIES 1024U
#define RKNPU_IOMMU_PTE_ENTRIES 1024U
#define RKNPU_IOMMU_PT_RANGE \
    (RKNPU_IOMMU_PAGE_SIZE * RKNPU_IOMMU_PTE_ENTRIES)

#define RKNPU_IOMMU_IOVA_ADDR_BITS 32UL
#define RKNPU_IOMMU_IOVA_BASE      0x01000000UL
#define RKNPU_IOMMU_IOVA_LIMIT     (1UL << RKNPU_IOMMU_IOVA_ADDR_BITS)

#define RKNPU_IOMMU_PHYS_ADDR_BITS  40UL
#define RKNPU_IOMMU_PHYS_ADDR_LIMIT (1UL << RKNPU_IOMMU_PHYS_ADDR_BITS)

#define RKNPU_IOMMU_DTE_VALID       RKNPU_IOMMU_BIT(0U)
#define RKNPU_IOMMU_DTE_ADDR_MASK   0xfffffff0U
#define RKNPU_IOMMU_DTE_LO_MASK     0xfffff000UL
#define RKNPU_IOMMU_DTE_HI_MASK     RKNPU_IOMMU_GENMASK_ULL(39U, 32U)
#define RKNPU_IOMMU_DTE_HI_SHIFT    28U
#define RKNPU_IOMMU_DTE_REG_HI_MASK 0x00000ff0UL

#define RKNPU_IOMMU_PTE_VALID     RKNPU_IOMMU_BIT(0U)
#define RKNPU_IOMMU_PTE_READABLE  RKNPU_IOMMU_BIT(1U)
#define RKNPU_IOMMU_PTE_WRITABLE  RKNPU_IOMMU_BIT(2U)
#define RKNPU_IOMMU_PTE_ADDR_MASK 0xfffffff0U
#define RKNPU_IOMMU_PTE_LO_MASK   0xfffff000UL

#define RKNPU_IOMMU_ADDR_HI1_LOWER 32U
#define RKNPU_IOMMU_ADDR_HI1_UPPER 35U
#define RKNPU_IOMMU_ADDR_HI2_LOWER 36U
#define RKNPU_IOMMU_ADDR_HI2_UPPER 39U
#define RKNPU_IOMMU_DESC_HI1_LOWER 8U
#define RKNPU_IOMMU_DESC_HI1_UPPER 11U
#define RKNPU_IOMMU_DESC_HI2_LOWER 4U
#define RKNPU_IOMMU_DESC_HI2_UPPER 7U
#define RKNPU_IOMMU_ADDR_HI_MASK1 \
    RKNPU_IOMMU_GENMASK_ULL(RKNPU_IOMMU_ADDR_HI1_UPPER, \
                            RKNPU_IOMMU_ADDR_HI1_LOWER)
#define RKNPU_IOMMU_ADDR_HI_MASK2 \
    RKNPU_IOMMU_GENMASK_ULL(RKNPU_IOMMU_ADDR_HI2_UPPER, \
                            RKNPU_IOMMU_ADDR_HI2_LOWER)
#define RKNPU_IOMMU_DESC_HI_MASK1 \
    RKNPU_IOMMU_GENMASK_ULL(RKNPU_IOMMU_DESC_HI1_UPPER, \
                            RKNPU_IOMMU_DESC_HI1_LOWER)
#define RKNPU_IOMMU_DESC_HI_MASK2 \
    RKNPU_IOMMU_GENMASK_ULL(RKNPU_IOMMU_DESC_HI2_UPPER, \
                            RKNPU_IOMMU_DESC_HI2_LOWER)
#define RKNPU_IOMMU_ADDR_HI_SHIFT1 \
    (RKNPU_IOMMU_ADDR_HI1_LOWER - RKNPU_IOMMU_DESC_HI1_LOWER)
#define RKNPU_IOMMU_ADDR_HI_SHIFT2 \
    (RKNPU_IOMMU_ADDR_HI2_LOWER - RKNPU_IOMMU_DESC_HI2_LOWER)

#define RKNPU_IOMMU_IOVA_DTE_MASK  0xffc00000UL
#define RKNPU_IOMMU_IOVA_DTE_SHIFT 22U
#define RKNPU_IOMMU_IOVA_PTE_MASK  0x003ff000UL
#define RKNPU_IOMMU_IOVA_PTE_SHIFT 12U

#define RKNPU_IOMMU_CMD_POLL_LIMIT 100000U

#define RKNPU_IOMMU_LOW_PT_PAGES \
    (RKNPU_IOMMU_LOW_PT_SIZE / RKNPU_IOMMU_PAGE_SIZE)
#define RKNPU_IOMMU_REQUIRED_PT_PAGES (RKNPU_IOMMU_DTE_ENTRIES + 1U)

#define RKNPU_IOMMU_MMIO_COUNT 4U
#define RKNPU_IOMMU0_BASE      0xfdab9000UL
#define RKNPU_IOMMU1_BASE      0xfdaba000UL
#define RKNPU_IOMMU2_BASE      0xfdaca000UL
#define RKNPU_IOMMU3_BASE      0xfdada000UL

struct rknpu_iova_region {
    unsigned long start;
    unsigned long size;
    struct rknpu_iova_region *next;
};

struct rknpu_iommu_pt {
    u32 *vaddr;
    unsigned long paddr;
};

struct rknpu_iommu_domain {
    u32 *dt;
    unsigned long dt_paddr;
    struct rknpu_iommu_pt pts[RKNPU_IOMMU_DTE_ENTRIES];
    struct rknpu_iova_region *free_regions;
    bool tables_ready;
};

struct rknpu_iommu {
    vaddr_t base[RKNPU_IOMMU_MMIO_COUNT];
    struct rknpu_iommu_domain domain;
    struct lock lock;
    bool lock_ready;
    bool hw_ready;
};

static struct rknpu_iommu rknpu_iommu_dev;

static const unsigned long rknpu_iommu_base_paddr[RKNPU_IOMMU_MMIO_COUNT] = {
    RKNPU_IOMMU0_BASE,
    RKNPU_IOMMU1_BASE,
    RKNPU_IOMMU2_BASE,
    RKNPU_IOMMU3_BASE,
};

static inline u32 rknpu_iommu_read(vaddr_t base, unsigned int offset)
{
    return get32(base + offset);
}

static inline void rknpu_iommu_write(vaddr_t base,
                                     unsigned int offset,
                                     unsigned int value)
{
    put32(base + offset, value);
}

static inline void rknpu_iommu_barrier(void)
{
    dsb(sy);
}

static void rknpu_iommu_clean_cache(void *addr, size_t size)
{
    if (addr == NULL || size == 0UL)
        return;

    arch_flush_cache((vaddr_t)addr, size, CACHE_CLEAN);
}

static void rknpu_iommu_clean_dt(struct rknpu_iommu_domain *domain)
{
    rknpu_iommu_clean_cache(domain->dt, RKNPU_IOMMU_PAGE_SIZE);
}

static void rknpu_iommu_clean_pt_range(struct rknpu_iommu_pt *pt,
                                       unsigned int pte_index,
                                       unsigned long nr_pages)
{
    rknpu_iommu_clean_cache(&pt->vaddr[pte_index],
                            nr_pages * sizeof(pt->vaddr[0]));
}

static void rknpu_iommu_clean_all_pts(struct rknpu_iommu_domain *domain)
{
    unsigned int i;

    for (i = 0; i < RKNPU_IOMMU_DTE_ENTRIES; i++)
        rknpu_iommu_clean_cache(domain->pts[i].vaddr,
                                RKNPU_IOMMU_PAGE_SIZE);
}

static inline unsigned long rknpu_iommu_align_down(unsigned long value)
{
    return value & ~RKNPU_IOMMU_PAGE_MASK;
}

static inline unsigned long rknpu_iommu_align_up(unsigned long value)
{
    return (value + RKNPU_IOMMU_PAGE_MASK) & ~RKNPU_IOMMU_PAGE_MASK;
}

static inline bool rknpu_iommu_range_overflows(unsigned long start,
                                               unsigned long size)
{
    return size > (~0UL - start);
}

static inline unsigned int rknpu_iommu_dte_index(unsigned long iova)
{
    return (iova & RKNPU_IOMMU_IOVA_DTE_MASK) >>
           RKNPU_IOMMU_IOVA_DTE_SHIFT;
}

static inline unsigned int rknpu_iommu_pte_index(unsigned long iova)
{
    return (iova & RKNPU_IOMMU_IOVA_PTE_MASK) >>
           RKNPU_IOMMU_IOVA_PTE_SHIFT;
}

static u32 rknpu_iommu_encode_phys(unsigned long paddr,
                                   unsigned int addr_mask)
{
    unsigned long desc;

    desc = (paddr & RKNPU_IOMMU_PTE_LO_MASK) |
           ((paddr & RKNPU_IOMMU_ADDR_HI_MASK1) >>
            RKNPU_IOMMU_ADDR_HI_SHIFT1) |
           ((paddr & RKNPU_IOMMU_ADDR_HI_MASK2) >>
            RKNPU_IOMMU_ADDR_HI_SHIFT2);

    return (u32)(desc & addr_mask);
}

static u32 rknpu_iommu_mk_dte(unsigned long pt_paddr)
{
    return rknpu_iommu_encode_phys(pt_paddr, RKNPU_IOMMU_DTE_ADDR_MASK) |
           RKNPU_IOMMU_DTE_VALID;
}

static u32 rknpu_iommu_mk_pte(unsigned long paddr, unsigned int prot)
{
    u32 flags = RKNPU_IOMMU_PTE_VALID;

    if ((prot & RKNPU_IOMMU_PROT_READ) != 0U)
        flags |= RKNPU_IOMMU_PTE_READABLE;
    if ((prot & RKNPU_IOMMU_PROT_WRITE) != 0U)
        flags |= RKNPU_IOMMU_PTE_WRITABLE;

    return rknpu_iommu_encode_phys(paddr, RKNPU_IOMMU_PTE_ADDR_MASK) |
           flags;
}

static u32 rknpu_iommu_encode_dt_addr(unsigned long dt_paddr)
{
    return (u32)((dt_paddr & RKNPU_IOMMU_DTE_LO_MASK) |
                 ((dt_paddr & RKNPU_IOMMU_DTE_HI_MASK) >>
                  RKNPU_IOMMU_DTE_HI_SHIFT));
}

static unsigned long rknpu_iommu_decode_dt_addr(u32 dte_addr)
{
    unsigned long addr = dte_addr;

    return (addr & RKNPU_IOMMU_DTE_LO_MASK) |
           ((addr & RKNPU_IOMMU_DTE_REG_HI_MASK) <<
            RKNPU_IOMMU_DTE_HI_SHIFT);
}

static unsigned long rknpu_iommu_decode_desc_addr(u32 desc)
{
    unsigned long addr = desc;

    return (addr & RKNPU_IOMMU_PTE_LO_MASK) |
           ((addr & RKNPU_IOMMU_DESC_HI_MASK1) <<
            RKNPU_IOMMU_ADDR_HI_SHIFT1) |
           ((addr & RKNPU_IOMMU_DESC_HI_MASK2) <<
            RKNPU_IOMMU_ADDR_HI_SHIFT2);
}

static bool rknpu_iommu_any_status(struct rknpu_iommu *iommu,
                                   unsigned int mask)
{
    unsigned int i;

    for (i = 0; i < RKNPU_IOMMU_MMIO_COUNT; i++) {
        if ((rknpu_iommu_read(iommu->base[i], RKNPU_IOMMU_REG_STATUS)
             & mask)
            != 0U)
            return true;
    }

    return false;
}

static bool rknpu_iommu_all_status(struct rknpu_iommu *iommu,
                                   unsigned int mask)
{
    unsigned int i;

    for (i = 0; i < RKNPU_IOMMU_MMIO_COUNT; i++) {
        if ((rknpu_iommu_read(iommu->base[i], RKNPU_IOMMU_REG_STATUS)
             & mask)
            == 0U)
            return false;
    }

    return true;
}

static void rknpu_iommu_command(struct rknpu_iommu *iommu,
                                unsigned int command)
{
    unsigned int i;

    for (i = 0; i < RKNPU_IOMMU_MMIO_COUNT; i++)
        rknpu_iommu_write(
            iommu->base[i], RKNPU_IOMMU_REG_COMMAND, command);
    rknpu_iommu_barrier();
}

static int rknpu_iommu_wait_status(struct rknpu_iommu *iommu,
                                   unsigned int mask,
                                   bool set)
{
    unsigned int i;

    for (i = 0; i < RKNPU_IOMMU_CMD_POLL_LIMIT; i++) {
        bool done = set ? rknpu_iommu_all_status(iommu, mask) :
                          !rknpu_iommu_any_status(iommu, mask);

        if (done)
            return 0;
        asm volatile("yield" ::: "memory");
    }

    return -ETIMEDOUT;
}

static int rknpu_iommu_zap(struct rknpu_iommu *iommu)
{
    rknpu_iommu_command(iommu, RKNPU_IOMMU_CMD_ZAP_CACHE);
    return 0;
}

static int rknpu_iommu_stop_locked(struct rknpu_iommu *iommu)
{
    int ret;

    if (rknpu_iommu_any_status(iommu, RKNPU_IOMMU_STATUS_PAGING_ENABLED)
        && !rknpu_iommu_all_status(iommu, RKNPU_IOMMU_STATUS_STALL_ACTIVE)) {
        rknpu_iommu_command(iommu, RKNPU_IOMMU_CMD_ENABLE_STALL);
        ret = rknpu_iommu_wait_status(
            iommu, RKNPU_IOMMU_STATUS_STALL_ACTIVE, true);
        if (ret != 0)
            return ret;
    }

    if (rknpu_iommu_any_status(iommu, RKNPU_IOMMU_STATUS_PAGING_ENABLED)) {
        rknpu_iommu_command(iommu, RKNPU_IOMMU_CMD_DISABLE_PAGING);
        ret = rknpu_iommu_wait_status(
            iommu, RKNPU_IOMMU_STATUS_PAGING_ENABLED, false);
        if (ret != 0)
            return ret;
    }

    return 0;
}

static int rknpu_iommu_clear_faults_locked(struct rknpu_iommu *iommu)
{
    unsigned int i;

    for (i = 0; i < RKNPU_IOMMU_MMIO_COUNT; i++)
        rknpu_iommu_write(iommu->base[i],
                          RKNPU_IOMMU_REG_INT_CLEAR,
                          RKNPU_IOMMU_IRQ_MASK);
    rknpu_iommu_barrier();

    if (!rknpu_iommu_any_status(iommu,
                                RKNPU_IOMMU_STATUS_PAGE_FAULT_ACTIVE))
        return 0;

    rknpu_iommu_command(iommu, RKNPU_IOMMU_CMD_PAGE_FAULT_DONE);
    return rknpu_iommu_wait_status(
        iommu, RKNPU_IOMMU_STATUS_PAGE_FAULT_ACTIVE, false);
}

static int rknpu_iommu_start_locked(struct rknpu_iommu *iommu)
{
    int ret;

    rknpu_iommu_command(iommu, RKNPU_IOMMU_CMD_ENABLE_PAGING);
    ret = rknpu_iommu_wait_status(
        iommu, RKNPU_IOMMU_STATUS_PAGING_ENABLED, true);
    if (ret != 0)
        return ret;

    if (rknpu_iommu_any_status(iommu, RKNPU_IOMMU_STATUS_STALL_ACTIVE)) {
        rknpu_iommu_command(iommu, RKNPU_IOMMU_CMD_DISABLE_STALL);
        ret = rknpu_iommu_wait_status(
            iommu, RKNPU_IOMMU_STATUS_STALL_ACTIVE, false);
        if (ret != 0)
            return ret;
    }

    return 0;
}

static void rknpu_iova_free_all(struct rknpu_iommu_domain *domain)
{
    struct rknpu_iova_region *cur = domain->free_regions;

    while (cur != NULL) {
        struct rknpu_iova_region *next = cur->next;

        kfree(cur);
        cur = next;
    }

    domain->free_regions = NULL;
}

static int rknpu_iova_insert_locked(struct rknpu_iommu_domain *domain,
                                    unsigned long start,
                                    unsigned long size)
{
    struct rknpu_iova_region *prev = NULL;
    struct rknpu_iova_region *cur = domain->free_regions;
    struct rknpu_iova_region *node;
    unsigned long end = start + size;

    while (cur != NULL && cur->start < start) {
        prev = cur;
        cur = cur->next;
    }

    if (prev != NULL && prev->start + prev->size > start)
        return -EINVAL;
    if (cur != NULL && end > cur->start)
        return -EINVAL;

    if (prev != NULL && prev->start + prev->size == start) {
        prev->size += size;
        if (cur != NULL && prev->start + prev->size == cur->start) {
            prev->size += cur->size;
            prev->next = cur->next;
            kfree(cur);
        }
        return 0;
    }

    if (cur != NULL && end == cur->start) {
        cur->start = start;
        cur->size += size;
        return 0;
    }

    node = kmalloc(sizeof(*node));
    if (node == NULL)
        return -ENOMEM;

    node->start = start;
    node->size = size;
    node->next = cur;
    if (prev != NULL)
        prev->next = node;
    else
        domain->free_regions = node;

    return 0;
}

static int rknpu_iova_alloc_locked(struct rknpu_iommu_domain *domain,
                                   unsigned long size,
                                   unsigned long *ret_iova)
{
    struct rknpu_iova_region *prev = NULL;
    struct rknpu_iova_region *cur = domain->free_regions;

    while (cur != NULL) {
        unsigned long start = rknpu_iommu_align_up(cur->start);
        unsigned long padding = start - cur->start;

        if (cur->size >= padding + size) {
            unsigned long tail_start = start + size;
            unsigned long tail_size = cur->size - padding - size;

            if (padding == 0UL && tail_size == 0UL) {
                if (prev != NULL)
                    prev->next = cur->next;
                else
                    domain->free_regions = cur->next;
                kfree(cur);
            } else if (padding == 0UL) {
                cur->start = tail_start;
                cur->size = tail_size;
            } else {
                cur->size = padding;
                if (tail_size != 0UL) {
                    int ret;

                    ret = rknpu_iova_insert_locked(
                        domain, tail_start, tail_size);
                    if (ret != 0)
                        return ret;
                }
            }

            *ret_iova = start;
            return 0;
        }

        prev = cur;
        cur = cur->next;
    }

    return -ENOMEM;
}

static void rknpu_iommu_clear_all_ptes(struct rknpu_iommu_domain *domain)
{
    unsigned int i;

    for (i = 0; i < RKNPU_IOMMU_DTE_ENTRIES; i++)
        memset(domain->pts[i].vaddr, 0, RKNPU_IOMMU_PAGE_SIZE);

    rknpu_iommu_clean_all_pts(domain);
}

static int rknpu_iommu_reset_iova_locked(struct rknpu_iommu_domain *domain)
{
    struct rknpu_iova_region *region;

    rknpu_iova_free_all(domain);

    region = kmalloc(sizeof(*region));
    if (region == NULL)
        return -ENOMEM;

    region->start = RKNPU_IOMMU_IOVA_BASE;
    region->size = RKNPU_IOMMU_IOVA_LIMIT - RKNPU_IOMMU_IOVA_BASE;
    region->next = NULL;
    domain->free_regions = region;

    return 0;
}

static int rknpu_iommu_build_tables_locked(struct rknpu_iommu_domain *domain)
{
    unsigned long paddr = RKNPU_IOMMU_LOW_PT_BASE;
    unsigned int i;

    if (domain->tables_ready)
        return 0;

    if (RKNPU_IOMMU_LOW_PT_PAGES < RKNPU_IOMMU_REQUIRED_PT_PAGES)
        return -ENOMEM;

    domain->dt_paddr = paddr;
    domain->dt = (u32 *)phys_to_virt(paddr);
    memset(domain->dt, 0, RKNPU_IOMMU_PAGE_SIZE);
    paddr += RKNPU_IOMMU_PAGE_SIZE;

    for (i = 0; i < RKNPU_IOMMU_DTE_ENTRIES; i++) {
        domain->pts[i].paddr = paddr;
        domain->pts[i].vaddr = (u32 *)phys_to_virt(paddr);
        memset(domain->pts[i].vaddr, 0, RKNPU_IOMMU_PAGE_SIZE);
        domain->dt[i] = rknpu_iommu_mk_dte(paddr);
        paddr += RKNPU_IOMMU_PAGE_SIZE;
    }

    rknpu_iommu_clean_dt(domain);
    rknpu_iommu_clean_all_pts(domain);
    rknpu_iommu_barrier();
    domain->tables_ready = true;
    return 0;
}

static int rknpu_iommu_map_locked(struct rknpu_iommu_domain *domain,
                                  unsigned long iova,
                                  unsigned long paddr,
                                  unsigned long size,
                                  unsigned int prot)
{
    unsigned long offset = 0UL;

    while (offset < size) {
        unsigned long cur_iova = iova + offset;
        unsigned int dte_index = rknpu_iommu_dte_index(cur_iova);
        unsigned int pte_index = rknpu_iommu_pte_index(cur_iova);
        unsigned int pte_avail = RKNPU_IOMMU_PTE_ENTRIES - pte_index;
        unsigned long remaining = size - offset;
        unsigned long nr_pages = remaining / RKNPU_IOMMU_PAGE_SIZE;
        struct rknpu_iommu_pt *pt;
        unsigned int i;

        if (dte_index >= RKNPU_IOMMU_DTE_ENTRIES)
            return -EINVAL;
        if (nr_pages > pte_avail)
            nr_pages = pte_avail;

        pt = &domain->pts[dte_index];
        for (i = 0; i < nr_pages; i++) {
            u32 *pte = &pt->vaddr[pte_index + i];

            if ((*pte & RKNPU_IOMMU_PTE_VALID) != 0U)
                return -EEXIST;
            *pte = rknpu_iommu_mk_pte(
                paddr + offset + i * RKNPU_IOMMU_PAGE_SIZE, prot);
        }

        rknpu_iommu_clean_pt_range(pt, pte_index, nr_pages);
        offset += nr_pages * RKNPU_IOMMU_PAGE_SIZE;
    }

    return 0;
}

static int rknpu_iommu_unmap_locked(struct rknpu_iommu_domain *domain,
                                    unsigned long iova,
                                    unsigned long size)
{
    unsigned long offset = 0UL;

    while (offset < size) {
        unsigned long cur_iova = iova + offset;
        unsigned int dte_index = rknpu_iommu_dte_index(cur_iova);
        unsigned int pte_index = rknpu_iommu_pte_index(cur_iova);
        unsigned int pte_avail = RKNPU_IOMMU_PTE_ENTRIES - pte_index;
        unsigned long remaining = size - offset;
        unsigned long nr_pages = remaining / RKNPU_IOMMU_PAGE_SIZE;
        struct rknpu_iommu_pt *pt;
        unsigned int i;

        if (dte_index >= RKNPU_IOMMU_DTE_ENTRIES)
            return -EINVAL;
        if (nr_pages > pte_avail)
            nr_pages = pte_avail;

        pt = &domain->pts[dte_index];
        for (i = 0; i < nr_pages; i++)
            pt->vaddr[pte_index + i] = 0U;

        rknpu_iommu_clean_pt_range(pt, pte_index, nr_pages);
        offset += nr_pages * RKNPU_IOMMU_PAGE_SIZE;
    }

    return 0;
}

static void rknpu_iommu_dump_iova_locked(struct rknpu_iommu *iommu,
                                         unsigned int mmu_index,
                                         unsigned long iova,
                                         const char *tag)
{
    struct rknpu_iommu_domain *domain = &iommu->domain;
    unsigned int dte_index = rknpu_iommu_dte_index(iova);
    unsigned int pte_index = rknpu_iommu_pte_index(iova);
    unsigned long page_offset = iova & RKNPU_IOMMU_PAGE_MASK;
    unsigned int hw_dte;
    u32 dte = 0U;
    u32 pte = 0U;
    unsigned long pt_paddr = 0UL;
    unsigned long page_paddr = 0UL;

    if (mmu_index >= RKNPU_IOMMU_MMIO_COUNT)
        mmu_index = 0U;

    hw_dte = rknpu_iommu_read(iommu->base[mmu_index],
                              RKNPU_IOMMU_REG_DTE_ADDR);

    if (dte_index < RKNPU_IOMMU_DTE_ENTRIES) {
        struct rknpu_iommu_pt *pt = &domain->pts[dte_index];

        dte = domain->dt[dte_index];
        pt_paddr = rknpu_iommu_decode_desc_addr(dte);
        pte = pt->vaddr[pte_index];
        page_paddr = rknpu_iommu_decode_desc_addr(pte) + page_offset;
    }

    kinfo("rknpu iommu %s mmu=%u iova=0x%lx dte_idx=%u pte_idx=%u off=0x%lx dt_pa=0x%lx hw_dte=0x%x hw_dt_pa=0x%lx dte=0x%x pt_pa=0x%lx pte=0x%x pa=0x%lx\n",
          tag,
          mmu_index,
          iova,
          dte_index,
          pte_index,
          page_offset,
          domain->dt_paddr,
          hw_dte,
          rknpu_iommu_decode_dt_addr(hw_dte),
          dte,
          pt_paddr,
          pte,
          page_paddr);
}

static int rknpu_iommu_prepare_locked(struct rknpu_iommu *iommu)
{
    unsigned int i;

    if (!iommu->lock_ready) {
        lock_init(&iommu->lock);
        iommu->lock_ready = true;
    }

    for (i = 0; i < RKNPU_IOMMU_MMIO_COUNT; i++)
        iommu->base[i] = phys_to_virt(rknpu_iommu_base_paddr[i]);

    return rknpu_iommu_build_tables_locked(&iommu->domain);
}

int sys_tee_npu_iommu_init(void)
{
    struct rknpu_iommu *iommu = &rknpu_iommu_dev;
    u32 dt_addr;
    unsigned int i;
    int ret;

    if (!iommu->lock_ready) {
        lock_init(&iommu->lock);
        iommu->lock_ready = true;
    }

    lock(&iommu->lock);

    ret = rknpu_iommu_prepare_locked(iommu);
    if (ret != 0)
        goto out_unlock;

    ret = rknpu_iommu_stop_locked(iommu);
    if (ret != 0)
        goto out_unlock;

    ret = rknpu_iommu_clear_faults_locked(iommu);
    if (ret != 0)
        goto out_unlock;

    rknpu_iommu_clear_all_ptes(&iommu->domain);
    ret = rknpu_iommu_reset_iova_locked(&iommu->domain);
    if (ret != 0)
        goto out_unlock;

    dt_addr = rknpu_iommu_encode_dt_addr(iommu->domain.dt_paddr);
    for (i = 0; i < RKNPU_IOMMU_MMIO_COUNT; i++) {
        rknpu_iommu_write(iommu->base[i], RKNPU_IOMMU_REG_INT_MASK, 0U);
        rknpu_iommu_write(iommu->base[i],
                          RKNPU_IOMMU_REG_DTE_ADDR,
                          dt_addr);
    }
    rknpu_iommu_barrier();

    ret = rknpu_iommu_start_locked(iommu);
    if (ret == 0) {
        iommu->hw_ready = true;
        ret = rknpu_iommu_zap(iommu);
    }

out_unlock:
    unlock(&iommu->lock);
    return ret;
}

int sys_tee_npu_iommu_map(unsigned long paddr,
                          unsigned long size,
                          unsigned int prot,
                          unsigned long iova_uaddr)
{
    struct rknpu_iommu *iommu = &rknpu_iommu_dev;
    unsigned long page_offset;
    unsigned long map_paddr;
    unsigned long map_size;
    unsigned long iova;
    int ret;

    if (size == 0UL || (prot & RKNPU_IOMMU_PROT_RW) == 0U)
        return -EINVAL;
    if (check_user_addr_range(iova_uaddr, sizeof(iova)) != 0)
        return -EINVAL;
    if (rknpu_iommu_range_overflows(paddr, size)
        || paddr + size > RKNPU_IOMMU_PHYS_ADDR_LIMIT)
        return -ERANGE;

    page_offset = paddr & RKNPU_IOMMU_PAGE_MASK;
    map_paddr = rknpu_iommu_align_down(paddr);
    map_size = rknpu_iommu_align_up(size + page_offset);
    if (map_size == 0UL || map_size > RKNPU_IOMMU_IOVA_LIMIT)
        return -ERANGE;

    lock(&iommu->lock);
    if (!iommu->hw_ready) {
        ret = -ENODEV;
        goto out_unlock;
    }

    ret = rknpu_iova_alloc_locked(&iommu->domain, map_size, &iova);
    if (ret != 0)
        goto out_unlock;

    ret = rknpu_iommu_map_locked(
        &iommu->domain, iova, map_paddr, map_size, prot);
    if (ret != 0) {
        (void)rknpu_iommu_unmap_locked(&iommu->domain, iova, map_size);
        (void)rknpu_iova_insert_locked(&iommu->domain, iova, map_size);
        goto out_unlock;
    }

    ret = rknpu_iommu_zap(iommu);
    if (ret == 0) {
        iova += page_offset;
        if (copy_to_user((void *)iova_uaddr, &iova, sizeof(iova)) != 0)
            ret = -EINVAL;
    }

out_unlock:
    unlock(&iommu->lock);
    return ret;
}

int sys_tee_npu_iommu_unmap(unsigned long iova, unsigned long size)
{
    struct rknpu_iommu *iommu = &rknpu_iommu_dev;
    unsigned long page_offset;
    unsigned long map_iova;
    unsigned long map_size;
    int ret;

    if (size == 0UL)
        return -EINVAL;
    if (rknpu_iommu_range_overflows(iova, size))
        return -ERANGE;

    page_offset = iova & RKNPU_IOMMU_PAGE_MASK;
    map_iova = rknpu_iommu_align_down(iova);
    map_size = rknpu_iommu_align_up(size + page_offset);

    lock(&iommu->lock);
    if (!iommu->hw_ready) {
        ret = -ENODEV;
        goto out_unlock;
    }

    ret = rknpu_iommu_unmap_locked(&iommu->domain, map_iova, map_size);
    if (ret == 0)
        ret = rknpu_iova_insert_locked(&iommu->domain, map_iova, map_size);
    if (ret == 0)
        ret = rknpu_iommu_zap(iommu);

out_unlock:
    unlock(&iommu->lock);
    return ret;
}

void sys_tee_npu_iommu_dump(unsigned long data_iova,
                            unsigned long dma_base_iova)
{
    struct rknpu_iommu *iommu = &rknpu_iommu_dev;
    unsigned int i;

    if (!iommu->lock_ready || !iommu->hw_ready)
        return;

    lock(&iommu->lock);

    for (i = 0; i < RKNPU_IOMMU_MMIO_COUNT; i++) {
        unsigned int status =
            rknpu_iommu_read(iommu->base[i], RKNPU_IOMMU_REG_STATUS);
        unsigned int raw =
            rknpu_iommu_read(iommu->base[i], RKNPU_IOMMU_REG_INT_RAWSTAT);
        unsigned int masked =
            rknpu_iommu_read(iommu->base[i], RKNPU_IOMMU_REG_INT_STATUS);
        unsigned int fault =
            rknpu_iommu_read(iommu->base[i],
                             RKNPU_IOMMU_REG_PAGE_FAULT_ADDR);
        unsigned int dte =
            rknpu_iommu_read(iommu->base[i], RKNPU_IOMMU_REG_DTE_ADDR);

        kinfo("rknpu iommu fault mmu=%u status=0x%x raw=0x%x masked=0x%x fault_iova=0x%x dte=0x%x dt_pa=0x%lx\n",
              i,
              status,
              raw,
              masked,
              fault,
              dte,
              rknpu_iommu_decode_dt_addr(dte));

        if (fault != 0U)
            rknpu_iommu_dump_iova_locked(iommu, i, fault, "fault");
    }

    rknpu_iommu_dump_iova_locked(iommu, 0U, data_iova, "pc-data");
    if (dma_base_iova != 0UL)
        rknpu_iommu_dump_iova_locked(iommu,
                                     0U,
                                     dma_base_iova,
                                     "pc-dma-base");

    unlock(&iommu->lock);
}
