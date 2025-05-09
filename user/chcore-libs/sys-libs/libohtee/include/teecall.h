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
#ifndef SYS_TEECALL_H
#define SYS_TEECALL_H

#include <chcore/type.h>
#include <stdbool.h>

typedef struct {
    unsigned long params_stack[8];
} __attribute__((packed)) kernel_shared_varibles_t;

int32_t tee_pull_kernel_variables(const kernel_shared_varibles_t *pVar);

void tee_push_rdr_update_addr(uint64_t addr, uint32_t size, bool is_cache_mem,
                              const char *chip_type_buff, uint32_t buff_len);
int debug_rdr_logitem(char *str, size_t str_len);

int32_t teecall_cap_time_sync(uint32_t seconds, uint32_t mills);

#endif
