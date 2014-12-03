/*
 *  linux/drivers/char/sharp_kbdctl.c
 *
 *  Driver for GSM I/O Ports On SHARP PDA
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
 * Change Log
 *	30-Jul-2002 Lineo Japan, Inc.  for 2.4.18
 *      12-Dec-2002 Sharp Corporation  for Poodle and Corgi
 *      1-Nov-2003 Sharp Corporation   for Tosa
 *      1-Jul-2004 Sharp Corporation   for Spitz
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
#include <linux/poll.h>

#include <asm/system.h>
#include <asm/uaccess.h>
#include <asm/io.h>
#include <asm/irq.h>
#include <asm/bitops.h>
#include <asm/arch/hardware.h>

#include <asm/sharp_char.h>
#include <asm/sharp_keycode.h>
#include <linux/kbd_ll.h>

#ifndef SHARP_KBDCTL_MAJOR
# define SHARP_KBDCTL_MAJOR 228 /* number of bare sharp_kbdctl driver */
#endif

#define DEBUGPRINT(s)   /* printk s */


extern int sharppda_kbd_extmodif_togglestatus(int which);
extern int sharppda_kbd_extmodif_showstatus(int which);
extern unsigned long sharppda_kbd_extmodif_packstatus(void);
extern int sharppda_kbd_set_hold_threshold(int msecs);
extern int sharppda_kbd_set_hold_threshold_group(int group,int msecs);
extern int customize_holdkey_char_info(int hardcode,int sl_char);
extern int customize_holdkey_char_info_by_slkey(unsigned char slcode,int sl_char);
extern int restore_all_holdkey_info(void);
extern int customize_del_holdkey_info(int hardcode);
extern int customize_restore_holdkey_info(int hardcode);
#if defined(CONFIG_ARCH_PXA_TOSA) || defined(CONFIG_ARCH_PXA_SPITZ)
extern int set_port_key_setting( int num, int val );
extern int get_port_key_setting( int num );
#endif

/*
 * operations....
 */

static struct fasync_struct	*fasyncptr;
static DECLARE_WAIT_QUEUE_HEAD(wq_in);

void sharpkbdctl_stat_changed(void)
{
  wake_up_interruptible(&wq_in);
  if( fasyncptr ){
    kill_fasync(&fasyncptr, SIGIO, POLL_IN);
  }
}

EXPORT_SYMBOL(sharpkbdctl_stat_changed);

static int sharpkbdctl_fasync(int fd, struct file *filep, int mode)
{
  int	ret;
  ret = fasync_helper((int)fd, filep, mode, &fasyncptr);
  if (ret < 0) return ret;
  return 0;
}

static uint sharpkbdctl_poll(struct file *filep, poll_table *wait)
{
  uint	mask = 0;
  static int initialized = 0;
  static unsigned long oldval = 0;
  unsigned long val1;
  unsigned long val2;
  if( ! initialized ){
    oldval = sharppda_kbd_extmodif_packstatus();
    initialized = 1;
  }
  val1 = oldval;
  poll_wait(filep, &wq_in, wait);
  val2 = sharppda_kbd_extmodif_packstatus();
  if( val1 != val2 ){
    mask = POLLIN|POLLRDNORM;
    oldval = val2;
  }
 END:
  return mask;
}

static int device_used = 0;

static int sharpkbdctl_open(struct inode *inode, struct file *filp)
{
  sharpkbdctl_fasync(-1, filp, 0);
  if( ! device_used ){
    init_waitqueue_head((wait_queue_head_t*)&wq_in);
    fasyncptr = NULL;
  }
  MOD_INC_USE_COUNT;
  return 0;
}

static int sharpkbdctl_release(struct inode *inode, struct file *filp)
{
  MOD_DEC_USE_COUNT;
  return 0;
}

static int sharpkbdctl_ioctl_sednkey(unsigned long arg)
{
	unsigned char c, s, t;
	int f = arg >> 24;
	if( f & 0xC0 )
		return -EINVAL;
	c = (unsigned char)arg;
	s = (unsigned char)(arg >> 8);
	t = (unsigned char)(arg >> 16);
	if( !f ) switch( c ){
	case SHARP_EXTMODIF_2ND:
		sharppda_kbd_extmodif_togglestatus(c);
		c = SLKEY_2ND; f = 3;
		break;
	case SHARP_EXTMODIF_CAPS:
		sharppda_kbd_extmodif_togglestatus(c);
		c = SLKEY_CAPS_LOCK; f = 3;
		break;
	case SHARP_EXTMODIF_NUMLOCK:
		sharppda_kbd_extmodif_togglestatus(c);
#if defined(CONFIG_ARCH_PXA_TOSA) || defined(CONFIG_ARCH_PXA_SPITZ)
		c = 32; f = 3; // numlock keycode 
#else
		c = SLKEY_NUMLOCK; f = 3;
#endif
		break;
	default:
		return -EINVAL;
	}
	if( f & (1<<4) ) handle_scancode(t, 1);
	if( f & (1<<2) ) handle_scancode(s, 1);
	if( f & (1<<0) ) handle_scancode(c, 1);
	if( f & (1<<1) ) handle_scancode(c, 0);
	if( f & (1<<3) ) handle_scancode(s, 0);
	if( f & (1<<5) ) handle_scancode(t, 0);
	return 0;
}

static int sharpkbdctl_ioctl(struct inode *inode,
			     struct file *filp,
			     unsigned int command,
			     unsigned long arg)
{
  int error;
  switch( command ) {
  case SHARP_KBDCTL_GETMODIFSTAT:
    {
      int error;
      int val;
      sharp_kbdctl_modifstat* puser = (sharp_kbdctl_modifstat*)arg;
      if( ! puser ) return -EINVAL;
      val = sharppda_kbd_extmodif_showstatus(puser->which);
      error = put_user(val,&(puser->stat));
      if( error ) return error;
    }
    break;
  case SHARP_KBDCTL_TOGGLEMODIFSTAT:
    {
      int error;
      error = sharppda_kbd_extmodif_togglestatus(arg);
      if( error ) return error;
    }
    break;
#if defined(CONFIG_ARCH_PXA_TOSA) || defined(CONFIG_ARCH_PXA_SPITZ)
  case SHARP_KBDCTL_SETMODIFSTAT:
    {
      int error;
      int val;
      sharp_kbdctl_modifstat* puser = (sharp_kbdctl_modifstat*)arg;
      if( ! puser ) return -EINVAL;
      val = sharppda_kbd_extmodif_showstatus(puser->which);
      if (puser->stat!=val) {
	error = sharppda_kbd_extmodif_togglestatus(puser->which);
	if( error ) return error;
      }
    }
    break;
#endif
  case SHARP_KBDCTL_SETHOLDTH:
    {
      int error;
      error = sharppda_kbd_set_hold_threshold(arg);
      if( error ) return error;
    }
    break;
  case SHARP_KBDCTL_SETHOLDTH_GR:
    {
      int error;
      sharp_kbdctl_holdstat* puser = (sharp_kbdctl_holdstat*)arg;
      error = sharppda_kbd_set_hold_threshold_group(puser->group,puser->timeout);
      if( error ) return error;
    }
    break;
  case SHARP_KBDCTL_HOLDINFO_SETHD:
    {
      int error;
      sharp_kbdctl_holdcustom* puser = (sharp_kbdctl_holdcustom*)arg;
      DEBUGPRINT(("SHARP_KBDCTL_HOLDINFO_SETHD\n"));
      if( ! puser ) return error;
      if( puser->hold_slcode < 0 ){
	error = customize_del_holdkey_info(puser->normal_hardcode);
      }else{
	error = customize_holdkey_char_info(puser->normal_hardcode,puser->hold_slcode);
      }
      if( error ) return error;
    }
    break;
  case SHARP_KBDCTL_HOLDINFO_SETSL:
    {
      int error;
      sharp_kbdctl_holdcustom* puser = (sharp_kbdctl_holdcustom*)arg;
      DEBUGPRINT(("SHARP_KBDCTL_HOLDINFO_SETSL\n"));
      if( ! puser ) return error;
      if( puser->hold_slcode < 0 ){
	error = customize_del_holdkey_info_by_slkey(puser->normal_slcode);
      }else{
	error = customize_holdkey_char_info_by_slkey(puser->normal_slcode,puser->hold_slcode);
      }
      if( error ) return error;
    }
    break;
  case SHARP_KBDCTL_HOLDINFO_DELHD:
    {
      int error;
      sharp_kbdctl_holdcustom* puser = (sharp_kbdctl_holdcustom*)arg;
      DEBUGPRINT(("SHARP_KBDCTL_HOLDINFO_DELHD\n"));
      error = customize_del_holdkey_info(puser->normal_hardcode);
      if( error ) return error;
    }
    break;
  case SHARP_KBDCTL_HOLDINFO_DELSL:
    {
      int error;
      sharp_kbdctl_holdcustom* puser = (sharp_kbdctl_holdcustom*)arg;
      DEBUGPRINT(("SHARP_KBDCTL_HOLDINFO_DELSL\n"));
      error = customize_del_holdkey_info_by_slkey(puser->normal_slcode);
      if( error ) return error;
    }
    break;
  case SHARP_KBDCTL_HOLDINFO_RESTHD:
    {
      int error;
      sharp_kbdctl_holdcustom* puser = (sharp_kbdctl_holdcustom*)arg;
      DEBUGPRINT(("SHARP_KBDCTL_HOLDINFO_RESTHD\n"));
      error = customize_restore_holdkey_info(puser->normal_hardcode);
      if( error ) return error;
    }
    break;
  case SHARP_KBDCTL_HOLDINFO_RESTSL:
    {
      int error;
      sharp_kbdctl_holdcustom* puser = (sharp_kbdctl_holdcustom*)arg;
      DEBUGPRINT(("SHARP_KBDCTL_HOLDINFO_RESTSL\n"));
      error = customize_restore_holdkey_info_by_slkey(puser->normal_slcode);
      if( error ) return error;
    }
    break;
  case SHARP_KBDCTL_HOLDINFO_RESTFULL:
    {
      int error;
      DEBUGPRINT(("SHARP_KBDCTL_HOLDINFO_RESTFULL\n"));
      error = restore_all_holdkey_info();
      if( error ) return error;
    }
    break;
#ifdef CONFIG_IRIS
  case IRIS_KBDCTL_ENABLEKEYBOARD:
    {
      extern int iris_kbdctl_keyboard_disabled;
      DEBUGPRINT(("IRIS_KBDCTL_ENABLEKEYBOARD\n"));
      iris_kbdctl_keyboard_disabled = 0;
      return 0;
    }
  case IRIS_KBDCTL_DISABLEKEYBOARD:
    {
      extern int iris_kbdctl_keyboard_disabled;
      DEBUGPRINT(("IRIS_KBDCTL_ENABLEKEYBOARD\n"));
      iris_kbdctl_keyboard_disabled = 1;
      return 0;
    }
  case IRIS_KBDCTL_GETSTATENUMBER:
    {
      extern int sharp_cur_status;
      return sharp_cur_status;
    }
#endif /* CONFIG_IRIS */
  case SHARP_KBDCTL_SENDKEY:
    {
      int error;
      error = sharpkbdctl_ioctl_sednkey(arg);
      if( error ) return error;
    }
    break;
#if defined(CONFIG_ARCH_PXA_TOSA) || defined(CONFIG_ARCH_PXA_SPITZ)
  case SHARP_KBDCTL_SETSWKEY:
    {
      sharp_kbdctl_swkey* puser = (sharp_kbdctl_swkey*)arg;
      if( ! puser ) return -EINVAL;
      return set_port_key_setting(puser->key,puser->mode);
    }
    break;
  case SHARP_KBDCTL_GETSWKEY:
    {
      return get_port_key_setting(arg);
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
struct file_operations sharp_kbdctl_fops = {
  open:    sharpkbdctl_open,
  release: sharpkbdctl_release,
  fasync:  sharpkbdctl_fasync,
  poll:	   sharpkbdctl_poll,
  ioctl:   sharpkbdctl_ioctl
};

/*
 *  init and exit
 */

#if defined(SHARPCHAR_USE_MISCDEV)

#include <linux/miscdevice.h>

static struct miscdevice sharpkbdctl_device = {
  SHARP_KBDCTL_MINOR,
  "sharp_kbdctl",
  &sharp_kbdctl_fops
};

static int __init sharpkbdctl_init(void)
{
  DEBUGPRINT(("sharpkbdctl init\n"));
  if( misc_register(&sharpkbdctl_device) )
    printk("failed to register sharpkbdctl\n");
  return 0;
}

static void __exit sharpkbdctl_cleanup(void)
{
  DEBUGPRINT(("sharpkbdctl cleanup\n"));
  misc_deregister(&sharpkbdctl_device);
}

module_init(sharpkbdctl_init);
module_exit(sharpkbdctl_cleanup);

#else /* ! SHARPCHAR_USE_MISCDEV */


#if 0 /* bare driver should not be supported */

static int sharpkbdctl_major = SHARP_KBDCTL_MAJOR;

static int __init sharpkbdctl_init(void)
{
  int result;
  DEBUGPRINT(("sharpkbdctl init\n"));
  if( ( result = register_chrdev(sharpkbdctl_major,"sharpkbdctl",&sharp_kbdctl_fops) ) < 0 ){
    DEBUGPRINT(("sharpkbdctl failed\n"));
    return result;
  }
  if( sharpkbdctl_major == 0 ) sharpkbdctl_major = result; /* dynamically assigned */
  DEBUGPRINT(("sharpkbdctl registered %d\n",sharpkbdctl_major));
  return 0;
}

static void __exit sharpkbdctl_cleanup(void)
{
  DEBUGPRINT(("sharpkbdctl cleanup\n"));
  unregister_chrdev(sharpkbdctl_major,"sharpkbdctl");
}

module_init(sharpkbdctl_init);
module_exit(sharpkbdctl_cleanup);
#endif

#endif /* ! SHARPCHAR_USE_MISCDEV */

/*
 *   end of source
 */
