/*
 * Copyright (c) 2003 Lineo uSolutions, Inc.
 *
 * include/asm-arm/arch-s1c38000/time.h
 *
 */

#ifndef __ASM_ARM_ARCH_S1C38000_TIME_H
#define __ASM_ARM_ARCH_S1C38000_TIME_H 1

#include <linux/config.h>
#include <asm/arch/irqs.h>

/*
 * Timer base register address
 */
#define TIMER_BASE	(IO_BASE + 0x80)

/*
 * Timer registers
 */
#define TIMER_LOAD0	(*(volatile unsigned long *) (TIMER_BASE + 0x000))
#define TIMER_COUNT0	(*(volatile unsigned long *) (TIMER_BASE + 0x004))
#define TIMER_CONTROL0	(*(volatile unsigned long *) (TIMER_BASE + 0x008))
#define TIMER_INTCLEAR0	(*(volatile unsigned long *) (TIMER_BASE + 0x00c))
#define TIMER_LOAD1	(*(volatile unsigned long *) (TIMER_BASE + 0x020))
#define TIMER_COUNT1	(*(volatile unsigned long *) (TIMER_BASE + 0x024))
#define TIMER_CONTROL1	(*(volatile unsigned long *) (TIMER_BASE + 0x028))
#define TIMER_INTCLEAR1	(*(volatile unsigned long *) (TIMER_BASE + 0x02c))
#define TIMER_INT	(*(volatile unsigned long *) (TIMER_BASE + 0x078))
#define TIMER_INDEX	(*(volatile unsigned long *) (TIMER_BASE + 0x07c))

/*
 * Timer register values
 */
#define	TIMER_ENABLE		0x80	/* Timer Enable */
#define	TIMER_MODE_FREE		0x00	/* Cound down mode: Free run */
#define	TIMER_MODE_CYCLIC	0x40	/* Cound down mode: Cyclic */
#define	TIMER_MODE_SINGLE	0x60	/* Cound down mode: Single */
#define	TIMER_CLDIV_0		0x00	/* Source clock direct */
#define	TIMER_CLDIV_4		0x10	/* Source clock 1/4   */
#define	TIMER_CLDIV_8		0x14	/* Source clock 1/8   */
#define	TIMER_CLDIV_16		0x04	/* Source clock 1/16  */
#define	TIMER_CLDIV_32		0x18	/* Source clock 1/32  */
#define	TIMER_CLDIV_256		0x08	/* Source clock 1/256 */
#define	TIMER_MODE		0x02	/* Mode bit */
#define	TIMER_REQUEST		0x01	/* Interrupt request */

/*
 * Handler for timer interrupt
 */
static void timer_interrupt(int irq, void *dev_id, struct pt_regs *regs)
{
    do_timer(regs);
    do_profile(regs);
    TIMER_INDEX = 0;
    TIMER_INTCLEAR0 = 0;
}

/*
 * Set up timer interrupt, and return the current time in seconds.
 */
static inline void setup_timer(void)
{
    TIMER_INDEX = 0;
    TIMER_INTCLEAR0 = 0;

    timer_irq.handler = timer_interrupt;

    setup_arm_irq(IRQ_TIMER0, &timer_irq);

    TIMER_INDEX = 0;
    TIMER_LOAD0 = (LATCH / 4) - 1;
    TIMER_CONTROL0 = TIMER_ENABLE | TIMER_MODE_CYCLIC | TIMER_CLDIV_4;
}

#endif
