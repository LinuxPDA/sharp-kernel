/*
 * linux/drivers/i2c/i2c-aa-dbmx.c
 *
 * Copyright (C) 2003 Lineo Solutions, Inc.
 *
 * Based on:
********************************************************************************
	Copyright (C) 2002 Motorola GSG-China

	This program is free software; you can redistribute it and/or
	modify it under the terms of the GNU General Public License
	as published by the Free Software Foundation; either version 2
	of the License, or (at your option) any later version.

	This program is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with this program; if not, write to the Free Software
	Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
*******************************************************************************/
/************************************************************************
 *  File Name : i2c-aa.c
 *  Description :
 *	Implementation of i2c Adapter/Algorithm Driver
 *  Auther
 *  History:
 *  	2002/2/7 use msgs[]
 *  Data
 *  ID
 ************************************************************************/
#include <linux/config.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/delay.h>
#include <linux/slab.h>
#include <linux/version.h>
#include <linux/init.h>
#include <asm/uaccess.h>
#include <linux/ioport.h>
#include <linux/errno.h>
#include <linux/sched.h>
#include <linux/hfs_sysdep.h>

#include <linux/i2c.h>
#include <linux/i2c-algo-bit.h>
#include <linux/i2c-id.h>
#include "i2c-aa-dbmx.h"
#include <asm/irq.h>

#ifdef CONFIG_ARCH_DBMX2
#include "asm/arch/mx2.h"
#include "asm/arch/pll.h"
#endif

#define POLLING_MODE

/************************************************************************
 * 	Globel Variable Define
 ************************************************************************/ 	

#ifdef CONFIG_ARCH_MX1ADS
static struct I2C_AA_REG *i2c_aa_reg = (struct I2C_AA_REG *)I2C_AA_REG_ADDR;
#endif

#ifdef CONFIG_ARCH_DBMX2
static struct I2C_AA_REG *i2c_aa_reg = (struct I2C_AA_REG *)&_reg_I2C_IADR;
#endif

#define i2c_aa_start()	i2c_aa_reg->i2cr |= 0x20
#define i2c_aa_stop() i2c_aa_reg->i2cr &= ~(0x20)
#define i2c_aa_repeat_start() i2c_aa_reg->i2cr |= 0x04

int i2c_aa_bus_release(void);
#if (LINUX_VERSION_CODE < 0x020301)
static struct wait_queue *aa_wait = NULL;
#else
static wait_queue_head_t aa_wait;
#endif

int32_t __init i2c_aa_init(void);
static void __exit i2c_aa_cleanup(void);
static void i2c_aa_inc(struct i2c_adapter *adap);
static void i2c_aa_dec(struct i2c_adapter *adap);
int i2c_aa_xfer(struct i2c_adapter *i2c_adap, struct i2c_msg msgs[], int num);
static void i2c_aa_isr (int16_t irq, void * dev_id, struct pt_regs * reg);
int i2c_aa_ioctl(struct i2c_adapter * adapter, unsigned int cmd, unsigned long arg);

static struct i2c_algorithm i2c_aa_algorithm = 
{
	/* name */		"DBMX I2C Algorithm",
	/* id: */		I2C_ALGO_BIT,
	/* master_xfer:*/	i2c_aa_xfer,
	/* smbus_xfer: */	NULL,
	/* slave_send: */	NULL,
	/* slave_recv: */	NULL,
	/* algo_control: */	i2c_aa_ioctl,
	/* functionality: */	NULL
};
	
static struct i2c_adapter i2c_csi_adapter = 
{
	/* name: */				"DBMX I2C Adapter",
	/* id = algo->id | hwdep.struct->id */	I2C_ALGO_BIT | I2C_HW_B_SER,	
	/* i2c_algorithm: */			&i2c_aa_algorithm,
	/* algo_data */				NULL,
	/* inc_use */				i2c_aa_inc,
	/* dec_use */				i2c_aa_dec,
	/* client_regester */			NULL,
	/* client_unregister */ 		NULL
};

/* return 1 : bus is idle/stop signal is detected , and Arbitration Lost
 * return 0 : bus is busy/start signal is detected */
 int i2c_aa_bus_grab(void)
{
	unsigned int val = 0;
	unsigned long timeout = 10000;

	while (timeout--) {
		val = i2c_aa_reg->i2sr;
		if (val & 0x10)	// if arbitration lost
		{
			i2c_aa_reg->i2sr &= ~(UINT32)0x10;	// clear arbitration lost bit	
			printk(KERN_DEBUG "i2c_aa_bus_grab arbitration lost\n");
			return 1;
		}
		if (val & 0x20)
			return 0;
	}
	printk(KERN_DEBUG "i2c_aa_bus_grab timeout %02x\n", val);
	return 1;
}
/* return when a stop signal is detect */
int i2c_aa_bus_release(void)
{	
	unsigned int val = 0;
	unsigned long timeout = 10000;

	while (timeout--) {
		val = i2c_aa_reg->i2sr;
		if (val & 0x10)	// if arbitration lost
		{
			i2c_aa_reg->i2sr &= ~(UINT32)0x10;	// clear arbitration lost bit	
			printk(KERN_DEBUG "i2c_aa_bus_release arbitration lost\n");
			return 1;
		}
		if (!(val & 0x20))
			return 0;
	}
	printk(KERN_DEBUG "i2c_aa_bus_release timeout %02x\n", val);
	return 1;
}
/* wait until the interrupt pending then , clear IIF[I2SR:1] */
int i2c_aa_transfer_complete(void)
{
	unsigned int val = 0;
	unsigned long timeout = 10000;

	while (timeout--) {
		val = i2c_aa_reg->i2sr;
		if (val & 0x10)	// if arbitration lost
		{
			i2c_aa_reg->i2sr &= ~(UINT32)0x10;	// clear arbitration lost bit	
			printk(KERN_DEBUG "i2c_aa_transfer_complete arbitration lost\n");
			return 1;
		}
		if (val & 0x02) {
			i2c_aa_reg->i2sr &= ~(UINT32)0x02;
			return 0;
		}
	}
	printk(KERN_DEBUG "i2c_aa_transfer_complete timeout %02x\n", val);
	return 1;
}

/************************************************************************
 * 	I2C_AA Driver
 ************************************************************************/ 	
void i2c_aa_inc(struct i2c_adapter *adap)
{
	FUNC_START;
	MOD_INC_USE_COUNT;
	FUNC_END;
}

void i2c_aa_dec(struct i2c_adapter *adap)
{
	FUNC_START;
	MOD_DEC_USE_COUNT;
	FUNC_END;
}

#ifndef POLLING_MODE
#include "i2c_rw.h"
#endif


void GPIO_init(void)
{
#ifdef POLLING_MODE

#ifdef CONFIG_ARCH_MX1ADS
	/* 
	 * port enable for I2c signal 
	 * PA15 : I2C_DATA
	 * PA16 : I2C_CLK
	 */
	* PTA_DDIR |=  0x00018000;
	* PTA_GIUS &= ~0x00018000;
#endif

#ifdef CONFIG_ARCH_DBMX2
	_reg_GPIO_GIUS(GPIOD)	&=~((1<<17)|(1<<18));
	_reg_GPIO_GPR(GPIOD)	&=~((1<<17)|(1<<18));
	_reg_GPIO_DDIR(GPIOD)	|=((1<<17)|(1<<18));
	
	/* 
	 * PD20 (!? 19)  --> I2C pull-up adaptor control, GPIO output, pull-up disabled, use DIDR for output
	 * this is for ADS0.0 errata #5.  The actual pull up/down codes are inside the respective i2c
	 * client write/read functions
	 */
	_reg_GPIO_GIUS(GPIOD)	|= 0x00100000;
	_reg_GPIO_DDIR(GPIOD)	|= 0x00100000;
	_reg_GPIO_OCR2(GPIOD)	|= 0x00000300;
	_reg_GPIO_DR(GPIOD)		|= 0x00100000;
	_reg_GPIO_DR(GPIOD)		&= ~0x00100000;

#endif
	
#else
#ifdef CONFIG_ARCH_MX1ADS
	SETMASK(PTA_DDIR,   0x18000);
	CLEARMASK(PTA_GIUS, 0x18000);
	CLEARMASK(PTA_GPR, 0x18000);
	CLEARMASK(PTA_PUEN, 0x18000);
#endif	
#endif

}


/*-----------------------------------------------------------------------
 * int i2c_aa_xfer(struct i2c_adapter *i2c_adap,
 *		    struct i2c_msg msgs[ ], SINT16 num)
 * This function is responsible for the transfer of the data through the I2C bus
 * 
 * Parameter :
 * i2c_adap	the structure associated with the related hardware.
 * msgs[ ]	the body of the message to be send out
 * num		number of message
 * 
 * Return : 
 *	Success		Number of message has been transferred
 *	Failure		-1	
 *----------------------------------------------------------------------*/	
//#define MSG_LEN
#ifdef POLLING_MODE
#if 1
int i2c_aa_xfer(struct i2c_adapter *i2c_adap, struct i2c_msg msgs[], int num)
{
	short i,j,count = 0;
	FUNC_START;
start:
	// I2C module reset
	i2c_aa_reg->i2cr = (UINT32)0x0;

	// Set the IEN[I2CR:7] to enable the I2C bus interface system
	i2c_aa_reg->i2cr |= (UINT32)0x80;

	// Set clock Freq
	i2c_aa_reg->ifdr = (UINT32)DEFAULT_FREQ;
	
	// Set i2c address
	i2c_aa_reg->iadr = (UINT32)DEFAULT_I2C_ADDRESS;
	
	// Set the TXAK[I2CR:6] to disable the I2C transmit ACK
	i2c_aa_reg->i2cr |= (UINT32)0x08;
	
	for ( i = 0; i < num; i ++)	// for each message
	{
		CSI_LOC;
		// generate a START signal and select the master mode
		// and wait until the bus is not busy
		if (i == 0)
			i2c_aa_start();
		else
			i2c_aa_repeat_start();
		CSI_LOC;
		// if Arbitration lost and bus is idle
		if(i2c_aa_bus_grab())
		{
			CSI_LOC;
			i2c_aa_stop();
			i2c_aa_bus_release();
			goto start;
		}
		CSI_LOC;

		// Case READ
		if (((msgs[i].flags) & (UINT16)I2C_M_READ ))
		{
			// Set the MTX[I2CR:4] to enable the master transmit
			i2c_aa_reg->i2cr |= (UINT32)0x10;

			// Transmit SCM20014 slave calling addr for read
			i2c_aa_reg->i2dr = (UINT32)msgs[i].addr | (UINT32)0x1;
			// Wait until the I2C interrupt routine wakeup the thread after
			// successfully sending out the register address byte.
			if (i2c_aa_transfer_complete()) {
				i2c_aa_stop();
				i2c_aa_bus_release();
				goto start;
			}
			//interruptible_sleep_on(&aa_wait);
			// Clear the MTX bit in the I2CR to set the Master receive mode.
			i2c_aa_reg->i2cr &= ~(0x10);
			
			// dummy read from I2DR register to generate clk
			i2c_aa_reg->i2cr &= ~ 0x08;				//ack enable

			msgs[i].buf[0] = (UINT8)i2c_aa_reg->i2dr;
			//printk("data %x\n",msgs[i].buf[0]);
			//get MSB, and gen clock for reading LSB
			//i2c_aa_reg->i2cr |= 0x08;			//ack disable

			//according the dsc009 , add generate stop to enter slave mode
			// and poll until bus is release(IBB=0)

			
			// read the (length-1) bytes to the data buffer
			for ( j = 0; j < msgs[i].len; j ++ )
			{
				// Wait until the I2C interrupt routine wakeup the thread 
				if (i2c_aa_transfer_complete()) {
					i2c_aa_stop();
					i2c_aa_bus_release();
					goto start;
				}
				i2c_aa_reg->i2cr |= 0x08;		//ack disable
				//interruptible_sleep_on(&aa_wait);
	
				// read the data from I2DR register to the msgs[i].buf[j] 
				msgs[i].buf[j] = (UINT8)i2c_aa_reg->i2dr;
				//printk("data %x\n",msgs[i].buf[j]);
				if (j < msgs[i].len - 1)
					i2c_aa_reg->i2cr &= ~ 0x08;	//ack enable
			}

			// Wait until the I2C interrupt routine wakeup the thread 
			if (i2c_aa_transfer_complete()) {
				i2c_aa_stop();
				i2c_aa_bus_release();
				goto start;
			}
		} // end of if(READ)
		else
		// Case WRITE
		if(((msgs[i].flags) & (UINT16)I2C_M_WRITE ))
		{
			// Set the MTX[I2CR:4] to enable the master transmit
			i2c_aa_reg->i2cr |= (UINT32)0x10;

			// set the slave address to the I2DR register for WRITE
			i2c_aa_reg->i2dr = msgs[i].addr;
				
			// Wait until the address is sent to the slave,
			// the I2C interrupt routine will wakeup the thread.
			if (i2c_aa_transfer_complete()) {
				i2c_aa_stop();
				i2c_aa_bus_release();
				goto start;
			}
			//interruptible_sleep_on(&aa_wait);

			// write the data to the data buffer
			for ( j = 0; j < msgs[i].len ; j ++ )
			{	
				// Write the data to I2DR register from the msgs[i].buf[j]
				i2c_aa_reg->i2dr = msgs[i].buf[j];
				
				// Wait until the I2C interrupt routine wakeup the thread
				if (i2c_aa_transfer_complete()) {
					i2c_aa_stop();
					i2c_aa_bus_release();
					goto start;
				}
				//interruptible_sleep_on(&aa_wait);
			}
		} // end of I2C_M_WRITE
	
		count += msgs[i].len;
	} // for
	
	// send out the stop sigal to i2c bus
	i2c_aa_stop();
			
	// wait until the bus is released
			 
	i2c_aa_bus_release();
	
	// Clear the I2CR to disable I2C module
	i2c_aa_reg->i2cr &= (UINT32)0x0;
	return count;
}
#else
int i2c_aa_xfer(struct i2c_adapter *i2c_adap, struct i2c_msg msgs[], int num)
{
	short i,j,count = 0;
	FUNC_START;
start:
	// I2C module reset
	i2c_aa_reg->i2cr = (UINT32)0x0;

	// Set the IEN[I2CR:7] to enable the I2C bus interface system
	i2c_aa_reg->i2cr |= (UINT32)0x80;

	// Set clock Freq
	i2c_aa_reg->ifdr = (UINT32)DEFAULT_FREQ;
	
	// Set i2c address
	i2c_aa_reg->iadr = (UINT32)DEFAULT_I2C_ADDRESS;
	
	// Set the TXAK[I2CR:6] to disable the I2C transmit ACK
	i2c_aa_reg->i2cr |= (UINT32)0x08;
	
	for ( i = 0; i < num; i ++)	// for each message
	{
		CSI_LOC;
		// generate a START signal and select the master mode
		// and wait until the bus is not busy
		i2c_aa_start();
		CSI_LOC;
		// if Arbitration lost and bus is idle
		if(i2c_aa_bus_grab())
		{
			CSI_LOC;
			i2c_aa_stop();
			i2c_aa_bus_release();
			goto start;
		}
		CSI_LOC;

		// Set the MTX[I2CR:4] to enable the master transmit
		i2c_aa_reg->i2cr |= (UINT32)0x10;

		// set the slave address to the I2DR register for WRITE
		i2c_aa_reg->i2dr = msgs[i].addr;
				
		// Wait until the address is sent to the slave,
		// the I2C interrupt routine will wakeup the thread.
		i2c_aa_transfer_complete();
		//interruptible_sleep_on(&aa_wait);

		// write the msgs[i].buf[0] (register addr for example: 
		// SCM20014:0x31) to the I2DR register.
		i2c_aa_reg->i2dr = msgs[i].buf[0];
		
		// Wait until the I2C interrupt routine wakeup the thread 
		// after successfully sending out the register address byte.
		i2c_aa_transfer_complete();
		//interruptible_sleep_on(&aa_wait);
		
		// Case READ
		if (((msgs[i].flags) & (UINT16)I2C_M_READ ))
		{
			// set the RSTA[I2CR:2] to generate a repeated START signal
			i2c_aa_repeat_start();
			// Transmit SCM20014 slave calling addr for read
			i2c_aa_reg->i2dr = (UINT32)msgs[i].addr | (UINT32)0x1;
			// Wait until the I2C interrupt routine wakeup the thread after
			// successfully sending out the register address byte.
			i2c_aa_transfer_complete();
			//interruptible_sleep_on(&aa_wait);
			// Clear the MTX bit in the I2CR to set the Master receive mode.
			i2c_aa_reg->i2cr &= ~(0x10);
			
			// dummy read from I2DR register to generate clk
			i2c_aa_reg->i2cr &= ~ 0x08;				//ack enable

			msgs[i].buf[0] = (UINT8)i2c_aa_reg->i2dr;
			//printk("data %x\n",msgs[i].buf[0]);
			//get MSB, and gen clock for reading LSB
			//i2c_aa_reg->i2cr |= 0x08;			//ack disable

			//according the dsc009 , add generate stop to enter slave mode
			// and poll until bus is release(IBB=0)

			
			// read the (length-1) bytes to the data buffer
			for ( j = 0; j < msgs[i].len - 1; j ++ )
			{
				// Wait until the I2C interrupt routine wakeup the thread 
				i2c_aa_transfer_complete();
				i2c_aa_reg->i2cr |= 0x08;		//ack disable
				//interruptible_sleep_on(&aa_wait);
	
				// read the data from I2DR register to the msgs[i].buf[j] 
				msgs[i].buf[j] = (UINT8)i2c_aa_reg->i2dr;
				//printk("data %x\n",msgs[i].buf[j]);
				i2c_aa_reg->i2cr &= ~ 0x08;			//ack enable
			}

			// this is the last byte to read,  The master need un-ack the slave 
			// set the TXAK[I2CR:3] to disable the ack bit

			// Wait until the I2C interrupt routine wakeup the thread 
			i2c_aa_transfer_complete();
			
			// send out the stop sigal to i2c bus
			i2c_aa_stop();

			// wait until the bus is released
			i2c_aa_bus_release();
			
			// read the last byte back
			msgs[i].buf[j] = (UINT8)i2c_aa_reg->i2dr;
			//printk("data %x\n",msgs[i].buf[j]);
		} // end of if(READ)
		else
		// Case WRITE
		if(((msgs[i].flags) & (UINT16)I2C_M_WRITE ))
		{
			// write the data to the data buffer
			for ( j = 1; j < msgs[i].len ; j ++ )
			{	
				// Write the data to I2DR register from the msgs[i].buf[j]
				i2c_aa_reg->i2dr = msgs[i].buf[j];
				
				// Wait until the I2C interrupt routine wakeup the thread
				i2c_aa_transfer_complete();
				//interruptible_sleep_on(&aa_wait);
			}
			// send out the stop sigal to i2c bus
			i2c_aa_stop();
			
			// wait until the bus is released
			 
			i2c_aa_bus_release();
		} // end of I2C_M_WRITE
	
		count += msgs[i].len;
	} // for
	
	
	// Clear the I2CR to disable I2C module
	i2c_aa_reg->i2cr &= (UINT32)0x0;
	return count;
}
#endif
#else

int i2c_aa_xfer(struct i2c_adapter *i2c_adap, struct i2c_msg msgs[], int num)
{
	short i, count = 0;
	unsigned long clock_div=0x17;
	
	for(i=0; i<num; i++){
		if(msgs[i].flags & I2C_S_READ){
			slave_rx(	msgs[i].addr, 
					clock_div, 
					msgs[i].buf, 
					msgs[i].len);
		}

		if(msgs[i].flags & I2C_S_WRITE){
			slave_tx(	msgs[i].addr, 
					clock_div, 
					msgs[i].buf, 
					msgs[i].len);
		}

		if(msgs[i].flags & I2C_M_READ){
			scm_rx(	(UINT32)msgs[i].buf[0], 
					msgs[i].addr, 
					clock_div, 
					msgs[i].buf, 
					msgs[i].len);
				
		}

		if(msgs[i].flags & I2C_M_WT){
			master_tx(	I2C_DRIVERID_I2CCSI, 
					msgs[i].addr, 
					clock_div, 
					msgs[i].buf, 
					msgs[i].len);
		}

		count += msgs[i].len;
	}// for

	/* Clear the I2CR to disable I2C module */
	// TODO: before this, HeYing and ZhiMing disable the i2c
	// after one transfer, but why?	Chen Ning.
	// i2c_aa_reg->i2cr &= (UINT32)0x0;
	SETMASK(I2C_I2CR, 0x0); 
	// printk(KERN_ERR"i2c trasfered %x bytes", count);
	return count;
}
#endif // POLLING_MODE

/*-----------------------------------------------------------------------
 * void i2c_aa_isr (SINT16 irq, void * dev_id, struct pt_regs * reg)
 * This function deals with the interrupt for the I2C module.
 *
 * Parameters:
 * 	irq	the interrupt number
 * 	dev_id	device id
 * 	reg	processor register	
 *
 * Return:
 * 	None
 *----------------------------------------------------------------------*/
#ifdef POLLING_MODE
void i2c_aa_isr (int16_t irq, void * dev_id, struct pt_regs * reg)
{
	FUNC_START;
	printk("get interrupt i2sr = %x \n", i2c_aa_reg->i2sr);
	
#if 0
	/* Clear the IIF[I2SR:1] bit */
	i2c_aa_reg->i2sr &= ~(0x02);
	CSI_LOC;
	CSI_DB("I2SR:%X", (int)i2c_aa_reg->i2sr)
	
	if (i2c_aa_reg->i2sr & 0x80)	/* tansfer complete */
	{
		CSI_LOC;
		CSI_DB("I2SR:%X", (int)i2c_aa_reg->i2sr);
		if (!( i2c_aa_reg->i2sr & 0x01)) /* ACK detected at 9th clock */
		{
			CSI_DB("I2SR:%X", (in/t)i2c_aa_reg->i2sr);
			/* wakeup the thread in the wait queue associated with the I2C module */
			wake_up_interruptible(&aa_wait);
		}
	}
	else if ( i2c_aa_reg->i2sr & 0x10); 	/* IAL[I2SR:4] is set means arbitration lost */
	/* do not deal with the I2C error right now */

	CSI_DB("I2SR:%X", (int)i2c_aa_reg->i2sr);
#endif
	FUNC_END;
	return;
}
#else
// in i2c_rw.c 
#endif // POLLING_MODE

/*-----------------------------------------------------------------------
 * static int i2c_aa_ioctl(struct i2c_adapter * adapter, 
 * 				unsigned int cmd, unsigned long arg)
 * This function control the I2C module itself
 *
 * Parameters :
 * 	Adapter		the adapter associated to the I2C module
 * 	Cmd		IO control command
 * 	Arg		argument associated with the command
 * Return :
 * 	Success		0
 * 	Failure		-1	
 *----------------------------------------------------------------------*/ 
int i2c_aa_ioctl(struct i2c_adapter * adapter, unsigned int cmd, unsigned long arg)
{
	FUNC_START;
	switch(cmd )
	{
		case I2C_IO_CHANGE_FREQ:
			// set the parameter to the IFDR register
			i2c_aa_reg->ifdr = (UINT32)(arg & 0x003F);
			break;

		case I2C_IO_GET_STATUS:
			// Store the value of I2SR to the arg
			arg = i2c_aa_reg->i2sr;
			break;
		
		default:
			break;
	}

	FUNC_END;
	return 0;
}









/*-----------------------------------------------------------------------
 * int16_t __init i2c_aa_init(void)
 * initializes the I2C module in the DBMX1, and registers itself to the 
 * Linux I2C system
 *
 * Parameters:
 * 	None
 * Return:
 * 	0	indicates SUCCESS
 * 	-1	indicates FAILURE
 * --------------------------------------------------------------------*/
int32_t __init i2c_aa_init(void)
{
	int tmp;
	tmp = 0;

	FUNC_START;

	printk(KERN_INFO "Initialize i2c-aa-dbmx module\n");
	
	GPIO_init();	
	
	printk("I2C driver "__DATE__" / "__TIME__"\n");
	printk("Base Addr %p\n", i2c_aa_reg);
#ifdef CONFIG_ARCH_DBMX2
	mx_module_clk_open(IPG_MODULE_I2C);

#endif	
	// Init queue
#if (LINUX_VERSION_CODE >= 0x020301)
	init_waitqueue_head(&aa_wait);
#endif
	// add the I2C adapter/algorithm driver to the linux kernel
	if (i2c_add_adapter(&i2c_csi_adapter))
	{
		CSI_LOC;
		return -1;
	}

#ifndef POLLING_MODE
	// install the I2C_AA ISR to the Linux Kernel
	tmp = request_irq(I2C_AA_IRQ,
		    (void *)i2c_aa_isr,
//		    SA_SHIRQ | SA_INTERRUPT,
			SA_INTERRUPT,
		    "I2C_AA",
		    "i2c_bus");
	if (tmp < 0)
		i2c_del_adapter(&i2c_csi_adapter);


	enable_irq(I2C_AA_IRQ);
	IOW(0x223008, I2C_AA_IRQ); // enable 
#endif
	FUNC_END;
	return tmp;
}



/*-----------------------------------------------------------------------
 * static void __exit i2c_aa_cleanup( void )
 * This routine is called when the driver is unloaded
 *
 * Parameter :
 * 	None
 * Return :
 * 	None
 *----------------------------------------------------------------------*/
static void __exit i2c_aa_cleanup( void )
{
	FUNC_START;
	//???should free_irq() first and del_adapter 
	// call the i2c_del_adapter() to deregister the driver from the
	// Linux I2C system release the I2C_AA ISR handler
#ifndef POLLING_MODE
	free_irq (I2C_AA_IRQ, "i2c_bus");
#endif
	i2c_del_adapter(&i2c_csi_adapter);
	
	FUNC_END;
}


/************************************************************************
 * 	Init Module
 ************************************************************************/ 	
MODULE_AUTHOR("Motorola Inc");
MODULE_DESCRIPTION("i2c-aa-dbmx algorithm/adaptor driver for the Motorola DBMX");
MODULE_LICENSE("GPL");

EXPORT_NO_SYMBOLS;

module_init(i2c_aa_init);
module_exit(i2c_aa_cleanup);
