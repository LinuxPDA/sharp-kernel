/*
 * XSCALE Assembler Power Management Routines
 *
 * Copyright (C) 2002 Steve Lin (stevelin168@hotmail.com)
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 * May be copied or modified under the terms of the GNU General Public
 * License.  See linux/COPYING for more information.
 *
 * Based on linux/arch/arm/mach-sa1100/suspend.S
 *
 */

#include <linux/linkage.h>
#include <asm/assembler.h>
#include <linux/autoconf.h>

#include "cotulla_param.h"

/* 
 * cpu_sabinal_do_suspend()
 * 
 * Causes sa11x0 to enter sleep state
 * 
 */

#ifdef CONFIG_XIP_ROM
	.bss
#else
	.text
#endif

	.global	sleep_param
	.global	sleep_param_p

sleep_param:	.word	0		@ virtual address of parameter array
sleep_param_p:	.word	0		@ physical address of parameter array

IC_BASE:        .word   0xf0D00000
	

RST_BASE:	.word	0xf0F00000	
#define RCSR	0x30

CMR_BASE:	.word	0xf1300000	
#define CCCR	0x00
#define OSCC	0x08

FFUART:		.word	0xf0100000
BTUART:		.word	0xf0200000
#define THR	0x00
#define IER	0x04
#define FCR	0x08
#define LCR	0x0C
#define MCR	0x10	
#define LSR	0x14
#define	MSR	0x18
#define DLL	0x00
#define DLH	0x04

GPLRx:		.word	0xf0E00000	@GPIO pin-level register GPIO<31:0>
GPLRy:		.word	0xf0E00004	@GPIO pin-level register GPIO<63:32>
GPLRz:		.word	0xf0E00008	@GPIO pin-level register GPIO<80:64>
GPDRx:		.word	0xf0E0000C	@GPIO pin direction register GPIO<31:0>
GPDRy:		.word	0xf0E00010	@GPIO pin direction register GPIO<63:32>
GPDRz:		.word	0xf0E00014	@GPIO pin direction register GPIO<80:64>
GPSRx:		.word	0xf0E00018	@GPIO pin output set register GPIO<31:0>
GPSRy:		.word	0xf0E0001C	@GPIO pin output set register GPIO<63:32>
GPSRz:		.word	0xf0E00020	@GPIO pin output set register GPIO<80:64>
GPCRx:		.word	0xf0E00024	@GPIO pin output clear register GPIO<31:0>
GPCRy:		.word	0xf0E00028	@GPIO pin output clear register GPIO <63:32>
GPCRz:		.word	0xf0E0002C	@GPIO pin output clear register GPIO <80:64>
GRERx:		.word	0xf0E00030	@GPIO rising-edge detect register GPIO<31:0>
GRERy:		.word	0xf0E00034	@GPIO rising-edge detect register GPIO<63:32>
GRERz:		.word	0xf0E00038	@GPIO rising-edge detect register GPIO<80:64>
GFERx:		.word	0xf0E0003C	@GPIO falling-edge detect register GPIO<31:0>
GFERy:		.word	0xf0E00040	@GPIO falling-edge detect register GPIO<63:32>
GFERz:		.word	0xf0E00044	@GPIO falling-edge detect register GPIO<80:64>
GEDRx:		.word	0xf0E00048	@GPIO edge detect status register GPIO<31:0>
GEDRy:		.word	0xf0E0004C	@GPIO edge detect status register GPIO<63:32>
GEDRz:		.word	0xf0E00050	@GPIO edge detect status register GPIO<80:64>
GAFR0x:		.word	0xf0E00054	@GPIO alternate function select register GPIO<15:0>
GAFR1x:		.word	0xf0E00058	@GPIO alternate function select register GPIO<31:16>
GAFR0y:		.word	0xf0E0005C	@GPIO alternate function select register GPIO<47:32>
GAFR1y:		.word	0xf0E00060	@GPIO alternate function select register GPIO<63:48>
GAFR0z:		.word	0xf0E00064	@GPIO alternate function select register GPIO<79:64>
GAFR1z:		.word	0xf0E00068@        //GPIO alternate function select register GPIO 80

MMU_CTL_MASK:				.word		0xFFFF0000	

MMU_TTB_MASK:				.word		0x00003FFF	

UVIR_PWR_BASE:	.word	0xf0F00000
#define PMCR				0x000	
#define PSSR				0x004	
#define PSPR				0x008	
#define PWER				0x00C	
#define PRER				0x010	
#define PFER				0x014	
#define PEDR				0x018	
#define PCFR				0x01C	
#define PGSRx				0x020	
#define PGSRy				0x024	
#define PGSRz				0x028	

UVIR_OST_BASE:				.word		0xf0a00000

UVIR_RTC_BASE:				.word		0xf0900000
#define	RCNR		0x000
#define	RTAR		0x004
#define	RTSR		0x008
#define	RTTR		0x00C
#define	RTC_AL		0x0001
#define	RTC_HZ		0x0010
#define	RTC_ALE		0x0100

UVIR_IC_BASE:        .word   0xf0D00000
#define ICIP				0x000
#define ICMR				0x004
#define ICLR				0x008
#define ICFP				0x00C
#define ICPR				0x010
#define ICCR_IC				0x014

#define Mode_USR            0x10
#define Mode_FIQ			0x11
#define Mode_IRQ            0x12
#define Mode_SVC            0x13
#define Mode_ABT            0x17
#define Mode_UND            0x1B
#define Mode_SYS	   		0x1F

PWER_VALUE: .word	0x80000003
PFER_VALUE: .word	0x80000002

#define DCACHE_CACHELINECOUNT			1024
#define MINIDCACHE_CACHELINECOUNT		64
#define CACHELINESIZE					32

UVIR_GPIO_BASE:	.word 0xf0e00000

R0VALUE:	.word	0xf6000000

LCD_BASE:	.word	0xf4000000
#define LCCR3	0x0c
LCCR3VALUE1:	.word	0x0730000A
LCCR3VALUE2:	.word	0x07300090


#ifdef CONFIG_XIP_ROM

	.align 5

mmu_switch:	.space	8

	.text
#endif

	
ENTRY(cpu_sabinal_do_suspend)
	@ save registers on stack

	stmfd	sp!, {r0, r1, r2, r3, r4, r5, r6, r7, r8, r9, r10, r11, r12, lr}

	ldr 	r0, UVIR_PWR_BASE

	mov		r1, #0
	str		r1, [r0, #PMCR]

	mov		r1, #3
	str		r1, [r0, #PEDR]

	mov		r1, #1
	str		r1, [r0, #PRER]
	
	mov		r1, #2	
	str		r1, [r0, #PFER]
	
	
	ldr		r1, PWER_VALUE
	str		r1, [r0, #PWER]
	
	mov		r1, #1
	str		r1, [r0, #PCFR]


	ldr     r0, GPCRx
	mvn     r1, #0
	str     r1, [r0]        @ turn off all GPIOs

	ldr     r0, GPCRy
	mvn     r1, #0
	str     r1, [r0]        @ turn off all GPIOs

	ldr     r0, GPCRz
	mvn     r1, #0
	str     r1, [r0]        @ turn off all GPIOs

	ldr		r3, sleep_param
	ldr     r2, =Awake_address              @ store Virtual return address
	str     r2, [r3], #4
	
	mrc     p15, 0, r2, c1, c0, 0           @ load r2 with MMU Control
	ldr     r0, MMU_CTL_MASK               @ mask off the undefined bits
	bic     r2, r2, r0
	str     r2, [r3], #4                    @ store MMU Control data

	mrc     p15, 0, r2, c2, c0, 0           @ load r2 with TTB address.
	ldr     r0, MMU_TTB_MASK               @ mask off the undefined bits
	bic     r2, r2, r0
	str     r2, [r3], #4                    @ store TTB address

	mrc     p15, 0, r2, c3, c0, 0           @ load r2 with domain access control.
	str     r2, [r3], #4                    @ store domain access control

	str     sp, [r3], #4                    @ store SVC stack pointer

	mrs     r2, spsr
	str     r2, [r3], #4                    @ store SVC status register

	mov     r1, #(Mode_FIQ | I_BIT | F_BIT)   @ Enter FIQ mode, no interrupts
	msr     cpsr, r1
	mrs     r2, spsr
	stmia   r3!, {r2, r8-r12, sp, lr}       @ store the FIQ mode registers

	mov     r1, #(Mode_ABT | I_BIT | F_BIT) @ Enter ABT mode, no interrupts
	msr     cpsr, r1
	mrs		r0, spsr
	stmia   r3!, {r0, sp, lr}               @ store the ABT mode Registers

	mov     r1, #(Mode_IRQ | I_BIT | F_BIT) @ Enter IRQ mode, no interrupts
	msr     cpsr, r1
	mrs     r0, spsr
	stmia   r3!, {r0, sp, lr}               @ store the IRQ Mode Registers

	mov     r1, #(Mode_UND | I_BIT | F_BIT) @ Enter UND mode, no interrupts
	msr     cpsr, r1
	mrs     r0, spsr
	stmia   r3!, {r0, sp, lr}               @ store the UND mode Registers

	mov     r1, #(Mode_SYS | I_BIT | F_BIT) @ Enter SYS mode, no interrupts
	msr     cpsr, r1
	stmia   r3!, {sp, lr}                   @ store the SYS mode Registers

	mov     r1, #(Mode_SVC | I_BIT | F_BIT) @ Back to SVC mode, no interrupts
	msr     cpsr, r1

	ldr     r5, GAFR0x
	ldr		r0, [r5]
	str     r0, [r3], #4
	
	ldr     r5, GAFR1x
	ldr		r0, [r5]
	str     r0, [r3], #4

	ldr     r5, GAFR0y
	ldr		r0, [r5]
	str     r0, [r3], #4

	ldr     r5, GAFR1y
	ldr		r0, [r5]
	str     r0, [r3], #4

	ldr     r5, GAFR0z
	ldr		r0, [r5]
	str     r0, [r3], #4

	ldr     r5, GAFR1z
	ldr		r0, [r5]
	str     r0, [r3], #4

	ldr     r5, GFERx
	ldr		r0, [r5]
	str     r0, [r3], #4

	ldr     r5, GFERy
	ldr		r0, [r5]
	str     r0, [r3], #4

	ldr     r5, GFERz
	ldr		r0, [r5]
	str     r0, [r3], #4

	ldr     r5, GRERx
	ldr		r0, [r5]
	str     r0, [r3], #4

	ldr     r5, GRERy
	ldr		r0, [r5]
	str     r0, [r3], #4

	ldr     r5, GRERz
	ldr		r0, [r5]
	str     r0, [r3], #4

	ldr     r0, UVIR_OST_BASE
	ldmia   r0!, {r4-r7}                    @ Get OSMR array
	stmia   r3!, {r4-r7}
	ldmia   r0!, {r4-r7}                    @ Get OSCR, OSSR, OWER, OIER
	stmia   r3!, {r4-r7}

	ldr     r0, UVIR_RTC_BASE

	ldr     r5, [r0, #RTTR]
	str     r5, [r3], #4

	ldr     r5, [r0, #RTSR]
	str     r5, [r3], #4

	ldr     r1, [r0, #RTSR]
	orr     r1, r1, #(RTC_AL | RTC_HZ)        @ Clear Clock Interrupt Sources
	str     r1, [r0, #RTSR]

	ldr     r0, UVIR_IC_BASE
	ldr     r5, [r0, #ICMR]
	str     r5, [r3]

	ldr     r1, [r0, #ICMR]
	and     r1, r1, #0x03								@ Mask all interrupts but hold GPIO0 & GPIO1
	str     r1, [r0, #ICMR]

	@@@@@@@@ show A
	ldr		r0, =0xff000fe0
	mov		r1, #0x41
	strb	r1, [r0, #0x00]

		mov		r0,#0							@
		mcr		p15, 0, r0, c7, c10, 4			@ Drain	the write Buffer

		mov     r1, #DCACHE_CACHELINECOUNT

		ldr     r0, R0VALUE		

LOOP17:
/* DisableInts r2 */
        mrs r2, cpsr							@ Get value of CPSR 
        orr r2, r2, #0x000000C0				@ Set IRQ and FIQ-disabling bits
        msr cpsr_c, r2						@ Disable the IRQ/FIQ

		mcr p15, 0, r0, c7, c2, 5               @ Allocate a line at the virtual address FLUSHRANGE_BASEADDRESS
												@  (tossing out a dirty line back to memory)

		mcr p15, 0, r0, c7, c6, 1               @ Invalidate the same cache-line

/* EnableInts r2 */
        mrs r2, cpsr							@ Get value of CPSR 
		and r2, r2, #0xFFFFFF3F			@ Set enable/disable bits
        msr cpsr_c, r2						@ Control the IRQ/FIQ

		add r0, r0, #CACHELINESIZE              @ Increment the address in R0 to the next cache line
		subs r1, r1, #1                         @ Decrement loop count
		bne LOOP17

		mov	r0,#0
		mcr p15, 0, r0, c7, c10, 4              @ Drain the Write Buffer

	b RefreshF

RefreshF:
	NOP
	NOP
	NOP
	NOP
	NOP
	NOP
	NOP
	NOP
	NOP
	NOP
	NOP
	mov		r0, #3						@ 3 = Sleep Mode (C7)
	mcr		p14, 0, r0, c7, c0, 0			@ Enter Sleep mode
	NOP
	NOP
	NOP
	NOP
	NOP
	NOP
	NOP
DLOOP:
	b	DLOOP


/*
 * cpu_sabinal_resume()
 *
 * entry point from bootloader into kernel during resume
 * 
 */

	.align 5
ENTRY(cpu_sabinal_resume)


	ldr     r3, sleep_param_p			@ Sleep mode information data structure
	mov		r0, r3

	ldmia	r0!, {r7-r10}

#ifdef CONFIG_XSCALE_CACHE_ERRATA
	bic	r8, r8, #0x0004
#endif

	mcr		p15, 0, r10, c3, c0, 0			//Restore DomainControlRegister
	mcr		p15, 0, r9, c2, c0, 0			//Restore TTB
	mcr		p15, 0, r0, c8, c7, 0			//Flush I+D TLBs
	mcr		p15, 0, r8, c1, c0, 0			//Restore MMU control
	mov		pc, r7							//go to Awake_address
	nop
	nop
	nop
	nop
	nop
	nop
	nop
	nop

Awake_address:	

#ifdef CONFIG_XSCALE_CACHE_ERRATA
	bl	cpu_xscale_proc_init
#endif
	ldr     r3, sleep_param			@ Sleep mode information data structure
	mov		r0, r3

#ifdef CONFIG_SABINAL_DISCOVERY_PreMV

	ldr     r0, GPSRy
	ldr     r1, =0x00040000
	str     r1, [r0]        @ turn off all GPIOs

#else

	ldr     r0, GPCRx
	mvn     r1, #0
	str     r1, [r0]        @ turn off all GPIOs

#endif

	add     r2, r3, #SleepState_GAFR0_X
	ldr     r1, [r2], #4
	ldr     r0, GAFR0x
	str     r1, [r0]
	ldr     r1, [r2], #4
	ldr     r0, GAFR1x
	str     r1, [r0]
	ldr     r1, [r2], #4
	ldr     r0, GAFR0y
	str     r1, [r0]
	ldr     r1, [r2], #4
	ldr     r0, GAFR1y
	str     r1, [r0]
	ldr     r1, [r2], #4
	ldr     r0, GAFR0z
	str     r1, [r0]
	ldr     r1, [r2], #4
	ldr     r0, GAFR1z
	str     r1, [r0]



	ldr     r1, [r2], #4
	ldr     r0, GFERx
	str     r1, [r0]
	ldr     r1, [r2], #4
	ldr     r0, GFERy
	str     r1, [r0]
	ldr     r1, [r2], #4
	ldr     r0, GFERz
	str     r1, [r0]


	ldr     r1, [r2], #4
	ldr     r0, GRERx
	str     r1, [r0]
	ldr     r1, [r2], #4
	ldr     r0, GRERy
	str     r1, [r0]
	ldr     r1, [r2], #4
	ldr     r0, GRERz
	str     r1, [r0]


	ldr     r0, UVIR_OST_BASE
	add     r2, r3, #SleepState_OSMR0
	ldr     r1, [r2], #4
	str     r1, [r0], #4					@ OSMR0
	ldr     r1, [r2], #4
	str     r1, [r0], #4            		@ OSMR1
	ldr     r1, [r2], #4
	str     r1, [r0], #4            		@ OSMR2
	ldr     r1, [r2], #4
	str     r1, [r0], #4            		@ OSMR3
	ldr     r1, [r2], #4
	str     r1, [r0], #4            		@ OSCR
	ldr     r1, [r2], #4
	str     r1, [r0], #4            		@ OSSR
	ldr     r1, [r2], #4
	str     r1, [r0], #4            		@ OWER
	ldr     r1, [r2], #4
	str     r1, [r0], #4            		@ OIER


	ldr     r0, UVIR_RTC_BASE
	add     r2, r3, #SleepState_RTTR
	ldr     r1, [r2], #4
//	str     r1, [r0, #RTTR]

	ldr     r0, UVIR_IC_BASE
	mov     r1, #1
	str     r1, [r0, #ICCR_IC]         		@ All interrupts bring the Cotulla out of idle
	str     r1, [r0, #ICLR]


	add     r2, r3, #SleepState_FIQ_SPSR
	mov     r1, #(Mode_FIQ | I_BIT |F_BIT) @ Enter FIQ mode, no interrupts
	msr     cpsr, r1
	ldr     r0, [r2], #4
	msr     spsr, r0
	ldr     r8, [r2], #4
	ldr     r9, [r2], #4
	ldr     r10, [r2], #4
	ldr     r11, [r2], #4
	ldr     r12, [r2], #4
	ldr     sp, [r2], #4
	ldr     lr, [r2], #4

	mov     r1, #(Mode_ABT | I_BIT | F_BIT) @ Enter ABT mode, no interrupts
	msr     cpsr, r1
	ldr     r0, [r2], #4
	msr     spsr, r0
	ldr     sp, [r2], #4
	ldr     lr, [r2], #4

	mov     r1, #(Mode_IRQ | I_BIT | F_BIT) @ Enter IRQ mode, no interrupts
	msr     cpsr, r1
	ldr     r0, [r2], #4
	msr     spsr, r0
	ldr     sp, [r2], #4
	ldr     lr, [r2], #4

	mov     r1, #(Mode_UND | I_BIT | F_BIT) @ Enter UND mode, no interrupts
	msr     cpsr, r1
	ldr     r0, [r2], #4
	msr     spsr, r0
	ldr     sp, [r2], #4
	ldr     lr, [r2], #4

	mov     r1, #(Mode_SYS | I_BIT | F_BIT) @ Enter SYS mode, no interrupts
	msr     cpsr, r1
	ldr     sp, [r2], #4
	ldr     lr, [r2]

	mov     r1, #(Mode_SVC | I_BIT | F_BIT) @ Enter SVC mode, no interrupts
	msr     cpsr, r1
	ldr     r0, [r3, #SleepState_SVC_SPSR]
	msr     spsr, r0
	ldr     sp, [r3, #SleepState_SVC_SP]

	
	ldr     r0, UVIR_IC_BASE	                @ Restore Cotulla Interrupt MASK
	ldr     r1, [r3, #SleepState_ICMR]
	str     r1, [r0, #ICMR]
	mov     r1, #1
	str     r1, [r0, #ICCR_IC]         		@ only unmasked interrupts can bring the xscale out of idle


	ldr     r1, =0xFFFFFFFF                 @ Clear Cotulla system interrupt sources
	ldr     r0, GEDRx
	str     r1, [r0]
	ldr     r0, GEDRy
	str     r1, [r0]
	ldr     r0, GEDRz
	str     r1, [r0]

	bl		SYMBOL_NAME(resume_init)

	ldmfd	sp!, {r0, r1, r2, r3, r4, r5, r6, r7, r8, r9, r10, r11, r12, pc}
	nop
	nop
	nop
	nop
	nop
	nop
	nop
	nop
	nop
	nop
	nop
	
#ifdef CONFIG_APM_CPU_IDLE
	.align 5
	.text

ENTRY(cpu_xscale_idle)

    mov		r0, #0x0    				@ set to Frequency Change Mode
    mcr		p14, 0, r0, c6, c0, 0		@ write to the CCLKCFG coprocessor reg

	mov		r0, #0x01						@ 1 = Idle Mode
	mcr		p14, 0, r0, c7, c0, 0			@ Enter Idle mode


	mov     pc, lr                          @ return

	.align 5
	.text

ENTRY(cpu_xscale_idle_exit)

    mov		r0, #0x1    				@ set to Frequency Change Mode
    mcr		p14, 0, r0, c6, c0, 0		@ write to the CCLKCFG coprocessor reg

	mov	pc ,lr	
	.align 5
	.text

ENTRY(cpu_xscale_change_speed_221)

	ldr		r0, LCD_BASE
	ldr		r1, LCCR3VALUE2
	str		r1, [r0, #LCCR3]

	ldr		r0, CMR_BASE
	ldr		r1, =0x221
	str		r1, [r0, #CCCR]
	mov		r0, #0x3    				@ set to Frequency Change Mode
	mcr		p14, 0, r0, c6, c0, 0		@ write to the CCLKCFG coprocessor reg

	ldr		r0, LCD_BASE
	ldr		r1, LCCR3VALUE1
	str		r1, [r0, #LCCR3]

	mov     pc, lr                          @ return

	.align 5
	.text

ENTRY(cpu_xscale_change_speed_141)

	ldr		r0, LCD_BASE
	ldr		r1, LCCR3VALUE2
	str		r1, [r0, #LCCR3]

	ldr		r0, CMR_BASE
	ldr		r1, =0x141
	str		r1, [r0, #CCCR]
  	mov		r0, #0x2    				@ set to Frequency Change Mode
	mcr		p14, 0, r0, c6, c0, 0		@ write to the CCLKCFG coprocessor reg

	ldr		r0, LCD_BASE
	ldr		r1, LCCR3VALUE1
	str		r1, [r0, #LCCR3]

	mov     pc, lr                          @ return

#endif
