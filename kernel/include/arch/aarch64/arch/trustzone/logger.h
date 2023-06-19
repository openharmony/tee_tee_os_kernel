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
#ifndef ARCH_AARCH64_ARCH_TRUSTZONE_LOGGER_H
#define ARCH_AARCH64_ARCH_TRUSTZONE_LOGGER_H

#include <object/memory.h>

extern volatile bool using_logger;

void logger_init(paddr_t log_paddr);
void logger_send(int c);

#endif /* ARCH_AARCH64_ARCH_TRUSTZONE_LOGGER_H */
