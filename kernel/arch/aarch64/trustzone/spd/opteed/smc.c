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
#include <mm/mm.h>
#include <machine.h>
#include <common/lock.h>
#include <sched/sched.h>
#include <sched/fpu.h>
#include <object/thread.h>
#include <object/object.h>
#include <object/memory.h>
#include <common/errno.h>
#include <mm/uaccess.h>
#include <arch/machine/smp.h>
#include <arch/trustzone/smc.h>
#include <arch/trustzone/tlogger.h>

struct smc_percpu_struct {
    bool pending_req;
    struct thread *waiting_thread;
    struct smc_registers regs_k;
    struct smc_registers *regs_u;
} smc_percpu_structs[PLAT_CPU_NUM];

paddr_t smc_ttbr0_el1;

#define SMC_ASID 1000UL
static void init_smc_page_table(void)
{
    extern ptp_t boot_ttbr0_l0;

    /* Reuse the boot stage low memory page table */
    smc_ttbr0_el1 = (paddr_t)&boot_ttbr0_l0;
    smc_ttbr0_el1 |= SMC_ASID << 48;
}

void smc_init(void)
{
    u32 cpuid;
    struct smc_percpu_struct *percpu;

    for (cpuid = 0; cpuid < PLAT_CPU_NUM; cpuid++) {
        percpu = &smc_percpu_structs[cpuid];
        percpu->pending_req = false;
        percpu->waiting_thread = NULL;
    }

    init_smc_page_table();
}

static bool kernel_shared_var_recved = false;
static kernel_shared_varibles_t kernel_var;

#define SEC_MEM_SHADOW_PENDING_MAX 512
#define SEC_MEM_SHADOW_COOKIE_MAGIC 0x5345434d00000000UL
#define SEC_MEM_SHADOW_COOKIE_MAGIC_MASK 0xffffffff00000000UL
#define SEC_MEM_SHADOW_COOKIE_GEN_SHIFT 16
#define SEC_MEM_SHADOW_COOKIE_GEN_MASK 0x00000000ffff0000UL
#define SEC_MEM_SHADOW_COOKIE_SLOT_MASK 0xffffUL

struct sec_mem_shadow_pending {
    bool used;
    u16 generation;
    unsigned long cookie;
    unsigned long op;
    unsigned long arg;
    struct thread *thread;
    struct cap_group *cap_group;
    bool recycle_retry;
};

static struct lock sec_mem_shadow_pending_lock;
static bool sec_mem_shadow_pending_lock_inited;
static struct sec_mem_shadow_pending
    sec_mem_shadow_pending[SEC_MEM_SHADOW_PENDING_MAX];

static void sec_mem_shadow_pending_lock_init_once(void)
{
    if (!sec_mem_shadow_pending_lock_inited) {
        lock_init(&sec_mem_shadow_pending_lock);
        sec_mem_shadow_pending_lock_inited = true;
    }
}

#ifdef CHCORE_ENABLE_TZASC_CMA
static unsigned long sec_mem_shadow_make_cookie(u16 slot, u16 generation)
{
    return SEC_MEM_SHADOW_COOKIE_MAGIC
           | ((unsigned long)generation << SEC_MEM_SHADOW_COOKIE_GEN_SHIFT)
           | slot;
}

static int sec_mem_shadow_pending_alloc(unsigned long op, unsigned long arg,
                                        struct cap_group *owner,
                                        bool recycle_retry,
                                        unsigned long *cookie)
{
    u16 slot;
    struct sec_mem_shadow_pending *pending;

    if (owner == NULL || cookie == NULL)
        return -EINVAL;

    sec_mem_shadow_pending_lock_init_once();
    lock(&sec_mem_shadow_pending_lock);

    for (slot = 0; slot < SEC_MEM_SHADOW_PENDING_MAX; slot++) {
        pending = &sec_mem_shadow_pending[slot];
        if (pending->used)
            continue;

        pending->generation++;
        if (pending->generation == 0)
            pending->generation++;

        pending->cookie =
            sec_mem_shadow_make_cookie(slot, pending->generation);
        pending->op = op;
        pending->arg = arg;
        pending->thread = current_thread;
        pending->cap_group = owner;
        pending->recycle_retry = recycle_retry;
        pending->used = true;
        *cookie = pending->cookie;
        unlock(&sec_mem_shadow_pending_lock);
        return 0;
    }

    unlock(&sec_mem_shadow_pending_lock);
    return -ENOMEM;
}
#endif

static bool sec_mem_shadow_cookie_valid(unsigned long cookie, u16 *slot_out)
{
    unsigned long slot;

    if ((cookie & SEC_MEM_SHADOW_COOKIE_MAGIC_MASK)
        != SEC_MEM_SHADOW_COOKIE_MAGIC)
        return false;

    if ((cookie & SEC_MEM_SHADOW_COOKIE_GEN_MASK) == 0)
        return false;

    slot = cookie & SEC_MEM_SHADOW_COOKIE_SLOT_MASK;
    if (slot >= SEC_MEM_SHADOW_PENDING_MAX)
        return false;

    *slot_out = (u16)slot;
    return true;
}

static bool sec_mem_shadow_pending_take(
    unsigned long cookie, struct sec_mem_shadow_pending *taken)
{
    u16 slot;
    struct sec_mem_shadow_pending *pending;

    if (taken == NULL || !sec_mem_shadow_cookie_valid(cookie, &slot))
        return false;

    sec_mem_shadow_pending_lock_init_once();
    lock(&sec_mem_shadow_pending_lock);

    pending = &sec_mem_shadow_pending[slot];
    if (!pending->used || pending->cookie != cookie) {
        unlock(&sec_mem_shadow_pending_lock);
        return false;
    }

    *taken = *pending;
    pending->used = false;
    pending->cookie = 0;
    pending->op = 0;
    pending->arg = 0;
    pending->thread = NULL;
    pending->cap_group = NULL;
    pending->recycle_retry = false;

    unlock(&sec_mem_shadow_pending_lock);
    return true;
}

static void handle_sec_mem_shadow_resume(
    const struct sec_mem_shadow_pending *pending, unsigned long ret_tee,
    unsigned long arg0, unsigned long arg1)
{
    int ret;
    struct thread *thread = pending->thread;
    struct cap_group *owner = pending->cap_group;

    kinfo("sec_mem_shadow_resume ret=%ld cookie=0x%lx op=%lu arg=0x%lx "
          "arg0=0x%lx arg1=0x%lx thread=%p\n",
          (long)ret_tee,
          pending->cookie,
          pending->op,
          pending->arg,
          arg0,
          arg1,
          thread);

    if (thread == NULL || owner == NULL) {
        kwarn("sec_mem_shadow_resume: stale pending cookie=0x%lx\n",
              pending->cookie);
        return;
    }

    if (pending->op == SEC_MEM_SHADOW_ALLOC) {
        if ((long)ret_tee >= 0 && arg0 != 0 && arg1 != 0) {
            ret = tzasc_cma_record_alloc(ret_tee, arg0, arg1, owner);
            if (ret != 0) {
                kwarn("sec_mem_shadow_resume: record alloc failed ret=%d "
                      "chunk=%lu\n",
                      ret,
                      ret_tee);
                ret_tee = (unsigned long)(long)ret;
            }
        }
    } else if (pending->op == SEC_MEM_SHADOW_FREE) {
        if ((long)ret_tee == 0 && arg0 == pending->arg && arg1 == 0) {
            ret = tzasc_cma_record_free(arg0, owner);
            if (ret != 0) {
                kwarn("sec_mem_shadow_resume: record free failed ret=%d "
                      "chunk=%lu\n",
                      ret,
                      arg0);
                ret_tee = (unsigned long)(long)ret;
            }
        }
    } else {
        kwarn("sec_mem_shadow_resume: unknown op=%lu cookie=0x%lx\n",
              pending->op,
              pending->cookie);
        ret_tee = (unsigned long)(long)-EINVAL;
    }

    if (pending->recycle_retry && (long)ret_tee == 0)
        ret_tee = (unsigned long)(long)-EAGAIN;

    arch_set_thread_return(thread, ret_tee);
    thread->thread_ctx->state = TS_INTER;
    BUG_ON(sched_enqueue(thread));
}

void handle_yield_smc(unsigned long x0, unsigned long x1, unsigned long x2,
                      unsigned long x3, unsigned long x4)
{
    int ret;
    struct smc_percpu_struct *percpu;
    struct sec_mem_shadow_pending pending;

    enable_tlogger();

    kdebug("%s x: [%lx %lx %lx %lx %lx]\n", __func__, x0, x1, x2, x3, x4);

    if (sec_mem_shadow_pending_take(x2, &pending)) {
        handle_sec_mem_shadow_resume(&pending, x1, x3, x4);
        sched();
        eret_to_thread(switch_context());
        BUG("Should not reach here.\n");
    }

    /* Switch from SMC page table to process page table */
    switch_vmspace_to(current_thread->vmspace);

    if (!kernel_shared_var_recved && x2 == 0xf) {
        kernel_shared_var_recved = true;
        kernel_var.params_stack[0] = x0;
        kernel_var.params_stack[1] = x1;
        kernel_var.params_stack[2] = x2;
        kernel_var.params_stack[3] = x3;
        kernel_var.params_stack[4] = x4;
    }

    percpu = &smc_percpu_structs[smp_get_cpu_id()];
    if (percpu->pending_req) {
        sched();
        eret_to_thread(switch_context());
    }
    percpu->regs_k.x0 = TZ_SWITCH_REQ_STD_REQUEST;
    percpu->regs_k.x1 = x1;
    percpu->regs_k.x2 = x2;
    percpu->regs_k.x3 = x3;
    percpu->regs_k.x4 = x4;

    if (percpu->waiting_thread) {
        struct thread *current = current_thread;
        switch_thread_vmspace_to(percpu->waiting_thread);
        ret = copy_to_user((char *)percpu->regs_u,
                           (char *)&percpu->regs_k,
                           sizeof(percpu->regs_k));
        switch_thread_vmspace_to(current);
        arch_set_thread_return(percpu->waiting_thread, ret);
        percpu->waiting_thread->thread_ctx->state = TS_INTER;
        BUG_ON(sched_enqueue(percpu->waiting_thread));
        percpu->waiting_thread = NULL;
    } else {
        percpu->pending_req = true;
    }

    sched();
    eret_to_thread(switch_context());
}

int sys_tee_wait_switch_req(struct smc_registers *regs_u)
{
    int ret;
    struct smc_percpu_struct *percpu;

    percpu = &smc_percpu_structs[smp_get_cpu_id()];

    if (percpu->pending_req) {
        percpu->pending_req = false;
        ret = copy_to_user(
            (char *)regs_u, (char *)&percpu->regs_k, sizeof(percpu->regs_k));
        return ret;
    }

    if (percpu->waiting_thread) {
        return -EINVAL;
    }

    percpu->waiting_thread = current_thread;
    percpu->regs_u = regs_u;

    current_thread->thread_ctx->state = TS_WAITING;

    sched();
    eret_to_thread(switch_context());
    BUG("Should not reach here.\n");
}

int sys_tee_switch_req(struct smc_registers *regs_u)
{
    int ret;
    struct smc_registers regs_k;

    ret = copy_from_user((char *)&regs_k, (char *)regs_u, sizeof(regs_k));
    if (ret < 0) {
        return ret;
    }

    if (regs_k.x0 == TZ_SWITCH_REQ_ENTRY_DONE) {
        regs_k.x0 = SMC_ENTRY_DONE;
        regs_k.x1 = (vaddr_t)&tz_vectors;
    } else if (regs_k.x0 == TZ_SWITCH_REQ_STD_RESPONSE) {
        regs_k.x0 = SMC_STD_RESPONSE;
        regs_k.x1 = SMC_EXIT_NORMAL;
    } else {
        return -1;
    }

    save_and_release_fpu_owner();
    arch_set_thread_return(current_thread, 0);
    smc_call(regs_k.x0, regs_k.x1, regs_k.x2, regs_k.x3, regs_k.x4);
    BUG("Should not reach here.\n");
}

#ifdef CHCORE_ENABLE_TZASC_CMA
static unsigned long sec_mem_shadow_req_for_owner(unsigned long op,
                                                  unsigned long arg,
                                                  struct cap_group *owner,
                                                  bool recycle_retry,
                                                  bool put_owner_before_smc)
{
    struct thread *thread = current_thread;
    unsigned long encoded = (arg << SEC_MEM_SHADOW_ARG_SHIFT) | op;
    unsigned long cookie;
    int ret;

    if (thread == NULL || owner == NULL)
        return -EINVAL;

    ret = sec_mem_shadow_pending_alloc(
        op, arg, owner, recycle_retry, &cookie);
    if (ret != 0)
        return ret;

    arch_set_thread_return(thread, 0);
    thread->thread_ctx->state = TS_INTER;
    thread->thread_ctx->kernel_stack_state = KS_FREE;

    save_and_release_fpu_owner();
    /* Recycle requests will not return through this kernel stack. */
    if (put_owner_before_smc)
        obj_put(owner);
    current_thread = NULL;
    kinfo("sec_mem_shadow_req op=%lu arg=0x%lx encoded=0x%lx cookie=0x%lx "
          "thread=%p owner=%p recycle_retry=%d\n",
          op,
          arg,
          encoded,
          cookie,
          thread,
          owner,
          recycle_retry);
    smc_call(SMC_STD_RESPONSE, SMC_EXIT_SHADOW, encoded, cookie, 0);
    BUG("Should not reach here.\n");
}

static unsigned long sec_mem_shadow_req(unsigned long op, unsigned long arg)
{
    return sec_mem_shadow_req_for_owner(
        op, arg, current_cap_group, false, false);
}
#endif

unsigned long sys_tzasc_cma_alloc(unsigned long size)
{
#ifndef CHCORE_ENABLE_TZASC_CMA
    (void)size;
    return -ENOSYS;
#else
    if (size == 0 || (size >> (sizeof(unsigned long) * 8 - SEC_MEM_SHADOW_ARG_SHIFT)) != 0)
        return -EINVAL;

    size = ROUND_UP(size, PAGE_SIZE);
    return sec_mem_shadow_req(SEC_MEM_SHADOW_ALLOC, size);
#endif
}

int sys_tzasc_cma_free(unsigned long chunk_id)
{
#ifndef CHCORE_ENABLE_TZASC_CMA
    (void)chunk_id;
    return -ENOSYS;
#else
    int ret;

    if (chunk_id == 0 || (chunk_id >> (sizeof(unsigned long) * 8 - SEC_MEM_SHADOW_ARG_SHIFT)) != 0)
        return -EINVAL;

    ret = tzasc_cma_release_region(chunk_id, current_cap_group);
    if (ret < 0)
        return ret;
    return sec_mem_shadow_req(SEC_MEM_SHADOW_FREE, chunk_id);
#endif
}

int tee_recycle_tzasc_cma(struct cap_group *owner)
{
#ifndef CHCORE_ENABLE_TZASC_CMA
    (void)owner;
    return 0;
#else
    unsigned long chunk_id;
    int ret;

    ret = tzasc_cma_get_owned_chunk_id(owner, &chunk_id);
    if (ret == -ENOENT)
        return 0;
    if (ret < 0)
        return ret;

    ret = tzasc_cma_release_region(chunk_id, owner);
    if (ret == -ENOENT)
        return -EAGAIN;
    if (ret < 0) {
        kwarn("tee_recycle_tzasc_cma: release region failed ret=%d "
              "chunk=%lu owner=%p\n",
              ret,
              chunk_id,
              owner);
        return -EAGAIN;
    }

    kinfo("tee_recycle_tzasc_cma: free leaked chunk=%lu owner=%p\n",
          chunk_id,
          owner);
    ret = (int)(long)sec_mem_shadow_req_for_owner(
        SEC_MEM_SHADOW_FREE, chunk_id, owner, true, true);
    if (ret < 0)
        return -EAGAIN;
    return ret;
#endif
}

int sys_tee_pull_kernel_var(kernel_shared_varibles_t *kernel_var_ubuf)
{
    int ret;

    kinfo("%s\n", __func__);

    if (check_user_addr_range((vaddr_t)kernel_var_ubuf,
                              sizeof(kernel_shared_varibles_t))) {
        return -EINVAL;
    }

    ret = copy_to_user(
        kernel_var_ubuf, &kernel_var, sizeof(kernel_shared_varibles_t));

    return ret;
}
