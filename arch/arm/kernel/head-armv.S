/*
 *  linux/arch/arm/kernel/head-armv.S
 *
 *  Copyright (C) 1994-1999 Russell King
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 *  32-bit kernel startup code for all architectures
 *
 * Change Log
 *   12-Nov-2001 Lineo Japan, Inc.
 *   30-Jul-2002 Lineo Japan, Inc.  for 2.4.18
 *   31-Jul-2002 Lineo Japan, Inc.  for ARCH_PXA
 */
#include <linux/config.h>
#include <linux/linkage.h>

#include <asm/assembler.h>
#include <asm/mach-types.h>
#include <asm/mach/arch.h>

#define K(a,b,c)	((a) << 24 | (b) << 12 | (c))

/*
 * We place the page tables 16K below TEXTADDR.  Therefore, we must make sure
 * that TEXTADDR is correctly set.  Currently, we expect the least significant
 * "short" to be 0x8000, but we could probably relax this restriction to
 * TEXTADDR > PAGE_OFFSET + 0x4000
 *
 * Note that swapper_pg_dir is the virtual address of the page tables, and
 * pgtbl gives us a position-independent reference to these tables.  We can
 * do this because stext == TEXTADDR
 *
 * swapper_pg_dir, pgtbl and krnladr are all closely related.
 */
#ifndef CONFIG_XIP_KERNEL
#if (TEXTADDR & 0xffff) != 0x8000
#error TEXTADDR must start at 0xXXXX8000
#endif

		.globl	SYMBOL_NAME(swapper_pg_dir)
		.equ	SYMBOL_NAME(swapper_pg_dir), TEXTADDR - 0x4000

		.macro	pgtbl, reg, rambase
		adr	\reg, stext
		sub	\reg, \reg, #0x4000
		.endm
#else
#if (DATAADDR & 0xffff) != 0x8000
#error DATAADDR must start at 0xXXXX8000
#endif

#define	PAGE_OFFSET	0xc0000000
#ifdef CONFIG_ARCH_LUBBOCK
#define	PHYS_OFFSET	0xa0000000
#elif CONFIG_ARCH_OMAP
#define	PHYS_OFFSET	0x10000000
#elif CONFIG_ARCH_KATANA
#define	PHYS_OFFSET	0x10000000
#elif CONFIG_ARCH_DBMX1
#define	PHYS_OFFSET	0x08000000
#elif CONFIG_ARCH_DBMX2
#define	PHYS_OFFSET	0xc0000000
#endif

		.globl	SYMBOL_NAME(swapper_pg_dir)
		.equ	SYMBOL_NAME(swapper_pg_dir), DATAADDR - 0x4000

		.macro	pgtbl, reg, rambase
		ldr	\reg, PGTBL
		add	\reg, \reg, #PHYS_OFFSET - PAGE_OFFSET
		.endm
#endif

/*
 * Since the page table is closely related to the kernel start address, we
 * can convert the page table base address to the base address of the section
 * containing both.
 */
		.macro	krnladr, rd, pgtable, rambase
		bic	\rd, \pgtable, #0x000ff000
		.endm

#ifdef CONFIG_ARCH_KATANA
		.macro	ldrx, rd, addr
		mov	\rd, \addr&0xff000000
		add	\rd, \rd, \addr&0x00ff0000
		add	\rd, \rd, \addr&0x0000ff00
		add	\rd, \rd, \addr&0x000000ff
		.endm
	
		.macro waitm cn
		ldrx	r1, #0x80007800 @ RTC
		ldr	r2, =0x01
		str	r2, [r1, #0x24] @ clear status
		ldrx	r2, #(0x147 * \cn) @ 32KHz - 10ms
		str	r2, [r1, #0x14]
		ldr	r2, =0x04
		str	r2, [r1, #0x10] @ countdown ebable
100:		ldr	r2, [r1, #0x24]
		tst	r2, #1
		beq	100b
		ldr	r2, =0x01
		str	r2, [r1, #0x24] @ clear status
		@ module timer off
		ldr	r2, [r1, #0x10]
		bic	r2, r2, #0x04
		str	r2, [r1, #0x10]
		.endm
#endif

/*
 * Kernel startup entry point.
 * ---------------------------
 *
 * This is normally called from the decompressor code.  The requirements
 * are: MMU = off, D-cache = off, I-cache = dont care, r0 = 0,
 * r1 = machine nr.
 *
 * This code is mostly position independent, so if you link the kernel at
 * 0xc0008000, you call this at __pa(0xc0008000).
 *
 * See linux/arch/arm/tools/mach-types for the complete list of machine
 * numbers for r1.
 *
 * We're trying to keep crap to a minimum; DO NOT add any machine specific
 * crap here - that's what the boot loader (or in extreme, well justified
 * circumstances, zImage) is for.
 */
		.section ".text.init",#alloc,#execinstr
		.type	stext, #function
ENTRY(stext)
		mov	r12, r0
/*
 * NOTE!  Any code which is placed here should be done for one of
 * the following reasons:
 *
 *  1. Compatability with old production boot firmware (ie, users
 *     actually have and are booting the kernel with the old firmware)
 *     and therefore will be eventually removed.
 *  2. Cover the case when there is no boot firmware.  This is not
 *     ideal, but in this case, it should ONLY set r0 and r1 to the
 *     appropriate value.
 */
#if defined(CONFIG_ARCH_NETWINDER)
/*
 * Compatability cruft for old NetWinder NeTTroms.  This
 * code is currently scheduled for destruction in 2.5.xx
 */
		.rept	8
		mov	r0, r0
		.endr

		adr	r2, 1f
		ldmdb	r2, {r7, r8}
		and	r3, r2, #0xc000
		teq	r3, #0x8000
		beq	__entry
		bic	r3, r2, #0xc000
		orr	r3, r3, #0x8000
		mov	r0, r3
		mov	r4, #64
		sub	r5, r8, r7
		b	1f

		.word	_stext
		.word	__bss_start

1:
		.rept	4
		ldmia	r2!, {r6, r7, r8, r9}
		stmia	r3!, {r6, r7, r8, r9}
		.endr
		subs	r4, r4, #64
		bcs	1b
		movs	r4, r5
		mov	r5, #0
		movne	pc, r0

		mov	r1, #MACH_TYPE_NETWINDER	@ (will go in 2.5)
		mov	r12, #2 << 24			@ scheduled for removal in 2.5.xx
		orr	r12, r12, #5 << 12
__entry:
#endif
#if defined(CONFIG_ARCH_L7200)
/*
 * FIXME - No bootloader, so manually set 'r1' with our architecture number.
 */
		mov	r1, #MACH_TYPE_L7200
#elif defined(CONFIG_SA1100_COLLIE)
		mov	r1, #MACH_TYPE_COLLIE
#elif defined(CONFIG_SABINAL_DISCOVERY)
		mov	r1, #0xff
		add	r1, r1, #MACH_TYPE_DISCOVERY-0xff
#endif
#if defined(CONFIG_S1C38000_TTE301)
		mov	r1, #0xff
		add	r1, r1, #MACH_TYPE_TTE301-0xff
#endif
#if defined(CONFIG_DBMX1_TPM102)
		mov	r1, #0xff
		add	r1, r1, #MACH_TYPE_TPM102-0xff
#endif
#if defined(CONFIG_ARCH_MX2ADS)
		mov	r1, #0xff
		add	r1, r1, #MACH_TYPE_MX2ADS-0xff
#endif

#ifdef CONFIG_XIP_KERNEL

#if defined(CONFIG_ARCH_LUBBOCK)
		mov	r1, #MACH_TYPE_LUBBOCK
#endif
#if defined(CONFIG_ARCH_KATANA)
		adr	r1, __entry
		tst	r1, #0xf0000000
		addeq	r1, r1, #0xf0000000
		mov	pc, r1
__entry:
		@ supervisor mode - disable IRQ/FIQ - ARM execution
		ldr	r14, =0xd3
		msr	cpsr_cxsf, r14
		@ Disable caches, mmu, etc.
		ldr     r1, =0x00000070 @ I & D cache off, wb off, mmu off
		mcr	p15, 0, r1, c1, c0 ,0
		@ Program External Bus Interface (EBIU)
		ldrx	r1, #0x8000a800
		ldr	r2, =0x05
		ldrx	r3, #0x00008820
		str	r3, [r1, r2, lsl #2]
		@ Enable PLL and switch clock source
		ldrx	r1, #0x80005800
		ldr	r2, =0x01
		ldr	r3, =0x00000040
		str	r3, [r1, r2, lsl #2] @ SBI
		ldr	r2, =0x02
		ldrx	r3, #0x0001a113
		str	r3, [r1, r2, lsl #2] @ PLL1
		ldr	r2, =0x03
		ldrx	r3, #0x0001a113
		str	r3, [r1, r2, lsl #2] @ PLL2
		ldr	r2, =0x08
		ldr	r3, =0x0000003c
		str	r3, [r1, r2, lsl #2] @ Enable GE Clock
		ldr	r2, =0x00
		ldr	r3, =0x00000013
		str	r3, [r1, r2, lsl #2] @ Enable PLL1 & PLL2 & GE
		waitm	0x05 @ 50[ms] wait
		ldrx	r1, #0x80005800
		ldr	r2, =0x07
		ldr	r3, =0x00000020
		ldr	r4, [r1, r2, lsl #2]
		orr	r4, r4, r3
		str	r4, [r1, r2, lsl #2] @ Switch to PLL1 clock frequency
		ldr	r2, =0x0a
		ldrx	r3, #0x000002eb
		str	r3, [r1, r2, lsl #2] @ ??
		@ Initilaise SDRAM
		ldrx	r1, #0x8000a800
		ldr	r2, =0x00
		ldr	r3, =0x0000002f	
		str	r3, [r1, r2, lsl #2] @ EBIU Enable
		ldr	r2, =0x07
		ldrx	r3, #0x00804263
		str	r3, [r1, r2, lsl #2] @ SRAM Interface
		ldr	r2, =0x08
		ldrx	r3, #0x0855ca20
		str	r3, [r1, r2, lsl #2] @ External Peripheral 0 Interface
		ldr	r2, =0x09
		ldrx	r3, #0x0855ca21
		str	r3, [r1, r2, lsl #2] @ External Peripheral 1 Interface
		ldr	r2, =0x0a
		ldrx	r3, #0x0025ec7d
		str	r3, [r1, r2, lsl #2] @ SDRAM Control 1
		ldr	r2, =0x0b
		ldr	r3, =0x000e0000
		str	r3, [r1, r2, lsl #2] @ SDRAM Control 2
		ldr	r2, =0x0c
		ldr	r3, =0x00000640
		str	r3, [r1, r2, lsl #2] @ SDRAM Control 3
		ldr	r2, =0x0d
		ldr	r3, =0x00000001
		str	r3, [r1, r2, lsl #2] @ SDRAM Control 4
		@  Initialise static memory
		ldrx	r1, #0x8000a000
		ldr	r2, =0x01
		ldrx	r3, #0x064a63a8
		str	r3, [r1, r2, lsl #2] @ MIU Control 2
		ldr	r2, =0x05
		ldrx	r3, #0x000e0041
		str	r3, [r1, r2, lsl #2] @ MIU Clock Control
		ldr	r2, =0x00
		ldr	r3, =0x00000001
		str	r3, [r1, r2, lsl #2] @  MIU Control 1 -> Enable MIU <-
		@ Turn on the high vect
		mrc	p15, 0, r0, c1, c0, 0
		ldrx	r0, #0x00001070
		mcr	p15, 0, r0, c1, c0, 0
		@ UART1 initialize 38400bps
		ldrx	r1, #0x80004000
		ldr	r2, =0x00000000
		str	r2, [r1, #0x00]
		ldrx	r2, #0x1b1800d8
		str	r2, [r1, #0x04]
		ldr	r2, =0x00000000
		str	r2, [r1, #0x08]
		ldr	r2, =0x00000001
		str	r2, [r1, #0x00]
		ldr	r2, =0x0000000d
		str	r2, [r1, #0x00]
		ldrx	sp, #0x10002000
		@ Machine Type
		mov	r1, #0xFF
		add	r1, r1, #MACH_TYPE_KATANA-0xFF
#endif
#if defined(CONFIG_ARCH_DBMX1)
		@ SDRAMC initialize
		ldr	r2, =0x00221000
		@ precharge
		ldr	r0, =0x92130300
		str	r0, [r2]
		@ auto refresh
		ldr	r0, =0xa2130300
		str	r0, [r2]
		mov	r0, #0x08000000
		ldr	r3, [r0]
		ldr	r3, [r0]
		ldr	r3, [r0]
		ldr	r3, [r0]
		ldr	r3, [r0]
		ldr	r3, [r0]
		ldr	r3, [r0]
		ldr	r3, [r0]
		@ set mode
		ldr	r0, =0xb2130300
		str	r0, [r2]
		ldr	r0, =0x08119800
		ldr	r3, [r0]
		@ set normal mode
		ldr	r0, =0x8213c300
		str	r0, [r2]
#endif
#if defined(CONFIG_ARCH_DBMX2)
		@ AHB-Lite IP Interface initialize
                ldr	r2, =0x10000000
                ldr	r0, =0x00040304
		str	r0, [r2], #4
                ldr	r0, =0xfffbfcfb
		str	r0, [r2]
                ldr	r2, =0x10020000
                ldr	r0, =0x00000000
		str	r0, [r2], #4
                ldr	r0, =0xffffffff
		str	r0, [r2]
		@ PLL 133MHz
                ldr	r2, =0x10027000
                ldr	r0, =0x17000607
		str	r0, [r2]
		@ SDRAMC initialize
		ldr	r2, =0xdf000000
		@ precharge
		ldr	r0, =0x92120300
		str	r0, [r2]
		@ auto refresh
		ldr	r0, =0xc0200000
		ldr	r3, [r0]
		ldr	r0, =0xa2120300
		str	r0, [r2]
		mov	r0, #0xc0000000
		ldr	r3, [r0]
		ldr	r3, [r0]
		ldr	r3, [r0]
		ldr	r3, [r0]
		ldr	r3, [r0]
		ldr	r3, [r0]
		ldr	r3, [r0]
		ldr	r3, [r0]
		@ set mode
		ldr	r0, =0xb2120300
		str	r0, [r2]
		ldr	r0, =0xc0119800
		ldr	r3, [r0]
		@ set normal mode
		ldr	r0, =0x8212f339
		str	r0, [r2]
#endif
		@ Data cache might be active.
		@ Be sure to flush kernel binary out of the cache,
		@ whatever state it is, before it is turned off.
		@ This is done by fetching through currently executed
		@ memory to be sure we hit the same cache.
		bic	r2, pc, #0x1f
		add	r3, r2, #0x10000	@ 64 kb is quite enough...
1:		ldr	r0, [r2], #32
		teq	r2, r3
		bne	1b
		mcr	p15, 0, r0, c7, c10, 4	@ drain WB
		mcr	p15, 0, r0, c7, c7, 0	@ flush I & D caches

		@ disabling MMU and caches
		mrc	p15, 0, r0, c1, c0, 0	@ read control reg
		bic	r0, r0, #0x05		@ clear DC, MMU
		bic	r0, r0, #0x1000		@ clear Icache
		mcr	p15, 0, r0, c1, c0, 0
#endif

		mov	r0, #F_BIT | I_BIT | MODE_SVC	@ make sure svc mode
		msr	cpsr_c, r0			@ and all irqs disabled
		bl	__lookup_processor_type
		teq	r10, #0				@ invalid processor?
		moveq	r0, #'p'			@ yes, error 'p'
		beq	__error
		bl	__lookup_architecture_type
		teq	r7, #0				@ invalid architecture?
		moveq	r0, #'a'			@ yes, error 'a'
		beq	__error
		bl	__create_page_tables
		adr	lr, __ret			@ return address
		add	pc, r10, #12			@ initialise processor
							@ (return control reg)

		.type	__switch_data, %object
__switch_data:	.long	__mmap_switched
		.long	SYMBOL_NAME(__bss_start)
		.long	SYMBOL_NAME(_end)
		.long	SYMBOL_NAME(processor_id)
		.long	SYMBOL_NAME(__machine_arch_type)
		.long	SYMBOL_NAME(cr_alignment)
		.long	SYMBOL_NAME(init_task_union)+8192

/*
 * Enable the MMU.  This completely changes the structure of the visible
 * memory space.  You will not be able to trace execution through this.
 * If you have an enquiry about this, *please* check the linux-arm-kernel
 * mailing list archives BEFORE sending another post to the list.
 */
		.type	__ret, %function
__ret:		ldr	lr, __switch_data
		mcr	p15, 0, r0, c1, c0
		mrc	p15, 0, r0, c1, c0, 0		@ read it back.
		mov	r0, r0
		mov	r0, r0
		mov	pc, lr

/*
 * The following fragment of code is executed with the MMU on, and uses
 * absolute addresses; this is not position independent.
 *
 *  r0  = processor control register
 *  r1  = machine ID
 *  r9  = processor ID
 */
		.align	5
__mmap_switched:
#ifdef CONFIG_XIP_KERNEL
		ldr	r3, ETEXT			@ data section copy
		ldr	r4, SDATA
		ldr	r5, EDATA
1:
		ldr	r6, [r3], #4
		str	r6, [r4], #4
		cmp	r4, r5
		blo	1b
#endif

		adr	r3, __switch_data + 4
		ldmia	r3, {r4, r5, r6, r7, r8, sp}@ r2 = compat
							@ sp = stack pointer

		mov	fp, #0				@ Clear BSS (and zero fp)
1:		cmp	r4, r5
		strcc	fp, [r4],#4
		bcc	1b

		str	r9, [r6]			@ Save processor ID
		str	r1, [r7]			@ Save machine type
#ifdef CONFIG_ALIGNMENT_TRAP
		orr	r0, r0, #2			@ ...........A.
#endif
		bic	r2, r0, #2			@ Clear 'A' bit
		stmia	r8, {r0, r2}			@ Save control register values
		b	SYMBOL_NAME(start_kernel)



/*
 * Setup the initial page tables.  We only setup the barest
 * amount which are required to get the kernel running, which
 * generally means mapping in the kernel code.
 *
 * We only map in 4MB of RAM, which should be sufficient in
 * all cases.
 *
 * r5 = physical address of start of RAM
 * r6 = physical IO address
 * r7 = byte offset into page tables for IO
 * r8 = page table flags
 */
__create_page_tables:
		pgtbl	r4, r5				@ page table address

		/*
		 * Clear the 16K level 1 swapper page table
		 */
		mov	r0, r4
		mov	r3, #0
		add	r2, r0, #0x4000
1:		str	r3, [r0], #4
		str	r3, [r0], #4
		str	r3, [r0], #4
		str	r3, [r0], #4
		teq	r0, r2
		bne	1b

#ifndef CONFIG_XIP_KERNEL

		/*
		 * Create identity mapping for first MB of kernel to
		 * cater for the MMU enable.  This identity mapping
		 * will be removed by paging_init()
		 */
		krnladr	r2, r4, r5			@ start of kernel
		add	r3, r8, r2			@ flags + kernel base
		str	r3, [r4, r2, lsr #18]		@ identity mapping

		/*
		 * Now setup the pagetables for our kernel direct
		 * mapped region.  We round TEXTADDR down to the
		 * nearest megabyte boundary.
		 */
		add	r0, r4, #(TEXTADDR & 0xff000000) >> 18 @ start of kernel
		bic	r2, r3, #0x00f00000
		str	r2, [r0]			@ PAGE_OFFSET + 0MB
		add	r0, r0, #(TEXTADDR & 0x00f00000) >> 18
		str	r3, [r0], #4			@ KERNEL + 0MB
		add	r3, r3, #1 << 20
		str	r3, [r0], #4			@ KERNEL + 1MB
		add	r3, r3, #1 << 20
		str	r3, [r0], #4			@ KERNEL + 2MB
		add	r3, r3, #1 << 20
		str	r3, [r0], #4			@ KERNEL + 3MB

		/*
		 * Ensure that the first section of RAM is present.
		 * we assume that:
		 *  1. the RAM is aligned to a 32MB boundary
		 *  2. the kernel is executing in the same 32MB chunk
		 *     as the start of RAM.
		 */
		bic	r0, r0, #0x01f00000 >> 18	@ round down
		and	r2, r5, #0xfe000000		@ round down
		add	r3, r8, r2			@ flags + rambase
		str	r3, [r0]

#else	/* CONFIG_XIP_KERNEL */

		mov	r3, pc, lsr #20
		mov	r3, r3, lsl #20		@ phys kernel start

		add	r0, r4, r3, lsr #18
		orr	r3, r3, r8
		str	r3, [r0]

#if defined(CONFIG_ARCH_DBMX1)
		orr	r3, r3, #0x10000000
#endif

		mov	r0, #TEXTADDR & 0xff000000
		add	r0, r0, #TEXTADDR & 0x00f00000	@ virt kernel start
		add	r0, r4, r0, lsr #18
		add	r2, r3, #4 << 20		@ kernel + 4MB

1:
		str	r3, [r0], #4
		add	r3, r3, #1 << 20
		cmp	r3, r2
		bne	1b

		bic	r3, r4, #0x000ff000		@ ram start
		add	r0, r4, r3, lsr #18
		orr	r3, r3, r8
		str	r3, [r0], #4

		add	r0, r3, #PAGE_OFFSET - PHYS_OFFSET
		add	r0, r4, r0, lsr #18
		add	r2, r3, #4 << 20		@ ram + 4MB

1:
		str	r3, [r0], #4
		add	r3, r3, #1 << 20
		cmp	r3, r2
		bne	1b

#endif	/* CONFIG_XIP_KERNEL */

		bic	r8, r8, #0x0c			@ turn off cacheable
							@ and bufferable bits
#ifdef CONFIG_ARCH_KATANA
	/* create mapping into 0x80000000 for io space in debugging use */
	/* r7 used to be 0x80000000, but now it's set to MQ9000_REGS_VBASE  */
	/* hardcode 0x80000c12 section descriptor into location 0x10006000 */
		add	r0, r4, #0x2000
		mov	r2, #0
		add	r2, r2, #0x800 << 20
		add	r2 ,r2, #0xc00
		add	r2 ,r2, #0x12
		str	r2, [r0]
#endif
#ifdef CONFIG_DEBUG_LL
		/*
		 * Map in IO space for serial debugging.
		 * This allows debug messages to be output
		 * via a serial console before paging_init.
		 */
		add	r0, r4, r7
		rsb	r3, r7, #0x4000	@ PTRS_PER_PGD*sizeof(long)
		cmp	r3, #0x0800
		addge	r2, r0, #0x0800
		addlt	r2, r0, r3
		orr	r3, r6, r8
1:		str	r3, [r0], #4
		add	r3, r3, #1 << 20
		teq	r0, r2
		bne	1b
#if defined(CONFIG_ARCH_NETWINDER) || defined(CONFIG_ARCH_CATS)
		/*
		 * If we're using the NetWinder, we need to map in
		 * the 16550-type serial port for the debug messages
		 */
		teq	r1, #MACH_TYPE_NETWINDER
		teqne	r1, #MACH_TYPE_CATS
		bne	1f
		add	r0, r4, #0x3fc0
		mov	r3, #0x7c000000
		orr	r3, r3, r8
		str	r3, [r0], #4
		add	r3, r3, #1 << 20
		str	r3, [r0], #4
1:
#endif
#endif
#ifdef CONFIG_ARCH_RPC
		/*
		 * Map in screen at 0x02000000 & SCREEN2_BASE
		 * Similar reasons here - for debug.  This is
		 * only for Acorn RiscPC architectures.
		 */
		add	r0, r4, #0x80			@ 02000000
		mov	r3, #0x02000000
		orr	r3, r3, r8
		str	r3, [r0]
		add	r0, r4, #0x3600			@ d8000000
		str	r3, [r0]
#endif
		mov	pc, lr



/*
 * Exception handling.  Something went wrong and we can't
 * proceed.  We ought to tell the user, but since we
 * don't have any guarantee that we're even running on
 * the right architecture, we do virtually nothing.
 * r0 = ascii error character:
 *	a = invalid architecture
 *	p = invalid processor
 *	i = invalid calling convention
 *
 * Generally, only serious errors cause this.
 */
__error:
#ifdef CONFIG_DEBUG_LL
		mov	r8, r0				@ preserve r0
		adr	r0, err_str
		bl	printascii
		mov	r0, r8
		bl	printch
#endif
#ifdef CONFIG_ARCH_RPC
/*
 * Turn the screen red on a error - RiscPC only.
 */
		mov	r0, #0x02000000
		mov	r3, #0x11
		orr	r3, r3, r3, lsl #8
		orr	r3, r3, r3, lsl #16
		str	r3, [r0], #4
		str	r3, [r0], #4
		str	r3, [r0], #4
		str	r3, [r0], #4
#endif
1:		mov	r0, r0
		b	1b

#ifdef CONFIG_DEBUG_LL
err_str:	.asciz	"\nError: "
		.align
#endif

/*
 * Read processor ID register (CP#15, CR0), and look up in the linker-built
 * supported processor list.  Note that we can't use the absolute addresses
 * for the __proc_info lists since we aren't running with the MMU on
 * (and therefore, we are not in the correct address space).  We have to
 * calculate the offset.
 *
 * Returns:
 *	r5, r6, r7 corrupted
 *	r8  = page table flags
 *	r9  = processor ID
 *	r10 = pointer to processor structure
 */
__lookup_processor_type:
		adr	r5, 2f
		ldmia	r5, {r7, r9, r10}
		sub	r5, r5, r10			@ convert addresses
		add	r7, r7, r5			@ to our address space
		add	r10, r9, r5
#if defined(CONFIG_ARCH_DBMX2)
		mrc	p15,0,r6,c15,c0,0
		orr	r6,r6,#0x10000
		mcr	p15,0,r6,c15,c0,0
#endif
		mrc	p15, 0, r9, c0, c0		@ get processor id
1:		ldmia	r10, {r5, r6, r8}		@ value, mask, mmuflags
		and	r6, r6, r9			@ mask wanted bits
		teq	r5, r6
		moveq	pc, lr
		add	r10, r10, #36			@ sizeof(proc_info_list)
		cmp	r10, r7
		blt	1b
		mov	r10, #0				@ unknown processor
		mov	pc, lr

/*
 * Look in include/asm-arm/procinfo.h and arch/arm/kernel/arch.[ch] for
 * more information about the __proc_info and __arch_info structures.
 */
2:		.long	__proc_info_end
		.long	__proc_info_begin
		.long	2b
		.long	__arch_info_begin
		.long	__arch_info_end

/*
 * Lookup machine architecture in the linker-build list of architectures.
 * Note that we can't use the absolute addresses for the __arch_info
 * lists since we aren't running with the MMU on (and therefore, we are
 * not in the correct address space).  We have to calculate the offset.
 *
 *  r1 = machine architecture number
 * Returns:
 *  r2, r3, r4 corrupted
 *  r5 = physical start address of RAM
 *  r6 = physical address of IO
 *  r7 = byte offset into page tables for IO
 */
__lookup_architecture_type:
		adr	r4, 2b
		ldmia	r4, {r2, r3, r5, r6, r7}	@ throw away r2, r3
		sub	r5, r4, r5			@ convert addresses
		add	r4, r6, r5			@ to our address space
		add	r7, r7, r5
1:		ldr	r5, [r4]			@ get machine type
		teq	r5, r1
		beq	2f
		add	r4, r4, #SIZEOF_MACHINE_DESC
		cmp	r4, r7
		blt	1b
		mov	r7, #0				@ unknown architecture
		mov	pc, lr
2:		ldmib	r4, {r5, r6, r7}		@ found, get results
		mov	pc, lr

#ifdef CONFIG_XIP_KERNEL

PGTBL:		.long	SYMBOL_NAME(swapper_pg_dir)

ETEXT:		.long	SYMBOL_NAME(_endtext)
SDATA:		.long	SYMBOL_NAME(_sdata)
EDATA:		.long	SYMBOL_NAME(__bss_start)

#endif
		.data
