/*
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

#include <video/corgi_backlight.h>

#include <linux/interrupt.h>


static int is_corgibl_pm = 0;

#define CORGI_LIGHT_SETTING 7     // range setting : 0(OFF) dim 1 2 3 4 5(MAX)
#define CORGI_LIGHT_DEFAULT 6
static int is_corgibl_blank = 0;
int counter_step_contrast = CORGI_LIGHT_DEFAULT;
static corgibl_limit = CORGI_LIGHT_SETTING - 1;

static int counter_step_save     = 0;

static int corgibl_ioctl(struct inode* inode,
						 struct file*  filp,
						 unsigned int  cmd,
						 unsigned long arg);

static int corgibl_step_contrast_setting(int);

static spinlock_t bl_lock = SPIN_LOCK_UNLOCKED;

#ifdef CONFIG_PROC_FS
struct proc_dir_entry *proc_bl;

typedef struct _duty_vr_t {
	int duty;
	int vr;
} duty_vr_t;

static duty_vr_t corgibl_duty_table[CORGI_LIGHT_SETTING] = {
	{0x00, 0},	//   0%		Light Off
	{0x01, 0},	//  20%		Dim
	{0x02, 0},	//  25%		1
	{0x0b, 0},	//  70%		2
	{0x05, 1},	//  40%		3
	{0x0b, 1},	//  70%		4
	{0x1f, 1}	// 100%		5
};

#define SetBacklightDuty(a)	ssp_put_dac_val(0x40 | ((a) & 0x1f), CS_LZ9JG18);
#define SetBacklightVR(a)	if (a) {set_scoop_gpio(SCP_BACKLIGHT_CONT);} else {reset_scoop_gpio(SCP_BACKLIGHT_CONT);}

static ssize_t corgibl_read_params(struct file *file, char *buf,
								   size_t nbytes, loff_t *ppos)
{
	char outputbuf[15];
	int count;
	if (*ppos>0) /* Assume reading completed in previous read*/
		return 0;
	count = sprintf(outputbuf, "0x%02X%02X\n",
					corgibl_duty_table[counter_step_contrast].vr,
					corgibl_duty_table[counter_step_contrast].duty);
	*ppos += count;
	if (count>nbytes)	/* Assume output can be read at one time */
		return -EINVAL;
	if (copy_to_user(buf, outputbuf, count))
		return -EFAULT;
	return count;
}

static ssize_t corgibl_write_params(struct file *file, const char *buf,
									size_t nbytes, loff_t *ppos)
{
	unsigned long		param;
	char			*endp;
	unsigned long flags;

	param = simple_strtoul(buf,&endp,0);
	SetBacklightDuty(param & 0xff);
	SetBacklightVR((param & 0xff00) >> 8);
	return nbytes+endp-buf;
}

static struct file_operations proc_params_operations = {
	read:	corgibl_read_params,
	write:	corgibl_write_params,
};
#endif

void corgibl_blank(int blank)
{
	if (blank) {
		if (!is_corgibl_blank) {
			is_corgibl_blank = 1;
			counter_step_save = counter_step_contrast;
			corgibl_step_contrast_setting(0);
		}
	} else {
		if (is_corgibl_blank && !is_corgibl_pm) {
			corgibl_step_contrast_setting(counter_step_save);
			is_corgibl_blank = 0;
		}
	}
}


int corgibl_pm_callback(struct pm_dev* pm_dev,
							   pm_request_t req, void *data)
{
	switch (req) {
	case PM_SUSPEND:
		is_corgibl_pm = 1;
		corgibl_blank(1);
		break;
	case PM_RESUME:
		is_corgibl_pm = 0;
		corgibl_blank(0);
		break;
	}
	return 0;
}

static struct file_operations corgibl_fops = {
	ioctl:		corgibl_ioctl,
};

static int corgibl_major;

#define CHECK_BATTERY_TIME	1
static int corgibl_step_contrast_setting(int need_value)
{
	int ac_in = 0;
	unsigned long flags;
	extern void sharpsl_kick_battery_check(int,int,int);

	sharpsl_kick_battery_check(0,1,0);	// check battery and wait 10msec

	if (need_value < 0 ) need_value = 0;
	if ( need_value > corgibl_limit )
		need_value = corgibl_limit;

	spin_lock_irqsave(&bl_lock, flags);
	SetBacklightDuty(corgibl_duty_table[need_value].duty);
	SetBacklightVR(corgibl_duty_table[need_value].vr);
	spin_unlock_irqrestore(&bl_lock, flags);
	counter_step_contrast = need_value;

	sharpsl_kick_battery_check(1,0,0);	// wait 10msec and check battery

	return counter_step_contrast;
}

static int corgibl_step_contrast_setting_nocheck(int need_value)
{
	int ac_in = 0;
	unsigned long flags;


	if (need_value < 0 ) need_value = 0;
	if ( need_value > corgibl_limit )
		need_value = corgibl_limit;

	spin_lock_irqsave(&bl_lock, flags);
	SetBacklightDuty(corgibl_duty_table[need_value].duty);
	SetBacklightVR(corgibl_duty_table[need_value].vr);
	spin_unlock_irqrestore(&bl_lock, flags);

	counter_step_contrast = need_value;

	return counter_step_contrast;
}


static int	temporary_contrast_set_flag = 0;

void corgibl_temporary_contrast_set(void)
{
	int need_value = counter_step_contrast;
	unsigned long flags;

	if (temporary_contrast_set_flag) {
		return;
	}
	temporary_contrast_set_flag = 1;
	corgibl_step_contrast_setting(need_value);
}

void corgibl_temporary_contrast_reset(void)
{
	int need_value = counter_step_contrast;
	unsigned long flags;

	if (!temporary_contrast_set_flag) {
		return;
	}
	temporary_contrast_set_flag = 0;
	corgibl_step_contrast_setting(need_value);
}

void corgibl_set_limit_contrast(int val)
{
	unsigned long flags;
	spin_lock_irqsave(&bl_lock, flags);
	if ((val > CORGI_LIGHT_SETTING - 1) || (val < 0)) {
		if (corgibl_limit != CORGI_LIGHT_SETTING - 1) {
			printk("corgi_bl : unlimit contrast\n");
		}
		corgibl_limit = CORGI_LIGHT_SETTING - 1;
	}
	else {
		if (corgibl_limit != val) {
			printk("corgi_bl : change limit contrast %d\n", val);
		}
		corgibl_limit = val;
	}
	spin_unlock_irqrestore(&bl_lock, flags);
	if (counter_step_contrast > corgibl_limit) {
		corgibl_step_contrast_setting_nocheck(corgibl_limit);
	}
}

static int corgibl_ioctl(struct inode* inode,
						 struct file*  filp,
						 unsigned int  cmd,
						 unsigned long arg)
{
	int ret;

	ret = (-EINVAL);
	
	switch(cmd) {
	case CORGI_BL_IOCTL_ON:
		ret = corgibl_step_contrast_setting(counter_step_contrast);
		break;
	case CORGI_BL_IOCTL_OFF:
		ret = corgibl_step_contrast_setting(0);
		break;
	case CORGI_BL_IOCTL_STEP_CONTRAST:
		ret = corgibl_step_contrast_setting(arg);
		break;
	case CORGI_BL_IOCTL_GET_STEP_CONTRAST:
		ret = counter_step_contrast;
		break;
	case CORGI_BL_IOCTL_GET_STEP:
		ret = CORGI_LIGHT_SETTING;
		break;
	default:
		;
	}
	
	return ret;
}

int __init corgibl_init(void)
{
	int result;

	corgibl_major = BL_MAJOR;

	result = register_chrdev(corgibl_major, BL_NAME, &corgibl_fops);

	if (result < 0) {
#ifdef DEBUG
	  	printk(KERN_WARNING "corgibl: cant get major %d\n", 
		       corgibl_major);
#endif
		return result;
	}
  
	if (corgibl_major == 0) {
	  	corgibl_major = result;
	}

	corgibl_step_contrast_setting(counter_step_contrast);

#ifdef CONFIG_PROC_FS
	{
		struct proc_dir_entry *entry;

		proc_bl = proc_mkdir("driver/fl", NULL);
		if (proc_bl == NULL) {
			corgibl_step_contrast_setting(0);
			unregister_chrdev(corgibl_major, "corgi-bl");
			printk(KERN_ERR "corgibl: can't create /proc/driver/fl\n");
			return -ENOMEM;
		}
		entry = create_proc_entry("corgi-bl",
								  S_IWUSR |S_IRUSR | S_IRGRP | S_IROTH,
								  proc_bl);
		if (entry) {
			entry->proc_fops = &proc_params_operations;
		} else {
			remove_proc_entry("driver/fl", &proc_root);
			proc_bl = 0;
			corgibl_step_contrast_setting(0);
			unregister_chrdev(corgibl_major, "corgi-bl");
			printk(KERN_ERR "corgibl: can't create /proc/driver/fl/\n");
			return -ENOMEM;
		}
	}
#endif

	printk("Backlight Driver Initialized.\n");
  
	return 0;
}

#ifdef MODULE
void __exit corgibl_exit(void)
{
	corgibl_step_contrast_setting(0);

	unregister_chrdev(corgibl_major, "corgi-bl");

#ifdef CONFIG_PROC_FS
	{
		remove_proc_entry("corgi-bl", proc_bl);
		remove_proc_entry("driver/fl", NULL);
		proc_bl = 0;
	}
#endif
	printk("Backlight Driver Unloaded\n");
}
#endif

module_init(corgibl_init);
#ifdef MODULE
module_exit(corgibl_exit);
#endif
