/*
 *  linux/drivers/char/sharp_audioctl.c
 *
 *  Driver for Audio I/O Ports On SHARP PDA
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
#include <linux/poll.h>

#include <asm/system.h>
#include <asm/uaccess.h>
#include <asm/io.h>
#include <asm/irq.h>
#include <asm/bitops.h>
#include <asm/arch/hardware.h>

#include <asm/sharp_char.h>
#include <asm/arch/gpio.h>

#ifndef SHARP_AUDIOCTL_MAJOR
# define SHARP_AUDIOCTL_MAJOR 229 /* number of bare sharp_gsm driver */
#endif

#define DEBUGPRINT(s)   /* printk s */

#if defined(CONFIG_IRIS)
extern int iris_audioctl_open(struct inode *inode, struct file *filp);
extern int iris_audioctl_release(struct inode *inode, struct file *filp);
extern int iris_audioctl_ioctl(struct inode *inode,
			       struct file *filp,
			       unsigned int command,
			       unsigned long arg);
#define arch_open(inode,filp)     iris_audioctl_open((inode),(filp))
#define arch_release(inode,filp)  iris_audioctl_release((inode),(filp))
#define arch_ioctl(inode,filp,cmd,arg) iris_audioctl_ioctl((inode),(filp),(cmd),(arg))
#else
#warn "sharp audioctl interface not implemented"
#define arch_open(inode,filp)          0
#define arch_release(inode,filp)       0
#define arch_ioctl(inode,filp,cmd,arg) (-EINVAL)
#endif

/*
 * operations....
 */

static int device_used = 0;

static int sharpaudioctl_open(struct inode *inode, struct file *filp)
{
  int minor = MINOR(inode->i_rdev);
  if( minor != SHARP_AUDIOCTL_MINOR ) return -ENODEV;
  if( ! device_used ){
    arch_open(inode,filp);
    DEBUGPRINT(("SHARP AudioCtl Arch Opened %x\n",minor));
  }
  DEBUGPRINT(("SHARP AudioCtl Opened %x\n",minor));
  device_used++;
  MOD_INC_USE_COUNT;
  return 0;
}

static int sharpaudioctl_release(struct inode *inode, struct file *filp)
{
  DEBUGPRINT(("SHARP AudioCtl Closed\n"));
  device_used--;
  if( ! device_used ){
    arch_release(inode,filp);
    DEBUGPRINT(("SHARP AudioCtl Arch Free %x\n",minor));
  }
  MOD_DEC_USE_COUNT;
  return 0;
}

static int sharpaudioctl_ioctl(struct inode *inode,
			  struct file *filp,
			  unsigned int command,
			  unsigned long arg)
{
  int error;
  switch( command ) {
  default:
    return arch_ioctl(inode,filp,command,arg);
  }
  return 0;
}

/*
 * The file operations
 */
struct file_operations sharp_audioctl_fops = {
  open:    sharpaudioctl_open,
  release: sharpaudioctl_release,
  ioctl:   sharpaudioctl_ioctl
};

/*
 *  init and exit
 */
#if defined(SHARPCHAR_USE_MISCDEV)

#include <linux/miscdevice.h>

static struct miscdevice sharpaudioctl_device = {
  SHARP_AUDIOCTL_MINOR,
  "sharp_audioctl",
  &sharp_audioctl_fops
};

static int __init sharpaudioctl_init(void)
{
  DEBUGPRINT(("sharpaudioctl init\n"));
  if( misc_register(&sharpaudioctl_device) )
    printk("failed to register sharpaudioctl\n");
  return 0;
}

static void __exit sharpaudioctl_cleanup(void)
{
  DEBUGPRINT(("sharpaudioctl init\n"));
  if( misc_register(&sharpaudioctl_device) )
    printk("failed to register sharpaudioctl\n");
  return 0;
}

module_init(sharpaudioctl_init);
module_exit(sharpaudioctl_cleanup);

#else /* ! SHARPCHAR_USE_MISCDEV */

#if 0 /* bare driver should not be supported */

static int sharpaudioctl_major = SHARP_AUDIOCTL_MAJOR;

static int __init sharpaudioctl_init(void)
{
  int result;
  DEBUGPRINT(("sharpaudioctl init\n"));
  if( ( result = register_chrdev(sharpaudioctl_major,"sharpaudioctl",&sharp_audioctl_fops) ) < 0 ){
    DEBUGPRINT(("sharpaudioctl failed\n"));
    return result;
  }
  if( sharpaudioctl_major == 0 ) sharpaudioctl_major = result; /* dynamically assigned */
  DEBUGPRINT(("sharpaudioctl registered %d\n",sharpaudioctl_major));
  return 0;
}

static void __exit sharpaudioctl_cleanup(void)
{
  DEBUGPRINT(("sharpaudioctl cleanup\n"));
  unregister_chrdev(sharpaudioctl_major,"sharpaudioctl");
}

module_init(sharpaudioctl_init);
module_exit(sharpaudioctl_cleanup);
#endif

#endif /* ! SHARPCHAR_USE_MISCDEV */

/*
 *   end of source
 */
