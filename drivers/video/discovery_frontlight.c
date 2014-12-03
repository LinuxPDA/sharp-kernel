/*
 * linux/drivers/video/discovery_frontlight.c : Discovery frontlight driver
 *
 * Copyright (C) 2002 Richard Rau (richard.rau@msa.hinet.net)
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
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 * May be copied or modified under the terms of the GNU General Public
 * License.  See linux/COPYING for more information.
 *
 * Based on:
 *   drivers/video/collie_frontlight.c
 *   Initial Version by: Nicholas Mistry (nmistry@lhsl.com)
 *
 * ChangeLog:
 *   06-20-2002 SHARP
 *      turn on the frontlight after soft reset.
 *   31-Jul-2002 Lineo Japan, Inc.  for ARCH_PXA
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

#include <video/discovery_frontlight.h>
#if defined(CONFIG_SABINAL_DISCOVERY)
#include <asm/arch/discovery_asic3.h>
#include <asm/arch/power_consumption.h>
//#include <asm/arch/discovery_gpio.h>
#endif

#ifdef CONFIG_PM
static struct pm_dev *fb_pm_dev;
static int is_discoveryfl_pm = 0;
#endif

#define	LCM_ALC_EN	0x8000
#define	LCM_ALC_INV	0x4000
#define	LCM_ALC_HSYSEN	0x0100
#define	LCM_ALC_BPWF	0x00ff
#define	LCM_ALD_BPDF	0x01ff


#define PWM_PERVAL1_VAL	0x3ff

#define MIN_CONTRAST   0
#define MAX_CONTRAST   100

#define DISCOVERY_LIGHT_SETTING	   5     // range setting : 0(OFF) 1 2 3 4(MAX)
#define DISCOVERY_LIGHT_DEFAULT    4
static int is_discoveryfl_blank = 0;
int counter_step_contrast = DISCOVERY_LIGHT_DEFAULT;

static int counter_step_save     = 0;

static int discoveryfl_ioctl(struct inode* inode,
			   struct file*  filp,
			   unsigned int  cmd,
			   unsigned long arg);
/*
static int discoveryfl_enable_accel(void);
static int discoveryfl_disable_accel(void);*/
static int discoveryfl_step_contrast_setting(int);

extern unsigned short driver_waste;

#define	VR_ON	1
#define	VR_OFF	0

#define FL_LVL0_BPDF		0     // 0
#define FL_LVL1_BPDF		0x0cc // 25
#define FL_LVL2_BPDF		0x199 // 50
#define FL_LVL3_BPDF		0x265 // 75
#define FL_LVL4_BPDF		0x332 // 100
#define FL_LVL0_VR		VR_OFF
#define FL_LVL1_VR		VR_OFF
#define FL_LVL2_VR		VR_OFF
#define FL_LVL3_VR		VR_ON
#define FL_LVL4_VR		VR_ON
#define	FL_LVL0_DEFAULT		((FL_LVL0_VR<<15)|FL_LVL0_BPDF)
#define	FL_LVL1_DEFAULT		((FL_LVL1_VR<<15)|FL_LVL1_BPDF)
#define	FL_LVL2_DEFAULT		((FL_LVL2_VR<<15)|FL_LVL2_BPDF)
#define	FL_LVL3_DEFAULT		((FL_LVL3_VR<<15)|FL_LVL3_BPDF)
#define	FL_LVL4_DEFAULT		((FL_LVL4_VR<<15)|FL_LVL4_BPDF)

#define FL_LVL0_BPDF_AC		0
#define FL_LVL1_BPDF_AC		0x0cc
#define FL_LVL2_BPDF_AC		0x199
#define FL_LVL3_BPDF_AC		0x265
#define FL_LVL4_BPDF_AC		0x332
#define FL_LVL0_VR_AC		VR_OFF
#define FL_LVL1_VR_AC		VR_OFF
#define FL_LVL2_VR_AC		VR_OFF
#define FL_LVL3_VR_AC		VR_ON
#define FL_LVL4_VR_AC		VR_ON
#define	FL_LVL0_AC_DEFAULT	((FL_LVL0_VR_AC<<15)|FL_LVL0_BPDF_AC)
#define	FL_LVL1_AC_DEFAULT	((FL_LVL1_VR_AC<<15)|FL_LVL1_BPDF_AC)
#define	FL_LVL2_AC_DEFAULT	((FL_LVL2_VR_AC<<15)|FL_LVL2_BPDF_AC)
#define	FL_LVL3_AC_DEFAULT	((FL_LVL3_VR_AC<<15)|FL_LVL3_BPDF_AC)
#define	FL_LVL4_AC_DEFAULT	((FL_LVL4_VR_AC<<15)|FL_LVL4_BPDF_AC)

#define FL_LVL0_BPDF_TP		0
#define FL_LVL1_BPDF_TP		0x0cc
#define FL_LVL2_BPDF_TP		0x199
#define FL_LVL3_BPDF_TP		0x265
#define FL_LVL4_BPDF_TP		0x332
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
#define FL_LVL1_BPDF_TPAC	0x0cc
#define FL_LVL2_BPDF_TPAC	0x199
#define FL_LVL3_BPDF_TPAC	0x265
#define FL_LVL4_BPDF_TPAC	0x332
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
} duty_vr_t;

static duty_vr_t discoveryfl_duty_vr[2][DISCOVERY_LIGHT_SETTING] = {
  /* AC ON */
  { {FL_LVL0_BPDF_AC, FL_LVL0_VR_AC},
    {FL_LVL1_BPDF_AC, FL_LVL1_VR_AC},
    {FL_LVL2_BPDF_AC, FL_LVL2_VR_AC},
    {FL_LVL3_BPDF_AC, FL_LVL3_VR_AC},
    {FL_LVL4_BPDF_AC, FL_LVL4_VR_AC}
  },
  /* AC OFF */
  { {FL_LVL0_BPDF, FL_LVL0_VR},
    {FL_LVL1_BPDF, FL_LVL1_VR},
    {FL_LVL2_BPDF, FL_LVL2_VR},
    {FL_LVL3_BPDF, FL_LVL3_VR},
    {FL_LVL4_BPDF, FL_LVL4_VR}
  }
};

static duty_vr_t discoveryfl_duty_vr_for_touch[2][DISCOVERY_LIGHT_SETTING] = {
   /* AC ON */
  { {FL_LVL0_BPDF_TPAC, FL_LVL0_VR_TPAC},
    {FL_LVL1_BPDF_TPAC, FL_LVL1_VR_TPAC},
    {FL_LVL2_BPDF_TPAC, FL_LVL2_VR_TPAC},
    {FL_LVL3_BPDF_TPAC, FL_LVL3_VR_TPAC},
    {FL_LVL4_BPDF_TPAC, FL_LVL4_VR_TPAC}
  },
  /* AC OFF */
  { {FL_LVL0_BPDF_TP, FL_LVL0_VR_TP},
    {FL_LVL1_BPDF_TP, FL_LVL1_VR_TP},
    {FL_LVL2_BPDF_TP, FL_LVL2_VR_TP},
    {FL_LVL3_BPDF_TP, FL_LVL3_VR_TP},
    {FL_LVL4_BPDF_TP, FL_LVL4_VR_TP}
  }
};

static spinlock_t fl_lock = SPIN_LOCK_UNLOCKED;


#ifdef CONFIG_PROC_FS
struct proc_dir_entry *proc_fl;

typedef struct discovery_fl_entry {
	duty_vr_t*	addr;
	int		def_value;
	char*		name;
	char*		description;
	unsigned short	low_ino;
} discovery_fl_entry_t;

static discovery_fl_entry_t discovery_fl_params[] = {
/*  { addr,	    def_value,		name,	    description }*/
  { &discoveryfl_duty_vr[0][0], FL_LVL0_AC_DEFAULT, "lvl0ac", "level0 duty&vr (ac)" },
  { &discoveryfl_duty_vr[0][1], FL_LVL1_AC_DEFAULT, "lvl1ac", "level1 duty&vr (ac)" },
  { &discoveryfl_duty_vr[0][2], FL_LVL2_AC_DEFAULT, "lvl2ac", "level2 duty&vr (ac)" },
  { &discoveryfl_duty_vr[0][3], FL_LVL3_AC_DEFAULT, "lvl3ac", "level3 duty&vr (ac)" },
  { &discoveryfl_duty_vr[0][4], FL_LVL4_AC_DEFAULT, "lvl4ac", "level4 duty&vr (ac)" },

  { &discoveryfl_duty_vr[1][0], FL_LVL0_DEFAULT, "lvl0", "level0 duty&vr (bt)" },
  { &discoveryfl_duty_vr[1][1], FL_LVL1_DEFAULT, "lvl1", "level1 duty&vr (bt)" },
  { &discoveryfl_duty_vr[1][2], FL_LVL2_DEFAULT, "lvl2", "level2 duty&vr (bt)" },
  { &discoveryfl_duty_vr[1][3], FL_LVL3_DEFAULT, "lvl3", "level3 duty&vr (bt)" },
  { &discoveryfl_duty_vr[1][4], FL_LVL4_DEFAULT, "lvl4", "level4 duty&vr (bt)" },

  { &discoveryfl_duty_vr_for_touch[0][0], FL_LVL0_TPAC_DEFAULT, "lvl0tpac", "level0 duty&vr (tp,ac)" },
  { &discoveryfl_duty_vr_for_touch[0][1], FL_LVL1_TPAC_DEFAULT, "lvl1tpac", "level1 duty&vr (tp,ac)" },
  { &discoveryfl_duty_vr_for_touch[0][2], FL_LVL2_TPAC_DEFAULT, "lvl2tpac", "level2 duty&vr (tp,ac)" },
  { &discoveryfl_duty_vr_for_touch[0][3], FL_LVL3_TPAC_DEFAULT, "lvl3tpac", "level3 duty&vr (tp,ac)" },
  { &discoveryfl_duty_vr_for_touch[0][4], FL_LVL4_TPAC_DEFAULT, "lvl4tpac", "level4 duty&vr (tp,ac)" },

  { &discoveryfl_duty_vr_for_touch[1][0], FL_LVL0_TP_DEFAULT, "lvl0tp", "level0 duty&vr (tp)" },
  { &discoveryfl_duty_vr_for_touch[1][1], FL_LVL1_TP_DEFAULT, "lvl1tp", "level1 duty&vr (tp)" },
  { &discoveryfl_duty_vr_for_touch[1][2], FL_LVL2_TP_DEFAULT, "lvl2tp", "level2 duty&vr (tp)" },
  { &discoveryfl_duty_vr_for_touch[1][3], FL_LVL3_TP_DEFAULT, "lvl3tp", "level3 duty&vr (tp)" },
  { &discoveryfl_duty_vr_for_touch[1][4], FL_LVL4_TP_DEFAULT, "lvl4tp", "level4 duty&vr (tp)" }
};
#define NUM_OF_DISCOVERY_FL_ENTRY	(sizeof(discovery_fl_params)/sizeof(discovery_fl_entry_t))

static ssize_t discoveryfl_read_params(struct file *file, char *buf,
				      size_t nbytes, loff_t *ppos)
{
	int i_ino = (file->f_dentry->d_inode)->i_ino;
	char outputbuf[15];
	int count;
	int i;
	discovery_fl_entry_t	*current_param = NULL;
	if (*ppos>0) /* Assume reading completed in previous read*/
		return 0;
	for (i=0; i<NUM_OF_DISCOVERY_FL_ENTRY; i++) {
		if (discovery_fl_params[i].low_ino==i_ino) {
			current_param = &discovery_fl_params[i];
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

static ssize_t discoveryfl_write_params(struct file *file, const char *buf,
				       size_t nbytes, loff_t *ppos)
{
	int			i_ino = (file->f_dentry->d_inode)->i_ino;
	discovery_fl_entry_t	*current_param = NULL;
	int			i;
	unsigned long		param;
	char			*endp;

	for (i=0; i<NUM_OF_DISCOVERY_FL_ENTRY; i++) {
		if(discovery_fl_params[i].low_ino==i_ino) {
			current_param = &discovery_fl_params[i];
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
	read:	discoveryfl_read_params,
	write:	discoveryfl_write_params,
};
#endif


void discoveryfl_blank(int blank)
{
        if (blank) {
                if (!is_discoveryfl_blank) {
                        is_discoveryfl_blank = 1;
                        counter_step_save = counter_step_contrast;
                        discoveryfl_step_contrast_setting(0);
                }
        } else {
#ifdef CONFIG_PM
                if (is_discoveryfl_blank && !is_discoveryfl_pm) {
#else
                if (is_discoveryfl_blank) {
#endif
                        counter_step_contrast = counter_step_save;
                        discoveryfl_step_contrast_setting(counter_step_contrast);
                        is_discoveryfl_blank = 0;
                }
        }
}

void discoveryfl_blank_power_button(void)
{
                if (!is_discoveryfl_blank) {
                        is_discoveryfl_blank = 1;
                        counter_step_save = counter_step_contrast;
                        discoveryfl_step_contrast_setting(0);
                } else {
                        counter_step_contrast = counter_step_save;
                        discoveryfl_step_contrast_setting(counter_step_contrast);
                        is_discoveryfl_blank = 0;
                }
}
#ifdef CONFIG_PM
static int discoveryfl_pm_callback(struct pm_dev* pm_dev,
				pm_request_t req, void *data)
{
//	printk("%s \n", __FUNCTION__);
	switch (req) {
	case PM_BLANK:	
#ifdef CONFIG_PM
	  is_discoveryfl_pm = 1;
#endif
	  discoveryfl_blank(1);
	  break;
	case PM_SUSPEND:
#ifdef CONFIG_PM
	  is_discoveryfl_pm = 1;
#endif
	  discoveryfl_blank(1);
	  ASIC3_GPIO_PIOD_B &= ~FL_PWR_ON;
	  clr_driver_waste_bits(FLIGHT_WASTE);
	  CKEN &= ~(1 << 1);
	  break;
	case PM_UNBLANK:
#ifdef CONFIG_PM
          is_discoveryfl_pm = 0;
#endif
	  discoveryfl_blank(0);
	  break;
	case PM_RESUME:
	  CKEN |= (1 << 1);
	  PWM_CTRL1 = 0;
	  PWM_PERVAL1 = PWM_PERVAL1_VAL;
#ifdef CONFIG_PM
	  is_discoveryfl_pm = 0;
#endif
	  discoveryfl_blank(0);
	  break;
	}
	return 0;
}
#endif

#if 0
static int discoveryfl_open(struct tty_struct* tty, struct file* filp)
{
	MOD_INC_USE_COUNT;
	return 0;
}

static void discoveryfl_close(struct tty_struct* tty, struct file* filp)
{
	MOD_DEC_USE_COUNT;
}
#endif

static struct file_operations discoveryfl_fops = {
#if 0
        open:           discoveryfl_open,
        release:        discoveryfl_close,
#endif
	ioctl:		discoveryfl_ioctl,
};

static int discoveryfl_major;

static int discoveryfl_step_contrast_setting(int need_value)
{
        int ac_in = 0;
	unsigned long flags;

	if ( need_value < 0 ) need_value = 0;
        if ( need_value > ( DISCOVERY_LIGHT_SETTING - 1 ) )
		need_value = ( DISCOVERY_LIGHT_SETTING - 1 );
/*
	GPDR &= ~GPIO_AC_IN;
	ac_in = (GPLR & GPIO_AC_IN) ? 0 : 1;
*/
	spin_lock_irqsave(&fl_lock, flags);
/*
	if (discoveryfl_duty_vr[ac_in][need_value].vr) {
	    discoveryfl_enable_accel();
	} else {
	    discoveryfl_disable_accel();
	}

#if defined(CONFIG_DISCOVERY_TS) || defined(CONFIG_DISCOVERY_TR0) || \
    defined(CONFIG_DISCOVERY_TR1) || defined(CONFIG_DISCOVERY_DEV)
	if ( LCM_ALD != discoveryfl_duty_vr[ac_in][need_value].duty ) {
      	  LCM_ALS = BPWF_VALUE;
	  udelay(100);
	  LCM_ALD = discoveryfl_duty_vr[ac_in][need_value].duty;
	  LCM_ALS = LCM_ALC_EN | BPWF_VALUE;
	  counter_step_contrast = need_value;
	}
#else
	if ( ( LCM_ALD != discoveryfl_duty_vr[ac_in][need_value].duty ) ||
	     ( LCM_ALS != ( LCM_ALC_EN | discoveryfl_duty_vr[ac_in][need_value].bpwf ) ) ) {
	  LCM_ALS = discoveryfl_duty_vr[ac_in][need_value].bpwf;
	  udelay(100);
	  LCM_ALD = discoveryfl_duty_vr[ac_in][need_value].duty;
	  LCM_ALS = LCM_ALC_EN | discoveryfl_duty_vr[ac_in][need_value].bpwf;
	  counter_step_contrast = need_value;
	}
#endif
*/
	if ( PWM_PWDUTY1 != discoveryfl_duty_vr[ac_in][need_value].duty ) {
		PWM_PWDUTY1 = discoveryfl_duty_vr[ac_in][need_value].duty;
		counter_step_contrast = need_value;
		if (need_value != 0) {
//			counter_step_contrast = need_value;
			if (!(ASIC3_GPIO_PSTS_B & FL_PWR_ON)) {
				ASIC3_GPIO_PIOD_B |= FL_PWR_ON;
				set_driver_waste_bits(FLIGHT_WASTE);
			}
		} else {
			if (ASIC3_GPIO_PSTS_B & FL_PWR_ON) {
				ASIC3_GPIO_PIOD_B &= ~FL_PWR_ON;
				clr_driver_waste_bits(FLIGHT_WASTE);
			}
		}
		driver_waste &= 0xFFC7;
		driver_waste |= need_value << 3;
		
	}
	spin_unlock_irqrestore(&fl_lock, flags);

	return counter_step_contrast;
}

/*
static int discoveryfl_enable_accel(void)
{
	if ( ! (LCM_GPO & LCM_GPIO9) ) {
		LCM_GPD &= ~LCM_GPIO9;
		LCM_GPE &= ~LCM_GPIO9;
		LCM_GPO |=  LCM_GPIO9;
	}
	return 0;
}

static int discoveryfl_disable_accel(void)
{
	if (  LCM_GPO & LCM_GPIO9 ) {
	        LCM_GPD &= ~LCM_GPIO9;
		LCM_GPE &= ~LCM_GPIO9;
		LCM_GPO &= ~LCM_GPIO9;
	}
	return 0;
}
*/
/*
static int	temporary_contrast_set_flag = 0;

void discoveryfl_temporary_contrast_set(void)
{
	int need_value = counter_step_contrast;
        int ac_in = 0;


	if (temporary_contrast_set_flag) {
	  return;
	}
	temporary_contrast_set_flag = 1;

	GPDR &= ~GPIO_AC_IN;
	ac_in = (GPLR & GPIO_AC_IN) ? 0 : 1;


	if ( discoveryfl_duty_vr_for_touch[ac_in][need_value].duty ) {
	  if (discoveryfl_duty_vr_for_touch[ac_in][need_value].vr) {
		discoveryfl_enable_accel();
	  } else {
		discoveryfl_disable_accel();
	  }

#if defined(CONFIG_DISCOVERY_TS) || defined(CONFIG_DISCOVERY_TR0) || \
    defined(CONFIG_DISCOVERY_TR1) || defined(CONFIG_DISCOVERY_DEV)
	  LCM_ALS = BPWF_VALUE;
	  udelay(100);
	  LCM_ALD = discoveryfl_duty_vr_for_touch[ac_in][need_value].duty;
	  LCM_ALS = LCM_ALC_EN | BPWF_VALUE;
#else
	  LCM_ALS = discoveryfl_duty_vr[ac_in][need_value].bpwf;
	  udelay(100);
	  LCM_ALD = discoveryfl_duty_vr[ac_in][need_value].duty;
	  LCM_ALS = LCM_ALC_EN | discoveryfl_duty_vr[ac_in][need_value].bpwf;
#endif
	}

}

void discoveryfl_temporary_contrast_reset(void)
{
	int need_value = counter_step_contrast;
        int ac_in = 0;


	if (!temporary_contrast_set_flag) {
	  return;
	}
	temporary_contrast_set_flag = 0;

	GPDR &= ~GPIO_AC_IN;
	ac_in = (GPLR & GPIO_AC_IN) ? 0 : 1;


	if (discoveryfl_duty_vr[ac_in][need_value].vr) {
	    discoveryfl_enable_accel();
	} else {
	    discoveryfl_disable_accel();
	}

	if ( discoveryfl_duty_vr[ac_in][need_value].duty ) {
#if defined(CONFIG_DISCOVERY_TS) || defined(CONFIG_DISCOVERY_TR0) || \
    defined(CONFIG_DISCOVERY_TR1) || defined(CONFIG_DISCOVERY_DEV)
	  LCM_ALS = BPWF_VALUE;
	  udelay(100);
	  LCM_ALD = discoveryfl_duty_vr[ac_in][need_value].duty;
	  LCM_ALS = LCM_ALC_EN | BPWF_VALUE;
#else
	  LCM_ALS = discoveryfl_duty_vr[ac_in][need_value].bpwf;
	  udelay(100);
	  LCM_ALD = discoveryfl_duty_vr[ac_in][need_value].duty;
	  LCM_ALS = LCM_ALC_EN | discoveryfl_duty_vr[ac_in][need_value].bpwf;
#endif
	}

}

#if defined(CONFIG_DISCOVERY_TS) || defined(CONFIG_DISCOVERY_TR0)
static int	counter_contrast_save_flag =0;
static int	counter_contrast_save = 0;
void discoveryfl_temporary_suspend(void)
{
	if( !counter_contrast_save_flag ) {
		counter_contrast_save_flag = -1;
 		counter_contrast_save = counter_step_contrast;
		discoveryfl_step_contrast_setting(0);
	}
}

void discoveryfl_temporary_resume(void)
{
	if( counter_contrast_save_flag ) {
		discoveryfl_step_contrast_setting(counter_contrast_save);
	 	counter_step_contrast = counter_contrast_save;
		counter_contrast_save_flag = 0;
	}
}
#endif
*/

static int discoveryfl_ioctl(struct inode* inode,
			   struct file*  filp,
			   unsigned int  cmd,
			   unsigned long arg)
{
	int ret;

	ret = (-EINVAL);

	switch(cmd) {
	case DISCOVERY_FL_IOCTL_ON:
		ret = discoveryfl_step_contrast_setting(counter_step_contrast);
		break;
	case DISCOVERY_FL_IOCTL_OFF:
		ret = discoveryfl_step_contrast_setting(0);
		break;
	case DISCOVERY_FL_IOCTL_STEP_CONTRAST:
		ret = discoveryfl_step_contrast_setting(arg);
		break;
	case DISCOVERY_FL_IOCTL_GET_STEP_CONTRAST:
		ret = counter_step_contrast;
		break;
	case DISCOVERY_FL_IOCTL_GET_STEP:
		ret = DISCOVERY_LIGHT_SETTING;
		break;
	default:
		;
	}
	
	return ret;
}

int __init discoveryfl_init(void)
{
	int result;

	discoveryfl_major = FL_MAJOR;

	result = register_chrdev(discoveryfl_major, FL_NAME, &discoveryfl_fops);

	if (result < 0) {
#ifdef DEBUG
	  	printk(KERN_WARNING "discoveryfl: cant get major %d\n", 
		       discoveryfl_major);
#endif
		return result;
	}
  
#ifdef CONFIG_PM
//	fb_pm_dev = pm_register(PM_ILLUMINATION_DEV, 
//				PM_SYS_LIGHT, discoveryfl_pm_callback);
	fb_pm_dev = pm_register(PM_COTULLA_DEV, 
				PM_SYS_LIGHT, discoveryfl_pm_callback);
				
#endif

	if (discoveryfl_major == 0) {
	  	discoveryfl_major = result;
	}
//	ASIC3_GPIO_PIOD_B |= FL_PWR_ON;
	PWM_CTRL1 = 0;
	PWM_PERVAL1 = PWM_PERVAL1_VAL;
	PWM_PWDUTY1 = 0; // 2002/6/20 T.N Initialize 
	discoveryfl_step_contrast_setting(counter_step_contrast);
/*
#ifdef CONFIG_PROC_FS
	{
	int	i;
	struct proc_dir_entry *entry;

	proc_fl = proc_mkdir("driver/fl", NULL);
	if (proc_fl == NULL) {
		discoveryfl_step_contrast_setting(0);
		discoveryfl_disable_accel();
		unregister_chrdev(discoveryfl_major, "discovery-fl");
		printk(KERN_ERR "ts: can't create /proc/driver/fl\n");
		return -ENOMEM;
	}
	for (i=0; i<NUM_OF_DISCOVERY_FL_ENTRY; i++) {
		entry = create_proc_entry(discovery_fl_params[i].name,
					  S_IWUSR |S_IRUSR | S_IRGRP | S_IROTH,
					  proc_fl);
		if (entry) {
			discovery_fl_params[i].low_ino = entry->low_ino;
			entry->proc_fops = &proc_params_operations;
		} else {
			int	j;
			for (j=0; j<i; j++) {
				remove_proc_entry(discovery_fl_params[i].name,
						  proc_fl);
			}
			remove_proc_entry("driver/fl", &proc_root);
			proc_fl = 0;
			discoveryfl_step_contrast_setting(0);
			discoveryfl_disable_accel();
			unregister_chrdev(discoveryfl_major, "discovery-fl");
			printk(KERN_ERR "fl: can't create /proc/driver/fl/\n");
			return -ENOMEM;
		}
	}
	}-
#endif
*/
	printk("Frontlight Driver Initialized.\n");
  
	return 0;
}

#ifdef MODULE
void __exit discoveryfl_exit(void)
{
	discoveryfl_step_contrast_setting(0);
	discoveryfl_disable_accel();

	unregister_chrdev(discoveryfl_major, "discovery-fl");
	ASIC3_GPIO_PIOD_B &= ~FL_PWR_ON;
	clr_driver_waste_bits(FLIGHT_WASTE);
#ifdef CONFIG_PROC_FS
	{
	int	i;
	for (i=0; i<NUM_OF_DISCOVERY_FL_ENTRY; i++) {
		remove_proc_entry(discovery_fl_params[i].name, proc_fl);
	}
	remove_proc_entry("driver/fl", NULL);
	proc_fl = 0;
	}
#endif
	printk("Frontlight Driver Unloaded\n");
}
#endif

module_init(discoveryfl_init);
#ifdef MODULE
module_exit(discoveryfl_exit);
#endif

EXPORT_SYMBOL(discoveryfl_blank_power_button);
