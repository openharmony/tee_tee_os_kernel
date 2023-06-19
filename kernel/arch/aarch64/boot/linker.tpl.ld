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
#include "image.h"

ENTRY(_start)

SECTIONS
{
    . = TEXT_OFFSET;

    img_start = .;
    init : {
        ${init_objects}
    }
    . = ALIGN(4K);
    init_end = ABSOLUTE(.);

    .text KERNEL_VADDR + init_end : AT(init_end) {
    _text_start = .;
        *(.text*)
    }
    . = ALIGN(4K);
    _text_end = .;

    .data : {
        *(.data*)
    }
    . = ALIGN(4K);

    .rodata : {
        *(.rodata*)
    }
    . = ALIGN(4K);
    _edata = . - KERNEL_VADDR;

    _bss_start = . - KERNEL_VADDR;
    .bss : {
        *(.bss*)
    }
    . = ALIGN(4K);
    _bss_end = . - KERNEL_VADDR;

    _extable_start = .;
    .extable : {
        *(.extable*)
    }
    . = ALIGN(4K);
    _extable_end = .;
    
    img_end = . - KERNEL_VADDR;

    /DISCARD/ : { *(.comment) *(.gnu*) *(.note*) *(.eh_frame*) }
}
