/*
 *  linux/drivers/sound/i2sc.c
 *
 *  Driver for I2c interface on Xscale
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
 */


/*********************************************

* modify: TX/RX I2C for EBATT DEMO data is ok.

*********************************************/
#include <linux/sched.h>
#include <linux/interrupt.h>
#include <linux/delay.h>
#include <asm/arch-cotulla/cotulla.h>
#include <asm/arch/i2sc.h>

#define AUDIO_DEVICE_ADDR		0x30
extern struct semaphore i2c_sem;

#define PRT_I2C_MSG		0	// if 0:no print message , 1:print it

#define printk

unsigned short READ_I2C_WORD(unsigned char byDeviceAddress,unsigned char byRegisterAddress)
{
	unsigned char	byTemp;
	unsigned short	wReturn;
	long	dwTickCount;
	unsigned short   rvalue;

	
 	up(&i2c_sem); 
	I2C_ICR |= I2C_ICR_SCLE;
	I2C_ICR |= I2C_ICR_IUE;

	I2C_IDBR = byDeviceAddress;

	I2C_ICR &= ~I2C_ICR_ALDIE;
	I2C_ICR &= ~I2C_ICR_STOP;
	I2C_ICR |= I2C_ICR_START;
	I2C_ICR |= I2C_ICR_TB;
#if PRT_I2C_MSG
	printk("I2C-r1, write Addr : %x\n",byRegisterAddress);
#endif
	dwTickCount = jiffies;
	while (1)
	{
		if (TimeOut(dwTickCount))		// check if timeout
		{
			printk("Time out 1\n");
			break;
		}

		if( ((I2C_ISR & I2C_ISR_RWM ) ==0 ) && ( (I2C_ISR & I2C_ISR_UB) == I2C_ISR_UB)&& ( (I2C_ISR & I2C_ISR_ITE)== I2C_ISR_ITE) )
		{
			break;			// ub -> unit busy, ite -> IDBR transmit empty
		}
	}

	I2C_ISR = I2C_ISR_ITE;

	I2C_IDBR = byRegisterAddress;
	I2C_ICR |= I2C_ICR_ALDIE;
	I2C_ICR &= ~I2C_ICR_STOP;
	I2C_ICR &= ~I2C_ICR_START;
	I2C_ICR |= I2C_ICR_TB;

	dwTickCount = jiffies;
	while (1)
	{
		if (TimeOut(dwTickCount))		// check if timeout
		{
			printk("Time out 2\n");
			break;
		}

		if( ((I2C_ISR & I2C_ISR_RWM) ==0 ) && ( (I2C_ISR & I2C_ISR_UB) == I2C_ISR_UB)&& ( (I2C_ISR & I2C_ISR_ITE)==I2C_ISR_ITE) )
				break;			// ub -> unit busy, ite -> IDBR transmit empty
	}

	I2C_ISR = I2C_ISR_ITE;
	/////////////////////////////////////////////
	// begin to read

	I2C_IDBR = byDeviceAddress | 0x01;

	I2C_ICR &= ~I2C_ICR_ALDIE;
	I2C_ICR |= I2C_ICR_START;
	I2C_ICR &= ~I2C_ICR_STOP;
	I2C_ICR |= I2C_ICR_TB;

	dwTickCount = jiffies;
	while (1)
	{
		if (TimeOut(dwTickCount))		// check if timeout
		{
			printk("Time out 3\n");
			break;
		}

		if( ((I2C_ISR & I2C_ISR_RWM) ==I2C_ISR_RWM ) && ( (I2C_ISR & I2C_ISR_UB) == I2C_ISR_UB)&& ( (I2C_ISR & I2C_ISR_ITE)==I2C_ISR_ITE) )
			break;			// ub -> unit busy, ite -> IDBR transmit empty
	}

	I2C_ISR = I2C_ISR_ITE;

	I2C_ICR |= I2C_ICR_ALDIE;
	I2C_ICR &= ~I2C_ICR_START;
	I2C_ICR &= ~I2C_ICR_STOP;
	I2C_ICR &= ~ I2C_ICR_ACK;
	I2C_ICR |= I2C_ICR_TB;

	dwTickCount = jiffies;
	while (1)
	{
		if (TimeOut(dwTickCount))		// check if timeout
		{
			printk("Time out 4\n");
			break;
		}

		if( ((I2C_ISR & I2C_ISR_RWM) == 1 ) && ( (I2C_ISR & I2C_ISR_UB) == I2C_ISR_UB ) && ( (I2C_ISR & I2C_ISR_IRF) == I2C_ISR_IRF )&& ( (I2C_ISR & I2C_ISR_ACK) == 0x0 ))
			break;			// ub -> unit busy, irf -> IDBR receive full
	}

	I2C_ISR = I2C_ICR_IRF;

	byTemp = (unsigned char)I2C_IDBR;
	wReturn = byTemp << 8;				// MSB

	I2C_ICR &= ~I2C_ICR_STOP;
	I2C_ICR &= ~I2C_ICR_ACK;

	I2C_ICR |= I2C_ICR_ALDIE;
	I2C_ICR &= ~I2C_ICR_START;
	I2C_ICR &= ~I2C_ICR_STOP;
	I2C_ICR |= I2C_ICR_ACK;
	I2C_ICR |= I2C_ICR_TB;

	dwTickCount = jiffies;
	while (1)
	{
		if (TimeOut(dwTickCount))		// check if timeout
		{
			printk("Time out 5\n");
			break;
		}

		if( ((I2C_ISR & I2C_ISR_RWM) == 1 ) && ( (I2C_ISR & I2C_ISR_UB) == I2C_ISR_UB ) && ( (I2C_ISR & I2C_ISR_IRF) == I2C_ISR_IRF )&& ( (I2C_ISR & I2C_ISR_ACK) == I2C_ISR_ACK ))
			break;			// ub -> unit busy, irf -> IDBR receive full
	}

	I2C_ISR = I2C_ISR_IRF;

	byTemp = (unsigned char)I2C_IDBR;
	wReturn |= byTemp;					// LSB

	I2C_ICR &= ~I2C_ICR_MA;
	
	rvalue = wReturn;
#if PRT_I2C_MSG
	printk(" data %x\n",rvalue);
#endif
	mdelay(1);
 	down(&i2c_sem); 

}

void Write_I2C_WORD(unsigned char byDeviceAddress,unsigned char byRegisterAddress,unsigned short wData)
{    	  
	long dwTickCount;
 
 	up(&i2c_sem); 
	
	I2C_ICR |= I2C_ICR_SCLE;
	I2C_ICR |= I2C_ICR_IUE;
	I2C_IDBR = byDeviceAddress;
#if PRT_I2C_MSG
	printk("Device address = %x \n",I2C_IDBR);
#endif

	//I2C_ICR |= I2C_ICR_IUE;
	I2C_ICR &= ~I2C_ICR_ALDIE;
	I2C_ICR &= ~I2C_ICR_STOP;
	I2C_ICR |= I2C_ICR_START;
	I2C_ICR |= I2C_ICR_TB;		// not stop
	
#if PRT_I2C_MSG	
	printk("I2C-ww1, write Addr : %x, Data : %x\n",byRegisterAddress,wData);
#endif
	dwTickCount = jiffies;
	while (1)
	{
		
		if (TimeOut(dwTickCount))		// check if timeout
		{
			printk("Time out 1\n");
			break;
		}
		if( ((I2C_ISR & I2C_ISR_RWM ) ==0 ) && ( (I2C_ISR & I2C_ISR_UB) == I2C_ISR_UB)&& ( (I2C_ISR & I2C_ISR_ITE)== I2C_ISR_ITE) )
		{
				break;			// ub -> unit busy, ite -> IDBR transmit empty
		}		
	}

	I2C_ISR = I2C_ISR_ITE;

	I2C_IDBR = byRegisterAddress;	
	I2C_ICR |= I2C_ICR_ALDIE ;
	I2C_ICR &= ~I2C_ICR_STOP;
	I2C_ICR &= ~I2C_ICR_START;
	I2C_ICR |= I2C_ICR_TB;

	dwTickCount = jiffies;
	while (1)
	{
		if (TimeOut(dwTickCount))		// check if timeout
		{
			printk("Time out 2\n");
			break;
		}
		if( ((I2C_ISR & I2C_ISR_RWM) ==0 ) && ( (I2C_ISR & I2C_ISR_UB) == I2C_ISR_UB)&& ( (I2C_ISR & I2C_ISR_ITE)==I2C_ISR_ITE) )
			break;			// ub -> unit busy, ite -> IDBR transmit empty
	}

	I2C_ISR = I2C_ISR_ITE;

	I2C_IDBR = (unsigned char) (wData >> 8);

	I2C_ICR |= I2C_ICR_ALDIE;
	I2C_ICR &= ~I2C_ICR_START;
	I2C_ICR &= ~I2C_ICR_STOP;
	I2C_ICR |= I2C_ICR_TB;

	dwTickCount = jiffies;
	while (1)
	{
		if (TimeOut(dwTickCount))		// check if timeout
		{
			printk("Time out 3\n");
			break;
		}
		if( ((I2C_ISR & I2C_ISR_RWM) ==0 ) && ( (I2C_ISR & I2C_ISR_UB) == I2C_ISR_UB)&& ( (I2C_ISR & I2C_ISR_ITE)==I2C_ISR_ITE) )
			break;			// ub -> unit busy, ite -> IDBR transmit empty
	}

	I2C_ISR = I2C_ISR_ITE;

	I2C_IDBR = (unsigned char) (wData);	

	I2C_ICR |= I2C_ICR_ALDIE;
	I2C_ICR &= ~I2C_ICR_START;
	I2C_ICR |= I2C_ICR_STOP;
	I2C_ICR |= I2C_ICR_TB;

	dwTickCount = jiffies;
	while (1)
	{
		if (TimeOut(dwTickCount))		// check if timeout
		{
			printk("Time out 4\n");
			break;
		}
		if( ((I2C_ISR & I2C_ISR_RWM) == 0 ) && ( (I2C_ISR & I2C_ISR_ITE) == I2C_ISR_ITE ) )
			break;			// ub -> unit busy, ite -> IDBR transmit empty
	}

	I2C_ISR = I2C_ISR_ITE;

	I2C_ICR &= ~I2C_ICR_ALDIE;
	I2C_ICR &= ~I2C_ICR_TB;
	I2C_ICR &= ~I2C_ICR_START;
	I2C_ICR &= ~I2C_ICR_STOP;
                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                               
                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                 
	I2C_ICR &= ~I2C_ICR_IUE;
	I2C_ICR &= ~I2C_ICR_SCLE;

	mdelay(1);
 	down(&i2c_sem); 
	
}


EXPORT_SYMBOL(READ_I2C_BYTE);
EXPORT_SYMBOL(WRITE_I2C_BYTE);

