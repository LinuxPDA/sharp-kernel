/*
 * linux/arch/arm/mach-pxa/sharpsl_battery.c
 *
 * Based on
 *  linux/arch/arm/mach-sa1100/collie_battery.c
 *
 * Battery routines for corgi (SHARP)
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
 * ChangeLog:
 *	12-Nov-2001 Lineo Japan, Inc.
 *	21-Aug-2002 Lineo Japan, Inc.  for 2.4.18
 *	12-Dec-2002 Sharp Corporation for Poodle and Corgi
 *	16-Jan-2003 SHARP sleep_on -> interruptible_sleep_on
 *
 */


/* this driver support the following functions
 *      - apm_get_power_status
 *      - charge proc
 */


#include <linux/config.h>
#include <linux/module.h>

#include <linux/poll.h>
#include <linux/types.h>
#include <linux/stddef.h>
#include <linux/timer.h>
#include <linux/fcntl.h>
#include <linux/slab.h>
#include <linux/stat.h>
#include <linux/proc_fs.h>
#include <linux/miscdevice.h>
#include <linux/init.h>
#include <linux/sched.h>
#include <linux/pm.h>
#include <linux/kernel.h>
#include <linux/smp_lock.h>
#include <linux/apm_bios.h>
#include <linux/delay.h>

#include <asm/hardware.h>
#include <asm/irq.h>
#include <asm/mach/irq.h>
#include <asm/hardirq.h>

#include <asm/sharp_char.h>
#include <asm/sharp_keycode.h>
#if defined(CONFIG_ARCH_PXA_POODLE)
#include <asm/arch/keyboard_poodle.h>
#include <asm/arch/ads7846_ts.h>
#include <video/poodle_frontlight.h>
#elif defined(CONFIG_ARCH_PXA_CORGI)
#include <asm/arch/keyboard_corgi.h>
#include <asm/arch/corgi.h>
#include <video/corgi_backlight.h>
#endif
#include <asm/arch/sharpsl_battery.h>



//#define DEBUG	1
#ifdef DEBUG
#define DPRINTK(x, args...)  printk(__FUNCTION__ ": " x,##args)
#define DPRINTK2(x, args...)  printk(__FUNCTION__ ": " x,##args)
#else
#define DPRINTK(x, args...)   if ( msglevel > 1 )	printk(x,##args);
#define DPRINTK2(x, args...)  if ( msglevel > 0 )	printk(x,##args);
#endif

/*
 * The battery device is one of the misc char devices.
 * This is its minor number.
 */
#define	BATTERY_MINOR_DEV	215


#if defined(CONFIG_ARCH_PXA_POODLE) && !defined(CONFIG_ARCH_SHARP_SL_J)
// SL-5600
#define CHARGE_ON()	({							\
				GPSR(GPIO38_FFRI) = GPIO_bit(GPIO38_FFRI);	\
				LCM_GPO |= LCM_GPIO_JK_B;			\
			})
#define CHARGE_OFF()	({							\
				GPCR(GPIO38_FFRI) = GPIO_bit(GPIO38_FFRI);	\
				LCM_GPO &= ~LCM_GPIO_JK_B;			\
			})
#elif defined(CONFIG_ARCH_PXA_CORGI)
// SL-C700
#define CHARGE_ON()	({							\
				if ( sharpsl_off_charge ) {	\
					GPSR(GPIO38_FFRI) = GPIO_bit(GPIO38_FFRI);	\
					GPCR(GPIO43_BTTXD) = GPIO_bit(GPIO43_BTTXD);	\
				} else {	\
					GPCR(GPIO38_FFRI) = GPIO_bit(GPIO38_FFRI);	\
					GPSR(GPIO43_BTTXD) = GPIO_bit(GPIO43_BTTXD);	\
				}						\
			})
#define CHARGE_OFF()	({							\
				GPCR(GPIO38_FFRI) = GPIO_bit(GPIO38_FFRI);	\
				GPCR(GPIO43_BTTXD) = GPIO_bit(GPIO43_BTTXD);	\
			})

#define DISCHARGE_ON()	GPSR(GPIO42_BTRXD) = GPIO_bit(GPIO42_BTRXD)
#define DISCHARGE_OFF()	GPCR(GPIO42_BTRXD) = GPIO_bit(GPIO42_BTRXD)

#else
// SL-B500
#define CHARGE_ON()	({							\
				if ( sharpsl_off_charge ) {			\
					LCM_GPO |= LCM_GPIO_JK_B;		\
					GPSR(GPIO38_FFRI) = GPIO_bit(GPIO38_FFRI);	\
				} else {					\
					LCM_GPO |= LCM_GPIO_JK_B;		\
					GPCR(GPIO38_FFRI) = GPIO_bit(GPIO38_FFRI);	\
				}						\
			})
#define CHARGE_OFF()	({							\
				GPCR(GPIO38_FFRI) = GPIO_bit(GPIO38_FFRI);	\
				LCM_GPO &= ~LCM_GPIO_JK_B;			\
			})
#endif


#define CHARGE_LED_ON()		set_led_status(SHARP_LED_CHARGER,LED_CHARGER_CHARGING)
#define CHARGE_LED_OFF()	set_led_status(SHARP_LED_CHARGER,LED_CHARGER_OFF)
#define CHARGE_LED_ERR()	set_led_status(SHARP_LED_CHARGER,LED_CHARGER_ERROR)
#define CHARGE_LED_ERR_OFF()	sharpsl_charge_err_off()

#define SHARPSL_BATTERY_STATUS_HIGH	APM_BATTERY_STATUS_HIGH
#define SHARPSL_BATTERY_STATUS_LOW	APM_BATTERY_STATUS_LOW
#define SHARPSL_BATTERY_STATUS_VERYLOW	APM_BATTERY_STATUS_VERY_LOW
#define SHARPSL_BATTERY_STATUS_CRITICAL	APM_BATTERY_STATUS_CRITICAL

#define SHARPSL_AC_LINE_STATUS	(!( GPLR(GPIO_AC_IN) & GPIO_bit(GPIO_AC_IN) )	? APM_AC_OFFLINE : APM_AC_ONLINE)

#define SHARPSL_APO_TICKTIME		(  30 * SHARPSL_PM_TICK )		// 30 sec
#define SHARPSL_LPO_TICKTIME		SHARPSL_APO_TICKTIME
#define SHARPSL_APO_DEFAULT		( ( 3 * 60 ) * SHARPSL_PM_TICK )	// 3 min
#define SHARPSL_LPO_DEFAULT		(  20 * SHARPSL_PM_TICK )		// 20 sec

#if defined(CONFIG_ARCH_PXA_CORGI)
#define SHARPSL_PM_TICK         	( HZ )          		        // 1 sec
#define SHARPSL_BATCHK_TIME		( SHARPSL_PM_TICK )			// 1 sec
#define SHARPSL_MAIN_GOOD_COUNT		( HZ / SHARPSL_PM_TICK )		// 1 sec
#define SHARPSL_MAIN_NOGOOD_COUNT	( HZ / SHARPSL_PM_TICK )		// 1 sec
#else
#define SHARPSL_PM_TICK         	( HZ )          		        // 1 sec
#define SHARPSL_BATCHK_TIME		( SHARPSL_PM_TICK )			// 1 sec
#define SHARPSL_MAIN_GOOD_COUNT		( HZ / SHARPSL_PM_TICK )		// 1 sec
#define SHARPSL_MAIN_NOGOOD_COUNT	( HZ / SHARPSL_PM_TICK )		// 1 sec
#endif

#define SHARPSL_CHARGE_ON_TIME_INTERVAL	( 1*60*1000 / 10 )	// 1min
#define SHARPSL_CHARGE_WAIT_TIME	15			// 15 msec
#define SHARPSL_CHARGE_FINISH_TIME	( 10*60*1000/10 )	// 10 min

#define SHARPSL_CHARGE_CO_CHECK_TIME		5	// 5 msec

#define SHARPSL_BATTERY_CK_TIME		( 10*60*1*100 )		// 10min ( base jiffies )


#if defined(CONFIG_ARCH_PXA_POODLE)
#define SHARPSL_POWER_MODE_CHECK		1475
#define SHARPSL_POODLE_FATAL_VOLT		1433
#define SHARPSL_POODLE_FATAL_NOACIN_FLON_VOLT	1454    // 3.55V
#define SHARPSL_POODLE_FATAL_NOACIN_FLOFF_VOLT	1433    // 3.50V
#define SHARPSL_POODLE_FATAL_ACIN_FLON_VOLT	1474    // 3.60V
#define SHARPSL_POODLE_FATAL_ACIN_FLOFF_VOLT	1454    // 3.55V
#define SHARPSL_CHARGE_ON_VOLT			1187	// 2.9V
#define SHARPSL_CHARGE_ON_TEMP			2441	// 
#define SHARPSL_CHECK_BATTERY_WAIT_TIME_TEMP	5	// 50msec
#define SHARPSL_CHECK_BATTERY_WAIT_TIME_VOLT	1	// 10msec
#define SHARPSL_WAIT_DISCHARGE_ON		5	// 50msec
#elif defined(CONFIG_ARCH_PXA_CORGI)
#define SHARPSL_CHARGE_ON_VOLT			0x99	// 2.9V
#define SHARPSL_CHARGE_ON_TEMP			0xe0	// 2.9V
#define SHARPSL_CHARGE_ON_JKVAD_HIGH		0x9b	// 6V
#define SHARPSL_CHARGE_ON_JKVAD_LOW		0x34	// 2V
#define SHARPSL_WAIT_DISCHARGE_ON		10	// 100msec
#define SHARPSL_CHECK_BATTERY_WAIT_TIME_TEMP	1	// 10msec
#define SHARPSL_CHECK_BATTERY_WAIT_TIME_VOLT	1	// 10msec
#define SHARPSL_CHECK_BATTERY_WAIT_TIME_JKVAD	1	// 10msec
#define SHARPSL_CORGI_FATAL_ACIN_VOLT		182	// 3.45V
#define SHARPSL_CORGI_FATAL_NOACIN_VOLT		179	// 3.40V
#define SHARPSL_CORGI_WAIT_CO_TIME		15	// 15 Sec
							//NOTICE !!  you want to change this value , so you must change
							//           alarm check mirgin time ( +30 ) in the sharpsl_power.c.
#else
#error "limit parameter not defined"
#endif


#if defined(CONFIG_ARCH_PXA_POODLE)
#define SHARPSL_CAUTION_CONTRAST		POODLE_FL_CAUTION_CONTRAST
#define SHARPSL_RESET_CONTRAST			POODLE_FL_RESET_CONTRAST
#define SHARPSL_LIMIT_CONTRAST(x)		poodlefl_set_limit_contrast(x)
#elif defined(CONFIG_ARCH_PXA_CORGI)
#define SHARPSL_CAUTION_CONTRAST		CORGI_BL_CAUTION_CONTRAST
#define SHARPSL_RESET_CONTRAST			CORGI_BL_RESET_CONTRAST
#define SHARPSL_LIMIT_CONTRAST(x)		corgibl_set_limit_contrast(x)
#endif

#define CHARGE_STEP1		1
#define CHARGE_STEP2		2
#define CHARGE_STEP3		3
#define CHARGE_STEP4		4
#define CHARGE_STEP5	        5
#define CHARGE_STEP6	        6
#define CHARGE_STEP7	        7
#define CHARGE_STEP_END		10
#define CHARGE_STEP_EXIT	11
#define CHARGE_STEP_ERROR	12
#define SHARPSL_CHARGE_RETRY_CNT	1	// 10 min
#define SHARPSL_AFTER_CHARGE_CNT	0	// 0

#define SHARPSL_CNV_VALUE_NUM	10


typedef struct BatteryThresh {
    int voltage;
    int percentage;
    int status;
} BATTERY_THRESH;

/*** prototype *********************************************************************/
int sharpsl_read_MainBattery(void);
static int sharpsl_charge_on(void*);
static int sharpsl_charge_off(void*);
BATTERY_THRESH *GetMainLevel( int Volt );
int sharpsl_get_main_battery(void);
int suspend_sharpsl_read_Voltage(void);
int GetMainChargePercent(int);
int Get_DAC_Value(int);
int sharpsl_check_battery(int mode);

/*** extern ***********************************************************************/
extern u32 apm_wakeup_src_mask;
#if defined(CONFIG_ARCH_PXA_POODLE) || defined(CONFIG_ARCH_PXA_CORGI)
extern int counter_step_contrast;
extern int pass_charge_flag;
#endif
extern unsigned long ssp_get_dac_val(unsigned long, int);
#if defined(CONFIG_ARCH_PXA_POODLE)
extern int counter_step_save;
extern unsigned long ads7846_rw_cli(unsigned long);
#elif defined(CONFIG_ARCH_PXA_CORGI)
extern unsigned int max1111_read(unsigned long);
extern int ssp_get_max1111_val(ulong data);
extern int sharpsl_is_power_mode_sensitive_battery(void);
#endif
extern int set_led_status(int which,int status);


/*** variables ********************************************************************/


#if defined(CONFIG_ARCH_PXA_POODLE)

BATTERY_THRESH  sharpsl_main_battery_thresh_fl[] = {
    {1582, 100, SHARPSL_BATTERY_STATUS_HIGH},
    {1527,  75, SHARPSL_BATTERY_STATUS_HIGH},
    {1482,  50, SHARPSL_BATTERY_STATUS_HIGH},
    {1458,  25, SHARPSL_BATTERY_STATUS_LOW},
    {1404,   5, SHARPSL_BATTERY_STATUS_VERYLOW},
    {   0,   0, SHARPSL_BATTERY_STATUS_CRITICAL},
};

BATTERY_THRESH  sharpsl_main_battery_thresh_nofl[] = {
    {1589, 100, SHARPSL_BATTERY_STATUS_HIGH},
    {1536,  75, SHARPSL_BATTERY_STATUS_HIGH},
    {1490,  50, SHARPSL_BATTERY_STATUS_HIGH},
    {1466,  25, SHARPSL_BATTERY_STATUS_LOW},
    {1413,   5, SHARPSL_BATTERY_STATUS_VERYLOW},
    {   0,   0, SHARPSL_BATTERY_STATUS_CRITICAL},
};

#elif defined(CONFIG_ARCH_PXA_CORGI)

BATTERY_THRESH  sharpsl_main_battery_thresh_fl[] = {
    { 194, 100, SHARPSL_BATTERY_STATUS_HIGH},
    { 188,  75, SHARPSL_BATTERY_STATUS_HIGH},
    { 184,  50, SHARPSL_BATTERY_STATUS_HIGH},
    { 180,  25, SHARPSL_BATTERY_STATUS_LOW},
    { 171,   5, SHARPSL_BATTERY_STATUS_VERYLOW},
    {   0,   0, SHARPSL_BATTERY_STATUS_CRITICAL},
};

BATTERY_THRESH  sharpsl_main_battery_thresh_nofl[] = {
    { 194, 100, SHARPSL_BATTERY_STATUS_HIGH},
    { 188,  75, SHARPSL_BATTERY_STATUS_HIGH},
    { 184,  50, SHARPSL_BATTERY_STATUS_HIGH},
    { 180,  25, SHARPSL_BATTERY_STATUS_LOW},
    { 171,   5, SHARPSL_BATTERY_STATUS_VERYLOW},
    {   0,   0, SHARPSL_BATTERY_STATUS_CRITICAL},
};
#endif

#if defined(CONFIG_ARCH_PXA_POODLE)

BATTERY_THRESH sharpsl_main_battery_thresh_charge_fl[] = {
    {1601, 100, SHARPSL_BATTERY_STATUS_HIGH},
    {1536,  75, SHARPSL_BATTERY_STATUS_HIGH},
    {1503,  50, SHARPSL_BATTERY_STATUS_HIGH},
    {1482,  25, SHARPSL_BATTERY_STATUS_LOW},
    {   0,   5, SHARPSL_BATTERY_STATUS_VERYLOW},
};

BATTERY_THRESH sharpsl_main_battery_thresh_charge_nofl[] = {
    {1609, 100, SHARPSL_BATTERY_STATUS_HIGH},
    {1548,  75, SHARPSL_BATTERY_STATUS_HIGH},
    {1527,  50, SHARPSL_BATTERY_STATUS_HIGH},
    {1495,  25, SHARPSL_BATTERY_STATUS_LOW},
    {   0,   5, SHARPSL_BATTERY_STATUS_VERYLOW},
};

#elif defined(CONFIG_ARCH_PXA_CORGI)

BATTERY_THRESH sharpsl_main_battery_thresh_charge_fl[] = {
    { 200, 100, SHARPSL_BATTERY_STATUS_HIGH},
    { 196,  75, SHARPSL_BATTERY_STATUS_HIGH},
    { 192,  50, SHARPSL_BATTERY_STATUS_HIGH},
    { 187,  25, SHARPSL_BATTERY_STATUS_LOW},
    { 171,   5, SHARPSL_BATTERY_STATUS_VERYLOW},
    {   0,   0, SHARPSL_BATTERY_STATUS_CRITICAL},
};

BATTERY_THRESH sharpsl_main_battery_thresh_charge_nofl[] = {
    { 200, 100, SHARPSL_BATTERY_STATUS_HIGH},
    { 196,  75, SHARPSL_BATTERY_STATUS_HIGH},
    { 192,  50, SHARPSL_BATTERY_STATUS_HIGH},
    { 187,  25, SHARPSL_BATTERY_STATUS_LOW},
    { 171,   5, SHARPSL_BATTERY_STATUS_VERYLOW},
    {   0,   0, SHARPSL_BATTERY_STATUS_CRITICAL},
};

#endif

#if 1
static struct file_operations sharpsl_battery_fops = {
};
#else
static struct file_operations sharpsl_battery_fops = {
	owner:		THIS_MODULE,
	read:		do_read,
	poll:		do_poll,
	ioctl:		do_ioctl,
	open:		do_open,
	release:	do_release,
};
#endif

static struct miscdevice battery_device = {
	BATTERY_MINOR_DEV,
	"battery",
	&sharpsl_battery_fops
};


int charge_status = 0;			/* charge status  1 : charge  0: not charge */
int sharpsl_off_charge = 0;		/* 0: on , 1 : off */

unsigned int APOCnt = SHARPSL_APO_DEFAULT;
unsigned int APOCntWk = 0;
unsigned int LPOCnt = SHARPSL_LPO_DEFAULT;
unsigned int LPOCntWk = 0;
static DECLARE_WAIT_QUEUE_HEAD(battery_queue);
static int	msglevel;

int sharpsl_main_battery   = SHARPSL_BATTERY_STATUS_HIGH;
int sharpsl_main_battery_percentage = 100;
int sharpsl_main_charge_battery = 100;
int sharpsl_ac_status = APM_AC_OFFLINE;

static int MainCntWk = SHARPSL_MAIN_GOOD_COUNT;
static int MainCnt   = SHARPSL_MAIN_NOGOOD_COUNT;
static int FattCnt   = 0;
static int back_ac_status = -1;
static int back_battery_status = -1;
static int sharpsl_check_time = SHARPSL_BATCHK_TIME;

static int sharpsl_main_status_bk = SHARPSL_BATTERY_STATUS_HIGH;
static int sharpsl_main_percent_bk = 100;
static int sharpsl_check_ac_err = 0;
int sharpsl_main_bk_flag = 0;

static struct timer_list ac_kick_timer;


#ifdef CONFIG_PM
static struct pm_dev *battery_pm_dev;	 /* registered PM device */
#endif

static int battery_off_flag = 0;	/* charge : suspend while get adc */
static int charge_off_mode = 0;		/* charge : check volt or non     */
static int is_ac_adaptor = 0;		/* AC adaptor in/out */

static DECLARE_WAIT_QUEUE_HEAD(wq_on);
static DECLARE_WAIT_QUEUE_HEAD(wq_off);
static DECLARE_WAIT_QUEUE_HEAD(battery_waitqueue);

static int sharpsl_fatal_off = 1;
static unsigned int sharpsl_charge_time = 0;

int sharpsl_charge_state = CHARGE_STEP1;
int sharpsl_charge_cnt = 0;
int sharpsl_charge_dummy_cnt = 1;

static unsigned int sharpsl_charge_on_time = 0;

static int sharpsl_ad[SHARPSL_CNV_VALUE_NUM+1];
static int sharpsl_ad_index = 0;

static int sharpsl_change_battery_status = 0;


// for DEBUG
static int sharpsl_debug_flag = 0;



#if defined(CONFIG_ARCH_PXA_CORGI)
#define SHARPSL_IS_BATT		245	/* battery : in/out */

#define MAXCTRL_PD0_SH		0
#define MAXCTRL_PD1_SH		1
#define MAXCTRL_SGL_SH		2
#define MAXCTRL_UNI_SH		3
#define MAXCTRL_SEL_SH		4
#define MAXCTRL_STR_SH		7
#define MAXCTRL_PD1_MSK		2
#endif


/*** apm subroutines  ***********************************************************/
int sharpsl_apm_get_power_status(u_char *ac_line_status,
                                u_char *battery_status,
                                u_char *battery_flag,
                                u_char *battery_percentage,
                                u_short *battery_life)
{
        // set ac status
	*ac_line_status = sharpsl_ac_status;

	// set battery_status  main
	*battery_status = sharpsl_main_battery;

	// set battery percentage
	*battery_percentage = sharpsl_main_battery_percentage;

	if ( *ac_line_status == APM_AC_ONLINE )
		*battery_percentage = 100;

	// set battery_flag
	if ( *battery_flag == SHARPSL_BATTERY_STATUS_VERYLOW )
		*battery_flag = (1 << 5);
	else
		*battery_flag = (1 << *battery_status);

	// chk charge status
	if ( charge_status ) {
		*battery_status = APM_BATTERY_STATUS_CHARGING;
		*battery_flag = (1 << 3);
		// charging now, so can not get battery percentage.
		//*battery_percentage = sharpsl_main_charge_battery;
		*battery_percentage = -1;
	}

	// set battery life
	*battery_life = APM_BATTERY_LIFE_UNKNOWN;

	return APM_SUCCESS;
}

/*** battery main thread  ***********************************************************/
// waitms : waitms*10 msec wait
// flag   : 0 : check battery w/o reflcting battery status.
//          1 : check battery w/  reflcting battery status.
//          2 : check battery w/  refresh battery status.
void sharpsl_kick_battery_check(int before_waitms,int after_waitms,int flag)
{
  int start;


  MainCntWk = MainCnt + 1;

  // before check battery
  mdelay(before_waitms*10);


  switch (flag) {
  case 0:
    sharpsl_battery_thread_main();
    break;
  case 1:
    if ( sharpsl_main_bk_flag == 0 ) {
      sharpsl_main_bk_flag = 1;
      sharpsl_battery_thread_main();
      sharpsl_main_bk_flag = 0;
    } else {
      sharpsl_battery_thread_main();
    }
    break;
  case 2:
    back_battery_status = -1;
    sharpsl_battery_thread_main();
    break;
  }

  // after check battery
  mdelay(after_waitms*10);
}


void sharpsl_kick_battery_check_queue(void)
{
  MainCntWk = MainCnt + 1;
  wake_up(&battery_queue);
}

static int sharpsl_battery_thread_main(void)
{

  // start battery check
  if ( ( jiffies > SHARPSL_BATTERY_CK_TIME ) && ( sharpsl_main_bk_flag == 0 ) ) {
    sharpsl_main_bk_flag = 1;
    printk("start check battery ! \n");
  }

  // get ac status. if change ac status , chk main battery.
  sharpsl_ac_status =  SHARPSL_AC_LINE_STATUS;
  if ( back_ac_status != sharpsl_ac_status ) {
    MainCntWk = MainCnt + 1;			// chk battery
    sharpsl_change_battery_status = 1;		// change status
    wake_up_interruptible(&battery_waitqueue);
  }
  back_ac_status = sharpsl_ac_status;

  // get main battery
  sharpsl_get_main_battery();

  // if battery is low , backlight driver become to save power.
  if ( ( ( sharpsl_main_battery == SHARPSL_BATTERY_STATUS_VERYLOW  ) ||
       ( sharpsl_main_battery == SHARPSL_BATTERY_STATUS_CRITICAL ) ) &&
       ( sharpsl_main_battery != back_battery_status ) &&
       ( sharpsl_main_bk_flag ) ) {
    SHARPSL_LIMIT_CONTRAST(SHARPSL_CAUTION_CONTRAST);
  } else if ( sharpsl_main_battery != back_battery_status ) {
    SHARPSL_LIMIT_CONTRAST(SHARPSL_RESET_CONTRAST);
  }
  back_battery_status = sharpsl_main_battery;

  // good or ac in   --> GOOD_COUNT
  // low or very low --> NOGOOD_COUNT
  if ( ( sharpsl_main_battery == SHARPSL_BATTERY_STATUS_HIGH ) || ( sharpsl_ac_status == APM_AC_ONLINE ) )
    MainCnt = SHARPSL_MAIN_GOOD_COUNT;
  else
    MainCnt = SHARPSL_MAIN_NOGOOD_COUNT;
  DPRINTK("MainCnt = %d\n",MainCnt);

  // chk critical
  if ( ( sharpsl_main_battery == SHARPSL_BATTERY_STATUS_CRITICAL ) && sharpsl_fatal_off && sharpsl_main_bk_flag ) {
#if !defined(CONFIG_ARCH_PXA_CORGI)
    if ( FattCnt++ >= SHARPSL_MAIN_NOGOOD_COUNT ) {
#endif
      sharpsl_fatal_off = 0;
      printk("Fatal Off\n");
      handle_scancode(SLKEY_OFF|KBDOWN , 1);
      mdelay(30);
      handle_scancode(SLKEY_OFF|KBUP   , 0);
#if !defined(CONFIG_ARCH_PXA_CORGI)
    }
#endif
  } else {
      FattCnt = 0;
  }

#if defined(CONFIG_ARCH_PXA_CORGI)
  if ( sharpsl_check_ac_err && sharpsl_main_bk_flag && sharpsl_fatal_off ) {
    sharpsl_fatal_off = 0;
    printk("ac error\n");
    handle_scancode(SLKEY_OFF|KBDOWN , 1);
    mdelay(30);
    handle_scancode(SLKEY_OFF|KBUP   , 0);
  }
#endif

}


static int sharpsl_battery_thread(void* unused)
{
	strcpy(current->comm, "sharpsl_bat");
	sharpsl_check_time = SHARPSL_BATCHK_TIME;

	while(1) {
		interruptible_sleep_on_timeout((wait_queue_head_t*)&battery_queue, sharpsl_check_time );

#if !defined(CONFIG_ARCH_PXA_POODLE) || defined(CONFIG_ARCH_SHARP_SL_J)
		// AC adapter is inserted , but charging not start.
		// SW can not confirm inserted AC adapter, so kick !
		if ( ( charge_status == 0 ) && ( sharpsl_ac_status == APM_AC_ONLINE )) {
		  is_ac_adaptor = 1;
		  wake_up(&wq_on);
		} else if ( ( charge_status == 1 ) && ( sharpsl_ac_status == APM_AC_OFFLINE )) {
		  is_ac_adaptor = 0;
		  wake_up(&wq_off);
		}

		// re-kick charging
		if ( sharpsl_charge_on_time > 0 ) {
		  if ( ( jiffies - sharpsl_charge_on_time ) > SHARPSL_CHARGE_ON_TIME_INTERVAL ) {
		    sharpsl_charge_on_time = jiffies;
		    CHARGE_OFF();
		    mdelay(SHARPSL_CHARGE_WAIT_TIME);
		    CHARGE_ON();
		  }
		}
#endif

		// check battery !
		sharpsl_battery_thread_main();
	}
	return 0;
}


/*** battery charge thread  ***********************************************************/
void sharpsl_charge_start(void)
{
#if !defined(CONFIG_ARCH_PXA_POODLE) || defined(CONFIG_ARCH_SHARP_SL_J)
		sharpsl_charge_on_time = jiffies;
#endif

		if ( sharpsl_check_battery(1) ) {
		  /* error led on */
		  CHARGE_LED_ERR();
		  CHARGE_OFF();
		  /* charge status flag reset */
		  charge_status = 0;
#if !defined(CONFIG_ARCH_PXA_POODLE) || defined(CONFIG_ARCH_SHARP_SL_J)
		  sharpsl_charge_on_time = 0;
#endif
		} else {
		  /* led on */
		  CHARGE_LED_OFF();
		  CHARGE_LED_ON();
		  /* Charge ON */
		  CHARGE_OFF();
		  mdelay(SHARPSL_CHARGE_WAIT_TIME);
		  CHARGE_ON();
		  /* charge status flag set */
		  charge_status = 1;
		}

		sharpsl_change_battery_status = 1;
		wake_up_interruptible(&battery_waitqueue);
}


static int sharpsl_charge_on(void* unused)
{

	// daemonize();
	strcpy(current->comm, "battchrgon");
	sigfillset(&current->blocked);

	while(1) {

		interruptible_sleep_on(&wq_on);

                // if ac is not insert, so stop charge start proc.
                if ( ( GPLR(GPIO_AC_IN) & GPIO_bit(GPIO_AC_IN) ) == 0 ) continue;

		// flag clear
		sharpsl_main_status_bk = SHARPSL_BATTERY_STATUS_HIGH;
		sharpsl_main_percent_bk = 100;

#if defined(CONFIG_ARCH_PXA_CORGI)
		/* AC Check */
		if ( sharpsl_ac_check() ) {
		  DPRINTK("err\n");
		  CHARGE_LED_ERR();
		  CHARGE_OFF();
		} else {
		  DPRINTK("call charge on \n");
		  sharpsl_charge_start();
		}
#else
		DPRINTK("call charge on \n");
		sharpsl_charge_start();
#endif
	}
	return 0;
}


static int sharpsl_charge_off(void* unused)
{

	// daemonize();
	strcpy(current->comm, "battchrgoff");
	sigfillset(&current->blocked);

	while(1) {

		interruptible_sleep_on(&wq_off);

		DPRINTK("charge off\n");
		DPRINTK("charge off mode = %d\n",charge_off_mode);

#if !defined(CONFIG_ARCH_PXA_POODLE) || defined(CONFIG_ARCH_SHARP_SL_J)
		sharpsl_charge_on_time = 0;
#endif

		/* Charge OFF */
		CHARGE_OFF();

		/* led off */
		CHARGE_LED_OFF();

		if ( sharpsl_check_battery(charge_off_mode) ) {
			/* error led on */
			CHARGE_LED_ERR();
		}

		/* charge status flag reset */
		charge_status = 0;
		sharpsl_charge_time = 0;

		sharpsl_change_battery_status = 1;
		wake_up_interruptible(&battery_waitqueue);

	}
	return 0;
}

/*** Int ***********************************************************************/
static void sharpsl_ac_kick_timer(void)
{
  int level = (GPLR(GPIO_AC_IN) & GPIO_bit(GPIO_AC_IN));
  int match = 0;
  int err = 0;

  while(1) {
    if ( level == (GPLR(GPIO_AC_IN) & GPIO_bit(GPIO_AC_IN)) ) {
      match++;
    } else {
      level = (GPLR(GPIO_AC_IN) & GPIO_bit(GPIO_AC_IN));
      match = 0;
      err++;
    }
    if ( match > 1 ) break;
    if ( err > 50 ) {
      // if ac port is not stable , so use last port data.
      level = (GPLR(GPIO_AC_IN) & GPIO_bit(GPIO_AC_IN));
      printk("err : ac port\n");
      break;
    }
  }

  if ( level == 0 ) {
    /* High->Low  : desert */
    is_ac_adaptor = 0;
    sharpsl_ad_index = 0;
    charge_off_mode = 1;
    wake_up(&wq_off);
  } else {
    /* Low->High  : assert */
    is_ac_adaptor = 1;
    wake_up(&wq_on);
  }
  sharpsl_kick_battery_check_queue();
}


static void Sharpsl_ac_interrupt(int irq, void *dummy, struct pt_regs *fp)
{
  // clear flag
  sharpsl_check_ac_err = 0;
  // wait delay
  mod_timer(&ac_kick_timer, jiffies + HZ / 100);
}

static void Sharpsl_co_interrupt(int irq, void *dummy, struct pt_regs *fp)
{
#if defined(CONFIG_ARCH_PXA_POODLE) && !defined(CONFIG_ARCH_SHARP_SL_J)
  // SL-5600

  if ((GPLR(GPIO_AC_IN) & GPIO_bit(GPIO_AC_IN))==0) {
    /* High->Low : AC remove */
    DPRINTK("co ac remove\n");
    charge_off_mode = 1;
    wake_up(&wq_off);
  } else {
    DPRINTK("co ac insert %d\n",jiffies);
    // interrupt has occured , but it is not means finish charging. We have to check it !
    if ( sharpsl_charge_time == 0 ) {
      sharpsl_charge_time = jiffies;
      wake_up(&wq_on);
      DPRINTK("co retry 1  %d\n",sharpsl_charge_time);
    } else {
      if ( jiffies - sharpsl_charge_time > SHARPSL_CHARGE_FINISH_TIME ) {
          // charging has not finished. so we re-try !
	sharpsl_charge_time = jiffies;
	wake_up(&wq_on);
	DPRINTK("co retry 2\n");
      } else {
         // charging has finished !
	charge_off_mode = 0;
	sharpsl_charge_time = 0;
	wake_up(&wq_off);
	DPRINTK("co OK\n");
      }
    }
  }

#else
  // SL-B500/C700
  if ((GPLR(GPIO_AC_IN) & GPIO_bit(GPIO_AC_IN))==0) {
    /* High->Low  : AC remove */
    charge_off_mode = 0;
    wake_up(&wq_off);
  } else {
    /* retry... charging is continue ..... */
    wake_up(&wq_on);
  }
#endif
}

/*** get adc *********************************************************************/
int sharpsl_cnv_value(int ad)
{
  int ad_val = 0;
  int ret,i;


  if ( sharpsl_main_battery != SHARPSL_BATTERY_STATUS_HIGH ) {
    sharpsl_ad_index = 0;
    return ad;
  }

  sharpsl_ad[sharpsl_ad_index] = ad;
  sharpsl_ad_index++;
  if ( sharpsl_ad_index >= SHARPSL_CNV_VALUE_NUM ) {
    for(i=0;i<(SHARPSL_CNV_VALUE_NUM-1);i++)
      sharpsl_ad[i] = sharpsl_ad[i+1];
    sharpsl_ad_index = SHARPSL_CNV_VALUE_NUM -1;
  }
  for(i=0;i<sharpsl_ad_index;i++)
    ad_val += sharpsl_ad[i];
  ret = ( ad_val / sharpsl_ad_index );

#if 0
  printk("sharpsl_ad_index = %d\n",sharpsl_ad_index);
  for(i=0;i<SHARPSL_CNV_VALUE_NUM;i++)
    printk("ad = %d\n",sharpsl_ad[i]);
  printk("ret = %d\n",ret);
#endif

  return ret;
}

int sharpsl_get_main_battery(void)
{
	int i = 0;
	BATTERY_THRESH *thresh;

	MainCntWk++;

	if ( MainCntWk > MainCnt ) {
		int voltage;

		MainCntWk = 0;

		while(1) {
			voltage = sharpsl_read_MainBattery();
			if ( voltage > 0 ) break;
			if ( i++ > 5 ) {
			  voltage = sharpsl_main_battery_thresh_fl[0].voltage;
			  printk("please fix me !  can not read main battery \n");
			  break;
			}
		}


#if defined(CONFIG_ARCH_PXA_POODLE)
		if ( voltage > SHARPSL_POWER_MODE_CHECK ) {
		  // set to LOW
		  GPCR(GPIO36_FFDCD) = GPIO_bit(GPIO36_FFDCD);
		} else {
		  // set to High
		  GPSR(GPIO36_FFDCD) = GPIO_bit(GPIO36_FFDCD);
		}
#endif

		voltage = sharpsl_cnv_value(voltage);

		thresh = GetMainLevel(voltage);

		// if battery is low , backlight driver become to save power.
		if ( ( ( thresh->status == SHARPSL_BATTERY_STATUS_VERYLOW  ) ||
		       ( thresh->status == SHARPSL_BATTERY_STATUS_CRITICAL ) ||
		       ( thresh->status == SHARPSL_BATTERY_STATUS_LOW      ) ) &&
		       ( !sharpsl_main_bk_flag ) ) {
		  SHARPSL_LIMIT_CONTRAST(SHARPSL_CAUTION_CONTRAST);
		}

		if ( sharpsl_main_bk_flag == 0 ) {
		  return sharpsl_main_battery;
		}
		sharpsl_main_battery = thresh->status;
		sharpsl_main_battery_percentage = thresh->percentage;
		sharpsl_main_charge_battery = GetMainChargePercent(voltage);

		//printk("bat : main battery = %d\n",sharpsl_main_battery);

		if ( sharpsl_debug_flag != 0 ) {
		  int i;
		  printk("debug flag on\n");
		  sharpsl_main_battery = sharpsl_debug_flag;
		  for (i = 0; sharpsl_main_battery_thresh_nofl[i].voltage > 0; i++) {
		    if ( sharpsl_debug_flag == sharpsl_main_battery_thresh_nofl[i].status ) {
		      sharpsl_main_battery_percentage = sharpsl_main_battery_thresh_nofl[i].percentage;
		      break;
		    }
		  }
		}

		// Value is kept until it will operate OFF and AC, once low AD comes out.
		if ( sharpsl_main_battery_percentage < sharpsl_main_percent_bk ) {
		  sharpsl_main_status_bk = sharpsl_main_battery;
		  sharpsl_main_percent_bk = sharpsl_main_battery_percentage;

		  sharpsl_change_battery_status = 1;
		  wake_up_interruptible(&battery_waitqueue);

		} else {
		  sharpsl_main_battery = sharpsl_main_status_bk;
		  sharpsl_main_battery_percentage = sharpsl_main_percent_bk;
		}

		DPRINTK2("charge percent = %d ( at %d ) \n",sharpsl_main_charge_battery,(int)jiffies);
		DPRINTK(" get Main battery status %d\n",sharpsl_main_battery);

	} else {
		DPRINTK(" not get Main battery \n");
		DPRINTK("MainCntWk = %d\n",MainCntWk);
	}
	return sharpsl_main_battery;
}

int GetMainChargePercent( int Volt )
{
    int i;
    BATTERY_THRESH *thresh;

    DPRINTK("  volt = %d  \n",Volt);

    if (counter_step_contrast)
	thresh = sharpsl_main_battery_thresh_charge_fl;
    else
	thresh = sharpsl_main_battery_thresh_charge_nofl;
    for (i = 0; thresh[i].voltage > 0; i++) {
	if (Volt >= thresh[i].voltage)
	    return thresh[i].percentage;
    }
    return thresh[i].percentage;
}

BATTERY_THRESH *GetMainLevel( int Volt )
{
    int i;
    BATTERY_THRESH *thresh;

    DPRINTK("  volt = %d  \n",Volt);

    if (counter_step_contrast)
	thresh = sharpsl_main_battery_thresh_fl;
    else
	thresh = sharpsl_main_battery_thresh_nofl;
    for (i = 0; thresh[i].voltage > 0; i++) {
	if (Volt >= thresh[i].voltage)
	    return &thresh[i];
    }
    return &thresh[i];
}


/* 
 * Translate Analog signal to Digital data
 */
int Get_DAC_Value(int channel)
{
	unsigned long cmd;
	unsigned int dummy;
	int voltage;

#if defined(CONFIG_ARCH_PXA_POODLE)
	/* translate ADC */
	cmd = (1u << ADSCTRL_PD0_SH) | (1u << ADSCTRL_PD1_SH) |(1u << ADSCTRL_DFR_SH)|
		(channel << ADSCTRL_ADR_SH) | (1u << ADSCTRL_STS_SH);
	dummy = ads7846_rw_cli(cmd);
	voltage = ads7846_rw_cli(cmd);

	cmd &= ~(ADSCTRL_PD0_MSK | ADSCTRL_PD1_MSK);
	dummy = ads7846_rw_cli(cmd);

#elif defined(CONFIG_ARCH_PXA_CORGI)
//
#if 1
	cmd = (1u << MAXCTRL_PD0_SH) | (1u << MAXCTRL_PD1_SH) |
	    (1u << MAXCTRL_SGL_SH) | (1u << MAXCTRL_UNI_SH) |
	    (channel << MAXCTRL_SEL_SH) | (1u <<MAXCTRL_STR_SH);

	voltage = ssp_get_max1111_val(cmd);
#else
	int voltage1, voltage2;
	/* translate ADC */
	cmd = (1u << MAXCTRL_PD0_SH) | (1u << MAXCTRL_PD1_SH) |
		(1u << MAXCTRL_SGL_SH) | (1u << MAXCTRL_UNI_SH) |
		(channel << MAXCTRL_SEL_SH) | (1u <<MAXCTRL_STR_SH);
	/* TB1/RB1 */
	dummy = ssp_get_dac_val(cmd, CS_MAX1111);
	/* TB2/RB2 */
	voltage1 = ssp_get_dac_val(0, CS_MAX1111);
	/* TB3/RB3 */
	voltage2 = ssp_get_dac_val(0, CS_MAX1111);

	if (voltage1 & 0xc0 || voltage2 & 0x3f)
		return -1;
	voltage = ((voltage1 << 2) & 0xfc) | ((voltage2 >> 6) & 0x03);
#endif

#endif

	return voltage;
}

#if defined(CONFIG_ARCH_PXA_CORGI)
int sharpsl_get_MainBattery(void)
{
	int voltage, batt;
	int chrg_off = 0;
	
	GPSR(GPIO21_TEMP) = GPIO_bit(GPIO21_TEMP);
	mdelay(SHARPSL_CHECK_BATTERY_WAIT_TIME_TEMP*10);
	batt = Get_DAC_Value(BATT_THM);
	GPCR(GPIO21_TEMP) = GPIO_bit(GPIO21_TEMP);
	if (batt >= SHARPSL_IS_BATT || batt < 0)
		return -1;
	
	if (is_ac_adaptor) {
		chrg_off = 1;
		CHARGE_OFF();
		mdelay(SHARPSL_WAIT_DISCHARGE_ON*10);
	}
	voltage = Get_DAC_Value(BATT_AD);
	if (chrg_off)
		CHARGE_ON();

	return voltage;
}
#endif

int sharpsl_read_MainBattery(void)
{
	int voltage;

	battery_off_flag = 0;

#if defined(CONFIG_ARCH_PXA_POODLE)
	voltage = Get_DAC_Value(BATT_CHL);
#elif defined(CONFIG_ARCH_PXA_CORGI)
	voltage = Get_DAC_Value(BATT_AD);
#endif
	if ( battery_off_flag )
		voltage = -1;

	return voltage;
}

int sharpsl_read_Temp(void)
{
	int temp;

	battery_off_flag = 0;

#if defined(CONFIG_ARCH_PXA_POODLE)
	GPSR(GPIO21_TEMP) = GPIO_bit(GPIO21_TEMP);
	mdelay(SHARPSL_CHECK_BATTERY_WAIT_TIME_TEMP*10);
	temp = Get_DAC_Value(MUX_CHL);
	GPCR(GPIO21_TEMP) = GPIO_bit(GPIO21_TEMP);
#elif defined(CONFIG_ARCH_PXA_CORGI)
	GPSR(GPIO21_TEMP) = GPIO_bit(GPIO21_TEMP);
	mdelay(SHARPSL_CHECK_BATTERY_WAIT_TIME_TEMP*10);
	temp = Get_DAC_Value(BATT_THM);
	GPCR(GPIO21_TEMP) = GPIO_bit(GPIO21_TEMP);
#endif
	if ( battery_off_flag )
		temp = -1;

	return temp;
}


#if defined(CONFIG_ARCH_PXA_CORGI)
int sharpsl_read_jkvad(void)
{
  int temp;

  battery_off_flag = 0;

  temp = Get_DAC_Value(JK_VAD);
  if ( battery_off_flag )
    temp = -1;

  return temp;
}
#endif


int get_select_val(int *val)
{
  int i,j,k,temp,sum = 0;


  // Get MAX
  temp = val[0];
  j=0;
  for(i=1;i<5;i++) {
    if ( temp < val[i] ) { temp = val[i]; j = i; }
  }

  // Get MIN
  temp = val[4];
  k=4;
  for(i=3;i>=0;i--) {
    if ( temp > val[i] ) { temp = val[i]; k = i; }
  }

  for(i=0;i<5;i++) {
    if ( i == j || i == k ) continue;
    sum += val[i];
  }
  DPRINTK("val = %d(%d)\n",sum,sum/3);

  return ( sum / 3 );
}

/////////////////////////////////////////////////
// mode 0 : check temp & voltage
//      1 : check temp
// return 1: err
//        0: OK
int sharpsl_check_battery(int mode)
{
#if defined(CONFIG_ARCH_PXA_CORGI)
	int temp, i;
	int buff[5];

	if ( in_interrupt() ) {
		DPRINTK("charge(temp) : in_interrupt !\n");
		return 1;
	}

	// Check Temp : check inserting battery ?
	for(i=0;i<5;i++) {
		mdelay(SHARPSL_CHECK_BATTERY_WAIT_TIME_TEMP*10);
		buff[i] = sharpsl_read_Temp();
	}
	DPRINTK("temp\n");
	temp = get_select_val(buff);

	if ( temp > SHARPSL_CHARGE_ON_TEMP ) return 1;
	if ( mode == 1 ) return 0;

	// disable charge
	CHARGE_OFF();

	// enable discharge
	DISCHARGE_ON();

	mdelay(SHARPSL_WAIT_DISCHARGE_ON*10);

	// Check Voltage : check full charging
	for(i=0;i<5;i++) {
		buff[i] = sharpsl_read_MainBattery();
		mdelay(SHARPSL_CHECK_BATTERY_WAIT_TIME_VOLT*10);
	}

	// disable discharge
	DISCHARGE_OFF();

	DPRINTK("volt\n");
	temp = get_select_val(buff);

	if ( temp < SHARPSL_CHARGE_ON_VOLT ) return 1;

	return 0;

#else ////////// not CORGI //////////////
	int temp;

	if ( in_interrupt() ) {
		DPRINTK("charge(temp) : in_interrupt !\n");
		return 1;
	}

	// Check Temp : check inserting battery ?
	//mdelay(SHARPSL_CHECK_BATTERY_WAIT_TIME_TEMP*10);
	temp = sharpsl_read_Temp();
	DPRINTK("temp %d\n",temp);

	if ( temp > SHARPSL_CHARGE_ON_TEMP ) return 1;
	if ( mode == 1 ) return 0;

	// disable charge
	CHARGE_OFF();
	mdelay(SHARPSL_WAIT_DISCHARGE_ON*10);

	// Check Voltage : check full charging
	//mdelay(SHARPSL_CHECK_BATTERY_WAIT_TIME_VOLT*10);
	temp = sharpsl_read_MainBattery();
	DPRINTK("volt %d\n",temp);

	if ( temp < SHARPSL_CHARGE_ON_VOLT ) return 1;

	return 0;
#endif ///////////////

}


#if defined(CONFIG_ARCH_PXA_CORGI)
/////////////////////////////////////////////////
// return 1: err
//        0: OK
int sharpsl_ac_check(void)
{
	int temp, i, volt;
	int buff[5];

	if ( in_interrupt() ) {
		DPRINTK("charge(ac_check) : in_interrupt !\n");
		return 1;
	}

	// Check JK_VAD
	for(i=0;i<5;i++) {
		buff[i] = sharpsl_read_jkvad();
		mdelay(SHARPSL_CHECK_BATTERY_WAIT_TIME_JKVAD*10);	
	}
	DPRINTK("jkvad\n");
	temp = get_select_val(buff);

	// error occured !
	if ( temp > SHARPSL_CHARGE_ON_JKVAD_HIGH ) {
	  sharpsl_check_ac_err = 1;
	  CHARGE_LED_ERR();
	  return 1;
	}

	if ( temp < SHARPSL_CHARGE_ON_JKVAD_LOW ) {
	  sharpsl_check_ac_err = 1;
	  CHARGE_LED_ERR();
	  return 1;
	}

	sharpsl_check_ac_err = 0;
	return 0;
}
#endif


/*** PM *************************************************************************/

#ifdef CONFIG_PM
static int Sharpsl_battery_pm_callback(struct pm_dev *pm_dev, pm_request_t req, void *data)
{
	switch (req) {
	case PM_SUSPEND:

	  // flag clear
	  sharpsl_main_status_bk = SHARPSL_BATTERY_STATUS_HIGH;
	  sharpsl_main_percent_bk = 100;
	  sharpsl_check_ac_err = 0;

	  if ( !sharpsl_main_bk_flag )
	    sharpsl_main_battery = SHARPSL_BATTERY_STATUS_HIGH;


	  sharpsl_charge_state = CHARGE_STEP1;
	  battery_off_flag = 1;
#if !defined(CONFIG_ARCH_PXA_POODLE) || defined(CONFIG_ARCH_SHARP_SL_J)
	  /* Charge OFF */
	  CHARGE_OFF();
	  CHARGE_LED_OFF();
	  charge_status = 0;
	  sharpsl_charge_time = 0;
#else
	  if ( charge_status ) {
	    // charging now , so kick.
	    pass_charge_flag = 0;
	  } else {
	    // not charging now, so not kick
	    pass_charge_flag = 1;
	  }
#endif
	  apm_wakeup_src_mask |= ( GPIO_bit(GPIO_AC_IN) | GPIO_bit(GPIO_CO) );

	  // wake up by key
	  apm_wakeup_src_mask |=  GPIO_bit(0);

	  break;
	case PM_RESUME:
	  sharpsl_fatal_off = 1;
	  FattCnt = 0;
	  sharpsl_charge_state = CHARGE_STEP1;
	  MainCntWk = MainCnt + 1;	 // chk battery
	  back_battery_status = -1;
	  back_ac_status = -1;
	  sharpsl_ad_index = 0;
	  sharpsl_check_ac_err = 0;

	  // AC insert , so kick the charging
#if !defined(CONFIG_ARCH_PXA_POODLE) || defined(CONFIG_ARCH_SHARP_SL_J)
	  if ((GPLR(GPIO_AC_IN) & GPIO_bit(GPIO_AC_IN)) != 0 ) {
	    is_ac_adaptor = 1;
	    wake_up(&wq_on);
	  } else {
	    is_ac_adaptor = 0;
	    charge_off_mode = 1;
	    wake_up(&wq_off);
	  }
#else
	  if ( charge_status ) {
	    /* Charge ON */
	    CHARGE_OFF();
	    mdelay(SHARPSL_CHARGE_WAIT_TIME);
	    CHARGE_ON();
	  }
#endif
	  sharpsl_kick_battery_check_queue();
	  break;
	}
	return 0;
}
#endif




/*** check fatal battery ************************************************/
// Fatal : 0
// OK    : 1
unsigned short chkFatalBatt(void)
{
#if defined(CONFIG_ARCH_PXA_POODLE)
	int volt;


	DPRINTK("Check fatal batt\n");

	if ((GPLR(GPIO_AC_IN) & GPIO_bit(GPIO_AC_IN)) != 0 ) {
	  DPRINTK("fatal check. ac in\n");

	  volt = sharpsl_read_MainBattery();
	  DPRINTK("fatal chk = %d\n",volt);

	  if ( counter_step_save ) {
	    if ( volt < SHARPSL_POODLE_FATAL_ACIN_FLON_VOLT )
	      return 0;
            else
	      return 1;
	  } else {
	    if ( volt < SHARPSL_POODLE_FATAL_ACIN_FLOFF_VOLT )
	      return 0;
            else
	      return 1;
	  }

	} else {
	  DPRINTK("fatal check. ac not insert\n");

	  volt = sharpsl_read_MainBattery();
	  DPRINTK("fatal chk = %d\n",volt);

	  if ( counter_step_save ) {
	    if ( volt < SHARPSL_POODLE_FATAL_NOACIN_FLON_VOLT )
	      return 0;
            else
	      return 1;
	  } else {
	    if ( volt < SHARPSL_POODLE_FATAL_NOACIN_FLOFF_VOLT )
	      return 0;
            else
	      return 1;
	  }
	}
#else
	int buff[5],temp,i;

	DPRINTK("Check fatal batt\n");

#if 0 // cut ... 
	// Check Temp : check inserting battery ?
	for(i=0;i<5;i++) {
		buff[i] = sharpsl_read_Temp();
		mdelay(SHARPSL_CHECK_BATTERY_WAIT_TIME_TEMP*10);
	}

	DPRINTK("temp\n");
	temp = get_select_val(buff);

	if ( temp > SHARPSL_CHARGE_ON_TEMP ) {
	  CHARGE_LED_ERR();
	  // if temp is not good , so maybe battery is not inserted.
	  while(1) {
	    if ((GPLR(GPIO_AC_IN) & GPIO_bit(GPIO_AC_IN))==0) break;
	  }
	  return 0;
	}
#endif

	// Check AC-Adapter
	if ((GPLR(GPIO_AC_IN) & GPIO_bit(GPIO_AC_IN)) != 0 ) {
	  // insert ac
	  DPRINTK("in-ac\n");

	  if ( charge_status == 1 ) {
	    DPRINTK("fatal : charge off\n");
	    CHARGE_OFF();
	    udelay(100);
	    DISCHARGE_ON();	// enable discharge
	    mdelay(SHARPSL_WAIT_DISCHARGE_ON*10);
	  }

	  // Check battery : check inserting battery ?
	  for(i=0;i<5;i++) {
		buff[i] = sharpsl_read_MainBattery();
		mdelay(SHARPSL_CHECK_BATTERY_WAIT_TIME_VOLT*10);
	  }

	  DPRINTK("volt\n");
	  temp = get_select_val(buff);
	  DPRINTK("fatal chk = %d\n",temp);

	  if ( charge_status == 1 ) {
	    udelay(100);
	    CHARGE_ON();
	    GEDR(GPIO_CO) = GPIO_bit(GPIO_CO);  // FIX ME !
	    //DPRINTK("CO = %x\n",GEDR&GPIO_CO);
	    DISCHARGE_OFF();	// disable discharge
	  }

	  if ( temp < SHARPSL_CORGI_FATAL_ACIN_VOLT ) 
	    return 0;
	  else
	    return 1;

	} else {
	  // not insert ac
	  DPRINTK("no-ac\n");

	  // Check battery : check inserting battery ?
	  for(i=0;i<5;i++) {
		buff[i] = sharpsl_read_MainBattery();
		mdelay(SHARPSL_CHECK_BATTERY_WAIT_TIME_VOLT*10);
	  }
	  DPRINTK("volt\n");
	  temp = get_select_val(buff);
	  DPRINTK("fatal chk = %d\n",temp);

	  if ( temp < SHARPSL_CORGI_FATAL_NOACIN_VOLT ) 
	    return 0;
	  else
	    return 1;

	}

#endif

}

/*** for suspend hook ***********************************************************/
int sharpsl_off_charge_battery(void)
{


  DPRINTK("charge ? %s\n", (GPLR1 & GPIO_bit(GPIO38_FFRI)) ? "on" : "off" );

  while(1) {
    switch ( sharpsl_charge_state ) {

    case CHARGE_STEP1:
      {
	DPRINTK("STEP 1\n");

#if defined(CONFIG_ARCH_PXA_SHEPHERD)
	if ( !(GPLR(GPIO_MAIN_BAT_LOW) & GPIO_bit(GPIO_MAIN_BAT_LOW)) ) {
	  // unlock
	  CHARGE_LED_OFF();
	  CHARGE_OFF();
	  sharpsl_charge_state = CHARGE_STEP1;
	  charge_status = 0;
	  return 0;
	}
#endif

#if defined(CONFIG_ARCH_PXA_CORGI)
	/* AC Check */
	if ( sharpsl_ac_check() ) {
	  sharpsl_charge_state = CHARGE_STEP_ERROR;
	  break;
	}
#endif

	if ( sharpsl_check_battery(1) ) {
	  sharpsl_charge_state = CHARGE_STEP_ERROR;
	} else {
	  sharpsl_charge_state = CHARGE_STEP2;
	}
	break;
      }

    case CHARGE_STEP2:
      {
	DPRINTK("STEP 2\n");

	/* Charge Start */
	CHARGE_LED_OFF();
	CHARGE_LED_ON();	/* led on */
	CHARGE_OFF();
	mdelay(SHARPSL_CHARGE_WAIT_TIME);
	CHARGE_ON();		/* Charge ON */
	charge_status = 1;	/* charge status flag set */

	/* wait time */
	sharpsl_charge_state = CHARGE_STEP3;
	return 1;
      }

    case CHARGE_STEP3:
      {
	DPRINTK("STEP 3\n");

	if ( sharpsl_check_battery(0) ) {
	  sharpsl_charge_state = CHARGE_STEP_ERROR;
	} else {
	  CHARGE_OFF();
	  mdelay(SHARPSL_CHARGE_WAIT_TIME);
	  CHARGE_ON();		/* Charge ON */
	  charge_status = 1;	/* charge status flag set */
	  sharpsl_charge_state = CHARGE_STEP4;
	}
	break;
      }

    case CHARGE_STEP4:
      {

	DPRINTK("STEP 4\n");

	mdelay(SHARPSL_CHARGE_CO_CHECK_TIME);

#if defined(CONFIG_ARCH_PXA_CORGI)
	{
	  int time;
	  unsigned int ret;
	  extern int sharpsl_wakeup_check_charge(void);
	  time = RCNR;
	  while(1) {
	    ret = sharpsl_wakeup_check_charge();
	    if ( ( RCNR - time ) > SHARPSL_CORGI_WAIT_CO_TIME ) return 1;
	    if ( ret != 0 ) return 0;
	    if ( GPLR(GPIO_CO) & GPIO_bit(GPIO_CO) ) break;
	  }
	}
#endif

	if ( GPLR(GPIO_CO) & GPIO_bit(GPIO_CO) ) {
	  sharpsl_charge_state = CHARGE_STEP5;
	  DPRINTK("co occured\n");
	  break;
	  //return 1;
	} else {
	  return 1;
	}
      }

    case CHARGE_STEP5:
      {
	DPRINTK("STEP 5\n");

	sharpsl_charge_cnt = 0;

	// retry charge
	CHARGE_OFF();
	mdelay(SHARPSL_CHARGE_WAIT_TIME);
	CHARGE_ON();		/* Charge ON */
	sharpsl_charge_state = CHARGE_STEP6;
	//break;
	return 1;
      }

    case CHARGE_STEP6:
      {
	DPRINTK("STEP 6\n");

	mdelay(SHARPSL_CHARGE_CO_CHECK_TIME);

#if defined(CONFIG_ARCH_PXA_CORGI)
	{
	  int time;
	  unsigned int ret;
	  extern int sharpsl_wakeup_check_charge(void);
	  time = RCNR;
	  while(1) {
	    ret = sharpsl_wakeup_check_charge();
	    if ( ( RCNR - time ) > SHARPSL_CORGI_WAIT_CO_TIME ) break;
	    if ( ret != 0 ) return 0;
	    if ( GPLR(GPIO_CO) & GPIO_bit(GPIO_CO) ) {
	      break;
	    }
	  }
	}
#endif

	sharpsl_charge_cnt++;
	if ( GPLR(GPIO_CO) & GPIO_bit(GPIO_CO) ) {
	  sharpsl_charge_state = CHARGE_STEP7;
	  sharpsl_charge_cnt = 0;
	  sharpsl_charge_dummy_cnt = 1;
	  DPRINTK("co occured\n");
	  //return 1;
	  break;
	}


	if ( sharpsl_charge_cnt > SHARPSL_CHARGE_RETRY_CNT ) {
	  DPRINTK("current co is fake. retry\n");
	  sharpsl_charge_state = CHARGE_STEP3;
	  break;
	} else {
	  // no counter is not finished.
	  return 1;
	}
	break;
      }

    case CHARGE_STEP7:
      {
	DPRINTK("STEP 7\n");

	sharpsl_charge_dummy_cnt--;

	if ( sharpsl_charge_dummy_cnt == 0 ) {
	  sharpsl_charge_state = CHARGE_STEP_END;
	  DPRINTK("chare is finished !\n");
	  break;
	} else {
	  // chaging is not finished.
	  return 1;
	}
      }

    default:
      break;
    }

      if ( sharpsl_charge_state == CHARGE_STEP_END ) {
	DPRINTK("STEP END\n");
	DPRINTK("charge end \n");
	CHARGE_LED_OFF();
	CHARGE_OFF();
	charge_status = 0;
	//sharpsl_charge_state = CHARGE_STEP1;
	return 0;
      }

      if ( sharpsl_charge_state == CHARGE_STEP_ERROR ) {
	DPRINTK("STEP ERROR\n");
	CHARGE_OFF();
	/* error led on */
	CHARGE_LED_ERR_OFF();
	//CHARGE_LED_ERR();
	charge_status = 0;
	  // if error is occured , so led is blinking continue...
	  while(1) {
	    if ((GPLR(GPIO_AC_IN) & GPIO_bit(GPIO_AC_IN))==0) break;
	  }
	CHARGE_LED_OFF();
	//sharpsl_charge_state = CHARGE_STEP1;
	return 0;
      }

      if ( sharpsl_charge_state == CHARGE_STEP_EXIT ) {
	DPRINTK("STEP EXIT\n");
	charge_status = 0;
	//sharpsl_charge_state = CHARGE_STEP1;
	return 0;
      }

  }

}


void sharpsl_battery_charge_hook(int mode)
{

	switch (mode){
	case 0:			// CO
	  {
	    DPRINTK("co interrupt\n");
	  }
	  break;
	case 1:			// Charge Off
	  {
	    DPRINTK("ac remove\n");
	    CHARGE_LED_OFF();
	    CHARGE_OFF();
	    sharpsl_charge_state = CHARGE_STEP1;
	    charge_status = 0;
	  }
	  break;
	case 2:			// Charge on
	  {
	    sharpsl_charge_state = CHARGE_STEP1;
	    charge_status = 0;
	    pass_charge_flag = 0;

	    DPRINTK("ac insert\n");
	  }
	  break;

	default:
	  break;
	}
}



/*** Config & Setup **********************************************************/
void battery_init(void)
{
	int err;


	/* poodle: TEMP_ON / corgi: pullup resistor */
	GPDR(GPIO21_TEMP) |= GPIO_bit(GPIO21_TEMP);
	/* charge on signal */
	GPDR(GPIO38_FFRI) |= GPIO_bit(GPIO38_FFRI);
#if defined(CONFIG_ARCH_PXA_CORGI)
	GPDR(GPIO43_BTTXD) |= GPIO_bit(GPIO43_BTTXD);
#endif
#if defined(CONFIG_ARCH_PXA_POODLE)
	GPDR(GPIO36_FFDCD) |= GPIO_bit(GPIO36_FFDCD);
#endif

	init_timer(&ac_kick_timer);
	ac_kick_timer.function = sharpsl_ac_kick_timer;


	GPDR(GPIO_AC_IN) &= ~GPIO_bit(GPIO_AC_IN);
	GPDR(GPIO_CO) &= ~GPIO_bit(GPIO_CO);

	/* Set transition detect */
	set_GPIO_IRQ_edge( GPIO_AC_IN  , GPIO_BOTH_EDGES ); /* AC IN */
#if defined(CONFIG_ARCH_PXA_POODLE) && !defined(CONFIG_ARCH_SHARP_SL_J)
	set_GPIO_IRQ_edge( GPIO_CO     , GPIO_RISING_EDGE ); /* CHRG FULL */
#endif

	/* Register interrupt handler. */
	if ((err = request_irq(IRQ_GPIO_AC_IN, Sharpsl_ac_interrupt,
			SA_INTERRUPT, "ACIN", Sharpsl_ac_interrupt))) {
		printk("Could not get irq %d.\n", IRQ_GPIO_AC_IN);
		return;
	}


#if defined(CONFIG_ARCH_PXA_POODLE) && !defined(CONFIG_ARCH_SHARP_SL_J)
	/* Register interrupt handler. */
	if ((err = request_irq(IRQ_GPIO_CO, Sharpsl_co_interrupt, SA_INTERRUPT,
			 "CO", Sharpsl_co_interrupt))) {
		free_irq(IRQ_GPIO_AC_IN, Sharpsl_ac_interrupt);
		printk("Could not get irq %d.\n", IRQ_GPIO_CO);
		return;
	}
#endif

	/* regist to Misc driver */
	misc_register(&battery_device);

	/* Make threads */
	kernel_thread(sharpsl_charge_on,      NULL, CLONE_FS | CLONE_FILES | CLONE_SIGHAND | SIGCHLD);
	kernel_thread(sharpsl_charge_off,     NULL, CLONE_FS | CLONE_FILES | CLONE_SIGHAND | SIGCHLD);
	kernel_thread(sharpsl_battery_thread, NULL, CLONE_FS | CLONE_FILES | CLONE_SIGHAND | SIGCHLD);

}



#ifdef CONFIG_PROC_FS
struct proc_dir_entry *proc_batt;

typedef struct sharpsl_battery_entry {
	int*		addr;
	int		def_value;
	char*		name;
	char*		description;
	unsigned short	low_ino;
} sharpsl_battery_entry_t;

static sharpsl_battery_entry_t sharpsl_battery_params[] = {
/*  { addr,	def_value,	name,	    description }*/
  { &msglevel,	0,		"msglevel",    "debug message output level" },
  { &sharpsl_debug_flag , 0 , "dflag", "debug flag" },
  { &sharpsl_change_battery_status , 0 , "chg_status", "Change status" }
};
#define NUM_OF_BATTERY_ENTRY	(sizeof(sharpsl_battery_params)/sizeof(sharpsl_battery_entry_t))

static ssize_t sharpsl_battery_read_params(struct file *file, char *buf,
				      size_t nbytes, loff_t *ppos)
{
	int i_ino = (file->f_dentry->d_inode)->i_ino;
	char outputbuf[15];
	int count;
	int i;
	sharpsl_battery_entry_t	*current_param = NULL;

	if (*ppos>0) /* Assume reading completed in previous read*/
		return 0;
	for (i=0; i<NUM_OF_BATTERY_ENTRY; i++) {
		if (sharpsl_battery_params[i].low_ino==i_ino) {
			current_param = &sharpsl_battery_params[i];
			break;
		}
	}
	if (current_param==NULL) {
		return -EINVAL;
	}
	count = sprintf(outputbuf, "0x%08X\n",
			*((volatile Word *) current_param->addr));
	*ppos += count;
	if (count>nbytes)	/* Assume output can be read at one time */
		return -EINVAL;
	if (copy_to_user(buf, outputbuf, count))
		return -EFAULT;
	return count;
}

static ssize_t sharpsl_battery_write_params(struct file *file, const char *buf,
				       size_t nbytes, loff_t *ppos)
{
	int			i_ino = (file->f_dentry->d_inode)->i_ino;
	sharpsl_battery_entry_t	*current_param = NULL;
	int			i;
	unsigned long		param;
	char			*endp;

	for (i=0; i<NUM_OF_BATTERY_ENTRY; i++) {
		if(sharpsl_battery_params[i].low_ino==i_ino) {
			current_param = &sharpsl_battery_params[i];
			break;
		}
	}
	if (current_param==NULL) {
		return -EINVAL;
	}

	param = simple_strtoul(buf,&endp,0);
	if (param == -1) {
		*current_param->addr = current_param->def_value;
	} else {
		*current_param->addr = param;
	}
	return nbytes+endp-buf;
}

static unsigned int sharpsl_battery_poll(struct file *fp, poll_table * wait)
{
  poll_wait(fp, &battery_waitqueue, wait);
  if ( sharpsl_change_battery_status ) {
    sharpsl_change_battery_status = 0;
    return POLLIN | POLLRDNORM;
  } else {
    return 0;
  }
}

static struct file_operations proc_params_operations = {
	read:	sharpsl_battery_read_params,
	write:	sharpsl_battery_write_params,
	poll:	sharpsl_battery_poll,
};
#endif




int __init Sharpsl_battery_init(void)
{
#ifdef CONFIG_PM
        battery_pm_dev = pm_register(PM_SYS_DEV, 0, Sharpsl_battery_pm_callback);
#endif

	battery_init();

#ifdef CONFIG_PROC_FS
	{
	int	i;
	struct proc_dir_entry *entry;

	proc_batt = proc_mkdir("driver/battery", NULL);
	if (proc_batt == NULL) {
	  free_irq(IRQ_GPIO_AC_IN, Sharpsl_ac_interrupt);
#if defined(CONFIG_ARCH_PXA_POODLE) && !defined(CONFIG_ARCH_SHARP_SL_J)
	  free_irq(IRQ_GPIO_CO, Sharpsl_co_interrupt);
#endif
	  misc_deregister(&battery_device);
	  printk(KERN_ERR "can't create /proc/driver/battery\n");
	  return -ENOMEM;
	}
	for (i=0; i<NUM_OF_BATTERY_ENTRY; i++) {
		entry = create_proc_entry(sharpsl_battery_params[i].name,
					  S_IWUSR |S_IRUSR | S_IRGRP | S_IROTH,
					  proc_batt);
		if (entry) {
			sharpsl_battery_params[i].low_ino = entry->low_ino;
			entry->proc_fops = &proc_params_operations;
		} else {
			int	j;
			for (j=0; j<i; j++) {
				remove_proc_entry(sharpsl_battery_params[i].name,
						  proc_batt);
			}
			remove_proc_entry("driver/battery", &proc_root);
			proc_batt = 0;
			free_irq(IRQ_GPIO_AC_IN, Sharpsl_ac_interrupt);
#if defined(CONFIG_ARCH_PXA_POODLE) && !defined(CONFIG_ARCH_SHARP_SL_J)
			free_irq(IRQ_GPIO_CO, Sharpsl_co_interrupt);
#endif
			misc_deregister(&battery_device);
			printk(KERN_ERR "battery: can't create /proc/driver/battery\n");
			return -ENOMEM;
		}
	}
	}
#endif


	return 0;
}

module_init(Sharpsl_battery_init);



#ifdef MODULE
int init_module(void)
{
	Sharpsl_battery_init();
	return 0;
}

void cleanup_module(void)
{
	free_irq(IRQ_GPIO_AC_IN, Sharpsl_ac_interrupt);
#if defined(CONFIG_ARCH_PXA_POODLE) && !defined(CONFIG_ARCH_SHARP_SL_J)
	free_irq(IRQ_GPIO_CO, Sharpsl_co_interrupt);
#endif

#ifdef CONFIG_PROC_FS
	{
	int	i;
	for (i=0; i<NUM_OF_BATTERY_ENTRY; i++) {
		remove_proc_entry(sharpsl_battery_params[i].name, proc_batt);
	}
	remove_proc_entry("driver/battery", NULL);
	proc_batt = 0;
	}
#endif

	misc_deregister(&battery_device);
}
#endif /* MODULE */
