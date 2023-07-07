// SPDX-License-Identifier:     GPL-2.0+
/*
 * (C) Copyright 2018 Rockchip Electronics Co., Ltd.
 *
 */

#include <common/kprint.h>
#include <rk_atags.h>
#include <common/errno.h>
#include <common/util.h>

#define HASH_LEN sizeof(u32)

static u32 js_hash(void *buf, u32 len)
{
    u32 i, hash = 0x47C6A7E6;
    char *data = buf;

    if (!buf || !len)
        return hash;

    for (i = 0; i < len; i++)
        hash ^= ((hash << 5) + data[i] + (hash >> 2));

    return hash;
}

int atags_bad_magic(u32 magic)
{
    bool bad;

    bad = ((magic != ATAG_CORE) && (magic != ATAG_NONE)
           && (magic < ATAG_SERIAL || magic > ATAG_MAX));
    if (bad) {
        printk("Magic(%x) is not support\n", magic);
    }

    return bad;
}

static int inline atags_size_overflow(struct tag *t, u32 tag_size)
{
    return (unsigned long)t + (tag_size << 2) - ATAGS_VIRT_BASE > ATAGS_SIZE;
}

int atags_overflow(struct tag *t)
{
    bool overflow;

    overflow = atags_size_overflow(t, 0) || atags_size_overflow(t, t->hdr.size);
    if (overflow) {
        printk("Tag is overflow\n");
    }

    return overflow;
}

int atags_is_available(void)
{
    struct tag *t = (struct tag *)ATAGS_VIRT_BASE;

    return (t->hdr.magic == ATAG_CORE);
}

int atags_set_tag(u32 magic, void *tagdata)
{
    u32 length, size = 0, hash;
    struct tag *t = (struct tag *)ATAGS_VIRT_BASE;

    if (!atags_is_available())
        return -EPERM;

    if (!tagdata)
        return -ENODATA;

    if (atags_bad_magic(magic))
        return -EINVAL;

    /* Not allowed to be set by user directly, so do nothing */
    if ((magic == ATAG_CORE) || (magic == ATAG_NONE))
        return -EPERM;

    /* If not initialized, setup now! */
    if (t->hdr.magic != ATAG_CORE) {
        t->hdr.magic = ATAG_CORE;
        t->hdr.size = tag_size(tag_core);
        t->u.core.flags = 0;
        t->u.core.pagesize = 0;
        t->u.core.rootdev = 0;

        t = tag_next(t);
    } else {
        /* Find the end, and use it as a new tag */
        for_each_tag(t, (struct tag *)ATAGS_VIRT_BASE)
        {
            if (atags_overflow(t))
                return -EINVAL;

            if (atags_bad_magic(t->hdr.magic))
                return -EINVAL;

            /* This is an old tag, override it */
            if (t->hdr.magic == magic)
                break;

            if (t->hdr.magic == ATAG_NONE)
                break;
        }
    }

    /* Initialize new tag */
    switch (magic) {
    case ATAG_SERIAL:
        size = tag_size(tag_serial);
        break;
    case ATAG_BOOTDEV:
        size = tag_size(tag_bootdev);
        break;
    case ATAG_TOS_MEM:
        size = tag_size(tag_tos_mem);
        break;
    case ATAG_DDR_MEM:
        size = tag_size(tag_ddr_mem);
        break;
    case ATAG_RAM_PARTITION:
        size = tag_size(tag_ram_partition);
        break;
    case ATAG_ATF_MEM:
        size = tag_size(tag_atf_mem);
        break;
    case ATAG_PUB_KEY:
        size = tag_size(tag_pub_key);
        break;
    case ATAG_SOC_INFO:
        size = tag_size(tag_soc_info);
        break;
    case ATAG_BOOT1_PARAM:
        size = tag_size(tag_boot1p);
        break;
    };

    if (!size)
        return -EINVAL;

    if (atags_size_overflow(t, size))
        return -ENOMEM;

    /* It's okay to setup a new tag */
    t->hdr.magic = magic;
    t->hdr.size = size;
    length = (t->hdr.size << 2) - sizeof(struct tag_header) - HASH_LEN;
    memcpy(&t->u, (char *)tagdata, length);
    hash = js_hash(t, (size << 2) - HASH_LEN);
    memcpy((char *)&t->u + length, &hash, HASH_LEN);

    /* Next tag */
    t = tag_next(t);

    /* Setup done */
    t->hdr.magic = ATAG_NONE;
    t->hdr.size = 0;

    return 0;
}

struct tag *atags_get_tag(u32 magic)
{
    u32 *hash, calc_hash, size;
    struct tag *t;

    if (!atags_is_available())
        return NULL;

    for_each_tag(t, (struct tag *)ATAGS_VIRT_BASE)
    {
        if (atags_overflow(t))
            return NULL;

        if (atags_bad_magic(t->hdr.magic))
            return NULL;

        if (t->hdr.magic != magic)
            continue;

        size = t->hdr.size;
        hash = (u32 *)((unsigned long)t + (size << 2) - HASH_LEN);
        if (!*hash) {
            printk("No hash, magic(%x)\n", magic);
            return t;
        } else {
            calc_hash = js_hash(t, (size << 2) - HASH_LEN);
            if (calc_hash == *hash) {
                printk("Hash okay, magic(%x)\n", magic);
                return t;
            } else {
                printk("Hash bad, magic(%x), orgHash=%x, nowHash=%x\n",
                       magic,
                       *hash,
                       calc_hash);
                return NULL;
            }
        }
    }

    return NULL;
}

void atags_destroy(void)
{
    if (atags_is_available())
        memset((char *)ATAGS_VIRT_BASE, 0, sizeof(struct tag));
}
