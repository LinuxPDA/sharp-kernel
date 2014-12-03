/*
 * linux/drivers/sound/poodle_i2sc.c
 *
 * I2C,I2S funstions for PXA (SHARP)
 *
 * Copyright (C) 2002  SHARP
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
 * Change Log
 *
 *
*/
#include <linux/sched.h>
#include <linux/delay.h>
#include <linux/config.h>
#include <linux/init.h>
#include <asm/arch/hardware.h>
#include <asm/mach/arch.h>
#include <asm/arch/i2sc.h>

#include <asm/arch/poodle_i2sc.h>


/*** Some declarations ***********************************************/
#define MY_TRACE	0

#define DBG_ALWAYS	0
#define DBG_LEVEL1	1
#define DBG_LEVEL2	2
#define DBG_LEVEL3	3

#ifdef MY_TRACE
#define I2C_DBGPRINT(level, x, args...)  if ( level <= MY_TRACE ) printk("%s: " x,__FUNCTION__ ,##args)
#define I2S_DBGPRINT(level, x, args...)  if ( level <= MY_TRACE ) printk("%s: " x,__FUNCTION__ ,##args)
#else
#define I2C_DBGPRINT(level, x, args...)  if ( !level ) printk("%s: " x,__FUNCTION__ ,##args)
#define I2S_DBGPRINT(level, x, args...)  if ( !level ) printk("%s: " x,__FUNCTION__ ,##args)
#endif


#define TRANSMIT_TIMEOUT	5	// 50msec
#define RECIEVE_TIMEOUT		5	// 50msec

#define ERR_SEND1		0x01
#define ERR_SEND2		0x02
#define ERR_SEND3		0x04
/*** Some valiables **************************************************/
static unsigned char isInUseI2C;
static unsigned char isInUseI2S;



/*** Open & Close ******************************************************/
int i2c_open(unsigned char address)
{
  if ( isInUseI2C ) {
    I2C_DBGPRINT( DBG_ALWAYS, "i2c device is busy now !\n");
    return -EBUSY;
  }

  isInUseI2C = address;


  CKEN |= CKEN14_I2C;		// clock enable

  I2C_ICR = I2C_ICR_UR;		// Reset I2C Controler

  mdelay(1);

  I2C_ISAR = address+1;		// initialize SAR

  //  CKEN |= CKEN14_I2C;		// clock enable

  I2C_ICR = (I2C_ICR_IUE | I2C_ICR_SCLE);	// Setup
  //  I2C_ICR |= I2C_ICR_FM;	// Set to FAST mode

  return 0;
}


int i2c_close(unsigned char address)
{
  if ( isInUseI2C != address ) {
    I2C_DBGPRINT(DBG_ALWAYS, "mismatch !\n");
    return -EBUSY;
  }

  isInUseI2C = FALSE;

  I2C_ICR = I2C_ICR_UR;		// Reset I2C Controler
  I2C_ISR = 0;			// clear all ISR

  // disable all I2C interrupts
  I2C_ICR &= ~( I2C_ICR_SADIE | I2C_ICR_ALDIE | I2C_ICR_SSDIE | I2C_ICR_BEIE | I2C_ICR_IRFIE | I2C_ICR_ITEIE );

  I2C_ISAR = 0x00;		// initialize SAR

  CKEN &= ~CKEN14_I2C;		// clock disable

  return 0;
}



int i2s_open(unsigned char id, unsigned int freq)
{
  int div;

  if ( isInUseI2S ) {
    I2S_DBGPRINT(DBG_ALWAYS, "i2s device is busy now !\n");
    return -EBUSY;
  }

  //if ( freq == 8000 ) freq = 48000;

  freq /= 1000;			// calc sampling freq.
  if ( ( freq > 48 ) || ( freq < 8 ) ) return -EINVAL;
  div = ( 147600 / ( freq*64*4 ) );

  I2S_DBGPRINT(DBG_LEVEL2 , "div = %x\n",div);

  isInUseI2S = id;

  CKEN |= CKEN8_I2S;		// clock enable

  I2S_SACR0 |= SACR0_RST;	// reset
  I2S_SACR0 &= ~SACR0_ENB;	// disable module
  I2S_SACR0 &= ~SACR0_RST;

  I2S_SACR0 |= SACR0_BCKD;	// output BITCLK
  I2S_SACR0 |= 0x7700;
  I2S_SACR0 &= ~( SACR0_EFWR | SACR0_STRF );

  I2S_SADR = 0x0000;
  I2S_SADIV = ( div & SADIV_MASK );
  I2S_SACR1 &= ~( SACR1_AMSL | SACR1_DREC | SACR1_DRPL | SACR1_ENLBF );

  I2S_SACR0 |= SACR0_ENB;	// enable module

  return 0;
}


int i2s_close(unsigned char id)
{

  if ( isInUseI2S != id ) {
    I2S_DBGPRINT(DBG_ALWAYS, "mismatch !\n");
    return -EBUSY;
  }

  isInUseI2S = FALSE;

  I2S_SACR0 |= SACR0_RST;	// reset
  I2S_SACR0 &= ~SACR0_ENB;	// disable module
  I2S_SACR0 &= ~SACR0_RST;

  CKEN &= ~CKEN8_I2S;		// clock disable

  return 0;
}


/*** write & read  ******************************************************/
int i2c_byte_write(unsigned char DeviceAdr,unsigned char RegisterAdr,unsigned char Data)
{    	  
	long start_time;
	long err = 0;

	udelay(20);

	I2C_ICR |= I2C_ICR_SCLE;
	I2C_ICR |= I2C_ICR_IUE;
	I2C_ISR = I2C_ISR_ITE;

	I2S_DBGPRINT(DBG_LEVEL2, "i2c_byte_write adr(%8x),reg(%8x),data(%8x)\n",DeviceAdr,RegisterAdr,Data);

	//
	// Send First Byte
	//
	I2C_IDBR = ( DeviceAdr << 1 );	// set data
					// We have to set device , so we have to shift 1 bit.

	I2C_ICR |= I2C_ICR_START;	// first data,  start = on, stop = off
	I2C_ICR &= ~I2C_ICR_STOP;
	I2C_ICR &= ~I2C_ICR_ALDIE;	// clear ALDIE
	I2C_ICR |= I2C_ICR_TB;		// clear TB , transfer data !

	start_time = jiffies;
	while (1) {
	  //schedule();
	  if( !(I2C_ISR & I2C_ISR_RWM ) && (I2C_ISR & I2C_ISR_UB) && (I2C_ISR & I2C_ISR_ITE) ) {
	    break;	// IDBR Transmit Empty = 1 and Unit Busy = 1 and R/nW = 0 , so break
	  }
	  if ( ( jiffies - start_time ) > TRANSMIT_TIMEOUT ) {
	    I2C_DBGPRINT(DBG_ALWAYS, "time out1 %x\n",I2C_ISR);
	    I2S_DBGPRINT(DBG_LEVEL1, "i2c_byte_write adr(%8x),reg(%8x),data(%8x)\n",DeviceAdr,RegisterAdr,Data);
	    err |= ERR_SEND1;
	    break;
	  }
	}
	I2C_ISR = I2C_ISR_ITE;		// clear interrupt


	//
	// Send Second Byte
	//
	I2C_IDBR = RegisterAdr;		// set data	

	I2C_ICR |= I2C_ICR_ALDIE;
	I2C_ICR &= ~I2C_ICR_STOP;	// second data, start = off, stop = off
	I2C_ICR &= ~I2C_ICR_START;
	I2C_ICR |= I2C_ICR_TB;		// clear TB , transfer data !

	start_time = jiffies;
	while (1) {
	  //schedule();
	  if( !(I2C_ISR & I2C_ISR_RWM ) && (I2C_ISR & I2C_ISR_UB) && (I2C_ISR & I2C_ISR_ITE) ) {
	    break;	// IDBR Transmit Empty = 1 and Unit Busy = 1 and R/nW = 0 , so break
	  }
	  if ( ( jiffies - start_time ) > TRANSMIT_TIMEOUT ) {
	    I2C_DBGPRINT(DBG_ALWAYS, "time out2 %x\n",I2C_ISR);
	    I2S_DBGPRINT(DBG_LEVEL1, "i2c_byte_write adr(%8x),reg(%8x),data(%8x)\n",DeviceAdr,RegisterAdr,Data);
	    err |= ERR_SEND2;
	    break;
	  }
	}
	I2C_ISR = I2C_ISR_ITE;		// clear interrupt


	//
	// Send Third Byte
	//
	I2C_IDBR = Data;		// set data

	I2C_ICR |= I2C_ICR_ALDIE;
	I2C_ICR &= ~I2C_ICR_START;	// second data, start = off, stop = on
	I2C_ICR |= I2C_ICR_STOP;
	I2C_ICR |= I2C_ICR_TB;		// clear TB , transfer data !

	start_time = jiffies;
	while (1) {
	  //schedule();
	  if( !(I2C_ISR & I2C_ISR_RWM ) && (I2C_ISR & I2C_ISR_ITE) ) {
	    break;	// IDBR Transmit Empty = 1 and R/nW = 0 , so break
	  }
	  if ( ( jiffies - start_time ) > TRANSMIT_TIMEOUT ) {
	    I2C_DBGPRINT(DBG_ALWAYS, "time out3 %x\n",I2C_ISR);
	    I2S_DBGPRINT(DBG_LEVEL1, "i2c_byte_write adr(%8x),reg(%8x),data(%8x)\n",DeviceAdr,RegisterAdr,Data);
	    err |= ERR_SEND3;
	    break;
	  }
	}
	I2C_ISR = I2C_ISR_ITE;		// clear interrupt


	I2C_ICR &= ~I2C_ICR_START;
	I2C_ICR &= ~I2C_ICR_STOP;
	I2C_ICR &= ~I2C_ICR_ALDIE;
	I2C_ICR &= ~I2C_ICR_TB;

#if 0
	I2C_ICR &= ~I2C_ICR_IUE;
	I2C_ICR &= ~I2C_ICR_SCLE;
#endif	
	return err;
}


/*** Config & Setup ******************************************************/
int i2c_init(void)
{
  isInUseI2C = FALSE;

  I2C_ICR = I2C_ICR_UR;		// Reset I2C Controler
  I2C_ISR = 0;			// clear all ISR
  I2C_ICR = 0;			// 

  // disable all I2C interrupts
  I2C_ICR &= ~( I2C_ICR_SADIE | I2C_ICR_ALDIE | I2C_ICR_SSDIE | I2C_ICR_BEIE | I2C_ICR_IRFIE | I2C_ICR_ITEIE );

  I2C_ISAR = 0x00;		// initialize SAR

  CKEN &= ~CKEN14_I2C;		// clock disable

  return 0;
}



void i2c_suspend(void)
{
  I2C_ICR = I2C_ICR_UR;		// Reset I2C Controler
  I2C_ISR = 0;			// clear all ISR

  // disable all I2C interrupts
  I2C_ICR &= ~( I2C_ICR_SADIE | I2C_ICR_ALDIE | I2C_ICR_SSDIE | I2C_ICR_BEIE | I2C_ICR_IRFIE | I2C_ICR_ITEIE );

  I2C_ISAR = 0x00;		// initialize SAR

  CKEN &= ~CKEN14_I2C;		// clock disable
}


void i2c_resume(void)
{
  i2c_init();
}
