/*
 * linux/drivers/char/mx2-digi.c
 *
 * Copyright (C) 2003 Lineo Solutions, Inc.
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file "COPYING" in the main directory of this archive
 * for more details.
 * 
 * Based on:
 * Motorola i.MX21ADS BSP elinux v1.0.2
 * mx21_rel_beta_1.0.2/Drivers/digi/source/digi.c
 */
/*************************************************************************

Title:   	
Filename:   $Header: /home/cvs/devel/ulinux/kernel/linux-2.4/drivers/char/Attic/mx2-digi.c,v 1.1.2.9 2004/02/20 08:36:39 hide Exp $
Hardware:   MX21
Summay:

License:    The contents of this file are subject to the Mozilla Public
            License Version 1.1 (the "License"); you may not use this file
            except in compliance with the License. You may obtain a copy of
            the License at http://www.mozilla.org/MPL/

Author:   
Company:    Motorola Suzhou Design Center

====================Change Log========================
$Log: mx2-digi.c,v $
Revision 1.1.2.9  2004/02/20 08:36:39  hide
mx2ads-20040220.diff

Revision 1.1.2.8  2004/02/18 06:57:25  hide
mx2ads-20040218.diff


********************************************************************************/
///@file  digi.c
///@brief This is the digi driver source code
///@author Karen Kang
///@bug
///@verstion $Version$

#ifndef __KERNEL__
#  define __KERNEL__
#endif

#include <linux/config.h>
#include <linux/module.h>
#include <linux/version.h>
#include <linux/init.h>
#include <linux/fs.h>
#include <linux/delay.h>
#include <linux/poll.h>
#include <asm/uaccess.h>        
#include <linux/miscdevice.h>
#include <linux/string.h>
#include <linux/kernel.h>
#include <linux/sched.h>
#include <linux/types.h>
#include <linux/fcntl.h>
#include <linux/interrupt.h>
#include <linux/ptrace.h>
#include <linux/ioport.h>
#include <linux/in.h>
#include <linux/slab.h>
#include <linux/vmalloc.h>
#include <linux/string.h>
#include <linux/init.h>
#include <asm/bitops.h>
#include <asm/io.h>
#include <linux/errno.h>
#include <linux/tqueue.h>
#include <linux/wait.h>

#include <asm/irq.h>
#include <asm/arch/hardware.h>
#include <asm/arch/irqs.h>
#include <linux/pm.h>

#include "mx2-digi.h"

#ifdef CONFIG_ARCH_DBMX2
#include <asm/arch/mx2.h>
#endif

#define MODULE_NAME "ts"

//#define DBMX_DEBUG 1
#ifdef DBMX_DEBUG
#define TRACE(fmt, args...) \
	{ \
		printk("\n %s:%d:%s:",__FILE__, __LINE__,__FUNCTION__); \
		printk(fmt, ## args);\
	}
#else
#define TRACE(fmt, args...)
#endif

#define FAILED(fmt, args...) \
	{ \
		printk("\n %s:%d:%s:",__FILE__, __LINE__,__FUNCTION__); \
		printk(fmt, ## args);\
	}

#define INFO(fmt, args...) \
	{ \
		printk("\n"); \
		printk(fmt, ## args);\
	}


static int digi_open(struct inode * inode, struct file * filp);
static int digi_release(struct inode * inode, struct file * filp);
static int digi_fasync(int fd, struct file *filp, int mode);
static int digi_ioctl(struct inode * inode,struct file *filp,unsigned int cmd,unsigned long arg);
static ssize_t digi_read(struct file * filp, char * buf, size_t count, loff_t * l);
static unsigned int digi_poll(struct file * filp, struct poll_table_struct * wait);

static int check_device(struct inode *pInode);
static ts_event_t* get_data(void);
static void spi_tx_data(u16 data);
static int get_block(ts_event_t* * block, int size);
static void  digi_sam_callback(unsigned long data);

static void add_x_y(unsigned x, unsigned y, unsigned flag);
static int init_buf(void);
static void spi_flush_fifo(void);

#ifdef CONFIG_ARCH_DBMX2
static int spi_tx_fifo_empty(void);
static int spi_rx_fifo_data_ready(void);
static unsigned int spi_exchange_data(unsigned int dataTx);
#endif

static int	g_digi_major=241;
static int	result=0;
//static struct proc_dir_entry * g_proc_dir;
static devfs_handle_t g_devfs_handle;
static wait_queue_head_t digi_wait;
struct fasync_struct *ts_fasync;
static u16 	rptr, wptr;
static struct timer_list pen_timer;	
u8 pen_timer_status;	
spinlock_t pen_lock;
static struct tasklet_struct	digi_tasklet; 

static u32 lastX, lastY;

struct pm_dev *g_digi_pm;
static int g_Digi_Status;
#define DIGI_OPEN_STATUS		0x0001
#define DIGI_SUSPEND_STATUS		0x0002


struct file_operations g_digi_fops = 
		{
		open:		digi_open,
		release:	digi_release,
		read:		digi_read,
		poll:		digi_poll,
		ioctl:		digi_ioctl,
		fasync:		digi_fasync,
		};                       


#define NODATA()	(rptr==wptr)
/**
*Misc functions for touch pannel
*/
static void touch_pan_enable(void)
{
#ifdef CONFIG_ARCH_DBMX2
	//set cspi1_ss0 to be low effective 
	//cspi1_ss0 --pd28, it connect with pen_cs_b
	_reg_GPIO_DR(GPIOD) &= (~0x10000000);
#endif
#ifdef CONFIG_ARCH_MX1ADS
	REG_PC_DR &= (~0x00008000);	
#endif	
}

static void touch_pan_disable(void)
{
#ifdef CONFIG_ARCH_DBMX2
	//set the cspi1_ss0 to be high 
	_reg_GPIO_DR(GPIOD) |= 0x10000000;
#endif
#ifdef CONFIG_ARCH_MX1ADS
	//set the output [15] to be high 
	REG_PC_DR |= 0x00008000;
#endif	
}

static void touch_pan_set_inter(void)
{
	int temp;
#ifdef CONFIG_ARCH_DBMX2
	//pe10, uart3_cts pin connect to penIrq
	_reg_GPIO_GIUS(GPIOE) |= 0x00000400;
	_reg_GPIO_OCR1(GPIOE) |= 0x00300000; //new
	_reg_GPIO_IMR(GPIOE)  &= ~(0x00000400);
	_reg_GPIO_DDIR(GPIOE) &= ~(0x00000400);
	temp = _reg_GPIO_SSR(GPIOE);
#endif
#ifdef CONFIG_ARCH_MX1ADS
	//PenIrq is connected to PD31, 
	//first set bit31 of PortD GPIO GIUS_D
	REG_PD_GIUS |= 0x80000000;
	//mask the Pen interrupt in PD[31]
	REG_PD_IMR &= TOUCH_INT_MASK;	
	//set it to be input
	REG_PD_DDIR &= 0x7fffffff;
	REG_PD_ICR2 &= 0x3fffffff;
	temp = REG_PD_SSR;
#endif	
	
}

static void touch_pan_enable_inter(void)
{
#ifdef CONFIG_ARCH_DBMX2
	_reg_GPIO_IMR(GPIOE) |= 0x00000400;	
#endif
#ifdef CONFIG_ARCH_MX1ADS
	//unmask the Pen interrupt in PD[31]
	REG_PD_IMR |= ~TOUCH_INT_MASK;
#endif	
}

static void touch_pan_disable_inter(void)
{
#ifdef CONFIG_ARCH_DBMX2
	_reg_GPIO_IMR(GPIOE) &= ~(0x00000400);
#endif
#ifdef CONFIG_ARCH_MX1ADS
	//mask the Pen interrupt in PD[31]
	REG_PD_IMR &= TOUCH_INT_MASK;	
#endif	

}

static void touch_pan_clear_inter(void)
{
#ifdef CONFIG_ARCH_DBMX2
	_reg_GPIO_ISR(GPIOE) = 0x00000400;
#endif
#ifdef CONFIG_ARCH_MX1ADS
	REG_PD_ISR = ~TOUCH_INT_MASK;
#endif	
}

static void touch_pan_set_pos_inter(void)
{
#ifdef CONFIG_ARCH_DBMX2
	_reg_GPIO_ICR1(GPIOE) &= 0xffcfffff;
#endif
#ifdef CONFIG_ARCH_MX1ADS
	//here set it to be positive edge sensitive
	REG_PD_ICR2 &= 0x3fffffff;
#endif	
}

//here set it to be Negative level sensitive
static void touch_pan_set_neg_inter(void)
{
#ifdef CONFIG_ARCH_DBMX2
	_reg_GPIO_ICR1(GPIOE) &= 0xffcfffff;
	_reg_GPIO_ICR1(GPIOE) |= 0x00100000;
#endif
#ifdef CONFIG_ARCH_MX1ADS
	REG_PD_ICR2 &= 0x7fffffff;
	REG_PD_ICR2 |= 0x40000000; //to be 01
#endif
}

//here check if it is positive level sensitive
static u8 touch_pan_check_int_pol(void)
{
#ifdef CONFIG_ARCH_DBMX2
	if((_reg_GPIO_ICR1(GPIOE) & 0x00300000) == 0x00100000)
	return 0;
	else
	return 1;
#endif
#ifdef CONFIG_ARCH_MX1ADS
	if((REG_PD_ICR2 & 0xC0000000) == TOUCH_INT_NEG_POLARITY)
	return 0;
	else
	return 1;
#endif
}

void touch_pan_init(void)//chip select for ADC
{
#ifdef CONFIG_ARCH_DBMX2
	//cspi1_ss0 --pd28, it connect with pen_cs_b
	_reg_GPIO_GIUS(GPIOD) |= 0x10000000;
	_reg_GPIO_OCR2(GPIOD) |= 0x03000000;
	_reg_GPIO_DR(GPIOD) |= 0x10000000;	
	_reg_GPIO_DDIR(GPIOD) |= 0x10000000;
#endif
#ifdef CONFIG_ARCH_MX1ADS
	//Here should also configure the Touch pannel select pin
	//Port C 15, it should be configured to be GPIO function
	//first set GIUS to be 1 to select GPIO signal
	REG_PC_GIUS |= 0x00008000;
	//then set the OCR1 to be 11 to use data register
	REG_PC_OCR1 |= 0xc0000000;
	//SPI1_SS to be output bit 15
	REG_PC_DDIR |= 0x00008000;
	//set PUEN to be 1 to set the pin is pulled high
	REG_PC_PUEN |= 0x00008000;
	//set the output [15] to be high disable
	REG_PC_DR |= 0x00008000;	
#endif
}

//read data from Touch Pannel
void touch_pan_read_dev(u32* x, u32* y)
{
	u32 x_upper, x_lower, y_upper, y_lower;
	u32 temp;
#ifdef CONFIG_ARCH_DBMX2
	temp = _reg_GPIO_ISR(GPIOE);
#endif
#ifdef CONFIG_ARCH_MX1ADS
	temp = REG_PD_ISR;
#endif

	touch_pan_disable_inter();
	touch_pan_enable();
#ifdef CONFIG_ARCH_DBMX2	
	x_upper = spi_exchange_data(0xD0);	// this is dummy data
	x_upper = spi_exchange_data(0x00);
	x_lower = spi_exchange_data(0x90);
	y_upper = spi_exchange_data(0x00);
	y_lower = spi_exchange_data(0x00);
#endif
#ifdef CONFIG_ARCH_MX1ADS
	spi_tx_data(0xD0);
	x_upper = REG_SPI_RXDATA;// this should be the dummy data
	spi_tx_data(0x00);
	x_upper =  REG_SPI_RXDATA;
	spi_tx_data(0x90);
	x_lower = REG_SPI_RXDATA;
	spi_tx_data(0x00);
	y_upper = REG_SPI_RXDATA;
	spi_tx_data(0x00);
	y_lower = REG_SPI_RXDATA;
#endif	
	
	*x=( ((x_upper<<5) & 0xFFE0) | ((x_lower>>3) & 0x1F) );
	*y=( ((y_upper<<5) & 0xFFE0) | ((y_lower>>3) & 0x1F) );
	touch_pan_disable();
	TRACE("x=%x ,y=%x \n",*x,*y);
#ifdef CONFIG_ARCH_DBMX2
	_reg_GPIO_ISR(GPIOE) = temp;
#endif
#ifdef CONFIG_ARCH_MX1ADS
	REG_SPI_CONTRL &= (~SPI_EN_BIT);//disable SPI
	REG_PD_ISR = temp;
	touch_pan_enable_inter();
#endif
	
}
void touch_pan_read_data(u32* x,u32* y)
{
#define NUM_OF_SAMPLE  (4)
#define MAX_POS_DIFF   (4)
	u32 xPos[NUM_OF_SAMPLE], yPos[NUM_OF_SAMPLE];

	touch_pan_read_dev(&xPos[0], &yPos[0]);
	if ((xPos[0]<100)||(xPos[0]>5000) || (yPos[0]<100)||(yPos[0]>5000)) {
		*x = TPNL_PEN_UPVALUE;
		*y = TPNL_PEN_UPVALUE;
		return;
	}
	touch_pan_read_dev(&xPos[1], &yPos[1]);
	if ((xPos[1]<100)||(xPos[1]>5000) || (yPos[1]<100)||(yPos[1]>5000)) {
		*x = TPNL_PEN_UPVALUE;
		*y = TPNL_PEN_UPVALUE;
		return;
	}

	touch_pan_read_dev(&xPos[2], &yPos[2]);
    if ((xPos[2]<100)||(xPos[2]>5000) ||
        (yPos[2]<100)||(yPos[2]>5000))
    {
        *x = TPNL_PEN_UPVALUE;
        *y = TPNL_PEN_UPVALUE;
        return;
    }
	touch_pan_read_dev(&xPos[3], &yPos[3]);
    if ((xPos[3]<100)||(xPos[3]>5000) ||
        (yPos[3]<100)||(yPos[3]>5000))
    {
        *x = TPNL_PEN_UPVALUE;
        *y = TPNL_PEN_UPVALUE;
        return;
    }

	*x = (xPos[0] + xPos[1] + xPos[2] + xPos[3])>>2;
	*y = (yPos[0] + yPos[1] + yPos[2] + yPos[3])>>2;


	return;
}

/**
 * Function Name: digi_isr
 *
 * Input: filp	:
 * 		  wait	: 
 * Value Returned:
 *
 * Description: support touch pannel interrupt,since MX1 and MX21 adopted different ADC chip,in MX1,AD7843 will
 *				generate interrupt according to level trigger configuration, so can generate one interrupt to 
 *				detect pen down, then reconfigure it to generate pen up interrupt.But in MX21, the AD7873 will
 *				have the pen irq line reassert after each pen sample is taken. It will deassert when you take a
 *				sample. When the pen is down, we disable the interrupt and take a sample every 20ms. Before we 
 *				take the sample, we check if the pen irq line is deasserted. If it is deasserted, then this means 
 *				the pen is up.
 */
static void 
digi_isr(int irq, void *dev_id, struct pt_regs *regs)
{
	//judge if it is the pen interrupt
#ifdef CONFIG_ARCH_MX1ADS
	if(((REG_PD_ISR&REG_PD_IMR)&(~TOUCH_INT_MASK))!=0)
	{
//		FAILED("Pen touch IRQ\n");
		touch_pan_clear_inter(); 
		tasklet_schedule(&digi_tasklet);
	}	
#endif	
#ifdef CONFIG_ARCH_DBMX2

	if(((_reg_GPIO_ISR(GPIOE) & _reg_GPIO_IMR(GPIOE))&0x00000400)!=0)
	{
		touch_pan_clear_inter(); 
		if ((_reg_GPIO_IMR(GPIOE) & 0x00000400) == 0)
			return;
		touch_pan_disable_inter();
//		pen_timer.expires = jiffies + HZ/50 + 2*HZ/100;
		pen_timer.expires = jiffies + HZ/10000;
		add_timer(&pen_timer);
		pen_timer_status = 1;
	}	
#endif
	return;
}
/**
*	Function Name: digi_tasklet_action
*	Input		 : data
*	Output		 : none
*	Description  : The tasklet function used in MX1.
*	
*
*/
static void digi_tasklet_action(unsigned long data)
{	
#ifdef CONFIG_ARCH_MX1ADS
	u32  newX;
	u32  newY;
	u32  maxDelay;
	newX = TPNL_PEN_UPVALUE;
	maxDelay = 0;
//	save_flags(touchflags);
//	cli();
	
	//check the interrupt polarity to see it is pen up or down.
	if(touch_pan_check_int_pol())//it is pen down
	{
		if((REG_PD_SSR & 0x80000000) == 0)//judge if is pen down
		{
			return;			
		}
		//then just change the interrupt polarity to be pen up
		touch_pan_set_neg_inter();
		//check if the pen sampling timer is in use
		if(pen_timer_status!=0)//in use
		{
			//then just change the interrupt polarity to be pen up
			return;
		}
		else //no sampling timer
		{
			touch_pan_disable_inter();
			//read data with MAX delay from ADC
			touch_pan_read_data(&newX, &newY);

			while((newX == TPNL_PEN_UPVALUE)&&(maxDelay <= 100))
			{
				touch_pan_read_data(&newX, &newY);
				maxDelay++;
			}
			if(newX == TPNL_PEN_UPVALUE)
			{
				//clear the interrupt bit
//				FAILED("TouchPanIsr: error always PEN_UP \n");
				return;
		   	}

			TRACE("TouchPanIsr:get pendata x= %d, y=%d \n",newX,newY);
			add_x_y(newX, newY, PENDOWN);

			wake_up_interruptible(&digi_wait);
	
			if(ts_fasync)
			kill_fasync(&ts_fasync, SIGIO, POLL_IN);


			//then just change the interrupt polarity to be pen up
			//then just change the interrupt polarity to be pen up
			touch_pan_set_neg_inter();

			spin_lock_irq(&pen_lock);
			pen_timer.expires = jiffies + HZ/50 + 2*HZ/100;
			add_timer(&pen_timer);
			pen_timer_status = 1;
			spin_unlock_irq (&pen_lock);

			return;
		}
	}
	else//pen up interrupt
	{
		if((REG_PD_SSR & 0x80000000) != 0) //value is high
		{
			return;			
		}
		//check if the pen sampling timer is in use
		if(pen_timer_status!=0)//in use
		{
			//disable pen down interrupt
			//get Pen up
			TRACE("TouchPanIsr:get pen up 0,0,penup \n");
			add_x_y(0, 0, PENUP);

		// if there is a pen up, shall tell apps to handle the event, and it shall
		// be data there
			if (!NODATA())
			{
				wake_up_interruptible(&digi_wait);
				if(ts_fasync)
				kill_fasync(&ts_fasync, SIGIO, POLL_IN);
			}

			//release timer
			spin_lock_irq(&pen_lock);
			pen_timer_status = 0;
			del_timer(&pen_timer);
			spin_unlock_irq(&pen_lock);
			
			touch_pan_set_pos_inter();//set polarity to detach pen down
		}
		else //no timer
		{
//			FAILED("TouchPanIsr :pen up, no timer \n");
		}
		return;
		
	}
#endif
	
}

/******************************************************************************
 * Function Name: digi_sam_callback
 *
 * Input: 		:
 * * Value Returned:
 *
 * Description: spi sampling timer call back
 *
 *****************************************************************************/
static void digi_sam_callback(unsigned long data)
{
	u32 newX,newY;
	u32  maxDelay;
	maxDelay = 0;
	newX = TPNL_PEN_UPVALUE;
//	save_flags(samflags);
//	cli();

	if(pen_timer_status !=0)
		mod_timer(&pen_timer, jiffies + HZ/50 + 2*HZ/100);
#ifdef CONFIG_ARCH_DBMX2
	if((_reg_GPIO_SSR(GPIOE) & 0x00000400) != 0)//judge if is pen down
#endif
#ifdef CONFIG_ARCH_MX1ADS
	if((REG_PD_SSR & 0x80000000) == 0)//judge if is pen down
#endif	
	{
		if(pen_timer_status!=0)//in use
		{
			//get Pen up
			add_x_y(lastX, lastY, PENUP);
			// if there is a pen up, shall tell apps to handle the event, and it shall be data there
			if (!NODATA())
			{
				wake_up_interruptible(&digi_wait);
				if(ts_fasync)
				kill_fasync(&ts_fasync, SIGIO, POLL_IN);
			}
			
			//release timer
			spin_lock_irq(&pen_lock);
			pen_timer_status = 0;
			del_timer(&pen_timer);
			spin_unlock_irq(&pen_lock);

#ifdef CONFIG_ARCH_DBMX2
			touch_pan_set_neg_inter();
#endif
#ifdef CONFIG_ARCH_MX1ADS
			touch_pan_set_pos_inter();//set polarity to detach pen down
#endif
			touch_pan_clear_inter();
			touch_pan_enable_inter();
		}
		return;
	}
#ifdef CONFIG_ARCH_DBMX2
	if((_reg_GPIO_SSR(GPIOE) & 0x00000400) == 0)//positive,it is pen down
#endif
#ifdef CONFIG_ARCH_MX1ADS
	if(!touch_pan_check_int_pol())//positive,it is pen down
#endif
	{
		//read data with MAX delay from ADC
		while((newX == TPNL_PEN_UPVALUE)&&(maxDelay <= 100))
		{
			touch_pan_read_data(&newX, &newY);
			maxDelay++;
		}
		if(newX == TPNL_PEN_UPVALUE)
		{
//			FAILED("SpiSamCallback: error always PEN_UP \n");
			return;
		}
		//add element into buffer
		add_x_y(newX, newY, PENDOWN);
		lastX = newX;
		lastY = newY;

		wake_up_interruptible(&digi_wait);
		
		if(ts_fasync)
			kill_fasync(&ts_fasync, SIGIO, POLL_IN);
		return;	
	}
	else
	{
		FAILED(" error to call me \n");
		return;
	}

}
/************************************************
Misc functions for SPI 
*************************************************/
void spi_init(void)
{
#ifdef CONFIG_ARCH_DBMX2
	int datarate;
	int cs;
//	datarate = 0x7;//512
	datarate = 0x8;//1024
	cs = 0x0;//ss0
//	cs = 0x1;//ss1	
	//cspi1_mosi--pd31 output
	//cspi1_miso--pd30 input
	//cspi1_sclk--pd29 output
	//cspi1_ss0 --pd28,output it connect with pen_cs_b
	_reg_GPIO_GIUS(GPIOD) &= 0x0fffffff;
	_reg_GPIO_GPR(GPIOD) &= 0x0fffffff;
	_reg_GPIO_DDIR(GPIOD) |= 0xb0000000;
	_reg_GPIO_DDIR(GPIOD) &= 0xbfffffff;
	
	//reset spi1
	_reg_CSPI_RESETREG(1) = 0x00000001;
	udelay(200);//wait
	// use 32.768kHz
	_reg_CSPI_PERIODREG(1) = 0x00008000;
	//bit19,18 CS[1:0], bit 17~14 datarate[3:0]
	_reg_CSPI_CONTROLREG(1) |= (cs<<18);
	_reg_CSPI_CONTROLREG(1) |= (datarate<<14);
	_reg_CSPI_CONTROLREG(1) |= 0x00000c07;//ss low active, 8 bit transfer,master mode
	
	_reg_CRM_PCCR0 |= 0x00000010;
#endif
#ifdef CONFIG_ARCH_MX1ADS
	//Port C 13,14, 15, 16, 17
	//first clear bit in GIUS register, then clear bit in GPR register
	REG_PC_GIUS &= 0xfffc1fff;
	REG_PC_GPR &= 0xfffc1fff;
	//SPI1_SCLK to be output bit 14
	//SPI1_SS to be output bit 15
	//SPI_MISO to be input bit 16
	//SPI_MOSI to be output bit 17
	REG_PC_DDIR |= 0x0002c000;
	REG_PC_DDIR &= 0xfffeffff;
	//Reset SPI 1 
	REG_SPI_SOFTRESET = 0x00000001;
	for(i=0;i<4000;i++);//loop
	REG_SPI_CONTRL = 0x0000E607;//data rate be divide by 512,bit_count be 8 bit transfer
#endif	 
}
//To polling when Spi transfer is not finished
static void spi_poll_done(void)
{
#ifdef CONFIG_ARCH_DBMX2
	//here should poll the IRQ bit in IRQ reg to see 
	while(!(_reg_CSPI_INTREG(1) & SPI_TE_INT_BIT));//transfer FIFO is empty
	/* Reset SPI IRQ bit */
	_reg_CSPI_INTREG(1) &= SPI_TE_INT_BIT;
	//wait until XCH bit is clear, hence exchange is complete
	while(!(_reg_CSPI_CONTROLREG(1) & SPI_XCH_BIT));
#endif
#ifdef CONFIG_ARCH_MX1ADS
	//here should poll the IRQ bit in IRQ reg to see 
	while(!(REG_SPI_INTER & SPI_TE_INT));//transfer FIFO is empty
	/* Reset SPI IRQ bit */
	REG_SPI_INTER &= SPI_TE_INT;
	//wait until XCH bit is clear, hence exchange is complete
	while(!(REG_SPI_CONTRL & SPI_XCH_BIT));
#endif	
}

static void spi_tx_data(u16 data)
{
	/* ensure data will not be transmitted while writing to SPI */
#ifdef CONFIG_ARCH_DBMX2    
/*
 	while(_reg_CSPI_INTREG(1) & SPI_TE_INT_BIT == 0);
 	_reg_CSPI_TXDATAREG(1)  = data;			//transfer data
	_reg_CSPI_CONTROLREG(1) |= SPI_XCH_BIT;		// exchange data
	while(_reg_CSPI_INTREG(1) & SPI_TSHFE_INT_BIT == 0);
	while(_reg_CSPI_INTREG(1) & SPI_RR_INT_BIT);
*/
 
 
	_reg_CSPI_CONTROLREG(1) &= SPI_XCH_MASK;
	_reg_CSPI_CONTROLREG(1) |= SPI_EN_BIT;
	_reg_CSPI_TXDATAREG(1)  = (u32)data;//transfer data
	_reg_CSPI_CONTROLREG(1) |= SPI_XCH_BIT;
	spi_poll_done();
	_reg_CSPI_CONTROLREG(1) &= SPI_XCH_MASK;
#endif
#ifdef CONFIG_ARCH_MX1ADS
   	REG_SPI_CONTRL &= SPI_XCH_MASK;
	
	REG_SPI_CONTRL |= SPI_EN_BIT;
    
	REG_SPI_TXDATA  = data;//transfer data

	/* exchange data */
	REG_SPI_CONTRL |= SPI_XCH_BIT;
	spi_poll_done();
   	REG_SPI_CONTRL &= SPI_XCH_MASK;
#endif   	
}

#ifdef CONFIG_ARCH_DBMX2
static int spi_tx_fifo_empty(void)
{
	return (_reg_CSPI_INTREG(1) & SPI_TE_INT_BIT);
}


static int spi_rx_fifo_data_ready(void)
{
	return (_reg_CSPI_INTREG(1) & SPI_RR_INT_BIT);
}

static unsigned int spi_exchange_data(unsigned int dataTx)
{
	while(!spi_tx_fifo_empty());

//	spi_tx_data(dataTx);

	_reg_CSPI_TXDATAREG(1)  = dataTx;		//transfer data
	_reg_CSPI_CONTROLREG(1) |= SPI_XCH_BIT;		// exchange data
    
	while(!spi_rx_fifo_data_ready());
    
	return _reg_CSPI_RXDATAREG(1);
}
	
#endif

/******************************************************************************
 * Function Name: check_device
 *
 * Input: 		inode	:
 * Value Returned:	int	: Return status.If no error, return 0.
 *
 * Description: verify if the inode is a correct inode
 *
 *****************************************************************************/
static int check_device(struct inode *pInode)
{
	int minor;
	kdev_t dev = pInode->i_rdev;
	
	if( MAJOR(dev) != g_digi_major)
	{
//		printk("DIGI: check_device bad major = %d\n",MAJOR(dev) );
		return -1;
	}
	minor = MINOR(dev);
	
	if ( minor < MAX_ID )
		return minor;
	else
	{       
//		printk("DIGI: check_device bad minor = %d\n",minor );
		return -1;
	}
}

/**************************************************
Misc functions for buffer
***************************************************/
/* local buffer for store data
 * a new feature is read block, assume the buffer is a continuous buffer
 * so, if there is enough data, read data from current position to 
 * buffer end, then next time, from buffer head to data end. -- the buffer
 * is a loop buffer.
 */
ts_event_t * buffer;
#define BUFLEN		250
#define BUFSIZE 	(BUFLEN*sizeof(ts_event_t))
#define NEXTI(i)	{i=(i==BUFLEN-1)?0:i+1;}
#define GETNEXTI(i)	((i==BUFLEN-1)?0:i+1)
#define BLKEND(i)	((i)==0)

#if 1
#define DSIZE()		((wptr<rptr)?(BUFLEN-rptr):(wptr-rptr))
#else
#define DSIZE()		((wptr<rptr)?(BUFLEN-rptr+wptr):(wptr-rptr))
#endif


void spi_flush_fifo(void)
{
	u32 i,j;
	for(i=0;i<8;i++)
	{
#ifdef CONFIG_ARCH_DBMX2
		if(_reg_CSPI_INTREG(1)&SPI_RR_INT_BIT)
		j = _reg_CSPI_RXDATAREG(1);
#endif
#ifdef CONFIG_ARCH_MX1ADS
		if(REG_SPI_INTER&SPI_RR_INT)
		j = REG_SPI_RXDATA;
#endif
	}
}
/******************************************************************************
 * Function Name: init_buf 
 *
 * Input: 		void	:
 * Value Returned:	int	: Return status.If no error, return 0.
 *
 * Description: init the loop buffer for store data read out from spi FIFO
 * 	and shall be copied to user
 *
 *****************************************************************************/
static int init_buf(void)
{
	buffer = (ts_event_t*) vmalloc(BUFSIZE);	
	if(!buffer){
//		printk(KERN_ERR"no enough kernel memory for spi data buffer\n");
		return -1;
	}

	rptr = wptr = 0;
	return 1;
}

/******************************************************************************
 * Function Name: add_x_y
 *
 * Input: 		i	:
 * Value Returned:	void	: 
 *
 * Description: add pen data to buffer
 *
 *****************************************************************************/
static void add_x_y(unsigned x, unsigned y, unsigned flag)
{
	
	if (GETNEXTI(wptr) == rptr)
		return;
	buffer[wptr].x = x;
	buffer[wptr].y = y;

	buffer[wptr].pressure = flag;

	NEXTI(wptr);	// goto next wptr
}

/******************************************************************************
 * Function Name: get_block
 *
 * Input: 		block:
 				size:
 * Value Returned:	int	: count of data size
 *
 * Description: read a block of 'touch' data from buffer, read count shall less
 *	than size,but shall be as more as possible. the data shall not really copy
 *  to upper layer,untill copy_to_user is invoked.
 *****************************************************************************/
//assume continuous buffer
static int get_block(ts_event_t * * block, int size)
{
	int cnt, rd;
	unsigned long flags;
	ts_event_t *p;
	if(NODATA())
		return 0;
		
	cnt = 0;
	
	save_flags(flags);
	cli();
	//critical section
	//get the actual size of data need read
	if(DSIZE()*sizeof(ts_event_t) >= size)
	{
		rd = size/sizeof(ts_event_t);
	}
	else
	{
		rd = DSIZE();
	}
	
	* block = p = get_data();
	cnt ++;
	
	while(p && (cnt < rd))
	{
		if(rptr == 0)
			break;
		p = get_data();
		cnt ++;
	}
	
	restore_flags(flags);
	return (cnt)*sizeof(ts_event_t);
	
}
/******************************************************************************
 * Function Name: get_data
 *
 * Input: 		void	:
 * Value Returned:	ts_event_t	: a 'touch' event data format
 *
 * Description: read a 'touch' event from data buffer
 *
 *****************************************************************************/
// no really read out the data, so no copy
static ts_event_t* get_data(void)
{
	ts_event_t *data;
	
	if(NODATA())
		return NULL;

	data = & (buffer[rptr]);

	TRACE("*** Read - x: 0x%04x, y: 0x%04x, rptr: 0x%04x, wptr: 0x%04x\n", data->x, data->y, (int)rptr, (int)wptr);

	
	NEXTI(rptr);
	return data;
}

/******************************************************************************
 * Function Name: digi_release
 *
 * Input: 		inode	:
 * 			filp	:
 * Value Returned:	int	: Return status.If no error, return 0.
 *
 * Description: release resource when close the inode
 *
 *****************************************************************************/
int digi_release(struct inode * inode, struct file * filp)
{
//	printk("digi_release: ----\n");
	digi_fasync(-1, filp, 0);
	//disable the ADC interrupt
	//disable_pen_interrupt();
	touch_pan_disable_inter();
	g_Digi_Status &= ~DIGI_OPEN_STATUS;
	MOD_DEC_USE_COUNT;
	return 0;
}

/******************************************************************************
 * Function Name: digi_open
 *
 * Input: 		inode	:
 * 			filp	:
 * Value Returned:	int	: Return status.If no error, return 0.
 *
 * Description: allocate resource when open the inode
 *
 *****************************************************************************/
int digi_open(struct inode * inode, struct file * filp)
{
//	printk("digi_open: ----\n");
        MOD_INC_USE_COUNT;

	spi_flush_fifo();
	//enable ADC interrupt
	touch_pan_disable_inter();
#ifdef CONFIG_ARCH_DBMX2
	touch_pan_set_neg_inter();//set default polarity
#endif
#ifdef CONFIG_ARCH_MX1ADS
	touch_pan_set_pos_inter();//set default polarity
#endif
	touch_pan_clear_inter();
	touch_pan_enable_inter();
	
	pen_timer_status = 0;
	g_Digi_Status = DIGI_OPEN_STATUS;
	return 0;
}

/******************************************************************************
 * Function Name: digi_fasync
 *
 * Input: 		fd	:
 * 			filp	:
 * 			mode	:
 * Value Returned:	int	: Return status.If no error, return 0.
 *
 * Description: provide fasync functionality for select system call
 *
 *****************************************************************************/
static int digi_fasync(int fd, struct file *filp, int mode)
{
	/* TODO TODO put this data into file private data */
	int minor = check_device( filp->f_dentry->d_inode);
	if ( minor == - 1)
	{
//		printk("<1>spi_fasyn:bad device minor\n");
		return -ENODEV;
	}
	return( fasync_helper(fd, filp, mode, &ts_fasync) );
}

/******************************************************************************
 * Function Name: digi_ioctl
 *
 * Input: 		inode	:
 * 			filp	:
 * 			cmd	: command for ioctl
 * 			arg	: parameter for command
 * Value Returned:	int	: Return status.If no error, return 0.
 *
 * Description: ioctl for this device driver
 *
 *****************************************************************************/
int digi_ioctl(struct inode * inode, 
		struct file *filp,
		unsigned int cmd , 
		unsigned long arg)
{
	int ret = -EIO;
	int minor;
	
	
	minor = check_device( inode );
	if ( minor == - 1)
	{
		TRACE("bad minor\n");
		return -ENODEV;
	}
	
	TRACE("minor=%08x cmd=%d\n",minor,cmd);
	
	return ret;
}

/******************************************************************************
 * Function Name: spi_poll
 *
 * Input: 		filp	:
 * 			wait	: 
 * Value Returned:	int	: Return status.If no error, return 0.
 *
 * Description: support poll and select
 *
 *****************************************************************************/
unsigned int digi_poll(struct file * filp, struct poll_table_struct * wait)
{
	int minor;
	
	minor = check_device( filp->f_dentry->d_inode);
	if ( minor == - 1)
		return -ENODEV;

	poll_wait(filp, &digi_wait, wait);
	
	return ( NODATA() ) ? 0 : (POLLIN | POLLRDNORM);
}


/******************************************************************************
 * Function Name: digi_read
 *
 * Input: 		filp	: the file 
 * 			buf	: data buffer
 * 			count	: number of chars to be readed
 * 			l	: offset of file
 * Value Returned:	int	: Return status.If no error, return 0.
 *
 * Description: read device driver
 *
 *****************************************************************************/
ssize_t digi_read(struct file * filp, char * buf, size_t count, loff_t * l)
{       
	int nonBlocking = filp->f_flags & O_NONBLOCK;
	int minor;
	ts_event_t *ev;
	int cnt=0;

	minor = check_device( filp->f_dentry->d_inode );
	
	if ( minor == - 1)
		return -ENODEV;
	
	if( nonBlocking )
	{
		if( !NODATA() ) 
		{
			/* returns length to be copied otherwise errno -Exxx */

			cnt = get_block(&ev, count) ;
			if(cnt > count){
				TRACE(KERN_ERR"Error read spi buffer\n");
				return -EINVAL;
			}

			__copy_to_user(buf, (char*)ev, cnt );
			return cnt;
		}
		else
			return -EINVAL;
	}
    else
	{ 

		/* check , when woken, there is a complete event to read */
		while(1){
			if( !NODATA() )
			{

				cnt = get_block(&ev, count);
				if(cnt > count){
					TRACE(KERN_ERR"Error read spi buffer\n");
					return -EINVAL;
				}
				/* returns length to be copied otherwise errno -Exxx */


				__copy_to_user(buf, (char*)ev, cnt);
				return cnt;
			}
			else
			{
				interruptible_sleep_on(&digi_wait);
				if(signal_pending(current))
					return -ERESTARTSYS;
			}
		}
	}
}
/**
* power management callback function, will disable the clk from PCCR0 register
*/
int digi_pm_handler(struct pm_dev *dev, pm_request_t rqst, void *data)
{
	switch(rqst){
		case PM_RESUME:
			if((g_Digi_Status & DIGI_SUSPEND_STATUS)!=0)
			{
				#ifdef CONFIG_ARCH_DBMX2
				_reg_CRM_PCCR0 |= 0x00000010;
				#endif
				g_Digi_Status &= ~DIGI_SUSPEND_STATUS;
			}
			break;
		case PM_SUSPEND:
			if((g_Digi_Status & DIGI_OPEN_STATUS)!=0)
			{
				#ifdef CONFIG_ARCH_DBMX2
				_reg_CRM_PCCR0 &= ~0x00000010;
				#endif
				g_Digi_Status |= DIGI_SUSPEND_STATUS;
			}
			break;
		default:
			break;
		}
	return 0;
}
/*******************************************************
*digitizer init function
*Parameters:None
*Return	
*	0	indicates SUCCESS
* 	-1	indicates FAILURE
*Description: in this function should initialize the SPI device 
as well as the external touch pannel hardware
********************************************************/
signed short __init digi_init(void)
{
	int tmp;
	int i;
	INFO("Pen Driver 0.4.0");
	INFO("Motorola SPS-Suzhou\n");
	init_timer(&pen_timer);
	pen_timer.function = digi_sam_callback;
	
	tasklet_init(&digi_tasklet, digi_tasklet_action,(unsigned long)0);	

	result = devfs_register_chrdev(g_digi_major, MODULE_NAME, &g_digi_fops);
 	if ( result < 0 )
 	{
		TRACE("%s driver: Unable to register driver\n",MODULE_NAME);
		return -ENODEV;
	}

	g_devfs_handle = devfs_register(NULL, MODULE_NAME, DEVFS_FL_DEFAULT,
				      g_digi_major, 0,
				      S_IFCHR | S_IRUSR | S_IWUSR,
				      &g_digi_fops, NULL);   	
				      
 /*
	g_proc_dir=create_proc_entry(MODULE_NAME, 0, NULL);
	g_proc_dir->read_proc=get_proc_list;
	g_proc_dir->data=NULL;				      
*/
	tmp = request_irq(TPNL_IRQ,
			(void * )digi_isr,
			TPNL_INTR_MODE,
			MODULE_NAME,
			MODULE_NAME);
	if (tmp)
	{
		TRACE("digi_init:cannot init major= %d irq=%d\n", g_digi_major, TPNL_IRQ);
        devfs_unregister_chrdev(g_digi_major, MODULE_NAME);
        devfs_unregister(g_devfs_handle);
		return -1;
	}
	//init SPI buffer
	if(-1 == init_buf()){
		TRACE("digi_init: cannot init spi buffer, exit\n");

		free_irq(TPNL_IRQ,MODULE_NAME);

		devfs_unregister_chrdev(g_digi_major, MODULE_NAME);
		devfs_unregister(g_devfs_handle);
		disable_irq(TPNL_IRQ);
		return -1;
	}

	init_waitqueue_head(&digi_wait);
	
	//first init touch pannel
	touch_pan_set_inter();//set pen IRQ 
	touch_pan_disable_inter();//mask pen interrupt
	//init pen touch pannel in GPIO,disabled
	//init SPI module
	spi_init();
	touch_pan_init();
	
	touch_pan_enable();//enable touch pannel

#ifdef CONFIG_ARCH_DBMX2
	spi_exchange_data(0x90);
	spi_exchange_data(0x00);
	spi_exchange_data(0xD0);
	spi_exchange_data(0x00);
	spi_exchange_data(0x00);
#endif
#ifdef  CONFIG_ARCH_MX1ADS
	spi_tx_data(0xd000);
	spi_tx_data(0x0000);
	spi_tx_data(0x9000);
	spi_tx_data(0x0000);
	spi_tx_data(0xD000);
	spi_tx_data(0x0000);
	spi_tx_data(0x9000);
	spi_tx_data(0x0000);
	spi_tx_data(0x0000);
#endif 
	touch_pan_disable();

	/* Delay for A/D Converter to be stable */
	for(i=0;i<50000;i++);  
	g_digi_pm = pm_register(PM_SYS_DEV, PM_SYS_VGA, digi_pm_handler);
	g_Digi_Status = 0;
	return 0;
}

static void __exit digi_cleanup(void)
{
	/*Do some cleanup work*/
//	disable_irq(TPNL_IRQ);
	free_irq(TPNL_IRQ, MODULE_NAME);

	tasklet_kill(&digi_tasklet);

	if(result>0)
	{
		devfs_unregister_chrdev(g_digi_major, MODULE_NAME);
		devfs_unregister(g_devfs_handle);
	}
	pm_unregister(g_digi_pm);
}

module_init(digi_init);
module_exit(digi_cleanup);

MODULE_PARM(irq_mask, "i");
MODULE_PARM(irq_list, "1-4i");
MODULE_LICENSE("GPL");
