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
#include <arch/sync.h>
#include <arch/tools.h>
#include <common/errno.h>
#include <common/kprint.h>
#include <common/macro.h>
#include <machine.h>
#include <rknpu.h>

#define NPU_POWER_POLL_LIMIT 10000U

struct npu_clock_gate {
    const char *name;
    unsigned int offset;
    unsigned int bit;
};

struct npu_power_domain {
    const char *name;
    u32 pwr_mask;
    u32 status_mask;
    u32 req_mask;
    u32 idle_mask;
    u32 ack_mask;
    u32 mem_status_mask;
    u32 repair_status_mask;
};

static const struct npu_clock_gate npu_clock_gates[] = {
    { "hclk_npu_root", CRU_CLKGATE_CON(29), 0 },
    { "clk_npu_dsu0", CRU_CLKGATE_CON(29), 1 },
    { "pclk_npu_root", CRU_CLKGATE_CON(29), 4 },
    { "pclk_npu_grf", CRU_CLKGATE_CON(29), 13 },
    { "hclk_npu_cm0_root", CRU_CLKGATE_CON(30), 1 },
    { "aclk_npu0", CRU_CLKGATE_CON(30), 6 },
    { "hclk_npu0", CRU_CLKGATE_CON(30), 8 },
    { "aclk_npu1", CRU_CLKGATE_CON(27), 0 },
    { "hclk_npu1", CRU_CLKGATE_CON(27), 2 },
    { "aclk_npu2", CRU_CLKGATE_CON(28), 0 },
    { "hclk_npu2", CRU_CLKGATE_CON(28), 2 },
};

static const struct npu_power_domain npu_parent_domain = {
    .name = "npu",
    .pwr_mask = BIT(1),
    .status_mask = BIT(1),
};

static const struct npu_power_domain npu_child_domains[] = {
    {
        .name = "nputop",
        .pwr_mask = BIT(3),
        .req_mask = BIT(1),
        .idle_mask = BIT(1),
        .ack_mask = BIT(1),
        .mem_status_mask = BIT(11),
        .repair_status_mask = BIT(2),
    },
    {
        .name = "npu1",
        .pwr_mask = BIT(4),
        .req_mask = BIT(2),
        .idle_mask = BIT(2),
        .ack_mask = BIT(2),
        .mem_status_mask = BIT(12),
        .repair_status_mask = BIT(3),
    },
    {
        .name = "npu2",
        .pwr_mask = BIT(5),
        .req_mask = BIT(3),
        .idle_mask = BIT(3),
        .ack_mask = BIT(3),
        .mem_status_mask = BIT(13),
        .repair_status_mask = BIT(4),
    },
};

static inline vaddr_t cru_vaddr(void)
{
    return phys_to_virt(CRU_BASE);
}

static inline void cru_write(unsigned int offset, u32 value)
{
    put32(cru_vaddr() + offset, value);
    dsb(sy);
}

static inline vaddr_t pmu_vaddr(void)
{
    return phys_to_virt(PMU2_BASE);
}

static inline u32 pmu_read(unsigned int offset)
{
    return get32(pmu_vaddr() + offset);
}

static inline void pmu_write(unsigned int offset, u32 value)
{
    put32(pmu_vaddr() + offset, value);
    dsb(sy);
}

static inline void npu_cpu_relax(void)
{
    asm volatile("yield" ::: "memory");
}

static void npu_set_clock_gates(bool enable)
{
    unsigned int i;

    for (i = 0; i < sizeof(npu_clock_gates) /
                        sizeof(npu_clock_gates[0]);
         i++) {
        const struct npu_clock_gate *gate =
            &npu_clock_gates[i];
        u32 value = enable ? CRU_GATE_ENABLE(gate->bit) :
                             CRU_GATE_DISABLE(gate->bit);

        cru_write(gate->offset, value);
    }
}

static bool npu_domain_is_idle(
    const struct npu_power_domain *domain)
{
    if (domain->idle_mask == 0)
        return false;

    return (pmu_read(PMU_IDLE_OFFSET) & domain->idle_mask) ==
           domain->idle_mask;
}

static bool npu_domain_is_mem_on(
    const struct npu_power_domain *domain)
{
    if (domain->mem_status_mask == 0)
        return false;

    return (pmu_read(PMU_MEM_STATUS_OFFSET) &
            domain->mem_status_mask) == 0;
}

static bool npu_domain_is_chain_on(
    const struct npu_power_domain *domain)
{
    if (domain->mem_status_mask == 0)
        return true;

    return (pmu_read(PMU_CHAIN_STATUS_OFFSET) &
            domain->mem_status_mask) != 0;
}

static bool npu_domain_is_on(
    const struct npu_power_domain *domain)
{
    if (domain->repair_status_mask != 0)
        return (pmu_read(PMU_REPAIR_STATUS_OFFSET) &
                domain->repair_status_mask) != 0;

    if (domain->status_mask == 0)
        return !npu_domain_is_idle(domain);

    return (pmu_read(PMU_STATUS_OFFSET) &
            domain->status_mask) == 0;
}

static int npu_wait_domain_on(
    const struct npu_power_domain *domain,
    bool on)
{
    unsigned int i;

    for (i = 0; i < NPU_POWER_POLL_LIMIT; i++) {
        if (npu_domain_is_on(domain) == on)
            return 0;
        npu_cpu_relax();
    }

    kwarn("rknpu: timeout waiting power domain %s %s\n",
          domain->name,
          on ? "on" : "off");
    return -ETIMEDOUT;
}

static int npu_wait_mem_on(
    const struct npu_power_domain *domain,
    bool on)
{
    unsigned int i;

    for (i = 0; i < NPU_POWER_POLL_LIMIT; i++) {
        if (npu_domain_is_mem_on(domain) == on)
            return 0;
        npu_cpu_relax();
    }

    kwarn("rknpu: timeout waiting memory for domain %s %s\n",
          domain->name,
          on ? "on" : "off");
    return -ETIMEDOUT;
}

static int npu_wait_chain_on(
    const struct npu_power_domain *domain)
{
    unsigned int i;

    for (i = 0; i < NPU_POWER_POLL_LIMIT; i++) {
        if (npu_domain_is_chain_on(domain))
            return 0;
        npu_cpu_relax();
    }

    kwarn("rknpu: timeout waiting chain for domain %s on\n",
          domain->name);
    return -ETIMEDOUT;
}

static int npu_set_idle_request(
    const struct npu_power_domain *domain,
    bool idle)
{
    u32 req_value;
    u32 target_ack;
    unsigned int i;

    if (domain->req_mask == 0)
        return 0;

    req_value = PMU_WRITE_MASK(domain->req_mask);
    if (idle)
        req_value |= domain->req_mask;

    pmu_write(PMU_REQ_OFFSET, req_value);

    target_ack = idle ? domain->ack_mask : 0;
    for (i = 0; i < NPU_POWER_POLL_LIMIT; i++) {
        u32 ack = pmu_read(PMU_ACK_OFFSET);
        bool is_idle = npu_domain_is_idle(domain);

        if ((ack & domain->ack_mask) == target_ack && is_idle == idle)
            return 0;
        npu_cpu_relax();
    }

    kwarn("rknpu: timeout setting domain %s idle=%d\n",
          domain->name,
          idle);
    return -ETIMEDOUT;
}

static int npu_mem_reset(
    const struct npu_power_domain *domain)
{
    int ret;

    if (domain->mem_status_mask == 0)
        return 0;

    ret = npu_wait_chain_on(domain);
    if (ret != 0)
        return ret;

    pmu_write(PMU_MEM_PWR_OFFSET,
                     domain->pwr_mask |
                         PMU_WRITE_MASK(domain->pwr_mask));
    ret = npu_wait_mem_on(domain, false);
    if (ret != 0)
        return ret;

    pmu_write(PMU_MEM_PWR_OFFSET,
                     PMU_WRITE_MASK(domain->pwr_mask));
    return npu_wait_mem_on(domain, true);
}

static int npu_set_power_domain(
    const struct npu_power_domain *domain,
    bool on)
{
    bool mem_was_on;
    u32 value;
    int ret;

    if (domain->pwr_mask == 0)
        return 0;

    if (npu_domain_is_on(domain) == on)
        return 0;

    mem_was_on = on && npu_domain_is_mem_on(domain);

    value = PMU_WRITE_MASK(domain->pwr_mask);
    if (!on)
        value |= domain->pwr_mask;
    pmu_write(PMU_PWR_OFFSET, value);

    if (mem_was_on) {
        ret = npu_mem_reset(domain);
        if (ret != 0)
            return ret;
    }

    return npu_wait_domain_on(domain, on);
}

static int npu_power_domain_on(
    const struct npu_power_domain *domain)
{
    int ret;

    ret = npu_set_power_domain(domain, true);
    if (ret != 0)
        return ret;

    return npu_set_idle_request(domain, false);
}

static int npu_power_domain_off(
    const struct npu_power_domain *domain)
{
    int ret;

    if (!npu_domain_is_on(domain))
        return 0;

    ret = npu_set_idle_request(domain, true);
    if (ret != 0)
        return ret;

    return npu_set_power_domain(domain, false);
}

static int npu_power_on(void)
{
    unsigned int i;
    int ret;

    npu_set_clock_gates(true);

    ret = npu_power_domain_on(&npu_parent_domain);
    if (ret != 0)
        goto out_disable_clocks;

    for (i = 0; i < sizeof(npu_child_domains) /
                        sizeof(npu_child_domains[0]);
         i++) {
        ret = npu_power_domain_on(&npu_child_domains[i]);
        if (ret != 0)
            goto out_power_off_children;
    }

    return 0;

out_power_off_children:
    while (i > 0) {
        i--;
        (void)npu_power_domain_off(&npu_child_domains[i]);
    }
    (void)npu_power_domain_off(&npu_parent_domain);
out_disable_clocks:
    npu_set_clock_gates(false);
    return ret;
}

static int npu_power_off(void)
{
    unsigned int i;
    int ret;

    for (i = sizeof(npu_child_domains) /
             sizeof(npu_child_domains[0]);
         i > 0; i--) {
        ret = npu_power_domain_off(&npu_child_domains[i - 1]);
        if (ret != 0)
            return ret;
    }

    ret = npu_power_domain_off(&npu_parent_domain);
    if (ret == 0)
        npu_set_clock_gates(false);

    return ret;
}

int sys_tee_npu_ree_power_on(void)
{
    return npu_power_on();
}

int sys_tee_npu_ree_power_off(void)
{
    return npu_power_off();
}

void sys_tee_npu_secure_switch(bool secure)
{
     /* restore npu to non-secure */
    extern void switch_firewall_ddr(int secure, int core_index);
    extern void switch_secure_device(int secure, int core_index);
    /* restore irq to non-secure */
    extern void set_npu_irqs_ns(void);
    extern void set_npu_irqs_s(void);

    switch_firewall_ddr(secure, 0);
    switch_firewall_ddr(secure, 1);
    switch_firewall_ddr(secure, 2);
    switch_secure_device(secure, 0);
    switch_secure_device(secure, 1);
    switch_secure_device(secure, 2);

    if (secure)
        set_npu_irqs_s();
    else
        set_npu_irqs_ns();  
}
