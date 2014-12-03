/*
 *  arch/ppc/kernel/misc.S
 *
 *  
 *
 * This file contains miscellaneous low-level functions.
 *    Copyright (C) 1995-1996 Gary Thomas (gdt@linuxppc.org)
 *
 * Largely rewritten by Cort Dougan (cort@cs.nmt.edu)
 * and Paul Mackerras.
 * Adapted for iSeries by Mike Corrigan (mikejc@us.ibm.com)
 * PPC64 updates by Dave Engebretsen (engebret@us.ibm.com) 
 * 
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version
 * 2 of the License, or (at your option) any later version.
 *
 */

#include <linux/config.h>
#include <linux/sys.h>
#include <asm/unistd.h>
#include <asm/errno.h>
#include <asm/processor.h>
#include <asm/page.h>
#include <asm/cache.h>
#include "ppc_asm.h"

	.text

/*
 * Returns (address we're running at) - (address we were linked at)
 * for use before the text and data are mapped to KERNELBASE.
 */

_GLOBAL(reloc_offset)
	mflr	r0
	bl	1f
1:	mflr	r3
	LOADADDR(r4,1b)
	sub	r3,r4,r3
	mtlr	r0
	blr

_GLOBAL(get_msr)
	mfmsr	r3
	blr

_GLOBAL(get_dar)
	mfdar	r3
	blr

_GLOBAL(get_srr0)
	mfsrr0  r3
	blr

_GLOBAL(get_srr1)
	mfsrr1  r3
	blr
	
_GLOBAL(get_sp)
	mr	r3,r1
	blr
		
_GLOBAL(get_paca)
	mfspr   r3,SPRG3;
	blr

/* void __no_use_save_flags(unsigned long *flags) */
_GLOBAL(__no_use_save_flags)
	mfmsr	r4
	std	r4,0(r3)
	blr

/* void __no_use_restore_flags(unsigned long flags) */	
_GLOBAL(__no_use_restore_flags)
/*
 * Just set/clear the MSR_EE bit through restore/flags but do not
 * change anything else.  This is needed by the RT system and makes
 * sense anyway.
 *    -- Cort
 */
	mfmsr 	r5
	mr	r4,r3
	mr	r3,r5
	rlwimi	r3,r4,0,16,16
	/* Copy all except the MSR_EE bit from r4 (current MSR value)
	   to r3.  This is the sort of thing the rlwimi instruction is
	   designed for.  -- paulus. */
	 /* Check if things are setup the way we want _already_. */
	cmpw	0,r3,r5
	beqlr
	/* are we enabling interrupts? */
	rlwinm.	r0,r3,0,16,16
	beq	1f
	/* Check pending interrupts
	 *   A decrementer, IPI or PMC interrupt may have occurred
	 *   while we were in the hypervisor (which enables)
	 */
	CHECKANYINT(r4,r5)
	beq+	1f

	/* 
	 * Handle pending interrupts in interrupt context
	 */
	mtmsrd	r3
	li	r0,0x5555
	sc	
	blr
1:
	sync
	mtmsrd	r3
	isync
	blr
/* void __no_lpq_restore_flags(unsigned long flags) */	
_GLOBAL(__no_lpq_restore_flags)
	mfmsr 	r5
	mr	r4,r3
	mr	r3,r5
	rlwimi	r3,r4,0,16,16
	/* Copy all except the MSR_EE bit from r4 (current MSR value)
	   to r3.  This is the sort of thing the rlwimi instruction is
	   designed for.  -- paulus. */
	 /* Check if things are setup the way we want _already_. */
	cmpw	0,r3,r5
	beqlr
	sync
	mtmsrd	r3
	isync
	blr

_GLOBAL(__no_use_cli)
	mfmsr	r0		/* Get current interrupt state */
	rlwinm	r3,r0,16+1,32-1,31	/* Extract old value of 'EE' */
	rldicl	r0,r0,48,1
	rldicl	r0,r0,16,0      /* clear MSR_EE in r0 */
	mtmsrd	r0		/* Update machine state */
	blr			/* Done */

_GLOBAL(__no_use_sti)
	mfmsr	r3		/* Get current state */
	ori	r3,r3,MSR_EE	/* Turn on 'EE' bit */

	/* Check for pending interrupts
	 *   A decrementer, IPI or PMC interrupt may have occurred
	 *   while we were in the hypervisor (which enables)
	 */
	CHECKANYINT(r4,r5)
	beq+	1f

	/* 
	 * Handle pending interrupts in interrupt context
	 */
	mtmsrd	r3
	li	r0,0x5555
	sc	
	blr
1:	
	sync			/* Some chip revs have problems here... */
	mtmsrd	r3		/* Update machine state */
	blr

/*
 * We were about to enable interrupts but we have to simulate
 * some interrupts that were lost by enable_irq first.
 */
#if 0
_GLOBAL(do_fake_interrupt)
	mflr	r0
	std	r0,16(r1)
	std	r3,-8(r1)
	stdu	r1,-(STACK_FRAME_OVERHEAD+16)(r1)
1:	bl	.fake_interrupt
	/* Check for pending interrupt
	 *   A decrementer, IPI or PMC interrupt may have occurred
	 *   while we were in the hypervisor (which enables)
	 */
	CHECKANYINT(r4,r5)
	bne-	1b
	addi	r1,r1,STACK_FRAME_OVERHEAD+16
	ld	r3,-8(r1)
	ld	r0,16(r1)
	mtlr	r0
	mtmsrd	r3
	blr
#endif	
/*
 * complement mask on the msr then "or" some values on.
 *     _nmask_and_or_msr(nmask, value_to_or)
 */
_GLOBAL(_nmask_and_or_msr)
	mfmsr	r0		/* Get current msr */
	andc	r0,r0,r3	/* And off the bits set in r3 (first parm) */
	or	r0,r0,r4		/* Or on the bits in r4 (second parm) */
	mtmsrd	r0		/* Update machine state */
	SYNC
	blr			/* Done */

/*
 * Flush instruction cache.
 * This is a no-op on the 601.
 */
_GLOBAL(flush_instruction_cache)

/*
 * This is called by kgdb code
 * and should probably go away
 * to be replaced by invalidating
 * the cache lines that are actually
 * modified
 */
	mfspr	r3,PVR
	rlwinm	r3,r3,16,16,31
	cmpi	0,r3,1
	beqlr			/* for 601, do nothing */
	/* 603/604 processor - use invalidate-all bit in HID0 */
	mfspr	r3,HID0
	ori	r3,r3,HID0_ICFI
	mtspr	HID0,r3
	SYNC
	blr

/*
 * Write any modified data cache blocks out to memory
 * and invalidate the corresponding instruction cache blocks.
 *
 * flush_icache_range(unsigned long start, unsigned long stop)
 *
 *   flush all bytes from start through stop-1 inclusive
 */

_GLOBAL(flush_icache_range)

/*
 * Flush the data cache to memory 
 * 
 * Different systems have different cache line sizes
 * and in some cases i-cache and d-cache line sizes differ from
 * each other.
 */
	LOADADDR(r10,naca)		/* Get Naca address */
	ld	r10,0(r10)
	lhz	r7,DCACHEL1LINESIZE(r10)	/* Get cache line size */
	addi	r5,r7,-1
	andc	r6,r3,r5		/* round low to line bdy */
	subf	r8,r6,r4		/* compute length */
	add	r8,r8,r5		/* ensure we get enough */
	lhz	r9,DCACHEL1LOGLINESIZE(r10)	/* Get log-2 of cache line size */
	srw.	r8,r8,r9		/* compute line count */
	beqlr				/* nothing to do? */
	mtctr	r8
1:	dcbst	0,r6
	add	r6,r6,r7
	bdnz	1b
	sync

/* Now invalidate the instruction cache */
	
	lhz	r7,ICACHEL1LINESIZE(r10)	/* Get Icache line size */
	addi	r5,r7,-1
	andc	r6,r3,r5		/* round low to line bdy */
	subf	r8,r6,r4		/* compute length */
	add	r8,r8,r5
	lhz	r9,ICACHEL1LOGLINESIZE(r10)	/* Get log-2 of Icache line size */
	srw.	r8,r8,r9		/* compute line count */
	beqlr				/* nothing to do? */
	mtctr	r8
2:	icbi	0,r6
	add	r6,r6,r7
	bdnz	2b
	sync
	isync
	blr
	
/*
 * Like above, but only do the D-cache.
 *
 * flush_dcache_range(unsigned long start, unsigned long stop)
 *
 *    flush all bytes from start to stop-1 inclusive
 */
_GLOBAL(flush_dcache_range)

/*
 * Flush the data cache to memory 
 * 
 * Different systems have different cache line sizes
 */
	LOADADDR(r10,naca)		/* Get Naca address */
	ld	r10,0(r10)
	lhz	r7,DCACHEL1LINESIZE(r10)	/* Get dcache line size */
	addi	r5,r7,-1
	andc	r6,r3,r5		/* round low to line bdy */
	subf	r8,r6,r4		/* compute length */
	add	r8,r8,r5		/* ensure we get enough */
	lhz	r9,DCACHEL1LOGLINESIZE(r10)	/* Get log-2 of dcache line size */
	srw.	r8,r8,r9		/* compute line count */
	beqlr				/* nothing to do? */
	mtctr	r8
0:	dcbst	0,r6
	add	r6,r6,r7
	bdnz	0b
	sync
	blr

/*
 * Flush a particular page from the data cache to RAM.
 * Note: this is necessary because the instruction cache does *not*
 * snoop from the data cache.
 * This is a no-op on the 601 which has a unified cache.
 *
 *	void __flush_page_to_ram(void *page)
 */
_GLOBAL(__flush_page_to_ram)
/*
 * Flush the data cache to memory 
 * 
 * Different systems have different cache line sizes
 */

/* Flush the dcache */
	LOADADDR(r7,naca)
	ld	r7,0(r7)
	clrrdi	r3,r3,12           	    /* Page align */
	lhz	r4,DCACHEL1LINESPERPAGE(r7)	/* Get # dcache lines per page */
	lhz	r5,DCACHEL1LINESIZE(r7)		/* Get dcache line size */
	mr	r6,r3
	mtctr	r4
0:	dcbst	0,r6
	add	r6,r6,r5
	bdnz	0b
	sync

/* Now invalidate the icache */	

	lhz	r4,ICACHEL1LINESPERPAGE(r7)	/* Get # icache lines per page */
	lhz	r5,ICACHEL1LINESIZE(r7)		/* Get icache line size */
	mtctr	r4
1:	icbi	0,r3
	add	r3,r3,r5
	bdnz	1b
	sync	
	isync
	blr
	

/*
 * Flush a particular page from the instruction cache.
 * Note: this is necessary because the instruction cache does *not*
 * snoop from the data cache.
 *
 *	void __flush_icache_page(void *page)
 */
_GLOBAL(__flush_icache_page)
/*
 * Different systems have different cache line sizes
 */
 
/* Invalidate the icache */

	LOADADDR(r6,naca)
	ld	r6,0(r6)
	clrrdi	r3,r3,12           	    /* Page align */
	lhz	r4,ICACHEL1LINESPERPAGE(r6) /* Get # icache lines per page */
	lhz	r5,ICACHEL1LINESIZE(r6)	    /* Get icache line size */
	mtctr	r4
1:	icbi	0,r3
	add	r3,r3,r5
	bdnz	1b
	sync	
	isync
	blr
	
/*
 * Clear a page using the dcbz instruction, which doesn't cause any
 * memory traffic (except to write out any cache lines which get
 * displaced).  This only works on cacheable memory.
 */
_GLOBAL(clear_page)
	LOADADDR(r6,naca)
	ld	r6,0(r6)
	clrrdi	r3,r3,12           	    /* Page align */
	lhz	r4,DCACHEL1LINESPERPAGE(r6)	/* Get # dcache lines per page */
	lhz	r5,DCACHEL1LINESIZE(r6)		/* Get dcache line size */
	mtctr	r4
0:	dcbz	0,r3
	add	r3,r3,r5
	bdnz	0b
	blr

/*
 * Copy a whole page.  We use the dcbz instruction on the destination
 * to reduce memory traffic (it eliminates the unnecessary reads of
 * the destination into cache).  This requires that the destination
 * is cacheable.
 */
#define COPY_32_BYTES		\
	ld	r6,8(r4);	\
	ld	r7,16(r4);	\
	ld	r8,24(r4);	\
	ldu	r9,32(r4);	\
	std	r6,8(r3);	\
	std	r7,16(r3);	\
	std	r8,24(r3);	\
	stdu	r9,32(r3)

_GLOBAL(copy_page)

	LOADADDR(r9,naca)
	ld	r9,0(r9)
	clrrdi	r3,r3,12           	    /* Page align */
	clrrdi	r4,r4,12           	    /* Page align */
	lhz	r5,DCACHEL1LINESPERPAGE(r9)	/* Get # dcache lines per page */
	lhz	r0,DCACHEL1LINESIZE(r9)		/* Get dcache line size */
	mtctr	r5
	addi	r3,r3,-8
	addi	r4,r4,-8
	li	r10,8

	cmpi	0,r0,32
	beq	do_32_byte_line
	cmpi	0,r0,64
	beq	do_64_byte_line
	cmpi	0,r0,128
	beq	do_128_byte_line

	/* We don't have code specifically for this cache line size */
	/* Assume that the cache line size is at least 32 (and of */
	/* course a multiple of 32)  */
	/* This code will work for all power-of-2 cache line sizes */
	/* from 32 to 4096 */
	
1:	mr	r5,r0
	dcbz	r10,r3
0:	COPY_32_BYTES
	addi	r5,r5,-32
	or.	r5,r5,r5
	bne	0b
	bdnz	1b
	blr

	.globl do_32_byte_line
do_32_byte_line:
	dcbz	r10,r3
	COPY_32_BYTES
	bdnz	do_32_byte_line
	blr

	.globl do_64_byte_line
do_64_byte_line:
	dcbz	r10,r3
	COPY_32_BYTES
	COPY_32_BYTES
	bdnz	do_64_byte_line
	blr

	.globl do_128_byte_line	
do_128_byte_line:	
	dcbz	r10,r3
	COPY_32_BYTES
	COPY_32_BYTES
	COPY_32_BYTES
	COPY_32_BYTES
	bdnz	do_128_byte_line
	blr

/*
 * I/O string operations
 *
 * insb(port, buf, len)
 * outsb(port, buf, len)
 * insw(port, buf, len)
 * outsw(port, buf, len)
 * insl(port, buf, len)
 * outsl(port, buf, len)
 * insw_ns(port, buf, len)
 * outsw_ns(port, buf, len)
 * insl_ns(port, buf, len)
 * outsl_ns(port, buf, len)
 *
 * The *_ns versions don't do byte-swapping.
 */
_GLOBAL(_insb)
	cmpwi	0,r5,0
	mtctr	r5
	subi	r4,r4,1
	blelr-
00:	lbz	r5,0(r3)
	eieio
	stbu	r5,1(r4)
	bdnz	00b
	blr

_GLOBAL(_outsb)
	cmpwi	0,r5,0
	mtctr	r5
	subi	r4,r4,1
	blelr-
00:	lbzu	r5,1(r4)
	stb	r5,0(r3)
	eieio
	bdnz	00b
	blr	

_GLOBAL(_insw)
	cmpwi	0,r5,0
	mtctr	r5
	subi	r4,r4,2
	blelr-
00:	lhbrx	r5,0,r3
	eieio
	sthu	r5,2(r4)
	bdnz	00b
	blr

_GLOBAL(_outsw)
	cmpwi	0,r5,0
	mtctr	r5
	subi	r4,r4,2
	blelr-
00:	lhzu	r5,2(r4)
	eieio
	sthbrx	r5,0,r3	
	bdnz	00b
	blr	

_GLOBAL(_insl)
	cmpwi	0,r5,0
	mtctr	r5
	subi	r4,r4,4
	blelr-
00:	lwbrx	r5,0,r3
	eieio
	stwu	r5,4(r4)
	bdnz	00b
	blr

_GLOBAL(_outsl)
	cmpwi	0,r5,0
	mtctr	r5
	subi	r4,r4,4
	blelr-
00:	lwzu	r5,4(r4)
	stwbrx	r5,0,r3
	eieio
	bdnz	00b
	blr	

_GLOBAL(ide_insw)
_GLOBAL(_insw_ns)
	cmpwi	0,r5,0
	mtctr	r5
	subi	r4,r4,2
	blelr-
00:	lhz	r5,0(r3)
	eieio
	sthu	r5,2(r4)
	bdnz	00b
	blr

_GLOBAL(ide_outsw)
_GLOBAL(_outsw_ns)
	cmpwi	0,r5,0
	mtctr	r5
	subi	r4,r4,2
	blelr-
00:	lhzu	r5,2(r4)
	sth	r5,0(r3)
	eieio
	bdnz	00b
	blr	

_GLOBAL(_insl_ns)
	cmpwi	0,r5,0
	mtctr	r5
	subi	r4,r4,4
	blelr-
00:	lwz	r5,0(r3)
	eieio
	stwu	r5,4(r4)
	bdnz	00b
	blr

_GLOBAL(_outsl_ns)
	cmpwi	0,r5,0
	mtctr	r5
	subi	r4,r4,4
	blelr-
00:	lwzu	r5,4(r4)
	stw	r5,0(r3)
	eieio
	bdnz	00b
	blr	

/*
 * Extended precision shifts
 *
 * R3/R4 has 64 bit value
 * R5    has shift count
 * result in R3/R4
 *
 *  ashrdi3:     XXXYYY/ZZZAAA -> SSSXXX/YYYZZZ
 *  ashldi3:     XXXYYY/ZZZAAA -> YYYZZZ/AAA000
 *  lshrdi3:     XXXYYY/ZZZAAA -> 000XXX/YYYZZZ
 */
/* MIKEC: These may no longer be needed...what does gcc expect ? */
	
_GLOBAL(__ashrdi3)
	li	r6,32
	sub	r6,r6,r5
	slw	r7,r3,r6	/* isolate YYY */
	srw	r4,r4,r5	/* isolate ZZZ */
	or	r4,r4,r7	/* YYYZZZ */
	sraw	r3,r3,r5	/* SSSXXX */
	blr

_GLOBAL(__ashldi3)
	li	r6,32
	sub	r6,r6,r5
	srw	r7,r4,r6	/* isolate ZZZ */
	slw	r4,r4,r5	/* AAA000 */
	slw	r3,r3,r5	/* YYY--- */
	or	r3,r3,r7	/* YYYZZZ */
	blr

_GLOBAL(__lshrdi3)
	li	r6,32
	sub	r6,r6,r5
	slw	r7,r3,r6        /* isolate YYY */
	srw	r4,r4,r5        /* isolate ZZZ */
	or	r4,r4,r7        /* YYYZZZ */
	srw	r3,r3,r5        /* 000XXX */
	blr

_GLOBAL(abs)
	cmpi	0,r3,0
	bge	10f
	neg	r3,r3
10:	blr

_GLOBAL(_get_SP)
	mr	r3,r1		/* Close enough */
	blr
	
_GLOBAL(_get_PVR)
	mfspr	r3,PVR
	blr

_GLOBAL(_get_HID0)
	mfspr	r3,HID0
	blr

_GLOBAL(_get_ICTC)
	mfspr	r3,ICTC
	blr

_GLOBAL(_set_ICTC)
	mtspr	ICTC,r3
	blr

	

_GLOBAL(cvt_fd)
	lfd	0,-4(r5)	/* load up fpscr value */
	mtfsf	0xff,0
	lfs	0,0(r3)
	stfd	0,0(r4)
	mffs	0		/* save new fpscr value */
	stfd	0,-4(r5)
	blr

_GLOBAL(cvt_df)
	lfd	0,-4(r5)	/* load up fpscr value */
	mtfsf	0xff,0
	lfd	0,0(r3)
	stfs	0,0(r4)
	mffs	0		/* save new fpscr value */
	stfd	0,-4(r5)
	blr

_GLOBAL(__clear_msr_me)
        mfmsr   r0                      /* Get current interrupt state */
        lis     r3,0
        ori     r3,r3,MSR_ME
        andc    r0,r0,r3                /* Clears bit in (r4) */
        sync                            /* Some chip revs have problems here */
        mtmsrd   r0                     /* Update machine state */
        blr

/*
 * Create a kernel thread
 *   kernel_thread(fn, arg, flags)
 */
_GLOBAL(kernel_thread)
	mr	r6,r3		/* function */
	ori	r3,r5,CLONE_VM	/* flags */
	li	r0,__NR_clone
	sc
	cmpi	0,r3,0		/* parent or child? */
	bnelr			/* return if parent */

	mfspr	r5,SPRG3	/* Get Paca */
	ld	r5,PACACURRENT(r5)	/* Get 'current' */
	li	r0,0		/* clear out p->thread.regs */
	std	r0,THREAD+PT_REGS(r5)	/* since we don't have user ctx */
        
	ld	r6,0(r6)
	mtlr	r6              /* fn addr in lr */
	mr	r3,r4	        /* load arg and call fn */
	blrl
	li	r0,__NR_exit	/* exit after child exits */
        li	r3,0
	sc

/*
 * This routine is just here to keep GCC happy - sigh...
 */	
_GLOBAL(__main)
	blr

#define SYSCALL(name) \
_GLOBAL(name) \
	li	r0,__NR_##name; \
	sc; \
	bnslr; \
	/* MIKEC: Is errno a 32-bit int or a 64-bit long ? */\
	LOADBASE(r4,errno); \
	stw	r3,errno@l(r4); \
	li	r3,-1; \
	blr

#define __NR__exit __NR_exit

SYSCALL(sync)
SYSCALL(setsid)
SYSCALL(write)
SYSCALL(dup)
SYSCALL(execve)
SYSCALL(open)
SYSCALL(close)
SYSCALL(waitpid)
SYSCALL(fork)
SYSCALL(delete_module)
SYSCALL(_exit)
SYSCALL(lseek)
SYSCALL(read)
#ifdef CONFIG_BINFMT_ELF32
/* Why isn't this a) automatic, b) written in 'C'? */	
	.data
	.align 8
_GLOBAL(sys_call_table32)
	.llong .sys_ni_syscall	/* 0  -  old "setup()" system call */
	.llong .sys32_exit
	.llong .sys32_fork
	.llong .sys_read
	.llong .sys_write
	.llong .sys32_open		/* 5 */
	.llong .sys_close
	.llong .sys32_waitpid
	.llong .sys32_creat
	.llong .sys_link
	.llong .sys_unlink      	/* 10 */
	.llong .sys32_execve
	.llong .sys_chdir
	.llong .sys_time
	.llong .sys32_mknod
	.llong .sys32_chmod		/* 15 */
	.llong .sys_lchown
	.llong .sys_ni_syscall			/* old break syscall holder */
	.llong .sys32_stat
	.llong .sys32_lseek
	.llong .sys_getpid              /* 20 */
	.llong .sys32_mount
	.llong .sys_oldumount
	.llong .sys_setuid
	.llong .sys_getuid
	.llong .sys_stime		/* 25 */
	.llong .sys32_ptrace
	.llong .sys_alarm
	.llong .sys32_fstat
	.llong .sys32_pause
	.llong .sys32_utime		/* 30 */
	.llong .sys_ni_syscall			/* old stty syscall holder */
	.llong .sys_ni_syscall			/* old gtty syscall holder */
	.llong .sys32_access
	.llong .sys32_nice
	.llong .sys_ni_syscall	/* 35 */	/* old ftime syscall holder */
	.llong .sys_sync
	.llong .sys32_kill
	.llong .sys_rename
	.llong .sys32_mkdir
	.llong .sys_rmdir		/* 40 */
	.llong .sys_dup
	.llong .sys_pipe
	.llong .sys32_times
	.llong .sys_ni_syscall			/* old prof syscall holder */
	.llong .sys_brk		/* 45 */
	.llong .sys_setgid
	.llong .sys_getgid
	.llong .sys_signal
	.llong .sys_geteuid
	.llong .sys_getegid  	/* 50 */
	.llong .sys_acct
	.llong .sys32_umount			/* recycled never used phys() */
	.llong .sys_ni_syscall			/* old lock syscall holder */
	.llong .sys32_ioctl
	.llong .sys32_fcntl		/* 55 */
	.llong .sys_ni_syscall			/* old mpx syscall holder */
	.llong .sys32_setpgid
	.llong .sys_ni_syscall			/* old ulimit syscall holder */
	.llong .sys_olduname
	.llong .sys32_umask		/* 60 */
	.llong .sys_chroot
	.llong .sys_ustat
	.llong .sys_dup2
	.llong .sys_getppid
	.llong .sys_getpgrp	        /* 65 */
	.llong .sys_setsid
	.llong .sys32_sigaction
	.llong .sys_sgetmask
	.llong .sys32_ssetmask
	.llong .sys_setreuid	        /* 70 */
	.llong .sys_setregid
	.llong .sys_sigsuspend
	.llong .sys32_sigpending
	.llong .sys32_sethostname
	.llong .sys32_setrlimit	        /* 75 */
	.llong .sys32_old_getrlimit
	.llong .sys32_getrusage
	.llong .sys32_gettimeofday
	.llong .sys32_settimeofday
	.llong .sys32_getgroups	        /* 80 */
	.llong .sys32_setgroups
	.llong .ppc32_select
	.llong .sys_symlink
	.llong .sys32_lstat
	.llong .sys32_readlink	        /* 85 */
	.llong .sys_uselib
	.llong .sys32_swapon
	.llong .sys32_reboot
	.llong .old32_readdir
	.llong .sys32_mmap		/* 90 */
	.llong .sys_munmap
	.llong .sys_truncate
	.llong .sys_ftruncate
	.llong .sys_fchmod
	.llong .sys_fchown              /* 95 */
	.llong .sys32_getpriority
	.llong .sys32_setpriority
	.llong .sys_ni_syscall			/* old profil syscall holder */
	.llong .sys32_statfs
	.llong .sys32_fstatfs	/* 100 */
	.llong .sys32_ioperm
	.llong .sys32_socketcall
	.llong .sys32_syslog
	.llong .sys32_setitimer
	.llong .sys32_getitimer	/* 105 */
	.llong .sys32_newstat
	.llong .sys32_newlstat
	.llong .sys32_newfstat
	.llong .sys_uname
	.llong .sys32_iopl		/* 110 */
	.llong .sys_vhangup
	.llong .sys_ni_syscall	/* old 'idle' syscall */
	.llong .sys32_vm86
	.llong .sys32_wait4
	.llong .sys_swapoff	/* 115 */
	.llong .sys32_sysinfo
	.llong .sys32_ipc
	.llong .sys_fsync
	.llong .sys32_sigreturn
	.llong .sys32_clone		/* 120 */
	.llong .sys32_setdomainname
	.llong .ppc64_newuname
	.llong .sys32_modify_ldt
	.llong .sys32_adjtimex
	.llong .sys_mprotect	/* 125 */
	.llong .sys32_sigprocmask
	.llong .sys32_create_module
	.llong .sys32_init_module
	.llong .sys32_delete_module
	.llong .sys32_get_kernel_syms	/* 130 */
	.llong .sys32_quotactl
	.llong .sys32_getpgid
	.llong .sys_fchdir
	.llong .sys32_bdflush
	.llong .sys32_sysfs		/* 135 */
	.llong .sys32_personality
	.llong .sys_ni_syscall	        /* for afs_syscall */
	.llong .sys_setfsuid
	.llong .sys_setfsgid
	.llong .sys_llseek	        /* 140 */
        .llong .sys32_getdents
	.llong .ppc32_select
	.llong .sys_flock
	.llong .sys32_msync
	.llong .sys32_readv		/* 145 */
	.llong .sys32_writev
	.llong .sys32_getsid
	.llong .sys_fdatasync
	.llong .sys_sysctl
	.llong .sys_mlock		/* 150 */
	.llong .sys_munlock
	.llong .sys32_mlockall
	.llong .sys_munlockall
	.llong .sys32_sched_setparam
	.llong .sys32_sched_getparam	/* 155 */
	.llong .sys32_sched_setscheduler
	.llong .sys32_sched_getscheduler
	.llong .sys_sched_yield
	.llong .sys32_sched_get_priority_max
	.llong .sys32_sched_get_priority_min  /* 160 */
	.llong .sys32_sched_rr_get_interval
	.llong .sys32_nanosleep
	.llong .sys32_mremap
	.llong .sys_setresuid
	.llong .sys_getresuid	        /* 165 */
	.llong .sys32_query_module
	.llong .sys_poll
	.llong .sys32_nfsservctl
	.llong .sys_setresgid
	.llong .sys_getresgid	        /* 170 */
	.llong .sys32_prctl
	.llong .sys32_rt_sigreturn
	.llong .sys32_rt_sigaction
	.llong .sys32_rt_sigprocmask
	.llong .sys32_rt_sigpending     /* 175 */
	.llong .sys32_rt_sigtimedwait
	.llong .sys32_rt_sigqueueinfo
	.llong .sys32_rt_sigsuspend
	.llong .sys32_pread
	.llong .sys32_pwrite	        /* 180 */
	.llong .sys_chown
	.llong .sys_getcwd
	.llong .sys_capget
	.llong .sys_capset
	.llong .sys32_sigaltstack	        /* 185 */
	.llong .sys32_sendfile
	.llong .sys_ni_syscall		/* streams1 */
	.llong .sys_ni_syscall		/* streams2 */
	.llong .sys32_vfork
	.llong .sys32_getrlimit	        /* 190 */
	.llong .sys_ni_syscall		/* 191 */	/* Unused */
	.llong .sys_ni_syscall		/* 192 - reserved - mmap2 */
	.llong .sys32_truncate64	/* 193 - truncate64 */
	.llong .sys32_ftruncate64	/* 194 - ftruncate64 */
	.llong .sys_stat64      	/* 195 - stat64 */
	.llong .sys_lstat64     	/* 196 - lstat64 */
	.llong .sys_fstat64		/* 197 - fstat64 */
	.llong .sys32_pciconfig_read  	/* 198 */
	.llong .sys32_pciconfig_write  	/* 199 */
	.llong .sys_pciconfig_iobase 	/* 200 */
	.llong .sys_ni_syscall		/* 201 - reserved - MacOnLinux - new */
	.llong .sys_getdents64		/* 202 */
	.llong .sys_pivot_root		/* 203 */
	.llong .sys32_fcntl64		/* 204 */
	.llong .sys_madvise		/* 205 */
	.llong .sys_mincore		/* 206 */
	.rept NR_syscalls-206
		.llong .sys_ni_syscall
	.endr
#endif
	.data
	.align 8
_GLOBAL(sys_call_table)
	.llong .sys_ni_syscall	/* 0  -  old "setup()" system call */
	.llong .sys_exit
	.llong .sys_fork
	.llong .sys_read
	.llong .sys_write
	.llong .sys_open		/* 5 */
	.llong .sys_close
	.llong .sys_waitpid
	.llong .sys_creat
	.llong .sys_link
	.llong .sys_unlink	/* 10 */
	.llong .sys_execve
	.llong .sys_chdir
	.llong .sys_time
	.llong .sys_mknod
	.llong .sys_chmod		/* 15 */
	.llong .sys_lchown
	.llong .sys_ni_syscall			/* old break syscall holder */
	.llong .sys_stat
	.llong .sys_lseek
	.llong .sys_getpid	/* 20 */
	.llong .sys_mount
	.llong .sys_oldumount
	.llong .sys_setuid
	.llong .sys_getuid
	.llong .sys_stime		/* 25 */
	.llong .sys_ptrace
	.llong .sys_alarm
	.llong .sys_fstat
	.llong .sys_pause
	.llong .sys_utime		/* 30 */
	.llong .sys_ni_syscall			/* old stty syscall holder */
	.llong .sys_ni_syscall			/* old gtty syscall holder */
	.llong .sys_access
	.llong .sys_nice
	.llong .sys_ni_syscall	/* 35 */	/* old ftime syscall holder */
	.llong .sys_sync
	.llong .sys_kill
	.llong .sys_rename
	.llong .sys_mkdir
	.llong .sys_rmdir		/* 40 */
	.llong .sys_dup
	.llong .sys_pipe
	.llong .sys_times
	.llong .sys_ni_syscall			/* old prof syscall holder */
	.llong .sys_brk		/* 45 */
	.llong .sys_setgid
	.llong .sys_getgid
	.llong .sys_signal
	.llong .sys_geteuid
	.llong .sys_getegid	/* 50 */
	.llong .sys_acct
	.llong .sys_umount			/* recycled never used phys() */
	.llong .sys_ni_syscall			/* old lock syscall holder */
	.llong .sys_ioctl
	.llong .sys_fcntl		/* 55 */
	.llong .sys_ni_syscall			/* old mpx syscall holder */
	.llong .sys_setpgid
	.llong .sys_ni_syscall			/* old ulimit syscall holder */
	.llong .sys_olduname
	.llong .sys_umask		/* 60 */
	.llong .sys_chroot
	.llong .sys_ustat
	.llong .sys_dup2
	.llong .sys_getppid
	.llong .sys_getpgrp	/* 65 */
	.llong .sys_setsid
	.llong .sys_sigaction
	.llong .sys_sgetmask
	.llong .sys_ssetmask
	.llong .sys_setreuid	/* 70 */
	.llong .sys_setregid
	.llong .sys_sigsuspend
	.llong .sys_sigpending
	.llong .sys_sethostname
	.llong .sys_setrlimit	/* 75 */
	.llong .sys_old_getrlimit
	.llong .sys_getrusage
	.llong .sys_gettimeofday
	.llong .sys_settimeofday
	.llong .sys_getgroups	/* 80 */
	.llong .sys_setgroups
	.llong .sys_select
	.llong .sys_symlink
	.llong .sys_lstat
	.llong .sys_readlink	/* 85 */
	.llong .sys_uselib
	.llong .sys_swapon
	.llong .sys_reboot
	.llong .old_readdir
	.llong .sys_mmap	/* 90 */
	.llong .sys_munmap
	.llong .sys_truncate
	.llong .sys_ftruncate
	.llong .sys_fchmod
	.llong .sys_fchown	/* 95 */
	.llong .sys_getpriority
	.llong .sys_setpriority
	.llong .sys_ni_syscall			/* old profil syscall holder */
	.llong .sys_statfs
	.llong .sys_fstatfs	/* 100 */
	.llong .sys_ioperm
	.llong .sys_socketcall
	.llong .sys_syslog
	.llong .sys_setitimer
	.llong .sys_getitimer	/* 105 */
	.llong .sys_newstat
	.llong .sys_newlstat
	.llong .sys_newfstat
	.llong .sys_uname
	.llong .sys_iopl		/* 110 */
	.llong .sys_vhangup
	.llong .sys_ni_syscall	/* old 'idle' syscall */
	.llong .sys_vm86
	.llong .sys_wait4
	.llong .sys_swapoff	/* 115 */
	.llong .sys_sysinfo
	.llong .sys_ipc
	.llong .sys_fsync
	.llong .sys_sigreturn
	.llong .sys_clone		/* 120 */
	.llong .sys_setdomainname
	.llong .ppc64_newuname
	.llong .sys_modify_ldt
	.llong .sys_adjtimex
	.llong .sys_mprotect	/* 125 */
	.llong .sys_sigprocmask
	.llong .sys_create_module
	.llong .sys_init_module
	.llong .sys_delete_module
	.llong .sys_get_kernel_syms	/* 130 */
	.llong .sys_quotactl
	.llong .sys_getpgid
	.llong .sys_fchdir
	.llong .sys_bdflush
	.llong .sys_sysfs		/* 135 */
	.llong .sys_personality
	.llong .sys_ni_syscall	        /* for afs_syscall */
	.llong .sys_setfsuid
	.llong .sys_setfsgid
	.llong .sys_llseek	        /* 140 */
        .llong .sys_getdents
	.llong .sys_select
	.llong .sys_flock
	.llong .sys_msync
	.llong .sys_readv		/* 145 */
	.llong .sys_writev
	.llong .sys_getsid
	.llong .sys_fdatasync
	.llong .sys_sysctl
	.llong .sys_mlock		/* 150 */
	.llong .sys_munlock
	.llong .sys_mlockall
	.llong .sys_munlockall
	.llong .sys_sched_setparam
	.llong .sys_sched_getparam	/* 155 */
	.llong .sys_sched_setscheduler
	.llong .sys_sched_getscheduler
	.llong .sys_sched_yield
	.llong .sys_sched_get_priority_max
	.llong .sys_sched_get_priority_min  /* 160 */
	.llong .sys_sched_rr_get_interval
	.llong .sys_nanosleep
	.llong .sys_mremap
	.llong .sys_setresuid
	.llong .sys_getresuid	        /* 165 */
	.llong .sys_query_module
	.llong .sys_poll
	.llong .sys_nfsservctl
	.llong .sys_setresgid
	.llong .sys_getresgid	        /* 170 */
	.llong .sys_prctl
	.llong .sys_rt_sigreturn
	.llong .sys_rt_sigaction
	.llong .sys_rt_sigprocmask	
	.llong .sys_rt_sigpending	/* 175 */
	.llong .sys_rt_sigtimedwait
	.llong .sys_rt_sigqueueinfo
	.llong .sys_rt_sigsuspend
	.llong .sys_pread
	.llong .sys_pwrite	        /* 180 */
	.llong .sys_chown
	.llong .sys_getcwd
	.llong .sys_capget
	.llong .sys_capset
	.llong .sys_sigaltstack	        /* 185 */
	.llong .sys_sendfile
	.llong .sys_ni_syscall		/* streams1 */
	.llong .sys_ni_syscall		/* streams2 */
	.llong .sys_vfork
	.llong .sys_getrlimit	        /* 190 */
	.llong .sys_ni_syscall		/* 191 */	/* Unused */
	.llong .sys_ni_syscall		/* 192 - reserved - mmap2 */
	.llong .sys_ni_syscall		/* 193 - reserved - truncate64 */
	.llong .sys_ni_syscall		/* 194 - reserved - ftruncate64 */
	.llong .sys_ni_syscall		/* 195 - reserved - stat64 */
	.llong .sys_ni_syscall		/* 196 - reserved - lstat64 */
	.llong .sys_ni_syscall		/* 197 - reserved - fstat64 */
	.llong .sys_pciconfig_read	/* 198 */
	.llong .sys_pciconfig_write 	/* 199 */
	.llong .sys_pciconfig_iobase 	/* 200 */
	.llong .sys_ni_syscall		/* 201 - reserved - MacOnLinux - new */
	.llong .sys_getdents64		/* 202 */
	.llong .sys_pivot_root		/* 203 */
	.llong .sys_ni_syscall		/* 204 */
	.llong .sys_madvise		/* 205 */
	.llong .sys_mincore		/* 206 */
	.rept NR_syscalls-206
	.llong .sys_ni_syscall
	.endr
