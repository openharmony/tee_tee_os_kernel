/*
 * Copyright (c) 2026 Institute of Parallel And Distributed Systems (IPADS), Shanghai Jiao Tong University (SJTU)
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
#include <arch/machine/registers.h>
#include <common/asm.h>

	.macro set_sctlr_el1
		mrs	x0, sctlr_el1
		orr	x0, x0, #SCTLR_EL1_I
		orr	x0, x0, #SCTLR_EL1_SA
		orr	x0, x0, #SCTLR_EL2_SPAN
		orr	x0, x0, #SCTLR_EL2_WXN
		bic	x0, x0, #SCTLR_EL2_A
		msr	sctlr_el1, x0
	.endm

	/*
	 * Setup SP_EL0 and SPEL1, SP will be set to SP_EL0.
	 * SP_EL0 is assigned:
	 *   stack_tmp + (cpu_id + 1) * stack_tmp_stride - STACK_TMP_GUARD
	 * SP_EL1 is assigned thread_core_local[cpu_id]
	 */
	.macro set_sp
		bl	__get_core_pos
		add	x0, x0, #1
		mov	x1, #BOOT_STACK_SIZE
		mul	x1, x0, x1
		adr	x0, boot_stacks
		add	x0, x0, x1
		msr	spsel, #1
		mov	sp, x0
		# msr	spsel, #0
	.endm

BEGIN_FUNC(__get_core_pos)
	mrs	x0, mpidr_el1
	b	get_core_pos_mpidr
END_FUNC(__get_core_pos)

/* size_t get_core_pos_mpidr(uint32_t mpidr); */
/* Let platforms override this if needed */
BEGIN_FUNC(get_core_pos_mpidr)
	/* RK3588 encodes logical CPUs as MPIDR values 0x0, 0x100, ..., 0x700. */
	and	x1, x0, #0xff
	ubfx	x0, x0, #8, #8
	add	x0, x0, x1

	ret
END_FUNC(get_core_pos_mpidr)

BEGIN_FUNC(cpu_on_handler)
	mov	x19, x30

	/* Setup SP_EL0 and SP_EL1, SP will be set to SP_EL0 */
	set_sp

	bl	el1_mmu_activate
	isb

	/* Get current cpu id and store it in x0 */
	bl	__get_core_pos

/***************** Begin return *****************/
	mov	x30, x19
	b	secondary_cpu_boot
/****************** End return ******************/
END_FUNC(cpu_on_handler)
