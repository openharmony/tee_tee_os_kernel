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

#include "drv_io_share.h"
#include <chcore/memory.h>
#include <chcore/container/hashtable.h>
#include <pthread.h>
#include <chcore/syscall.h>
#include <malloc.h>

#define HTABLE_SIZE 509
static struct htable addr2ent;
static pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;
static bool io_init;

struct entry {
    paddr_t paddr;
    vaddr_t vaddr;
    unsigned long size;
    cap_t pmo;
    struct hlist_node node;
};

static void __self_init(void)
{
    if (io_init) {
        return;
    }
    pthread_mutex_lock(&lock);
    if (!io_init) {
        init_htable(&addr2ent, HTABLE_SIZE);
    }
    pthread_mutex_unlock(&lock);
}

static inline u32 __hash(paddr_t paddr)
{
    return (u32)(paddr >> 12);
}

static struct entry *__get_entry(paddr_t paddr, vaddr_t vaddr)
{
    struct entry *entry;
    struct hlist_head *bucket;

    bucket = htable_get_bucket(&addr2ent, __hash(paddr));
    for_each_in_hlist (entry, node, bucket) {
        if (entry->paddr == paddr && entry->vaddr == vaddr) {
            return entry;
        }
    }

    return NULL;
}

void *ioremap(uintptr_t phys_addr, unsigned long size, int32_t prot)
{
    struct entry *entry;
    void *vaddr;
    paddr_t paddr;
    cap_t pmo;
    int perm;

    __self_init();
    pthread_mutex_lock(&lock);

    entry = malloc(sizeof(entry));
    if (entry == NULL) {
        goto out_unlock;
    }

    paddr = ROUND_DOWN((paddr_t)phys_addr, PAGE_SIZE);
    size = ROUND_UP(size, PAGE_SIZE);

    pmo = usys_create_device_pmo(paddr, size);
    if (pmo <= 0) {
        goto out_free_entry;
    }

    perm = 0;
    if (prot & PROT_READ) {
        perm |= VMR_READ;
    }
    if (prot & PROT_WRITE) {
        perm |= VMR_WRITE;
    }
    vaddr = chcore_auto_map_pmo(pmo, size, perm);
    if (vaddr == NULL) {
        goto out_revoke_pmo;
    }

    init_hlist_node(&entry->node);
    entry->paddr = paddr;
    entry->vaddr = (vaddr_t)vaddr;
    entry->size = size;
    entry->pmo = pmo;
    htable_add(&addr2ent, __hash(paddr), &entry->node);

    return vaddr;

out_revoke_pmo:
    usys_revoke_cap(pmo, false);
out_free_entry:
    free(entry);
out_unlock:
    pthread_mutex_unlock(&lock);
}

int32_t iounmap(uintptr_t pddr, const void *addr)
{
    struct entry *entry;
    int ret = 0;

    __self_init();

    pthread_mutex_lock(&lock);

    entry = __get_entry(pddr, (vaddr_t)addr);
    if (entry == NULL) {
        ret = -ENOENT;
        goto out_unlock;
    }

    htable_del(&entry->node);
    chcore_auto_unmap_pmo(entry->pmo, entry->vaddr, entry->size);
    usys_revoke_cap(entry->pmo, false);
    free(entry);

out_unlock:
    pthread_mutex_unlock(&lock);
    return ret;
}
