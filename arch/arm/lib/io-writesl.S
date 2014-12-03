/*
 *  linux/arch/arm/lib/io-writesl.S
 *
 *  Copyright (C) 1995-2000 Russell King
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */
#include <linux/linkage.h>
#include <asm/assembler.h>

ENTRY(__raw_writesl)
		teq	r2, #0		@ do we have to check for the zero len?
		moveq	pc, lr
		ands	ip, r1, #3
		bne	2f

1:		ldr	r3, [r1], #4
		str	r3, [r0]
		subs	r2, r2, #1
		bne	1b
		mov	pc, lr

2:		bic	r1, r1, #3
		cmp	ip, #2
		ldr	ip, [r1], #4
		bgt	4f
		blt	6f

		mov	ip, ip, pull #16
3:		ldr	r3, [r1], #4
		orr	ip, ip, r3, push #16
		str	ip, [r0]
		mov	ip, r3, pull #16
		subs	r2, r2, #1
		bne	3b
		mov	pc, lr

4:		mov	ip, ip, pull #24
5:		ldr	r3, [r1], #4
		orr	ip, ip, r3, push #8
		str	ip, [r0]
		mov	ip, r3, pull #24
		subs	r2, r2, #1
		bne	5b
		mov	pc, lr

6:		mov	ip, ip, pull #8
7:		ldr	r3, [r1], #4
		orr	ip, ip, r3, push #24
		str	ip, [r0]
		mov	ip, r3, pull #8
		subs	r2, r2, #1
		bne	7b
		mov	pc, lr


