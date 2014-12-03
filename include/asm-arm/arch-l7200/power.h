/*
 * linux/include/asm-arm/arch-l7200/power.h
 *
 * (C) Copyright 2001 Lineo Japan, Inc.
 *
 * May be copied or modified under the terms of the GNU General Public
 * License.  See linux/COPYING for more information.
 *
 * Based On:
 *  include/asm-arm/arch-sa1100/power.h
 *
 * ChangeLog:
 *   04-02-2001 Lineo Japan, Inc.
 */
#ifndef __ASM_ARCH_POWER_H
#define __ASM_ARCH_POWER_H

#define PMU_TRANSOP_MASK		(0x3 << 25)
#define PMU_TRANSOP(x)			(x << 25)
#define PMU_CLOCK_RECOVERY_MASK		(0x7f << 18)
#define PMU_CLOCK_RECOVERY(x)		(x << 18)
#define PMU_INTRET			(1 << 17)
#define PMU_OSCEN			(1 << 16)
#define PMU_OSCMUX			(1 << 15)
#define PMU_PLLMUL_MASK			(0x3f << 9)
#define PMU_PLLMUL(x)			(x << 9)
#define PMU_PLLEN			(1 << 8)
#define PMU_PLLMUX			(1 << 7)
#define PMU_SDR_STOP			(1 << 6)
#define PMU_SYSCLKEN			(1 << 5)
#define PMU_BCLK_DIV_MASK		(0x3 << 3)
#define PMU_BCLK_DIV(x)			(x << 3)
#define PMU_SDRB_SEL			(1 << 2)
#define PMU_SDRF_SEL			(1 << 1)
#define PMU_nFASTBUS			(1 << 0)

#if 0
#define SLEEP_PARAM_USER_R0		0
#define SLEEP_PARAM_USER_R1		1
#define SLEEP_PARAM_USER_R2		2
#define SLEEP_PARAM_USER_R3		3
#define SLEEP_PARAM_USER_R4		4
#define SLEEP_PARAM_USER_R5		5
#define SLEEP_PARAM_USER_R6		6
#define SLEEP_PARAM_USER_R7		7
#define SLEEP_PARAM_USER_R8		8
#define SLEEP_PARAM_USER_R9		9
#define SLEEP_PARAM_USER_R10		10		
#define SLEEP_PARAM_USER_R11		11
#define SLEEP_PARAM_USER_R12		12
#define SLEEP_PARAM_USER_R13		13
#define SLEEP_PARAM_USER_R14		14

#define SLEEP_PARAM_PC			15

#define SLEEP_PARAM_SVC_R13		16
#define SLEEP_PARAM_SVC_R14		17

#define SLEEP_PARAM_ABORT_R13		18
#define SLEEP_PARAM_ABORT_R14		19

#define SLEEP_PARAM_UNDEF_R13		20
#define SLEEP_PARAM_UNDEF_R14		21

#define SLEEP_PARAM_IRQ_R13		22
#define SLEEP_PARAM_IRQ_R14		23

#define SLEEP_PARAM_FIQ_R8		24
#define SLEEP_PARAM_FIQ_R9		25
#define SLEEP_PARAM_FIQ_R10		26
#define SLEEP_PARAM_FIQ_R11		27
#define SLEEP_PARAM_FIQ_R12		28
#define SLEEP_PARAM_FIQ_R13		29
#define SLEEP_PARAM_FIQ_R14		30

#define SLEEP_PARAM_CPSR		31
#define SLEEP_PARAM_SVC_SPSR		32
#define SLEEP_PARAM_ABORT_SPSR		33
#define SLEEP_PARAM_UNDEF_SPSR		34
#define SLEEP_PARAM_IRQ_SPSR		35
#define SLEEP_PARAM_FIQ_SPSR		36

#define SLEEP_PARAM_CP15_R1		37
#define SLEEP_PARAM_CP15_R2		38
#define SLEEP_PARAM_CP15_R3		39
#define SLEEP_PARAM_CP15_R5		40
#define SLEEP_PARAM_CP15_R6		41
#define SLEEP_PARAM_CP15_R13		42

#define SLEEP_PARAM_SIZE		(SLEEP_PARAM_CP15_R13 + 1)
#endif

#ifndef __ASSEMBLY__

int l7200_suspend(void);
int l7200_idle(void);

#endif	// __ASSEMBLY__

#endif
