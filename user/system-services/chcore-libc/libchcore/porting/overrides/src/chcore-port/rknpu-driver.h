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
#ifndef NPU_DRIVER_H
#define NPU_DRIVER_H

#include "rknpu-ioctl.h"

#define RKNPU_DEFAULT_TIMEOUT_MS 6000

struct rknpu_device;

int rknpu_init(struct rknpu_device **rknpu_dev);

int rknpu_submit(struct rknpu_device *rknpu_dev, struct rknpu_submit *data);
int rknpu_submit_multi(struct rknpu_device *rknpu_dev,
		       struct rknpu_submit *args[],
		       int task_num);

int rknpu_dma_map(struct rknpu_device *rknpu_dev,
		  unsigned long paddr,
		  unsigned long size,
		  unsigned long *dma_addr);
int rknpu_dma_unmap(struct rknpu_device *rknpu_dev,
		    unsigned long dma_addr,
		    unsigned long size);

int rknpu_soft_reset(struct rknpu_device *rknpu_dev);

#endif /* NPU_DRIVER_H */
