/*
 * linux/arch/arm/mach-pxa/tosa_battery.c
 *
 * Based on
 *  linux/arch/arm/mach-sa1100/collie_battery.c
 *  linux/arch/arm/mach-pxa/sharpsl_battery.c
 *
 * Battery routines for tosa (SHARP)
 *
 * Copyright (C) 2004  SHARP
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

#include <asm/io.h>
#include <asm/hardware.h>
#include <asm/irq.h>
#include <asm/mach/irq.h>
#include <asm/hardirq.h>

#include <asm/sharp_char.h>
#include <asm/sharp_keycode.h>
#include <linux/ac97_codec.h>
#include <asm/arch/tosa_wm9712.h>
#include <asm/arch/keyboard_tosa.h>
#include <asm/arch/tosa.h>
#include <video/tosa_backlight.h>
#include <asm/arch/sharpsl_battery.h>

#define TOSA_BATTERY_FAILSAFE_LOWBAT
#define TOSA_BATTERY_AD_OFFSET (0)

#define TOSA_OFF_CHARGE_FAILSAFE

#define TOSA_FOR_ENGLISH

#ifdef DEBUG
#define DPRINTK(x, args...)  printk(x,##args)
#else
#define DPRINTK(x, args...)   if ( msglevel & 1 )	printk(x,##args);
#endif

#ifdef DEBUG2
#define DPRINTK2(x, args...)  printk( x,##args)
#else
#define DPRINTK2(x, args...)  if ( msglevel & 2 )	printk(x,##args);
#endif

#ifdef DEBUG3
#define DPRINTK3(x, args...)  printk( x,##args)
#else
#define DPRINTK3(x, args...)  if ( msglevel & 4 )	printk(x,##args);
#endif

#ifdef DEBUG4
#define DPRINTK4(x, args...)  printk( x,##args)
#else
#define DPRINTK4(x, args...)  if ( msglevel & 8 )	printk(x,##args);
#endif

#ifdef TOSA_FOR_ENGLISH
#ifdef DEBUG5
#define DPRINTK5(x, args...)  printk( x,##args)
#else
#define DPRINTK5(x, args...)  if ( msglevel & 16 )	printk("[CHKCHGFULL]: "x,##args);
#endif
#endif // TOSA_FOR_ENGLISH

#define CHECK_LOG (0) // for DEBUG
#if CHECK_LOG
int gStartTime = 0;
#endif

/*
 * The battery device is one of the misc char devices.
 * This is its minor number.
 */
#define	BATTERY_MINOR_DEV	215

#define SEL_MAIN	0
#define SEL_JACKET	1
#define SEL_BU	        2

static void tosa_charge_on_off(int,int);

#define CHARGE_ON()  ( { CHARGE_0_ON();  CHARGE_1_ON();  } )
#define CHARGE_OFF() ( { CHARGE_0_OFF(); CHARGE_1_OFF(); } )

#define CHARGE_0_ON()  tosa_charge_on_off(SEL_MAIN, 1)
#define CHARGE_0_OFF() tosa_charge_on_off(SEL_MAIN, 0)

#define CHARGE_1_ON()  tosa_charge_on_off(SEL_JACKET, 1)
#define CHARGE_1_OFF() tosa_charge_on_off(SEL_JACKET, 0)

extern void sharpsl_charge_err_off(void);
#define CHARGE_LED_ON()		set_led_status(SHARP_LED_CHARGER,LED_CHARGER_CHARGING)
#define CHARGE_LED_OFF()	set_led_status(SHARP_LED_CHARGER,LED_CHARGER_OFF)
#define CHARGE_LED_ERR()	set_led_status(SHARP_LED_CHARGER,LED_CHARGER_ERROR)
#define CHARGE_LED_ERR_OFF()	sharpsl_charge_err_off()

#define SHARPSL_BATTERY_STATUS_HIGH	APM_BATTERY_STATUS_HIGH
#define SHARPSL_BATTERY_STATUS_LOW	APM_BATTERY_STATUS_LOW
#define SHARPSL_BATTERY_STATUS_VERYLOW	APM_BATTERY_STATUS_VERY_LOW
#define SHARPSL_BATTERY_STATUS_CRITICAL	APM_BATTERY_STATUS_CRITICAL

#define APM_BATL_CLOSE 0
#define APM_BATL_OPEN  1
#define SHARPSL_BAT_LOCKED_STATUS	\
 (( GPLR(GPIO_BAT_LOCKED) & GPIO_bit(GPIO_BAT_LOCKED) )	? APM_BATL_CLOSE : APM_BATL_OPEN)

static int tosa_ac_line_status(int);
#define SHARPSL_AC_LINE_STATUS	tosa_ac_line_status(20 /* msec */)

#define SHARPSL_MAIN_BATT_FATAL \
 (( GPLR(GPIO_BAT0_LOW) & GPIO_bit(GPIO_BAT0_LOW) ) ? 0 : 1 )

#define SHARPSL_JACKET_BATT_FATAL \
 (( GPLR(GPIO_BAT1_LOW) & GPIO_bit(GPIO_BAT1_LOW) ) ? 0 : 1 )

#define SHARPSL_MAIN_BATT_FULL \
 (( GPLR(GPIO_BAT0_CRG) & GPIO_bit(GPIO_BAT0_CRG) ) ? 1 : 0 )

#define SHARPSL_JACKET_BATT_FULL \
 (( GPLR(GPIO_BAT1_CRG) & GPIO_bit(GPIO_BAT1_CRG) ) ? 1 : 0 )


#define SHARPSL_APO_DEFAULT		( ( 3 * 60 ) * SHARPSL_PM_TICK )	// 3 min
#define SHARPSL_LPO_DEFAULT		(  20 * SHARPSL_PM_TICK )		// 20 sec


#define SHARPSL_PM_TICK         	( HZ )          		        // 1 sec
#define SHARPSL_BATCHK_TIME		( SHARPSL_PM_TICK )			// 1 sec
#define SHARPSL_MAIN_GOOD_COUNT		( HZ / SHARPSL_PM_TICK )		// 1 sec
#define SHARPSL_MAIN_NOGOOD_COUNT	( HZ / SHARPSL_PM_TICK )		// 1 sec

#define SHARPSL_BATTERY_CK_TIME			( 10*60*1*100 )		// 10min ( base jiffies )
#define TOSA_TEMP_READ_WAIT_TIME		(5)    // 5msec [Fix]
#define TOSA_BATTERY_READ_WAIT_TIME         	(5)    // 5msec [Fix]
#define TOSA_DISCHARGE_WAIT_TIME                (10)   // 10msec

#define TOSA_MAIN_BATTERY_EXIST_THRESH         	(3600)  // 2.9V [Fix]
#define TOSA_JACKET_BATTERY_EXIST_THRESH        (3600)  // 2.9V [Fix]

#define TOSA_MAIN_BATTERY_ERR_THRESH           (1200)  // 2.9V [Fix]
#define TOSA_JACKET_BATTERY_ERR_THRESH         (1200)  // 2.9V [Fix]


#define TOSA_MAIN_BATT_FATAL_ACIN_VOLT         (1572+TOSA_BATTERY_AD_OFFSET)  // 3.80V [Fix]
#define TOSA_JACKET_BATT_FATAL_ACIN_VOLT       (1572+TOSA_BATTERY_AD_OFFSET)  // 3.80V [Fix]
#define TOSA_MAIN_BATT_FATAL_NOACIN_VOLT       (1551+TOSA_BATTERY_AD_OFFSET)  // 3.75V [Fix]
#define TOSA_JACKET_BATT_FATAL_NOACIN_VOLT     (1551+TOSA_BATTERY_AD_OFFSET)  // 3.75V [Fix]


#define TOSA_JC_CHECK_WAIT_TIME		10	// 10*10mS Jacket Check [Fix]
#define TOSA_JC_CHECK_TIMES		5	// 5 times

#define SHARPSL_TOSA_WAIT_CO_TIME	20	// 20 Sec
						//NOTICE !!  you want to change this value , so you must change
						//           alarm check mirgin time ( +30 ) in the sharpsl_power.c.

#define SHARPSL_CAUTION_CONTRAST		TOSA_BL_CAUTION_CONTRAST
#define SHARPSL_RESET_CONTRAST			TOSA_BL_RESET_CONTRAST
#define SHARPSL_LIMIT_CONTRAST(x)		tosa_bl_set_limit_contrast(x)

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
#define SHARPSL_CNV_TEMP_NUM	10


typedef struct BatteryThresh {
    int voltage;
    int percentage;
    int status;
} BATTERY_THRESH;

/*** prototype *********************************************************************/
static int tosa_get_jacket_status(void);
static void tosa_request_off(void);
static int tosa_battery_thread_main(void);
static int tosa_get_sel_battery(int); //[Fix]
static int tosa_read_battery(int); //[Fix]
static int tosa_read_temp(int); //[Fix]
static int tosa_read_battery_avg(int);
static int tosa_read_temp_avg(int); //[Fix]
static int tosa_check_battery_error(int,int);
static int tosa_check_battery_exist(int,int); //[Fix]
static int tosa_check_main_battery_error(void);
static int tosa_check_jacket_battery_error(void);
static int tosa_check_main_battery_not_exist(void); //[Fix]
static int tosa_check_jacket_battery_not_exist(void); //[Fix]
static int tosa_read_main_battery(void);
static int tosa_read_jacket_battery(void);
static int tosa_read_backup_battery(void);
static int tosa_check_main_batt_fatal(void);
static int tosa_check_jacket_batt_fatal(void);
static void tosa_charge_start(void); //[Fix]
static void tosa_charge_stop(void); //[Fix]
static void tosa_reset_battery_info(void);
static void tosa_jacket_kick_timer(void);
static void tosa_jacket_interrupt(int,void *,struct pt_regs *);
static void tosa_ac_interrupt(int,void *,struct pt_regs *);
static void tosa_main_batt_fatal_interrupt(int,void *,struct pt_regs *);
static void tosa_jacket_batt_fatal_interrupt(int,void *,struct pt_regs *);
static int tosa_cnv_battery(int,int);
static void tosa_get_all_battery(void); //[Fix]
static int tosa_get_sel_charge_percent(int,int);
static BATTERY_THRESH *tosa_get_sel_level(int,int);
static int tosa_get_dac_value(int); //[Fix]
static int get_select_val(int *);
static int tosa_get_jacket_status_for_on(void);
static void tosa_off_jc_card_limit(void);


/* thread */
static int tosa_charge_on(void*);
static int tosa_charge_off(void*);
static int tosa_battery_thread(void*);
static int tosa_fatal_check(void*);
static int tosa_jacket_check(void*);


/*** extern ***********************************************************************/
extern u32 apm_wakeup_src_mask;
extern int counter_step_contrast;
extern void handle_scancode(unsigned char,int);
extern unsigned long ssp_get_dac_val(unsigned long,int);
extern int sharpsl_is_power_mode_sensitive_battery(void);
extern int ac97_ad_input(int,unsigned short *);
extern int set_led_status(int,int);
extern unsigned short set_scoop2_gpio(unsigned short);
extern unsigned short reset_scoop2_gpio(unsigned short);

#ifdef CONFIG_PM
extern int pass_charge_flag;
//extern int counter_step_save;
#endif

/*** variables ********************************************************************/

/* BL 5-3 */
BATTERY_THRESH  sharpsl_main_battery_thresh_fl[] = {
    { 1663, 100, SHARPSL_BATTERY_STATUS_HIGH},
    { 1605,  75, SHARPSL_BATTERY_STATUS_HIGH},
    { 1564,  50, SHARPSL_BATTERY_STATUS_HIGH},
    { 1510,  25, SHARPSL_BATTERY_STATUS_LOW},
    { 1435,   5, SHARPSL_BATTERY_STATUS_VERYLOW},
    {   0,    0, SHARPSL_BATTERY_STATUS_CRITICAL},
};

/* BL 2-0 */
BATTERY_THRESH  sharpsl_main_battery_thresh_nofl[] = {
    { 1679, 100, SHARPSL_BATTERY_STATUS_HIGH},
    { 1617,  75, SHARPSL_BATTERY_STATUS_HIGH},
    { 1576,  50, SHARPSL_BATTERY_STATUS_HIGH},
    { 1530,  25, SHARPSL_BATTERY_STATUS_LOW},
    { 1448,   5, SHARPSL_BATTERY_STATUS_VERYLOW},
    {   0,    0, SHARPSL_BATTERY_STATUS_CRITICAL},
};

/* BL 5-3 */
BATTERY_THRESH  sharpsl_jacket_battery_thresh_fl[] = {
    { 1663, 100, SHARPSL_BATTERY_STATUS_HIGH},
    { 1605,  75, SHARPSL_BATTERY_STATUS_HIGH},
    { 1564,  50, SHARPSL_BATTERY_STATUS_HIGH},
    { 1510,  25, SHARPSL_BATTERY_STATUS_LOW},
    { 1435,   5, SHARPSL_BATTERY_STATUS_VERYLOW},
    {   0,    0, SHARPSL_BATTERY_STATUS_CRITICAL},
};

/* BL 2-0 */
BATTERY_THRESH  sharpsl_jacket_battery_thresh_nofl[] = {
    { 1679, 100, SHARPSL_BATTERY_STATUS_HIGH},
    { 1617,  75, SHARPSL_BATTERY_STATUS_HIGH},
    { 1576,  50, SHARPSL_BATTERY_STATUS_HIGH},
    { 1530,  25, SHARPSL_BATTERY_STATUS_LOW},
    { 1448,   5, SHARPSL_BATTERY_STATUS_VERYLOW},
    {   0,    0, SHARPSL_BATTERY_STATUS_CRITICAL},
};

BATTERY_THRESH  sharpsl_bu_battery_thresh_fl[] = {
    { 3226, 100, SHARPSL_BATTERY_STATUS_HIGH},
    {   0,   0, SHARPSL_BATTERY_STATUS_VERYLOW},
};

BATTERY_THRESH  sharpsl_bu_battery_thresh_nofl[] = {
    { 3226, 100, SHARPSL_BATTERY_STATUS_HIGH},
    {   0,   0, SHARPSL_BATTERY_STATUS_VERYLOW},
};


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
int sharpsl_jacket_battery   = SHARPSL_BATTERY_STATUS_HIGH;
int sharpsl_jacket_battery_percentage = 100;
int sharpsl_bu_battery   = SHARPSL_BATTERY_STATUS_HIGH;
int sharpsl_bu_battery_percentage = 100;

int sharpsl_bat_locked = 0;

int sharpsl_jacket_exist = 0;
int sharpsl_jacket_batt_exist = 0;
static int tosa_jacket_status_change = 0;
static int tosa_jacket_check_only = 0;
static int tosa_cardslot_error = 0;
#define ERR_CARDSLOT_CF1	(0x1<<0)
#define ERR_CARDSLOT_CF2	(0x1<<1)
#define ERR_CARDSLOT_SD		(0x1<<2)

int sharpsl_ac_status = APM_AC_OFFLINE;

static int MainCntWk = SHARPSL_MAIN_GOOD_COUNT;
static int MainCnt   = SHARPSL_MAIN_NOGOOD_COUNT;
static int back_ac_status = -1;

static int back_battery_status = -1;
static int back_jacket_battery_status = -1;
static int back_bu_battery_status = -1;
static int sharpsl_check_time = SHARPSL_BATCHK_TIME;

static int sharpsl_main_status_bk = SHARPSL_BATTERY_STATUS_HIGH;
static int sharpsl_main_percent_bk = 100;
static int sharpsl_jacket_status_bk = SHARPSL_BATTERY_STATUS_HIGH;
static int sharpsl_jacket_percent_bk = 100;
static int sharpsl_bu_status_bk = SHARPSL_BATTERY_STATUS_HIGH;
static int sharpsl_bu_percent_bk = 100;

int sharpsl_main_bk_flag = 0;

static struct timer_list ac_kick_timer;
static struct timer_list jacket_kick_timer;

#ifdef CONFIG_PM
static struct pm_dev *battery_pm_dev;	 /* registered PM device */
#endif

static int battery_off_flag = 0;	/* charge : suspend while get adc */
static int is_ac_adaptor = 0;		/* AC adaptor in/out */

static DECLARE_WAIT_QUEUE_HEAD(charge_on_queue);
static DECLARE_WAIT_QUEUE_HEAD(charge_off_queue);
static DECLARE_WAIT_QUEUE_HEAD(battery_waitqueue);
static DECLARE_WAIT_QUEUE_HEAD(fatal_chk_queue);
static DECLARE_WAIT_QUEUE_HEAD(jacket_chk_queue);

static int is_enabled_off = 1;

int sharpsl_charge_cnt = 0;
int sharpsl_charge_dummy_cnt = 1;

static int sharpsl_ad_main[SHARPSL_CNV_VALUE_NUM+1];
static int sharpsl_ad_index_main = 0;
static int sharpsl_ad_jacket[SHARPSL_CNV_VALUE_NUM+1];
static int sharpsl_ad_index_jacket = 0;
static int sharpsl_ad_bu[SHARPSL_CNV_VALUE_NUM+1];
static int sharpsl_ad_index_bu = 0;

static int tosa_jacket_check_count = 0;
static unsigned int tosa_jacket_state = 0;
static int tosa_dac_init = 0;


static int sharpsl_change_battery_status = 0;

static int tosa_jc_card_limit_sel = 0;

static int tosa_off_main_batt_full = 0;
static int tosa_off_jacket_batt_full = 0;

static int tosa_off_check_jacket_req = 0;

// for DEBUG
static int sharpsl_debug_flag = 0;

#ifdef TOSA_OFF_CHARGE_FAILSAFE
#define TOSA_CHARGE_FULL_MAX_TIME   (4*60*60) // 4H
#define TOSA_CHARGE_FULL_DIFF_VOLT  (20) // about 50mV
static int tosa_charge_full_enable = 0;
static int tosa_charge_full_counter = 0;
static int tosa_charge_full_main_batt = 0;
static int tosa_charge_full_jacket_batt = 0;
#endif //TOSA_OFF_CHARGE_FAILSAFE

#ifdef TOSA_FOR_ENGLISH
static int tosa_charge_full_state = 0;
static int tosa_decide_charge_full_counter = 0;
extern int logo_lang; // (default): En , (1): Jp
#define TOSA_DECIDE_CHARGE_FULL_COUNTER ( 60*100 ) // 1min ( base jiffies )
#define DISCONNECT_AC      (0)
#define CONNECT_AC         (1)
#define DETECT_CHARGE_FULL (2)
#define DECIDE_CHARGE_FULL (3)
#define GET_CHARGE_FULL_STATE() tosa_charge_full_state
#define SET_CHARGE_FULL_STATE(state) (tosa_charge_full_state = state)
#define IS_ENGLISH_MODEL() (logo_lang != 1)
#endif // TOSA_FOR_ENGLISH

void set_jacketslot_error( int on )
{
  if (on) {
    if (!(tosa_cardslot_error&ERR_CARDSLOT_CF2)) {
      tosa_cardslot_error |= ERR_CARDSLOT_CF2;
      sharpsl_change_battery_status = 1;
      wake_up_interruptible(&battery_waitqueue);
    }
  } else {
    if (tosa_cardslot_error&ERR_CARDSLOT_CF2) {
      tosa_cardslot_error &= ~ERR_CARDSLOT_CF2;
      sharpsl_change_battery_status = 1;
      wake_up_interruptible(&battery_waitqueue);
    }
  }
}

int current_cardslot_error( int num )
{
  if (num==0) {
    return (tosa_cardslot_error & ERR_CARDSLOT_CF1)?1:0;
  } else { // slot 1
    return (tosa_cardslot_error & ERR_CARDSLOT_CF2)?1:0;
  }
}

/*** apm subroutines  ***********************************************************/
static int tosa_get_jacket_status(void)
{
  if (tosa_jacket_check_count == 0){
    tosa_jacket_state = GPLR(GPIO_JACKET_DETECT) & GPIO_bit(GPIO_JACKET_DETECT);
    tosa_jacket_check_count++;
    return 0;
  }

  if (tosa_jacket_state == (GPLR(GPIO_JACKET_DETECT) & GPIO_bit(GPIO_JACKET_DETECT))){
    if (tosa_jacket_check_count++ >= TOSA_JC_CHECK_TIMES){
      if (tosa_jacket_state){
	sharpsl_jacket_exist = 0;      // non-exsistent (unit)
	sharpsl_jacket_batt_exist = 0; // non-exststent (battery)
	DPRINTK2("[BATT] jacket was removed.\n");
      }else{
	sharpsl_jacket_exist = 1;      // exsistent (unit)
	DPRINTK2("JACKET STATUS [ EXIST ]\n");
	if(!tosa_check_jacket_battery_not_exist()){
	  sharpsl_jacket_batt_exist = 1; // exsistent (battery)
	  DPRINTK2("JACKET BATTERY STATUS [ EXIST ]\n");
	}else{
	  sharpsl_jacket_batt_exist = 0;  // non-exststent (battery)
	  DPRINTK2("JACKET BATTERY STATUS [ NOT EXIST ]\n");
	}
      }
      // jacket battery charge control.
      if(sharpsl_jacket_batt_exist){
	CHARGE_1_ON();
      }else{
	CHARGE_1_OFF();
      }

      tosa_jacket_status_change = 1; // jacket status is changed.

      if(!tosa_jacket_check_only){
	sharpsl_change_battery_status = 1;		// change status
	wake_up_interruptible(&battery_waitqueue);
      }

      tosa_jacket_check_count = 0;
      return 1;
    }
  }else{
    tosa_jacket_check_count = 0;
  }
  return 0;
}

static int tosa_get_jacket_status_for_on(void)
{
  tosa_jacket_check_count = 0;
  tosa_jacket_check_only = 1;
  while(1){
    if(tosa_get_jacket_status()){
      mdelay(TOSA_JC_CHECK_WAIT_TIME*10);
      break;
    }
    mdelay(TOSA_JC_CHECK_WAIT_TIME*10);
  }
  tosa_jacket_check_only = 0;
  return 0;
}

int sharpsl_apm_get_power_status(u_char *ac_line_status,
				 u_char *battery_main_status,
				 u_char *battery_jacket_status,
				 u_char *battery_bu_status,
				 u_char *battery_flag,
				 u_char *battery_main_percentage,
				 u_char *battery_jacket_percentage,
				 u_char *battery_bu_percentage,
				 u_short *battery_main_life,
				 u_short *battery_jacket_life,
				 u_short *battery_bu_life)

{
  // set ac status
  *ac_line_status = sharpsl_ac_status;

  // set battery_status  main
  *battery_main_status = sharpsl_main_battery;
  *battery_jacket_status = sharpsl_jacket_battery;
  *battery_bu_status = sharpsl_bu_battery;
  
  // set battery percentage
  *battery_main_percentage = sharpsl_main_battery_percentage;
  *battery_jacket_percentage = sharpsl_jacket_battery_percentage;
  *battery_bu_percentage = sharpsl_bu_battery_percentage;

  if ( *ac_line_status == APM_AC_ONLINE ){
    *battery_main_percentage = 100;
    *battery_jacket_percentage = 100;
    *battery_bu_percentage = 100;
  }

  // set battery_flag
  if ( *battery_main_status == SHARPSL_BATTERY_STATUS_VERYLOW ){
    *battery_flag = (1 << 5);
  }else{
    *battery_flag = (1 << *battery_main_status);
  }

  // chk charge status
#ifdef TOSA_FOR_ENGLISH
  if ( charge_status && (!IS_ENGLISH_MODEL() || (GET_CHARGE_FULL_STATE() != DECIDE_CHARGE_FULL))) {
#else
  if ( charge_status ) {
#endif // TOSA_FOR_ENGLISH
    *battery_main_status = APM_BATTERY_STATUS_CHARGING;
    *battery_jacket_status = APM_BATTERY_STATUS_CHARGING;
    *battery_bu_status = APM_BATTERY_STATUS_CHARGING;
    *battery_flag = (1 << 3);
    // charging now, so can not get battery percentage.
    *battery_main_percentage = -1;
    *battery_jacket_percentage = -1;
    *battery_bu_percentage = -1;
  }

  // set battery life
  *battery_main_life = APM_BATTERY_LIFE_UNKNOWN;
  *battery_jacket_life = APM_BATTERY_LIFE_UNKNOWN;
  *battery_bu_life = APM_BATTERY_LIFE_UNKNOWN;
  
  return APM_SUCCESS;
}


int sharpsl_apm_get_bp_status(u_char *ac_line_status,
			      u_char *battery_status,
			      u_char *battery_flag,
			      u_char *battery_percentage,
			      u_short *battery_life)
{
  *ac_line_status = APM_AC_OFFLINE;//collie_ac_status;
  
  if ( sharpsl_jacket_batt_exist ){
    /* In charging stage ,battery status */
    if ( charge_status ){
#ifdef TOSA_FOR_ENGLISH
      if ( IS_ENGLISH_MODEL() && (GET_CHARGE_FULL_STATE() == DECIDE_CHARGE_FULL) ) {
	*battery_status = SHARPSL_BATTERY_STATUS_HIGH;
	*battery_percentage = 100;
	*battery_flag = (1 << SHARPSL_BATTERY_STATUS_HIGH);
      }else{
#endif // TOSA_FOR_ENGLISH
      *battery_status = APM_BATTERY_STATUS_CHARGING;
      *battery_flag = (1 << 3);//0x08;
      /* set battery percentage */
      *battery_percentage = -1;
#ifdef TOSA_FOR_ENGLISH
      }
#endif // TOSA_FOR_ENGLISH
    }else{
      *battery_status = sharpsl_jacket_battery;
      if( !(*battery_status) ){
	*battery_flag = 0x01;
      }else if(*battery_status == APM_BATTERY_STATUS_VERY_LOW){
	*battery_flag = 0x20;
      }else{
	*battery_flag = *battery_status << 1;
      }
      /* set battery percentage */
      *battery_percentage = sharpsl_jacket_battery_percentage;
    }
  }else{
    *battery_status = APM_BATTERY_STATUS_FAULT;
    *battery_flag = APM_BATTERY_STATUS_FAULT << 1;
    /* set battery percentage */
    *battery_percentage = 0;
  }

  /* set battery life */
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
  MainCntWk = MainCnt + 1;

  // before check battery
  mdelay(before_waitms*10);

  switch (flag) {
  case 0:
    tosa_battery_thread_main();
    break;
  case 1:
    if ( sharpsl_main_bk_flag == 0 ) {
      sharpsl_main_bk_flag = 1;
      tosa_battery_thread_main();
      sharpsl_main_bk_flag = 0;
    } else {
      tosa_battery_thread_main();
    }
    break;
  case 2:
    back_battery_status = -1;
    back_jacket_battery_status = -1;
    back_bu_battery_status = -1;
    tosa_battery_thread_main();
    break;
  }

  // after check battery
  mdelay(after_waitms*10);
}

static void tosa_request_off(void)
{
  is_enabled_off = 0;
  handle_scancode(SLKEY_OFF|KBDOWN , 1);
  mdelay(10);
  handle_scancode(SLKEY_OFF|KBUP   , 0);
}

static int tosa_battery_thread_main(void)
{
  // start battery check
  if ((jiffies > SHARPSL_BATTERY_CK_TIME) && (sharpsl_main_bk_flag == 0)) {
    sharpsl_main_bk_flag = 1;
    DPRINTK2("start check battery ! \n");
  }

  // get ac status. if change ac status , chk main battery.
  sharpsl_ac_status =  SHARPSL_AC_LINE_STATUS;
  if ( back_ac_status != sharpsl_ac_status ) {
    MainCntWk = MainCnt + 1;				// chk battery
    sharpsl_change_battery_status = 1;		// change status
    wake_up_interruptible(&battery_waitqueue);
  }
  back_ac_status = sharpsl_ac_status;

  // get battery status.
  tosa_get_all_battery();

  if ( sharpsl_ac_status == APM_AC_ONLINE ){
    set_scoop2_gpio(SCP2_CARD_LIMIT_SEL); // non-limited
    tosa_jc_card_limit_sel = 0;
  }else if( !sharpsl_jacket_batt_exist ){
    reset_scoop2_gpio(SCP2_CARD_LIMIT_SEL); // limited
    tosa_jc_card_limit_sel = 1;
  }else if(!sharpsl_main_bk_flag){
    //default.
    set_scoop2_gpio(SCP2_CARD_LIMIT_SEL); // non-limited
    tosa_jc_card_limit_sel = 0;
  }else if (sharpsl_jacket_battery == SHARPSL_BATTERY_STATUS_VERYLOW ||
	    sharpsl_jacket_battery == SHARPSL_BATTERY_STATUS_CRITICAL){
    reset_scoop2_gpio(SCP2_CARD_LIMIT_SEL); // limited
    tosa_jc_card_limit_sel = 1;
  }else {
    set_scoop2_gpio(SCP2_CARD_LIMIT_SEL); // non-limited
    tosa_jc_card_limit_sel = 0;
  }

  // if battery is low , backlight driver become to save power.
  if ( ( ( sharpsl_main_battery == SHARPSL_BATTERY_STATUS_VERYLOW  ) ||
       ( sharpsl_main_battery == SHARPSL_BATTERY_STATUS_CRITICAL ) ) &&
       ( (sharpsl_main_battery != back_battery_status) || (sharpsl_jacket_battery != back_jacket_battery_status) ) &&
       ( sharpsl_main_bk_flag ) && 
       ( ( !sharpsl_jacket_batt_exist ) || ( tosa_jc_card_limit_sel ) ) ) {
    SHARPSL_LIMIT_CONTRAST(SHARPSL_CAUTION_CONTRAST);
  } else if ( (sharpsl_main_battery != back_battery_status) || 
	      (sharpsl_jacket_battery != back_jacket_battery_status) ) {
    SHARPSL_LIMIT_CONTRAST(SHARPSL_RESET_CONTRAST);
  }

  back_battery_status = sharpsl_main_battery;
  back_jacket_battery_status = sharpsl_jacket_battery;
  back_bu_battery_status = sharpsl_bu_battery;

  // good or ac in   --> GOOD_COUNT
  // low or very low --> NOGOOD_COUNT
  if ( ( sharpsl_main_battery == SHARPSL_BATTERY_STATUS_HIGH ) || ( sharpsl_ac_status == APM_AC_ONLINE ) )
    MainCnt = SHARPSL_MAIN_GOOD_COUNT;
  else
    MainCnt = SHARPSL_MAIN_NOGOOD_COUNT;
  DPRINTK("[BATT] MainCnt = %d\n",MainCnt);

  if(tosa_jacket_status_change && sharpsl_main_bk_flag){
    int battlow = 1;

    tosa_jacket_status_change = 0; // status clear.

    // main battery low irq control.
    if(tosa_check_main_batt_fatal()){
      DPRINTK2("[BATT] IRQ: Disable BAT0\n");
      disable_irq(IRQ_GPIO_BAT0_LOW);
    }else{
      DPRINTK2("[BATT] IRQ: Enable BAT0\n");
      enable_irq(IRQ_GPIO_BAT0_LOW);
      battlow = 0;
    }

    // jacket battery low irq control.
    if(sharpsl_jacket_batt_exist){
      if(tosa_check_jacket_batt_fatal()){
	// jacket battery is fatal.
	DPRINTK2("[BATT] IRQ: Disable BAT1 (jacket battery fatal)\n");
	disable_irq(IRQ_GPIO_BAT1_LOW);
      }else{
	DPRINTK2("[BATT] IRQ: Enable BAT1 (status change)\n");
	enable_irq(IRQ_GPIO_BAT1_LOW);
	battlow = 0;
      }
    }else{
      // no jacket.
      DPRINTK2("[BATT] IRQ: Disable BAT1 (no jacket battery)\n");
      disable_irq(IRQ_GPIO_BAT1_LOW);
    }
    if(battlow)
      tosa_request_off();
  }

#ifdef TOSA_BATTERY_FAILSAFE_LOWBAT
  // check critical ( check fatal )
  if ( ( sharpsl_main_battery == SHARPSL_BATTERY_STATUS_CRITICAL ) && 
       is_enabled_off && sharpsl_main_bk_flag ) {

    if( !sharpsl_jacket_batt_exist ){
      tosa_request_off();
      DPRINTK2("[BATT] <* Fail-Safe *> Fatal Off by ADC \n");
    }else{
      if( sharpsl_jacket_battery == SHARPSL_BATTERY_STATUS_CRITICAL ){
	tosa_request_off();
	DPRINTK2("[BATT] <* Fail-Safe *>Fatal Off by ADC \n");
      }
    }
  }
#endif

  DPRINTK("[BATT] LOWBAT0=%d\n",tosa_check_main_batt_fatal());
  DPRINTK("[BATT] LOWBAT1=%d\n",tosa_check_jacket_batt_fatal());

#ifdef TOSA_BATTERY_FAILSAFE
  if( sharpsl_jacket_batt_exist ){
    // jacket battery exists. (main battery always exists.)
    if( sharpsl_main_bk_flag && is_enabled_off &&
	tosa_check_main_batt_fatal() && tosa_check_jacket_batt_fatal() ){

      tosa_request_off();
      DPRINTK("Fatal Off by DETECTOR\n");
    }
  }else{
    if( sharpsl_main_bk_flag && is_enabled_off &&
	( ( GPLR(GPIO_BAT0_LOW) & GPIO_bit(GPIO_BAT0_LOW) ) == 0 ) ){
      tosa_request_off();
      DPRINTK("Fatal Off by DETECTOR\n");
    }
  }
#endif

  if(TC6393_SYS_REG(TC6393_SYS_GPODSR1) & TC6393_CHARGE_OFF){
    CHARGE_0_ON();
  }else{
  }

#ifdef TOSA_FOR_ENGLISH
  if(IS_ENGLISH_MODEL() && sharpsl_main_bk_flag){

    int detected_charge_full = 0;

    // In other than the Japanese version,
    // the following processings are performed.
    // Charging LED is switched off on a full charge.

    switch(GET_CHARGE_FULL_STATE()){
    case DISCONNECT_AC:
      DPRINTK5("DISCONNECT_AC\n");
      if( sharpsl_ac_status == APM_AC_ONLINE ){
	DPRINTK5("* (%d) ac in !\n",tosa_charge_full_state);
	SET_CHARGE_FULL_STATE(CONNECT_AC);
      }
      break;
    case CONNECT_AC:
      DPRINTK5("CONNECT_AC\n");
      if( sharpsl_ac_status == APM_AC_OFFLINE ){
	DPRINTK5("* (%d) ac out !\n",tosa_charge_full_state);
	SET_CHARGE_FULL_STATE(DISCONNECT_AC);
	break;
      }

      DPRINTK5("* (%d) ac in !\n",tosa_charge_full_state);
      DPRINTK5("* TC6393_SYS_GPOOECR1 = %08x\n",TC6393_SYS_REG(TC6393_SYS_GPOOECR1));
      DPRINTK5("* TC6393_SYS_GPODSR1 = %08x\n",TC6393_SYS_REG(TC6393_SYS_GPODSR1));
      tosa_decide_charge_full_counter = 0;
      detected_charge_full = SHARPSL_MAIN_BATT_FULL;
      DPRINTK5("* (%d) main charge full = %d!\n",tosa_charge_full_state,detected_charge_full);
      if( detected_charge_full && sharpsl_jacket_exist && sharpsl_jacket_batt_exist ){
	detected_charge_full = SHARPSL_JACKET_BATT_FULL;
	DPRINTK5("* (%d) jacket charge full = %d!\n",tosa_charge_full_state,detected_charge_full);
      }
      if( detected_charge_full ){
	SET_CHARGE_FULL_STATE(DETECT_CHARGE_FULL);
	tosa_decide_charge_full_counter = jiffies;
	break;
      }
      break;
    case DETECT_CHARGE_FULL:
      DPRINTK5("DETECT_CHARGE_FULL !\n",tosa_charge_full_state);
      DPRINTK5("* TC6393_SYS_GPOOECR1 = %08x\n",TC6393_SYS_REG(TC6393_SYS_GPOOECR1));
      DPRINTK5("* TC6393_SYS_GPODSR1 = %08x\n",TC6393_SYS_REG(TC6393_SYS_GPODSR1));
      if( sharpsl_ac_status == APM_AC_OFFLINE ){
	DPRINTK5("* (%d) ac out !\n",tosa_charge_full_state);
	SET_CHARGE_FULL_STATE(DISCONNECT_AC); 
	break;
      }
      DPRINTK5("* (%d) c = %d \n",tosa_charge_full_state,(jiffies - tosa_decide_charge_full_counter));
      detected_charge_full = SHARPSL_MAIN_BATT_FULL;
      DPRINTK5("* (%d) main charge full = %d!\n",tosa_charge_full_state,detected_charge_full);
      if( detected_charge_full && sharpsl_jacket_exist && sharpsl_jacket_batt_exist ){
	detected_charge_full = SHARPSL_JACKET_BATT_FULL;
	DPRINTK5("* (%d) jacket charge full = %d!\n",tosa_charge_full_state,detected_charge_full);
      }
      if( !detected_charge_full ){
	DPRINTK5("* (%d) canceled charge full !\n",tosa_charge_full_state);
	SET_CHARGE_FULL_STATE(CONNECT_AC);
	break;
      }

      if((jiffies - tosa_decide_charge_full_counter) > TOSA_DECIDE_CHARGE_FULL_COUNTER){
	DPRINTK5("* (%d) fixed charge full !\n",tosa_charge_full_state);
	CHARGE_LED_OFF();
	SET_CHARGE_FULL_STATE(DECIDE_CHARGE_FULL);
	sharpsl_change_battery_status = 1;
	wake_up_interruptible(&battery_waitqueue);
	break;
      }
      break;
    case DECIDE_CHARGE_FULL:
      DPRINTK5("DECIDE_CHARGE_FULL !\n",tosa_charge_full_state);
      if( sharpsl_ac_status == APM_AC_OFFLINE ){
	DPRINTK5("* (%d)ac out !\n",tosa_charge_full_state);
	CHARGE_LED_OFF();
	SET_CHARGE_FULL_STATE(DISCONNECT_AC);
	break;
      }
      detected_charge_full = SHARPSL_MAIN_BATT_FULL;
      DPRINTK5("* (%d) main charge full = %d!\n",tosa_charge_full_state,detected_charge_full);
      if( detected_charge_full && sharpsl_jacket_exist && sharpsl_jacket_batt_exist ){
	detected_charge_full = SHARPSL_JACKET_BATT_FULL;
	DPRINTK5("* (%d) jacket charge full = %d!\n",tosa_charge_full_state,detected_charge_full);
      }
      if( !detected_charge_full ){
	DPRINTK5("* (%d)canceled charge full !\n",tosa_charge_full_state);
	CHARGE_LED_ON();
	SET_CHARGE_FULL_STATE(CONNECT_AC);
	sharpsl_change_battery_status = 1;
	wake_up_interruptible(&battery_waitqueue);
	break;
      }
      break;
    default:
      DPRINTK5("* (%d)error state !\n",tosa_charge_full_state);
      SET_CHARGE_FULL_STATE(DISCONNECT_AC);
      sharpsl_change_battery_status = 1;
      wake_up_interruptible(&battery_waitqueue);
      break;
    }
  }
#endif // TOSA_FOR_ENGLISH
  return 0;
}


static int tosa_battery_thread(void* unused)
{
  strcpy(current->comm, "sharpsl_bat");
  sharpsl_check_time = SHARPSL_BATCHK_TIME;

  while(1) {
    interruptible_sleep_on_timeout((wait_queue_head_t*)&battery_queue, sharpsl_check_time );

    // AC adapter is inserted , but charging not start.
    // SW can not confirm inserted AC adapter, so kick !
    if ( ( charge_status == 0 ) && ( sharpsl_ac_status == APM_AC_ONLINE )) {
      wake_up(&charge_on_queue);
    } else if ( ( charge_status == 1 ) && ( sharpsl_ac_status == APM_AC_OFFLINE )) {
      wake_up(&charge_off_queue);
    }

    /* check battery ! */
    tosa_battery_thread_main();
  }
  return 0;
}



// TOSA-FUNCTIONS

static void tosa_charge_on_off(int nsel,int on)
{
  switch(nsel) {
  case SEL_MAIN:
    TC6393_SYS_REG(TC6393_SYS_GPOOECR1) |= TC6393_CHARGE_OFF;
    if(on){
      if((TC6393_SYS_REG(TC6393_SYS_GPODSR1) & TC6393_CHARGE_OFF) != TC6393_CHARGE_OFF){ 
	TC6393_SYS_REG(TC6393_SYS_GPODSR1) |= TC6393_CHARGE_OFF;
	mdelay(1);
      }
      TC6393_SYS_REG(TC6393_SYS_GPODSR1) &= ~TC6393_CHARGE_OFF;
      mdelay(1);
    }else{
      TC6393_SYS_REG(TC6393_SYS_GPODSR1) |= TC6393_CHARGE_OFF;
    }
    break;
  case SEL_JACKET:
    TC6393_SYS_REG(TC6393_SYS_GPOOECR1) |= TC6393_CHARGE_OFF_JC;
    if(on){
      if((TC6393_SYS_REG(TC6393_SYS_GPODSR1) & TC6393_CHARGE_OFF_JC) != TC6393_CHARGE_OFF_JC){ 
	TC6393_SYS_REG(TC6393_SYS_GPODSR1) |= TC6393_CHARGE_OFF_JC;
	mdelay(1);
      }
      TC6393_SYS_REG(TC6393_SYS_GPODSR1) &= ~TC6393_CHARGE_OFF_JC;
      mdelay(1);
    }else{
      TC6393_SYS_REG(TC6393_SYS_GPODSR1) |= TC6393_CHARGE_OFF_JC;
    }
    break;
  default:
    printk("%s: parameter error !\n",__func__);
    break;
  }
}

//
// 0:main 1:jacket 2:backup
//
static int tosa_get_sel_battery(int nsel) //[Fix]
{
  int i = 0;
  BATTERY_THRESH *thresh;
  int voltage;

  while(1) {
    voltage = tosa_read_battery(nsel);
    if ( voltage > 0 ) break;
    if ( i++ > 5 ) {
      switch(nsel) {
      case SEL_MAIN:
	voltage = sharpsl_main_battery_thresh_fl[0].voltage + TOSA_BATTERY_AD_OFFSET;
	break;
      case SEL_JACKET:
	voltage = sharpsl_jacket_battery_thresh_fl[0].voltage + TOSA_BATTERY_AD_OFFSET;
	break;
      case SEL_BU:
	voltage = sharpsl_bu_battery_thresh_fl[0].voltage + TOSA_BATTERY_AD_OFFSET;
	break;
      }
      printk("please fix me! can not read battery[%d] \n",nsel);
      break;
    }
  }

  voltage = tosa_cnv_battery(nsel,voltage);
  thresh = tosa_get_sel_level(nsel,voltage);

  switch(nsel) {
  case SEL_MAIN:
    if ( sharpsl_main_bk_flag == 0 ) {
      return sharpsl_main_battery;
    }
    sharpsl_main_battery = thresh->status;
    sharpsl_main_battery_percentage = thresh->percentage;
    break;
  case SEL_JACKET:
    if ( sharpsl_main_bk_flag == 0 ) {
      return sharpsl_jacket_battery;
    }
    sharpsl_jacket_battery = thresh->status;
    sharpsl_jacket_battery_percentage = thresh->percentage;
    break;
  case SEL_BU:
    sharpsl_bu_battery = thresh->status;
    sharpsl_bu_battery_percentage = thresh->percentage;
    break;
  }

  // Value is kept until it will operate OFF and AC, once low AD comes out.
  switch(nsel){
  case SEL_MAIN:
    if ( sharpsl_main_battery_percentage < sharpsl_main_percent_bk ) {
      sharpsl_main_status_bk = sharpsl_main_battery;
      sharpsl_main_percent_bk = sharpsl_main_battery_percentage;
      
      sharpsl_change_battery_status = 1;
      wake_up_interruptible(&battery_waitqueue);
    } else {
      sharpsl_main_battery = sharpsl_main_status_bk;
      sharpsl_main_battery_percentage = sharpsl_main_percent_bk;
    }
    return sharpsl_main_battery;
    break;
  case SEL_JACKET:
    if ( sharpsl_jacket_battery_percentage < sharpsl_jacket_percent_bk) {
      sharpsl_jacket_status_bk = sharpsl_jacket_battery;
      sharpsl_jacket_percent_bk = sharpsl_jacket_battery_percentage;
      
      sharpsl_change_battery_status = 1;
      wake_up_interruptible(&battery_waitqueue);
    } else {
      sharpsl_jacket_battery = sharpsl_jacket_status_bk;
      sharpsl_jacket_battery_percentage = sharpsl_jacket_percent_bk;
    }
    return sharpsl_jacket_battery;
    break;
  case SEL_BU:
    if ( sharpsl_bu_battery_percentage < sharpsl_bu_percent_bk ) {
      sharpsl_bu_status_bk = sharpsl_bu_battery;
      sharpsl_bu_percent_bk = sharpsl_bu_battery_percentage;

      sharpsl_change_battery_status = 1;
      wake_up_interruptible(&battery_waitqueue);
    } else {
      sharpsl_bu_battery = sharpsl_bu_status_bk;
      sharpsl_bu_battery_percentage = sharpsl_bu_percent_bk;
    }
    return sharpsl_bu_battery;
    break;
  }
  return sharpsl_main_battery;
}

// 0:main 1:jacket 2:backup
static int tosa_read_battery(int nsel) //[Fix]
{
  int voltage = -1;

  switch (nsel){
  case SEL_MAIN:
    TC6393_SYS_REG(TC6393_SYS_GPOOECR1) |= (TC6393_BAT0_V_ON | TC6393_BAT1_V_ON | TC6393_BAT_SW_ON);
    TC6393_SYS_REG(TC6393_SYS_GPODSR1) &= ~(TC6393_BAT1_V_ON | TC6393_BAT_SW_ON);
    TC6393_SYS_REG(TC6393_SYS_GPODSR1) |= TC6393_BAT0_V_ON;
    mdelay(TOSA_BATTERY_READ_WAIT_TIME);
    voltage = tosa_get_dac_value(BATT_V);
    TC6393_SYS_REG(TC6393_SYS_GPODSR1) &= ~(TC6393_BAT0_V_ON | TC6393_BAT1_V_ON | TC6393_BAT_SW_ON);
    DPRINTK("[BATT] main voltage = %d \n",voltage);
    break;
  case SEL_JACKET:
    TC6393_SYS_REG(TC6393_SYS_GPOOECR1) |= (TC6393_BAT0_V_ON | TC6393_BAT1_V_ON | TC6393_BAT_SW_ON);
    TC6393_SYS_REG(TC6393_SYS_GPODSR1) &= ~(TC6393_BAT0_V_ON | TC6393_BAT_SW_ON);
    TC6393_SYS_REG(TC6393_SYS_GPODSR1) |= TC6393_BAT1_V_ON;
    mdelay(TOSA_BATTERY_READ_WAIT_TIME);
    voltage = tosa_get_dac_value(BATT_V);
    TC6393_SYS_REG(TC6393_SYS_GPODSR1) &= ~(TC6393_BAT0_V_ON | TC6393_BAT1_V_ON | TC6393_BAT_SW_ON);
    DPRINTK("[BATT] jacket voltage = %d \n",voltage);
    break;
  case SEL_BU:
    TC6393_SYS_REG(TC6393_SYS_GPOOECR1) |= (TC6393_BU_CHRG_ON);
    TC6393_SYS_REG(TC6393_SYS_GPODSR1) |= (TC6393_BU_CHRG_ON);
    mdelay(TOSA_BATTERY_READ_WAIT_TIME);
    voltage = tosa_get_dac_value(BU_V);
    TC6393_SYS_REG(TC6393_SYS_GPODSR1) &= ~TC6393_BU_CHRG_ON;
    DPRINTK("[BATT] backup voltage = %d \n",voltage);
    break;
  default:
    printk("%s: parameter error !\n",__func__);
    break;
  }
  return voltage;
}

static int tosa_read_temp(int nsel) //[Fix]
{
  int temp = -1;

  switch (nsel){
  case SEL_MAIN:
    TC6393_SYS_REG(TC6393_SYS_GPOOECR1) |= TC6393_BAT0_TH_ON | TC6393_BAT1_TH_ON;
    TC6393_SYS_REG(TC6393_SYS_GPODSR1) &= ~TC6393_BAT0_TH_ON;
    TC6393_SYS_REG(TC6393_SYS_GPODSR1) |= TC6393_BAT1_TH_ON;
    mdelay(TOSA_TEMP_READ_WAIT_TIME);
    temp = tosa_get_dac_value(BATT_TH);
    TC6393_SYS_REG(TC6393_SYS_GPODSR1) &= ~(TC6393_BAT0_TH_ON|TC6393_BAT1_TH_ON);
    break;
  case SEL_JACKET:
    TC6393_SYS_REG(TC6393_SYS_GPOOECR1) |= TC6393_BAT0_TH_ON | TC6393_BAT1_TH_ON;
    TC6393_SYS_REG(TC6393_SYS_GPODSR1) &= ~TC6393_BAT1_TH_ON;
    TC6393_SYS_REG(TC6393_SYS_GPODSR1) |= TC6393_BAT0_TH_ON;
    mdelay(TOSA_TEMP_READ_WAIT_TIME);
    temp = tosa_get_dac_value(BATT_TH);
    TC6393_SYS_REG(TC6393_SYS_GPODSR1) &= ~(TC6393_BAT0_TH_ON|TC6393_BAT1_TH_ON);
    break;
  default:
    printk("%s: parameter error !\n",__func__);
    break;
  }
  return temp;
}

static int tosa_read_battery_avg(int nsel)
{
  int temp,i;
  int buff[5];

  for(i=0;i<5;i++) {
    buff[i] = tosa_read_battery(nsel);
  }
  temp = get_select_val(buff);
  return temp;
}

static int tosa_read_temp_avg(int nsel) //[Fix]
{
  int temp,i;
  int buff[5];

  for(i=0;i<5;i++) {
    buff[i] = tosa_read_temp(nsel);
  }
  temp = get_select_val(buff);
  return temp;
}

static int tosa_check_battery_error(int nsel,int thresh)
{
  int temp;

  if ( in_interrupt() ) {
    printk("%s: call in interrupt.\n",__func__);
    return 1;
  }
  temp = tosa_read_battery_avg(nsel);
  
  if ( temp < thresh ) return 1; // (1) battery error!

  return 0; // (0) battery ok!
}

static int tosa_check_battery_exist(int nsel,int thresh) //[Fix]
{
  int temp;

  if ( in_interrupt() ) {
    printk("%s: call in interrupt.<nsel:%d>\n",__func__,nsel);
    return 1;
  }
  temp = tosa_read_temp_avg(nsel);
  DPRINTK3("[BATT] batt exist check <nsel = %d, temp = %d>\n",nsel,temp);

  if ( temp > thresh ) return 1; // (1) not exist

  return 0; // (0) exist
}

static int tosa_check_main_battery_error(void)
{
  // 0: not error , 1: error
  return tosa_check_battery_error(SEL_MAIN, TOSA_MAIN_BATTERY_ERR_THRESH);
}

static int tosa_check_jacket_battery_error(void)
{
  // 0: not error , 1: error
  return tosa_check_battery_error(SEL_JACKET, TOSA_JACKET_BATTERY_ERR_THRESH);
}

static int tosa_check_main_battery_not_exist(void) //[Fix]
{
#ifdef TOSA_BATTERY_SKIP
  return 0;
#endif //TOSA_BATTERY_SKIP

  // 0: exist , 1: not exist
  return tosa_check_battery_exist(SEL_MAIN, TOSA_MAIN_BATTERY_EXIST_THRESH);
}

static int tosa_check_jacket_battery_not_exist(void) //[Fix]
{
  // 0: exist , 1: not exist
  return tosa_check_battery_exist(SEL_JACKET, TOSA_JACKET_BATTERY_EXIST_THRESH);
}

static int tosa_read_main_battery(void)
{
  return tosa_get_sel_battery(SEL_MAIN);
}

static int tosa_read_jacket_battery(void)
{
  return tosa_get_sel_battery(SEL_JACKET);
}

static int tosa_read_backup_battery(void)
{
  return tosa_get_sel_battery(SEL_BU);
}

static int tosa_check_main_batt_fatal(void)
{
  int i;
  for(i=0;i<2;i++){
    udelay(1);
    if(!SHARPSL_MAIN_BATT_FATAL) return 0;
  }
  return 1; // (1) fatal
}

static int tosa_check_jacket_batt_fatal(void)
{
  int i;
  for(i=0;i<2;i++){
    udelay(1);
    if(!SHARPSL_JACKET_BATT_FATAL) return 0;
  }
  return 1; // (1) fatal
}

static void tosa_charge_start(void) //[Fix]
{
  if ( tosa_check_main_battery_not_exist() ) { 

    /* error led on */
    CHARGE_LED_ERR();

    /* charge status flag reset */
    charge_status = 0;

  } else {

    /* led on */
    CHARGE_LED_OFF();
    CHARGE_LED_ON();

    /* charge status flag set */
    charge_status = 1;
  }
  sharpsl_change_battery_status = 1;
  wake_up_interruptible(&battery_waitqueue);
}

static void tosa_charge_stop(void) //[Fix]
{
  if ( tosa_check_main_battery_not_exist() ){

    /* error led on */
    CHARGE_LED_ERR();

    /* charge status flag reset */
    charge_status = 0;

  }else{
    /* led off */
    CHARGE_LED_OFF();

    /* charge status flag reset */
    charge_status = 0;
  }
  sharpsl_change_battery_status = 1;
  wake_up_interruptible(&battery_waitqueue);
}


static void tosa_reset_battery_info(void)
{
  sharpsl_main_status_bk = SHARPSL_BATTERY_STATUS_HIGH;
  sharpsl_main_percent_bk = 100;
  sharpsl_jacket_status_bk = SHARPSL_BATTERY_STATUS_HIGH;
  sharpsl_jacket_percent_bk = 100;
  sharpsl_bu_status_bk = SHARPSL_BATTERY_STATUS_HIGH;
  sharpsl_bu_percent_bk = 100;

  sharpsl_ad_index_main = 0;
  sharpsl_ad_index_jacket = 0;
  sharpsl_ad_index_bu = 0;
}

static int tosa_ac_line_status(int m_sec)
{
  int ac_status=(( GPLR(GPIO_AC_IN) & GPIO_bit(GPIO_AC_IN) ) ? APM_AC_OFFLINE : APM_AC_ONLINE);
  int counter=0;
  if(m_sec == 0) return ac_status;
  while(1){
    mdelay(1);
    if(ac_status == (( GPLR(GPIO_AC_IN) & GPIO_bit(GPIO_AC_IN) ) ? APM_AC_OFFLINE : APM_AC_ONLINE)){
      counter++;
    }else{
      ac_status = (( GPLR(GPIO_AC_IN) & GPIO_bit(GPIO_AC_IN) ) ? APM_AC_OFFLINE : APM_AC_ONLINE);
      counter=0;
    }
    if(counter>m_sec) break;
  }
  return ac_status;
}

//////////////////////

void sharpsl_kick_battery_check_queue(void)
{
  MainCntWk = MainCnt + 1;
  wake_up(&battery_queue);
}

void sharpsl_kick_jacket_check_queue(void)
{
  wake_up_interruptible(&jacket_chk_queue);
}

void sharpsl_charge_start(void)
{
  tosa_charge_start();
}

int sharpsl_get_main_battery(void)
{
  return tosa_read_main_battery();
}

void sharpsl_request_dac_init(void)
{
  tosa_dac_init = 0;
}

static int tosa_charge_on(void* unused)
{

  // daemonize();
  strcpy(current->comm, "battchrgon");
  sigfillset(&current->blocked);

  while(1) {
    interruptible_sleep_on(&charge_on_queue);

    if (SHARPSL_AC_LINE_STATUS == APM_AC_OFFLINE) continue;

    // flag clear
    tosa_reset_battery_info();

    is_ac_adaptor = 1;

    tosa_charge_start();
  }
  return 0;
}


static int tosa_charge_off(void* unused)
{
  // daemonize();
  strcpy(current->comm, "battchrgoff");
  sigfillset(&current->blocked);

  while(1) {
    interruptible_sleep_on(&charge_off_queue);

    if (SHARPSL_AC_LINE_STATUS == APM_AC_ONLINE) continue;

    tosa_reset_battery_info();

    is_ac_adaptor = 0;

    tosa_charge_stop();
  }
  return 0;
}

static int tosa_fatal_check(void* unused)
{
  int is_fatal;

  // daemonize();
  strcpy(current->comm, "fatalchk");
  sigfillset(&current->blocked);

  while(1) {
    interruptible_sleep_on(&fatal_chk_queue);

    is_fatal=0;
    
    DPRINTK2("[BATT] BAT0LOW OR BAT1LOW IRQ \n");
    if(tosa_check_main_batt_fatal()){
      disable_irq(IRQ_GPIO_BAT0_LOW);
      DPRINTK2("[BATT] IRQ: Disable BAT0\n");
      is_fatal = 1;
    }
    if(sharpsl_jacket_exist && sharpsl_jacket_batt_exist){ 
      if(tosa_check_jacket_batt_fatal()){
	disable_irq(IRQ_GPIO_BAT1_LOW);
	DPRINTK2("[BATT] IRQ: Disable BAT1\n");
      }else{
	is_fatal = 0;
      }
    }

    if(is_fatal){
      DPRINTK2("[BATT] FATAL OFF !!!\n");
      tosa_request_off();
    }else{
      DPRINTK2("[BATT] FATAL OFF CANCEL !\n");
    }
  }
  return 0;
}

static int tosa_jacket_check(void* unused)
{
  int sts;

  // daemonize();
  strcpy(current->comm, "jacketchk");
  sigfillset(&current->blocked);

  while(1) {
    interruptible_sleep_on(&jacket_chk_queue);

    sts = tosa_get_jacket_status();
    if (sts == 0){
      mod_timer(&jacket_kick_timer,
		jiffies + HZ / 100 * TOSA_JC_CHECK_WAIT_TIME);
    }else{
      sharpsl_kick_battery_check_queue();
    }
  }
  return 0;
}

// card error ( System must eject the card )
// RET = 0:no error, bit0:pcmcia slot0, bit1:pcmcia slot1, bit2:sdslot0
int sharpsl_get_cardslot_error(void)
{
  int cardslot_err = tosa_cardslot_error;
  //printk("%s:\n",__func__);
  tosa_cardslot_error = 0; // clear
  return cardslot_err;
}

/*** Int ***********************************************************************/

static void tosa_jacket_kick_timer(void)
{
  sharpsl_kick_jacket_check_queue();
}

static void sharpsl_ac_kick_timer(void)
{
  int level = tosa_ac_line_status(0);
  int match = 0;
  int err = 0;

  while(1) {
    if ( level == tosa_ac_line_status(0) ) {
      match++;
    } else {
      level = tosa_ac_line_status(0);
      match = 0;
      err++;
    }
    if ( match > 1 ) break;
    if ( err > 50 ) {
      // if ac port is not stable , so use last port data.
      level = tosa_ac_line_status(0);
      printk("err : ac port\n");
      break;
    }
  }

#if 1 // Hardware Issue.
  if ( charge_status == 1 && level == APM_AC_OFFLINE ){
    // AC IN -> OUT
    wake_up(&charge_off_queue);
  }else if ( charge_status == 0 && level == APM_AC_ONLINE ){
    // AC OUT -> IN
    wake_up(&charge_on_queue);
  }
#else
  if ( level == APM_AC_OFFLINE ) {
    /* High->Low  : desert */
    wake_up(&charge_off_queue);
  } else {
    /* Low->High  : assert */
    wake_up(&charge_on_queue);
  }
#endif

  sharpsl_kick_battery_check_queue();
}

static void tosa_jacket_interrupt(int irq, void *dummy, struct pt_regs *fp)
{
  mod_timer(&jacket_kick_timer, jiffies + HZ / 100);
}

static void tosa_ac_interrupt(int irq, void *dummy, struct pt_regs *fp)
{
  mod_timer(&ac_kick_timer, jiffies + HZ / 100);
}

static void tosa_main_batt_fatal_interrupt(int irq, void *dummy, struct pt_regs *fp)
{
  wake_up_interruptible(&fatal_chk_queue);
}

static void tosa_jacket_batt_fatal_interrupt(int irq, void *dummy, struct pt_regs *fp)
{
  wake_up_interruptible(&fatal_chk_queue);
}

/*** get adc *********************************************************************/

static int tosa_cnv_battery(int nsel,int ad)
{
  int ad_val = 0;
  int ret = -1,i;

  switch(nsel){
  case SEL_MAIN:
    if ( sharpsl_main_battery != SHARPSL_BATTERY_STATUS_HIGH ) {
      sharpsl_ad_index_main = 0;
      return ad;
    }
    sharpsl_ad_main[sharpsl_ad_index_main] = ad;
    sharpsl_ad_index_main++;
    if ( sharpsl_ad_index_main >= SHARPSL_CNV_VALUE_NUM ) {
      for(i=0;i<(SHARPSL_CNV_VALUE_NUM-1);i++)
	sharpsl_ad_main[i] = sharpsl_ad_main[i+1];
      sharpsl_ad_index_main = SHARPSL_CNV_VALUE_NUM -1;
    }
    for(i=0;i<sharpsl_ad_index_main;i++)
      ad_val += sharpsl_ad_main[i];
    ret = ( ad_val / sharpsl_ad_index_main );
    break;
  case SEL_JACKET:
    if ( sharpsl_jacket_battery != SHARPSL_BATTERY_STATUS_HIGH ) {
      sharpsl_ad_index_jacket = 0;
      return ad;
    }
    sharpsl_ad_jacket[sharpsl_ad_index_jacket] = ad;
    sharpsl_ad_index_jacket++;
    if ( sharpsl_ad_index_jacket >= SHARPSL_CNV_VALUE_NUM ) {
      for(i=0;i<(SHARPSL_CNV_VALUE_NUM-1);i++)
	sharpsl_ad_jacket[i] = sharpsl_ad_jacket[i+1];
      sharpsl_ad_index_jacket = SHARPSL_CNV_VALUE_NUM -1;
    }
    for(i=0;i<sharpsl_ad_index_jacket;i++)
      ad_val += sharpsl_ad_jacket[i];
    ret = ( ad_val / sharpsl_ad_index_jacket );
    break;
  case SEL_BU:
    if ( sharpsl_bu_battery != SHARPSL_BATTERY_STATUS_HIGH ) {
      sharpsl_ad_index_bu = 0;
      return ad;
    }
    sharpsl_ad_bu[sharpsl_ad_index_bu] = ad;
    sharpsl_ad_index_bu++;
    if ( sharpsl_ad_index_bu >= SHARPSL_CNV_VALUE_NUM ) {
      for(i=0;i<(SHARPSL_CNV_VALUE_NUM-1);i++)
	sharpsl_ad_bu[i] = sharpsl_ad_bu[i+1];
      sharpsl_ad_index_bu = SHARPSL_CNV_VALUE_NUM -1;
    }
    for(i=0;i<sharpsl_ad_index_bu;i++)
      ad_val += sharpsl_ad_bu[i];
    ret = ( ad_val / sharpsl_ad_index_bu );
    break;
  default:
    printk("%s: parameter error !\n",__func__);
    break;
  }
  return ret;
}

static void tosa_get_all_battery(void) //[Fix]
{
  MainCntWk++;

  if ( MainCntWk > MainCnt ) {

    MainCntWk = 0;

    // check main battery status.
    tosa_read_main_battery();

    // check backup battery status.
    tosa_read_backup_battery();

    if (sharpsl_jacket_exist && sharpsl_jacket_batt_exist){
      // jacket exists , and jacket's battery exists.
      // check jacket battery status.
      tosa_read_jacket_battery();
      DPRINTK3("[BATT] jacket-unit is existend, and battery is existent therein.\n");
    }else{
      if(!sharpsl_jacket_exist){
	DPRINTK3("[BATT] jacket-unit is non-existent.\n");
      }else{
	DPRINTK3("[BATT] jacket-unit is existent, but battery is non-existent therein.\n");
      }
    }

  }else{
    DPRINTK("[BATT] MainCntWk = %d\n",MainCntWk);
  }
}

static BATTERY_THRESH *tosa_get_sel_level(int nsel, int Volt )
{
  int i;
  BATTERY_THRESH *thresh;

  //DPRINTK("  volt = %d  \n",Volt);

  switch(nsel){
  case SEL_MAIN:
    if (counter_step_contrast > 2)
      thresh = sharpsl_main_battery_thresh_fl;
    else
      thresh = sharpsl_main_battery_thresh_nofl;
    break;
  case SEL_JACKET:
    if (counter_step_contrast > 2)
      thresh = sharpsl_jacket_battery_thresh_fl;
    else
      thresh = sharpsl_jacket_battery_thresh_nofl;
    break;
  case SEL_BU:
    if (counter_step_contrast > 2)
      thresh = sharpsl_bu_battery_thresh_fl;
    else
      thresh = sharpsl_bu_battery_thresh_nofl;
    break;
  default:
    printk("%s: parameter error !\n",__func__);
    thresh = sharpsl_main_battery_thresh_fl;
    break;
  }
  for (i = 0; thresh[i].voltage > 0; i++) {
    if (Volt >= (thresh[i].voltage + TOSA_BATTERY_AD_OFFSET))
      return &thresh[i];
  }
  return &thresh[i];
}

/* 
 * Translate Analog signal to Digital data
 */
static int tosa_get_dac_value(int channel) //[Fix]
{
  unsigned short indata;
  int voltage;


  lock_FCS(POWER_MODE_BATTERY, 1);

  battery_off_flag = 0;

  if (tosa_dac_init == 0){
    ac97_write(AC97_TS_REG2, 0xc009);
    ac97_write(AC97_TS_REG1, 0x0030);
    //ac97_bit_set(AC97_ADITFUNC1, (1<<1));
    set_GPIO_mode(GPIO32_SDATA_IN1_AC97_MD);
    tosa_dac_init = 1;
  }

  wm9712_power_mode_ts(WM9712_PWR_TP_CONV);
  if ((voltage = ac97_ad_input(channel,&indata)) == 0){
    voltage = (int)(indata & 0xfff);
  }
  wm9712_power_mode_ts(WM9712_PWR_TP_WAIT);

#ifdef TOSA_BATTERY_SKIP
  voltage = 0xfff; // for TEST
#endif //TOSA_BATTERY_SKIP

  lock_FCS(POWER_MODE_BATTERY, 0);

  if ( battery_off_flag )
    voltage = -1;

  return voltage;
}


static int get_select_val(int *val)
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
  return ( sum / 3 );
}


/*** PM *************************************************************************/

#ifdef CONFIG_PM
static int Sharpsl_battery_pm_callback(struct pm_dev *pm_dev, pm_request_t req, void *data)
{
  switch (req) {
  case PM_SUSPEND:

    disable_irq(IRQ_GPIO_AC_IN);
    disable_irq(IRQ_GPIO_JACKET_DETECT);
#ifndef TOSA_BATTERY_SKIP
    disable_irq(IRQ_GPIO_BAT0_LOW);
    disable_irq(IRQ_GPIO_BAT1_LOW);
#endif

    // flag clear
    tosa_reset_battery_info();

    tosa_off_main_batt_full = 0;
    tosa_off_jacket_batt_full = 0;
    tosa_off_check_jacket_req = 0;

    if ( !sharpsl_main_bk_flag )
      sharpsl_main_battery = SHARPSL_BATTERY_STATUS_HIGH;

    battery_off_flag = 1;

    CHARGE_LED_OFF();
    charge_status = 0;
    pass_charge_flag = 0;

    apm_wakeup_src_mask |= ( GPIO_bit(GPIO_AC_IN) |
			     GPIO_bit(GPIO_JACKET_DETECT) );
    apm_wakeup_src_mask &= ~( GPIO_bit(GPIO_BAT0_CRG) | GPIO_bit(GPIO_BAT1_CRG) );

    // wake up by key
    apm_wakeup_src_mask |=  GPIO_bit(0);

#ifdef TOSA_OFF_CHARGE_FAILSAFE
    tosa_charge_full_enable = 0;
    tosa_charge_full_counter = 0;
    tosa_charge_full_main_batt = 0;
    tosa_charge_full_jacket_batt = 0;
#endif //TOSA_OFF_CHARGE_FAILSAFE
    break;

  case PM_RESUME:
    is_enabled_off = 1;
    MainCntWk = MainCnt + 1;	 // chk battery
    back_battery_status = -1;
    back_jacket_battery_status = -1;
    back_bu_battery_status = -1;
    back_ac_status = -1;
    tosa_dac_init = 0;
    sharpsl_ad_index_main = 0;
    sharpsl_ad_index_jacket = 0;
    sharpsl_ad_index_bu = 0;
    tosa_off_check_jacket_req = 0;
#ifdef TOSA_FOR_ENGLISH
    if(IS_ENGLISH_MODEL()){
      SET_CHARGE_FULL_STATE(DISCONNECT_AC);
      tosa_decide_charge_full_counter = 0;
    }
#endif // TOSA_FOR_ENGLISH

    DPRINTK("[BATT] kick ac check!\n");
    // AC insert , so kick the charging
    if (SHARPSL_AC_LINE_STATUS != APM_AC_OFFLINE) {
      wake_up(&charge_on_queue);
    } else {
      wake_up(&charge_off_queue);
    }

    DPRINTK2("[BATT] kick jacket check!\n");
    tosa_get_jacket_status_for_on();
    tosa_off_jc_card_limit();

    CHARGE_0_ON();

    DPRINTK("[BATT] kick battery check!\n");
    sharpsl_kick_battery_check_queue();

    DPRINTK("[BATT] enabled all irq!\n");
#ifndef TOSA_BATTERY_SKIP
    disable_irq(IRQ_GPIO_BAT1_LOW);
    disable_irq(IRQ_GPIO_BAT0_LOW);
#endif
    enable_irq(IRQ_GPIO_JACKET_DETECT);
    enable_irq(IRQ_GPIO_AC_IN);
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
  int volt,thresh_mbat,thresh_jbat;
  int charge_value;

  DPRINTK("[BATT] check fatal battery\n");

  if (SHARPSL_AC_LINE_STATUS != APM_AC_OFFLINE ) {
    thresh_mbat = TOSA_MAIN_BATT_FATAL_ACIN_VOLT;
    thresh_jbat = TOSA_JACKET_BATT_FATAL_ACIN_VOLT;
  }else{
    thresh_mbat = TOSA_MAIN_BATT_FATAL_NOACIN_VOLT;
    thresh_jbat = TOSA_JACKET_BATT_FATAL_NOACIN_VOLT;
  }

  charge_value = TC6393_SYS_REG(TC6393_SYS_GPODSR1) & TC6393_CHARGE_OFF; //Main
  CHARGE_0_OFF(); //Main
  TC6393_SYS_REG(TC6393_SYS_GPOOECR1) |= TC6393_BAT_SW_ON;
  TC6393_SYS_REG(TC6393_SYS_GPODSR1) |= TC6393_BAT_SW_ON;
  mdelay(TOSA_DISCHARGE_WAIT_TIME);
  TC6393_SYS_REG(TC6393_SYS_GPODSR1) &= ~TC6393_BAT_SW_ON;

  volt = tosa_read_battery_avg(SEL_MAIN);

  TC6393_SYS_REG(TC6393_SYS_GPODSR1) |= charge_value;
  charge_value=0;
  mdelay(1);

  DPRINTK3("[BATT] check batt exist(1)\n");
  if(tosa_check_jacket_battery_not_exist()){
    if( volt < thresh_mbat ){
      DPRINTK("[BATT] main fatal battery (no jacket)\n");
      return 0; // MAIN BATTERY FATAL
    }
  }else{
    if( volt < thresh_mbat ){

      charge_value = TC6393_SYS_REG(TC6393_SYS_GPODSR1) & TC6393_CHARGE_OFF_JC; //Jacket
      CHARGE_1_OFF(); //Jacket
      TC6393_SYS_REG(TC6393_SYS_GPOOECR1) |= TC6393_BAT_SW_ON;
      TC6393_SYS_REG(TC6393_SYS_GPODSR1) |= TC6393_BAT_SW_ON;
      mdelay(TOSA_DISCHARGE_WAIT_TIME);
      TC6393_SYS_REG(TC6393_SYS_GPODSR1) &= ~TC6393_BAT_SW_ON;

      volt = tosa_read_battery_avg(SEL_JACKET);

      TC6393_SYS_REG(TC6393_SYS_GPODSR1) |= charge_value;
      mdelay(1);

      if( volt < thresh_jbat ){
	DPRINTK("[BATT] main & jacket fatal battery\n");
	return 0; // MAIN & JACKET BATTERY FATAL
      }
    }
  }
  DPRINTK("[BATT] not fatal\n");

  return 1; // OK
}


/*** for suspend hook ***********************************************************/
#ifdef CONFIG_PM
int sharpsl_off_charge_battery(void)
{
  if ( tosa_check_main_battery_not_exist() ) {

    DPRINTK("No main battery !\n");

    // battery error!
    CHARGE_LED_ERR_OFF();
    CHARGE_LED_ERR();
    charge_status = 0; /* charge status flag reset */

    // if error is occured , so led is blinking continue...
    while(1) {
      if (SHARPSL_AC_LINE_STATUS == APM_AC_OFFLINE) break;
    }

    CHARGE_LED_OFF();
    return -1;

  } else {

    // charge start!
    CHARGE_LED_OFF();
    CHARGE_LED_ON();	/* led on */
    charge_status = 1;	/* charge status flag set */

  }
  return 0;
}

static void tosa_off_jc_card_limit(void)
{
  if(SHARPSL_AC_LINE_STATUS != APM_AC_OFFLINE){
    // ac-in.
    set_scoop2_gpio(SCP2_CARD_LIMIT_SEL);  // non-limited
    tosa_jc_card_limit_sel = 0;
  }else if(sharpsl_jacket_exist && sharpsl_jacket_batt_exist){
    // ac-out, and jacket exist.
    int volt = tosa_read_battery_avg(SEL_JACKET);
    if(volt > TOSA_JACKET_BATT_FATAL_NOACIN_VOLT){
      set_scoop2_gpio(SCP2_CARD_LIMIT_SEL); // non-limited
      tosa_jc_card_limit_sel = 0;
    }else{
      reset_scoop2_gpio(SCP2_CARD_LIMIT_SEL); // limited
      tosa_jc_card_limit_sel = 1;
    }
  }else{
    reset_scoop2_gpio(SCP2_CARD_LIMIT_SEL); // limited
    tosa_jc_card_limit_sel = 1;
  }
}

int tosa_check_charge_full(int cktime)
{
  int time;
  unsigned int ret;
  int is_all_battery_full;
  int jacket_state, batt_sw_state;
  extern int sharpsl_wakeup_check_charge(void);
  time = RCNR;

  jacket_state = (GPLR(GPIO_JACKET_DETECT) & GPIO_bit(GPIO_JACKET_DETECT));
  batt_sw_state = SHARPSL_BAT_LOCKED_STATUS;

#if CHECK_LOG
  printk("*** CHG_CHK_START: time=%d(%08x) ***\n",RCNR-gStartTime,RCNR);
  printk(" MAIN AD: %08x JACKET AD %08x\n",tosa_read_battery(SEL_MAIN),tosa_read_battery(SEL_JACKET));
  printk(" TC6393_SYS_GPODSR1=%x\n",TC6393_SYS_REG(TC6393_SYS_GPODSR1));
#endif

  if(TC6393_SYS_REG(TC6393_SYS_GPODSR1) & TC6393_CHARGE_OFF_JC /*Jacket*/){
    if(sharpsl_jacket_batt_exist){
#if CHECK_LOG
      printk(" jacket battery charge on !\n");
#endif
      CHARGE_1_ON();
    }
#if CHECK_LOG
    printk(" TC6393_SYS_GPODSR1=%x\n",TC6393_SYS_REG(TC6393_SYS_GPODSR1));
#endif
  }

#ifdef TOSA_OFF_CHARGE_FAILSAFE
  if(!tosa_charge_full_enable){
    tosa_charge_full_enable = 1;        // @ Start!!
    tosa_charge_full_counter = RCNR;    // @ Reset Failsafe Counter.
    tosa_charge_full_main_batt = tosa_read_battery(SEL_MAIN);
    if(sharpsl_jacket_batt_exist)
      tosa_charge_full_jacket_batt = tosa_read_battery(SEL_JACKET);
#if CHECK_LOG
    printk("*** CHG_FAILSAFE_COUNTER_START: time=%d(%08x) ***\n",RCNR-tosa_charge_full_counter,RCNR);
#endif
  }
#endif //TOSA_OFF_CHARGE_FAILSAFE

  while(1){
    int jacket_batt_exist_now = 0;

    // ac out ?
    if ( SHARPSL_AC_LINE_STATUS == APM_AC_OFFLINE ) { 
      charge_status = 0;
      CHARGE_LED_OFF();
      //printk("%s: [go off] ac out !\n",__func__);
#ifdef TOSA_OFF_CHARGE_FAILSAFE
      tosa_charge_full_enable = 0; // @ Stop!!
#endif
      ret = -1;
      break;
    }

    // check wakeup interrupt.
    if ( ( ret = sharpsl_wakeup_check_charge() ) != 1 ) {
      break;
    }

    // timeout ?
    if ( ( RCNR - time ) > SHARPSL_TOSA_WAIT_CO_TIME ) { 
      //printk("%s: [go off] finished! not full !\n",__func__);
      ret = -1;
      break;
    }

    // battery switch status change ?
    if(batt_sw_state != SHARPSL_BAT_LOCKED_STATUS){
      //printk("%s: [go off] battery switch status is changed !\n",__func__);
      ret = -1; 
      break;
    }

    // jacket detect status change ?
    if(jacket_state != (GPLR(GPIO_JACKET_DETECT) & GPIO_bit(GPIO_JACKET_DETECT))) {
      tosa_get_jacket_status_for_on();
#ifdef TOSA_OFF_CHARGE_FAILSAFE
      tosa_charge_full_enable = 0;  // @ Stop!!
#endif
      //printk("%s: [go off] jacket status is changed !\n",__func__);
      ret = -1; 
      break;
    }

    // battery full ?
    tosa_off_main_batt_full = SHARPSL_MAIN_BATT_FULL;
    tosa_off_jacket_batt_full = 0;
    if(sharpsl_jacket_exist && sharpsl_jacket_batt_exist){
      tosa_off_jacket_batt_full = SHARPSL_JACKET_BATT_FULL;
    }
    is_all_battery_full = 0;

#ifdef TOSA_OFF_CHARGE_FAILSAFE
    if(tosa_charge_full_enable){
      if((RCNR - tosa_charge_full_counter) > (TOSA_CHARGE_FULL_MAX_TIME-cktime)){
#if CHECK_LOG
	printk("*** ALL BATTERY is FULL by FAILSAFE.***\n");
	printk(" CHG_FAILSAFE_COUNTER: time=%d\n",RCNR - tosa_charge_full_counter);
#endif
	is_all_battery_full = 1;
      }
    }
#endif //TOSA_OFF_CHARGE_FAILSAFE

    if(charge_status && tosa_off_main_batt_full){
      // main battery full !
#if CHECK_LOG
      printk("\n");
      printk(" MAIN BATTERY is FULL.\n");
#endif
      if(sharpsl_jacket_batt_exist){
	if(tosa_off_jacket_batt_full){
	  // all battery full !
	  is_all_battery_full = 1;
#if CHECK_LOG
	  printk(" JACKET BATTERY is FULL.\n");
#endif
	}
      }else{
	// all battery full ! (non-jacket)
	is_all_battery_full = 1;
#if CHECK_LOG
	  printk(" JACKET BATTERY is NON.\n");
#endif
      }
    }
#if CHECK_LOG
    else{
      printk(".");
    }
#endif
    if(is_all_battery_full){

      // ac out ?
      if ( SHARPSL_AC_LINE_STATUS == APM_AC_OFFLINE ) { 
	continue;
      }

      // all battery full !!!!
      //printk("%s: [go off] all battery full !\n",__func__);
#if CHECK_LOG
      printk("\n");
      printk(" CHG_FAILSAFE_COUNTER: time=%d\n",RCNR - tosa_charge_full_counter);
      printk("*** CHG_CHK_END: t=%d (%08x) ***\n",RCNR-gStartTime,RCNR);
      printk("*** ALL BATTERY is FULL!! ***\n");
#endif
      CHARGE_LED_OFF(); // led off
      charge_status = 0;
      pass_charge_flag = 1;
      ret = -1; // go off.
      return ret;
    }
  }

#ifdef TOSA_OFF_CHARGE_FAILSAFE
  if(tosa_charge_full_enable){
    int batt_now;

#if CHECK_LOG
    printk("\n");
    printk(" MAIN VOLTAGE CHECK: ");
#endif
    batt_now = tosa_read_battery(SEL_MAIN);
    if((tosa_charge_full_main_batt - batt_now) >= TOSA_CHARGE_FULL_DIFF_VOLT){
#if CHECK_LOG
      printk("[NG] (%d->%d)\n",tosa_charge_full_main_batt,batt_now);
      printk(" * RESTART: time=%d \n",RCNR);
#endif
      tosa_charge_full_counter = RCNR;    // @ Reset Failsafe Counter.
    }
#if CHECK_LOG
    else{
      printk("[OK] (%d->%d)\n",tosa_charge_full_main_batt,batt_now);
    }
#endif
    tosa_charge_full_main_batt = batt_now;

#if CHECK_LOG
    printk(" JACKET VOLTAGE CHECK: ");
#endif
    if(sharpsl_jacket_batt_exist){
      batt_now = tosa_read_battery(SEL_JACKET);
      if((tosa_charge_full_jacket_batt - batt_now) >= TOSA_CHARGE_FULL_DIFF_VOLT){
	tosa_charge_full_counter = RCNR;    // @ Reset Failsafe Counter.
#if CHECK_LOG
	printk("[NG] (%d->%d)\n",tosa_charge_full_main_batt,batt_now);
	printk(" * RESTART: time=%d ***\n",RCNR);
#endif
      }
#if CHECK_LOG
      else{
	printk("[OK] (%d->%d)\n",tosa_charge_full_jacket_batt,batt_now);
      }
#endif
      tosa_charge_full_jacket_batt = batt_now;
    }
#if CHECK_LOG
    else{
      printk("[OK] (NON-JACKET)\n");
    }
#endif
  }
#endif //TOSA_OFF_CHARGE_FAILSAFE

#ifdef TOSA_OFF_CHARGE_FAILSAFE
#if CHECK_LOG
  if(tosa_charge_full_enable){
    printk(" CHG_FAILSAFE_COUNTER: time=%d\n",RCNR - tosa_charge_full_counter);
  }
#endif
#endif

#if CHECK_LOG
  printk("*** CHG_CHK_END: time=%d(%08x) ***\n",RCNR-gStartTime,RCNR);
  printk("\n");
#endif

  pass_charge_flag = 0;
  return ret;
}

void tosa_check_ac_and_jacket_state(void)
{
  int is_all_battery_full = 0;

  if(!pass_charge_flag){
    if(SHARPSL_AC_LINE_STATUS != APM_AC_OFFLINE){
      sharpsl_off_charge_battery();
      charge_status = 1;
      CHARGE_LED_ON();
    }else{
      charge_status = 0;
      CHARGE_LED_OFF();
    }
  }
  if(!tosa_off_check_jacket_req){
    return;
  }
  tosa_off_check_jacket_req = 0;
  // restart charge.
  CHARGE_ON();

  // check jacket.
  tosa_get_jacket_status_for_on();
  mdelay(100);

  tosa_off_main_batt_full = 0;
  tosa_off_jacket_batt_full = 0;
  charge_status = 0;

  if(SHARPSL_AC_LINE_STATUS != APM_AC_OFFLINE){
    // ac in.
    tosa_off_main_batt_full = SHARPSL_MAIN_BATT_FULL;
    if(sharpsl_jacket_exist && sharpsl_jacket_batt_exist){
      tosa_off_jacket_batt_full = SHARPSL_JACKET_BATT_FULL;
    }
    //sharpsl_off_charge_battery();
    charge_status = 1;
    CHARGE_LED_ON();
#if CHECK_LOG
    printk("*** CHG_START: time=0(%08x) ***\n",RCNR);
    gStartTime = RCNR;
    printk(" MAIN AD: %d JACKET AD %d\n",tosa_read_battery(SEL_MAIN),tosa_read_battery(SEL_JACKET));
#endif
  }else{
    // ac out.
    charge_status = 0;
    CHARGE_LED_OFF();
  }

  tosa_off_jc_card_limit();

  DPRINTK4("[BATT]sharpsl_jacket_exist = %d\n",sharpsl_jacket_exist);
  DPRINTK4("[BATT]sharpsl_jacket_batt_exist = %d\n",sharpsl_jacket_batt_exist);
  DPRINTK4("[BATT]charge_status = %d\n",charge_status);
  DPRINTK4("[BATT]main_batt_full = %d\n",tosa_off_main_batt_full);
  DPRINTK4("[BATT]jacket_batt_full = %d\n",tosa_off_jacket_batt_full);

  pass_charge_flag = 0;
  return;
}

void sharpsl_battery_charge_hook(int mode)
{
  switch (mode){
  case 0:	// Main Battery Full
    DPRINTK4("[IRQ]main battery full.\n");
    break;
  case 1:	// Charge Off
    DPRINTK4("[IRQ]ac out.\n");
    break;
  case 2:	// Charge On
    DPRINTK4("[IRQ]ac in.\n");
    break;
  case 3:	// Jacket Battery Full
    DPRINTK4("[IRQ]jacket battery full.\n");
    break;
  case 4:	// Jacket Detect
    DPRINTK4("[IRQ]check jacket.\n");
    break;
  case 5:
    DPRINTK4("[IRQ]timer.(%d)\n",RCNR);
    tosa_check_ac_and_jacket_state();
    tosa_off_check_jacket_req = 0;
    return;
    break;
  default:
    return;
    break;
  }
#ifdef TOSA_OFF_CHARGE_FAILSAFE
  if(tosa_charge_full_enable){
    tosa_charge_full_enable = 0;
  }
#endif //TOSA_OFF_CHARGE_FAILSAFE
  tosa_off_check_jacket_req = 1;
  return;
}

#endif	/* CONFIG_PM */


/*** Config & Setup **********************************************************/
void battery_init(void)
{
  int err;

  init_timer(&ac_kick_timer);
  ac_kick_timer.function = (void*)sharpsl_ac_kick_timer;

  init_timer(&jacket_kick_timer);
  jacket_kick_timer.function = (void*)tosa_jacket_kick_timer;

  GPDR(GPIO_AC_IN) &= ~GPIO_bit(GPIO_AC_IN);
  GPDR(GPIO_BAT0_CRG) &= ~GPIO_bit(GPIO_BAT0_CRG);
  GPDR(GPIO_BAT1_CRG) &= ~GPIO_bit(GPIO_BAT1_CRG);

  GPDR(GPIO_BAT0_LOW) &= ~GPIO_bit(GPIO_BAT0_LOW);
  GPDR(GPIO_BAT1_LOW) &= ~GPIO_bit(GPIO_BAT1_LOW);

  printk("SHARPSL_AC_LINE_STATUS=%x\n",SHARPSL_AC_LINE_STATUS);

  /* Set transition detect */
  set_GPIO_IRQ_edge( GPIO_AC_IN  , GPIO_BOTH_EDGES ); /* AC IN */
  set_GPIO_IRQ_edge( GPIO_JACKET_DETECT  , GPIO_BOTH_EDGES );
#ifndef TOSA_BATTERY_SKIP
  set_GPIO_IRQ_edge( GPIO_BAT0_LOW  , GPIO_FALLING_EDGE );
  set_GPIO_IRQ_edge( GPIO_BAT1_LOW  , GPIO_FALLING_EDGE );
#endif

  /* Register interrupt handler. */
  if ((err = request_irq(IRQ_GPIO_AC_IN, tosa_ac_interrupt,
			 SA_INTERRUPT, "ACIN", tosa_ac_interrupt))) {
    printk("Could not get irq %d.\n", IRQ_GPIO_AC_IN);
    return;
  }
  if ((err = request_irq(IRQ_GPIO_JACKET_DETECT, tosa_jacket_interrupt,
			 SA_INTERRUPT, "JACKET_DETECT", tosa_jacket_interrupt))) {
    printk("Could not get irq %d.\n", IRQ_GPIO_JACKET_DETECT);
    return;
  }

#ifndef TOSA_BATTERY_SKIP
  if ((err = request_irq(IRQ_GPIO_BAT0_LOW, tosa_main_batt_fatal_interrupt,
			 SA_INTERRUPT, "MAIN_FATAL", tosa_main_batt_fatal_interrupt))) {
    printk("Could not get irq %d.\n", IRQ_GPIO_BAT0_LOW);
    return;
  }
  disable_irq(IRQ_GPIO_BAT0_LOW);

  if ((err = request_irq(IRQ_GPIO_BAT1_LOW, tosa_jacket_batt_fatal_interrupt,
			 SA_INTERRUPT, "JACKET_FATAL", tosa_jacket_batt_fatal_interrupt))) {
    printk("Could not get irq %d.\n", IRQ_GPIO_BAT1_LOW);
    return;
  }
  disable_irq(IRQ_GPIO_BAT1_LOW);
#endif

  /* regist to Misc driver */
  misc_register(&battery_device);
  
  /* Make threads */
  kernel_thread(tosa_charge_on,      NULL, CLONE_FS | CLONE_FILES | CLONE_SIGHAND | SIGCHLD);
  kernel_thread(tosa_charge_off,     NULL, CLONE_FS | CLONE_FILES | CLONE_SIGHAND | SIGCHLD);
  kernel_thread(tosa_battery_thread, NULL, CLONE_FS | CLONE_FILES | CLONE_SIGHAND | SIGCHLD);
  kernel_thread(tosa_fatal_check,    NULL, CLONE_FS | CLONE_FILES | CLONE_SIGHAND | SIGCHLD);
  kernel_thread(tosa_jacket_check,   NULL, CLONE_FS | CLONE_FILES | CLONE_SIGHAND | SIGCHLD);

  tosa_get_jacket_status_for_on();
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
      free_irq(IRQ_GPIO_AC_IN, tosa_ac_interrupt);
      free_irq(IRQ_GPIO_JACKET_DETECT, tosa_jacket_interrupt);
#ifndef TOSA_BATTERY_SKIP
      free_irq(IRQ_GPIO_BAT0_LOW, tosa_main_batt_fatal_interrupt);
      free_irq(IRQ_GPIO_BAT1_LOW, tosa_jacket_batt_fatal_interrupt);
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
	  remove_proc_entry(sharpsl_battery_params[i].name, proc_batt);
	}
	remove_proc_entry("driver/battery", &proc_root);
	proc_batt = 0;
	free_irq(IRQ_GPIO_AC_IN, tosa_ac_interrupt);
	free_irq(IRQ_GPIO_JACKET_DETECT, tosa_jacket_interrupt);
#ifndef TOSA_BATTERY_SKIP
	free_irq(IRQ_GPIO_BAT0_LOW, tosa_main_batt_fatal_interrupt);
	free_irq(IRQ_GPIO_BAT1_LOW, tosa_jacket_batt_fatal_interrupt);
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
  free_irq(IRQ_GPIO_AC_IN, tosa_ac_interrupt);
  free_irq(IRQ_GPIO_JACKET_DETECT, tosa_jacket_interrupt);
#ifndef TOSA_BATTERY_SKIP
  free_irq(IRQ_GPIO_BAT0_LOW, tosa_main_batt_fatal_interrupt);
  free_irq(IRQ_GPIO_BAT1_LOW, tosa_jacket_batt_fatal_interrupt);
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

