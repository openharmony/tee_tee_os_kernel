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

#include <hm_cache_flush.h>
#include <chcore/syscall.h>

void dma_flush_range(uint64_t start, uint64_t end)
{
    usys_cache_flush(start, end - start, CACHE_CLEAN);
}

void dma_inv_range(uint64_t start, uint64_t end)
{
    usys_cache_flush(start, end - start, CACHE_INVALIDATE);
}

void dma_map_area(uint64_t start, uint64_t size, int32_t dir)
{
    if (dir == DMA_FROM_DEVICE) {
        dma_inv_range(start, start + size);
    } else {
        dma_flush_range(start, start + size);
    }
}

void dma_unmap_area(uint64_t start, uint64_t size, int32_t dir)
{
    if (dir == DMA_FROM_DEVICE) {
        dma_inv_range(start, start + size);
    }
}

void dma_clean_range(uint64_t start, uint64_t end)
{
    usys_cache_flush(start, end - start, CACHE_CLEAN_AND_INV);
}
