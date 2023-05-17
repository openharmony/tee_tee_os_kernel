/*
 * Copyright (c) 2023 Institute of Parallel And Distributed Systems (IPADS)
 * Licensed under the Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 *     http://license.coscl.org.cn/MulanPSL2
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY OR FIT FOR A PARTICULAR
 * PURPOSE.
 * See the Mulan PSL v2 for more details.
 */
#ifndef THREAD_ENV_H
#define THREAD_ENV_H

struct process_metadata {
    unsigned long phdr_addr;
    unsigned long phentsize;
    unsigned long phnum;
    unsigned long flags;
    unsigned long entry;
};

#define ENV_SIZE_ON_STACK   0x1000
#define ROOT_BIN_HDR_SIZE   48
#define ROOT_MEM_SIZE_OFF   0
#define ROOT_ENTRY_OFF      8
#define ROOT_FLAGS_OFF      16
#define ROOT_PHENT_SIZE_OFF 24
#define ROOT_PHNUM_OFF      32
#define ROOT_PHDR_ADDR_OFF  40

#endif /* THREAD_ENV_H */
