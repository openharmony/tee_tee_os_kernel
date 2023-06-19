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
#include "boot.h"
#include <common/asm.h>

BEGIN_FUNC(_start)
        /* Set stack pointer */
        adr     x0, boot_stacks
        add     x0, x0, #BOOT_STACK_SIZE
        mov     sp, x0

        /* Get the physical address of _start */
        adr     x0, _start
        
        b       start_c
END_FUNC(_start)