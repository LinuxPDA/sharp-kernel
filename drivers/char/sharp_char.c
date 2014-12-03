/*
 *  linux/drivers/char/sharp_char.c
 *
 *  Driver for misc character devices On SHARP PDA
 *
 *  ChangeLog:
 *   04-16-2001 Lineo Japan, Inc. ...
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

#include <asm/system.h>
#include <asm/io.h>
#include <asm/irq.h>
#include <asm/bitops.h>
#include <asm/arch/hardware.h>

#include <asm/arch/gpio.h>

#include <asm/sharp_char.h>

#define DEBUGPRINT(s)   printk s

/*
 *
 */

#ifdef SHARP_LED_MINOR
extern struct file_operations sharp_led_fops;
# define SHARP_LED_FOPS_P   &sharp_led_fops
#else
# define SHARP_LED_FOPS_P   NULL
#endif

#ifdef SHARP_BUZZER_MINOR
extern struct file_operations sharp_buzzer_fops;
# define SHARP_BUZZER_FOPS_P   &sharp_buzzer_fops
#else
# define SHARP_BUZZER_FOPS_P   NULL
#endif

#ifdef SHARP_GSM_MINOR
extern struct file_operations sharp_gsm_fops;
# define SHARP_GSM_FOPS_P   &sharp_gsm_fops
#else
# define SHARP_GSM_FOPS_P   NULL
#endif

#ifdef SHARP_AUDIOCTL_MINOR
extern struct file_operations sharp_audioctl_fops;
# define SHARP_AUDIOCTL_FOPS_P   &sharp_audioctl_fops
#else
# define SHARP_AUDIOCTL_FOPS_P   NULL
#endif

#ifdef SHARP_KBDCTL_MINOR
extern struct file_operations sharp_kbdctl_fops;
# define SHARP_KBDCTL_FOPS_P   &sharp_kbdctl_fops
#else
# define SHARP_KBDCTL_FOPS_P   NULL
#endif

/*
 * low level drivers definition...
 */

struct file_operations *sharpdev_fop_array[(SHARP_DEV_MINOR_MAX+1)] = {
  SHARP_LED_FOPS_P, /* for SHARP_LED_MINOR */
  SHARP_BUZZER_FOPS_P, /* for SHARP_BUZZER_MINOR */
  SHARP_GSM_FOPS_P, /* for SHARP_GSM_MINOR */
  SHARP_AUDIOCTL_FOPS_P, /* for SHARP_AUDIOCTL_MINOR */
  SHARP_KBDCTL_FOPS_P, /* for SHARP_KBDCTL_MINOR */
};

/*
 * operations....
 */

static int sharpdev_open(struct inode *inode, struct file *filp)
{
  int dev = MINOR(inode->i_rdev) - SHARP_DEV_MINOR_START;
  /* check minor */
  if( dev < 0 || dev > SHARP_DEV_MINOR_MAX ){
    /* no such minor */
    return -ENODEV;
  }
  if( ! sharpdev_fop_array[dev] ){
    /* device not registered */
    return -ENODEV;
  }
  filp->f_op = sharpdev_fop_array[dev];
  return filp->f_op->open(inode, filp); /* dispatch to specific open */
}

static int sharpdev_release(struct inode *inode, struct file *filp)
{
  /* never used ... */
  return 0;
}

/*
 * The file operations
 */
struct file_operations sharpdev_fops = {
  open:    sharpdev_open,
  release: sharpdev_release,
};

/*
 *  init and exit
 */
static int __init sharpdev_init(void)
{
  DEBUGPRINT(("sharp-dev init\n"));
  if( register_chrdev(SHARP_DEV_MAJOR,"sharpdev",&sharpdev_fops) < 0 ){
    DEBUGPRINT(("sharp-dev failed\n"));
    return -EIO;
  }
  DEBUGPRINT(("sharp-dev registered %d\n",SHARP_DEV_MAJOR));
  return 0;
}

static void __exit sharpdev_cleanup(void)
{
  unregister_chrdev(SHARP_DEV_MAJOR,"sharpdev");
}

module_init(sharpdev_init);
module_exit(sharpdev_cleanup);

/*
 *   end of source
 */
