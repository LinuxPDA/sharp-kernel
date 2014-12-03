/*
 *  linux/drivers/char/sharp_led.c
 *
 *  Driver for LED devices On SHARP PDA
 *
 * Copyright (C) 2001  SHARP
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
 *  ChangeLog:
 *	30-Jul-2002 Lineo Japan, Inc.  for 2.4.18
 *	27-AUG-2002 Edward Chen, Steve Lin for Discovery
 *	12-Dec-2002 Lineo Japan, Inc.
 *      1-Nov-2003 Sharp Corporation   for Tosa
 *
 */
#include <linux/config.h>
#include <linux/module.h>
#include <linux/errno.h>
#include <linux/signal.h>
#include <linux/sched.h>
#include <linux/timer.h>
#include <linux/interrupt.h>
#include <linux/major.h>
#include <linux/string.h>
#include <linux/fcntl.h>
#include <linux/ptrace.h>
#include <linux/ioport.h>
#include <linux/mm.h>
#include <linux/slab.h>
#include <linux/init.h>
#include <linux/delay.h>
#include <linux/console.h>
#include <linux/pm.h>

#include <asm/system.h>
#include <asm/uaccess.h>
#include <asm/io.h>
#include <asm/irq.h>
#include <asm/bitops.h>
#include <asm/arch/hardware.h>

#include <asm/sharp_char.h>
#ifdef CONFIG_SABINAL_DISCOVERY
#include <asm/arch/discovery_asic3.h>
#endif
#if defined(CONFIG_ARCH_PXA_CORGI)
#include <asm/arch/corgi.h>
#endif


#ifndef SHARP_LED_MAJOR
# define SHARP_LED_MAJOR 226 /* number of bare sharp_led driver */
#endif

#define DEBUGPRINT(s)   /* printk s */


#if defined(CONFIG_SA1100_COLLIE) || defined(CONFIG_SABINAL_DISCOVERY)
extern int collie_led_supported(int which); /* return -EINVAL if unspoorted , otherwise return >= 0 value */
extern int collie_turn_led_status(int which,int status); /* set LED status to directed value. */
extern int collie_init_led(void);
extern int collie_suspend_led(void);
extern int collie_resume_led(void);
#define led_supported_arch(w)       collie_led_supported((w))
#define turn_led_status_arch(w,s)   collie_turn_led_status((w),(s))
#define init_led_arch()             collie_init_led()
#define suspend_led_arch()          collie_suspend_led()
#define resume_led_arch()           collie_resume_led()
#define standby_led_arch()          (0)
#elif defined(CONFIG_ARCH_PXA_POODLE) || defined(CONFIG_ARCH_PXA_CORGI) || defined(CONFIG_ARCH_PXA_TOSA)
extern int sharpsl_led_supported(int which); /* return -EINVAL if unspoorted , otherwise return >= 0 value */
extern int sharpsl_turn_led_status(int which,int status); /* set LED status to directed value. */
extern int sharpsl_init_led(void);
extern int sharpsl_suspend_led(void);
extern int sharpsl_resume_led(void);
#define led_supported_arch(w)       sharpsl_led_supported((w))
#define turn_led_status_arch(w,s)   sharpsl_turn_led_status((w),(s))
#define init_led_arch()             sharpsl_init_led()
#define suspend_led_arch()          sharpsl_suspend_led()
#define resume_led_arch()           sharpsl_resume_led()
#define standby_led_arch()
#endif
#ifdef USE_HDD_LED
extern void set_hdd_mode( int mode );
extern int get_hdd_mode();
#endif

/*
 * logical level drivers
 */

static unsigned char logical_led_status[SHARP_LED_WHICH_MAX+1];

static int is_led_supported(int which)
{
  if( which < 0 || which > SHARP_LED_WHICH_MAX ) return -EINVAL;

  return(led_supported_arch(which));
}

int get_led_status(int which) /* this is exported for usability from ARCH-Dep LED driver */
{
  if( which < 0 || which > SHARP_LED_WHICH_MAX ) return -EINVAL;

  return(logical_led_status[which]);
}

int set_led_status(int which,int status)
{
  if( which < 0 || which > SHARP_LED_WHICH_MAX ) return -EINVAL;

  logical_led_status[which] = status;
  turn_led_status_arch(which,logical_led_status[which]); /* ignore retval */

  return 0;
}

/*
 * operations....
 */

//static int sharpled_major = SHARP_LED_MAJOR;
static int device_initialized = 0;

static int sharpled_open(struct inode *inode, struct file *filp)
{
  int minor = MINOR(inode->i_rdev);
  if( minor != SHARP_LED_MINOR ) return -ENODEV;
  if( ! device_initialized ){
    init_led_arch();
  }
  
  DEBUGPRINT(("SHARP Led Opened %x\n",minor));

  MOD_INC_USE_COUNT;

  return 0;
}

static int sharpled_release(struct inode *inode, struct file *filp)
{
  DEBUGPRINT(("SHARP Led Closed\n"));

  MOD_DEC_USE_COUNT;

  return 0;
}

static int sharpled_ioctl(struct inode *inode,
			  struct file *filp,
			  unsigned int command,
			  unsigned long arg)
{
  int error;
  switch( command ) {
  case SHARP_LED_GETSTATUS:
    {
      int status;
      sharp_led_status* puser = (sharp_led_status*)arg;
      status = get_led_status(puser->which);
      if( status < 0 ) return -EINVAL;
      error = put_user(status,&(puser->status));
      if( error ) return error;
    }
    break;
  case SHARP_LED_SETSTATUS:
    {
      sharp_led_status* puser = (sharp_led_status*)arg;
      error = set_led_status(puser->which,puser->status);
      if( error ) return error;
    }
    break;
  case SHARP_LED_ISUPPORTED:
    {
      int status;
      sharp_led_status* puser = (sharp_led_status*)arg;
      status = is_led_supported(puser->which);
      if( status < 0 ) return -EINVAL;
      error = put_user(status,&(puser->status));
      if( error ) return error;
    }
    break;
#ifdef USE_HDD_LED
  case SHARP_LED_SETHDDMODE:
    {
      int status = (int)arg;
      set_hdd_mode( status );
      return 0;
    }
    break;
  case SHARP_LED_GETHDDMODE:
    {
      return get_hdd_mode();
    }
    break;
#endif
  default:
    return -EINVAL;
  }
  return 0;
}

/*
 * The file operations
 */
struct file_operations sharp_led_fops = {
  open:    sharpled_open,
  release: sharpled_release,
  ioctl:   sharpled_ioctl
};

/*
 *  power management
 */

#ifdef CONFIG_PM

static struct pm_dev *led_pm_dev;

static int led_pm_callback(struct pm_dev *pm_dev,
			   pm_request_t req, void *data)
{
  switch (req) {
  case PM_STANDBY:
#if !defined(CONFIG_SABINAL_DISCOVERY)
    set_led_status(SHARP_LED_PDA,LED_PDA_SUSPENDED);
    standby_led_arch();
#endif
    break;
    
  case PM_BLANK:
#if !defined(CONFIG_SABINAL_DISCOVERY)
    set_led_status(SHARP_LED_PDA,LED_PDA_SUSPENDED);
#endif
    break;
    
  case PM_UNBLANK:
#if !defined(CONFIG_SABINAL_DISCOVERY)
    set_led_status(SHARP_LED_PDA,LED_PDA_RUNNING);
#endif
    break;
    
  case PM_SUSPEND:
#if !defined(CONFIG_SABINAL_DISCOVERY)
    set_led_status(SHARP_LED_PDA,LED_PDA_OFF);
    suspend_led_arch();
#endif
    break;
    
  case PM_RESUME:
#if !defined(CONFIG_SABINAL_DISCOVERY)
    set_led_status(SHARP_LED_PDA,LED_PDA_RUNNING);
    resume_led_arch();
#endif
    break;
    
  }
  return 0;
}

#endif

/*
 *  init and exit
 */

#if defined(SHARPCHAR_USE_MISCDEV)

#include <linux/miscdevice.h>

static struct miscdevice sharpled_device = {
  SHARP_LED_MINOR,
  "sharp_led",
  &sharp_led_fops
};

static int __init sharpled_init(void)
{

	if( misc_register(&sharpled_device) )
	    printk("failed to register sharpled\n");

#ifdef CONFIG_SABINAL_DISCOVERY
	/* 1. Alternative function */
	ASIC3_GPIO_ALT_C |= (GPIO_P0+GPIO_P1+GPIO_P2);

	/* 2. clk_32768hz */
	ASIC3_CLOCK_CDEXCDCX |= GPIO_P13;

	/* 2. clk LED0,LED1 */
	ASIC3_CLOCK_CDEXCDCX &= ~(GPIO_P6+GPIO_P7);

	/* 3. CH LED */
	ASIC3_LEDCNTL_CH_LED |= GPIO_V1;
	ASIC3_LEDCNTL_CH_LED &= ~GPIO_P4; /* LEDEN off */
	ASIC3_LEDCNTL_CH_LED &= ~GPIO_P5;
	ASIC3_LEDCNTL_CH_LED |= GPIO_P6;

	ASIC3_LEDPTS_CH_LED |= GPIO_V1;
	
	ASIC3_LEDDTS_CH_LED |= 0;
	
	/* 3. Notify LED */
	ASIC3_LEDCNTL_NOTIFY_LED |= GPIO_V1;
	ASIC3_LEDCNTL_NOTIFY_LED &= ~GPIO_P4;
	ASIC3_LEDCNTL_NOTIFY_LED &= ~GPIO_P5;
	ASIC3_LEDCNTL_NOTIFY_LED |= GPIO_P6;

	ASIC3_LEDPTS_NOTIFY_LED |= GPIO_V1;
	
	ASIC3_LEDDTS_NOTIFY_LED = 0;

	/* 4. clk LED0,LED1 */
	ASIC3_CLOCK_CDEXCDCX |= GPIO_P6+GPIO_P7;

	/* 5. CH/NOTIFY LEDEN */
//	ASIC3_LEDCNTL_CH_LED |= GPIO_P4;
//	ASIC3_LEDCNTL_NOTIFY_LED |= GPIO_P4;

	/* test to close */
	ASIC3_LEDPTS_CH_LED = 0; /* off */
	ASIC3_LEDPTS_NOTIFY_LED = 0; /* off */
#endif

#ifdef CONFIG_PM
  led_pm_dev = pm_register(PM_SYS_DEV, 0, led_pm_callback);
#endif

  set_led_status(SHARP_LED_PDA,LED_PDA_RUNNING);

  printk("sharpled_init: done.\n");

  return 0;
}

static void __exit sharpled_cleanup(void)
{
  DEBUGPRINT(("sharpled cleanup\n"));
  misc_deregister(&sharpled_device);

  printk("sharpled_cleanup: done.\n");

}

module_init(sharpled_init);
module_exit(sharpled_cleanup);

#else /* ! SHARPCHAR_USE_MISCDEV */

#if 0 /* bare driver should not be supported */
static int __init sharpled_init(void)
{
  int result;
  DEBUGPRINT(("sharpled init\n"));
  if( ( result = register_chrdev(sharpled_major,"sharpled",&sharp_led_fops) ) < 0 ){
    DEBUGPRINT(("sharpled failed\n"));
    return result;
  }
  if( sharpled_major == 0 ) sharpled_major = result; /* dynamically assigned */
  DEBUGPRINT(("sharpled registered %d\n",sharpled_major));
  return 0;
}

static void __exit sharpled_cleanup(void)
{
  DEBUGPRINT(("sharpled cleanup\n"));
  unregister_chrdev(sharpled_major,"sharpled");
}

module_init(sharpled_init);
module_exit(sharpled_cleanup);

#endif

#endif /* ! SHARPCHAR_USE_MISCDEV */


/*
 *   end of source
 */
