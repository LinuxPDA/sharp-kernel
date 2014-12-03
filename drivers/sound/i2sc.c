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
 *
 * Change Log
 *	31-Jul-2002 Lineo Japan, Inc.  for ARCH_PXA
 */


/*********************************************

* modify: TX/RX I2C for EBATT DEMO data is ok.

*********************************************/
#include <asm/arch/hardware.h>
#include <linux/sched.h>
#include <linux/interrupt.h>
#include <linux/delay.h>
#include <asm/arch/i2sc.h>

#define AUDIO_DEVICE_ADDR		0x30

#define PRT_I2C_MSG		0	// if 0:no print message , 1:print it

#define printk

unsigned short READ_I2C_WORD(unsigned char byDeviceAddress,unsigned char byRegisterAddress)
{
	unsigned char	byTemp;
	unsigned short	wReturn;
	long	dwTickCount;
	unsigned short   rvalue;

	
	//pI2C_REGS->icr.scle = 1;			// SCL enable
	I2C_ICR |= I2C_ICR_SCLE;
	//pI2C_REGS->icr.iue = 1;			// I2C unit enable
	I2C_ICR |= I2C_ICR_IUE;

	//pI2C_REGS->idbr.data = m_byDeviceID;		// device ID of audio
	I2C_IDBR = byDeviceAddress;

	//pI2C_REGS->icr.aldie = 0;			// disable
	I2C_ICR &= ~I2C_ICR_ALDIE;
	//pI2C_REGS->icr.stop = 0;			// not stop
	I2C_ICR &= ~I2C_ICR_STOP;
	//pI2C_REGS->icr.start = 1;			// send start
	I2C_ICR |= I2C_ICR_START;
	//pI2C_REGS->icr.tb = 1;				// transmit byte
	I2C_ICR |= I2C_ICR_TB;
#if PRT_I2C_MSG
	printk("I2C-r1, write Addr : %x\n",byRegisterAddress);
#endif
	//dwTickCount = GetTickCount();
	dwTickCount = jiffies;
	while (1)
	{
		if (TimeOut(dwTickCount))		// check if timeout
		{
			printk("Time out 1\n");
			break;
		}

		//if ((pI2C_REGS->isr.rwm == 0) && (pI2C_REGS->isr.ub == 1) && (pI2C_REGS->isr.ite == 1))
		if( ((I2C_ISR & I2C_ISR_RWM ) ==0 ) && ( (I2C_ISR & I2C_ISR_UB) == I2C_ISR_UB)&& ( (I2C_ISR & I2C_ISR_ITE)== I2C_ISR_ITE) )
		{
			break;			// ub -> unit busy, ite -> IDBR transmit empty
		}
	}

	//pI2C_REGS->isr.ite = 1;				// clear interrupt
	I2C_ISR = I2C_ISR_ITE;

	//pI2C_REGS->idbr.data = Address;		// send addres
	I2C_IDBR = byRegisterAddress;
	//pI2C_REGS->icr.aldie = 1;			// enable
	I2C_ICR |= I2C_ICR_ALDIE;
	//pI2C_REGS->icr.stop = 0;			// not stop
	I2C_ICR &= ~I2C_ICR_STOP;
	//pI2C_REGS->icr.start = 0;			// not start
	I2C_ICR &= ~I2C_ICR_START;
	//pI2C_REGS->icr.tb = 1;				// transmit byte
	I2C_ICR |= I2C_ICR_TB;

	//dwTickCount = GetTickCount();
	dwTickCount = jiffies;
	while (1)
	{
		if (TimeOut(dwTickCount))		// check if timeout
		{
			printk("Time out 2\n");
			break;
		}

		//if ((pI2C_REGS->isr.rwm == 0) && (pI2C_REGS->isr.ub == 1) && (pI2C_REGS->isr.ite == 1))
		if( ((I2C_ISR & I2C_ISR_RWM) ==0 ) && ( (I2C_ISR & I2C_ISR_UB) == I2C_ISR_UB)&& ( (I2C_ISR & I2C_ISR_ITE)==I2C_ISR_ITE) )
				break;			// ub -> unit busy, ite -> IDBR transmit empty
	}

	//pI2C_REGS->isr.ite = 1;				// clear interrupt
	I2C_ISR = I2C_ISR_ITE;
	/////////////////////////////////////////////
	// begin to read

	//pI2C_REGS->idbr.data = m_byDeviceID | 0x01;		// device ID of audio (R/W bit = 1)
	I2C_IDBR = byDeviceAddress | 0x01;

	//pI2C_REGS->icr.aldie = 0;			// enable
	I2C_ICR &= ~I2C_ICR_ALDIE;
	//pI2C_REGS->icr.start = 1;			// not start
	I2C_ICR |= I2C_ICR_START;
	//pI2C_REGS->icr.stop = 0;			// not send stop
	I2C_ICR &= ~I2C_ICR_STOP;
	//pI2C_REGS->icr.tb = 1;				// transmit byte
	I2C_ICR |= I2C_ICR_TB;

	//dwTickCount = GetTickCount();
	dwTickCount = jiffies;
	while (1)
	{
		if (TimeOut(dwTickCount))		// check if timeout
		{
			printk("Time out 3\n");
			break;
		}

		//if ((pI2C_REGS->isr.rwm == 1) && (pI2C_REGS->isr.ub == 1) && (pI2C_REGS->isr.ite == 1))
		if( ((I2C_ISR & I2C_ISR_RWM) ==I2C_ISR_RWM ) && ( (I2C_ISR & I2C_ISR_UB) == I2C_ISR_UB)&& ( (I2C_ISR & I2C_ISR_ITE)==I2C_ISR_ITE) )
			break;			// ub -> unit busy, ite -> IDBR transmit empty
	}

	//pI2C_REGS->isr.ite = 1;				// clear interrupt
	I2C_ISR = I2C_ISR_ITE;

	//pI2C_REGS->icr.aldie = 1;			// disable
	I2C_ICR |= I2C_ICR_ALDIE;
	//pI2C_REGS->icr.start = 0;			// not start
	I2C_ICR &= ~I2C_ICR_START;
	//pI2C_REGS->icr.stop = 0;			// send stop
	I2C_ICR &= ~I2C_ICR_STOP;
	//pI2C_REGS->icr.ack = 0;			// ACK/NAK
	I2C_ICR &= ~ I2C_ICR_ACK;
	//pI2C_REGS->icr.tb = 1;				// transmit byte
	I2C_ICR |= I2C_ICR_TB;

	//dwTickCount = GetTickCount();
	dwTickCount = jiffies;
	while (1)
	{
		if (TimeOut(dwTickCount))		// check if timeout
		{
			printk("Time out 4\n");
			break;
		}

		//if ((pI2C_REGS->isr.rwm == 1) && (pI2C_REGS->isr.ub == 1) && (pI2C_REGS->isr.irf == 1) && (pI2C_REGS->isr.ack == 0))
		if( ((I2C_ISR & I2C_ISR_RWM) == 1 ) && ( (I2C_ISR & I2C_ISR_UB) == I2C_ISR_UB ) && ( (I2C_ISR & I2C_ISR_IRF) == I2C_ISR_IRF )&& ( (I2C_ISR & I2C_ISR_ACK) == 0x0 ))
			break;			// ub -> unit busy, irf -> IDBR receive full
	}

	//pI2C_REGS->isr.irf = 1;			// clear interrupt
	I2C_ISR = I2C_ICR_IRF;

	//byTemp = pI2C_REGS->idbr.data;
	byTemp = (unsigned char)I2C_IDBR;
	wReturn = byTemp << 8;				// MSB

	//pI2C_REGS->icr.stop = 0;			// send stop
	I2C_ICR &= ~I2C_ICR_STOP;
	//pI2C_REGS->icr.ack = 0;			// ACK/NAK
	I2C_ICR &= ~I2C_ICR_ACK;

	//pI2C_REGS->icr.aldie = 1;			// disable
	I2C_ICR |= I2C_ICR_ALDIE;
	//pI2C_REGS->icr.start = 0;			// not start
	I2C_ICR &= ~I2C_ICR_START;
	//pI2C_REGS->icr.stop = 0;			// send stop
	I2C_ICR &= ~I2C_ICR_STOP;
	//pI2C_REGS->icr.ack = 1;			// ACK/NAK
	I2C_ICR |= I2C_ICR_ACK;
	//pI2C_REGS->icr.tb = 1;				// transmit byte
	I2C_ICR |= I2C_ICR_TB;

	//dwTickCount = GetTickCount();
	dwTickCount = jiffies;
	while (1)
	{
		if (TimeOut(dwTickCount))		// check if timeout
		{
			printk("Time out 5\n");
			break;
		}

		//if ((pI2C_REGS->isr.rwm == 1) && (pI2C_REGS->isr.ub == 1) && (pI2C_REGS->isr.irf == 1) && (pI2C_REGS->isr.ack == 1))
		if( ((I2C_ISR & I2C_ISR_RWM) == 1 ) && ( (I2C_ISR & I2C_ISR_UB) == I2C_ISR_UB ) && ( (I2C_ISR & I2C_ISR_IRF) == I2C_ISR_IRF )&& ( (I2C_ISR & I2C_ISR_ACK) == I2C_ISR_ACK ))
			break;			// ub -> unit busy, irf -> IDBR receive full
	}

	//pI2C_REGS->isr.irf = 1;			// clear interrupt
	I2C_ISR = I2C_ISR_IRF;

	//byTemp = pI2C_REGS->idbr.data;
	byTemp = (unsigned char)I2C_IDBR;
	wReturn |= byTemp;					// LSB

	//pI2C_REGS->icr.ma = 0;				// disable
	I2C_ICR &= ~I2C_ICR_MA;
	
	rvalue = wReturn;
#if PRT_I2C_MSG
	printk(" data %x\n",rvalue);
#endif
}

void Write_I2C_WORD(unsigned char byDeviceAddress,unsigned char byRegisterAddress,unsigned short wData)
{    	  
	long dwTickCount;
	//pI2C_REGS = (PI2C_REGS) I2C_BASE_U_VIRTUAL;
  
	//pI2C_REGS->idbr.data = byDeviceAddress;		// device ID of audio
	
	/*pI2C_REGS->icr.scle = 1;			// SCL enable
	pI2C_REGS->icr.iue = 1;				// I2C unit enable
	pI2C_REGS->icr.tb = 1;				// transmit byte
	pI2C_REGS->icr.stop = 0;			// not stop
	pI2C_REGS->icr.start = 1;			// send start*/
	//I2C_ICR |= 0x0069;
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

	//pI2C_REGS->isr.ite = 1;				// clear interrupt
	I2C_ISR = I2C_ISR_ITE;

	//pI2C_REGS->idbr.data = byRegisterAddress;		// send addres
	I2C_IDBR = byRegisterAddress;	
	//pI2C_REGS->icr.aldie = 1;			// enable
	I2C_ICR |= I2C_ICR_ALDIE ;
	//pI2C_REGS->icr.stop = 0;			// not stop
	I2C_ICR &= ~I2C_ICR_STOP;
	//pI2C_REGS->icr.start = 0;			// not start
	I2C_ICR &= ~I2C_ICR_START;
	//pI2C_REGS->icr.tb = 1;				// transmit byte
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

	//pI2C_REGS->isr.ite = 1;				// clear interrupt
	I2C_ISR = I2C_ISR_ITE;

	//pI2C_REGS->idbr.data = (char) (wData >> 8);	// send data (high byte)
	I2C_IDBR = (unsigned char) (wData >> 8);

	//I2C_REGS->icr.aldie = 1;			// enable
	I2C_ICR |= I2C_ICR_ALDIE;
	//pI2C_REGS->icr.start = 0;			// not start
	I2C_ICR &= ~I2C_ICR_START;
	//pI2C_REGS->icr.stop = 0;			// not send stop
	I2C_ICR &= ~I2C_ICR_STOP;
	//pI2C_REGS->icr.tb = 1;				// transmit byte
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

	//pI2C_REGS->isr.ite = 1;				// clear interrupt
	I2C_ISR = I2C_ISR_ITE;

	//pI2C_REGS->idbr.data = (char) (wData);	// send data (low byte)
	I2C_IDBR = (unsigned char) (wData);	

	//pI2C_REGS->icr.aldie = 1;			// disable
	I2C_ICR |= I2C_ICR_ALDIE;
	//pI2C_REGS->icr.start = 0;			// not start
	I2C_ICR &= ~I2C_ICR_START;
	//pI2C_REGS->icr.stop = 1;			// send stop
	I2C_ICR |= I2C_ICR_STOP;
	//pI2C_REGS->icr.tb = 1;				// transmit byte
	I2C_ICR |= I2C_ICR_TB;

	dwTickCount = jiffies;
	while (1)
	{
		if (TimeOut(dwTickCount))		// check if timeout
		{
			printk("Time out 4\n");
			break;
		}
		//if ((pI2C_REGS->isr.rwm == 0) && (pI2C_REGS->isr.ite == 1))
		if( ((I2C_ISR & I2C_ISR_RWM) == 0 ) && ( (I2C_ISR & I2C_ISR_ITE) == I2C_ISR_ITE ) )
			break;			// ub -> unit busy, ite -> IDBR transmit empty
	}

	//pI2C_REGS->isr.ite = 1;				// clear interrupt
	I2C_ISR = I2C_ISR_ITE;

	//pI2C_REGS->icr.aldie = 0;			// disable
	I2C_ICR &= ~I2C_ICR_ALDIE;
	//pI2C_REGS->icr.tb = 0;				// transmit byte
	I2C_ICR &= ~I2C_ICR_TB;
	//pI2C_REGS->icr.start = 0;			// clear start
	I2C_ICR &= ~I2C_ICR_START;
	//pI2C_REGS->icr.stop = 0;			// clear stop
	I2C_ICR &= ~I2C_ICR_STOP;
                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                               
                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                 
	//pI2C_REGS->icr.iue = 0;				// I2C unit disable
	I2C_ICR &= ~I2C_ICR_IUE;
	//pI2C_REGS->icr.scle = 0;			// SCL disable
	I2C_ICR &= ~I2C_ICR_SCLE;
	
}


EXPORT_SYMBOL(READ_I2C_BYTE);
EXPORT_SYMBOL(WRITE_I2C_BYTE);

