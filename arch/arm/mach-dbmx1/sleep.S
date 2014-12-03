/*
 * linux/arch/arm/mach-dbmx1/sleep.S
 *
 * Power Management Routines
 *
 * (C) Copyright 2003 Lineo Japan, Inc.
 *
 * May be copied or modified under the terms of the GNU General Public
 * License.  See linux/COPYING for more information.
 *
 */

#include <linux/linkage.h>
#include <asm/assembler.h>
#include <asm/hardware.h>


		.text

/*
 * dbmx1_cpu_suspend()
 *
 * Causes DBMX1 to enter stop mode
 *
 */

ENTRY(dbmx1_cpu_suspend)

	stmfd	sp!, {r4 - r12, lr}		@ save registers on stack

	ldr	r1, =VA_PLL
	ldr	r2, [r1]
	bic	r2, r2, #0x03
	str	r2, [r1]
	mcr	p15, 0, r1, c7, c0, 4
	nop
	nop
	ldmfd	sp!, {r4 - r12, pc}		@ return to caller


/*
 * dbmx1_cpu_standby()
 *
 * Causes DBMX1 to enter doze mode
 *
 */

ENTRY(dbmx1_cpu_standby)
	stmfd	sp!, {r4 - r12, lr}		@ save registers on stack

	ldr	r1, =VA_PLL
	ldr	r2, [r1]
	bic	r2, r2, #0x01
	str	r2, [r1]
	mcr	p15, 0, r1, c7, c0, 4
	nop
	nop
	ldmfd	sp!, {r4 - r12, pc}		@ return to caller
