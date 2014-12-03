/*
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
 *   04-09-2001 Lineo Japan
 *   04-16-2001
 *   07-27-2001 SHARP	add logo screen
 *   10-01-2001 SHARP	change duty setting for touch noise reduction
 *   11-30-2001 SHARP   for SL-5500
 *   30-Jul-2002 Lineo Japan, Inc.  for 2.4.18
 *   02-Dec-2002 SHARP  for SL-B500/5600
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

#include <video/poodle_frontlight.h>

#include <linux/interrupt.h>

#ifdef CONFIG_PM
static struct pm_dev *fb_pm_dev;
static int is_poodlefl_pm = 0;
#endif

#define	LCM_ALC_EN	0x8000
#define	LCM_ALC_INV	0x4000
#define	LCM_ALC_HSYSEN	0x0100
#define	LCM_ALC_BPWF	0x00ff
#define	LCM_ALD_BPDF	0x01ff


#define MIN_CONTRAST   0
#define MAX_CONTRAST   100

#define POODLE_LIGHT_SETTING	5     // range setting : 0(OFF) 1 2 3 4(MAX)
#define POODLE_LIGHT_DEFAULT    4
static int is_poodlefl_blank = 0;
int counter_step_contrast = POODLE_LIGHT_DEFAULT;

int counter_step_save     = 0;
static poodlefl_limit = POODLE_LIGHT_SETTING - 1;

static int poodlefl_ioctl(struct inode* inode,
			   struct file*  filp,
			   unsigned int  cmd,
			   unsigned long arg);

static int poodlefl_enable_accel(void);
static int poodlefl_disable_accel(void);
static int poodlefl_step_contrast_setting(int);


#define	VR_ON	1
#define	VR_OFF	0


#define FL_LVL0_BPWF		161
#define FL_LVL1_BPWF		161
#define FL_LVL2_BPWF		148
#define FL_LVL3_BPWF		161
#define FL_LVL4_BPWF		161
#define FL_LVL0_BPDF		0
#define FL_LVL1_BPDF		117
#define FL_LVL2_BPDF		163
#define FL_LVL3_BPDF		194
#define FL_LVL4_BPDF		194
#define FL_LVL0_VR		VR_OFF
#define FL_LVL1_VR		VR_OFF
#define FL_LVL2_VR		VR_OFF
#define FL_LVL3_VR		VR_OFF
#define FL_LVL4_VR		VR_ON
#define	FL_LVL0_DEFAULT		((FL_LVL0_VR<<15)|FL_LVL0_BPDF)
#define	FL_LVL1_DEFAULT		((FL_LVL1_VR<<15)|FL_LVL1_BPDF)
#define	FL_LVL2_DEFAULT		((FL_LVL2_VR<<15)|FL_LVL2_BPDF)
#define	FL_LVL3_DEFAULT		((FL_LVL3_VR<<15)|FL_LVL3_BPDF)
#define	FL_LVL4_DEFAULT		((FL_LVL4_VR<<15)|FL_LVL4_BPDF)

#define FL_LVL0_BPWF_AC		161
#define FL_LVL1_BPWF_AC		161
#define FL_LVL2_BPWF_AC		148
#define FL_LVL3_BPWF_AC		161
#define FL_LVL4_BPWF_AC		161
#define FL_LVL0_BPDF_AC		0
#define FL_LVL1_BPDF_AC		117
#define FL_LVL2_BPDF_AC		163
#define FL_LVL3_BPDF_AC		194
#define FL_LVL4_BPDF_AC		194
#define FL_LVL0_VR_AC		VR_OFF
#define FL_LVL1_VR_AC		VR_OFF
#define FL_LVL2_VR_AC		VR_OFF
#define FL_LVL3_VR_AC		VR_OFF
#define FL_LVL4_VR_AC		VR_ON
#define	FL_LVL0_AC_DEFAULT	((FL_LVL0_VR_AC<<15)|FL_LVL0_BPDF_AC)
#define	FL_LVL1_AC_DEFAULT	((FL_LVL1_VR_AC<<15)|FL_LVL1_BPDF_AC)
#define	FL_LVL2_AC_DEFAULT	((FL_LVL2_VR_AC<<15)|FL_LVL2_BPDF_AC)
#define	FL_LVL3_AC_DEFAULT	((FL_LVL3_VR_AC<<15)|FL_LVL3_BPDF_AC)
#define	FL_LVL4_AC_DEFAULT	((FL_LVL4_VR_AC<<15)|FL_LVL4_BPDF_AC)


#define FL_LVL0_BPDF_TP		0
#define FL_LVL1_BPDF_TP		106
#define FL_LVL2_BPDF_TP		180
#define FL_LVL3_BPDF_TP		180
#define FL_LVL4_BPDF_TP		0
#define FL_LVL0_VR_TP		VR_OFF
#define FL_LVL1_VR_TP		VR_OFF
#define FL_LVL2_VR_TP		VR_OFF
#define FL_LVL3_VR_TP		VR_ON
#define FL_LVL4_VR_TP		VR_ON
#define	FL_LVL0_TP_DEFAULT	((FL_LVL0_VR_TP<<15)|FL_LVL0_BPDF_TP)
#define	FL_LVL1_TP_DEFAULT	((FL_LVL1_VR_TP<<15)|FL_LVL1_BPDF_TP)
#define	FL_LVL2_TP_DEFAULT	((FL_LVL2_VR_TP<<15)|FL_LVL2_BPDF_TP)
#define	FL_LVL3_TP_DEFAULT	((FL_LVL3_VR_TP<<15)|FL_LVL3_BPDF_TP)
#define	FL_LVL4_TP_DEFAULT	((FL_LVL4_VR_TP<<15)|FL_LVL4_BPDF_TP)

#define FL_LVL0_BPDF_TPAC	0
#define FL_LVL1_BPDF_TPAC	106
#define FL_LVL2_BPDF_TPAC	180
#define FL_LVL3_BPDF_TPAC	180
#define FL_LVL4_BPDF_TPAC	0
#define FL_LVL0_VR_TPAC		VR_OFF
#define FL_LVL1_VR_TPAC		VR_OFF
#define FL_LVL2_VR_TPAC		VR_OFF
#define FL_LVL3_VR_TPAC		VR_ON
#define FL_LVL4_VR_TPAC		VR_ON
#define	FL_LVL0_TPAC_DEFAULT	((FL_LVL0_VR_TPAC<<15)|FL_LVL0_BPDF_TPAC)
#define	FL_LVL1_TPAC_DEFAULT	((FL_LVL1_VR_TPAC<<15)|FL_LVL1_BPDF_TPAC)
#define	FL_LVL2_TPAC_DEFAULT	((FL_LVL2_VR_TPAC<<15)|FL_LVL2_BPDF_TPAC)
#define	FL_LVL3_TPAC_DEFAULT	((FL_LVL3_VR_TPAC<<15)|FL_LVL3_BPDF_TPAC)
#define	FL_LVL4_TPAC_DEFAULT	((FL_LVL4_VR_TPAC<<15)|FL_LVL4_BPDF_TPAC)


typedef struct _duty_vr_t {
  int duty;
  int vr;
  int bpwf;
} duty_vr_t;

static duty_vr_t poodlefl_duty_vr[2][POODLE_LIGHT_SETTING] = {
  /* AC ON */
  { {FL_LVL0_BPDF_AC, FL_LVL0_VR_AC, FL_LVL0_BPWF_AC},
    {FL_LVL1_BPDF_AC, FL_LVL1_VR_AC, FL_LVL1_BPWF_AC},
    {FL_LVL2_BPDF_AC, FL_LVL2_VR_AC, FL_LVL2_BPWF_AC},
    {FL_LVL3_BPDF_AC, FL_LVL3_VR_AC, FL_LVL3_BPWF_AC},
    {FL_LVL4_BPDF_AC, FL_LVL4_VR_AC, FL_LVL4_BPWF_AC}
  },
  /* AC OFF */
  { {FL_LVL0_BPDF, FL_LVL0_VR, FL_LVL0_BPWF},
    {FL_LVL1_BPDF, FL_LVL1_VR, FL_LVL1_BPWF},
    {FL_LVL2_BPDF, FL_LVL2_VR, FL_LVL2_BPWF},
    {FL_LVL3_BPDF, FL_LVL3_VR, FL_LVL3_BPWF},
    {FL_LVL4_BPDF, FL_LVL4_VR, FL_LVL4_BPWF}
  }
};

static duty_vr_t poodlefl_duty_vr_for_touch[2][POODLE_LIGHT_SETTING] = {
  /* AC ON */
  { {FL_LVL0_BPDF_TPAC, FL_LVL0_VR_TPAC, FL_LVL0_BPWF_AC},
    {FL_LVL1_BPDF_TPAC, FL_LVL1_VR_TPAC, FL_LVL1_BPWF_AC},
    {FL_LVL2_BPDF_TPAC, FL_LVL2_VR_TPAC, FL_LVL2_BPWF_AC},
    {FL_LVL3_BPDF_TPAC, FL_LVL3_VR_TPAC, FL_LVL3_BPWF_AC},
    {FL_LVL4_BPDF_TPAC, FL_LVL4_VR_TPAC, FL_LVL4_BPWF_AC}
  },
  /* AC OFF */
  { {FL_LVL0_BPDF_TP, FL_LVL0_VR_TP, FL_LVL0_BPWF},
    {FL_LVL1_BPDF_TP, FL_LVL1_VR_TP, FL_LVL1_BPWF},
    {FL_LVL2_BPDF_TP, FL_LVL2_VR_TP, FL_LVL2_BPWF},
    {FL_LVL3_BPDF_TP, FL_LVL3_VR_TP, FL_LVL3_BPWF},
    {FL_LVL4_BPDF_TP, FL_LVL4_VR_TP, FL_LVL4_BPWF}
  }
};


static spinlock_t fl_lock = SPIN_LOCK_UNLOCKED;


#ifdef CONFIG_PROC_FS
struct proc_dir_entry *proc_fl;

typedef struct poodle_fl_entry {
	duty_vr_t*	addr;
	int		def_value;
	char*		name;
	char*		description;
	unsigned short	low_ino;
} poodle_fl_entry_t;

static poodle_fl_entry_t poodle_fl_params[] = {
/*  { addr,	    def_value,		name,	    description }*/
  { &poodlefl_duty_vr[0][0], FL_LVL0_AC_DEFAULT, "lvl0ac", "level0 duty&vr (ac)" },
  { &poodlefl_duty_vr[0][1], FL_LVL1_AC_DEFAULT, "lvl1ac", "level1 duty&vr (ac)" },
  { &poodlefl_duty_vr[0][2], FL_LVL2_AC_DEFAULT, "lvl2ac", "level2 duty&vr (ac)" },
  { &poodlefl_duty_vr[0][3], FL_LVL3_AC_DEFAULT, "lvl3ac", "level3 duty&vr (ac)" },
  { &poodlefl_duty_vr[0][4], FL_LVL4_AC_DEFAULT, "lvl4ac", "level4 duty&vr (ac)" },

  { &poodlefl_duty_vr[1][0], FL_LVL0_DEFAULT, "lvl0", "level0 duty&vr (bt)" },
  { &poodlefl_duty_vr[1][1], FL_LVL1_DEFAULT, "lvl1", "level1 duty&vr (bt)" },
  { &poodlefl_duty_vr[1][2], FL_LVL2_DEFAULT, "lvl2", "level2 duty&vr (bt)" },
  { &poodlefl_duty_vr[1][3], FL_LVL3_DEFAULT, "lvl3", "level3 duty&vr (bt)" },
  { &poodlefl_duty_vr[1][4], FL_LVL4_DEFAULT, "lvl4", "level4 duty&vr (bt)" },

  { &poodlefl_duty_vr_for_touch[0][0], FL_LVL0_TPAC_DEFAULT, "lvl0tpac", "level0 duty&vr (tp,ac)" },
  { &poodlefl_duty_vr_for_touch[0][1], FL_LVL1_TPAC_DEFAULT, "lvl1tpac", "level1 duty&vr (tp,ac)" },
  { &poodlefl_duty_vr_for_touch[0][2], FL_LVL2_TPAC_DEFAULT, "lvl2tpac", "level2 duty&vr (tp,ac)" },
  { &poodlefl_duty_vr_for_touch[0][3], FL_LVL3_TPAC_DEFAULT, "lvl3tpac", "level3 duty&vr (tp,ac)" },
  { &poodlefl_duty_vr_for_touch[0][4], FL_LVL4_TPAC_DEFAULT, "lvl4tpac", "level4 duty&vr (tp,ac)" },

  { &poodlefl_duty_vr_for_touch[1][0], FL_LVL0_TP_DEFAULT, "lvl0tp", "level0 duty&vr (tp)" },
  { &poodlefl_duty_vr_for_touch[1][1], FL_LVL1_TP_DEFAULT, "lvl1tp", "level1 duty&vr (tp)" },
  { &poodlefl_duty_vr_for_touch[1][2], FL_LVL2_TP_DEFAULT, "lvl2tp", "level2 duty&vr (tp)" },
  { &poodlefl_duty_vr_for_touch[1][3], FL_LVL3_TP_DEFAULT, "lvl3tp", "level3 duty&vr (tp)" },
  { &poodlefl_duty_vr_for_touch[1][4], FL_LVL4_TP_DEFAULT, "lvl4tp", "level4 duty&vr (tp)" }
};
#define NUM_OF_POODLE_FL_ENTRY	(sizeof(poodle_fl_params)/sizeof(poodle_fl_entry_t))

static ssize_t poodlefl_read_params(struct file *file, char *buf,
				      size_t nbytes, loff_t *ppos)
{
	int i_ino = (file->f_dentry->d_inode)->i_ino;
	char outputbuf[15];
	int count;
	int i;
	poodle_fl_entry_t	*current_param = NULL;
	if (*ppos>0) /* Assume reading completed in previous read*/
		return 0;
	for (i=0; i<NUM_OF_POODLE_FL_ENTRY; i++) {
		if (poodle_fl_params[i].low_ino==i_ino) {
			current_param = &poodle_fl_params[i];
			break;
		}
	}
	if (current_param==NULL) {
		return -EINVAL;
	}
	count = sprintf(outputbuf, "0x%08X\n",
			current_param->addr->duty | (current_param->addr->vr<<15));
	*ppos += count;
	if (count>nbytes)	/* Assume output can be read at one time */
		return -EINVAL;
	if (copy_to_user(buf, outputbuf, count))
		return -EFAULT;
	return count;
}

static ssize_t poodlefl_write_params(struct file *file, const char *buf,
				       size_t nbytes, loff_t *ppos)
{
	int			i_ino = (file->f_dentry->d_inode)->i_ino;
	poodle_fl_entry_t	*current_param = NULL;
	int			i;
	unsigned long		param;
	char			*endp;

	for (i=0; i<NUM_OF_POODLE_FL_ENTRY; i++) {
		if(poodle_fl_params[i].low_ino==i_ino) {
			current_param = &poodle_fl_params[i];
			break;
		}
	}
	if (current_param==NULL) {
		return -EINVAL;
	}

	param = simple_strtoul(buf,&endp,0);
	if (param == -1) {
		current_param->addr->duty = current_param->def_value & 0x7fff;
		current_param->addr->vr = current_param->def_value & 0x8000 ? 1 : 0;
	} else {
		current_param->addr->duty = param & 0x7fff;
		current_param->addr->vr = param & 0x8000 ? 1 : 0;
	}
	return nbytes+endp-buf;
}

static struct file_operations proc_params_operations = {
	read:	poodlefl_read_params,
	write:	poodlefl_write_params,
};
#endif



#ifdef CONFIG_PM
void poodlefl_blank(int blank)
{
        if (blank) {
                if (!is_poodlefl_blank) {
                        counter_step_save = counter_step_contrast;
                        poodlefl_step_contrast_setting(0);
                        is_poodlefl_blank = 1;
                }
        } else {
                if (is_poodlefl_blank && !is_poodlefl_pm) {
                        is_poodlefl_blank = 0;
                        counter_step_contrast = counter_step_save;
                        poodlefl_step_contrast_setting(counter_step_contrast);
                }
        }
}


static int poodlefl_pm_callback(struct pm_dev* pm_dev,
				pm_request_t req, void *data)
{
	switch (req) {
	case PM_BLANK:
	case PM_SUSPEND:
	  is_poodlefl_pm = 1;
	  poodlefl_blank(1);
	  break;
	case PM_UNBLANK:
	case PM_RESUME:
	  is_poodlefl_pm = 0;
	  poodlefl_blank(0);
	  break;
	}
	return 0;
}
#endif

#if 0
static int poodlefl_open(struct tty_struct* tty, struct file* filp)
{
	MOD_INC_USE_COUNT;
	return 0;
}

static void poodlefl_close(struct tty_struct* tty, struct file* filp)
{
	MOD_DEC_USE_COUNT;
}
#endif

static struct file_operations poodlefl_fops = {
#if 0
        open:           poodlefl_open,
        release:        poodlefl_close,
#endif
	ioctl:		poodlefl_ioctl,
};

static int poodlefl_major;

#define CHECK_BATTERY_TIME	1
static int poodlefl_step_contrast_setting(int need_value)
{
        int ac_in = 0;
	unsigned long flags;
	extern void sharpsl_kick_battery_check(int,int,int);


	// blank now , so turn on fl.
	if ( is_poodlefl_blank == 1 ) {
	  //printk("currently blank\n");
	  return counter_step_contrast;
	}

	sharpsl_kick_battery_check(0,1,0);	// check battery and wait 10msec

	if (need_value < 0 ) need_value = 0;
        if ( need_value > poodlefl_limit )
	  need_value = poodlefl_limit;

	GPDR(GPIO_AC_IN) &= ~GPIO_bit(GPIO_AC_IN);
	ac_in = (GPLR(GPIO_AC_IN) & GPIO_bit(GPIO_AC_IN)) ? 0 : 1;

	spin_lock_irqsave(&fl_lock, flags);


	if (poodlefl_duty_vr[ac_in][need_value].vr) {
	    poodlefl_enable_accel();
	} else {
	    poodlefl_disable_accel();
	}

	if ( ( LCM_ALD != poodlefl_duty_vr[ac_in][need_value].duty ) ||
	     ( LCM_ALS != ( LCM_ALC_EN | poodlefl_duty_vr[ac_in][need_value].bpwf ) ) ) {
	  LCM_ALS = poodlefl_duty_vr[ac_in][need_value].bpwf;
	  udelay(100);
	  LCM_ALD = poodlefl_duty_vr[ac_in][need_value].duty;
	  LCM_ALS = LCM_ALC_EN | poodlefl_duty_vr[ac_in][need_value].bpwf;
	  counter_step_contrast = need_value;
	}
	spin_unlock_irqrestore(&fl_lock, flags);

	sharpsl_kick_battery_check(1,0,0);	// wait 10msec and check battery
	//printk("LCM_ALS %8x,LCM_ALD %8x\n",LCM_ALS,LCM_ALD);

	return counter_step_contrast;
}


static int poodlefl_step_contrast_setting_nocheck(int need_value)
{
        int ac_in = 0;
	unsigned long flags;
	extern void sharpsl_kick_battery_check(int,int,int);


	if (need_value < 0 ) need_value = 0;
        if ( need_value > poodlefl_limit )
	  need_value = poodlefl_limit;

	// blank now , so turn on fl.
	if ( is_poodlefl_blank == 1 ) {
	  //printk("currently blank\n");
	  return counter_step_contrast;
	}

	GPDR(GPIO_AC_IN) &= ~GPIO_bit(GPIO_AC_IN);
	ac_in = (GPLR(GPIO_AC_IN) & GPIO_bit(GPIO_AC_IN)) ? 0 : 1;

	spin_lock_irqsave(&fl_lock, flags);

	if (poodlefl_duty_vr[ac_in][need_value].vr) {
	    poodlefl_enable_accel();
	} else {
	    poodlefl_disable_accel();
	}

	if ( ( LCM_ALD != poodlefl_duty_vr[ac_in][need_value].duty ) ||
	     ( LCM_ALS != ( LCM_ALC_EN | poodlefl_duty_vr[ac_in][need_value].bpwf ) ) ) {
	  LCM_ALS = poodlefl_duty_vr[ac_in][need_value].bpwf;
	  udelay(100);
	  LCM_ALD = poodlefl_duty_vr[ac_in][need_value].duty;
	  LCM_ALS = LCM_ALC_EN | poodlefl_duty_vr[ac_in][need_value].bpwf;
	  counter_step_contrast = need_value;
	}
	spin_unlock_irqrestore(&fl_lock, flags);

	//printk("LCM_ALS %8x,LCM_ALD %8x\n",LCM_ALS,LCM_ALD);

	return counter_step_contrast;
}


static int poodlefl_enable_accel(void)
{
	if ( ! (LCM_GPO & LCM_GPIO9) ) {
		LCM_GPD &= ~LCM_GPIO9;
		LCM_GPE &= ~LCM_GPIO9;
		LCM_GPO |=  LCM_GPIO9;
	}
	return 0;
}

static int poodlefl_disable_accel(void)
{
	if (  LCM_GPO & LCM_GPIO9 ) {
	        LCM_GPD &= ~LCM_GPIO9;
		LCM_GPE &= ~LCM_GPIO9;
		LCM_GPO &= ~LCM_GPIO9;
	}
	return 0;
}

static int	temporary_contrast_set_flag = 0;

void poodlefl_temporary_contrast_set(void)
{
	int need_value = counter_step_contrast;
        int ac_in = 0;


	if (temporary_contrast_set_flag) {
	  return;
	}
	temporary_contrast_set_flag = 1;

	GPDR(GPIO_AC_IN) &= ~GPIO_bit(GPIO_AC_IN);
	ac_in = (GPLR(GPIO_AC_IN) & GPIO_bit(GPIO_AC_IN)) ? 0 : 1;


	if ( poodlefl_duty_vr_for_touch[ac_in][need_value].duty ) {
	  if (poodlefl_duty_vr_for_touch[ac_in][need_value].vr) {
		poodlefl_enable_accel();
	  } else {
		poodlefl_disable_accel();
	  }

	  LCM_ALS = poodlefl_duty_vr[ac_in][need_value].bpwf;
	  udelay(100);
	  LCM_ALD = poodlefl_duty_vr[ac_in][need_value].duty;
	  LCM_ALS = LCM_ALC_EN | poodlefl_duty_vr[ac_in][need_value].bpwf;
	}

}

void poodlefl_temporary_contrast_reset(void)
{
	int need_value = counter_step_contrast;
        int ac_in = 0;


	if (!temporary_contrast_set_flag) {
	  return;
	}
	temporary_contrast_set_flag = 0;

	GPDR(GPIO_AC_IN) &= ~GPIO_bit(GPIO_AC_IN);
	ac_in = (GPLR(GPIO_AC_IN) & GPIO_bit(GPIO_AC_IN)) ? 0 : 1;


	if (poodlefl_duty_vr[ac_in][need_value].vr) {
	    poodlefl_enable_accel();
	} else {
	    poodlefl_disable_accel();
	}

	if ( poodlefl_duty_vr[ac_in][need_value].duty ) {
	  LCM_ALS = poodlefl_duty_vr[ac_in][need_value].bpwf;
	  udelay(100);
	  LCM_ALD = poodlefl_duty_vr[ac_in][need_value].duty;
	  LCM_ALS = LCM_ALC_EN | poodlefl_duty_vr[ac_in][need_value].bpwf;
	}

}


void poodlefl_set_limit_contrast(int val)
{
	unsigned long flags;
	spin_lock_irqsave(&fl_lock, flags);
	if ((val > POODLE_LIGHT_SETTING - 1) || (val < 0)) {
		if (poodlefl_limit != POODLE_LIGHT_SETTING - 1) {
		  //printk("poodle_fl : unlimit contrast\n");
		}
		poodlefl_limit = POODLE_LIGHT_SETTING - 1;
	}
	else {
		if (poodlefl_limit != val) {
		  //printk("poodle_fl : change limit contrast %d\n", val);
		}
		poodlefl_limit = val;
	}
	spin_unlock_irqrestore(&fl_lock, flags);
	if (counter_step_contrast > poodlefl_limit) {
		poodlefl_step_contrast_setting_nocheck(poodlefl_limit);
	}
}


static int poodlefl_ioctl(struct inode* inode,
			   struct file*  filp,
			   unsigned int  cmd,
			   unsigned long arg)
{
	int ret;

	ret = (-EINVAL);
	
	switch(cmd) {
	case POODLE_FL_IOCTL_ON:
		ret = poodlefl_step_contrast_setting(counter_step_contrast);
		break;
	case POODLE_FL_IOCTL_OFF:
		ret = poodlefl_step_contrast_setting(0);
		break;
	case POODLE_FL_IOCTL_STEP_CONTRAST:
		ret = poodlefl_step_contrast_setting(arg);
		break;
	case POODLE_FL_IOCTL_GET_STEP_CONTRAST:
		ret = counter_step_contrast;
		break;
	case POODLE_FL_IOCTL_GET_STEP:
		ret = POODLE_LIGHT_SETTING;
		break;
	default:
		;
	}
	
	return ret;
}

int __init poodlefl_init(void)
{
	int result;

	poodlefl_major = FL_MAJOR;

	result = register_chrdev(poodlefl_major, FL_NAME, &poodlefl_fops);

	if (result < 0) {
#ifdef DEBUG
	  	printk(KERN_WARNING "poodlefl: cant get major %d\n", 
		       poodlefl_major);
#endif
		return result;
	}
  
#ifdef CONFIG_PM
	fb_pm_dev = pm_register(PM_ILLUMINATION_DEV, 
				PM_SYS_LIGHT, poodlefl_pm_callback);
#endif

	if (poodlefl_major == 0) {
	  	poodlefl_major = result;
	}

	poodlefl_step_contrast_setting(counter_step_contrast);

#ifdef CONFIG_PROC_FS
	{
	int	i;
	struct proc_dir_entry *entry;

	proc_fl = proc_mkdir("driver/fl", NULL);
	if (proc_fl == NULL) {
		poodlefl_step_contrast_setting(0);
		poodlefl_disable_accel();
		unregister_chrdev(poodlefl_major, "poodle-fl");
		printk(KERN_ERR "ts: can't create /proc/driver/fl\n");
		return -ENOMEM;
	}
	for (i=0; i<NUM_OF_POODLE_FL_ENTRY; i++) {
		entry = create_proc_entry(poodle_fl_params[i].name,
					  S_IWUSR |S_IRUSR | S_IRGRP | S_IROTH,
					  proc_fl);
		if (entry) {
			poodle_fl_params[i].low_ino = entry->low_ino;
			entry->proc_fops = &proc_params_operations;
		} else {
			int	j;
			for (j=0; j<i; j++) {
				remove_proc_entry(poodle_fl_params[i].name,
						  proc_fl);
			}
			remove_proc_entry("driver/fl", &proc_root);
			proc_fl = 0;
			poodlefl_step_contrast_setting(0);
			poodlefl_disable_accel();
			unregister_chrdev(poodlefl_major, "poodle-fl");
			printk(KERN_ERR "fl: can't create /proc/driver/fl/\n");
			return -ENOMEM;
		}
	}
	}
#endif

	printk("Frontlight Driver Initialized.\n");
  
	return 0;
}

#ifdef MODULE
void __exit poodlefl_exit(void)
{
	poodlefl_step_contrast_setting(0);
	poodlefl_disable_accel();

	unregister_chrdev(poodlefl_major, "poodle-fl");

#ifdef CONFIG_PROC_FS
	{
	int	i;
	for (i=0; i<NUM_OF_POODLE_FL_ENTRY; i++) {
		remove_proc_entry(poodle_fl_params[i].name, proc_fl);
	}
	remove_proc_entry("driver/fl", NULL);
	proc_fl = 0;
	}
#endif
	printk("Frontlight Driver Unloaded\n");
}
#endif

module_init(poodlefl_init);
#ifdef MODULE
module_exit(poodlefl_exit);
#endif
