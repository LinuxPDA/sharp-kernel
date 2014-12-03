/*
 * Carsten Langgaard, carstenl@mips.com
 * Copyright (C) 1999, 2000 MIPS Technologies, Inc.  All rights reserved.
 *
 * ########################################################################
 *
 *  This program is free software; you can distribute it and/or modify it
 *  under the terms of the GNU General Public License (Version 2) as
 *  published by the Free Software Foundation.
 *
 *  This program is distributed in the hope it will be useful, but WITHOUT
 *  ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 *  FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 *  for more details.
 *
 *  You should have received a copy of the GNU General Public License along
 *  with this program; if not, write to the Free Software Foundation, Inc.,
 *  59 Temple Place - Suite 330, Boston MA 02111-1307, USA.
 *
 * ########################################################################
 *
 * Interrupt exception dispatch code.
 *
 */
#include <linux/config.h>

#include <asm/asm.h>
#include <asm/mipsregs.h>
#include <asm/regdef.h>
#include <asm/stackframe.h>

/* A lot of complication here is taken away because:
 *
 * 1) We handle one interrupt and return, sitting in a loop and moving across
 *    all the pending IRQ bits in the cause register is _NOT_ the answer, the
 *    common case is one pending IRQ so optimize in that direction.
 *
 * 2) We need not check against bits in the status register IRQ mask, that
 *    would make this routine slow as hell.
 *
 * 3) Linux only thinks in terms of all IRQs on or all IRQs off, nothing in
 *    between like BSD spl() brain-damage.
 *
 * Furthermore, the IRQs on the BRECIS IC look basically (barring software
 * IRQs which we don't use at all and all external interrupt sources are
 * combined together on hardware interrupt 0 (MIPS IRQ 2)) like:
 *
 *	MIPS IRQ INT#	Source
 *      --------        ------
 *             0        Software Request 0 (ignored)
 *             1        Software Request 1 (Ignored)
 *             2  0	Ethernet MAC 0
 *             3  1     Ethernet MAC 1
 *             4  2     Option 2 (Voice, Packet or Security engine)
 *             5  3     Option 3 (voice, Packet or Security Engine)
 *             6  4     System Logic Module - the ones stuck together
 *             7  5     MIPS Internal Timer
 *
 * We handle the IRQ according to _our_ priority which is:
 *
 * Highest ----     R4k Timer
 * Lowest  ----     Combined hardware interrupt (NOTE: Still to be determined)
 *
 * then we just return, if multiple IRQs are pending then we will just take
 * another exception, big deal.
 */

/* #define DEBUG_IRQ 1 */ //...MaTed--- -- needs debug in msp5000.c
#define SLM_ERROR_BITS	0x3f00000	// just the error bits
			// 0x3f00300	// clear mask bits

	.text
	.set	noreorder
	.set	noat
	.align	5
	NESTED(mipsIRQ, PT_SIZE, sp)
	SAVE_ALL
	CLI
	.set	at

	mfc0	s0, CP0_CAUSE		# get irq mask

	/* First we check for r4k counter/timer IRQ. */
	andi	a0, s0, CAUSEF_IP7
	beq	a0, zero, 1f
	andi	a0, s0, CAUSEF_IP6	# delay slot, check hw0 interrupt

	/* Wheee, a timer interrupt. */
	move	a0, sp
	jal	mips_timer_interrupt
	nop

	j	ret_from_irq
	nop
1:
	beq	a0, zero, 1f
	andi	a0, s0, CAUSEF_IP2	# delay slot, check MAC 0 interrupt

	/* Wheee, combined hardware level zero interrupt. */
	jal	brecis_hw0_irqdispatch
	move	a0, sp			#delay slot

	j	ret_from_irq
	nop				# delay slot

1:
	beq	a0, zero, 1f
	andi	a0, s0, CAUSEF_IP3	# delay slot, check MAC 1 interrupt
	
	/***
	 *** Ethernet Mac 0 Interrupt
	 ***/
#ifdef DEBUG_IRQ
	PRINT("Got interrupt: MAC 0-NO brecis_hw0_irqdispatch .. retry\n")
	jal	dump154Mac
#endif
	jal	brecis_hw0_irqdispatch
	move	a0, sp			#delay slot

	j	ret_from_irq
	nop				# delay slot
1:	 
	beq	a0, zero, 1f
	andi	a0, s0, CAUSEF_IP4	# delay slot, Option 2 interrupt
	
	/***
	 *** Ethernet Mac 1 Interrupt
	 ***/
#ifdef DEBUG_IRQ
	PRINT("Got interrupt: MAC 1-NO brecis_hw0_irqdispatch.. retry\n")
	jal	dump154Mac
#endif
	jal	brecis_hw0_irqdispatch
	move	a0, sp			#delay slot

	j	ret_from_irq
	nop				# delay slot
1:	 
	beq	a0, zero, 1f
	andi	a0, s0, CAUSEF_IP5	# delay slot, Option 3 interrupt
	
	/***
	 *** Option 2 Interrupt
	 ***/
#ifdef DEBUG_IRQ
	PRINT("Got interrupt: Option 2 - unhandled\n")
	jal	dump154Mac
#endif
	jal	brecis_hw0_irqdispatch
	move	a0, sp			#delay slot

	j	ret_from_irq
	nop				# delay slot
1:	 
	beq	a0, zero, 1f
	nop			# delay slot
	/***
	 *** Option 3 Interrupt
	 ***/
#ifdef DEBUG_IRQ
	PRINT("Got interrupt: Option 3 - unhandled\n")
	jal	dump154Mac
#endif
	jal	brecis_hw0_irqdispatch
	move	a0, sp			#delay slot
	j	ret_from_irq
	nop				# delay slot
1:
	/*
	 * Here by mistake?  This is possible, what can happen is that by the
	 * time we take the exception the IRQ pin goes low, so just leave if
	 * this is the case.
	 */

// ...MaTed--- we get here for some unknown reason, no time for print
	lui	v0,0x8020
	lw	v0,0x2340(v0)
	lw	a1,0(v0)
	lui	a2,(SLM_ERROR_BITS >> 16)
	ori	a2,(SLM_ERROR_BITS & 0xffff)
	and	a2,a1,a2
	beq	a2,zero,1f
	nop
	sw	a2,0(v0)	// reset the bits that were on
#ifdef DEBUG_IRQ
	lw	a2,4(v0)
	PRINT("Brecis int status = %8x, mask = %8x\n")
	move	a1,s0
	PRINT("Got interrupt: c0_cause = %08x\n")
	mfc0	a1, CP0_EPC
	PRINT("c0_epc = %08x\n")
	jal	dump154Mac
#endif
// reset anything we are not handling
// NOTE:**** you may want to look at this section to make sure it 
//      ****  doesn't interfere with the other devices
1:
	j	ret_from_irq
	nop
	END(mipsIRQ)
