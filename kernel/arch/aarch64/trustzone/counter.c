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
#include <common/errno.h>
#include <mm/mm.h>
#include <mm/uaccess.h>

unsigned int sync_seconds = 0;
unsigned int sync_mills = 0;
unsigned int g_boot_time_seconds = 0;
unsigned int g_boot_time_mills = 0;

static void get_boot_time(void) {
    long long cur_count;
    unsigned int freq;
    long long timestamp;
    unsigned int sec;
    unsigned int nsec;

    __asm__ volatile("mrs %0, cntfrq_el0" : "=r"(freq));
    __asm__ volatile("mrs %0, cntpct_el0" : "=r"(cur_count));

    if (freq == 0) {
        return;
    }

    g_boot_time_seconds = cur_count / freq;
    nsec = (cur_count % freq) * 1000000000 / freq;
    g_boot_time_mills = nsec / 1000000;
}

int sys_teecall_cap_time_sync(unsigned int seconds, unsigned int mills)
{
    sync_seconds = seconds;
    sync_mills = mills;
    get_boot_time();
    return 0;
}

int sys_timer_get_offset(long seconds, long mills)
{
    int local_seconds;
    int local_mills;

    if (sync_mills > g_boot_time_mills) {
        local_seconds = sync_seconds - g_boot_time_seconds;
        local_mills = sync_mills - g_boot_time_mills;
    } else {
        local_seconds = sync_seconds - g_boot_time_seconds - 1;
        local_mills = 1000 + sync_mills - g_boot_time_mills;
    }

    if (check_user_addr_range((vaddr_t)seconds, sizeof(seconds))) {
        return -EINVAL;
    }
    if (check_user_addr_range((vaddr_t)mills, sizeof(mills))) {
        return -EINVAL;
    }
    copy_to_user((int *)seconds, &local_seconds, sizeof(int));
    copy_to_user((int *)mills, &local_mills, sizeof(int));
    return 0;
}