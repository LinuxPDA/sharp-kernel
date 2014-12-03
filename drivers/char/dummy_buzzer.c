/*
 *  dummy buzzer driver
 *
 * (C) Copyright 2002 Lineo Japan, Inc.
 *
 * May be copied or modified under the terms of the GNU General Public
 * License.  See linux/COPYING for more information.
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


/*
 * operations....
 */

static int sharpbuz_open(struct inode *inode, struct file *filp)
{
  MOD_INC_USE_COUNT;
  return 0;
}

static int sharpbuz_release(struct inode *inode, struct file *filp)
{
  MOD_DEC_USE_COUNT;
  return 0;
}

static ssize_t sharpbuz_write(struct file * file, char * buf,size_t count, loff_t *ppos)
{
  return count;
}

static int sharpbuz_ioctl(struct inode *inode,
			  struct file *filp,
			  unsigned int command,
			  unsigned long arg)
{
  return 0;
}

/*
 * The file operations
 */
struct file_operations sharp_buzzer_fops = {
  open:    sharpbuz_open,
  release: sharpbuz_release,
  write:   sharpbuz_write,
  ioctl:   sharpbuz_ioctl
};

/*
 *  init and exit
 */
#include <linux/miscdevice.h>

static struct miscdevice sharpbuz_device = {
  SHARP_BUZZER_MINOR,
  "sharp_buz",
  &sharp_buzzer_fops
};

static int __init sharpbuz_init(void)
{
  if( misc_register(&sharpbuz_device) )
    printk("failed to register sharpbuz\n");

  return 0;
}

static void __exit sharpbuz_cleanup(void)
{
  misc_deregister(&sharpbuz_device);
}

module_init(sharpbuz_init);
module_exit(sharpbuz_cleanup);
