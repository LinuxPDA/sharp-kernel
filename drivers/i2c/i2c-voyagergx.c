/* -------------------------------------------------------------------- */
/* i2c-voyagergx.c:                                                     */
/* -------------------------------------------------------------------- */
/*  This program is free software; you can redistribute it and/or modify
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
#include <linux/ioport.h>
#include <linux/module.h>
#include <linux/delay.h>
#include <linux/slab.h>
#include <linux/version.h>
#include <linux/init.h>
#include <asm/irq.h>
#include <asm/io.h>

#include <linux/i2c.h>
#include "i2c-algo-voyagergx.h"
#include "i2c-voyagergx.h"

//#define I2C_USE_INTR

#if defined(I2C_USE_INTR)
#define DEFAULT_IRQ   10
#else
#define DEFAULT_IRQ   0
#endif

static int i2c_debug = 0;
static struct iic_voyagergx {
	unsigned long	iic_base;
	int 		iic_irq;
};
static struct iic_voyagergx gpi;

#if (LINUX_VERSION_CODE < 0x020301)
static struct wait_queue *iic_wait = NULL;
#else
static wait_queue_head_t iic_wait;
#endif
static int iic_pending;

/* ----- global defines -----------------------------------------------	*/
#define DEB(x)	if (i2c_debug>=1) x
#define DEB2(x) if (i2c_debug>=2) x
#define DEB3(x) if (i2c_debug>=3) x
#define DEBE(x)	x	/* error messages 				*/


/* ----- local functions ----------------------------------------------	*/

static void iic_voyagergx_setiic(void *data, int ctl, char val)
{
        unsigned long j = jiffies + 10;

//	DEB3(printk(" Write 0x%02x to 0x%x\n",(unsigned char)val, ctl));
//	printk(" Write 0x%02x to 0x%x\n",(unsigned char)val, ctl);
	DEB3({while (jiffies < j) schedule();}) 
	outb(val,ctl);
}

static char iic_voyagergx_getiic(void *data, int ctl)
{
	char val;

	val = inb(ctl);
//	DEB3(printk("Read 0x%02x from 0x%x\n",(unsigned char)val, ctl));  
//	printk("Read 0x%02x from 0x%x\n",(unsigned char)val, ctl);  
	return (val);
}

/* Return our slave address.  This is the address
 * put on the I2C bus when another master on the bus wants to address us
 * as a slave
 */
static int iic_voyagergx_getown(void *data)
{
	return 0;
}


static int iic_voyagergx_getclock(void *data)
{
	return 0;
}

#if 0
static void iic_voyagergx_sleep(unsigned long timeout)
{
	schedule_timeout( timeout * HZ);
}
#endif


/* Put this process to sleep.  We will wake up when the
 * IIC controller interrupts.
 */
static void iic_voyagergx_waitforpin(void) {

   int timeout = 2;

   /* If interrupts are enabled (which they are), then put the process to
    * sleep.  This process will be awakened by two events -- either the
    * the IIC peripheral interrupts or the timeout expires. 
    * If interrupts are not enabled then delay for a reasonable amount 
    * of time and return.
    */
   if (gpi.iic_irq > 0) {
	cli();
	if (iic_pending == 0) {
		interruptible_sleep_on_timeout(&iic_wait, timeout*HZ );
	} else
		iic_pending = 0;
	sti();
   } else {
      udelay(100);
   }
}


static void iic_voyagergx_handler(int this_irq, void *dev_id, struct pt_regs *regs) 
{
	
   iic_pending = 1;

   DEB2(printk("iic_voyagergx_handler: in interrupt handler\n"));
   wake_up_interruptible(&iic_wait);
}


/* Lock the region of memory where I/O registers exist.  Request our
 * interrupt line and register its associated handler.
 */
static int iic_hw_resrc_init(void)
{
	unsigned char		ctl;
	unsigned long		val;

	// Power Mode Gate
	val = inl(POWER_MODE0_GATE);
	val |= 0x00000040;
	outl(val, POWER_MODE0_GATE);
	val = inl(POWER_MODE1_GATE);
	val |= 0x00000040;
	outl(val, POWER_MODE1_GATE);

	// GPIO Control
	val = inl( GPIO_MUX_HIGH);
	val |= 0x0000c000;
	outl(val, GPIO_MUX_HIGH);

#if defined(I2C_USE_INTR)
	// Interrupt Mask
	val = inl(INT_MASK);
	val |= 0x00800000;
	outl(val, INT_MASK);
#endif

        // Enable I2c controller and select mode to high
        ctl = inb(I2C_CONTROL);
        outb((ctl | I2C_CONTROL_E | I2C_CONTROL_MODE), I2C_CONTROL);

#if 0
  	if (check_region(gpi.iic_base, ITE_IIC_IO_SIZE) < 0 ) {
   	   return -ENODEV;
  	} else {
  	   request_region(gpi.iic_base, ITE_IIC_IO_SIZE, 
		"i2c (i2c bus adapter)");
  	}
#endif

#if defined(I2C_USE_INTR)
	if (gpi.iic_irq > 0) {
	   if (request_irq(gpi.iic_irq, iic_voyagergx_handler, 0, "VoyagerGX IIC", 0) < 0) {
	      gpi.iic_irq = 0;
	   } else
	      DEB3(printk("Enabled IIC IRQ %d\n", gpi.iic_irq));
	      enable_irq(gpi.iic_irq);
	}
#endif

	return 0;
}


static void iic_voyagergx_release(void)
{
	if (gpi.iic_irq > 0) {
		disable_irq(gpi.iic_irq);
		free_irq(gpi.iic_irq, 0);
	}
	release_region(gpi.iic_base , 2);
}


static int iic_voyagergx_reg(struct i2c_client *client)
{
	return 0;
}


static int iic_voyagergx_unreg(struct i2c_client *client)
{
	return 0;
}


static void iic_voyagergx_inc_use(struct i2c_adapter *adap)
{
#ifdef MODULE
	MOD_INC_USE_COUNT;
#endif
}


static void iic_voyagergx_dec_use(struct i2c_adapter *adap)
{
#ifdef MODULE
	MOD_DEC_USE_COUNT;
#endif
}


/* ------------------------------------------------------------------------
 * Encapsulate the above functions in the correct operations structure.
 * This is only done when more than one hardware adapter is supported.
 */
static struct i2c_algo_iic_data iic_voyagergx_data = {
	NULL,
	iic_voyagergx_setiic,
	iic_voyagergx_getiic,
	iic_voyagergx_getown,
	iic_voyagergx_getclock,
	iic_voyagergx_waitforpin,
	80, 80, 100,		/*	waits, timeout */
};

static struct i2c_adapter iic_voyagergx_ops = {
	"VoyagerGX I2C",
	I2C_ALGO_SMBUS,
	NULL,
	&iic_voyagergx_data,
	iic_voyagergx_inc_use,
	iic_voyagergx_dec_use,
	iic_voyagergx_reg,
	iic_voyagergx_unreg,
};

/* Called when the module is loaded.  This function starts the
 * cascade of calls up through the hierarchy of i2c modules (i.e. up to the
 *  algorithm layer and into to the core layer)
 */
static int __init iic_voyagergx_init(void) 
{

	struct iic_voyagergx *piic = &gpi;
	int irq = 0;

	printk(KERN_INFO "Initialize VoyagerGX I2C module\n");

	piic->iic_base	= VOYAGER_BASE;

#if defined(I2C_USE_INTR)
	piic->iic_irq	= DEFAULT_IRQ;
#else
	piic->iic_irq	= irq;
#endif

	iic_voyagergx_data.data = (void *)piic;
#if (LINUX_VERSION_CODE >= 0x020301)
	init_waitqueue_head(&iic_wait);
#endif
	if (iic_hw_resrc_init() == 0) {
		if (i2c_iic_add_bus(&iic_voyagergx_ops) < 0)
			return -ENODEV;
	} else {
		return -ENODEV;
	}

#if defined(I2C_USE_INTR)
	printk(KERN_INFO " found device at %#x irq %d.\n", 
		piic->iic_base, piic->iic_irq);
#else
	printk(KERN_INFO " found device at %#x\n",piic->iic_base);
#endif

/* DEBUG */
	{
		struct i2c_adapter i2c_adap = iic_voyagergx_ops;
		int cnt,ret;
		unsigned char buf[16];

#if defined(TC56XX)
		for (cnt = 0;cnt < 3;cnt++){
			buf[0] = 0;
			buf[1] = 1;
			buf[2] = 2;
			buf[3] = 3;

//			printk("[%d] iic_sendbytes() start\n",cnt);
			ret = iic_sendbytes(&i2c_adap,buf,4);
//			printk("[%d] iic_sendbytes() end ret = %d\n",cnt,ret);
		}
#endif
#if defined(OV7640)
		cnt = 0;
		for (;;){
			if (register_ov7640_7141[cnt]		== 0xFF &&
			    register_ov7640_7141[cnt + 1]	== 0xFF)
				break;

			buf[0] = register_ov7640_7141[cnt++];
			buf[1] = register_ov7640_7141[cnt++];

//			printk("iic_sendbytes() start [%02x][%02x]\n",buf[0],buf[1]);
			ret = iic_sendbytes(&i2c_adap,buf,PACKET_SIZE);
//			printk("iic_sendbytes() end ret = %d\n",ret);
		}
#endif
	}
/* DEBUG */

	return 0;
}


static void iic_voyagergx_exit(void)
{
        iic_voyagergx_release();
}

EXPORT_NO_SYMBOLS;

/* If modules is NOT defined when this file is compiled, then the MODULE_*
 * macros will resolve to nothing
 */
MODULE_AUTHOR("Lineo uSolutions,Inc. <www.lineo.co.jp>");
MODULE_DESCRIPTION("I2C-Bus adapter for VoyagerGX Silicon Motion, Inc.");
MODULE_LICENSE("GPL");

MODULE_PARM(i2c_debug,"i");

/* Called when module is loaded or when kernel is intialized.
 * If MODULES is defined when this file is compiled, then this function will
 * resolve to init_module (the function called when insmod is invoked for a
 * module).  Otherwise, this function is called early in the boot, when the
 * kernel is intialized.  Check out /include/init.h to see how this works.
 */
module_init(iic_voyagergx_init);

/* Resolves to module_cleanup when MODULES is defined. */
module_exit(iic_voyagergx_exit); 
