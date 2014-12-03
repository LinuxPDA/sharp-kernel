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
#include <linux/malloc.h>
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

#ifndef SHARP_LED_MAJOR
# define SHARP_LED_MAJOR 226 /* number of bare sharp_led driver */
#endif

#define DEBUGPRINT(s)   /* printk s */

#ifdef CONFIG_IRIS
extern int iris_led_supported(int which); /* return -EINVAL if unspoorted , otherwise return >= 0 value */
extern int iris_turn_led_status(int which,int status); /* set LED status to directed value. */
extern int iris_init_led(void);
extern int iris_suspend_led(void);
extern int iris_standby_led(void);
extern int iris_resume_led(void);
#define led_supported_arch(w)       iris_led_supported((w))
#define turn_led_status_arch(w,s)   iris_turn_led_status((w),(s))
#define init_led_arch()             iris_init_led()
#define suspend_led_arch()          iris_suspend_led()
#define resume_led_arch()           iris_resume_led()
#define standby_led_arch()          iris_standby_led()
#endif
#ifdef CONFIG_SA1100_COLLIE
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

static int sharpled_major = SHARP_LED_MAJOR;
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
    set_led_status(SHARP_LED_PDA,LED_PDA_SUSPENDED);
    standby_led_arch();
    break;
  case PM_BLANK:
#if defined(CONFIG_SA1100_COLLIE)
    set_led_status(SHARP_LED_PDA,LED_PDA_SUSPENDED);
#endif /* CONFIG_SA1100_COLLIE */
    break;
  case PM_UNBLANK:
#if defined(CONFIG_SA1100_COLLIE)
    set_led_status(SHARP_LED_PDA,LED_PDA_RUNNING);
#endif /* CONFIG_SA1100_COLLIE */
    break;
  case PM_SUSPEND:
    set_led_status(SHARP_LED_PDA,LED_PDA_OFF);
    suspend_led_arch();
    break;
  case PM_RESUME:
    set_led_status(SHARP_LED_PDA,LED_PDA_RUNNING);
    resume_led_arch();
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
  DEBUGPRINT(("sharpled init\n"));
  if( misc_register(&sharpled_device) )
    printk("failed to register sharpled\n");

#ifdef CONFIG_PM
  led_pm_dev = pm_register(PM_SYS_DEV, 0, led_pm_callback);
#endif

  set_led_status(SHARP_LED_PDA,LED_PDA_RUNNING);

  return 0;
}

static void __exit sharpled_cleanup(void)
{
  DEBUGPRINT(("sharpled cleanup\n"));
  misc_deregister(&sharpled_device);
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
