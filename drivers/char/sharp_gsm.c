/*
 *  linux/drivers/char/sharp_gsm.c
 *
 *  Driver for GSM I/O Ports On SHARP PDA
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
#include <linux/pm.h>

#include <asm/system.h>
#include <asm/uaccess.h>
#include <asm/io.h>
#include <asm/irq.h>
#include <asm/bitops.h>
#include <asm/arch/hardware.h>

#include <asm/sharp_char.h>
#include <asm/arch/gpio.h>

#ifndef SHARP_GSM_MAJOR
# define SHARP_GSM_MAJOR 228 /* number of bare sharp_gsm driver */
#endif

#define DEBUGPRINT(s)   /* printk s */


#if defined(CONFIG_IRIS)
extern int iris_read_carkit_val(void);
extern int iris_sharpgsm_release(struct inode *inode, struct file *filp);
extern int iris_sharpgsm_open(struct inode *inode, struct file *filp);
extern int iris_sharpgsm_ioctl(struct inode *inode,struct file *filp,unsigned int command,unsigned long arg);
extern int iris_sharpgsm_init(void);
extern int iris_sharpgsm_resume(void);
#define read_carkit_val()          iris_read_carkit_val()
#define read_headphonemic_val()    iris_read_headphonemic_val()
#define read_headphonemic_status() iris_read_headphonemic_status()
#define arch_open(inode,filp)      iris_sharpgsm_open((inode),(filp))
#define arch_release(inode,filp)   iris_sharpgsm_release((inode),(filp))
#define arch_ioctl(inode,filp,cmd,arg) iris_sharpgsm_ioctl((inode),(filp),(cmd),(arg))
#define arch_init()                iris_sharpgsm_init();
#define arch_suspend()             0
#define arch_resume()              iris_sharpgsm_resume()
#else
#warn "sharp gsm interface not implemented"
#define read_carkit_val()          0
#define arch_open(inode,filp)      0
#define arch_release(inode,filp)   0
#define arch_ioctl(inode,filp,cmd,arg) (-EINVAL)
#define arch_init()                0
#endif

/*
 *  GSM Mode IOCTL
 */

int gsm_modem_status = GSM_PHONE_NO_CONNECTION;

/*
 * operations....
 */

static struct fasync_struct	*fasyncptr;
static DECLARE_WAIT_QUEUE_HEAD(wq_in);

void gsmcarkit_status_changed(void)
{
  wake_up_interruptible(&wq_in);
  if( fasyncptr ){
    kill_fasync(&fasyncptr, SIGIO, POLL_IN);
  }
}

static int sharpgsm_fasync(int fd, struct file *filep, int mode)
{
  int	ret;
  ret = fasync_helper((int)fd, filep, mode, &fasyncptr);
  if (ret < 0) return ret;
  return 0;
}

static uint sharpgsm_poll(struct file *filep, poll_table *wait)
{
  uint	mask = 0;
  static int carkitval = -1;
  static int extportval = -1;
  int c;
  int e;
  c = read_carkit_val();
  e = read_headphonemic_status();
  if( c != carkitval || e != extportval ){
    mask = POLLIN|POLLRDNORM;
    goto END;
  }
  carkitval = c;
  extportval = e;

  poll_wait(filep, &wq_in, wait);

  c = read_carkit_val();
  e = read_headphonemic_status();
  if( c != carkitval || e != extportval ){
    mask = POLLIN|POLLRDNORM;
    goto END;
  }


END:
  carkitval = c;
  extportval = e;
  return mask;
}

static int device_used = 0;

static int sharpgsm_open(struct inode *inode, struct file *filp)
{
  int minor = MINOR(inode->i_rdev);
  if( minor != SHARP_GSM_MINOR ) return -ENODEV;
  sharpgsm_fasync(-1, filp, 0);
  if( ! device_used ){
    arch_open(inode,filp);
    DEBUGPRINT(("SHARP Gsm Arch Open%x\n",minor));
    fasyncptr = NULL;
  }
  DEBUGPRINT(("SHARP Gsm Opened %x\n",minor));
  device_used++;
  MOD_INC_USE_COUNT;
  return 0;
}

static int sharpgsm_release(struct inode *inode, struct file *filp)
{
  DEBUGPRINT(("SHARP Gsm Closed\n"));
  device_used--;
  if( ! device_used ){
    arch_release(inode,filp);
    DEBUGPRINT(("SHARP Gsm Arch Close%x\n",minor));
  }
  MOD_DEC_USE_COUNT;
  return 0;
}

static int sharpgsm_ioctl(struct inode *inode,
			  struct file *filp,
			  unsigned int command,
			  unsigned long arg)
{
  int error;
  switch( command ) {
  case SHARP_GSM_GETEXTSTATUS:
    {
      sharp_gsmext_status* puser = (sharp_gsmext_status*)arg;
      int carkit;
      int headphone_mic;
      int audio_status;
      int external_sp;
      DEBUGPRINT(("sharpgsm getext status\n"));
      carkit = read_carkit_val();
      audio_status = read_headphonemic_status();
      headphone_mic = ( audio_status == IRIS_AUDIO_EXT_IS_HEADPHONEMIC ? 1 : 0 );
      external_sp = ( audio_status == IRIS_AUDIO_EXT_IS_EXTSPEAKER ? 1 : 0 );
      error = put_user(carkit,&(puser->carkit));
      if( error ) return error;
      error = put_user(headphone_mic,&(puser->headphone_mic));
      if( error ) return error;
      error = put_user(external_sp,&(puser->external_sp));
      if( error ) return error;
    }
    break;
  case SHARP_GSM_INFO_TELL_MODE:
    switch(arg){
    case GSM_PHONE_NO_CONNECTION:
    case GSM_PHONE_IN_ANALOG_MODE:
    case GSM_PHONE_IN_DATA_MODE:
      gsm_modem_status = arg;
      break;
    default:
      break;
    }
    break;
  default:
    return arch_ioctl(inode,filp,command,arg);
  }
  return 0;
}

/*
 * The file operations
 */
struct file_operations sharp_gsm_fops = {
  open:    sharpgsm_open,
  release: sharpgsm_release,
  fasync:  sharpgsm_fasync,
  poll:	   sharpgsm_poll,
  ioctl:   sharpgsm_ioctl
};

/*
 *  power management
 */
#ifdef CONFIG_PM

static struct pm_dev *gsmctl_pm_dev;

static int gsmctl_pm_callback(struct pm_dev *pm_dev,
			      pm_request_t req, void *data)
{
  static int suspended = 0;
  switch (req) {
  case PM_STANDBY:
  case PM_BLANK:
  case PM_SUSPEND:
    {
      if( suspended ) return;
      arch_suspend();
      suspended = 1;
    }
    break;
  case PM_RESUME:
  case PM_UNBLANK:
    {
      if( ! suspended ) return;
      suspended = 0;
      arch_resume();
    }
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

static struct miscdevice sharpgsmctl_device = {
  SHARP_GSM_MINOR,
  "sharp_gsmctl",
  &sharp_gsm_fops
};

static int __init sharpgsm_init(void)
{
  DEBUGPRINT(("sharpgsmctl init\n"));
  if( misc_register(&sharpgsmctl_device) )
    printk("failed to register sharpgsmctl\n");
  arch_init();

#ifdef CONFIG_PM
  gsmctl_pm_dev = pm_register(PM_SYS_DEV, 0, gsmctl_pm_callback);
#endif

  return 0;
}

static void __exit sharpgsm_cleanup(void)
{
  DEBUGPRINT(("sharpgsmctl cleanup\n"));
  misc_deregister(&sharpgsmctl_device);
}

module_init(sharpgsm_init);
module_exit(sharpgsm_cleanup);



#else /* ! SHARPCHAR_USE_MISCDEV */

#if 0 /* bare driver should not be supported */

static int sharpgsm_major = SHARP_GSM_MAJOR;

static int __init sharpgsm_init(void)
{
  int result;
  DEBUGPRINT(("sharpgsm init\n"));
  if( ( result = register_chrdev(sharpgsm_major,"sharpgsm",&sharp_gsm_fops) ) < 0 ){
    DEBUGPRINT(("sharpgsm failed\n"));
    return result;
  }
  if( sharpgsm_major == 0 ) sharpgsm_major = result; /* dynamically assigned */
  DEBUGPRINT(("sharpgsm registered %d\n",sharpgsm_major));
  return 0;
}

static void __exit sharpgsm_cleanup(void)
{
  DEBUGPRINT(("sharpgsm cleanup\n"));
  unregister_chrdev(sharpgsm_major,"sharpgsm");
}

module_init(sharpgsm_init);
module_exit(sharpgsm_cleanup);
#endif

#endif /* ! SHARPCHAR_USE_MISCDEV */

/*
 *   end of source
 */
