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
#include <common/lock.h>
#include <common/types.h>
#include <irq/irq.h>
#include <mm/uaccess.h>
#include <rknpu.h>

#define RKNPU_OFFSET_PC_OP_EN        0x08U
#define RKNPU_OFFSET_PC_DATA_ADDR    0x10U
#define RKNPU_OFFSET_PC_DATA_AMOUNT  0x14U
#define RKNPU_OFFSET_INT_MASK        0x20U
#define RKNPU_OFFSET_INT_CLEAR       0x24U
#define RKNPU_OFFSET_INT_STATUS      0x28U
#define RKNPU_OFFSET_INT_RAW_STATUS  0x2cU
#define RKNPU_OFFSET_PC_TASK_CONTROL 0x30U
#define RKNPU_OFFSET_PC_DMA_BASE     0x34U
#define RKNPU_OFFSET_PC_TASK_STATUS  0x3cU

#define RKNPU_CNA_S_POINTER  0x1004U
#define RKNPU_CORE_S_POINTER 0x3004U

#define RKNPU_INT_CLEAR_ALL         0x1ffffU
#define RKNPU_INT_CLEAR_POLL_LIMIT  10000U
#define RKNPU_PC_IDLE_POLL_LIMIT    100000U
#define RKNPU_PC_DATA_EXTRA_AMOUNT  4U
#define RKNPU_PC_OP_ENABLE          1U
#define RKNPU_PC_OP_DISABLE         0U
#define RKNPU_PC_DATA_ADDR_RESET    1U
#define RKNPU_PC_TASK_OPCODE        0x6U

#define RKNPU_CORE0_MASK 0x01U
#define RKNPU_CORE1_MASK 0x02U
#define RKNPU_CORE2_MASK 0x04U

#define RK3588_RKNPU_PC_DATA_AMOUNT_SCALE 2U
#define RK3588_RKNPU_PC_TASK_NUMBER_BITS  12U
#define RK3588_RKNPU_PC_TASK_NUMBER_MASK \
    ((1U << RK3588_RKNPU_PC_TASK_NUMBER_BITS) - 1U)
#define RK3588_RKNPU_MAX_SUBMIT_NUMBER RK3588_RKNPU_PC_TASK_NUMBER_MASK

#define RKNPU_INT_GROUP_WIDTH 2U
#define RKNPU_INT_GROUP_COUNT 6U
#define RKNPU_INT_GROUP_MASK  ((1U << RKNPU_INT_GROUP_WIDTH) - 1U)

#define RKNPU_CORE_SELECT_VALUE_BASE   0xeU
#define RKNPU_CORE_SELECT_VALUE_STRIDE 0x10000000U
#define RKNPU_CORE_SELECT_VALUE(core_index) \
    (RKNPU_CORE_SELECT_VALUE_BASE + \
     RKNPU_CORE_SELECT_VALUE_STRIDE * (unsigned int)(core_index))
#define RKNPU_COMPLETED_MASK(task_num) ((1U << (unsigned int)(task_num)) - 1U)

struct rknpu_config {
    unsigned int pc_data_amount_scale;
    unsigned int pc_task_number_bits;
    unsigned int pc_task_number_mask;
    unsigned int pc_task_status_offset;
    unsigned long max_submit_number;
};

struct rknpu_inflight {
    bool active;
    int task_num;
    int core_index[RKNPU_CORE_NR];
    unsigned int expected_int_mask[RKNPU_CORE_NR];
    unsigned int timeout_ms;
    unsigned int completed_mask;
    u64 start_cntvct;
};

struct rknpu_device {
    vaddr_t base[RKNPU_CORE_NR];
    unsigned int irq[RKNPU_CORE_NR];
    const struct rknpu_config *config;
    struct lock submit_lock;
    struct lock core_locks[RKNPU_CORE_NR];
    struct rknpu_inflight inflight;
    bool ready;
};

static const struct rknpu_config rk3588_rknpu_config = {
    .pc_data_amount_scale = RK3588_RKNPU_PC_DATA_AMOUNT_SCALE,
    .pc_task_number_bits = RK3588_RKNPU_PC_TASK_NUMBER_BITS,
    .pc_task_number_mask = RK3588_RKNPU_PC_TASK_NUMBER_MASK,
    .pc_task_status_offset = RKNPU_OFFSET_PC_TASK_STATUS,
    .max_submit_number = RK3588_RKNPU_MAX_SUBMIT_NUMBER,
};

static const unsigned long rknpu_core_base_paddr[RKNPU_CORE_NR] = {
    RKNPU_CORE0_BASE_PADDR,
    RKNPU_CORE1_BASE_PADDR,
    RKNPU_CORE2_BASE_PADDR,
};

static const unsigned int rknpu_core_irq[RKNPU_CORE_NR] = {
    RKNPU_CORE0_IRQ,
    RKNPU_CORE1_IRQ,
    RKNPU_CORE2_IRQ,
};

static struct rknpu_device rknpu_dev;

static inline u32 rknpu_read(vaddr_t base, unsigned int offset)
{
    return get32(base + offset);
}

static inline void rknpu_write(vaddr_t base, unsigned int offset, u32 value)
{
    put32(base + offset, value);
}

static inline void rknpu_mmio_barrier(void)
{
    dsb(sy);
}

static inline void rknpu_cpu_relax(void)
{
    asm volatile("yield" ::: "memory");
}

static inline u64 rknpu_read_cntvct(void)
{
    u64 value;

    asm volatile("mrs %0, cntvct_el0" : "=r"(value));
    return value;
}

static inline u64 rknpu_read_cntfrq(void)
{
    u64 value;

    asm volatile("mrs %0, cntfrq_el0" : "=r"(value));
    return value;
}

static u64 rknpu_cntvct_elapsed_ms(u64 start)
{
    u64 freq = rknpu_read_cntfrq();

    if (freq == 0U)
        return 0U;

    return ((rknpu_read_cntvct() - start) * 1000ULL) / freq;
}

static unsigned int rknpu_fuzz_status(unsigned int status)
{
    unsigned int fuzz_status = 0U;
    unsigned int group;

    for (group = 0; group < RKNPU_INT_GROUP_COUNT; group++) {
        unsigned int mask =
            RKNPU_INT_GROUP_MASK << (group * RKNPU_INT_GROUP_WIDTH);

        if ((status & mask) != 0U)
            fuzz_status |= mask;
    }

    return fuzz_status;
}

static int rknpu_core_index_from_mask(unsigned int core_mask)
{
    switch (core_mask) {
    case RKNPU_CORE0_MASK:
        return 0;
    case RKNPU_CORE1_MASK:
        return 1;
    case RKNPU_CORE2_MASK:
        return 2;
    default:
        return -EINVAL;
    }
}

static int rknpu_prepare_device(struct rknpu_device *dev)
{
    unsigned int i;

    if (dev->ready)
        return 0;

    if (lock_init(&dev->submit_lock) != 0)
        return -EINVAL;

    for (i = 0; i < RKNPU_CORE_NR; i++) {
        dev->base[i] = phys_to_virt(rknpu_core_base_paddr[i]);
        dev->irq[i] = rknpu_core_irq[i];
        if (lock_init(&dev->core_locks[i]) != 0)
            return -EINVAL;
    }

    dev->config = &rk3588_rknpu_config;
    dev->ready = true;
    return 0;
}

static bool rknpu_uaddr_offset(u64 base,
                               unsigned long index,
                               unsigned long entry_size,
                               unsigned long *uaddr)
{
    unsigned long addr = (unsigned long)base;
    unsigned long offset;

    if (index > (~0UL / entry_size))
        return true;

    offset = index * entry_size;
    if (addr > (~0UL - offset))
        return true;

    *uaddr = addr + offset;
    return false;
}

static int rknpu_copy_user_task(u64 task_obj_addr,
                                unsigned int task_index,
                                struct rknpu_task *task)
{
    unsigned long task_uaddr;

    if (task_obj_addr == 0U)
        return -EINVAL;

    if (rknpu_uaddr_offset(task_obj_addr,
                           task_index,
                           sizeof(*task),
                           &task_uaddr))
        return -ERANGE;

    if (check_user_addr_range(task_uaddr, sizeof(*task)) != 0)
        return -EFAULT;

    if (copy_from_user(task, (void *)task_uaddr, sizeof(*task)) != 0)
        return -EFAULT;

    return 0;
}

static int rknpu_load_submit_tasks(const struct rknpu_device *dev,
                                   const struct rknpu_submit *submit,
                                   int core_index,
                                   struct rknpu_task *first_task,
                                   struct rknpu_task *last_task)
{
    unsigned int task_start;
    unsigned int task_number;
    unsigned int task_end;
    int ret;

    if (core_index < 0 || core_index >= RKNPU_CORE_NR)
        return -EINVAL;

    task_start = submit->subcore_task[core_index].task_start;
    task_number = submit->subcore_task[core_index].task_number;
    if (task_number == 0U)
        return -EINVAL;
    if (task_number > dev->config->max_submit_number)
        return -EINVAL;
    if (task_start > dev->config->max_submit_number - task_number)
        return -EINVAL;

    task_end = task_start + task_number - 1U;

    ret = rknpu_copy_user_task(submit->task_obj_addr, task_start, first_task);
    if (ret != 0)
        return ret;

    if (task_end == task_start) {
        *last_task = *first_task;
        return 0;
    }

    return rknpu_copy_user_task(submit->task_obj_addr, task_end, last_task);
}

static int rknpu_wait_int_cleared(vaddr_t core_base,
                                  int core_index,
                                  unsigned int expected_int_mask,
                                  const char *phase)
{
    unsigned int i;

    if (expected_int_mask == 0U)
        return 0;

    for (i = 0; i < RKNPU_INT_CLEAR_POLL_LIMIT; i++) {
        unsigned int status = rknpu_read(core_base, RKNPU_OFFSET_INT_STATUS);

        if ((rknpu_fuzz_status(status) & expected_int_mask) == 0U)
            return 0;

        rknpu_cpu_relax();
    }

    kwarn("npu int clear timeout phase=%s core=%d expect=0x%x\n",
          phase,
          core_index,
          expected_int_mask);
    return -ETIME;
}

static int rknpu_wait_pc_idle(const struct rknpu_device *dev,
                              vaddr_t core_base,
                              int core_index,
                              const char *phase)
{
    unsigned int mask = dev->config->pc_task_number_mask;
    unsigned int i;

    for (i = 0; i < RKNPU_PC_IDLE_POLL_LIMIT; i++) {
        unsigned int pc =
            rknpu_read(core_base, dev->config->pc_task_status_offset);

        if ((pc & mask) == 0U)
            return 0;

        rknpu_cpu_relax();
    }

    kwarn("npu pc idle timeout phase=%s core=%d mask=0x%x\n",
          phase,
          core_index,
          mask);
    return -ETIME;
}

static int rknpu_quiesce_before_submit(const struct rknpu_device *dev,
                                       vaddr_t core_base,
                                       int core_index,
                                       unsigned int expected_int_mask)
{
    int ret;

    rknpu_write(core_base, RKNPU_OFFSET_INT_CLEAR, RKNPU_INT_CLEAR_ALL);
    rknpu_mmio_barrier();

    ret = rknpu_wait_int_cleared(
        core_base, core_index, expected_int_mask, "pre-submit-clear");
    if (ret != 0)
        return ret;

    return rknpu_wait_pc_idle(dev, core_base, core_index, "pre-submit-idle");
}

static void rknpu_dump_timeout(struct rknpu_device *dev,
                               int core_index,
                               unsigned int expected_int_mask)
{
    vaddr_t core_base = dev->base[core_index];
    unsigned int status = rknpu_read(core_base, RKNPU_OFFSET_INT_STATUS);
    unsigned int raw = rknpu_read(core_base, RKNPU_OFFSET_INT_RAW_STATUS);
    unsigned int pc = rknpu_read(core_base, RKNPU_OFFSET_PC_TASK_STATUS);
    unsigned int data_addr = rknpu_read(core_base, RKNPU_OFFSET_PC_DATA_ADDR);
    unsigned int dma_base = rknpu_read(core_base, RKNPU_OFFSET_PC_DMA_BASE);

    kwarn("npu poll timeout core=%d expect=0x%x status=0x%x raw=0x%x pc=0x%x\n",
          core_index,
          expected_int_mask,
          status,
          raw,
          pc);
    sys_tee_npu_iommu_dump(data_addr, dma_base);
}

static int rknpu_poll_core_complete(struct rknpu_device *dev,
                                    int core_index,
                                    unsigned int expected_int_mask,
                                    bool *done)
{
    vaddr_t core_base = dev->base[core_index];
    unsigned int status = rknpu_read(core_base, RKNPU_OFFSET_INT_STATUS);

    *done = false;

    if (expected_int_mask == 0U
        || rknpu_fuzz_status(status) != expected_int_mask)
        return 0;

    rknpu_write(core_base, RKNPU_OFFSET_INT_CLEAR, RKNPU_INT_CLEAR_ALL);
    rknpu_mmio_barrier();

    *done = true;
    return rknpu_wait_int_cleared(
        core_base, core_index, expected_int_mask, "poll-clear");
}

static int rknpu_submit_core(struct rknpu_device *dev,
                             const struct rknpu_submit *submit,
                             unsigned int *expected_int_mask)
{
    struct rknpu_task first_task;
    struct rknpu_task last_task;
    vaddr_t core_base;
    unsigned int task_number;
    unsigned int task_pp_en;
    int core_index;
    int ret;

    core_index = rknpu_core_index_from_mask(submit->core_mask);
    if (core_index < 0)
        return core_index;

    ret = rknpu_load_submit_tasks(
        dev, submit, core_index, &first_task, &last_task);
    if (ret != 0)
        return ret;

    task_number = submit->subcore_task[core_index].task_number;
    task_pp_en = (submit->flags & RKNPU_JOB_PINGPONG) != 0U ? 1U : 0U;
    core_base = dev->base[core_index];
    if (expected_int_mask != NULL)
        *expected_int_mask = last_task.int_mask;

    lock(&dev->core_locks[core_index]);

    ret = rknpu_quiesce_before_submit(
        dev, core_base, core_index, last_task.int_mask);
    if (ret != 0)
        goto out_unlock;

    rknpu_write(core_base,
                RKNPU_OFFSET_PC_DATA_ADDR,
                RKNPU_PC_DATA_ADDR_RESET);

    rknpu_write(core_base,
                RKNPU_CNA_S_POINTER,
                RKNPU_CORE_SELECT_VALUE(core_index));
    rknpu_write(core_base,
                RKNPU_CORE_S_POINTER,
                RKNPU_CORE_SELECT_VALUE(core_index));

    rknpu_write(core_base, RKNPU_OFFSET_PC_DATA_ADDR, first_task.regcmd_addr);
    rknpu_write(core_base,
                RKNPU_OFFSET_PC_DATA_AMOUNT,
                (first_task.regcfg_amount + RKNPU_PC_DATA_EXTRA_AMOUNT +
                 dev->config->pc_data_amount_scale - 1U) /
                        dev->config->pc_data_amount_scale -
                    1U);

    rknpu_write(core_base, RKNPU_OFFSET_INT_MASK, last_task.int_mask);
    rknpu_write(core_base, RKNPU_OFFSET_INT_CLEAR, first_task.int_mask);
    rknpu_mmio_barrier();

    ret = rknpu_wait_int_cleared(
        core_base, core_index, first_task.int_mask, "submit-task-clear");
    if (ret != 0)
        goto out_clear_irq;

    rknpu_write(core_base,
                RKNPU_OFFSET_PC_TASK_CONTROL,
                ((RKNPU_PC_TASK_OPCODE | task_pp_en)
                 << dev->config->pc_task_number_bits) |
                    task_number);

    rknpu_write(core_base, RKNPU_OFFSET_PC_DMA_BASE, submit->task_base_addr);

    rknpu_write(core_base, RKNPU_OFFSET_PC_OP_EN, RKNPU_PC_OP_ENABLE);
    rknpu_mmio_barrier();
    (void)rknpu_read(core_base, RKNPU_OFFSET_PC_TASK_STATUS);
    rknpu_mmio_barrier();
    rknpu_write(core_base, RKNPU_OFFSET_PC_OP_EN, RKNPU_PC_OP_DISABLE);
    rknpu_mmio_barrier();
    (void)rknpu_read(core_base, RKNPU_OFFSET_PC_TASK_STATUS);
    rknpu_mmio_barrier();
    rknpu_cpu_relax();

out_clear_irq:
    if (ret != 0) {
        rknpu_write(core_base, RKNPU_OFFSET_INT_CLEAR, RKNPU_INT_CLEAR_ALL);
        rknpu_mmio_barrier();
    }

out_unlock:
    unlock(&dev->core_locks[core_index]);
    return ret;
}

static void rknpu_set_submit_irqs(struct rknpu_device *dev,
                                  const struct rknpu_submit submits[],
                                  int task_num,
                                  bool enable)
{
    int i;

    for (i = 0; i < task_num; i++) {
        int core_index = rknpu_core_index_from_mask(submits[i].core_mask);

        if (core_index < 0)
            continue;

        if (enable)
            plat_enable_irqno(dev->irq[core_index]);
        else
            plat_disable_irqno(dev->irq[core_index]);
    }
}

static void rknpu_finish_inflight_locked(struct rknpu_device *dev,
                                         bool clear_irqs)
{
    struct rknpu_inflight *inflight = &dev->inflight;
    int i;

    for (i = 0; i < inflight->task_num; i++) {
        int core_index = inflight->core_index[i];

        if (core_index < 0 || core_index >= RKNPU_CORE_NR)
            continue;

        if (clear_irqs) {
            rknpu_write(dev->base[core_index],
                        RKNPU_OFFSET_INT_CLEAR,
                        RKNPU_INT_CLEAR_ALL);
            rknpu_mmio_barrier();
        }

        plat_enable_irqno(dev->irq[core_index]);
    }

    inflight->active = false;
    inflight->task_num = 0;
    inflight->completed_mask = 0U;
}

static unsigned int rknpu_submit_timeout(const struct rknpu_submit submits[],
                                         int task_num)
{
    unsigned int timeout = RKNPU_DEFAULT_TIMEOUT_MS;
    int i;

    for (i = 0; i < task_num; i++) {
        unsigned int submit_timeout = submits[i].timeout != 0U ?
                                      submits[i].timeout :
                                      RKNPU_DEFAULT_TIMEOUT_MS;

        if (submit_timeout > timeout)
            timeout = submit_timeout;
    }

    return timeout;
}

static int rknpu_submit_start_locked(struct rknpu_device *dev,
                                     const struct rknpu_submit submits[],
                                     int task_num)
{
    struct rknpu_inflight *inflight = &dev->inflight;
    int i;
    int ret;

    if (task_num <= 0 || task_num > RKNPU_CORE_NR)
        return -EINVAL;
    if (inflight->active)
        return -EBUSY;

    for (i = 0; i < task_num; i++) {
        ret = rknpu_core_index_from_mask(submits[i].core_mask);
        if (ret < 0)
            return ret;

        inflight->core_index[i] = ret;
        inflight->expected_int_mask[i] = 0U;
    }

    inflight->active = true;
    inflight->task_num = task_num;
    inflight->timeout_ms = rknpu_submit_timeout(submits, task_num);
    inflight->completed_mask = 0U;
    inflight->start_cntvct = rknpu_read_cntvct();

    rknpu_set_submit_irqs(dev, submits, task_num, false);

    for (i = 0; i < task_num; i++) {
        ret = rknpu_submit_core(
            dev, &submits[i], &inflight->expected_int_mask[i]);
        if (ret != 0) {
            rknpu_finish_inflight_locked(dev, true);
            return ret;
        }
    }

    return 0;
}

static int rknpu_poll_complete_locked(struct rknpu_device *dev)
{
    struct rknpu_inflight *inflight = &dev->inflight;
    int i;
    int ret;

    if (!inflight->active)
        return -EINVAL;

    for (i = 0; i < inflight->task_num; i++) {
        bool done;

        if ((inflight->completed_mask & (1U << (unsigned int)i)) != 0U)
            continue;

        ret = rknpu_poll_core_complete(dev,
                                       inflight->core_index[i],
                                       inflight->expected_int_mask[i],
                                       &done);
        if (ret != 0)
            return ret;
        if (done)
            inflight->completed_mask |= 1U << (unsigned int)i;
    }

    if (inflight->completed_mask == RKNPU_COMPLETED_MASK(inflight->task_num)) {
        rknpu_finish_inflight_locked(dev, false);
        return 0;
    }

    if (rknpu_cntvct_elapsed_ms(inflight->start_cntvct)
        > inflight->timeout_ms) {
        for (i = 0; i < inflight->task_num; i++) {
            if ((inflight->completed_mask & (1U << (unsigned int)i)) == 0U) {
                rknpu_dump_timeout(dev,
                                   inflight->core_index[i],
                                   inflight->expected_int_mask[i]);
            }
        }
        rknpu_finish_inflight_locked(dev, true);
        return -ETIME;
    }

    return -EAGAIN;
}

int sys_tee_npu_submit_start(unsigned long submits_uaddr, int task_num)
{
    struct rknpu_submit submits[RKNPU_CORE_NR];
    unsigned long copy_size;
    int ret;

    if (task_num <= 0 || task_num > RKNPU_CORE_NR)
        return -EINVAL;

    copy_size = sizeof(submits[0]) * (unsigned long)task_num;
    if (check_user_addr_range(submits_uaddr, copy_size) != 0)
        return -EFAULT;
    if (copy_from_user(submits, (void *)submits_uaddr, copy_size) != 0)
        return -EFAULT;

    ret = rknpu_prepare_device(&rknpu_dev);
    if (ret != 0)
        return ret;

    lock(&rknpu_dev.submit_lock);
    ret = rknpu_submit_start_locked(&rknpu_dev, submits, task_num);
    unlock(&rknpu_dev.submit_lock);

    return ret;
}

int sys_tee_npu_poll_complete(void)
{
    int ret;

    ret = rknpu_prepare_device(&rknpu_dev);
    if (ret != 0)
        return ret;

    lock(&rknpu_dev.submit_lock);
    ret = rknpu_poll_complete_locked(&rknpu_dev);
    unlock(&rknpu_dev.submit_lock);

    return ret;
}

int sys_tee_npu_submit_cancel(void)
{
    int ret;

    ret = rknpu_prepare_device(&rknpu_dev);
    if (ret != 0)
        return ret;

    lock(&rknpu_dev.submit_lock);
    if (rknpu_dev.inflight.active)
        rknpu_finish_inflight_locked(&rknpu_dev, true);
    unlock(&rknpu_dev.submit_lock);

    return 0;
}
