/*
 * drivers/video/tosa_backlight.c 
 *
 * (C) Copyright 2004 Lineo Solutions, Inc.
 *
 * May be copied or modified under the terms of the GNU General Public
 * License.  See linux/COPYING for more information.
 *
 * Based on:
 *
 * linux/drivers/video/corgi_frontlight.c
 *
 * (C) Copyright 2002 SHARP
 *
 * May be copied or modified under the terms of the GNU General Public
 * License.  See linux/COPYING for more information.
 *
 * Based on:
 *
 * linux/drivers/video/poodle_frontlight.c
 *
 * (C) Copyright 2001 Lineo Japan, Inc.
 *
 * May be copied or modified under the terms of the GNU General Public
 * License.  See linux/COPYING for more information.
 *
 * Based on:
 *
 * linux/drivers/video/collie_frontlight.c
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
 * ChangeLog:
 *   02-Dec-2002 SHARP   for SL-C700
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
#include <linux/slab.h>
#include <linux/proc_fs.h>
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

#include <video/tosa_backlight.h>

#include <linux/interrupt.h>

#include <asm/arch/i2sc.h>

extern void pxa_nssp_output(unsigned char, unsigned char);

#if 0
#define DPRINTK(fmt, args...)	printk("%s: " fmt, __FUNCTION__ , ## args)
#else
#define DPRINTK(fmt, args...)
#endif

#ifdef CONFIG_PM
static int is_bl_pm = 0;
static int is_bl_blank = 0;
static int counter_step_save = 0;
#endif

#define BL_SETTING	6
#define BL_DEFAULT	3

int counter_step_contrast = BL_DEFAULT;
static int bl_limit = BL_SETTING - 1;
static spinlock_t bl_lock = SPIN_LOCK_UNLOCKED;

typedef struct _duty_vr_t {
  int duty;
  int vr;
} duty_vr_t;

static duty_vr_t bl_duty_table[BL_SETTING] = {
  {0,  0},	//Light Off
  {11, 1},
  {33, 1},
  {68, 1},
  {100, 1},
  {255, 1},
};

static int tosa_bl_ioctl(struct inode* inode, struct file*, unsigned int,
	       	unsigned long);
static int bl_step_contrast_setting(int);

extern unsigned short set_scoop_gpio(unsigned short);
extern unsigned short reset_scoop_gpio(unsigned short);

#define SetBacklightDuty(a)	set_bl_bright(a);
#define SetBacklightVR(a)	if (a) { TC6393_SYS_REG(TC6393_SYS_GPODSR1) |= TC6393_BL_C20MA; } \
				else { TC6393_SYS_REG(TC6393_SYS_GPODSR1) &= ~TC6393_BL_C20MA; }

/*
 * I2C functions.
 */
#define I2C_TIMEOUT		100	//wait for 5ms
#define I2C_ADR_DAC		0x4e	//DAC Slave address
#define DAC_CH1			0
#define DAC_CH2			1
#define I2C_ICR_DEF_VAL		(ICR_IUE | ICR_SCLE | ICR_FM)
#define i2c_reset()		i2c_init(1)
#define i2c_set()		i2c_init(0)
void i2c_init(int reset)
{
  if( reset ) {
    ICR = ICR_UR;
    CKEN &= ~CKEN14_I2C;
  }
  CKEN |= CKEN14_I2C;		//Set I2C cleck
  ICR = I2C_ICR_DEF_VAL;
  ISR = 0x6f0;			//Clear all status.
}

static int i2c_wait_for_fifo_empty(void)
{
  int timeo = 0;
  while( 1 ) {
    if( ISR & ISR_ITE) break;
    if( timeo++ > I2C_TIMEOUT ) {
      DPRINTK("timeout: %x\n", ISR);
      i2c_reset();
      return -EBUSY;
    }
    mdelay(1);
  }
  ISR = ISR_ITE;
  return 0;
}

static int i2c_write(unsigned char reg, unsigned char val)
{
  int ret;
  
  /* Start condition */
  if( ISR & ISR_IBB ) {
    DPRINTK("bus is busy\n");
    i2c_reset();
    return -EBUSY;
  }
  /* Slave address write */
  IDBR = (I2C_ADR_DAC << 1);
  ICR = (ICR_START | ICR_TB | I2C_ICR_DEF_VAL | ICR_ACKNAK);
  if( (ret = i2c_wait_for_fifo_empty()) < 0 ) return ret;
  
  /* DAC channel write */
  IDBR = reg;
  ICR = (ICR_TB | I2C_ICR_DEF_VAL);
  if( (ret = i2c_wait_for_fifo_empty()) < 0 ) return ret;
  
  /* data write */
  IDBR = val;
  ICR = (ICR_STOP | ICR_TB | I2C_ICR_DEF_VAL | ICR_ACKNAK);
  if( (ret = i2c_wait_for_fifo_empty()) < 0 ) return ret;
  
  ICR = I2C_ICR_DEF_VAL;
  return 0;
}

/*
 * Backlight control functions. 
 */
static void bl_enable(int sw)
{
  if( sw ) pxa_nssp_output(TG_GPODR2, 0x01);	//GP04=1
  else pxa_nssp_output(TG_GPODR2, 0x00);	//GP04=0
}

static int set_bl_bright(unsigned char dat)
{
  return i2c_write(DAC_CH2, dat);
}

#ifdef CONFIG_PROC_FS
struct proc_dir_entry *proc_bl;

static ssize_t bl_read_params(struct file *file, char *buf,
			size_t nbytes, loff_t *ppos)
{
  char obuf[15];
  int count = 0;
  
  if( *ppos>0 ) /* Assume reading completed in previous read*/
    return 0;
  count = sprintf(obuf, "0x%02X%02X\n",
		bl_duty_table[counter_step_contrast].vr,
		bl_duty_table[counter_step_contrast].duty);
  *ppos += count;
  if( count>nbytes )	/* Assume output can be read at one time */
    return -EINVAL;
  if( copy_to_user(buf, obuf, count) )
    return -EFAULT;
  return count;
}

static ssize_t bl_write_params(struct file *file, const char *buf,
		size_t nbytes, loff_t *ppos)
{
  unsigned long param;
  char *endp;

  param = simple_strtoul(buf,&endp,0);
  SetBacklightDuty(param & 0xff);
  SetBacklightVR((param & 0xff00) >> 8);
  return nbytes+endp-buf;
}

static struct file_operations proc_params_operations = {
	read:	bl_read_params,
	write:	bl_write_params,
};
#endif	/* CONFIG_PROC_FS */

#ifdef CONFIG_PM
void tosa_bl_blank(int blank)
{
  if( blank ) {
    if( !is_bl_blank ) {
      is_bl_blank = 1;
      counter_step_save = counter_step_contrast;
      bl_step_contrast_setting(0);
    }
  } else {
    if( is_bl_blank && !is_bl_pm ) {
      bl_step_contrast_setting(counter_step_save);
      is_bl_blank = 0;
    }
  }
}

void tosa_l_blank_power_button(void)
{
	if (!is_bl_blank) {
		is_bl_blank = 1;
		counter_step_save = counter_step_contrast;
		bl_step_contrast_setting(0);
	}else{
		counter_step_contrast = counter_step_save;
		bl_step_contrast_setting(counter_step_contrast);
		is_bl_blank = 0;
	}
}


int tosa_bl_pm_callback(struct pm_dev* pm_dev,
		pm_request_t req, void *data)
{
  switch (req) {
  case PM_SUSPEND:
    is_bl_pm = 1;
    tosa_bl_blank(1);
    break;
  case PM_RESUME:
    is_bl_pm = 0;
    tosa_bl_blank(0);
    break;
  }
  return 0;
}
#endif	/* CONFIG_PM */

static struct file_operations tosa_bl_fops = {
  ioctl: tosa_bl_ioctl,
};

static int bl_major;

#define CHECK_BATTERY_TIME	1
static int bl_step_contrast_setting_nocheck(int need_value)
{
  unsigned long flags;

  /* Check value */
  if( need_value < 0 ) need_value = 0;
  if( need_value > bl_limit ) need_value = bl_limit;

  spin_lock_irqsave(&bl_lock, flags);
  if(need_value > 0) bl_enable(1);
  else bl_enable(0);
  SetBacklightDuty(bl_duty_table[need_value].duty);
  SetBacklightVR(bl_duty_table[need_value].vr);
  spin_unlock_irqrestore(&bl_lock, flags);
  counter_step_contrast = need_value;
  
  return counter_step_contrast;
}


static int bl_step_contrast_setting(int need_value)
{
  int ret = 0;
  extern void sharpsl_kick_battery_check(int,int,int);
  sharpsl_kick_battery_check(0,1,0);	// check battery and wait 10msec
  ret = bl_step_contrast_setting_nocheck(need_value);
  sharpsl_kick_battery_check(1,0,0);	// wait 10msec and check battery
  return ret;
}

static int temporary_contrast_set_flag = 0;

void tosa_bl_temporary_contrast_set(void)
{
  int need_value = counter_step_contrast;

  if( temporary_contrast_set_flag ) return;

  temporary_contrast_set_flag = 1;
  bl_step_contrast_setting(need_value);
}

void tosa_bl_temporary_contrast_reset(void)
{
  int need_value = counter_step_contrast;

  if( !temporary_contrast_set_flag ) return;

  temporary_contrast_set_flag = 0;
  bl_step_contrast_setting(need_value);
}

void tosa_bl_set_limit_contrast(int val)
{
  unsigned long flags;
  spin_lock_irqsave(&bl_lock, flags);
  if( (val > BL_SETTING - 1) || (val < 0) ) {
    if( bl_limit != BL_SETTING - 1 ) {
      printk("bl : unlimit contrast\n");
    }
    bl_limit = BL_SETTING - 1;
  } else {
    if( bl_limit != val ) {
      printk("bl : change limit contrast %d\n", val);
    }
    bl_limit = val;
  }
  spin_unlock_irqrestore(&bl_lock, flags);
  if( counter_step_contrast > bl_limit )
    bl_step_contrast_setting_nocheck(bl_limit);
}

int tosa_set_common_voltage(unsigned char dat)
{
    return i2c_write(DAC_CH1, dat);
}

static int tosa_bl_ioctl(struct inode* inode, struct file* filp,
		unsigned int cmd, unsigned long arg)
{
  int ret = -EINVAL;
	
  switch(cmd) {
  case TOSA_BL_IOCTL_ON:
    if (is_bl_blank) return 0;
    ret = bl_step_contrast_setting_nocheck(counter_step_contrast);
    break;
  case TOSA_BL_IOCTL_OFF:
    if (is_bl_blank) return 0;
    ret = bl_step_contrast_setting_nocheck(0);
    break;
  case TOSA_BL_IOCTL_STEP_CONTRAST:
    if (is_bl_blank) return 0;
    ret = bl_step_contrast_setting_nocheck(arg);
    break;
  case TOSA_BL_IOCTL_GET_STEP_CONTRAST:
    ret = counter_step_contrast;
    break;
  case TOSA_BL_IOCTL_GET_STEP:
    ret = BL_SETTING;
    break;
  default:
    ;
  }

  return ret;
}

static __init int tosa_bl_init(void)
{
  int ret;

  bl_major = BL_MAJOR;

  if( (ret = register_chrdev(bl_major, BL_NAME, &tosa_bl_fops)) < 0 ) {
    DPRINTK("%s: cant get major %d\n", BL_NAME, bl_major);
    return ret;
  }
  
  if( !bl_major ) bl_major = ret;

  bl_step_contrast_setting_nocheck(counter_step_contrast);

#ifdef CONFIG_PROC_FS
  {
    struct proc_dir_entry *entry;
    
    proc_bl = proc_mkdir("driver/fl", NULL);
    if (proc_bl == NULL) {
      bl_step_contrast_setting(0);
      unregister_chrdev(bl_major, BL_NAME);
      printk(KERN_ERR "%s: can't create /proc/driver/fl\n", BL_NAME);
      return -ENOMEM;
    }
    entry = create_proc_entry(BL_NAME, S_IWUSR |S_IRUSR | S_IRGRP | S_IROTH,
			    proc_bl);
    if( entry ) {
      entry->proc_fops = &proc_params_operations;
    } else {
      remove_proc_entry("driver/fl", &proc_root);
      proc_bl = 0;
      bl_step_contrast_setting(0);
      unregister_chrdev(bl_major, BL_NAME);
      printk(KERN_ERR "%s: can't create /proc/driver/fl/\n", BL_NAME);
      return -ENOMEM;
    }
  }
#endif	/* CONFIG_PROC_FS */

  printk(KERN_INFO "%s: Initialized.\n", BL_NAME);

#if 0
  {	/* IOCTL TEST */
    int i;
    printk("\n%s: ioctl test.\n", BL_NAME);
    printk("%s: Backlight off\n", BL_NAME);
    ret = tosa_bl_ioctl(0, 0, TOSA_BL_IOCTL_OFF, 0);
    if(ret < 0) printk("%s: error:%d\n", BL_NAME, ret);
    mdelay(5000);
    printk("%s: Backlight on\n", BL_NAME);
    ret = tosa_bl_ioctl(0, 0, TOSA_BL_IOCTL_ON, 0);
    if(ret < 0) printk("%s: error:%d\n", BL_NAME, ret);
    mdelay(5000);
    for(i = 1; i < BL_SETTING; i++) {
      printk("%s: Backlight step: %d\n", BL_NAME, i);
      ret = tosa_bl_ioctl(0, 0, TOSA_BL_IOCTL_STEP_CONTRAST, i);
      if(ret < 0) printk("%s: error:%d\n", BL_NAME, ret);
      mdelay(5000);
    }
  }
#endif
  return 0;
}

#ifdef MODULE
void __exit tosa_bl_exit(void)
{
  bl_step_contrast_setting(0);

  unregister_chrdev(bl_major, BL_NAME);

#ifdef CONFIG_PROC_FS
  {
    remove_proc_entry(BL_NAME, proc_bl);
    remove_proc_entry("driver/fl", NULL);
    proc_bl = 0;
  }
#endif	/* CONFIG_PROC_FS */
  printk(KERN_INFO "%s: Unloaded\n", BL_NAME);
}

module_exit(tosa_bl_exit);
#endif	/* MODULE */

module_init(tosa_bl_init);
