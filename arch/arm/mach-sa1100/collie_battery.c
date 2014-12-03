/*
 * linux/arch/arm/mach-sa1100/collie_battery.c
 *
 * Battery routines for collie (SHARP)
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
 *	30-Jul-2002 Lineo Japan, Inc.  for 2.4.18
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
#include <linux/malloc.h>
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

#include <asm/ucb1200.h>
#include <asm/arch/tc35143.h>
#include <asm/sharp_char.h>

#include <asm/arch/keyboard_collie.h>
#include <asm/sharp_keycode.h>


//#define DEBUG	1
#ifdef DEBUG
#define DPRINTK(x, args...)  printk(__FUNCTION__ ": " x,##args)
#define DPRINTK2(x, args...)  printk(__FUNCTION__ ": " x,##args)
#else
#define DPRINTK(x, args...)   if ( msglevel > 1 )	printk(x,##args);
#define DPRINTK2(x, args...)  if ( msglevel > 0 )	printk(x,##args);
#endif




/*** prototype *********************************************************************/
int collie_read_MainBattery(void);
int collie_read_BackBattery(void);
int collie_read_Temp(void);
int collie_check_temp(void);
int collie_check_voltage(void);
static void collie_charge_on(void);
static void collie_charge_off(void);
int set_led_status(int which,int status);
int GetMainLevel( int Volt );
int GetBackLevel( int Volt );
int collie_get_main_battery(void);
unsigned short GetBackupBatteryAD(void);
int suspend_collie_read_Temp(void);

/*** extern ***********************************************************************/
extern u32 apm_wakeup_src_mask;
extern int counter_step_contrast;


/*** gloabal variables ************************************************************/
int charge_status = 0;			/* charge status  1 : charge  0: not charge */

typedef struct BatteryThresh {
	int high;
	int low;
	int verylow;
} BATTERY_THRESH;


#if defined(CONFIG_COLLIE_TS) || defined(CONFIG_COLLIE_TR0) || \
    defined(CONFIG_COLLIE_TR1) || defined(CONFIG_COLLIE_DEV)
BATTERY_THRESH  collie_main_battery_thresh_fl = {
  368, 358, 356
};

BATTERY_THRESH  collie_main_battery_thresh_nofl = {
  378, 364, 362
};
#else
BATTERY_THRESH  collie_main_battery_thresh_fl = {
  368, 358, 356
};

BATTERY_THRESH  collie_main_battery_thresh_nofl = {
  378, 365, 363
};
#endif



typedef struct ChargeThresh {
  int bar1;
  int bar2;
  int bar3;
  int bar4;
} CHARGE_THRESH;

CHARGE_THRESH collie_main_battery_thresh_charge = {
  391 , 403, 411 , 417
};

/*** local variables *************************************************************/
#if 1
static struct file_operations collie_battery_fops = {
};
#else
static struct file_operations collie_battery_fops = {
	owner:		THIS_MODULE,
	read:		do_read,
	poll:		do_poll,
	ioctl:		do_ioctl,
	open:		do_open,
	release:	do_release,
};
#endif

/*
 * The battery device is one of the misc char devices.
 * This is its minor number.
 */
#define	BATTERY_MINOR_DEV	215

static struct miscdevice battery_device = {
	BATTERY_MINOR_DEV,
	"battery",
	&collie_battery_fops
};

#define ADC_REQ_ID	(u32)(&battery_device)

#ifdef CONFIG_COLLIE_TR0
#define CHARGE_ON()        ucb1200_set_io(TC35143_GPIO_CHRG_ON, TC35143_IODAT_HIGH)
#define CHARGE_OFF()       ucb1200_set_io(TC35143_GPIO_CHRG_ON, TC35143_IODAT_LOW)
#else
#define CHARGE_ON()        SCP_REG_GPWR |= SCP_CHARGE_ON
#define CHARGE_OFF()       SCP_REG_GPWR &= ~SCP_CHARGE_ON
#endif

#define CHARGE_LED_ON()    set_led_status(SHARP_LED_CHARGER,LED_CHARGER_CHARGING)
#define CHARGE_LED_OFF()   set_led_status(SHARP_LED_CHARGER,LED_CHARGER_OFF)
#define CHARGE_LED_ERR()   set_led_status(SHARP_LED_CHARGER,LED_CHARGER_ERROR)

#define GetMainADCtoPower(x)   (( 330 * x * 2 ) / 1024 )     // MAX 4.2V
#define GetBackADCtoPower(x)   (( 330 * x * 2 ) / 1024 )     // MAX 3.3V
#define ConvRevise(x)          ( ( ad_revise * x ) / 652 )
#define MAIN_DIFF          50	// 0.5V 
#define DIFF_CNT           ( 3 - 1 )

#define COLLIE_BATTERY_STATUS_HIGH	APM_BATTERY_STATUS_HIGH
#define COLLIE_BATTERY_STATUS_LOW	APM_BATTERY_STATUS_LOW
#define COLLIE_BATTERY_STATUS_VERYLOW	APM_BATTERY_STATUS_VERY_LOW
#define COLLIE_BATTERY_STATUS_CRITICAL	APM_BATTERY_STATUS_CRITICAL

#define COLLIE_AC_LINE_STATUS	(!( GPLR & GPIO_AC_IN )	? APM_AC_OFFLINE : APM_AC_ONLINE)


#define COLLIE_PM_TICK         		( 1000 / 10 )                   // 1sec
#define COLLIE_APO_TICKTIME		(  5  * COLLIE_PM_TICK )	// 5sec
#define COLLIE_LPO_TICKTIME		COLLIE_APO_TICKTIME
#define COLLIE_APO_DEFAULT		( ( 3 * 60 ) * COLLIE_PM_TICK )	// 3 min
#define COLLIE_LPO_DEFAULT		(  20 * COLLIE_PM_TICK )	// 20 sec
#define COLLIE_MAIN_GOOD_COUNT		( 10*60 / ( COLLIE_APO_TICKTIME / COLLIE_PM_TICK ) )
#define COLLIE_MAIN_NOGOOD_COUNT	( 1*60  / ( COLLIE_APO_TICKTIME / COLLIE_PM_TICK ) )

#define COLLIE_BACKUP_BATTERY_CK_TIME	( 10*60*1*100 )		// 10min
#define COLLIE_BACKUP_BATTERY_LOW	( 190 )	


extern int autoPowerCancel;
extern int autoLightCancel;
extern int ppp_connect_count;
unsigned int APOCnt = COLLIE_APO_DEFAULT;
unsigned int APOCntWk = 0;
unsigned int LPOCnt = COLLIE_LPO_DEFAULT;
unsigned int LPOCntWk = 0;
static DECLARE_WAIT_QUEUE_HEAD(queue);
static int	msglevel;

int collie_backup_battery = COLLIE_BATTERY_STATUS_HIGH;
int collie_main_battery   = COLLIE_BATTERY_STATUS_HIGH;
int collie_main_charge_battery = 100;
int collie_ac_status = APM_AC_OFFLINE;
int ad_revise = 0;

static int MainCntWk = COLLIE_MAIN_GOOD_COUNT;
static int MainCnt   = COLLIE_MAIN_NOGOOD_COUNT;
static int FattCnt   = 0;
static int back_ac_status = 3;

#ifdef CONFIG_PM
static struct pm_dev *battery_pm_dev;	 /* registered PM device */
#endif

static int battery_off_flag = 0;	/* charge : suspend while get adc */
static int collie_charge_temp = 973;
static int collie_charge_volt = 465;	/* charge : check charge 3.0V     */
static int charge_off_mode = 0;		/* charge : check volt or non     */

static DECLARE_WAIT_QUEUE_HEAD(wq_on);
static DECLARE_WAIT_QUEUE_HEAD(wq_off);

static int collie_backup_battery_flag = 0;	// 0 : init    1: passed 10min
static int collie_fatal_off = 1;


/*** main ***********************************************************************/
// call by apm.c only
int collie_apm_get_power_status(u_char *ac_line_status,
                                u_char *battery_status,
                                u_char *battery_flag,
                                u_char *battery_percentage,
                                u_short *battery_life)
{

        // set ac status
	*ac_line_status = collie_ac_status;

	// set battery_status  main
	*battery_status = collie_main_battery;

	// main battery status to percentage
	switch (*battery_status)
	  {
	  case COLLIE_BATTERY_STATUS_HIGH:
	    *battery_percentage = 100;
	    break;
	  case COLLIE_BATTERY_STATUS_LOW:
	    *battery_percentage = 40;
	    break;
	  case COLLIE_BATTERY_STATUS_VERYLOW:
	    *battery_percentage = 5;
	    break;
	  case COLLIE_BATTERY_STATUS_CRITICAL:
	    *battery_percentage = 1;
	    break;
	  default:
	    *battery_percentage = 100;
	    break;
	  }

	if ( *ac_line_status == APM_AC_ONLINE )
	  *battery_percentage = 100;

	// good or ac in   --> GOOD_COUNT
	// low or very low --> NOGOOD_COUNT
	if ( ( *battery_status == COLLIE_BATTERY_STATUS_HIGH ) || ( *ac_line_status == APM_AC_ONLINE ) )
	  MainCnt = COLLIE_MAIN_GOOD_COUNT;
	else
	  MainCnt = COLLIE_MAIN_NOGOOD_COUNT;

	// set battery_flag
	if ( *battery_flag == COLLIE_BATTERY_STATUS_VERYLOW )
	  *battery_flag = (1 << 5);
	else
	  *battery_flag = (1 << *battery_status);

	// chk charge status
	if ( charge_status ) {
		*battery_status = APM_BATTERY_STATUS_CHARGING;
		*battery_flag = (1 << 3);
		*battery_percentage = collie_main_charge_battery;
		MainCnt = COLLIE_MAIN_NOGOOD_COUNT;
	}

	// set battery life
	*battery_life = APM_BATTERY_LIFE_UNKNOWN;



	DPRINTK("jiffies = %d / COLLIE_BACKUP_BATTERY_CK_TIME = %d / collie_backup_battery_flag = %d\n",
		       jiffies , COLLIE_BACKUP_BATTERY_CK_TIME , collie_backup_battery_flag);
	DPRINTK("MainCnt = %d\n",MainCnt);


	return APM_SUCCESS;
}


/*** misc ***********************************************************************/
static void collie_charge_on(void)
{
    while(1) {

      sleep_on(&wq_on);

      DPRINTK("call charge on \n");

      if ( collie_check_temp() ) {
        /* error led on */
	CHARGE_LED_ERR();
        /* charge status flag reset */
        charge_status = 0;
      } else {
        /* led on */
	CHARGE_LED_ON();
        /* Charge ON */
	CHARGE_ON();
	/* charge status flag set */
	charge_status = 1;
      }

    }
}

static void collie_charge_off(void)
{

    while(1) {

      sleep_on(&wq_off);

      DPRINTK("%d\n",charge_off_mode);

      /* Charge OFF */
      CHARGE_OFF();

      /* led off */
      CHARGE_LED_OFF();

      if ( charge_off_mode && collie_check_voltage() ) {
          /* error led on */
          CHARGE_LED_ERR();
      }

      /* charge status flag reset */
      charge_status = 0;

    }

}



static void collie_pm(void)
{

    while(1) {
      interruptible_sleep_on_timeout((wait_queue_head_t*)&queue, COLLIE_APO_TICKTIME );

      // get ac status. if change ac status , chk main battery.
      collie_ac_status =  COLLIE_AC_LINE_STATUS;
      if ( back_ac_status != collie_ac_status ) {
	 MainCntWk = COLLIE_MAIN_GOOD_COUNT;        // chk battery
      }
      back_ac_status = collie_ac_status;

      // get main battery
      collie_get_main_battery();

      // get backup battery status.
      if ( ( jiffies > COLLIE_BACKUP_BATTERY_CK_TIME ) && !collie_backup_battery_flag ) {
	collie_backup_battery_flag = 1;
	printk("check backup battery ! \n");
      }

      // chk critical
      if ( ( collie_main_battery == COLLIE_BATTERY_STATUS_CRITICAL ) && collie_fatal_off ) {
	if ( FattCnt++ >= COLLIE_MAIN_NOGOOD_COUNT ) {
	  collie_fatal_off = 0;
	  printk("Fatal Off\n");
	  handle_scancode(SLKEY_OFF|KBDOWN , 1);
	  mdelay(500);
	  handle_scancode(SLKEY_OFF|KBUP   , 0);
	}
      } else {
	FattCnt = 0;
      }

      // auto power off chk
      if ( !autoPowerCancel || ppp_connect_count > 0 ) {
	APOCntWk = 0;
	autoPowerCancel = 1;
      } else {
	APOCntWk += COLLIE_APO_TICKTIME;
      }

      if ( APOCntWk >= APOCnt ) {
	//	printk("Auto Power Off\n");
	//	handle_scancode(SLKEY_OFF|KBDOWN , 1);
	//	mdelay(500);
	//	handle_scancode(SLKEY_OFF|KBUP   , 0);
	APOCntWk = 0;
      }
      //      printk("Auto Power Off %d\n",APOCntWk);


      // auto frontlight off
      if ( !autoLightCancel ) {
	LPOCntWk = 0;
	autoLightCancel = 1;
      } else {
	LPOCntWk += COLLIE_LPO_TICKTIME;
      }

      if ( LPOCntWk >= LPOCnt ) {
	//	printk("Auto Light Off\n");
	//	colliefl_blank(1);
	LPOCntWk = 0;
      }
      //      printk("Auto Light Off %d\n",LPOCntWk);


    }

}


/*** Int ***********************************************************************/
static void Collie_ac_interrupt(int irq, void *dummy, struct pt_regs *fp)
{
  if ((GPLR & GPIO_AC_IN)==0) {
    /* High->Low  : desert */
    charge_off_mode = 0;
    wake_up(&wq_off);
  } else {
    /* Low->High  : assert */
    wake_up(&wq_on);
  }

}

static void Collie_co_interrupt(int irq, void *dummy, struct pt_regs *fp)
{
  if ((GPLR & GPIO_AC_IN)==0) {
    /* High->Low  : desert */
    charge_off_mode = 0;
    wake_up(&wq_off);
  } else {
    charge_off_mode = 1;
    wake_up(&wq_off);
  }
}



/*** get adc *********************************************************************/
int collie_get_main_battery(void)
{
#ifdef CONFIG_COLLIE_TR0
	return 	COLLIE_BATTERY_STATUS_HIGH;
#else
	int i = 0;

	MainCntWk++;

	if ( MainCntWk > MainCnt ) {
	  int voltage;

	  MainCntWk = 0;

	  while(1) {
	    voltage = collie_read_MainBattery();
	    if ( voltage > 0 ) break;
	    if ( i++ > 5 ) { voltage = 380; break; }
	  }

	  collie_main_battery = GetMainLevel(GetMainADCtoPower(voltage));
	  collie_main_charge_battery = GetMainChargePercent(GetMainADCtoPower(voltage));

	  DPRINTK2("charge percent = %d ( at %d ) \n",collie_main_charge_battery,jiffies);

	  DPRINTK(" get Main battery status %d\n",collie_main_battery);

	} else {
	  DPRINTK(" not get Main battery \n");
	  DPRINTK("MainCntWk = %d\n",MainCntWk);
	}
	return collie_main_battery;

#endif
}

int GetMainChargePercent( int Volt )
{
  DPRINTK("  volt = %d  \n",Volt);

  if ( Volt >= collie_main_battery_thresh_charge.bar4 ) {
    return 95;
  } else if ( Volt >= collie_main_battery_thresh_charge.bar3 ) {
    return 75;
  } else if ( Volt >= collie_main_battery_thresh_charge.bar2 ) {
    return 50;
  } else if ( Volt >= collie_main_battery_thresh_charge.bar1 ) {
    return 25;
  } else {
    return 5;
  }
}

int GetMainLevel( int Volt )
{

  DPRINTK("  volt = %d  \n",Volt);


  if ( counter_step_contrast ) {
	if ( Volt > collie_main_battery_thresh_fl.high )
		return COLLIE_BATTERY_STATUS_HIGH;
	else if ( Volt > collie_main_battery_thresh_fl.low )
		return COLLIE_BATTERY_STATUS_LOW;
	else if ( Volt > collie_main_battery_thresh_fl.verylow )
		return COLLIE_BATTERY_STATUS_VERYLOW;
	else
		return COLLIE_BATTERY_STATUS_CRITICAL;
  } else {
	if ( Volt > collie_main_battery_thresh_nofl.high )
		return COLLIE_BATTERY_STATUS_HIGH;
	else if ( Volt > collie_main_battery_thresh_nofl.low )
		return COLLIE_BATTERY_STATUS_LOW;
	else if ( Volt > collie_main_battery_thresh_nofl.verylow )
		return COLLIE_BATTERY_STATUS_VERYLOW;
	else
		return COLLIE_BATTERY_STATUS_CRITICAL;
  }

}


int GetBackLevel( int Volt )
{
  if ( Volt > COLLIE_BACKUP_BATTERY_LOW )
    return COLLIE_BATTERY_STATUS_HIGH;
  else
    return COLLIE_BATTERY_STATUS_VERYLOW;
}


int collie_read_MainBattery(void)
{
	int voltage;


  	ucb1200_set_io(TC35143_GPIO_BBAT_ON, TC35143_IODAT_LOW);
  	ucb1200_set_io(TC35143_GPIO_MBAT_ON, TC35143_IODAT_HIGH);
	voltage = ucb1200_get_adc_value(ADC_REQ_ID, TC35143_ADC_BAT_VOL);
	if ( battery_off_flag )
	  voltage = -1;
  	ucb1200_set_io(TC35143_GPIO_MBAT_ON, TC35143_IODAT_LOW);

	battery_off_flag = 0;

	DPRINTK("adc = %d\n",voltage);

	if ( voltage != -1 ) {
	  DPRINTK2("main adc = %d(%d)\n",(voltage+ConvRevise(voltage)),voltage);
	  return (voltage+ConvRevise(voltage));
	} else {
	  return voltage;
	}
}

int collie_read_BackBattery(void)
{
	int voltage;

  	ucb1200_set_io(TC35143_GPIO_MBAT_ON, TC35143_IODAT_LOW);
  	ucb1200_set_io(TC35143_GPIO_BBAT_ON, TC35143_IODAT_HIGH);
	mdelay(3);
	voltage = ucb1200_get_adc_value(ADC_REQ_ID, TC35143_ADC_BAT_VOL);
	if ( battery_off_flag )
	  voltage = -1;
  	ucb1200_set_io(TC35143_GPIO_BBAT_ON, TC35143_IODAT_LOW);

	battery_off_flag = 0;
	return voltage;
}

int collie_read_Temp(void)
{
	int voltage;


  	ucb1200_set_io(TC35143_GPIO_TMP_ON, TC35143_IODAT_HIGH);
	voltage = ucb1200_get_adc_value(ADC_REQ_ID, TC35143_ADC_BAT_TMP);
	if ( battery_off_flag )
	  voltage = -1;
  	ucb1200_set_io(TC35143_GPIO_TMP_ON, TC35143_IODAT_LOW);

	battery_off_flag = 0;
	return voltage;
}


int collie_check_temp(void)
{
  int temp , i = 0;

  if ( in_interrupt() ) {
    DPRINTK("charge(temp) : in_interrupt !\n");
    return 1;
  }

  while(1) {
    temp = collie_read_Temp();
    if ( temp > 0 ) break;
    if ( i > 5 ) { DPRINTK("charge : can not temp ad.\n"); break; }
    i++;
  }

  DPRINTK("collie_check_temp = %d\n",temp);

  if ( temp > collie_charge_temp ) return 1;


  return 0;
}

int collie_check_voltage(void)
{
  int temp , i = 0;


  if ( in_interrupt() ) {
    DPRINTK("charge(vol) : in_interrupt !\n");
    return 1;
  }

  while(1) {
    temp = collie_read_MainBattery();
    if ( temp > 0 ) break;
    if ( i > 5 ) { DPRINTK("charge : can not main ad.\n"); break; }
    i++;
  }

  DPRINTK("collie_check_volt = %d\n",temp);

  if ( temp < collie_charge_volt ) return 1;

  return 0;
}

/*** PM *************************************************************************/
void Collie_battery_power_off(void)
{
  //  	ucb1200_set_io(TC35143_GPIO_MBAT_ON, TC35143_IODAT_LOW);
  //  	ucb1200_set_io(TC35143_GPIO_BBAT_ON, TC35143_IODAT_LOW);
  //  	ucb1200_set_io(TC35143_GPIO_TMP_ON,  TC35143_IODAT_LOW);
}

void Collie_battery_power_on(void)
{
  /* 35143F io port initialize */
#ifdef CONFIG_COLLIE_TR0
  ucb1200_set_io_direction(TC35143_GPIO_CHRG_ON, TC35143_IODIR_OUTPUT);
#endif
  ucb1200_set_io_direction(TC35143_GPIO_MBAT_ON, TC35143_IODIR_OUTPUT);
  ucb1200_set_io_direction(TC35143_GPIO_TMP_ON,  TC35143_IODIR_OUTPUT);
  ucb1200_set_io_direction(TC35143_GPIO_BBAT_ON, TC35143_IODIR_OUTPUT);

  if ( collie_backup_battery_flag )
    collie_backup_battery = GetBackLevel(GetBackADCtoPower(GetBackupBatteryAD()));

}


#ifdef CONFIG_PM
static int Collie_battery_pm_callback(struct pm_dev *pm_dev, pm_request_t req, void *data)
{
	switch (req) {
	case PM_SUSPEND:
	  battery_off_flag = 1;
	  Collie_battery_power_off();
	  ucb1200_cancel_get_adc_value(ADC_REQ_ID);
	  apm_wakeup_src_mask |= ( GPIO_AC_IN | GPIO_CO );
	  break;
	case PM_RESUME:
	  collie_fatal_off = 1;
	  FattCnt = 0;
	  APOCntWk = 0;
	  LPOCntWk = 0;
	  ucb1200_cancel_get_adc_value(ADC_REQ_ID);
	  Collie_battery_power_on();
	  MainCntWk = COLLIE_MAIN_GOOD_COUNT + 1;
	  break;
	}
	return 0;
}
#endif

/*** for suspend hook ***********************************************************/
int suspend_collie_read_Temp(void)
{
  int temp;

  ucb1200_set_io(TC35143_GPIO_TMP_ON, TC35143_IODAT_HIGH);
  udelay(700);
  temp = suspend_read_adc((int)&temp, TC35143_ADC_BAT_TMP);
  ucb1200_set_io(TC35143_GPIO_TMP_ON, TC35143_IODAT_LOW);

  printk("suspend temp = %d\n",temp);


  return temp;
}


unsigned short suspend_collie_read_MainBattery(void)
{
  int temp;

  ucb1200_set_io(TC35143_GPIO_MBAT_ON, TC35143_IODAT_HIGH);
  udelay(700);
  temp = suspend_read_adc((int)&temp, TC35143_ADC_BAT_VOL);
  ucb1200_set_io(TC35143_GPIO_MBAT_ON, TC35143_IODAT_LOW);

  printk("suspend main adc = %d(%d)\n",(temp+ConvRevise(temp)),temp);

  return (temp+ConvRevise(temp));
}


unsigned short GetBackupBatteryAD(void)
{
  int temp;

  ucb1200_set_io(TC35143_GPIO_BBAT_ON, TC35143_IODAT_HIGH);
  //  mdelay(3);
  mdelay(5);
  temp = suspend_read_adc((int)&temp, TC35143_ADC_BAT_VOL);
  ucb1200_set_io(TC35143_GPIO_BBAT_ON, TC35143_IODAT_LOW);

  printk("suspend backup adc = %d(%d)\n",(temp+ConvRevise(temp)),temp);

  return temp;
}


unsigned short chkFatalBatt(void)
{
#ifdef CONFIG_COLLIE_TR0
  return 1;
#else
  int volt;

  // fail safe
  collie_get_ad();

  if ( charge_status == 1 ) {
      CHARGE_OFF();
      udelay(100);
  }
 
  volt = GetMainADCtoPower(suspend_collie_read_MainBattery());
  printk("fatal chk = %d\n",volt);


  if ( charge_status == 1 ) {
    udelay(100);
    CHARGE_ON();
    GEDR = GPIO_CO;
    //    printk("CO = %x\n",GEDR&GPIO_CO);
  }


  if ( volt < collie_main_battery_thresh_nofl.verylow )
    return 0;
  else
    return 1;
#endif
}




int suspend_collie_check_temp(void)
{
  unsigned short temp , i = 0;

  while(1) {
    temp = suspend_collie_read_Temp();
    if ( temp > 0 ) break;
    if ( i > 5 ) { DPRINTK("suspend charge : can not read temp ad.\n"); break; }
    i++;
  }

  DPRINTK("%d\n",temp);

  if ( temp > collie_charge_temp ) return 1;

  return 0;
}



int suspend_collie_check_voltage(void)
{
  int temp , i = 0;

  while(1) {
    temp = suspend_collie_read_MainBattery();
    if ( temp > 0 ) break;
    if ( i > 5 ) { DPRINTK("can not read main ad.\n"); break; }
    i++;
  }

  DPRINTK("%d\n",temp);

  if ( temp < collie_charge_volt ) return 1;

  return 0;
}



void suspend_collie_charge_on(void)
{

  DPRINTK("call suspend charge on \n");

  if ( suspend_collie_check_temp() ) {
    /* error led on */
    CHARGE_LED_ERR();
    /* charge status flag reset */
    charge_status = 0;
  } else {
    /* led on */
    CHARGE_LED_ON();
    /* Charge ON */
    CHARGE_ON();
    /* charge status flag set */
    charge_status = 1;
  }
}


void suspend_collie_charge_off(int charge_off_mode)
{

  DPRINTK("call suspend charge off \n");


  /* Charge OFF */
  CHARGE_OFF();

  /* led off  FIX ME !*/
  CHARGE_LED_OFF();

  if ( charge_off_mode && suspend_collie_check_voltage()) {
    /* error led on */
    CHARGE_LED_ERR();
  }

  /* charge status flag reset */
  charge_status = 0;
}


void collie_battery_charge_hook(int mode)
{
  switch (mode){
  case 0:
    if ( charge_status )
      suspend_collie_charge_off(1);
    break;

  case 1:
    if ( charge_status )
      suspend_collie_charge_off(0);
    break;

  case 2:
    if ( !charge_status )
      suspend_collie_charge_on();
    break;

  default:
    break;
  }
}



/*** Config & Setup **********************************************************/
void collie_get_ad()
{
  if ( FLASH_DATA(FLASH_AD_MAGIC_ADR) == FLASH_AD_MAJIC ) {
   ad_revise = 652-FLASH_DATA(FLASH_AD_DATA_ADR);
  } else {
   ad_revise = 0;
  }
  DPRINTK2("ad revise = %d(%d)\n",ad_revise,FLASH_DATA(FLASH_AD_DATA_ADR));
}


void battery_init(void)
{
  int err;

  /* get ad revise */
  collie_get_ad();

  /* 35143F io port initialize */
#ifdef CONFIG_COLLIE_TR0
  ucb1200_set_io_direction(TC35143_GPIO_CHRG_ON, TC35143_IODIR_OUTPUT);
#endif
  ucb1200_set_io_direction(TC35143_GPIO_MBAT_ON, TC35143_IODIR_OUTPUT);
  ucb1200_set_io_direction(TC35143_GPIO_TMP_ON,  TC35143_IODIR_OUTPUT);
  ucb1200_set_io_direction(TC35143_GPIO_BBAT_ON, TC35143_IODIR_OUTPUT);

  /* Set transition detect */
  set_GPIO_IRQ_edge( GPIO_AC_IN  , GPIO_RISING_EDGE );
  set_GPIO_IRQ_edge( GPIO_CO     , GPIO_RISING_EDGE );

  /* Register interrupt handler. */
  if ((err = request_irq(IRQ_GPIO_AC_IN, Collie_ac_interrupt, SA_INTERRUPT,
			 "ACIN", Collie_ac_interrupt))) {
    printk("Could not get irq %d.\n", IRQ_GPIO_AC_IN);
    return;
  }

  /* Register interrupt handler. */
  if ((err = request_irq(IRQ_GPIO_CO, Collie_co_interrupt, SA_INTERRUPT,
			 "CO", Collie_co_interrupt))) {
    free_irq(IRQ_GPIO_AC_IN, Collie_ac_interrupt);
    printk("Could not get irq %d.\n", IRQ_GPIO_CO);
    return;
  }

  /* regist to Misc driver */
  misc_register(&battery_device);

  /* Make threads */
  kernel_thread(collie_charge_on,  NULL, CLONE_FS | CLONE_FILES | CLONE_SIGHAND | SIGCHLD);
  kernel_thread(collie_charge_off, NULL, CLONE_FS | CLONE_FILES | CLONE_SIGHAND | SIGCHLD);
  kernel_thread(collie_pm,         NULL, CLONE_FS | CLONE_FILES | CLONE_SIGHAND | SIGCHLD);

  /* set backup work */
  collie_backup_battery = COLLIE_BATTERY_STATUS_HIGH;

}




#ifdef CONFIG_PROC_FS
struct proc_dir_entry *proc_batt;

typedef struct collie_battery_entry {
	int*		addr;
	int		def_value;
	char*		name;
	char*		description;
	unsigned short	low_ino;
} collie_battery_entry_t;

static collie_battery_entry_t collie_battery_params[] = {
/*  { addr,	def_value,	name,	    description }*/
  { &msglevel,	0,		"msglevel",    "debug message output level" }
};
#define NUM_OF_BATTERY_ENTRY	(sizeof(collie_battery_params)/sizeof(collie_battery_entry_t))

static ssize_t collie_battery_read_params(struct file *file, char *buf,
				      size_t nbytes, loff_t *ppos)
{
	int i_ino = (file->f_dentry->d_inode)->i_ino;
	char outputbuf[15];
	int count;
	int i;
	collie_battery_entry_t	*current_param = NULL;

	if (*ppos>0) /* Assume reading completed in previous read*/
		return 0;
	for (i=0; i<NUM_OF_BATTERY_ENTRY; i++) {
		if (collie_battery_params[i].low_ino==i_ino) {
			current_param = &collie_battery_params[i];
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

static ssize_t collie_battery_write_params(struct file *file, const char *buf,
				       size_t nbytes, loff_t *ppos)
{
	int			i_ino = (file->f_dentry->d_inode)->i_ino;
	collie_battery_entry_t	*current_param = NULL;
	int			i;
	unsigned long		param;
	char			*endp;

	for (i=0; i<NUM_OF_BATTERY_ENTRY; i++) {
		if(collie_battery_params[i].low_ino==i_ino) {
			current_param = &collie_battery_params[i];
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

static struct file_operations proc_params_operations = {
	read:	collie_battery_read_params,
	write:	collie_battery_write_params,
};
#endif




int __init Collie_battery_init(void)
{
#ifdef CONFIG_PM
        battery_pm_dev = pm_register(PM_SYS_DEV, 0, Collie_battery_pm_callback);
#endif

	battery_init();

#ifdef CONFIG_PROC_FS
	{
	int	i;
	struct proc_dir_entry *entry;

	proc_batt = proc_mkdir("driver/battery", NULL);
	if (proc_batt == NULL) {
	  ucb1200_cancel_get_adc_value(ADC_REQ_ID);
	  free_irq(IRQ_GPIO_AC_IN, Collie_ac_interrupt);
	  free_irq(IRQ_GPIO_CO, Collie_co_interrupt);

	  misc_deregister(&battery_device);
	  printk(KERN_ERR "can't create /proc/driver/battery\n");
	  return -ENOMEM;
	}
	for (i=0; i<NUM_OF_BATTERY_ENTRY; i++) {
		entry = create_proc_entry(collie_battery_params[i].name,
					  S_IWUSR |S_IRUSR | S_IRGRP | S_IROTH,
					  proc_batt);
		if (entry) {
			collie_battery_params[i].low_ino = entry->low_ino;
			entry->proc_fops = &proc_params_operations;
		} else {
			int	j;
			for (j=0; j<i; j++) {
				remove_proc_entry(collie_battery_params[i].name,
						  proc_batt);
			}
			remove_proc_entry("driver/battery", &proc_root);
			proc_batt = 0;
			ucb1200_cancel_get_adc_value(ADC_REQ_ID);
			free_irq(IRQ_GPIO_AC_IN, Collie_ac_interrupt);
			free_irq(IRQ_GPIO_CO, Collie_co_interrupt);
			misc_deregister(&battery_device);
			printk(KERN_ERR "ts: can't create /proc/driver/ts/threshold\n");
			return -ENOMEM;
		}
	}
	}
#endif


	return 0;
}

module_init(Collie_battery_init);



#ifdef MODULE
int init_module(void)
{
	Collie_battery_init();
	return 0;
}

void cleanup_module(void)
{
	ucb1200_cancel_get_adc_value(ADC_REQ_ID);
	free_irq(IRQ_GPIO_AC_IN, Collie_ac_interrupt);
	free_irq(IRQ_GPIO_CO, Collie_co_interrupt);

#ifdef CONFIG_PROC_FS
	{
	int	i;
	for (i=0; i<NUM_OF_COLLIE_BATTERY_ENTRY; i++) {
		remove_proc_entry(collie_battery_params[i].name, proc_batt);
	}
	remove_proc_entry("driver/battery", NULL);
	proc_batt = 0;
	}
#endif

	misc_deregister(&battery_device);
}
#endif /* MODULE */
