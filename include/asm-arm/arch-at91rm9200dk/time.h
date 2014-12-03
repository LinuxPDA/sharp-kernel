/*
 *  linux/include/asm-arm/arch-at91rm9200dk/time.h
 *
 *  Copyright (C) 2002 ATMEL Rousset
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
 */
#include <asm/system.h>

#define CLK_PER_TICK  (AT91C_CLK_SLOW / 100)
#define US_PER_CLK    (1000 * 1000 / AT91C_CLK_SLOW)

extern unsigned long (*gettimeoffset)(void);

/*
 * Returns number of us since last clock interrupt.  Note that interrupts
 * will have been disabled by do_gettimeoffset()
 */
static unsigned long at91rm9200dk_gettimeoffset(void)
{
        AT91PS_SYS pSys = (AT91PS_SYS)AT91C_VA_BASE_SYS;
        unsigned int crtr = pSys->ST_CRTR;
        unsigned int rtar = pSys->ST_RTAR;
	/*
	 * Get the current number of ticks, reading the current real time register
	 * reset by the irq handler
	 */
	return ((pSys->ST_CRTR - pSys->ST_RTAR) & 0xFFFFF) * US_PER_CLK;
}

/*
 * IRQ handler for the timer
 */
static void at91rm9200dk_timer_interrupt(int irq, void *dev_id, struct pt_regs *regs)
{
	AT91PS_SYS pSys = (AT91PS_SYS)AT91C_VA_BASE_SYS;
	unsigned int crtr;
	unsigned long flags;

	if (pSys->ST_SR & AT91C_ST_PITS) {
		crtr = (pSys->ST_CRTR - pSys->ST_RTAR) & 0xFFFFF;
		do {
			local_irq_save(flags);
			crtr -= CLK_PER_TICK;
			pSys->ST_RTAR = (pSys->ST_RTAR + CLK_PER_TICK) & 0xfffff;
	do_timer(regs);
			local_irq_restore(flags);
		} while (crtr > CLK_PER_TICK);
	do_profile(regs);
	}
}

/*
 * Set up timer interrupt, and return the current time in seconds.
 */
static inline void setup_timer(void)
{
	volatile int status;
	AT91PS_SYS pSys = (AT91PS_SYS)AT91C_VA_BASE_SYS;

	pSys->ST_IDR = AT91C_ST_ALMS | AT91C_ST_PITS;                   // Interrupt disable Register

	status = pSys->ST_SR;

	pSys->ST_RTMR = 1;             // reset the ST_CRTR
	pSys->ST_PIMR = CLK_PER_TICK;  // Set the first tick interrupt
	pSys->ST_RTAR = 0;  // Set the first tick interrupt

	/*
	 * Make irqs happen for the system timer
	 */
	timer_irq.handler =  at91rm9200dk_timer_interrupt;
	timer_irq.flags = SA_SHIRQ;
	setup_arm_irq( AT91C_ID_SYS , &timer_irq);    // irq number for system timer is AT91C_ID_SYS
	gettimeoffset = at91rm9200dk_gettimeoffset;

	pSys->ST_IER =  AT91C_ST_PITS;                // Enable periodic interval interupt Register
}
