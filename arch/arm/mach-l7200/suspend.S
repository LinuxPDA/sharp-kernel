/*
 * LinkUp 7200 Assembler Power Management Routines
 *
 * (C) Copyright 2001 Lineo Japan, Inc.
 *
 * Based on:
 *   arch/arm/mach-sa1100/suspend.S	
 *   Copyright (c) 2001 Cliff Brake <cbrake@accelent.com>
 *
 *   This program is free software; you can redistribute it and/or 
 *   modify it under the terms of the GNU General Public License.
 *
 * ChangeLog:
 *   04-02-2001 Lineo Japan, Inc.
 */
#include <linux/linkage.h>
#include <asm/assembler.h>
#include <asm/hardware.h>
#include <asm/arch/power.h>

	.data
	
	.global	sleep_param
	.global	sleep_param_p

sleep_param:	.word	0		@ virtual address of parameter array
sleep_param_p:	.word	0		@ physical address of parameter array

/* 
 * cpu_l7200_do_suspend()
 *	
 */
	.text
	.align 5
ENTRY(cpu_l7200_do_suspend)

/*
 * cpu_l7200_resume()
 *
 * entry point from bootloader into kernel during resume
 * 
 */
	.align 5
ENTRY(cpu_l7200_resume)	
