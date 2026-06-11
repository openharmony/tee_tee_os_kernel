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
#if defined(CHCORE_LLM)

#include "rknpu-driver.h"

#include <assert.h>
#include <chcore/syscall.h>
#include <errno.h>
#include <stdint.h>
#include <string.h>

#define RKNPU_MAX_CORES 3U
#define RKNPU_CORE_MASK(core_index) (1U << (unsigned int)(core_index))
#define RKNPU_DEFAULT_PREEMPT_INTERVAL_US 500U

#define RKNPU_IOMMU_PROT_READ  (1U << 0)
#define RKNPU_IOMMU_PROT_WRITE (1U << 1)
#define RKNPU_IOMMU_PROT_RW \
	(RKNPU_IOMMU_PROT_READ | RKNPU_IOMMU_PROT_WRITE)

struct rknpu_device {
	int initialized;
};

static struct rknpu_device rknpu_dev;

static inline unsigned long long rknpu_read_cntvct(void)
{
	unsigned long long value;

	__asm__ volatile("mrs %0, cntvct_el0" : "=r"(value));
	return value;
}

static inline unsigned long long rknpu_read_cntfrq(void)
{
	unsigned long long value;

	__asm__ volatile("mrs %0, cntfrq_el0" : "=r"(value));
	return value;
}

static unsigned long long rknpu_cntvct_elapsed_us(unsigned long long start)
{
	unsigned long long freq = rknpu_read_cntfrq();

	if (freq == 0)
		return 0;

	return ((rknpu_read_cntvct() - start) * 1000000ULL) / freq;
}

static unsigned long long rknpu_cntvct_elapsed_ms(unsigned long long start)
{
	return rknpu_cntvct_elapsed_us(start) / 1000ULL;
}

static int rknpu_wait_complete(unsigned int timeout_ms)
{
	unsigned int timeout =
		timeout_ms != 0 ? timeout_ms : RKNPU_DEFAULT_TIMEOUT_MS;
	unsigned long long start = rknpu_read_cntvct();
	unsigned long long last_preempt = start;

	for (;;) {
		int ret = usys_tee_npu_poll_complete();

		if (ret == 0)
			return 0;
		if (ret != -EAGAIN) {
			(void)usys_tee_npu_submit_cancel();
			return ret;
		}
		if (rknpu_cntvct_elapsed_ms(start) > timeout) {
			(void)usys_tee_npu_submit_cancel();
			return -1;
		}

		usys_yield();
	}
}

static unsigned int rknpu_submit_timeout(struct rknpu_submit submits[],
					 int task_num)
{
	unsigned int timeout = RKNPU_DEFAULT_TIMEOUT_MS;

	for (int i = 0; i < task_num; i++) {
		unsigned int submit_timeout =
			submits[i].timeout != 0 ?
			submits[i].timeout :
			RKNPU_DEFAULT_TIMEOUT_MS;

		if (submit_timeout > timeout)
			timeout = submit_timeout;
	}

	return timeout;
}

int rknpu_init(struct rknpu_device **ret_rknpu_dev)
{
	int ret;

	assert(ret_rknpu_dev != NULL);

	ret = usys_tee_npu_ree_power_on();
	if (ret != 0)
		return ret;

	usys_tee_npu_secure_switch(true);

	ret = usys_tee_npu_iommu_init();
	if (ret != 0)
		return ret;

	rknpu_dev.initialized = 1;
	*ret_rknpu_dev = &rknpu_dev;
	return 0;
}

int rknpu_dma_map(struct rknpu_device *dev,
		  unsigned long paddr,
		  unsigned long size,
		  unsigned long *dma_addr)
{
	if (dev == NULL || dma_addr == NULL)
		return -1;

	return usys_tee_npu_iommu_map(paddr,
				      size,
				      RKNPU_IOMMU_PROT_RW,
				      dma_addr);
}

int rknpu_dma_unmap(struct rknpu_device *dev,
		    unsigned long dma_addr,
		    unsigned long size)
{
	if (dev == NULL)
		return -1;

	return usys_tee_npu_iommu_unmap(dma_addr, size);
}

int rknpu_submit(struct rknpu_device *dev, struct rknpu_submit *submit)
{
	int ret;

	if (dev == NULL || submit == NULL)
		return -1;

	ret = usys_tee_npu_submit_start((unsigned long)submit, 1);
	if (ret != 0)
		return ret;

	return rknpu_wait_complete(submit->timeout);
}

int rknpu_submit_multi(struct rknpu_device *dev,
		       struct rknpu_submit *args[],
		       int task_num)
{
	struct rknpu_submit submits[RKNPU_MAX_CORES];
	unsigned int timeout;
	int ret;

	if (dev == NULL || args == NULL)
		return -1;
	if (task_num <= 0 || task_num > RKNPU_MAX_CORES)
		return -1;

	for (int core_index = 0; core_index < task_num; core_index++) {
		if (args[core_index] == NULL)
			return -1;

		memcpy(&submits[core_index],
		       args[core_index],
		       sizeof(submits[core_index]));
		submits[core_index].core_mask = RKNPU_CORE_MASK(core_index);
	}

	timeout = rknpu_submit_timeout(submits, task_num);
	ret = usys_tee_npu_submit_start((unsigned long)submits, task_num);
	if (ret != 0)
		return ret;

	return rknpu_wait_complete(timeout);
}

int rknpu_soft_reset(struct rknpu_device *dev)
{
	if (dev == NULL)
		return -1;

	return 0;
}

#endif /* CHCORE_LLM */
