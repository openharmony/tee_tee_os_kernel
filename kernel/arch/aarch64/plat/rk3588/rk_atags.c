/*
 * Copyright (c) 2023 Institute of Parallel And Distributed Systems (IPADS), Shanghai Jiao Tong University (SJTU)
 * Licensed under the Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 *     http://license.coscl.org.cn/MulanPSL2
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY
 * KIND, EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO
 * NON-INFRINGEMENT, MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE. See the
 * Mulan PSL v2 for more details.
 */
#include <common/kprint.h>
#include <rk_atags.h>
#include <common/errno.h>
#include <common/util.h>

#define HASH_LEN sizeof(u32)

static u32 get_hash(void *buf, u32 len)
{
    u32 i, hash = 0x47C6A7E6;
    char *data = buf;

    if (!buf || !len)
        return hash;

    for (i = 0; i < len; i++)
        hash ^= ((hash << 5) + data[i] + (hash >> 2));

    return hash;
}

static bool tag_range_valid(const struct tag *tag, u32 size, u32 tail_size)
{
    unsigned long tag_addr = (unsigned long)tag;
    u64 tag_bytes = (u64)size * sizeof(u32);
    u64 available;

    if (tag_addr < ATAGS_VIRT_BASE || tag_addr > ATAGS_VIRT_END) {
        return false;
    }

    available = (u64)(ATAGS_VIRT_END - tag_addr);
    if (tag_bytes > available || (u64)tail_size > available - tag_bytes) {
        return false;
    }

    return true;
}

int set_tos_mem_tag(void *tagdata)
{
    u32 length, size = 0, hash;
    struct tag *t = (struct tag *)ATAGS_VIRT_BASE;

    if (t->hdr.magic != MAGIC_CORE)
        return -EPERM;

    if (!tagdata)
        return -ENODATA;

    for_each_tag(t, (struct tag *)ATAGS_VIRT_BASE) {
        if (t->hdr.size < sizeof(struct tag_header) / sizeof(u32) ||
            !tag_range_valid(t, t->hdr.size, sizeof(struct tag_header))) {
            return -EINVAL;
        }

        if (((t->hdr.magic != MAGIC_CORE) && (t->hdr.magic != MAGIC_NONE)
             && (t->hdr.magic <= MAGIC_MIN || t->hdr.magic > MAGIC_MAX))) {
            return -EINVAL;
        }

        if (t->hdr.magic == MAGIC_TOS_MEM || t->hdr.magic == MAGIC_NONE) {
            break;
        }
    }

    size = tag_size(tag_tos_mem);

    if (!size)
        return -EINVAL;

    u64 tag_bytes = (u64)size * sizeof(u32);
    if (tag_bytes < sizeof(struct tag_header) + HASH_LEN) {
        return -EINVAL;
    }
    if (!tag_range_valid(t, size, sizeof(struct tag_header))) {
        return -ENOMEM;
    }

    t->hdr.magic = MAGIC_TOS_MEM;
    t->hdr.size = size;
    length = (u32)(tag_bytes - sizeof(struct tag_header) - HASH_LEN);
    memcpy(&t->u, (char *)tagdata, length);
    hash = get_hash(t, (u32)(tag_bytes - HASH_LEN));
    memcpy((char *)&t->u + length, &hash, HASH_LEN);

    t = tag_next(t);
    t->hdr.magic = MAGIC_NONE;
    t->hdr.size = 0;

    return 0;
}
