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

#include <drv_hwi_share.h>
#include <errno.h>
#include <malloc.h>
#include <chcore/syscall.h>
#include <pthread.h>

#define MAX_IRQ_NUM 512

#define IRQ_THREAD_SIZE (64 * 1024)

enum gic_op_t {
	GICV3_OP_SET_PRIO = 1,
	GICV3_OP_SET_GROUP,
	GICV3_OP_SET_TARGET,
	GICV3_OP_SET_PENDING,
	GICV3_OP_MASK,
};

struct hwi_cb {
    uint32_t hwi_num;
    uint16_t hwi_prio;
    uint16_t mode;
    HWI_PROC_FUNC handler;
    uint32_t args;
    cap_t irq;
    pthread_t tid;
};

static struct hwi_cb *hwi_cbs[MAX_IRQ_NUM];

static cap_t irq_notif;

static void *__irq_thread(void *arg) {
    struct hwi_cb *hwi = (struct hwi_cb *)arg;
    uint32_t hwi_num = hwi->hwi_num;
    int ret;

    usys_set_prio(0, 55);

    while (true) {
        ret = usys_irq_wait(hwi->irq, true);
        if (ret) {
            break;
        }
        hwi->handler(hwi->args);
    }
    printf("%s %d finish loop\n", __func__, __LINE__);

    free(hwi_cbs[hwi_num]);
    hwi_cbs[hwi_num] = NULL;

    return NULL;
}

uint32_t sys_hwi_create(uint32_t hwi_num, uint16_t hwi_prio, uint16_t mode,
                        HWI_PROC_FUNC handler, uint32_t args)
{
    struct hwi_cb *hwi_cb;
    int ret;
    cap_t irq;
    pthread_attr_t attr;

    if (hwi_num >= MAX_IRQ_NUM) {
        ret = -EINVAL;
        goto out;
    }

    hwi_cb = malloc(sizeof(struct hwi_cb));
    if (hwi_cb == NULL) {
        ret = -ENOMEM;
        goto out;
    }

    irq = usys_irq_register(hwi_num);
    if (irq < 0) {
        ret = irq;
        goto out_free;
    }

    ret = usys_irq_op(hwi_num, GICV3_OP_MASK, 1);
    if (ret) {
        goto out_set_ns;
    }
    ret = usys_irq_op(hwi_num, GICV3_OP_SET_PENDING, 0);
    if (ret) {
        goto out_set_ns;
    }
    ret = usys_irq_op(hwi_num, GICV3_OP_SET_GROUP, 0);
    if (ret) {
        goto out_set_ns;
    }
    ret = usys_irq_op(hwi_num, GICV3_OP_SET_TARGET, 0);
    if (ret) {
        goto out_set_ns;
    }
    ret = usys_irq_op(hwi_num, GICV3_OP_SET_PRIO, hwi_prio);
    if (ret) {
        goto out_set_ns;
    }

    hwi_cb->hwi_num = hwi_num;
    hwi_cb->hwi_prio = hwi_prio;
    hwi_cb->mode = mode;
    hwi_cb->handler = handler;
    hwi_cb->args = args;
    hwi_cb->irq = irq;
    hwi_cbs[hwi_num] = hwi_cb;

    pthread_attr_setstacksize(&attr, IRQ_THREAD_SIZE);
    ret = pthread_create(&hwi_cb->tid, NULL, __irq_thread, hwi_cb);
    if (ret) {
        goto out_unset_hwi_cbs;
    }

    return 0;

out_unset_hwi_cbs:
    hwi_cbs[hwi_num] = NULL;
out_set_ns:
    (void)usys_irq_op(hwi_num, GICV3_OP_SET_GROUP, 1);
    (void)usys_revoke_cap(irq, false);
out_free:
    free(hwi_cb);
out:
    return ret;
}

uint32_t sys_hwi_resume(uint32_t hwi_num, uint16_t hwi_prio, uint16_t mode)
{
    (void)hwi_num;
    (void)hwi_prio;
    (void)mode;
    return 0;
}

uint32_t sys_hwi_delete(uint32_t hwi_num)
{
    int ret = 0;

    if (hwi_cbs[hwi_num] == NULL) {
        ret = -EINVAL;
        goto out;
    }

    (void)usys_irq_stop(hwi_cbs[hwi_num]->irq);
    (void)usys_irq_op(hwi_num, GICV3_OP_SET_GROUP, 1);

out:
    return ret;
}

uint32_t sys_hwi_disable(uint32_t hwi_num)
{
    return usys_irq_op(hwi_num, GICV3_OP_MASK, 1);
}

uint32_t sys_hwi_enable(uint32_t hwi_num)
{
    return usys_irq_op(hwi_num, GICV3_OP_MASK, 0);
}

int32_t sys_hwi_notify(uint32_t hwi_num)
{
    return usys_irq_op(hwi_num, GICV3_OP_SET_PENDING, 1);
}
