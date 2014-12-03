/*                                                                            
 * linux/drivers/i2c/i2c-client-dbmx-codec.c
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
 *  File Name : i2c-ssi.c
 *  Description :
 *	Implementation of ssi interface and CMOS sensor Driver
 *  Auther
 *  Data
 *  ID
 ************************************************************************/
/*  
 * 	Initial Version
 */
#include <linux/config.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/delay.h>
#include <linux/slab.h>
#include <linux/version.h>
#include <linux/init.h>
#include <linux/ioport.h>
#include <linux/errno.h>
#include <linux/sched.h>
#include <linux/hfs_sysdep.h>
#include <linux/i2c.h>
#include <linux/i2c-algo-bit.h>
#include <linux/i2c-id.h>
#include <linux/slab.h>
#include <linux/delay.h>
#include <asm/uaccess.h>
#include <asm/arch/irqs.h>


#ifdef CONFIG_ARCH_DBMX2
#include <asm/arch/mx2.h>
#include <asm/arch/pll.h>
#endif
#include "i2c-aa-dbmx.h"
/************************************************************************
 * 
 * 	Functions declaration 
 * 	 	
 ************************************************************************/


static int i2c_ssi_attach_adapter (struct i2c_adapter * adap);
int16_t i2c_ssi_read (uint8_t * buf, size_t count, int16_t ssi_reg );
int16_t i2c_ssi_write (uint8_t * buf, size_t count, int16_t ssi_reg );
int32_t __init i2c_ssi_init (void);
static void __exit i2c_ssi_cleanup( void );

#if defined(CONFIG_ARCH_MX1ADS)

/*The address of the codec on I2C BUS*/
#define 	I2C_ADDR_DAC3550A	0x9a
#define 	CODEC_I2C_ADDR		I2C_ADDR_DAC3550A

/*Sub register adrress*/
#define     SR_REG				0x01
#define     AVOL				0x02
#define     GCFG				0x03

#elif defined(CONFIG_ARCH_DBMX2)

/*The address of the codec on I2C BUS*/
#define 	I2C_ADDR_WM8731		0x34
#define 	CODEC_I2C_ADDR		I2C_ADDR_WM8731


// this is for ADS0.0 errata #5
#define MW8731_I2C_START()	_reg_GPIO_DR(GPIOD) &= ~(1<<20)
//#define MW8731_I2C_END()	_reg_GPIO_DR(GPIOD) |= 1<<19

#endif


/**************************************************************************
 *
 *   Global Variables Define
 *
 *************************************************************************/
static int16_t i2c_ssi_initialized = 0;

static struct i2c_driver i2c_ssi_driver =
{
	name:			"i2c-ssi client driver",
	id:				I2C_DRIVERID_I2C_MW8731,
	flags:			I2C_DF_DUMMY | I2C_DF_NOTIFY,
	attach_adapter:	i2c_ssi_attach_adapter,
	detach_client:	NULL,
	command:		NULL
/*	inc_use:		NULL,
	dec_use:		NULL, */
};

static struct i2c_client i2c_ssi_client =
{
	name:		"i2c-ssi client",
	id:			1,
	flags:		0,
	addr:		-1,
	adapter:	NULL,
	driver:		&i2c_ssi_driver,
	data:		NULL
};


/*-----------------------------------------------------------------------
 * static int16_t i2c_ssi_attach_adapter (struct i2c_adapter * adap)
 * This function is called when the associated driver is to install to the I2C system. 
 * It shall check whether it can handle the adapter.
 *
 * Parameter :
 * 	adap		point o the adapter structure
 * Return :
 * 	Success		0
 * 	Failure		-1	
 *----------------------------------------------------------------------*/ 
static int i2c_ssi_attach_adapter (struct i2c_adapter * adap)
{
	/* find out the adapter for the I2C module in the DBMX*/
	if (memcmp(adap->name, "DBMX I2C Adapter", 16) != 0 )
		return -ENODEV;

	/* store the adapter to the client driver */
	i2c_ssi_client.adapter = adap;

	return 0;
}

/*-----------------------------------------------------------------------
 * int16_t i2c_ssi_read (uint8_t * buf, size_t count, int16_t register )
 * This function is to read the data from the I2C slave through the I2C bus. 
 * Before the access to the I2C bus, the driver should be opened first.
 *
 * Parameter :
 * 	buf		the buffer to store the data
 * 	count		size of the buffer
 * 	ssi_reg		the register number in the I2C slave (SSI sensor)
 * 	
 * Return :
 * 	Success		number of byte received.
 * 	Failure		-1	
 *----------------------------------------------------------------------*/
int16_t i2c_ssi_read (uint8_t * buf, size_t count, int16_t ssi_reg )
{
	struct i2c_msg msg[2];
	//char command;
	
	buf[0] = (uint8_t)ssi_reg;
	msg[0].addr = i2c_ssi_client.addr;
	msg[0].flags = I2C_M_WRITE;
	msg[0].len = 1;
	msg[0].buf = buf;
	
	msg[1].addr = i2c_ssi_client.addr;
	msg[1].flags = I2C_M_READ;
	msg[1].len = count;
	msg[1].buf = buf;
	
	MW8731_I2C_START();
	return i2c_transfer( i2c_ssi_client.adapter, msg, 2 );
	
	//struct i2c_msg msg[1];

	// 
	// store the register value to the first address of the buffer 
	// the adapter/algorithm driver will regard the first byte as the register value 
	//
	//buf[0] = (uint8_t)ssi_reg;

	// initialize the message structure
	//msg[0].addr = i2c_ssi_client.addr;
	//msg[0].flags = EMBEDDED_REGISTER | I2C_M_READ;
	//msg[0].len = count;
	//msg[0].buf = buf;

	//return i2c_transfer( i2c_ssi_client.adapter, msg, 1 );
}

/*-----------------------------------------------------------------------
 * int16_t i2c_ssi_write (uint8_t * buf, size_t count, int16_t register )
 * This function is to write the data to the I2C slave through the I2C bus. 
 * Before the access to the I2C bus, the driver should be opened first.
 *
 * Parameter :
 * 	buf		the buffer to store the data
 * 	count		size of the buffer
 * 	ssi_reg		the register number in the I2C slave (SSI sensor)
 * 	
 * Return :
 * 	Success		number of byte received.
 * 	Failure		-1	
 *----------------------------------------------------------------------*/
int16_t i2c_ssi_write (uint8_t * buf, size_t count, int16_t ssi_reg )
{
	struct i2c_msg msg[1];
	//char command;

#if defined(CONFIG_ARCH_MX1ADS)	
	buf[0] = (uint8_t)ssi_reg;
#elif defined(CONFIG_ARCH_DBMX2)
	// for MW8731, buf[0](7..1) == reg no. and buf[0](0) == msb of data
	// buf[0] is correctly filled up before calling i2c_ssi_write so
	// we do nothing here
#endif

	msg[0].addr = i2c_ssi_client.addr;
	msg[0].flags = I2C_M_WRITE;
	msg[0].len = count;
	msg[0].buf = buf;

	MW8731_I2C_START();
	return i2c_transfer( i2c_ssi_client.adapter, msg, 1 );
}

/*-----------------------------------------------------------------------
 * int16_t init  i2c_ssi_init (void)
 * This function initializes the I2C client driver
 * It is called when the module is being loaded to the system
 *
 * Parameters :
 * 	None
 * Return :
 * 	Success		0
 * 	Failure		-1	
 *----------------------------------------------------------------------*/
int32_t __init i2c_ssi_init (void)
{
	int16_t res;
	
	printk(KERN_INFO "Initialize i2c-client-dbmx-codec module\n");
	
	/*FUNC_START;fixme*/
	/* 
	 * allocate and initialize all the internal data structure to their default state
	 */
	if (i2c_ssi_initialized == 1) return 0;/*fixme*/

	/* 
	 * set the address of the CODEC to the client
	 */
	i2c_ssi_client.addr = CODEC_I2C_ADDR;//I2C_ADDR_DAC3550A;/*fixme*/

	/* 
	 * call the i2c_add_driver() to register the driver to the Linux I2C system
	 */
	if (( res = i2c_add_driver( &i2c_ssi_driver )))/*fixme*/
	{
		/* driver registration failed */
		//i2c_ssi_cleanup();
		return res;
	}
	
	/* 
	 * attach the client to the adapter by calling the i2c_attach_client() 
	 * function of the Linux I2C system
	 */
	if (( res = i2c_attach_client(&i2c_ssi_client )) != 0 )/*fixme*/
	{	
		//i2c_ssi_cleanup();
		i2c_del_driver(&i2c_ssi_driver);/*fixme*/
		return res;
	}
		
	i2c_ssi_initialized = 1;/*fixme*/

	/*FUNC_END;fixme*/
	return 0;
}


/*-----------------------------------------------------------------------
 * int16_t i2c_ssi_cleanup( void )
 * This routine is called when the driver is uninstalled.
 *
 * Parameters :
 * 	None
 * Return :
 * 	Success		 0
 * 	Failure		-1	
 *----------------------------------------------------------------------*/
static void __exit i2c_ssi_cleanup( void )
{
	int res;

	if (i2c_ssi_initialized == 1) 
	{
		/* 
	 	* detach the client to the adapter 
	 	*/
		if ((res = i2c_detach_client(&i2c_ssi_client )))
		{	
			printk("%s: Error detaching client\n", __FUNCTION__);
		}
	
		/*
		 * deregister the driver from the Linux I2C system
		 */
		if ((res = i2c_del_driver(&i2c_ssi_driver))) 
		{
			printk("%s: Error deleting driver\n", __FUNCTION__);
		}
		
		i2c_ssi_initialized = 0;
	}
	
}


/************************************************************************
 * 	Init/Cleanup Module
 ************************************************************************/ 	
MODULE_AUTHOR("Motorola Inc");
MODULE_DESCRIPTION("i2c-client-dbmx-codec driver for the Motorola DBMX");
MODULE_LICENSE("GPL");

EXPORT_SYMBOL(i2c_ssi_write);
EXPORT_SYMBOL(i2c_ssi_read);

module_init(i2c_ssi_init);
module_exit(i2c_ssi_cleanup);


