/*
 * linux/drivers/i2c/i2c-aa-dbmx.h
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
 *   File Name : i2c-aa.h
 *   Description :
 *   	i2c Adapter/Algorithm Driver Globel Define	
 *   Auther
 *   Data
 *   ID
 ************************************************************************/
#ifndef _I2C_AA_H
#define _I2C_AA_H

#include <linux/module.h>

//#define _DEBUG_CSI

#ifdef _DEBUG_CSI
#	define CSI_DB(fmt, args...) 	printk("\n[%s]:[line%d]----"fmt, __FILE__, __LINE__, ##args)
#	define CSI_LOC			printk(KERN_ERR"\nCurrent Location [%s]:[%d]\n", __FILE__, __LINE__)
#	define FUNC_START		printk(KERN_ERR"\n[%s]:start!\n", __FUNCTION__)
#	define FUNC_END			printk(KERN_ERR"\n[%s]:end!\n", __FUNCTION__)
#	define CLUE(arg)		printk(" %s\n",arg)

#else
#	define CSI_DB(fmt, args...)
#	define CSI_LOC
#	define FUNC_START
#	define FUNC_END
#endif
/************************************************************************
 *
 * Typedef
 * 
 ************************************************************************/
#ifndef BOOLEAN
typedef signed char BOOLEAN;
typedef unsigned char UINT8;
typedef signed char SINT8;
typedef unsigned short UINT16;
typedef signed short SINT16;
typedef unsigned long UINT32;
typedef signed long SINT32;

typedef volatile BOOLEAN VBOOLEAN;
typedef volatile UINT8 VUINT8;
typedef volatile SINT8 VSINT8;
typedef volatile UINT16 VUINT16;
typedef volatile SINT16 VSINT16;
typedef volatile UINT32 VUINT32;
typedef volatile SINT32 VSINT32;
#endif /* BOOLEAN */

/************************************************************************
 *
 * 		GPIO
 * 
 ************************************************************************/
#ifdef CONFIG_ARCH_MX1ADS

#define PTA_BASE_ADDR 0xF021c000
VUINT32 * PTA_DDIR = (VUINT32 *)(PTA_BASE_ADDR + 0x00);
VUINT32 * PTA_GIUS = (VUINT32 *)(PTA_BASE_ADDR + 0x20);

/************************************************************************
 *
 * Define DBMX1 I2C AA hardware dependent information 
 * 
 ************************************************************************/
/* Bit algorithm adapter ID, maybe should define in 'linux/i2c-id.h' */
//#define I2C_HW_B_DBMX1	0x12
#define I2C_DRIVERID_I2CCSI	0x1001
#define I2C_AA_REG_ADDR 	(0x00217000 + 0xF0000000)
/*
 * I2C clock = system clock / 960  (exactly 100k for 96M BCLK)
 * defined in table 29-4(29.5.2) of DBMX1 user's manual
 */
#define DEFAULT_FREQ 		0x17
#define I2C_AA_IRQ 		0x27
#define I2C_AA_INTR_MODE	0	//SA_SHIRQ | SA_INTERRUPT

/* Slave address of SCM20014 */
#define I2C_CSI_SLAVE_WADD	0x66
#define I2C_CSI_SLAVE_RADD	0x67
#define I2C_ADDR_CMOS_SENSOR	0x66


#define IADR			0xf0217000
#define IFDR			0xf0217004
#define I2CR			0xf0217008
#define I2SR			0xf021700c
#define I2DR			0xf0217010
/************************************************************************
 *
 * Struct
 * 
 ************************************************************************/
#endif  //CONFIG_ARCH_MX1ADS


#ifdef CONFIG_ARCH_DBMX2

#define I2C_DRIVERID_I2C_CSI	0xF000
#define I2C_DRIVERID_I2C_MW8731	0xF001

#define I2C_AA_IRQ		INT_I2C
#define DEFAULT_FREQ		0x17
#define DEFAULT_I2C_ADDRESS	0x1000

#endif

struct I2C_AA_REG 
{
	VUINT32 iadr;
	VUINT32	ifdr;
	VUINT32	i2cr;
	VUINT32	i2sr;
	VUINT32	i2dr;
};


/* Define of Massage Flag */
#define EMBEDDED_REGISTER	0x01
#define I2C_M_READ		0x02
#define I2C_M_WRITE		0x04
// support slave mode read/write, but in DBMX1, useless
#define I2C_S_READ		0x08
#define I2C_S_WRITE		0x10

/* Option Code of I2C Adapter/Algorithm I/O control */
#define I2C_IO_CHANGE_FREQ 	0xaa
#define I2C_IO_GET_STATUS 	0xab

/************************************************************************
 * 		Functions 
 ************************************************************************/
extern int i2c_aa_xfer(struct i2c_adapter *i2c_adap, struct i2c_msg msgs[ ], int num);
extern int i2c_aa_iotcl(struct i2c_adapter * adapter, 
			unsigned int cmd, unsigned long arg);
#endif /* _I2C_AA_H */	
