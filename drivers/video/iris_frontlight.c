/*
 * linux/drivers/video/iris_frontlight.c 
 *
 * (C) Copyright 2001 Lineo Japan, Inc.
 *
 * May be copied or modified under the terms of the GNU General Public
 * License.  See linux/COPYING for more information.
 *
 * Based on:
 *   drivers/video/sa1100_frontlight.c
 *   Initial Version by: Nicholas Mistry (nmistry@lhsl.com)
 *
 */

#include <linux/config.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/sched.h>
#include <linux/errno.h>
#include <linux/string.h>
#include <linux/ctype.h>
#include <linux/mm.h>
#include <linux/tty.h>
#include <linux/malloc.h>
#include <linux/init.h>
#include <linux/fb.h>
#include <linux/delay.h>

#include <linux/pm.h>

#include <asm/system.h>
#include <asm/hardware.h>
#include <asm/io.h>
#include <asm/irq.h>
#include <asm/uaccess.h>
#include <asm/proc/pgtable.h>
#include <asm/arch/fpga.h>

#include <video/iris_frontlight.h>

#ifdef CONFIG_PM
static struct pm_dev *fb_pm_dev;
#endif


#define MIN_CONTRAST   0
#define MAX_CONTRAST   100
static int is_irisfl_blank = 0;


#if 1 // K.Yamade
#define IRIS_LIGHT_SETTING	4     // range setting : 0(OFF) 1 2 3(MAX)
#define IRIS_LIGHT_DEFAULT      3     // MAX !
static int IrisLightSetting[2][IRIS_LIGHT_SETTING] = { { 0 , 4 , 8 , 16 } , { 0, 4 , 8, 16 } };

static int counter_step_contrast = IRIS_LIGHT_DEFAULT;
static int save_step_contrast;
#else
static int counter_bl = 1;
static int counter_contrast = 1;
static int counter_step_contrast = 1;
static int save_step_contrast = 1;
#endif




static int irisfl_ioctl(struct inode* inode,
			   struct file*  filp,
			   unsigned int  cmd,
			   unsigned long arg);

static int irisfl_enable(void);
static int irisfl_disable(void);
static int irisfl_contrast_setting(int);
static int irisfl_step_contrast_setting(int);




#ifdef CONFIG_PM
static int irisfl_pm_callback(struct pm_dev* pm_dev,
				pm_request_t req, void *data)
{
  static int suspended = 0;
	switch (req) {
	case PM_SUSPEND:
	case PM_BLANK:
#if 0
	  if( suspended ) break;
		save_step_contrast = counter_step_contrast;
		irisfl_step_contrast_setting(0);
		irisfl_disable();
	  suspended = 1;
#endif

		if (!is_irisfl_blank) {
			is_irisfl_blank = 1;
			save_step_contrast = counter_step_contrast;
			irisfl_step_contrast_setting(0);
			irisfl_disable();
		}
		break;
	case PM_RESUME:
	case PM_UNBLANK:
#if 0
	  if( ! suspended ) break;
		irisfl_enable();
		irisfl_step_contrast_setting(save_step_contrast);
	  suspended = 0;
#endif
		if (is_irisfl_blank) {
			irisfl_enable();
			irisfl_step_contrast_setting(save_step_contrast);
			is_irisfl_blank = 0;
		}
		break;
	}
	return 0;
}
#endif

#if 0
static int irisfl_open(struct tty_struct* tty, struct file* filp)
{
	MOD_INC_USE_COUNT;
	return 0;
}

static void irisfl_close(struct tty_struct* tty, struct file* filp)
{
	MOD_DEC_USE_COUNT;
}
#endif

static struct file_operations irisfl_fops = {
#if 0
        open:           irisfl_open,
        release:        irisfl_close,
#endif
	ioctl:		irisfl_ioctl,
};

static int irisfl_major;

#if 0
static int irisfl_setintensity(int intensity)
{
	return 0;
}
#endif

#if 0
static int iris_get_frontlight(void)
{
  int val;

  if (FPGA_GPO & (1<<9)) {
    return 16; // 100%
  }
  val = IO_PMPCON & 0x0000000f; // return l_value;

  return val;
}
#endif

static void iris_set_frontlight_value( int h_value, int l_value )
{
  IO_PMPCON &= 0xffff0000;
  IO_PMPCON |=  (l_value<<4) | h_value;
}

static void iris_set_frontlight( int value )
{
  //printk("clcd_linkup: fb=%08x\n",FB);
  if (value<0) value = 0;
  else if (value>16) value=16;

#if 0 /* Control GPO2 is not necessary. */
  if (value==0) { // off
    FPGA_GPO &= ~(1<<2);
  } else { // on
    FPGA_GPO |= (1<<2);
  }
#endif

  if (value==16) { // 100%
    FPGA_GPO |= (1<<9);
  } else { // duty = 0/16 - 15/16
    FPGA_GPO &= ~(1<<9);
    iris_set_frontlight_value( value, value );
  }
}

static int irisfl_contrast_setting(int need_value)
{

#if 1 
  extern int  iris_ac_line_status;


  if ( need_value < 0 ) need_value = 0;
  if ( need_value >= IRIS_LIGHT_SETTING ) {
    need_value = ( IRIS_LIGHT_SETTING - 1 );
    //    irisfl_enable();
  }

#if 0
  if ( need_value == 0 )
    irisfl_disable();
#endif

  counter_step_contrast = need_value;
  iris_set_frontlight( IrisLightSetting[iris_ac_line_status][counter_step_contrast]);

  return counter_step_contrast;

#else
        if ((need_value >= MIN_CONTRAST) && (need_value <= MAX_CONTRAST)) {
		irisfl_enable();
	}
	else {
		irisfl_disable();
	}

	counter_contrast = need_value;
	iris_set_frontlight( 16 * need_value / MAX_COUNTER);

	return counter_contrast;
#endif
}

static int irisfl_enable(void)
{

  iris_set_frontlight(16);
	return 0;
}

static int irisfl_disable(void)
{
	iris_set_frontlight(0); /* off */
	return 0;
}

static int irisfl_step_contrast_setting(int value)
{
  //iris_set_frontlight(value);
  irisfl_contrast_setting(value);
	counter_step_contrast = value;
	return counter_step_contrast;
}

static int irisfl_ioctl(struct inode* inode,
			   struct file*  filp,
			   unsigned int  cmd,
			   unsigned long arg)
{
	int ret;

	ret = (-EINVAL);
	
	switch(cmd) {
	case IRIS_FL_IOCTL_ON:
		ret = irisfl_enable();
		ret = irisfl_contrast_setting(100);
		break;
	case IRIS_FL_IOCTL_OFF:
		ret = irisfl_contrast_setting(0);
		ret = irisfl_disable();
		break;
	case IRIS_FL_IOCTL_CONTRAST:
		if ((arg >= 0)&&(arg <= 100)) {
			ret = irisfl_contrast_setting(arg);
		}
		break;
#if 0 // K.Yamade
	case IRIS_FL_IOCTL_GET_BACKLIGHT:	
		ret = counter_bl;
		break;
#endif
	case IRIS_FL_IOCTL_GET_CONTRAST:
#if 0
		ret = counter_contrast;
#else
		ret = counter_step_contrast;
#endif
		break;
	case IRIS_FL_IOCTL_STEP_CONTRAST:
		if ((arg >= 0)&&(arg <= 16)) {
			ret = irisfl_step_contrast_setting(arg);
		}
		break;
	case IRIS_FL_IOCTL_GET_STEP_CONTRAST:
		ret = counter_step_contrast;
		break;
#if 1
	case IRIS_FL_IOCTL_GET_STEP:
		ret = IRIS_LIGHT_SETTING;
		break;
#endif
	default:
		;
	}
	
	return ret;
}

int __init irisfl_init(void)
{
	int result;

	irisfl_major = FL_MAJOR;

	result = register_chrdev(irisfl_major, FL_NAME, &irisfl_fops);

	if (result < 0) {
#ifdef DEBUG
	  	printk(KERN_WARNING "irisfl: cant get major %d\n",
		       irisfl_major);
#endif
		return result;
	}
  
#ifdef CONFIG_PM
	fb_pm_dev = pm_register(PM_ILLUMINATION_DEV, 
				PM_SYS_LIGHT, irisfl_pm_callback);
#endif

	if (irisfl_major == 0) {
	  	irisfl_major = result;
	}

	irisfl_enable();

#if 0
	irisfl_contrast_setting(100);
#else
	irisfl_contrast_setting(IRIS_LIGHT_SETTING-1);
#endif

	printk("Frontlight Driver Initialized.\n");
  
	return 0;
}

#ifdef MODULE
void __exit irisfl_exit(void)
{
	irisfl_contrast_setting(0);
	irisfl_disable();

	unregister_chrdev(irisfl_major, "iris-fl");

	printk("Frontlight Driver Unloaded\n");
}
#endif

module_init(irisfl_init);
#ifdef MODULE
module_exit(irisfl_exit);
#endif
