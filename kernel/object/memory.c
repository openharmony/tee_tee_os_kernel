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
#include <object/object.h>
#include <object/thread.h>
#include <object/memory.h>
#include <mm/vmspace.h>
#include <mm/uaccess.h>
#include <mm/mm.h>
#include <mm/kmalloc.h>
#include <common/lock.h>
#include <common/util.h>
#include <arch/mmu.h>
#include <object/user_fault.h>
#include <syscall/syscall_hooks.h>
#include <arch/mm/cache.h>

#include "mmap.h"

static int pmo_init(struct pmobject *pmo, pmo_type_t type, size_t len,
                    paddr_t paddr, struct cap_group *cap_group);
void pmo_deinit(void *pmo_ptr);

/*
 * @paddr is only used when creating device pmo;
 * @new_pmo is output arg if it is not NULL.
 */
cap_t create_pmo(size_t size, pmo_type_t type, struct cap_group *cap_group,
                 paddr_t paddr, struct pmobject **new_pmo)
{
    cap_t r, cap;
    struct pmobject *pmo;

    pmo = obj_alloc(TYPE_PMO, sizeof(*pmo));
    if (!pmo) {
        r = -ENOMEM;
        goto out_fail;
    }

    r = pmo_init(pmo, type, size, paddr, cap_group);
    if (r)
        goto out_free_obj;

    cap = cap_alloc(cap_group, pmo);
    if (cap < 0) {
        r = cap;
        goto out_pmo_deinit;
    }

    if (new_pmo != NULL)
        *new_pmo = pmo;

    return cap;

out_pmo_deinit:
    pmo_deinit(pmo);
out_free_obj:
    obj_free(pmo);
out_fail:
    return r;
}

cap_t sys_create_device_pmo(unsigned long paddr, unsigned long size)
{
    cap_t r;

    if (size == 0)
        return -EINVAL;

    // Authorization on this senstive syscall
    if ((r = hook_sys_create_device_pmo(paddr, size)) != 0)
        return r;

    r = create_pmo(size, PMO_DEVICE, current_cap_group, paddr, NULL);

    return r;
}

int sys_tee_create_ns_pmo(unsigned long paddr, unsigned long size)
{
    int r;

    if (size == 0)
        return -EINVAL;

    r = create_pmo(size, PMO_TZ_NS, current_cap_group, paddr, NULL);

    return r;
}

cap_t sys_create_pmo(unsigned long size, pmo_type_t type)
{
    if ((size == 0) || (type == PMO_DEVICE))
        return -EINVAL;
#ifdef CHCORE_OH_TEE
    if (type == PMO_TZ_NS)
        return -EINVAL;
#endif /* CHCORE_OH_TEE */
    return create_pmo(size, type, current_cap_group, 0, NULL);
}

#define WRITE 0
#define READ  1
static int read_write_pmo(cap_t pmo_cap, unsigned long offset,
                          unsigned long user_buf, unsigned long size,
                          unsigned long op_type)
{
    struct pmobject *pmo;
    pmo_type_t pmo_type;
    vaddr_t kva;
    int r = 0;

    /* Only READ and WRITE operations are allowed. */
    if (op_type != READ && op_type != WRITE) {
        r = -EINVAL;
        goto out_fail;
    }

    if (check_user_addr_range(user_buf, size) != 0) {
        r = -EINVAL;
        goto out_fail;
    }

    pmo = obj_get(current_cap_group, pmo_cap, TYPE_PMO);
    if (!pmo) {
        r = -ECAPBILITY;
        goto out_fail;
    }

    /* Overflow check and Range check */
    if ((offset + size < offset) || (offset + size > pmo->size)) {
        r = -EINVAL;
        goto out_obj_put;
    }

    pmo_type = pmo->type;
    if ((pmo_type != PMO_DATA) && (pmo_type != PMO_DATA_NOCACHE)
        && (pmo_type != PMO_ANONYM)) {
        r = -EINVAL;
        goto out_obj_put;
    }

    if (pmo_type == PMO_DATA || pmo_type == PMO_DATA_NOCACHE) {
        kva = phys_to_virt(pmo->start) + offset;
        if (op_type == WRITE)
            r = copy_from_user((void *)kva, (void *)user_buf, size);
        else // op_type == READ
            r = copy_to_user((void *)user_buf, (void *)kva, size);

        if (r) {
            r = -EINVAL;
            goto out_obj_put;
        }
    } else {
        /* PMO_ANONYM */
        unsigned long index;
        unsigned long pa;
        unsigned long to_read_write;
        unsigned long offset_in_page;

        while (size > 0) {
            index = ROUND_DOWN(offset, PAGE_SIZE) / PAGE_SIZE;
            pa = get_page_from_pmo(pmo, index);
            if (pa == 0) {
                /* Allocate a physical page for the anonymous
                 * pmo like a page fault happens.
                 */
                kva = (vaddr_t)get_pages(0);
                if (kva == 0) {
                    r = -ENOMEM;
                    goto out_obj_put;
                }

                pa = virt_to_phys((void *)kva);
                memset((void *)kva, 0, PAGE_SIZE);
                commit_page_to_pmo(pmo, index, pa);

                /* No need to map the physical page in the page
                 * table of current process because it uses
                 * write/read_pmo which means it does not need
                 * the mappings.
                 */
            } else {
                kva = phys_to_virt(pa);
            }
            /* Now kva is the beginning of some page, we should add
             * the offset inside the page. */
            offset_in_page = offset - ROUND_DOWN(offset, PAGE_SIZE);
            kva += offset_in_page;
            to_read_write = MIN(PAGE_SIZE - offset_in_page, size);

            if (op_type == WRITE)
                r = copy_from_user(
                    (void *)kva, (void *)user_buf, to_read_write);
            else // op_type == READ
                r = copy_to_user((void *)user_buf, (void *)kva, to_read_write);

            if (r) {
                r = -EINVAL;
                goto out_obj_put;
            }

            offset += to_read_write;
            size -= to_read_write;
        }
    }

out_obj_put:
    obj_put(pmo);
out_fail:
    return r;
}

/*
 * A process can send a PMO (with msgs) to others.
 * It can write the msgs without mapping the PMO with this function.
 */
int sys_write_pmo(cap_t pmo_cap, unsigned long offset, unsigned long user_ptr,
                  unsigned long len)
{
    return read_write_pmo(pmo_cap, offset, user_ptr, len, WRITE);
}

int sys_read_pmo(cap_t pmo_cap, unsigned long offset, unsigned long user_ptr,
                 unsigned long len)
{
    return read_write_pmo(pmo_cap, offset, user_ptr, len, READ);
}

/* Given a virtual address, return its corresponding physical address. */
int sys_get_phys_addr(vaddr_t va, paddr_t *pa_buf)
{
    struct vmspace *vmspace;
    paddr_t pa;
    int ret;

    // if ((ret = hook_sys_get_phys_addr(va, pa_buf)) != 0)
    //         return ret;

    if ((check_user_addr_range(va, 0) != 0)
        || (check_user_addr_range((vaddr_t)pa_buf, sizeof(*pa_buf)) != 0))
        return -EINVAL;

    vmspace = current_thread->vmspace;
    lock(&vmspace->pgtbl_lock);
    ret = query_in_pgtbl(vmspace->pgtbl, va, &pa, NULL);
    unlock(&vmspace->pgtbl_lock);

    if (ret < 0)
        return ret;

    ret = copy_to_user(pa_buf, &pa, sizeof(*pa_buf));
    if (ret) {
        return -EINVAL;
    }

    return 0;
}

int trans_uva_to_kva(vaddr_t user_va, vaddr_t *kernel_va)
{
    struct vmspace *vmspace = current_thread->vmspace;
    paddr_t pa;
    int ret;

    lock(&vmspace->pgtbl_lock);
    ret = query_in_pgtbl(vmspace->pgtbl, user_va, &pa, NULL);
    unlock(&vmspace->pgtbl_lock);

    if (ret < 0)
        return ret;

    *kernel_va = phys_to_virt(pa);
    return 0;
}

/*
 * A process can not only map a PMO into its private address space,
 * but also can map a PMO to some others (e.g., load code for others).
 */
int sys_map_pmo(cap_t target_cap_group_cap, cap_t pmo_cap, unsigned long addr,
                unsigned long perm, unsigned long len)
{
    struct vmspace *vmspace;
    struct pmobject *pmo;
    struct cap_group *target_cap_group;
    int r;

    pmo = obj_get(current_cap_group, pmo_cap, TYPE_PMO);
    if (!pmo) {
        r = -ECAPBILITY;
        goto out_fail;
    }

#ifdef CHCORE_OH_TEE
    if (pmo->type == PMO_TZ_NS) {
        if (((struct ns_pmo_private *)pmo->private)->mapped) {
            r = -EINVAL;
            goto out_obj_put_pmo;
        }
    }
#endif /* CHCORE_OH_TEE */

    /* set default length (-1) to pmo_size */
    if (likely(len == -1))
        len = pmo->size;

    if (check_user_addr_range(addr, len) != 0) {
        r = -EINVAL;
        goto out_obj_put_pmo;
    }

    /* map the pmo to the target cap_group */
    target_cap_group =
        obj_get(current_cap_group, target_cap_group_cap, TYPE_CAP_GROUP);
    if (!target_cap_group) {
        r = -ECAPBILITY;
        goto out_obj_put_pmo;
    }
    vmspace = obj_get(target_cap_group, VMSPACE_OBJ_ID, TYPE_VMSPACE);
    BUG_ON(vmspace == NULL);

#ifdef CHCORE_OH_TEE
    if (pmo->type == PMO_SHM && pmo->private != NULL) {
        struct tee_shm_private *private;
    private
        = (struct tee_shm_private *)pmo->private;
        if (private->owner != target_cap_group
            && memcmp(&current_cap_group->uuid,
                      &private->uuid,
                      sizeof(struct tee_uuid))
                   != 0) {
            r = -EINVAL;
            goto out_obj_put_vmspace;
        }
    }
#endif /* CHCORE_OH_TEE */

    r = vmspace_map_range(vmspace, addr, len, perm, pmo);
    if (r != 0) {
        r = -EPERM;
        goto out_obj_put_vmspace;
    }

    /*
     * when a process maps a pmo to others,
     * this func returns the new_cap in the target process.
     */
    if (target_cap_group != current_cap_group)
        /* if using cap_move, we need to consider remove the mappings */
        r = cap_copy(current_cap_group, target_cap_group, pmo_cap);
    else
        r = 0;

out_obj_put_vmspace:
    obj_put(vmspace);
    obj_put(target_cap_group);
out_obj_put_pmo:
    obj_put(pmo);
out_fail:
    return r;
}

/* Example usage: Used in ipc/connection.c for mapping ipc_shm */
int map_pmo_in_current_cap_group(cap_t pmo_cap, unsigned long addr,
                                 unsigned long perm)
{
    struct vmspace *vmspace;
    struct pmobject *pmo;
    int r;

    pmo = obj_get(current_cap_group, pmo_cap, TYPE_PMO);
    if (!pmo) {
        kdebug("map fails: invalid pmo (cap is %lu)\n", pmo_cap);
        r = -ECAPBILITY;
        goto out_fail;
    }

    vmspace = obj_get(current_cap_group, VMSPACE_OBJ_ID, TYPE_VMSPACE);
    BUG_ON(vmspace == NULL);

    if (check_user_addr_range(addr, pmo->size) != 0) {
        r = -EINVAL;
        goto out_fail;
    }

    r = vmspace_map_range(vmspace, addr, pmo->size, perm, pmo);
    if (r != 0) {
        goto out_obj_put_vmspace;
    }

out_obj_put_vmspace:
    obj_put(vmspace);
    obj_put(pmo);
out_fail:
    return r;
}

int sys_unmap_pmo(cap_t target_cap_group_cap, cap_t pmo_cap, unsigned long addr)
{
    struct vmspace *vmspace;
    struct pmobject *pmo;
    struct cap_group *target_cap_group;
    int ret;

    /* caller should have the pmo_cap */
    pmo = obj_get(current_cap_group, pmo_cap, TYPE_PMO);
    if (!pmo)
        return -ECAPBILITY;

    /* map the pmo to the target cap_group */
    target_cap_group =
        obj_get(current_cap_group, target_cap_group_cap, TYPE_CAP_GROUP);
    if (!target_cap_group) {
        ret = -ECAPBILITY;
        goto out_obj_put_pmo;
    }

    vmspace = obj_get(target_cap_group, VMSPACE_OBJ_ID, TYPE_VMSPACE);
    if (!vmspace) {
        ret = -ECAPBILITY;
        goto out_obj_put_cap_group;
    }

    ret = vmspace_unmap_range(vmspace, addr, pmo->size);

    obj_put(vmspace);

out_obj_put_cap_group:
    obj_put(target_cap_group);
out_obj_put_pmo:
    obj_put(pmo);

    return ret;
}

/*
 * Initialize an allocated pmobject.
 * @paddr is only used when @type == PMO_DEVICE || @type == PMO_TZ_NS.
 */
static int pmo_init(struct pmobject *pmo, pmo_type_t type, size_t len,
                    paddr_t paddr, struct cap_group *cap_group)
{
    int ret = 0;

#ifdef CHCORE_OH_TEE
    lock(&cap_group->heap_size_lock);
    if (cap_group->heap_size_used + len > cap_group->heap_size_limit) {
        ret = -ENOMEM;
        goto out;
    }
#endif /* CHCORE_OH_TEE */

    memset((void *)pmo, 0, sizeof(*pmo));

    len = ROUND_UP(len, PAGE_SIZE);
    pmo->size = len;
    pmo->type = type;

    switch (type) {
    case PMO_DATA:
    case PMO_DATA_NOCACHE: {
        /*
         * For PMO_DATA, the user will use it soon (we expect).
         * So, we directly allocate the physical memory.
         * Note that kmalloc(>2048) returns continous physical pages.
         */
        void *new_va = kmalloc(len);
        if (new_va == NULL)
            return -ENOMEM;

        /* Clear the allocated memory */
        memset(new_va, 0, len);
        if (type == PMO_DATA_NOCACHE)
            arch_flush_cache((vaddr_t)new_va, len, CACHE_CLEAN_AND_INV);
        pmo->start = virt_to_phys(new_va);
        break;
    }
    case PMO_FILE: {
#ifdef CHCORE_ENABLE_FMAP
        /*
         * PMO backed by a file.
         * We store PMO_FILE metadata in fmap_fault_pool in
         * pmo->private, and pmo->private is initialized NULL by memset.
         */
        struct fmap_fault_pool *pool_iter;
        badge_t badge;
        badge = current_cap_group->badge;
        lock(&fmap_fault_pool_list_lock);
        for_each_in_list (
            pool_iter, struct fmap_fault_pool, node, &fmap_fault_pool_list) {
            if (pool_iter->cap_group_badge == badge) {
                pmo->private = pool_iter;
                break;
            }
        }
        unlock(&fmap_fault_pool_list_lock);
        if (pmo->private == NULL) {
            /* fmap_fault_pool not registered */
            ret = -EINVAL;
            break;
        }

        pmo->radix = new_radix();
        init_radix(pmo->radix);
#else
        kwarn("fmap is not implemented, we should not use PMO_FILE\n");
        ret = -EINVAL;
#endif
        break;
    }
    case PMO_ANONYM:
    case PMO_SHM: {
        /*
         * For PMO_ANONYM (e.g., stack and heap) or PMO_SHM,
         * we do not allocate the physical memory at once.
         */
        pmo->radix = new_radix();
        init_radix(pmo->radix);
        break;
    }
    case PMO_DEVICE: {
        /*
         * For device memory (e.g., for DMA).
         * We must ensure the range [paddr, paddr+len) is not
         * in the main memory region.
         */
        pmo->start = paddr;
        break;
    }
#ifdef CHCORE_OH_TEE
    case PMO_TZ_NS: {
        pmo->start = paddr;
        pmo->private = kzalloc(sizeof(struct ns_pmo_private));
        if (pmo->private == NULL) {
            ret = -ENOMEM;
        } else {
            ((struct ns_pmo_private *)pmo->private)->creater =
                current_cap_group;
        }
        break;
    }
#endif /* CHCORE_OH_TEE */
    case PMO_FORBID: {
        /* This type marks the corresponding area cannot be accessed */
        break;
    }
    default: {
        ret = -EINVAL;
        break;
    }
    }
#ifdef CHCORE_OH_TEE
out:
    if (ret == 0) {
        cap_group->heap_size_used += len;
        lock_init(&pmo->owner_lock);
        pmo->owner = obj_get(cap_group, CAP_GROUP_OBJ_ID, TYPE_CAP_GROUP);
        BUG_ON(pmo->owner == NULL);
    }
    unlock(&cap_group->heap_size_lock);
#endif /* CHCORE_OH_TEE */
    return ret;
}

/* Record the physical page allocated to a pmo */
void commit_page_to_pmo(struct pmobject *pmo, unsigned long index, paddr_t pa)
{
    int ret;

    BUG_ON((pmo->type != PMO_ANONYM) && (pmo->type != PMO_SHM)
           && (pmo->type != PMO_FILE));
    /* The radix interfaces are thread-safe */
    ret = radix_add(pmo->radix, index, (void *)pa);
    BUG_ON(ret != 0);
}

/* Return 0 (NULL) when not found */
paddr_t get_page_from_pmo(struct pmobject *pmo, unsigned long index)
{
    paddr_t pa;

    /* The radix interfaces are thread-safe */
    pa = (paddr_t)radix_get(pmo->radix, index);
    return pa;
}

static void __free_pmo_page(void *addr)
{
    kfree((void *)phys_to_virt(addr));
}

void pmo_deinit(void *pmo_ptr)
{
    struct pmobject *pmo;
    pmo_type_t type;

    pmo = (struct pmobject *)pmo_ptr;
    type = pmo->type;

#ifdef CHCORE_OH_TEE
    lock(&pmo->owner->heap_size_lock);
    pmo->owner->heap_size_used -= pmo->size;
    unlock(&pmo->owner->heap_size_lock);
    obj_put(pmo->owner);
#endif /* CHCORE_OH_TEE */

    switch (type) {
    case PMO_DATA:
    case PMO_DATA_NOCACHE: {
        paddr_t start_addr;

        /* PMO_DATA contains continous physical pages */
        start_addr = pmo->start;
        kfree((void *)phys_to_virt(start_addr));

        break;
    }
    case PMO_SHM: {
        if (pmo->private) {
            kfree(pmo->private);
        }
    }
    case PMO_FILE:
    case PMO_ANONYM: {
        struct radix *radix;

        radix = pmo->radix;
        BUG_ON(radix == NULL);
        /*
         * Set value_deleter to free each memory page during
         * traversing the radix tree in radix_free.
         */
        radix->value_deleter = __free_pmo_page;
        radix_free(radix);

        break;
    }
    case PMO_DEVICE:
#ifdef CHCORE_OH_TEE
    case PMO_TZ_NS: {
        kfree(pmo->private);
        break;
    }
#endif /* CHCORE_OH_TEE */
    case PMO_FORBID: {
        break;
    }
    default: {
        kinfo("Unsupported pmo type: %d\n", type);
        BUG_ON(1);
        break;
    }
    }

    /* The pmo struct itself will be free in __free_object */
}

unsigned long sys_handle_brk(unsigned long addr, unsigned long heap_start)
{
    struct vmspace *vmspace;
    struct pmobject *pmo;
    struct vmregion *heap_vmr;
    size_t len;
    unsigned long retval = 0;
    cap_t pmo_cap;

    if ((check_user_addr_range(addr, 0) != 0)
        || (check_user_addr_range(heap_start, 0) != 0))
        return -EINVAL;

    vmspace = obj_get(current_cap_group, VMSPACE_OBJ_ID, TYPE_VMSPACE);
    BUG_ON(vmspace == NULL);
    lock(&vmspace->vmspace_lock);
    if (addr == 0) {
        retval = heap_start;

        /* create the heap pmo for the user process */
        len = 0;
        pmo_cap = create_pmo(len, PMO_ANONYM, current_cap_group, 0, &pmo);
        if (pmo_cap < 0) {
            kinfo("Fail: cannot create the initial heap pmo.\n");
            BUG_ON(1);
        }

        /* setup the vmr for the heap region */
        heap_vmr = init_heap_vmr(vmspace, retval, pmo);
        if (!heap_vmr) {
            kinfo("Fail: cannot create the initial heap pmo.\n");
            BUG_ON(1);
        }
        vmspace->heap_vmr = heap_vmr;
    } else {
        heap_vmr = vmspace->heap_vmr;
        if (unlikely(heap_vmr == NULL))
            goto out;

        /* old heap end */
        retval = heap_vmr->start + heap_vmr->size;

        if (addr >= retval) {
            /* enlarge the heap vmr and pmo */
            len = addr - retval;
            lock(&current_cap_group->heap_size_lock);
            if (current_cap_group->heap_size_used + len > current_cap_group->heap_size_limit) {
                retval = -ENOMEM;
            } else {
                current_cap_group->heap_size_used += len;
                adjust_heap_vmr(vmspace, len);
                retval = addr;
            }
            unlock(&current_cap_group->heap_size_lock);
        } else {
            kwarn("VM: ignore shrinking the heap.\n");
        }
    }

out:
    unlock(&vmspace->vmspace_lock);
    obj_put(vmspace);
    return retval;
}

/* A process mmap region start:  MMAP_START (defined in mm/vmregion.c) */
static vmr_prop_t get_vmr_prot(int prot)
{
    vmr_prop_t ret;

    ret = 0;
    if (prot & PROT_READ)
        ret |= VMR_READ;
    if (prot & PROT_WRITE)
        ret |= VMR_WRITE;
    if (prot & PROT_EXEC)
        ret |= VMR_EXEC;

    return ret;
}

int sys_handle_mprotect(unsigned long addr, unsigned long length, int prot)
{
    vmr_prop_t target_prot;
    struct vmspace *vmspace;
    struct vmregion *vmr;
    s64 remaining;
    unsigned long va;
    int ret;

    if ((addr % PAGE_SIZE) || (length % PAGE_SIZE)) {
        return -EINVAL;
    }

    if (length == 0)
        return 0;

    target_prot = get_vmr_prot(prot);
    vmspace = obj_get(current_cap_group, VMSPACE_OBJ_ID, TYPE_VMSPACE);
    BUG_ON(vmspace == NULL);

    lock(&vmspace->vmspace_lock);
    /*
     * Validate the VM range [addr, addr + lenght]
     * - the range is totally mapped
     * - the range has more permission than prot
     */
    va = addr;
    remaining = length;
    while (remaining > 0) {
        vmr = find_vmr_for_va(vmspace, va);

        if (!vmr) {
            ret = -EINVAL;
            goto out;
        }

        if (remaining < vmr->size) {
            static int warn = 1;

            
            if (warn == 1) {
                kwarn("func: %s, ignore a mprotect since no supporting for "
                      "splitting vmr now.\n",
                      __func__);
                warn = 0;
            }

            ret = 0;
            goto out;
        }

        /* Change the prot in each vmr */
        vmr->perm = target_prot;

        remaining -= vmr->size;
        va += vmr->size;
    }

    /* Modify the existing mappings in pgtbl */
    lock(&vmspace->pgtbl_lock);
    mprotect_in_pgtbl(vmspace->pgtbl, addr, length, target_prot);
    unlock(&vmspace->pgtbl_lock);
    ret = 0;

out:
    obj_put(vmspace);
    unlock(&vmspace->vmspace_lock);

    return ret;
}

unsigned long sys_get_free_mem_size(void)
{
    return get_free_mem_size();
}

#ifdef CHCORE_OH_TEE
static int __destroy_ns_pmo(struct vmspace *vmspace, struct pmobject *pmobject)
{
    int ret;
    struct ns_pmo_private *private;

    if (pmobject->type != PMO_TZ_NS) {
        ret = -EINVAL;
        goto out;
    }
private
    = pmobject->private;
    if (private->creater != current_cap_group) {
        ret = -EINVAL;
        goto out;
    }

    ret = vmspace_unmap_range(vmspace, private->vaddr, private->len);

out:
    return ret;
}

cap_t sys_create_ns_pmo(cap_t cap_group, unsigned long paddr,
                        unsigned long size)
{
    cap_t ret;
    struct cap_group *target_cap_group;

    if (size == 0) {
        ret = -EINVAL;
        goto out;
    }

    target_cap_group = obj_get(current_cap_group, cap_group, TYPE_CAP_GROUP);
    if (target_cap_group == NULL) {
        ret = -ECAPBILITY;
        goto out;
    }

    /* pmo is the cap in target_cap_group */
    ret = create_pmo(size, PMO_TZ_NS, target_cap_group, paddr, NULL);
    if (ret < 0) {
        goto out_put_cap_group;
    }

out_put_cap_group:
    obj_put(target_cap_group);

out:
    return ret;
}

int sys_destroy_ns_pmo(cap_t cap_group, cap_t pmo)
{
    int ret;
    struct cap_group *target_cap_group;
    struct vmspace *vmspace;
    struct pmobject *pmobject;

    target_cap_group = obj_get(current_cap_group, cap_group, TYPE_CAP_GROUP);
    if (target_cap_group == NULL) {
        ret = -ECAPBILITY;
        goto out;
    }
    pmobject = obj_get(target_cap_group, pmo, TYPE_PMO);
    if (pmobject == NULL) {
        /* task may already unmap and revoke pmo */
        ret = 0;
        goto out_put_cap_group;
    }
    vmspace = obj_get(target_cap_group, VMSPACE_OBJ_ID, TYPE_VMSPACE);
    if (vmspace == NULL) {
        ret = -ECAPBILITY;
        goto out_put_pmo;
    }

    ret = __destroy_ns_pmo(vmspace, pmobject);
    if (ret < 0) {
        goto out_put_vmspace;
    }

    ret = cap_free(target_cap_group, pmo);

out_put_vmspace:
    obj_put(vmspace);
out_put_pmo:
    obj_put(pmobject);
out_put_cap_group:
    obj_put(target_cap_group);
out:
    return ret;
}

cap_t sys_create_tee_shared_pmo(cap_t cap_group, struct tee_uuid *uuid,
                                unsigned long size, cap_t *self_cap)
{
    int ret;
    struct tee_shm_private *private;
    struct cap_group *target_cap_group;
    struct pmobject *pmobject;
    bool success = false;
    cap_t self_pmo, target_pmo;

    if (check_user_addr_range((vaddr_t)uuid, sizeof(*uuid)) != 0) {
        ret = -EINVAL;
        goto out;
    }
    if (check_user_addr_range((vaddr_t)self_cap, sizeof(*self_cap)) != 0) {
        ret = -EINVAL;
        goto out;
    }

private
    = kmalloc(sizeof(*private));
    if (private == NULL) {
        ret = -ENOMEM;
        goto out;
    }
    copy_from_user(&private->uuid, uuid, sizeof(*uuid));

    target_cap_group = obj_get(current_cap_group, cap_group, TYPE_CAP_GROUP);
    if (target_cap_group == NULL) {
        ret = -ECAPBILITY;
        goto out_free_uuid;
    }

    target_pmo = create_pmo(size, PMO_SHM, target_cap_group, 0, &pmobject);
    if (target_pmo < 0) {
        ret = target_pmo;
        goto out_put_cap_group;
    }
private
    ->owner = target_cap_group;
    pmobject->private = private;

    self_pmo = cap_copy(target_cap_group, current_cap_group, target_pmo);
    if (self_pmo < 0) {
        ret = self_pmo;
        goto out_destroy_pmo;
    }
    copy_to_user(self_cap, &self_pmo, sizeof(self_pmo));
    success = true;
    ret = target_pmo;

out_destroy_pmo:
    if (!success) {
        cap_free(target_cap_group, target_pmo);
    }
out_put_cap_group:
    obj_put(target_cap_group);
out_free_uuid:
    if (!success) {
        kfree(private);
    }
out:
    return ret;
}

int sys_transfer_pmo_owner(cap_t pmo, cap_t cap_group)
{
    int ret;
    bool put_cap_group = true;
    struct pmobject *pmobject;
    struct cap_group *target_cap_group;

    pmobject = obj_get(current_cap_group, pmo, TYPE_PMO);
    if (pmobject == NULL) {
        ret = -ECAPBILITY;
        goto out;
    }

    target_cap_group = obj_get(current_cap_group, cap_group, TYPE_CAP_GROUP);
    if (target_cap_group == NULL) {
        ret = -ECAPBILITY;
        goto out_put_pmo;
    }

    if (target_cap_group == current_cap_group) {
        ret = 0;
        goto out_put_cap_group;
    }

    lock(&pmobject->owner_lock);
    if (pmobject->owner == current_cap_group) {
        if (target_cap_group < current_cap_group) {
            lock(&target_cap_group->heap_size_lock);
            lock(&current_cap_group->heap_size_lock);
        } else {
            lock(&current_cap_group->heap_size_lock);
            lock(&target_cap_group->heap_size_lock);
        }

        if (target_cap_group->heap_size_used + pmobject->size
            <= target_cap_group->heap_size_limit) {
            current_cap_group->heap_size_used -= pmobject->size;
            target_cap_group->heap_size_used += pmobject->size;

            pmobject->owner = target_cap_group;
            /* CANNOT put target_cap_group because pmo has this ref */
            put_cap_group = false;
            obj_put(current_cap_group);

            ret = 0;
        } else {
            ret = -ENOMEM;
        }

        if (target_cap_group < current_cap_group) {
            unlock(&current_cap_group->heap_size_lock);
            unlock(&target_cap_group->heap_size_lock);
        } else {
            unlock(&target_cap_group->heap_size_lock);
            unlock(&current_cap_group->heap_size_lock);
        }
    } else {
        ret = -EINVAL;
    }
    unlock(&pmobject->owner_lock);

out_put_cap_group:
    if (put_cap_group) {
        obj_put(target_cap_group);
    }
out_put_pmo:
    obj_put(pmobject);
out:
    return ret;
}
#endif /* CHCORE_OH_TEE */
