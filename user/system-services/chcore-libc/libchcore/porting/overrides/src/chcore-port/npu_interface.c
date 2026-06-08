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

#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <assert.h>
#include <stdio.h>
#include <chcore/defs.h>
#include <chcore/memory.h>
#include <chcore/syscall.h>

#include "rknpu-ioctl.h"
#include "rknpu-driver.h"
#include "npu_interface.h"

#include <pthread.h>

#define RKNPU_PLATFORM_CORE_COUNT 3U
#define RKNPU_CORE_MASK(core_index) (1U << (core_index))

struct npu_dma_handle {
    struct chcore_dma_handle dma;
    unsigned long iova;
    unsigned long size;
};

static struct rknpu_device *rknpu_dev;
static pthread_mutex_t rknpu_dev_lock = PTHREAD_MUTEX_INITIALIZER;
static struct rknpu_device *npu_open(void);

static pthread_once_t npu_dma_stats_once = PTHREAD_ONCE_INIT;
static pthread_spinlock_t npu_dma_stats_lock;
static unsigned long npu_dma_live_count;
static unsigned long npu_dma_alloc_count;
static unsigned long npu_dma_live_bytes;
static unsigned long npu_dma_peak_bytes;

static struct rknpu_device *npu_get_device(void)
{
	struct rknpu_device *dev;

	pthread_mutex_lock(&rknpu_dev_lock);
	if (rknpu_dev == NULL)
		rknpu_dev = npu_open();
	dev = rknpu_dev;
	pthread_mutex_unlock(&rknpu_dev_lock);

	return dev;
}

static void npu_dma_stats_init(void)
{
	pthread_spin_init(&npu_dma_stats_lock, 0);
}

static void npu_dma_stats_alloc(const struct npu_dma_handle *dma_handle,
				unsigned long requested_size,
				uint32_t flags)
{
	unsigned long live_count;
	unsigned long alloc_count;
	unsigned long live_bytes;
	unsigned long peak_bytes;

	pthread_once(&npu_dma_stats_once, npu_dma_stats_init);
	pthread_spin_lock(&npu_dma_stats_lock);
	npu_dma_live_count++;
	npu_dma_alloc_count++;
	npu_dma_live_bytes += dma_handle->size;
	if (npu_dma_live_bytes > npu_dma_peak_bytes)
		npu_dma_peak_bytes = npu_dma_live_bytes;
	live_count = npu_dma_live_count;
	alloc_count = npu_dma_alloc_count;
	live_bytes = npu_dma_live_bytes;
	peak_bytes = npu_dma_peak_bytes;
	pthread_spin_unlock(&npu_dma_stats_lock);
}

static void npu_dma_stats_free(const struct npu_dma_handle *dma_handle)
{
	unsigned long live_count;
	unsigned long live_bytes;
	unsigned long peak_bytes;

	pthread_once(&npu_dma_stats_once, npu_dma_stats_init);
	pthread_spin_lock(&npu_dma_stats_lock);
	if (npu_dma_live_count > 0)
		npu_dma_live_count--;
	if (npu_dma_live_bytes >= dma_handle->size)
		npu_dma_live_bytes -= dma_handle->size;
	else
		npu_dma_live_bytes = 0;
	live_count = npu_dma_live_count;
	live_bytes = npu_dma_live_bytes;
	peak_bytes = npu_dma_peak_bytes;
	pthread_spin_unlock(&npu_dma_stats_lock);
}

void *mem_allocate(size_t size,
		   uint64_t *dma_addr,
		   uint64_t *obj,
		   uint32_t flags,
		   uint64_t *handle)
{
	void *ret;
	struct npu_dma_handle *dma_handle;
	unsigned long iova;
	int map_ret;

	rknpu_dev = npu_get_device();
	assert(rknpu_dev);

	dma_handle = malloc(sizeof(*dma_handle));
	assert(dma_handle);

	ret = chcore_alloc_dma_mem(size,
				   &dma_handle->dma,
				   flags & RKNPU_MEM_CACHEABLE);
	if (!ret) {
		printf("[npu-dma] alloc failed req=0x%lx flags=0x%x "
		       "free_mem=0x%lx\n",
		       (unsigned long)size,
		       flags,
		       usys_get_free_mem_size());
	}
	assert(ret);

	dma_handle->size = dma_handle->dma.size;
	map_ret = rknpu_dma_map(rknpu_dev,
				dma_handle->dma.paddr,
				dma_handle->size,
				&iova);
	if (map_ret != 0) {
		printf("[npu-dma] iommu map failed ret=%d req=0x%lx size=0x%lx "
		       "vaddr=0x%lx paddr=0x%lx flags=0x%x free_mem=0x%lx\n",
		       map_ret,
		       (unsigned long)size,
		       dma_handle->size,
		       dma_handle->dma.vaddr,
		       dma_handle->dma.paddr,
		       flags,
		       usys_get_free_mem_size());
	}
	assert(map_ret == 0);

	dma_handle->iova = iova;
	npu_dma_stats_alloc(dma_handle, (unsigned long)size, flags);

	*dma_addr = dma_handle->iova;
	*obj = dma_handle->dma.vaddr;
	*handle = (uint64_t)(uintptr_t)dma_handle;
	return (void *)dma_handle->dma.vaddr;
}

void mem_destroy(void *addr, size_t len, uint64_t handle, uint64_t obj_addr)
{
	struct npu_dma_handle *dma_handle = (void *)(uintptr_t)handle;
	int unmap_ret;

	rknpu_dev = npu_get_device();
	assert(rknpu_dev);

	unused(addr);
	unused(len);
	unused(obj_addr);

	unmap_ret = rknpu_dma_unmap(rknpu_dev,
				    dma_handle->iova,
				    dma_handle->size);
	if (unmap_ret != 0) {
		printf("[npu-dma] iommu unmap failed ret=%d size=0x%lx "
		       "vaddr=0x%lx paddr=0x%lx iova=0x%lx free_mem=0x%lx\n",
		       unmap_ret,
		       dma_handle->size,
		       dma_handle->dma.vaddr,
		       dma_handle->dma.paddr,
		       dma_handle->iova,
		       usys_get_free_mem_size());
	}
	assert(unmap_ret == 0);
	npu_dma_stats_free(dma_handle);
	chcore_free_dma_mem(&dma_handle->dma);
	free(dma_handle);
}

static struct rknpu_device *npu_open(void)
{
	struct rknpu_device *rknpu_dev = NULL;
	int ret;

	ret = rknpu_init(&rknpu_dev);
	if (ret != 0) {
		printf("[npu] rknpu_init failed ret=%d\n", ret);
		return NULL;
	}

	return rknpu_dev;
}

int npu_reset(void)
{
	rknpu_dev = npu_get_device();
	assert(rknpu_dev);
	return rknpu_soft_reset(rknpu_dev);
}

int npu_submit(__u64 task_obj_addr, __u32 core_mask)
{
	struct rknpu_submit submit = {
		.flags = RKNPU_JOB_PC | RKNPU_JOB_BLOCK | RKNPU_JOB_PINGPONG,
		.timeout = RKNPU_DEFAULT_TIMEOUT_MS,
		.task_start = 0,
		.task_number = 1,
		.task_counter = 0,
		.priority = 0,
		.task_obj_addr = task_obj_addr,
		.regcfg_obj_addr = 0,
		.task_base_addr = 0,
		.user_data = 0,
		.core_mask = core_mask,
		.fence_fd = -1,
		.subcore_task = {{0, 1}, {0, 1}, {0, 1}, {0, 0}, {0, 0}},
	};

	rknpu_dev = npu_get_device();
	assert(rknpu_dev);
	return rknpu_submit(rknpu_dev, &submit);
}

int npu_submit_multi(__u64 task_obj_addr[], int task_num)
{
	struct rknpu_submit submit_base = {
		.flags = RKNPU_JOB_PC | RKNPU_JOB_BLOCK | RKNPU_JOB_PINGPONG,
		.timeout = RKNPU_DEFAULT_TIMEOUT_MS,
		.task_start = 0,
		.task_number = 1,
		.task_counter = 0,
		.priority = 0,
		.regcfg_obj_addr = 0,
		.task_base_addr = 0,
		.user_data = 0,
		.fence_fd = -1,
		.subcore_task = {{0, 1}, {0, 1}, {0, 1}, {0, 0}, {0, 0}},
	};
	struct rknpu_submit submits[task_num];
	struct rknpu_submit *submit_ptrs[task_num];

	assert(task_num > 0 && task_num <= RKNPU_PLATFORM_CORE_COUNT);
	rknpu_dev = npu_get_device();
	assert(rknpu_dev);

	for (int core_index = 0; core_index < task_num; core_index++) {
		memcpy(&submits[core_index], &submit_base, sizeof(submit_base));
		submits[core_index].core_mask = RKNPU_CORE_MASK(core_index);
		submits[core_index].task_obj_addr = task_obj_addr[core_index];
		submit_ptrs[core_index] = &submits[core_index];
	}

	return rknpu_submit_multi(rknpu_dev, submit_ptrs, task_num);
}

void npu_close(void)
{
	struct rknpu_device *dev;
	int ret;

	pthread_mutex_lock(&rknpu_dev_lock);
	dev = rknpu_dev;
	rknpu_dev = NULL;
	pthread_mutex_unlock(&rknpu_dev_lock);

	if (dev == NULL)
		return;

	usys_tee_npu_secure_switch(false);
	ret = usys_tee_npu_ree_power_off();
	assert(ret == 0);
}
