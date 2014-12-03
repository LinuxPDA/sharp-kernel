/* -------------------------------------------------------------------- */
/* i2c-algo-voyagergx.c:                                                */
/* -------------------------------------------------------------------- */
/*
    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.

    Copyright 2003 (c) Lineo uSolutions,Inc.
*/
/* -------------------------------------------------------------------- */


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

#include <linux/i2c.h>
#include "i2c-algo-voyagergx.h"
#include "i2c-voyagergx.h"

/* ----- global defines ----------------------------------------------- */
#define DEB(x) if (i2c_debug>=1) x
#define DEB2(x) if (i2c_debug>=2) x
#define DEB3(x) if (i2c_debug>=3) x /* print several statistical values*/
#define DEBPROTO(x) if (i2c_debug>=9) x;
 	/* debug the protocol by showing transferred bits */
#define DEF_TIMEOUT 16

/* debugging - slow down transfer to have a look at the data .. 	*/
/* I use this with two leds&resistors, each one connected to sda,scl 	*/
/* respectively. This makes sure that the algorithm works. Some chips   */
/* might not like this, as they have an internal timeout of some mils	*/


/* ----- global variables ---------------------------------------------	*/

/* module parameters:
 */
static int i2c_debug=0;
static int iic_test=0;	/* see if the line-setting functions work	*/
//static int iic_scan=0;	/* have a look at what's hanging 'round		*/

/* --- setting states on the bus with the right timing: ---------------	*/

#define iic_outb(adap, reg, val) adap->setiic(adap->data, reg, val)
#define iic_inb(adap, reg) adap->getiic(adap->data, reg)

/* --- other auxiliary functions --------------------------------------	*/

static void iic_start(struct i2c_algo_iic_data *adap)
{
	unsigned char ctl;
 	ctl = iic_inb(adap, I2C_CONTROL);
	ctl = (ctl | I2C_CONTROL_STATUS);
	DEB(printk("iic_start I2C_CONTROL = 0x%x\n", ctl));
	iic_outb(adap,I2C_CONTROL,ctl);
}

static void iic_stop(struct i2c_algo_iic_data *adap)
{
	unsigned char ctl;
	ctl = iic_inb(adap, I2C_CONTROL);
	ctl = (ctl & ~I2C_CONTROL_STATUS);
	DEB(printk("iic_stop I2C_CONTROL = 0x%x\n", ctl));
	iic_outb(adap,I2C_CONTROL,ctl);
}

static void iic_reset(struct i2c_algo_iic_data *adap)
{
	unsigned char ctl;
	ctl = iic_inb(adap, I2C_RESET);
	ctl = (ctl & ~I2C_RESET_ERROR);
	DEB(printk("iic_reset I2C_CONTROL = 0x%x\n", ctl));
	iic_outb(adap,I2C_RESET,ctl);
}


static int wait_for_bb(struct i2c_algo_iic_data *adap)
{
	int timeout = DEF_TIMEOUT;
	char status;

	status = iic_inb(adap, I2C_STATUS);
#ifndef STUB_I2C
	while (timeout-- && (status & I2C_STATUS_BUSY)) {
		udelay(1000); /* How much is this? */
		status = iic_inb(adap, I2C_STATUS);
	}
#endif
	if (timeout<=0) {
		printk(KERN_ERR "Timeout, host is busy (%d)\n",timeout);
		iic_reset(adap);
	}
	return(timeout<=0);
}

/*
 * Puts this process to sleep for a period equal to timeout 
 */
static inline void iic_sleep(unsigned long timeout)
{
	schedule_timeout( timeout * HZ);
}

static int wait_for_pin(struct i2c_algo_iic_data *adap, char *status)
{
	int timeout = DEF_TIMEOUT;

	timeout = wait_for_bb(adap);
	if (timeout) {
  		DEB2(printk("Timeout waiting for host not busy\n");)
  		return -EIO;
	}

	timeout = DEF_TIMEOUT;

	*status = iic_inb(adap, I2C_STATUS);
	while (timeout-- && !(*status & I2C_STATUS_ACK)) {
	   adap->waitforpin();
	   *status = iic_inb(adap, I2C_STATUS);
	}
	if (timeout <= 0)
		return(-1);
	else
		return(0);
}

static int iic_init (struct i2c_algo_iic_data *adap)
{
	return 0;
}


/*
 * Sanity check for the adapter hardware - check the reaction of
 * the bus lines only if it seems to be idle.
 */
static int test_bus(struct i2c_algo_iic_data *adap, char *name)
{
	return (0);
}

/* ----- Utility functions
 */

/* Verify the device we want to talk to on the IIC bus really exists. */
static inline int try_address(struct i2c_algo_iic_data *adap,
		       unsigned int addr, int retries)
{
	int i, ret = -1;
	unsigned char status;

	for (i=0;i<retries;i++) {
		iic_outb(adap, I2C_SADDRESS, addr);
		iic_start(adap);
		if (wait_for_pin(adap, &status) == 0) {
			ret=1;
			break;	/* success! */
		}
		iic_stop(adap);
		udelay(adap->udelay);
	}
	DEB2(if (i) printk("try_address: needed %d retries for 0x%x\n",i,
	                   addr));
	return ret;
}

#if 1	/* DEBUG */
int iic_sendbytes(struct i2c_adapter *i2c_adap,const char *buf,
                         int count)
#else	/* DEBUG */
static int iic_sendbytes(struct i2c_adapter *i2c_adap,const char *buf,
                         int count)
#endif	/* DEBUG */
{
	struct i2c_algo_iic_data	*adap = i2c_adap->algo_data;
	int				wrcount,timeout,i;
	unsigned char			*addr,status;

	if (count != PACKET_SIZE)
		return -EPROTO;

	iic_outb(adap,I2C_BYTECOUNT,(unsigned char)(count - 1));

/* DEBUG */
//	printk("I2C_BYTECOUNT = 0x%x\n",iic_inb(adap,I2C_BYTECOUNT));
/* DEBUG */

	iic_outb(adap,I2C_SADDRESS,(unsigned char)SERIAL_WRITE_ADDR);

	timeout = wait_for_bb(adap);
	if (timeout)
		return -ETIMEDOUT;

	wrcount	= 0;
	addr	= (unsigned char *)I2C_DATA;

	for (i = 0;i < count;i++){
		iic_outb(adap,addr++,buf[wrcount++]);
	}

	iic_start(adap);
		
	/* Wait for transmission to complete */
	timeout = wait_for_pin(adap,&status);
	if (timeout){
		iic_stop(adap);
		printk("iic_sendbytes: %s write timeout.\n", i2c_adap->name);
		return -EREMOTEIO; /* got a better one ?? */
     	}

	iic_stop(adap);

	return wrcount;
}


static int iic_readbytes(struct i2c_adapter *i2c_adap, char *buf, int count,
	int sread)
{
	struct i2c_algo_iic_data	*adap = i2c_adap->algo_data;
	int				rdcount,timeout,i;
	unsigned char			*addr,wk,status;

#if 1
	iic_outb(adap,I2C_BYTECOUNT,(unsigned char)(PACKET_SIZE - 1));
#else
	wk = iic_inb(adap,I2C_BYTECOUNT);
	if (wk != (PACKET_SIZE - 1))
		return -EPROTO;
#endif

	iic_outb(adap,I2C_SADDRESS,(unsigned char)SERIAL_READ_ADDR);

	rdcount	= 0;
	addr	= (unsigned char *)I2C_DATA;

	iic_start(adap);

	timeout = wait_for_pin(adap,&status);
	if (timeout){
		iic_stop(adap);
		printk("iic_sendbytes: %s write timeout.\n", i2c_adap->name);
		return -EREMOTEIO; /* got a better one ?? */
     	}

	for (i = 0;i < PACKET_SIZE;i++){
		wk = iic_inb(adap,addr++);
		buf[rdcount++] = wk;
	}

	iic_stop(adap);

	return rdcount;
}


/* This function implements combined transactions.  Combined
 * transactions consist of combinations of reading and writing blocks of data.
 * Each transfer (i.e. a read or a write) is separated by a repeated start
 * condition.
 */
#if 0
static int iic_combined_transaction(struct i2c_adapter *i2c_adap, struct i2c_msg msgs[], int num) 
{
   int i;
   struct i2c_msg *pmsg;
   int ret;

   DEB2(printk("Beginning combined transaction\n"));

   for(i=0; i<(num-1); i++) {
      pmsg = &msgs[i];
      if(pmsg->flags & I2C_M_RD) {
         DEB2(printk("  This one is a read\n"));
         ret = iic_readbytes(i2c_adap, pmsg->buf, pmsg->len, IIC_COMBINED_XFER);
      }
      else if(!(pmsg->flags & I2C_M_RD)) {
         DEB2(printk("This one is a write\n"));
         ret = iic_sendbytes(i2c_adap, pmsg->buf, pmsg->len, IIC_COMBINED_XFER);
      }
   }
   /* Last read or write segment needs to be terminated with a stop */
   pmsg = &msgs[i];

   if(pmsg->flags & I2C_M_RD) {
      DEB2(printk("Doing the last read\n"));
      ret = iic_readbytes(i2c_adap, pmsg->buf, pmsg->len, IIC_SINGLE_XFER);
   }
   else if(!(pmsg->flags & I2C_M_RD)) {
      DEB2(printk("Doing the last write\n"));
      ret = iic_sendbytes(i2c_adap, pmsg->buf, pmsg->len, IIC_SINGLE_XFER);
   }

   return ret;
}
#endif


/* Whenever we initiate a transaction, the first byte clocked
 * onto the bus after the start condition is the address (7 bit) of the
 * device we want to talk to.  This function manipulates the address specified
 * so that it makes sense to the hardware when written to the IIC peripheral.
 *
 * Note: 10 bit addresses are not supported in this driver, although they are
 * supported by the hardware.  This functionality needs to be implemented.
 */
static inline int iic_doAddress(struct i2c_algo_iic_data *adap,
                                struct i2c_msg *msg, int retries) 
{
//	unsigned short flags = msg->flags;
	unsigned int addr;
	int ret;

	addr = ( msg->addr << 1 );

	if (iic_inb(adap, I2C_SADDRESS) != addr) {
		iic_outb(adap, I2C_SADDRESS, addr);
		ret = try_address(adap, addr, retries);
		if (ret!=1) {
			printk("iic_doAddress: died at address code.\n");
				return -EREMOTEIO;
		}
	}

	return 0;
}


/* Description: Prepares the controller for a transaction (clearing status
 * registers, data buffers, etc), and then calls either iic_readbytes or
 * iic_sendbytes to do the actual transaction.
 *
 * still to be done: Before we issue a transaction, we should
 * verify that the bus is not busy or in some unknown state.
 */
static int iic_xfer(struct i2c_adapter *i2c_adap,
		    struct i2c_msg msgs[], 
		    int num)
{
	struct i2c_algo_iic_data *adap = i2c_adap->algo_data;
	struct i2c_msg *pmsg;
	int i = 0;
	int ret, timeout;
    
	pmsg = &msgs[i];

	if(!pmsg->len) {
		DEB2(printk("iic_xfer: read/write length is 0\n");)
		return -EIO;
	}
//	if(!(pmsg->flags & I2C_M_RD) && (!(pmsg->len)%2) ) {
//		DEB2(printk("iic_xfer: write buffer length is not odd\n");)
//		return -EIO; 
//	}

	/* Wait for any pending transfers to complete */
	timeout = wait_for_bb(adap);
	if (timeout) {
		DEB2(printk("iic_xfer: Timeout waiting for host not busy\n");)
		return -EIO;
	}

	/* Flush FIFO */
//	iic_outb(adap, ITE_I2CFCR, ITE_I2CFCR_FLUSH);

	/* Load address */
	ret = iic_doAddress(adap, pmsg, i2c_adap->retries);
	if (ret)
		return -EIO;

#if 0
	/* Combined transaction (read and write) */
	if(num > 1) {
           DEB2(printk("iic_xfer: Call combined transaction\n"));
           ret = iic_combined_transaction(i2c_adap, msgs, num);
  }
#endif

	DEB3(printk("iic_xfer: Msg %d, addr=0x%x, flags=0x%x, len=%d\n",
		i, msgs[i].addr, msgs[i].flags, msgs[i].len);)

	if(pmsg->flags & I2C_M_RD) 		/* Read */
		ret = iic_readbytes(i2c_adap, pmsg->buf, pmsg->len, 0);
	else {													/* Write */ 
		udelay(1000);
		ret = iic_sendbytes(i2c_adap, pmsg->buf, pmsg->len);
	}

	if (ret != pmsg->len)
		DEB3(printk("iic_xfer: error or fail on read/write %d bytes.\n",ret)); 
	else
		DEB3(printk("iic_xfer: read/write %d bytes.\n",ret));

	return ret;
}


/* Implements device specific ioctls.  Higher level ioctls can
 * be found in i2c-core.c and are typical of any i2c controller (specifying
 * slave address, timeouts, etc).  These ioctls take advantage of any hardware
 * features built into the controller for which this algorithm-adapter set
 * was written.  These ioctls allow you to take control of the data and clock
 * lines and set the either high or low,
 * similar to a GPIO pin.
 */
static int algo_control(struct i2c_adapter *adapter, 
	unsigned int cmd, unsigned long arg)
{

//  struct i2c_algo_iic_data *adap = adapter->algo_data;
  struct i2c_iic_msg s_msg;
  char *buf;
	int ret;

  if (cmd == I2C_SREAD) {
		if(copy_from_user(&s_msg, (struct i2c_iic_msg *)arg, 
				sizeof(struct i2c_iic_msg))) 
			return -EFAULT;
		buf = kmalloc(s_msg.len, GFP_KERNEL);
		if (buf== NULL)
			return -ENOMEM;

		ret = iic_readbytes(adapter, buf, s_msg.len, 1);
		if (ret>=0) {
			if(copy_to_user( s_msg.buf, buf, s_msg.len) ) 
				ret = -EFAULT;
		}
		kfree(buf);
	}
	return 0;
}


static u32 iic_func(struct i2c_adapter *adap)
{
	return I2C_FUNC_SMBUS_EMUL | I2C_FUNC_10BIT_ADDR | 
	       I2C_FUNC_PROTOCOL_MANGLING; 
}

/* -----exported algorithm data: -------------------------------------	*/

static struct i2c_algorithm iic_algo = {
	"VoyagerGX IIC algorithm",
	I2C_ALGO_IIC,
	iic_xfer,		/* master_xfer	*/
	NULL,				/* smbus_xfer	*/
	NULL,				/* slave_xmit		*/
	NULL,				/* slave_recv		*/
	algo_control,			/* ioctl		*/
	iic_func,			/* functionality	*/
};


/* 
 * registering functions to load algorithms at runtime 
 */
int i2c_iic_add_bus(struct i2c_adapter *adap)
{
//	int i;
//	short status;
	struct i2c_algo_iic_data *iic_adap = adap->algo_data;

	if (iic_test) {
		int ret = test_bus(iic_adap, adap->name);
		if (ret<0)
			return -ENODEV;
	}

	DEB2(printk("i2c-algo-voyagergx: hw routines for %s registered.\n",
	            adap->name));

	/* register new adapter to i2c module... */

	adap->id |= iic_algo.id;
	adap->algo = &iic_algo;

	adap->timeout = 100;	/* default values, should	*/
	adap->retries = 3;		/* be replaced by defines	*/
	adap->flags = 0;

#ifdef MODULE
	MOD_INC_USE_COUNT;
#endif

	i2c_add_adapter(adap);
	iic_init(iic_adap);

	return 0;
}


int i2c_iic_del_bus(struct i2c_adapter *adap)
{
	int res;
	if ((res = i2c_del_adapter(adap)) < 0)
		return res;
	DEB2(printk("i2c-algo-voyagergx: adapter unregistered: %s\n",adap->name));

#ifdef MODULE
	MOD_DEC_USE_COUNT;
#endif
	return 0;
}


int __init i2c_algo_iic_init (void)
{
	printk(KERN_INFO "ITE iic (i2c) algorithm module\n");
	return 0;
}


void i2c_algo_iic_exit(void)
{
	return;
}


/* The MODULE_* macros resolve to nothing if MODULES is not defined
 * when this file is compiled.
 */
MODULE_AUTHOR("Lineo uSolutions,Inc. <www.lineo.co.jp>");
MODULE_DESCRIPTION("VoyagerGX I2C algorithm");
MODULE_LICENSE("GPL");

MODULE_PARM(iic_test, "i");
MODULE_PARM(iic_scan, "i");
MODULE_PARM(i2c_debug,"i");

MODULE_PARM_DESC(iic_test, "Test if the I2C bus is available");
MODULE_PARM_DESC(iic_scan, "Scan for active chips on the bus");
MODULE_PARM_DESC(i2c_debug,
        "debug level - 0 off; 1 normal; 2,3 more verbose; 9 iic-protocol");


/* This function resolves to init_module (the function invoked when a module
 * is loaded via insmod) when this file is compiled with MODULES defined.
 * Otherwise (i.e. if you want this driver statically linked to the kernel),
 * a pointer to this function is stored in a table and called
 * during the intialization of the kernel (in do_basic_setup in /init/main.c) 
 *
 * All this functionality is complements of the macros defined in linux/init.h
 */
module_init(i2c_algo_iic_init);


/* If MODULES is defined when this file is compiled, then this function will
 * resolved to cleanup_module.
 */
module_exit(i2c_algo_iic_exit);
