/*
 *  i2c_adap_pxa.c
 *
 *  I2C adapter for the PXA I2C bus access.
 *
 *  Copyright (C) 2002 Intrinsyc Software Inc.
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License version 2 as
 *  published by the Free Software Foundation.
 *
 *  History:
 *    Apr 2002: Initial version [CS]
 *    Jun 2002: Properly seperated algo/adap [FB]
 *    Jan 2003: Fixed several bugs concerning interrupt handling [Kai-Uwe Bloem]
 *    Jan 2003: added limited signal handling [Kai-Uwe Bloem]
 */

#include <linux/kernel.h>
#include <linux/module.h>

#include <linux/i2c.h>
#include <linux/i2c-id.h>
#include <linux/init.h>
#include <linux/time.h>
#include <linux/sched.h>
#include <linux/delay.h>
#include <linux/errno.h>

#include <asm/hardware.h>
#include <asm/irq.h>
#include <asm/arch/irqs.h>              /* for IRQ_I2C */

#include "i2c-pxa.h"

/*
 * Set this to zero to remove all debug statements via dead code elimination.
 */
//#define DEBUG       1

#if DEBUG
static unsigned int i2c_debug = DEBUG;
#else
#define i2c_debug	0
#endif

static int irq = 0;
static volatile int i2c_pending = 0;             /* interrupt pending when 1 */
static volatile int bus_error = 0;
static volatile int tx_finished = 0;
static volatile int rx_finished = 0;

static wait_queue_head_t i2c_wait;
static void i2c_pxa_transfer( int lastbyte, int receive, int midbyte);

/* place a byte in the transmit register */
static void i2c_pxa_write_byte(u8 value)
{
	IDBR = value;
}

/* read byte in the receive register */
static u8 i2c_pxa_read_byte(void)
{
	return (u8) (0xff & IDBR);
}

static void i2c_pxa_start(void)
{
	unsigned long icr = ICR;
	icr |= ICR_START;
	icr &= ~(ICR_STOP | ICR_ALDIE | ICR_ACKNAK);
	ICR = icr;

	bus_error=0;            /* clear any bus_error from previous txfers */
	tx_finished=0;          /* clear rx and tx interrupts from previous txfers */
	rx_finished=0;
	i2c_pending = 0;
}

static void i2c_pxa_repeat_start(void)
{
	unsigned long icr = ICR;
	icr |= ICR_START;
	icr &= ~(ICR_STOP | ICR_ALDIE);
	ICR = icr;

	bus_error=0;            /* clear any bus_error from previous txfers */
	tx_finished=0;          /* clear rx and tx interrupts from previous txfers */
	rx_finished=0;
	i2c_pending = 0;
}

static void i2c_pxa_stop(void)
{
	unsigned long icr = ICR;
	icr |= ICR_STOP;
	icr &= ~(ICR_START);
	ICR = icr;
}

static void i2c_pxa_midbyte(void)
{
	unsigned long icr = ICR;
	icr &= ~(ICR_START | ICR_STOP);
	ICR = icr;
}

static void i2c_pxa_abort(void)
{
	unsigned long timeout = jiffies + HZ/4;

#ifdef PXA_ABORT_MA
	while ((long)(timeout - jiffies) > 0 && (ICR & ICR_TB)) {
		set_current_state(TASK_INTERRUPTIBLE);
		schedule_timeout(1);
	}

	ICR |= ICR_MA;
	udelay(100);
#else
	while ((long)(timeout - jiffies) > 0 && (IBMR & 0x1) == 0) {
		i2c_pxa_transfer( 1, I2C_RECEIVE, 1);
		set_current_state(TASK_INTERRUPTIBLE);
		schedule_timeout(1);
	}
#endif
	ICR &= ~(ICR_MA | ICR_START | ICR_STOP);
}

static int i2c_pxa_wait_bus_not_busy( void)
{
	int timeout = DEF_TIMEOUT;

	while (timeout-- && (ISR & ISR_IBB)) {
		udelay(100); /* wait for 100 us */
	}

	return (timeout<=0);
}

static void i2c_pxa_wait_for_ite(void){
	unsigned long flags;
	if (irq > 0) {
		save_flags_cli(flags);
		if (i2c_pending == 0) {
			interruptible_sleep_on_timeout(&i2c_wait, I2C_SLEEP_TIMEOUT );
		}
		i2c_pending = 0;
		restore_flags(flags);
	} else {
		udelay(100);
	}
}

static int i2c_pxa_wait_for_int( int wait_type)
{
	int timeout = DEF_TIMEOUT;
#ifdef DEBUG
	if (bus_error)
		printk(KERN_INFO"i2c_pxa_wait_for_int: Bus error on enter\n");
	if (rx_finished)
		printk(KERN_INFO"i2c_pxa_wait_for_int: Receive interrupt on enter\n");
	if (tx_finished)
		printk(KERN_INFO"i2c_pxa_wait_for_int: Transmit interrupt on enter\n");
#endif

	if (wait_type == I2C_RECEIVE){         /* wait on receive */

		do {
			i2c_pxa_wait_for_ite();
		} while (!(rx_finished) && timeout-- && !signal_pending(current));

#ifdef DEBUG
		if (timeout<0){
			if (tx_finished)
				printk("Error: i2c-algo-pxa.o: received a tx"
						" interrupt while waiting on a rx in wait_for_int");
		}
#endif
	} else {                  /* wait on transmit */

		do {
			i2c_pxa_wait_for_ite();
		} while (!(tx_finished) && timeout-- && !signal_pending(current));

#ifdef DEBUG
		if (timeout<0){
			if (rx_finished)
				printk("Error: i2c-algo-pxa.o: received a rx"
					" interrupt while waiting on a tx in wait_for_int");
		}
#endif
	}

	udelay(ACK_DELAY);      /* this is needed for the bus error */

	tx_finished=0;
	rx_finished=0;

	if (bus_error){
		bus_error=0;
		if( i2c_debug > 2)printk("wait_for_int: error - no ack.\n");
		return BUS_ERROR;
	}

	if (signal_pending(current)) {
		return (-ERESTARTSYS);
	} else if (timeout < 0) {
		if( i2c_debug > 2)printk("wait_for_int: timeout.\n");
		return(-EIO);
	} else
		return(0);
}

static void i2c_pxa_transfer( int lastbyte, int receive, int midbyte)
{
	if( lastbyte)
	{
		if( receive==I2C_RECEIVE) ICR |= ICR_ACKNAK;
		i2c_pxa_stop();
	}
	else if( midbyte)
	{
		i2c_pxa_midbyte();
	}
	ICR |= ICR_TB;
}

static void i2c_pxa_reset( void)
{
#ifdef DEBUG
	printk("Resetting I2C Controller Unit\n");
#endif

	/* abort any transfer currently under way */
	i2c_pxa_abort();

	/* reset according to 9.8 */
	ICR = ICR_UR;
	ISR = I2C_ISR_INIT;
	ICR &= ~ICR_UR;

	/* set the global I2C clock on */
	CKEN |= CKEN14_I2C;

	/* set our slave address */
	ISAR = I2C_PXA_SLAVE_ADDR;

	/* set control register values */
	ICR = I2C_ICR_INIT;

	/* clear any leftover states from prior transmissions */
	i2c_pending = rx_finished = tx_finished = bus_error = 0;

	/* enable unit */
	ICR |= ICR_IUE;
	udelay(100);
}

static void i2c_pxa_handler(int this_irq, void *dev_id, struct pt_regs *regs)
{
	int status, wakeup = 0;
	status = (ISR);

	if (status & ISR_BED){
		(ISR) |= ISR_BED;
		bus_error=ISR_BED;
		wakeup = 1;
	}
	if (status & ISR_ITE){
		(ISR) |= ISR_ITE;
		tx_finished=ISR_ITE;
		wakeup = 1;
	}
	if (status & ISR_IRF){
		(ISR) |= ISR_IRF;
		rx_finished=ISR_IRF;
		wakeup = 1;
	}
	if (wakeup) {
		i2c_pending = 1;
		wake_up_interruptible(&i2c_wait);
	}
}

static int i2c_pxa_resource_init( void)
{
	init_waitqueue_head(&i2c_wait);

	if (request_irq(IRQ_I2C, &i2c_pxa_handler, SA_INTERRUPT, "I2C_PXA", 0) < 0) {
		irq = 0;
		if( i2c_debug)
			printk(KERN_INFO "I2C: Failed to register I2C irq %i\n", IRQ_I2C);
		return -ENODEV;
	}else{
		irq = IRQ_I2C;
		enable_irq(irq);
	}
	return 0;
}

static void i2c_pxa_resource_release( void)
{
	if( irq > 0)
	{
		disable_irq(irq);
		free_irq(irq,0);
		irq=0;
	}
}

static void i2c_pxa_inc_use(struct i2c_adapter *adap)
{
#ifdef MODULE
	MOD_INC_USE_COUNT;
#endif
}

static void i2c_pxa_dec_use(struct i2c_adapter *adap)
{
#ifdef MODULE
	MOD_DEC_USE_COUNT;
#endif
}

static int i2c_pxa_client_register(struct i2c_client *client)
{
	return 0;
}

static int i2c_pxa_client_unregister(struct i2c_client *client)
{
	return 0;
}

static struct i2c_algo_pxa_data i2c_pxa_data = {
	write_byte:		i2c_pxa_write_byte,
	read_byte:		i2c_pxa_read_byte,

	start:			i2c_pxa_start,
	repeat_start:		i2c_pxa_repeat_start,
	stop:			i2c_pxa_stop,
	abort:			i2c_pxa_abort,

	wait_bus_not_busy:	i2c_pxa_wait_bus_not_busy,
	wait_for_interrupt:	i2c_pxa_wait_for_int,
	transfer:		i2c_pxa_transfer,
	reset:			i2c_pxa_reset,

	udelay:			10,
	timeout:		DEF_TIMEOUT,
};

static struct i2c_adapter i2c_pxa_ops = {
	name:                   "PXA-I2C-Adapter",
	id:                     I2C_ALGO_PXA,
	algo_data:              &i2c_pxa_data,
	inc_use:                i2c_pxa_inc_use,
	dec_use:                i2c_pxa_dec_use,
	client_register:        i2c_pxa_client_register,
	client_unregister:      i2c_pxa_client_unregister,
	retries:                2,
};

extern int i2c_pxa_add_bus(struct i2c_adapter *);
extern int i2c_pxa_del_bus(struct i2c_adapter *);

static int __init i2c_adap_pxa_init(void)
{
	if( i2c_pxa_resource_init() == 0) {

		if (i2c_pxa_add_bus(&i2c_pxa_ops) < 0) {
			i2c_pxa_resource_release();
			printk(KERN_INFO "I2C: Failed to add bus\n");
			return -ENODEV;
		}
	} else {
		return -ENODEV;
	}

	printk(KERN_INFO "I2C: Successfully added bus\n");

	return 0;
}

static void i2c_adap_pxa_exit(void)
{
	i2c_pxa_del_bus( &i2c_pxa_ops);
	i2c_pxa_resource_release();

	printk(KERN_INFO "I2C: Successfully removed bus\n");
}

module_init(i2c_adap_pxa_init);
module_exit(i2c_adap_pxa_exit);
