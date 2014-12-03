/*
 * LinkUp 7200 Power Management Routines
 *
 * linux/arch/arm/mach-l7200/power.c
 * 
 * (C) Copyright 2001 Lineo Japan, Inc.
 *
 * Based on:
 *   arch/arm/mach-sa1100/power.c
 *   Copyright (c) 2001 Cliff Brake <cbrake@accelent.com>
 *
 *   This program is free software; you can redistribute it and/or 
 *   modify it under the terms of the GNU General Public License.
 *
 * ChangeLog:
 *   04-02-2001 Lineo Japan, Inc.
 */

/*
 *  Debug macros 
 */
//#define DEBUG 1
#ifdef DEBUG
#  define DPRINTK(fmt, args...)	printk("%s: " fmt, __FUNCTION__ , ## args)
#else
#  define DPRINTK(fmt, args...)
#endif

#include <linux/config.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/pm.h>
#include <linux/malloc.h>
#include <linux/sysctl.h>
#include <linux/acpi.h>

#include <asm/mach-types.h>
#include <asm/hardware.h>
#include <asm/memory.h>
#include <asm/system.h>
#include <asm/proc-fns.h>
#include <asm/proc/cache.h>
#ifdef CONFIG_IRIS
#include <asm/irq.h>
#include <asm/arch/irqs.h>
#include <asm/arch/fpga_iris.h>
#endif
#include <asm/arch/power.h>

#include <asm/delay.h>

#if 0
extern unsigned long *sleep_param;	/* virtual address */
extern unsigned long  sleep_param_p;	/* physical address */

extern void cpu_l7200_resume(void);
extern int  cpu_l7200_do_suspend(void);
#endif


#ifdef CONFIG_IRIS

#include <asm/arch/dmasound_iris.h> /* for IO_SICR0 */
#include <asm/arch/iris_apm.h>

#undef USBPWR_INVERTED_ON_TARGET /* define , if use on Target */
#undef USE_INT0_INTERRUPT

#define NOT_COPY_XTIME_TO_RTCDR_ON_ENTERING_SUSPEND

#define MMCPOWER_BIT    (bitPE1)
#define	TABLET_CS_BIT	(bitPE3)
#define THERM_STB_BIT	(bitPE4)

//#define WAKEUP_SRC	(bitWSRC1 | bitWSRC2 | bitWSRC3 | bitWSRC4 | bitWSRC7)
/* ignore BAT-Cover on SUSPEND state */
#define WAKEUP_SRC_SUSPEND	(bitWSRC1 | bitWSRC2 | bitWSRC3 | bitWSRC4 | bitWSRC7 | bitWSRC8 )
#define WAKEUP_SRC_STANDBY	(bitWSRC1 | bitWSRC2 | bitWSRC3 | bitWSRC4 | bitWSRC7 | bitWSRC8 )
#define WAKEUP_SRC_MASKIGNORE_SUSPEND   (bitWSRC1 | bitWSRC2 | bitWSRC7 | bitWSRC8)
#define WAKEUP_SRC_MASKIGNORE_STANDBY   (bitWSRC1 | bitWSRC2 | bitWSRC7 | bitWSRC8)
/* .... 0:PowerOnReset , 1:Ring , 2:ExtPower , 3:UART2 , 4:USB , 5:Card , 6:Touch , 7:Key , 8:BatCover , 9:FullCharge */

#define GPO_FORCE_DISABLE_BITS ( ( 1 << 0 ) |( 1 << 1 ) )


static struct {
	unsigned int fpga_een;
	unsigned int fpga_esel;
	unsigned int fpga_de;
	unsigned int fpga_lsel;
	unsigned int fpga_enable;
	unsigned int irqhenable;
	unsigned int irqenable;
	unsigned int fiqenable;

  unsigned int kbdmr;
  unsigned int kbksr;
  unsigned int kbdr;

} save_regs;

u32 apm_wakeup_src_mask = 0xffffffff; /* ALL factors enabled ... */
u32 apm_wakeup_factor = 0;

#define THIS_IS_SUSPEND_MODE  1
#define THIS_IS_STANDBY_MODE  2
#define THIS_IS_OTHER_MODE    0   /* why ? */


#define OFFCHARGE_PROC_ON_POWER_C

extern int iris_read_battery_mV(void);
extern int iris_read_battery_temp(void);
extern int gsm_modem_status;


#ifdef OFFCHARGE_PROC_ON_POWER_C

#include <asm/arch/gpio.h>

#define IRIS_OFF_CHARGER_FLOW_VERSION   2

/* timer1 values... these are same as those on l72xx_buzzer.c */
#define BUZZER_MODE_BZTOG        0x000 /* bit 9 */
#define BUZZER_MODE_UNDERFLOW    0x200 /* bit 9 */
#define BUZZER_TOGGLE            0x100 /* bit 8 */
#define TIMER_PSCE_STBUZ         0x400 /* bit 10 */
#define TIMER_PSCE_TIMER1        0x000 /* bit 10 */
#define TIMER_PERIODIC           0x040 /* bit 6 */
#define TIMER_ENABLE             0x080 /* bit 7 */
#define PRESCALE_div1            0x000 /* bit 2,3 */
#define PRESCALE_div16           0x002 /* bit 2,3 */
#define PRESCALE_div256          0x004 /* bit 2,3 */
#define PRESCALE_BITS            0x006 /* bit 2,3 */

#define OFFCHARGE_ERROR     -1
#define NOT_CHARGING         0
#define OFFCHARGE_PHASE1     1
#define OFFCHARGE_PHASE1_5   2
#define OFFCHARGE_PHASE2     3
#define OFFCHARGE_PHASE2_1   4
#define OFFCHARGE_PHASE2_5   5
#define OFFCHARGE_PHASE3     6
#define OFFCHARGE_PHASE3_5   7
#define OFFCHARGE_PHASE4     8
#define OFFCHARGE_BATCHECKPOLLING 9  /* Not Charging. BatteryCheck Only */

#if 0 /* debug ver. */
#define OFFCHARGE_PHASE1_WAIT     15       /* secs */
#define OFFCHARGE_PHASE2_TIMEOUT  (60*3) /* secs */
#define OFFCHARGE_PHASE3_WAIT     (60)  /* secs */
#define OFFCHARGE_PRECHARGE_TIMEOUT 30    /* secs */
#define OFFCHARGE_DISCHARGE_TIMEOUT 30    /* secs */
#define STANDBYMODE_BATTERY_POLLING (15) /* secs */
#define STANDBYMODE_BATTERY_POLLING_UNDER_LO (5) /* secs */
#else
#define OFFCHARGE_PHASE1_WAIT     60       /* secs */
#define OFFCHARGE_PHASE2_TIMEOUT  (3600*5) /* secs */
#define OFFCHARGE_PHASE3_WAIT     (60*20)  /* secs */
#define OFFCHARGE_PRECHARGE_TIMEOUT 60    /* secs */
#define OFFCHARGE_DISCHARGE_TIMEOUT 30    /* secs */
#define STANDBYMODE_BATTERY_POLLING (60*10) /* secs */
#define STANDBYMODE_BATTERY_POLLING_UNDER_LO (60) /* secs */
#endif


#define NO_RTCIRQ            0
#define RTCIRQ_IS_OFFCHARGE  1
#define RTCIRQ_IS_DRIVER     2

static int off_charging = NOT_CHARGING;
static int rtcirq_setting = NO_RTCIRQ;
static int offcharge_errno = 0;
static unsigned long save_rtcmr = 0;
static unsigned long save_rtccr = 0;
static int fullfill_irq_enable = 0;
static int save_pbdr_charger = 0;

static int save_gpio_blink = 0;
static int save_gpio_blink_sbsr = 0;
static int save_gpio_green = 0;
static int save_gpio_red   = 0;

static int interrupt_count = 0;

#define FULLFILL_INTERRUPT_MAXCOUNT  3

#define RTCCR_ENALARM 0x1

#define SECS_MERGIN 10 /* secs */
#define CHARGER_START  (bitPB5)
#define AC_INPUT       (bitWSRC2)

#define LED_HARD_BLINK_CTRL (bitPE0)
#define PHONE_COLOR_RED    (bitPB3)
#define PHONE_COLOR_GREEN  (bitPB2)

static void save_rtcregs(void)
{
  save_rtcmr = IO_RTCMR;
  save_rtccr = IO_RTCCR;
  rtcirq_setting = RTCIRQ_IS_DRIVER;
  save_pbdr_charger = ( GET_PBDR(CHARGER_START) ? 1 : 0 );

  save_gpio_blink = ( GET_PEDR(LED_HARD_BLINK_CTRL) ? 1 : 0 );
  save_gpio_green = ( GET_PBDR(PHONE_COLOR_GREEN) ? 1 : 0 );
  save_gpio_red   = ( GET_PBDR(PHONE_COLOR_RED) ? 1 : 0 );

  if( save_gpio_blink ){
    SET_PESBSR_HI(LED_HARD_BLINK_CTRL);
  }else{
    SET_PESBSR_LO(LED_HARD_BLINK_CTRL);
  }
}

static void restore_rtcregs(void)
{
  if( rtcirq_setting == RTCIRQ_IS_OFFCHARGE ){
    unsigned long tmpval;
    tmpval = IO_RTCS; /* clear interrupt */
    IO_RTCCR = 0;
    IO_RTCMR = save_rtcmr;
    IO_RTCCR = save_rtccr;
  }
  if( save_pbdr_charger ){
    SET_PBDR_HI(CHARGER_START);
  }else{
    SET_PBDR_LO(CHARGER_START);
  }

  if( save_gpio_blink ){
    SET_PEDR_HI(LED_HARD_BLINK_CTRL);
  }else{
    SET_PEDR_LO(LED_HARD_BLINK_CTRL);
  }

  if( save_gpio_green ){
    SET_PBDR_HI(PHONE_COLOR_GREEN);
  }else{
    SET_PBDR_LO(PHONE_COLOR_GREEN);
  }

  if( save_gpio_red ){
    SET_PBDR_HI(PHONE_COLOR_RED);
  }else{
    SET_PBDR_LO(PHONE_COLOR_RED);
  }

}


#if IRIS_CHARGER_FLOW_VERSION > 1 /* ver2 or later */
#define CHARGER_IC_BAD_VALUE      4400  /* mV */
#else
#define CHARGER_IC_BAD_VALUE      4500  /* mV */
#endif
#define FULLFILLED_VOLTAGE        3900  /* mV */
#define RE_CHARGE_VOLTAGE         3700  /* mV */
#define CHARGER_BATTERY_BAD_VALUE 2500  /* mV */

static void set_rtcmr(unsigned long sec)
{
  unsigned long curval = IO_RTCDR;
  unsigned long diff = 0;
  unsigned long sec2 = sec;
  if( ! (save_rtccr & RTCCR_ENALARM) ){
    IO_RTCMR = curval + sec;
    rtcirq_setting = RTCIRQ_IS_OFFCHARGE;
    return;
  }
  if( save_rtcmr > curval ){
    diff = save_rtcmr - curval;
  }else{
    diff = 0xffffffff - curval + save_rtcmr;
  }
  sec2 = sec + SECS_MERGIN;
  if( sec2 < diff ){
    IO_RTCMR = curval + sec;
    rtcirq_setting = RTCIRQ_IS_OFFCHARGE;
  }else{
    IO_RTCMR = save_rtcmr;
    IO_RTCCR = save_rtccr;
    rtcirq_setting = RTCIRQ_IS_DRIVER;
  }
}

static void set_rtcmr_driver(void)
{
  IO_RTCMR = save_rtcmr;
  IO_RTCCR = save_rtccr;
  rtcirq_setting = RTCIRQ_IS_DRIVER;
}

static void offcharger_on(void)
{
  SET_PB_OUT(CHARGER_START);
  SET_PBDR_HI(CHARGER_START);
  SET_PBSBSR_HI(CHARGER_START);
}

static void offcharger_off(void)
{
  SET_PB_OUT(CHARGER_START);
  SET_PBDR_LO(CHARGER_START);
  SET_PBSBSR_LO(CHARGER_START);
}

#include <asm/sharp_char.h>

static int irisled_internal_logical[(SHARP_LED_WHICH_MAX+1)];

static void show_offcharge_fullcharge(void)
{
  if( save_gpio_green || save_gpio_red ){
    SET_PBDR_HI(PHONE_COLOR_GREEN);
    SET_PBSBSR_HI(PHONE_COLOR_GREEN);
    SET_PBDR_LO(PHONE_COLOR_RED);
    SET_PBSBSR_LO(PHONE_COLOR_RED);
  }else{
    SET_PBDR_LO(PHONE_COLOR_RED|PHONE_COLOR_GREEN);
    SET_PBSBSR_LO(PHONE_COLOR_RED|PHONE_COLOR_GREEN);
  }
}


static void show_offcharge_noncharge(void)
{
  if( save_gpio_green ){
    SET_PBDR_HI(PHONE_COLOR_GREEN);
    SET_PBSBSR_HI(PHONE_COLOR_GREEN);
  }else{
    SET_PBDR_LO(PHONE_COLOR_GREEN);
    SET_PBSBSR_LO(PHONE_COLOR_GREEN);
  }
  if( save_gpio_red ){
    SET_PBDR_HI(PHONE_COLOR_RED);
    SET_PBSBSR_HI(PHONE_COLOR_RED);
  }else{
    SET_PBDR_LO(PHONE_COLOR_RED);
    SET_PBSBSR_LO(PHONE_COLOR_RED);
  }
}

static void show_offcharge_charging(void)
{
  SET_PB_OUT(PHONE_COLOR_RED|PHONE_COLOR_GREEN);
  SET_PBDR_HI(PHONE_COLOR_RED|PHONE_COLOR_GREEN);
  SET_PBSBSR_HI(PHONE_COLOR_RED|PHONE_COLOR_GREEN);
}

static void show_offcharge_error(void)
{
  SET_PBDR_LO(PHONE_COLOR_GREEN);
  SET_PBSBSR_LO(PHONE_COLOR_GREEN);
  SET_PBDR_HI(PHONE_COLOR_RED);
  SET_PBSBSR_HI(PHONE_COLOR_RED);
  SET_PESBSR_LO(LED_HARD_BLINK_CTRL);
}

static void offcharge_debug(void)
{
  int val;
  int o;
  int g;
  if( ( o = off_charging ) < 0 ){
    o = 0xf;
    g = offcharge_errno;
  }else{
    g = ( GET_PBDR(CHARGER_START) ? 1 : 0 );
  }
  val = ( o << 4 ) | g;
  IO_IRIS1_DEBUG_LED = val;
}

static void offcharge_shutdown(void)
{
  /* turn charger off */
  IO_IRIS1_DEBUG_LED = 0xc0;
  fullfill_irq_enable = 0;
  offcharger_off();
  show_offcharge_noncharge();
  off_charging = NOT_CHARGING;
  set_rtcmr_driver();
  offcharge_debug();
}

static unsigned long phase2_timeout_dr = 0;

static int battery_polling_check_count = 0;

static int offcharge_batterycheck_is_OK(void)
{
  int th;
  int lo_th;
  int v;
  int temp;
  v = iris_read_battery_mV();
  temp = iris_read_battery_temp();
  if( temp > 100 ){
    if( gsm_modem_status == GSM_PHONE_IN_ANALOG_MODE
	|| gsm_modem_status == GSM_PHONE_IN_DATA_MODE ){
      th = 3600;
      lo_th = 3740;
    }else{
      th = 3640;
      lo_th = 3790;
    }
  }else if( temp > 0 ){
    if( gsm_modem_status == GSM_PHONE_IN_ANALOG_MODE
	|| gsm_modem_status == GSM_PHONE_IN_DATA_MODE ){
      th = 3530;
      lo_th = 3600;
    }else{
      th = 3550;
      lo_th = 3700;
    }
  }else if( temp > -100 ){
    if( gsm_modem_status == GSM_PHONE_IN_ANALOG_MODE
	|| gsm_modem_status == GSM_PHONE_IN_DATA_MODE ){
      th = 3530;
      lo_th = 3850;
    }else{
      th = 3530;
      lo_th = 3900;
    }
  }else{
    if( gsm_modem_status == GSM_PHONE_IN_ANALOG_MODE
	|| gsm_modem_status == GSM_PHONE_IN_DATA_MODE ){
      th = 3530;
      lo_th = 4000;
    }else{
      th = 3530;
      lo_th = 3950;
    }
  }
  th = th + 50;
  if( v < th ){
    battery_polling_check_count++;
  }
  if( v > lo_th ){
    set_rtcmr(STANDBYMODE_BATTERY_POLLING);
    return 1;
  }else if( v > th ){
    battery_polling_check_count = 0;
    set_rtcmr(STANDBYMODE_BATTERY_POLLING_UNDER_LO);
    return 1;
  }else if( battery_polling_check_count < 3 ){
    set_rtcmr(STANDBYMODE_BATTERY_POLLING_UNDER_LO);
    return 1;
  }else{
    set_rtcmr(STANDBYMODE_BATTERY_POLLING_UNDER_LO);
    return 0;
  }
}

#if IRIS_OFF_CHARGER_FLOW_VERSION > 1 /* ver2 or later */

static void offcharge_phase4(void)
{
  IO_IRIS1_DEBUG_LED = 0xc9;
  fullfill_irq_enable = 0;
  /* check current voltage */
  off_charging = OFFCHARGE_PHASE4;
  show_offcharge_fullcharge();
  offcharger_off();
  offcharger_on();
  set_rtcmr(OFFCHARGE_PHASE2_TIMEOUT);
  phase2_timeout_dr = IO_RTCDR + OFFCHARGE_PHASE2_TIMEOUT;
  offcharge_debug();
  return;
}

static void offcharge_phase3_5(void)
{
  int v;
  int diff;
  IO_IRIS1_DEBUG_LED = 0xc8;
  fullfill_irq_enable = 0;
  /* check current voltage */
  off_charging = OFFCHARGE_PHASE3_5;
  offcharger_off();
  v = iris_read_battery_mV();
  if( v < CHARGER_BATTERY_BAD_VALUE ){
    /* this is a bad battery */
    off_charging = OFFCHARGE_ERROR;
    offcharge_errno = 0x1;
    show_offcharge_error();
    set_rtcmr_driver();
    offcharge_debug();
    return;
  }
  if( v < FULLFILLED_VOLTAGE ){
    /* not fullfilled yet. */
    offcharger_on();
    diff = phase2_timeout_dr - IO_RTCDR;
    set_rtcmr(diff);
    off_charging = OFFCHARGE_PHASE2_5;
#if 1 /* 01.07.19 part1*/
    /* !!!!! CHECK Whether or not FullFill IRQ is already ON !!!!!! */
    set_fpga_int_trigger_highlevel(bitWSRC9); /* set RawStat to HI-Level Int. */
    if( IO_FPGA_RawStat & bitWSRC9 ){
      /* FullFill IRQ is Already ON , So go to 'interrupted' phase */
      /* these are the same as phase 3 */
      unsigned long diff = 0;
      interrupt_count++;
      fullfill_irq_enable = 0;
      /* check current voltage */
      off_charging = OFFCHARGE_PHASE3;
      offcharger_off();
      offcharge_debug();
      /* set next timer */
      diff = phase2_timeout_dr - IO_RTCDR;
      if( diff > OFFCHARGE_DISCHARGE_TIMEOUT ){
	off_charging = OFFCHARGE_PHASE3_5;
	set_rtcmr(OFFCHARGE_DISCHARGE_TIMEOUT);
      }else{
	off_charging = OFFCHARGE_PHASE4;
	set_rtcmr(diff);
      }
      offcharge_debug();
      return;
    }
#endif
    fullfill_irq_enable = 1;
    offcharge_debug();
    return;
  }
  /* full-charged */
  offcharger_on();
  diff = phase2_timeout_dr - IO_RTCDR;
  off_charging = OFFCHARGE_PHASE4;
  if( diff > OFFCHARGE_PHASE3_WAIT ){
    set_rtcmr(OFFCHARGE_PHASE3_WAIT);
  }else{
    set_rtcmr(diff);
  }
  return;
}
#endif /* IRIS_OFF_CHARGER_FLOW_VERSION > 1 */


static void offcharge_phase3(void) /* phase2 , fullfill irq occrred , phase3 timeout */
{
  unsigned long diff = 0;
  int v;
  IO_IRIS1_DEBUG_LED = 0xc1;
  interrupt_count++;
  fullfill_irq_enable = 0;
  /* check current voltage */
  off_charging = OFFCHARGE_PHASE3;
  offcharger_off();
  offcharge_debug();
#if IRIS_OFF_CHARGER_FLOW_VERSION > 1 /* ver2 or later */
  if( interrupt_count >= FULLFILL_INTERRUPT_MAXCOUNT ){
    show_offcharge_fullcharge();
    off_charging = OFFCHARGE_PHASE4;
    fullfill_irq_enable = 0;
    offcharger_off();
    offcharger_on();
    set_rtcmr(OFFCHARGE_PHASE2_TIMEOUT);
    phase2_timeout_dr = IO_RTCDR + OFFCHARGE_PHASE2_TIMEOUT;
    offcharge_debug();
    return;
  }
  diff = phase2_timeout_dr - IO_RTCDR;
  if( diff > OFFCHARGE_DISCHARGE_TIMEOUT ){
    off_charging = OFFCHARGE_PHASE3_5;
    set_rtcmr(OFFCHARGE_DISCHARGE_TIMEOUT);
  }else{
    off_charging = OFFCHARGE_PHASE4;
    set_rtcmr(diff);
  }
  offcharge_debug();
  return;
#else /* ! IRIS_OFF_CHARGER_FLOW_VERSION > 1 */
  v = iris_read_battery_mV();
  if( v < FULLFILLED_VOLTAGE ){
    /* not fullfilled yet. */
    offcharger_on();
    diff = phase2_timeout_dr - IO_RTCDR;
    if( diff > OFFCHARGE_PHASE3_WAIT ){
      set_rtcmr(OFFCHARGE_PHASE3_WAIT);
    }else{
      set_rtcmr(OFFCHARGE_PHASE2_TIMEOUT);
    }
    offcharge_debug();
    return;
  }
  /* charging done */
  /* check if AC online */
  if( IO_FPGA_LSEL & AC_INPUT ){
    /* Currentry HI level trigger , So 1->1 convert */
    if( (IO_FPGA_RawStat & AC_INPUT) ) goto NON_CHARGE;
  }else{
    /* Currentry LO level trigger , so wsrc is inverted. */
    if( ! (IO_FPGA_RawStat & AC_INPUT) ) goto NON_CHARGE;
  }
  /* hold charger on , LED turn off */
  show_offcharge_fullcharge();
  off_charging = OFFCHARGE_PHASE4;
  offcharger_on();
  set_rtcmr(OFFCHARGE_PHASE2_TIMEOUT);
  offcharge_debug();
  return;
 NON_CHARGE:
  /* turn charger off , LED turn off */
  offcharger_off();
  show_offcharge_fullcharge();
  off_charging = NOT_CHARGING;
  set_rtcmr_driver();
  offcharge_debug();
  return;
#endif /* ! IRIS_OFF_CHARGER_FLOW_VERSION > 1 */
}

static void offcharge_phase2_5(void) /* phase2 , timeout */
{
  IO_IRIS1_DEBUG_LED = 0xc2;
#if IRIS_OFF_CHARGER_FLOW_VERSION > 1 /* ver2 or later */
  /* restart charger , but irq disable */
  offcharger_off();
  offcharger_on();
  fullfill_irq_enable = 0;
  show_offcharge_fullcharge();
  off_charging = OFFCHARGE_PHASE4;
  set_rtcmr(OFFCHARGE_PHASE2_TIMEOUT);
  phase2_timeout_dr = IO_RTCDR + OFFCHARGE_PHASE2_TIMEOUT;
  offcharge_debug();
  return;
#else /* ! IRIS_OFF_CHARGER_FLOW_VERSION > 1 */
  offcharger_off();
  show_offcharge_fullcharge();
  fullfill_irq_enable = 0;
  off_charging = NOT_CHARGING;
  set_rtcmr_driver();
  offcharge_debug();
  return;
#endif /* ! IRIS_OFF_CHARGER_FLOW_VERSION > 1 */
}

#if IRIS_OFF_CHARGER_FLOW_VERSION > 1 /* ver2 or later */
static void offcharge_phase2_1(void)
{
  int v;
  IO_IRIS1_DEBUG_LED = 0xc4;
  v = iris_read_battery_mV();
  offcharge_debug();
  if( v < CHARGER_BATTERY_BAD_VALUE ){
    /* this is a bad battery */
    offcharger_off();
    fullfill_irq_enable = 0;
    off_charging = OFFCHARGE_ERROR;
    offcharge_errno = 0x1;
    show_offcharge_error();
    set_rtcmr_driver();
    offcharge_debug();
    return;
  }
  /* pre-charge done. goto phase2 */
  offcharger_on();
  fullfill_irq_enable = 1;
  off_charging = OFFCHARGE_PHASE2_5;
  set_rtcmr(OFFCHARGE_PHASE2_TIMEOUT);
  phase2_timeout_dr = IO_RTCDR + OFFCHARGE_PHASE2_TIMEOUT;
  offcharge_debug();
#if 1 /* 01.07.19 part1*/
  /* !!!!! CHECK Whether or not FullFill IRQ is already ON !!!!!! */
  set_fpga_int_trigger_highlevel(bitWSRC9); /* set RawStat to HI-Level Int. */
  if( IO_FPGA_RawStat & bitWSRC9 ){
    /* FullFill IRQ is Already ON , So go to 'interrupted' phase */
    /* these are the same as phase 3 */
    unsigned long diff = 0;
    interrupt_count++;
    fullfill_irq_enable = 0;
    /* check current voltage */
    off_charging = OFFCHARGE_PHASE3;
    offcharger_off();
    offcharge_debug();
    /* set next timer */
    diff = phase2_timeout_dr - IO_RTCDR;
    if( diff > OFFCHARGE_DISCHARGE_TIMEOUT ){
      off_charging = OFFCHARGE_PHASE3_5;
      set_rtcmr(OFFCHARGE_DISCHARGE_TIMEOUT);
    }else{
      off_charging = OFFCHARGE_PHASE4;
      set_rtcmr(diff);
    }
    offcharge_debug();
  }
#endif
  return;
}
#endif

static void offcharge_phase2(void)
{
  IO_IRIS1_DEBUG_LED = 0xc3;
  /* start charging */
  offcharger_on();
  show_offcharge_charging();
#if IRIS_OFF_CHARGER_FLOW_VERSION > 1 /* ver2 or later */
  fullfill_irq_enable = 0;
  off_charging = OFFCHARGE_PHASE2_1;
  set_rtcmr(OFFCHARGE_PRECHARGE_TIMEOUT);
  phase2_timeout_dr = IO_RTCDR + OFFCHARGE_PHASE2_TIMEOUT;
  offcharge_debug();
  return;
#else /* ! IRIS_OFF_CHARGER_FLOW_VERSION > 1 */
  fullfill_irq_enable = 1;
  off_charging = OFFCHARGE_PHASE2_5;
  set_rtcmr(OFFCHARGE_PHASE2_TIMEOUT);
  phase2_timeout_dr = IO_RTCDR + OFFCHARGE_PHASE2_TIMEOUT;
  offcharge_debug();
  return;
#endif /* ! IRIS_OFF_CHARGER_FLOW_VERSION > 1 */
}

static void offcharge_phase1_5(void) /* phase1 , timeout */
{
#if IRIS_OFF_CHARGER_FLOW_VERSION < 2 /* ver1 or earlier */
  int v;
  IO_IRIS1_DEBUG_LED = 0xc4;
  offcharger_off();
  v = iris_read_battery_mV();
  offcharge_debug();
  fullfill_irq_enable = 0;
  if( v < CHARGER_BATTERY_BAD_VALUE ){
    /* this is a bad battery */
    off_charging = OFFCHARGE_ERROR;
    offcharge_errno = 0x1;
    show_offcharge_error();
    set_rtcmr_driver();
    offcharge_debug();
    return;
  }
  /* pre-charge done. goto phase2 */
  off_charging = OFFCHARGE_PHASE2;
  offcharge_phase2();
  return;
#endif /* IRIS_OFF_CHARGER_FLOW_VERSION < 2 */
}

static void start_offcharge(void)
{
  int v;
  IO_IRIS1_DEBUG_LED = 0xc5;
  fullfill_irq_enable = 0;
  switch(off_charging){
  case OFFCHARGE_PHASE1_5:
    offcharge_phase1_5();
    return;
#if IRIS_OFF_CHARGER_FLOW_VERSION > 1 /* ver2 or later */
  case OFFCHARGE_PHASE2_1:
    offcharge_phase2_1();
    return;
  case OFFCHARGE_PHASE4:
    offcharge_phase4();
    return;
  case OFFCHARGE_PHASE3_5:
    offcharge_phase3_5();
    return;
#endif
  case OFFCHARGE_PHASE2_5:
    offcharge_phase2_5();
    return;
  case OFFCHARGE_PHASE3:
    offcharge_phase3();
    return;
  case OFFCHARGE_PHASE2:
    offcharge_phase2();
    return;
  default:
    /* do nothing */
    break;
  }
  off_charging = NOT_CHARGING;
  offcharge_debug();
  interrupt_count = 0;
  /* check if AC online */
  if( IO_FPGA_LSEL & AC_INPUT ){
    /* Currentry HI level trigger , So 1->1 convert */
    if( (IO_FPGA_RawStat & AC_INPUT) ) goto NON_CHARGE;
  }else{
    /* Currentry LO level trigger , so wsrc is inverted. */
    if( ! (IO_FPGA_RawStat & AC_INPUT) ) goto NON_CHARGE;
  }
  /* read charger IC status */
  offcharger_on();
  offcharge_debug();
  v = iris_read_battery_mV();
#if IRIS_OFF_CHARGER_FLOW_VERSION > 1 /* ver2 or later */
  if( v > CHARGER_IC_BAD_VALUE ) goto NON_CHARGE;
  /* go to phase2 charge */
  offcharge_phase2();
  return;
#else /* ! IRIS_OFF_CHARGER_FLOW_VERSION > 1 */
  offcharger_off();
  offcharge_debug();
  if( v > CHARGER_IC_BAD_VALUE ) goto NON_CHARGE;
  /* check battery exist or not */
  off_charging = OFFCHARGE_PHASE1;
  v = iris_read_battery_mV();
  if( v < CHARGER_BATTERY_BAD_VALUE ){
    off_charging = OFFCHARGE_PHASE1_5;
    offcharger_on();
    set_rtcmr(OFFCHARGE_PHASE1_WAIT);
    offcharge_debug();
  }else{
    off_charging = OFFCHARGE_PHASE2;
    offcharge_phase2();
  }
  return;
#endif /* ! IRIS_OFF_CHARGER_FLOW_VERSION > 1 */
NON_CHARGE:
  offcharger_off();
  show_offcharge_noncharge();
  set_rtcmr_driver();
  off_charging = NOT_CHARGING;
  offcharge_debug();
  return;
}



static void start_batpoll(void)
{
  IO_IRIS1_DEBUG_LED = 0xc5;
  battery_polling_check_count = 0;
  fullfill_irq_enable = 0;
  off_charging = OFFCHARGE_BATCHECKPOLLING;
  set_rtcmr(STANDBYMODE_BATTERY_POLLING);
  return;
}


static void CommonCheckOffChargeInEntrance(int mode)
{
  /* check AC status and start charger */
  if( IO_FPGA_LSEL & AC_INPUT ){
    /* Currentry HI level trigger , So 1->1 convert */
    if( ! (IO_FPGA_RawStat & AC_INPUT) ){
      /* AC online , so start charger */
      start_offcharge();
    }else if( mode == THIS_IS_STANDBY_MODE ){
      start_batpoll();
    }
  }else{
    /* Currentry LO level trigger , so wsrc is inverted. */
    if( (IO_FPGA_RawStat & AC_INPUT) ){
      /* AC online , so start charger */
      start_offcharge();
    }else if( mode == THIS_IS_STANDBY_MODE ){
      start_batpoll();
    }
  }
}


#endif /* OFFCHARGE_PROC_ON_POWER_C */

/*
 *   Common Routines for Suspend/Standby
 *
 */


#define KEY_CHECK_AND_REBOOT_AFTER_BATOK

#ifdef KEY_CHECK_AND_REBOOT_AFTER_BATOK
extern void machine_restart(char * __unused);
#endif /*KEY_CHECK_AND_REBOOT_AFTER_BATOK */

static void emerge_off_common(void);


static void CommonSaveRegisters(void)
{
  save_regs.irqenable = IO_IRQENABLE;
  save_regs.fiqenable = IO_FIQENABLE;
  save_regs.irqhenable = IO_IRQHENABLE;
  save_regs.fpga_een = IO_FPGA_EEN;
  save_regs.fpga_esel = IO_FPGA_ESEL;
  save_regs.fpga_de = IO_FPGA_DE;
  save_regs.fpga_lsel = IO_FPGA_LSEL;
  save_regs.fpga_enable = IO_FPGA_Enable;

  save_regs.kbdmr = IO_KBDMR;
  save_regs.kbksr = IO_KBKSR;
  save_regs.kbdr = IO_KBDR;
}

static void CommonRestoreRegisters(void)
{
  IO_KBDMR = save_regs.kbdmr;
  IO_KBKSR = save_regs.kbksr;
  IO_KBDR = save_regs.kbdr;

  IO_FPGA_EEN = save_regs.fpga_een;
  IO_FPGA_ESEL = save_regs.fpga_esel;
  IO_FPGA_DE = save_regs.fpga_de;
  IO_FPGA_LSEL = save_regs.fpga_lsel;
  IO_FIQENABLESET = save_regs.fiqenable;
  IO_IRQENABLESET = save_regs.irqenable;
  IO_IRQHENABLESET = save_regs.irqhenable;
}

static int first_key_release_waiting = 0;
static int port_usb_is_waiting_for_remove = 0;
static int port_uart_is_waiting_for_remove = 0;
static int port_ac_is_waiting_for_remove = 0;
static int port_ring_is_wating_for_low = 0;


static void CommonCheckCurrentExternalPorts(void)
{
  if( IO_FPGA_LSEL & bitWSRC1 ){
    /* Currently HI level trigger , So 1->1 convert */
    if( IO_FPGA_RawStat & bitWSRC1 ){
      /* Ring wsrc is HI --- Ringing ? --- wait for LO */
      port_ring_is_wating_for_low = 1;
    }else{
      port_ring_is_wating_for_low = 0;
    }
  }else{
    /* Currently LO level trigger , So wsrc is inverted */
    if( IO_FPGA_RawStat & bitWSRC1 ){ 
      port_ring_is_wating_for_low = 0;
    }else{
      /* Ring wsrc is HI --- Ringing ? --- wait for LO */
      port_ring_is_wating_for_low = 1;
    }
  }


  if( IO_FPGA_LSEL & bitWSRC2 ){
    /* Currently HI level trigger , So 1->1 convert */
    if( IO_FPGA_RawStat & bitWSRC2 ){
      /* AC wsrc is HI --- AC OFFLINE --- wait for insert */
      port_ac_is_waiting_for_remove = 0;
    }else{
      port_ac_is_waiting_for_remove = 1;
    }
  }else{
    /* Currently LO level trigger , So wsrc is inverted */
    if( IO_FPGA_RawStat & bitWSRC2 ){
      port_ac_is_waiting_for_remove = 1;
    }else{
      /* AC wsrc is HI --- AC OFFLINE --- wait for insert */
      port_ac_is_waiting_for_remove = 0;
    }
  }

  if( IO_FPGA_LSEL & bitWSRC3 ){
    /* Currently HI level trigger , So 1->1 convert */
    if( IO_FPGA_RawStat & bitWSRC3 ){
      /* UART wsrc is HI --- UART OFFLINE --- wait for insert */
      port_uart_is_waiting_for_remove = 0;
    }else{
      port_uart_is_waiting_for_remove = 1;
    }
  }else{
    /* Currently LO level trigger , So wsrc is inverted */
    if( IO_FPGA_RawStat & bitWSRC3 ){
      port_uart_is_waiting_for_remove = 1;
    }else{
      /* UART wsrc is HI --- UART OFFLINE --- wait for insert */
      port_uart_is_waiting_for_remove = 0;
    }
  }

  if( IO_FPGA_LSEL & bitWSRC4 ){
    /* Currently HI level trigger , So 1->1 convert */
    if( IO_FPGA_RawStat & bitWSRC4 ){
#ifdef USBPWR_INVERTED_ON_TARGET /* If use on Target , On which USBPWR is HI if inserted */
      /* USB wsrc is HI --- USB ONLINE --- wait for remove */
      port_usb_is_waiting_for_remove = 1;
#else /* ! USBPWR_INVERTED_ON_TARGET */
      /* USB wsrc is HI --- USB OFFLINE --- wait for insert */
      port_usb_is_waiting_for_remove = 0;
#endif /* ! USBPWR_INVERTED_ON_TARGET */
    }else{
#ifdef USBPWR_INVERTED_ON_TARGET /* If use on Target , On which USBPWR is HI if inserted */
      port_usb_is_waiting_for_remove = 0;
#else /* ! USBPWR_INVERTED_ON_TARGET */
      port_usb_is_waiting_for_remove = 1;
#endif /* ! USBPWR_INVERTED_ON_TARGET */
    }
  }else{
    /* Currently LO level trigger , So wsrc is inverted */
    if( IO_FPGA_RawStat & bitWSRC4 ){
#ifdef USBPWR_INVERTED_ON_TARGET /* If use on Target , On which USBPWR is HI if inserted */
      port_usb_is_waiting_for_remove = 0;
#else /* ! USBPWR_INVERTED_ON_TARGET */
      port_usb_is_waiting_for_remove = 1;
#endif /* ! USBPWR_INVERTED_ON_TARGET */
    }else{
#ifdef USBPWR_INVERTED_ON_TARGET /* If use on Target , On which USBPWR is HI if inserted */
      /* USB wsrc is HI --- USB ONLINE --- wait for remove */
      port_usb_is_waiting_for_remove = 1;
#else /* ! USBPWR_INVERTED_ON_TARGET */
      /* USB wsrc is HI --- USB OFFLINE --- wait for insert */
      port_usb_is_waiting_for_remove = 0;
#endif /* ! USBPWR_INVERTED_ON_TARGET */
    }
  }
}


static void CommonCheckCurrentKeyPress(int mode)
{
  int save_kbdmr;
  save_kbdmr = IO_KBDMR;
  IO_KBDMR &= ~(1 << 12); /* keyboard mode */
  IO_KBKSR = 1; /* all pins low , and discharge */
  udelay(20); /* wait discharge delay */
#if 1
  first_key_release_waiting = 0;
  IO_KBKSR = 1; /* all pins low , and discharge */
  udelay(20); /* wait discharge delay */
  IO_KBKSR = 6; /* activate KBCOL2 , which has Display key at RAW5 */
  udelay(20); /* wait discharge delay */
  if( IO_PADR & ( 1 << 5 ) ){ /* check RAW5 */
    /* Disp Key Is Pressed. So , Wait for Release */
    if( mode == THIS_IS_STANDBY_MODE ){
      first_key_release_waiting = 1;
    }else{
    }
  }
  IO_KBKSR = 1; /* all pins low , and discharge */
  udelay(20); /* wait discharge delay */
  IO_KBKSR = 4; /* activate KBCOL0 , which has Phone(Power) key at RAW5 */
  udelay(20); /* wait activate delay */
  if( IO_PADR & ( 1 << 5 ) ){ /* check RAW5 */
    /* Power Key Is Pressed. So , Wait for Release */
    if( mode == THIS_IS_STANDBY_MODE ){
      first_key_release_waiting = 1;
    }else{
      first_key_release_waiting = 1;
    }
  }
  IO_KBKSR = 1; /* all pins low , and discharge */
  udelay(20); /* wait discharge delay */
  IO_KBKSR = 11; /* activate KBCOL7 , which has Main(Home) key at RAW5 */
  udelay(20); /* wait activate delay */
  if( IO_PADR & ( 1 << 5 ) ){ /* check RAW5 */
    /* Main Key Is Pressed. So , Wait for Release */
    if( mode == THIS_IS_STANDBY_MODE ){
      first_key_release_waiting = 1;
    }else{
      first_key_release_waiting = 1;
    }
  }
  IO_KBKSR = 1; /* all pins low , and discharge */
  udelay(20); /* wait discharge delay */
#endif
#if 0
  IO_KBKSR = 0; /* activate ALL COLs */
  udelay(20); /* wait discharge delay */
  if( IO_PADR & (bitPA0|bitPA1|bitPA2|bitPA3|bitPA4|bitPA5) ){ /* check RAW5 */
    /* If Any of Keys are Pressed */
    first_key_release_waiting = 1;
  }else{
    first_key_release_waiting = 0;
  }
  IO_KBKSR = 1; /* all pins low , and discharge */
  udelay(20); /* wait discharge delay */
#endif
#if 0
  if( THIS_IS_STANDBY_MODE ){
    IO_KBDMR = (1 << 12); /* GPIO Mode */
    IO_KBDR = ( 1 << 0 ) | ( 1 << 7 ); /* COL0(Phone) , COL7(Main) are activated as GPIO */
  }else{
    IO_KBDMR = (1 << 12); /* GPIO Mode */
    IO_KBDR = ( 1 << 0 ) | ( 1 << 7 ); /* COL0(Phone) , COL7(Main) are activated as GPIO */
  }
  if( IO_FPGA_LSEL & bitWSRC7 ){
    /* Currently HI level trigger , So 1->1 convert */
    if( IO_FPGA_RawStat & bitWSRC7 ){
      /* Key wsrc is HI --- Key Pressed --- wait for release */
      first_key_release_waiting = 1;
      //IO_IRIS1_DEBUG_LED = 0x98; /* 10011000 */
    }else{
      first_key_release_waiting = 0;
    }
  }else{
    /* Currently LO level trigger , So wsrc is inverted */
    if( IO_FPGA_RawStat & bitWSRC7 ){
      first_key_release_waiting = 0;
    }else{
      /* Key wsrc is HI --- Key Pressed --- wait for release */
      first_key_release_waiting = 1;
      IO_IRIS1_DEBUG_LED = 0x99; /* 10011001 */
    }
  }
#endif
  IO_KBDMR = save_kbdmr;
}

static void CommonSetUpGAInterruptBeforeSuspend(int mode)
{
  /* ============= WSRC0 : Power On ============ */
  //set_fpga_int_trigger_highedge(bitWSRC0);	/* Power on */
  /* ============= WSRC1 : Ringer From Communication Module ============ */
  /* RI interrupt is changed from highedge -> highlevel */
  //set_fpga_int_trigger_highedge(bitWSRC1);	/* Communication Module */
  if( mode == THIS_IS_STANDBY_MODE ){
    if( port_ring_is_wating_for_low ){
      set_fpga_int_trigger_lowlevel(bitWSRC1);	/* Communication Module */
    }else{
      set_fpga_int_trigger_highlevel(bitWSRC1);	/* Communication Module */
    }
  }else{
    //set_fpga_int_trigger_highlevel(bitWSRC1);	/* Communication Module */
    if( port_ring_is_wating_for_low ){
      set_fpga_int_trigger_lowlevel(bitWSRC1);	/* Communication Module */
    }else{
      set_fpga_int_trigger_highlevel(bitWSRC1);	/* Communication Module */
    }
  }
  /* ============= WSRC2 : External Power ============ */
  if( ! port_ac_is_waiting_for_remove ){
    /* waiting for insert --- waiting for LO */
    set_fpga_int_trigger_lowlevel(bitWSRC2);
  }else{
    set_fpga_int_trigger_highlevel(bitWSRC2);	/* Externel Power */
  }
  //if( IO_FPGA_RawStat & bitWSRC2 ){ /* already ON , so detect LO */
  //set_fpga_int_trigger_lowlevel(bitWSRC2);
  //}
  //set_fpga_int_trigger_highlevel(bitWSRC2);	/* Externel Power */
  //if( IO_FPGA_RawStat & bitWSRC2 ){ /* already ON , so detect LO */
  //set_fpga_int_trigger_lowlevel(bitWSRC2);
  //}
  /* ============= WSRC3 : External UART , DSR2 ============ */
  if( ! port_uart_is_waiting_for_remove ){
    /* waiting for insert --- waiting for LO */
    set_fpga_int_trigger_lowlevel(bitWSRC3);	/* UART2 DSR2 */
  }else{
    set_fpga_int_trigger_highlevel(bitWSRC3);
  }
  //set_fpga_int_trigger_lowlevel(bitWSRC3);	/* UART2 DSR2 */
  //if( IO_FPGA_RawStat & bitWSRC3 ){ /* already ON , so detect HI */
  //set_fpga_int_trigger_highlevel(bitWSRC3);
  //}
  /* ============= WSRC4 : External USB ============ */
  if( ! port_usb_is_waiting_for_remove ){
#ifdef USBPWR_INVERTED_ON_TARGET /* If use on Target , On which USBPWR is HI if inserted */
    /* waiting for insert --- waiting for HI */
    set_fpga_int_trigger_highlevel(bitWSRC4);	/* Connect USB cable */	
#else /* ! USBPWR_INVERTED_ON_TARGET */
    set_fpga_int_trigger_lowlevel(bitWSRC4);	/* Connect USB cable */	
#endif /* ! USBPWR_INVERTED_ON_TARGET */
  }else{
#ifdef USBPWR_INVERTED_ON_TARGET /* If use on Target , On which USBPWR is HI if inserted */
    set_fpga_int_trigger_lowlevel(bitWSRC4);
#else /* ! USBPWR_INVERTED_ON_TARGET */
    set_fpga_int_trigger_highlevel(bitWSRC4);
#endif /* ! USBPWR_INVERTED_ON_TARGET */
  }
  //set_fpga_int_trigger_highlevel(bitWSRC4);	/* Connect USB cable */	
  //if( IO_FPGA_RawStat & bitWSRC4 ){ /* already ON , so detect LO */
  //set_fpga_int_trigger_lowlevel(bitWSRC4);
  //}
  /* ============= WSRC5 : MMC CardSlot ============ */
  //set_fpga_int_trigger_bothedge(bitWSRC5);	/* Detect MMC/SD card */
  /* ============= WSRC6 : Touch Panel ============ */
  //set_fpga_int_trigger_lowlevel(bitWSRC6);	/* Touch Panel */
  /* ============= WSRC7 : Any Key ============ */
  if( first_key_release_waiting ){
    /* waiting for key release... */
    set_fpga_int_trigger_lowlevel(bitWSRC7);	/* Any key */
  }else{
    set_fpga_int_trigger_highlevel(bitWSRC7);	/* Any key */
  }
  /* ============= WSRC8 : Battery Cover ============ */
  if( mode == THIS_IS_STANDBY_MODE ){
    set_fpga_int_trigger_highlevel(bitWSRC8);	/* Detect BAT-Cover */
  }else{
    /* Detect BatteryCover Closed */
    set_fpga_int_trigger_lowedge(bitWSRC8);	/* Detect BAT-Cover */
  }
  /* ============= WSRC9 : Battery Charge Full ============ */
#ifdef OFFCHARGE_PROC_ON_POWER_C
  if( fullfill_irq_enable ){
    set_fpga_int_trigger_highlevel(bitWSRC9);	/* Battery Charge Full */
  }
#endif /* OFFCHARGE_PROC_ON_POWER_C */
}


static void CommonEnableGAInterruptBeforeSuspend(int mode)
{
  unsigned long ga_mask;
  unsigned long mask;
  unsigned long wsrc_def;
  ga_mask = (( apm_wakeup_src_mask >> WAKEUP_SRC_GA_SHIFT ) & WAKEUP_SRC_GA_MASK);

  if( mode == THIS_IS_SUSPEND_MODE ){
    ga_mask |= WAKEUP_SRC_MASKIGNORE_SUSPEND;
    wsrc_def = WAKEUP_SRC_SUSPEND;
  }else{
    ga_mask |= WAKEUP_SRC_MASKIGNORE_STANDBY;
    wsrc_def = WAKEUP_SRC_STANDBY;
  }

#ifdef OFFCHARGE_PROC_ON_POWER_C
  if( fullfill_irq_enable ){
    mask = (ga_mask|bitWSRC9) & (wsrc_def|bitWSRC9);
  }else{
    mask = ga_mask & wsrc_def;
  }
#else /* OFFCHARGE_PROC_ON_POWER_C */
  mask = ga_mask & wsrc_def;
#endif /* OFFCHARGE_PROC_ON_POWER_C */

  enable_fpga_irq(mask);


}


static void CommonSetUpRTCInterruptBeforeSuspend(int mode)
{
#ifdef OFFCHARGE_PROC_ON_POWER_C
  if( rtcirq_setting == RTCIRQ_IS_OFFCHARGE ){
    IO_RTCCR |= RTCCR_ENALARM;
  }else{
    IO_RTCCR = save_rtccr;
  }
#else /* OFFCHARGE_PROC_ON_POWER_C */
#endif /* OFFCHARGE_PROC_ON_POWER_C */
}


static int CommonCheckBatteryCoverIsClosed(int mode)
{
  set_fpga_int_trigger_highlevel(bitWSRC8); /* set RawStat to HI-Level Int. */
  if( ( IO_FPGA_LSEL & bitWSRC8 ) ){
    /* Currently HI level interrupt , RawStat tells HI if cover opened */
    if( IO_FPGA_RawStat & bitWSRC8 ){ /* HI , so cover open */
      if( mode == THIS_IS_STANDBY_MODE ){
	IO_IRIS1_DEBUG_LED = 0xee; /* 10101100 */
	emerge_off_common();
      }
      return 0; /* Open ! */
    }else{ /* LO , so cover close */
      /* check ok */
      return 1; /* Closed ! */
    }
  }else{
    /* Currently LO level interrupt , RawStat tells LO if cover opened */
    if( IO_FPGA_RawStat & bitWSRC8 ){ /* LO , so cover open */
      /* check ok */
      return 1; /* Closed ! */
    }else{ /* HI , so cover close */
      if( mode == THIS_IS_STANDBY_MODE ){
	IO_IRIS1_DEBUG_LED = 0xee; /* 10101100 */
	emerge_off_common();
      }
      return 0; /* Open ! */
    }
  }
}

static int CommonCheckIfBatteryStatusIsEnough(void)
{
  int v;
  int temp;
  int th;
  v = iris_read_battery_mV();
  temp = iris_read_battery_temp();
  if( temp > 100 ){
    if( gsm_modem_status == GSM_PHONE_IN_ANALOG_MODE
	|| gsm_modem_status == GSM_PHONE_IN_DATA_MODE ){
      th = 3600;
    }else{
      th = 3640;
    }
  }else if( temp > 0 ){
    if( gsm_modem_status == GSM_PHONE_IN_ANALOG_MODE
	|| gsm_modem_status == GSM_PHONE_IN_DATA_MODE ){
      th = 3530;
    }else{
      th = 3550;
    }
  }else{
    th = 3530;
  }
  if( v < th ) return 0;
  /* voltage is enough */
  if( GET_PBDR(CHARGER_START) ){
    /* currently , charger ON */
    /* tempolally charger OFF , and check voltage */
    offcharger_off();
    v = iris_read_battery_mV();
    offcharger_on(); /* restore charger status */
  }else{
    /* currently , charger OFF */
    return 1;
  }
  /* charger ON and voltage recheck */
  return ( v < th ? 0 : 1 );
}


static int CommonCheckWakeupSrcAndCheckIfResume(int mode,unsigned long fpga_stat)
{
  if( IO_IRQSTATUS & (1 << IRQ_RTC_ALARM) ){
#ifndef OFFCHARGE_PROC_ON_POWER_C
    /* RTC Alarm... Go Resume */
    if( apm_wakeup_src_mask & WAKEUP_RTC ){ /* not masked only ... */
      apm_wakeup_factor |= WAKEUP_RTC;
      goto do_resume;
    }
#else /* ! OFFCHARGE_PROC_ON_POWER_C */
    if( rtcirq_setting == RTCIRQ_IS_DRIVER ){
      /* RTC Alarm... Go Resume */
      if( apm_wakeup_src_mask & WAKEUP_RTC ){ /* not masked only ... */
	apm_wakeup_factor |= WAKEUP_RTC;
	goto do_resume;
      }
    }else{
      unsigned long tmpval;
      tmpval = IO_RTCS; /* clear interrupt */
      switch(off_charging){
      case OFFCHARGE_PHASE1_5:
	offcharge_phase1_5();
	break;
      case OFFCHARGE_PHASE2_5:
	offcharge_phase2_5();
	break;
      case OFFCHARGE_PHASE3:
	offcharge_phase3();
	break;
#if IRIS_OFF_CHARGER_FLOW_VERSION > 1 /* ver2 or later */
      case OFFCHARGE_PHASE2_1:
	offcharge_phase2_1();
	break;
      case OFFCHARGE_PHASE4:
	offcharge_phase4();
	break;
      case OFFCHARGE_PHASE3_5:
	offcharge_phase3_5();
	break;
#endif
      case OFFCHARGE_BATCHECKPOLLING:
	if( mode == THIS_IS_STANDBY_MODE ){
	  if( ! offcharge_batterycheck_is_OK() ){
	    /* Battery Fatal !! Need to be OFF */
	    apm_wakeup_factor |= WAKEUP_BATCRIT;
	    goto do_resume;
	  }
	}
	break;
      case OFFCHARGE_ERROR:
      case NOT_CHARGING:
      case OFFCHARGE_PHASE1:
      case OFFCHARGE_PHASE2:
      default:
	/* do nothing */
	break;
      }
    }
#endif /* OFFCHARGE_PROC_ON_POWER_C */
  }
#ifdef OFFCHARGE_PROC_ON_POWER_C
  if( fullfill_irq_enable && ( fpga_stat & bitWSRC9 ) ){
    /* charger fullfilled ... */
    disable_fpga_irq(bitWSRC9);
    clear_fpga_int(bitWSRC9);
    offcharge_phase3();
  }
#endif /* OFFCHARGE_PROC_ON_POWER_C */
  if( fpga_stat & bitWSRC1 ){
    udelay(20);
    if( ( IO_FPGA_LSEL & bitWSRC1 ) ){
      if( IO_FPGA_RawStat & bitWSRC1 ){
	/* UART1 Ring ... Go Resume */
	apm_wakeup_factor |= WAKEUP_COMM;
	goto do_resume;
      }else{
	port_ring_is_wating_for_low = 0;
      }
    }else{
      if( IO_FPGA_RawStat & bitWSRC1 ){
	port_ring_is_wating_for_low = 0;
      }else{
	/* UART1 Ring ... Go Resume */
	apm_wakeup_factor |= WAKEUP_COMM;
	goto do_resume;
      }
    }
  }
  if( fpga_stat & bitWSRC2 ){
    /* External Power ... Check Current Level */
    if( ( IO_FPGA_LSEL & bitWSRC2 ) ){
      /* Currently HI level interrupt , HI Detected , and detect LO level next */
#ifdef OFFCHARGE_PROC_ON_POWER_C
      /* AC turned off */
      offcharge_shutdown();
#endif /* OFFCHARGE_PROC_ON_POWER_C */
      set_fpga_int_trigger_lowlevel(bitWSRC2);
      if( mode == THIS_IS_STANDBY_MODE ){
	start_batpoll();
      }
      port_ac_is_waiting_for_remove = 0; /* waiting for AC turn ON */
    }else{
      /* Currently LO level interrupt , LO Detected , and Go Resume */
#ifdef OFFCHARGE_PROC_ON_POWER_C
      /* AC turned on */
      start_offcharge();
#endif /* OFFCHARGE_PROC_ON_POWER_C */
      port_ac_is_waiting_for_remove = 1; /* waiting for AC turn OFF */
    }
  }
  if( ( fpga_stat & bitWSRC3 ) && ( port_uart_is_waiting_for_remove == 0 ) ){
    /* UART2 DSR ... Check Current Level */
    if( ! ( IO_FPGA_LSEL & bitWSRC3 ) ){
      /* Currently LO level interrupt , LO Detected , and Go Resume */
      apm_wakeup_factor |= WAKEUP_UART2;
      goto do_resume;
    }else{
      /* Currently HI level interrupt , HI Detected , and detect LO level next */
      set_fpga_int_trigger_lowlevel(bitWSRC3);
      port_uart_is_waiting_for_remove = 0;
    }
  }
  else if( ( fpga_stat & bitWSRC3 ) && port_uart_is_waiting_for_remove ){
    set_fpga_int_trigger_lowlevel(bitWSRC3);
    port_uart_is_waiting_for_remove = 0;
  }
  if( ( fpga_stat & bitWSRC4 ) && ( port_usb_is_waiting_for_remove == 0 ) ){
    /* USB ... Check Current Level */
    if( ( IO_FPGA_LSEL & bitWSRC4 ) ){
#ifdef USBPWR_INVERTED_ON_TARGET /* If use on Target , On which USBPWR is HI if inserted */
      /* Currently HI level interrupt , HI Detected , and Go Resume */
      apm_wakeup_factor |= WAKEUP_USB;
      goto do_resume;
#else /* ! USBPWR_INVERTED_ON_TARGET */
      /* Currently HI level interrupt , HI Detected , and detect LO level next */
      set_fpga_int_trigger_lowlevel(bitWSRC4);
      port_usb_is_waiting_for_remove = 0;
#endif /* ! USBPWR_INVERTED_ON_TARGET */
    }else{
#ifdef USBPWR_INVERTED_ON_TARGET /* If use on Target , On which USBPWR is HI if inserted */
      /* Currently LO level interrupt , LO Detected , and detect HI level next */
      set_fpga_int_trigger_highlevel(bitWSRC4);
      port_usb_is_waiting_for_remove = 0;
#else /* ! USBPWR_INVERTED_ON_TARGET */
      /* Currently LO level interrupt , LO Detected , and Go Resume */
      apm_wakeup_factor |= WAKEUP_USB;
      goto do_resume;
#endif /* ! USBPWR_INVERTED_ON_TARGET */
    }
  }
  else if( ( fpga_stat & bitWSRC4 ) && port_usb_is_waiting_for_remove ){
#ifdef USBPWR_INVERTED_ON_TARGET /* If use on Target , On which USBPWR is HI if inserted */
    /* waiting for usb remove is done... */
    set_fpga_int_trigger_highlevel(bitWSRC4);
    port_usb_is_waiting_for_remove = 0;
#else /* ! USBPWR_INVERTED_ON_TARGET */
    /* waiting for usb remove is done... */
    set_fpga_int_trigger_lowlevel(bitWSRC4);
    port_usb_is_waiting_for_remove = 0;
#endif /* ! USBPWR_INVERTED_ON_TARGET */
  }
  if( fpga_stat & bitWSRC8 ){
    /* Battery Cover Interrupt */
    if( mode == THIS_IS_SUSPEND_MODE ){
      /* This is under Suspended Mode */
      /* restart charger if possible */
      IO_SYS_INT_CTLR = 0x7; /* clear all bits ( especially for BATOK ) */
      offcharge_shutdown();
      CommonCheckOffChargeInEntrance(mode);
    }else{
      /* BATOK fail. Do Force Sleep */
      IO_IRIS1_DEBUG_LED = 0xac; /* 10101100 */
      /* sleep again , as soon as possible */
      if( mode == THIS_IS_STANDBY_MODE ){
	emerge_off_common();
      }
      goto set_registers_and_suspend;
    }
  }
  if( ( fpga_stat & bitWSRC7 ) && ( first_key_release_waiting == 0 ) ){
    /* key press ... check key , and wait hold-timeout if necessary */
    int save_kbdmr;
    int onetime = 0;
    save_kbdmr = IO_KBDMR;
    IO_KBDMR &= ~(1 << 12); /* keyboard mode */
    if( mode == THIS_IS_STANDBY_MODE ){
    check_standby_pressed:
      IO_KBKSR = 1; /* all pins low , and discharge */
      udelay(100); /* wait discharge delay */
      IO_KBKSR = 6; /* activate KBCOL2 , which has Display key at RAW5 */
      udelay(20); /* wait discharge delay */
      if( IO_PADR & ( 1 << 5 ) ){ /* check RAW5 */
	/* Disp Key Is Pressed. But Check Again */
	udelay(100); /* wait a moment */
	IO_KBKSR = 1; /* all pins low , and discharge */
	udelay(20); /* wait discharge delay */
	IO_KBKSR = 6; /* activate KBCOL2 , which has Display key at RAW5 */
	udelay(20); /* wait discharge delay */
	if( IO_PADR & ( 1 << 5 ) ){ /* check RAW5 */
	  /* Disp Key Is Pressed. Go Resume ! */
	  extern int wakeup_from_disp_and_ignore_first_disp;
	  wakeup_from_disp_and_ignore_first_disp = 1;
	  IO_KBDMR = save_kbdmr;
	  apm_wakeup_factor |= WAKEUP_ANYKEY | WAKEUP_ST_DISP;
	  goto do_resume;
	}
      }
      /* standby (LCD-off) should be resumed by 'Phone' key , not hold */
      IO_KBKSR = 1; /* all pins low , and discharge */
      udelay(100); /* wait discharge delay */
      IO_KBKSR = 4; /* activate KBCOL0 , which has Phone(Power) key at RAW5 */
      udelay(20); /* wait activate delay */
      if( IO_PADR & ( 1 << 5 ) ){ /* check RAW5 */
	/* Power Key Is Pressed. But Check Again ! */
	udelay(100);
	IO_KBKSR = 1; /* all pins low , and discharge */
	udelay(20); /* wait discharge delay */
	IO_KBKSR = 4; /* activate KBCOL0 , which has Phone(Power) key at RAW5 */
	udelay(20); /* wait activate delay */
	if( IO_PADR & ( 1 << 5 ) ){ /* check RAW5 */
	  /* Power Key Is Pressed. Go Resume ! */
	  extern int wakeup_from_phone_and_ignore_first_phone;
	  IO_KBDMR = save_kbdmr;
	  wakeup_from_phone_and_ignore_first_phone = 1;
	  apm_wakeup_factor |= WAKEUP_ANYKEY | WAKEUP_ST_PHONE;
	  goto do_resume;
	}
      }
      IO_KBKSR = 1; /* all pins low , and discharge */
      udelay(100); /* wait discharge delay */
      IO_KBKSR = 11; /* activate KBCOL7 , which has Main(Home) key at RAW5 */
      udelay(20); /* wait activate delay */
      if( IO_PADR & ( 1 << 5 ) ){ /* check RAW5 */
	/* Main Key Is Pressed. But Check Again ! */
	udelay(100);
	IO_KBKSR = 1; /* all pins low , and discharge */
	udelay(20); /* wait discharge delay */
	IO_KBKSR = 11; /* activate KBCOL7 , which has Main(Home) key at RAW5 */
	udelay(20); /* wait activate delay */
	if( IO_PADR & ( 1 << 5 ) ){ /* check RAW5 */
	  /* Main Key Is Pressed. Go Resume ! */
	  /* FIX ME ---- DO SOMETHING ---- */
	  IO_KBDMR = save_kbdmr;
	  apm_wakeup_factor |= WAKEUP_ANYKEY | WAKEUP_ST_MAIN;
	  goto do_resume;
	}
      }
      /* end of check for STANDBY mode */
    }else{
      /* this is check for SUSPEND mode */
    check_phone_pressed:
      IO_KBKSR = 1; /* all pins low , and discharge */
      udelay(10); /* wait discharge delay */
      IO_KBKSR = 4; /* activate KBCOL0 , which has Phone(Power) key at RAW5 */
      udelay(10); /* wait activate delay */
      if( IO_PADR & ( 1 << 5 ) ){ /* check RAW5 */
	int c;
	/* Power Key Is Pressed. Check for hold-timeout */
	IO_KBKSR = 1; /* all pins low , and discharge */
	for(c=0;c<2000;c++) udelay(1000); /* wait for 1sec */
	IO_KBKSR = 4; /* activate KBCOL0 , which has Phone(Power) key at RAW5 */
	udelay(10); /* wait activate delay */
	if( IO_PADR & ( 1 << 5 ) ){ /* check RAW5 */
	  extern int wakeup_from_phone_and_ignore_first_phone;
	  /* If Pressed , Go Resume ! */
	  wakeup_from_phone_and_ignore_first_phone = 1;
	  /* check if Main+Phone is intended */
	  IO_KBKSR = 1; /* all pins low , and discharge */
	  udelay(10); /* wait discharge delay */
	  IO_KBKSR = 11; /* activate KBCOL7 , which has Main(Home) key at RAW5 */
	  udelay(10); /* wait activate delay */
	  if( IO_PADR & ( 1 << 5 ) ){ /* check RAW5 */
	    /* Activated. Main+Phone is intended */
	    /* FIX ME ---- DO SOMETHING ---- */
	    apm_wakeup_factor |= WAKEUP_PDA;
	  }else{
	    apm_wakeup_factor |= WAKEUP_PHONE;
	  }
	  /* Go Resume Now ! */
	  apm_wakeup_factor |= WAKEUP_ANYKEY;
	  IO_KBDMR = save_kbdmr;
	  goto do_resume;
	}
      }else{
	if( onetime < 5 ){
	  /* Consider PHONE and ??? keys are roll-over.
	     wait for PHONE key for a while */
	  int i;
	  for(i=0;i<1000;i++) udelay(1000);
	  onetime++;
	  goto check_phone_pressed;
	}
      }
    }
    /* Other Keys are ignored */
    IO_KBKSR = 1; /* all pins low , and discharge */
    udelay(100); /* wait discharge delay */
    IO_KBDMR = save_kbdmr;
    /* wait for release ... */
    first_key_release_waiting = 1;
  }
  else if( ( fpga_stat & bitWSRC7 ) && first_key_release_waiting ){
    /* waiting for key-release done.... */
    first_key_release_waiting = 0;
  }
  /* these cases are all ignored */

 set_registers_and_suspend:
  return 0;

 do_resume:
  return 1;
}



/*
 *    Suspend / Standby Main Routine
 *
 */

int l7200_suspend(void)
{
	long sys_config_next;
	unsigned long fpga_stat;

	int gpo_suspend_save = 0;

	first_key_release_waiting = 0;

#ifdef OFFCHARGE_PROC_ON_POWER_C
	/* init */
	rtcirq_setting = NO_RTCIRQ;
	/* save RTCMR */
	save_rtcregs();
#endif /* OFFCHARGE_PROC_ON_POWER_C */

#ifndef NOT_COPY_XTIME_TO_RTCDR_ON_ENTERING_SUSPEND
	IO_RTCDR = xtime.tv_sec;
#endif /* NOT_COPY_XTIME_TO_RTCDR_ON_ENTERING_SUSPEND */
	/* save register */
	CommonSaveRegisters();

#ifdef OFFCHARGE_PROC_ON_POWER_C
	CommonCheckOffChargeInEntrance(THIS_IS_SUSPEND_MODE);
#endif /* OFFCHARGE_PROC_ON_POWER_C */

#if 1
 check_current_keypress_and_set_ignore:
	CommonCheckCurrentKeyPress(THIS_IS_SUSPEND_MODE);
	CommonCheckCurrentExternalPorts();
#endif

 set_registers_and_suspend:
     	/* clear all interrupt enables */
	IO_IRQENABLECLEAR = 0xffffffff;
	IO_FIQENABLECLEAR = 0xffffffff;
	IO_IRQHENABLECLEAR = 0xffffffff;

	disable_fpga_irq(0x3ff);
	clear_fpga_int(0x3ff);

	CommonSetUpGAInterruptBeforeSuspend(THIS_IS_SUSPEND_MODE);


	clear_fpga_int(0x3ff);

	CommonEnableGAInterruptBeforeSuspend(THIS_IS_SUSPEND_MODE);
	CommonSetUpRTCInterruptBeforeSuspend(THIS_IS_SUSPEND_MODE);

	/* keyboard */
	IO_KBSBSR = ( 1 << 0 ) | ( 1 << 7 ); /* COL0(Phone) , COL7(Main) are activated on suspend */
	IO_KBDMR = (1 << 12); /* GPIO Mode */
	IO_KBDR = ( 1 << 0 ) | ( 1 << 7 ); /* COL0(Phone) , COL7(Main) are activated as GPIO */

	/* force Cut GPO */
	gpo_suspend_save = IO_FPGA_GPO & GPO_FORCE_DISABLE_BITS;
	IO_FPGA_GPO &= ~(GPO_FORCE_DISABLE_BITS);

	/* enable nINTF and RTCALARM */
	IO_IRQENABLESET = (1 << IRQ_INTF) | (1 << IRQ_RTC_ALARM);


	if( IO_FPGA_Status & bitWSRC0 ){
	  IO_FPGA_ECL |= bitWSRC0;  /* clear WSRC0 edge int. */
	  IO_FPGA_CLEX = bitWSRC0;  /* clear WSRC0 edge int. */
	}


#if 1 /* 01.07.19 part2 */
	if( IO_IRQSTATUS ){
	  IO_FPGA_GPO |= gpo_suspend_save;
	  goto chip_resumed_to_run_mode;
	}
#endif

	/* Into standby mode */
	sys_config_next = IO_SYS_CONFIG_NEXT;
	sys_config_next &= ~(PMU_TRANSOP_MASK | PMU_CLOCK_RECOVERY_MASK 
				| PMU_INTRET | PMU_OSCEN | PMU_PLLEN 
				| PMU_SYSCLKEN | PMU_SDRB_SEL);
	sys_config_next |= (PMU_TRANSOP(3) | PMU_INTRET 
				//| PMU_OSCEN 
				| PMU_CLOCK_RECOVERY(0x7f));
	IO_SYS_CONFIG_NEXT = sys_config_next;
	flush_cache_all();
	//sti();
	IO_SYS_CONFIG_COMM = 0;
	//cli();

 chip_resumed_to_run_mode:

	if( IO_FPGA_Status & bitWSRC0 ){
	  IO_FPGA_ECL |= bitWSRC0;  /* clear WSRC0 edge int. */
	  IO_FPGA_CLEX = bitWSRC0;  /* clear WSRC0 edge int. */
	}

	/* Resume */
	fpga_stat = IO_FPGA_Status;
	{
	  udelay(20);
	  fpga_stat = IO_FPGA_Status;
	}

	disable_fpga_irq(0x3ff);

	/* restore GPO */
	IO_FPGA_GPO |= gpo_suspend_save;

 check_battery_cover_sw:
	if( CommonCheckBatteryCoverIsClosed(THIS_IS_SUSPEND_MODE) ){
	  /* Battery Cover Closed */
	}else{
	  /* Battery Cover Open */
	  goto set_registers_and_suspend;
	}

	apm_wakeup_factor = 0;

 check_wakeup_src:
	if( CommonCheckWakeupSrcAndCheckIfResume(THIS_IS_SUSPEND_MODE,fpga_stat) ){
	  if( apm_wakeup_factor & WAKEUP_BATCRIT ){
	    goto do_resume;
	  }else if( CommonCheckIfBatteryStatusIsEnough() ){
	    goto do_resume;
	  }else{
	    IO_IRIS1_DEBUG_LED = 0x12;
	    goto set_registers_and_suspend;
	  }
	}else{
	  goto set_registers_and_suspend;
	}

 do_resume:

	IO_SYS_INT_CTLR = 0x7; /* clear all bits ( especially for BATOK ) */


	CommonRestoreRegisters();
	xtime.tv_sec = IO_RTCDR;

	clear_fpga_int(0x3ff);
	enable_fpga_irq(save_regs.fpga_enable);

#ifdef OFFCHARGE_PROC_ON_POWER_C
	/* finish off-charge forcelly */
	offcharge_shutdown();
	/* restore RTCMR */
	restore_rtcregs();
#endif /* OFFCHARGE_PROC_ON_POWER_C */


	return 0;
}


int l7200_standby(void)
{
	long sys_config_next;
	unsigned long fpga_stat;

	int gpo_suspend_save = 0;

	first_key_release_waiting = 0;

#ifdef OFFCHARGE_PROC_ON_POWER_C
	/* init */
	rtcirq_setting = NO_RTCIRQ;
	/* save RTCMR */
	save_rtcregs();
#endif /* OFFCHARGE_PROC_ON_POWER_C */


#ifndef NOT_COPY_XTIME_TO_RTCDR_ON_ENTERING_SUSPEND
	IO_RTCDR = xtime.tv_sec;
#endif /* NOT_COPY_XTIME_TO_RTCDR_ON_ENTERING_SUSPEND */
	/* save register */
	CommonSaveRegisters();

#ifdef OFFCHARGE_PROC_ON_POWER_C
	CommonCheckOffChargeInEntrance(THIS_IS_STANDBY_MODE);
#endif /* OFFCHARGE_PROC_ON_POWER_C */

#if 1
 check_current_keypress_and_set_ignore:
	CommonCheckCurrentKeyPress(THIS_IS_STANDBY_MODE);
	{
	  int save_kbdmr;
	  save_kbdmr = IO_KBDMR;
	  IO_KBDMR &= ~(1 << 12); /* keyboard mode */
	  IO_KBKSR = 1; /* all pins low , and discharge */
	  udelay(100); /* wait discharge delay */
	  IO_KBDMR = save_kbdmr;
	}
	CommonCheckCurrentExternalPorts();
#endif

 set_registers_and_suspend:
     	/* clear all interrupt enables */
	IO_FIQENABLECLEAR = 0xffffffff;
	IO_IRQENABLECLEAR = 0xffffffff;
	IO_IRQHENABLECLEAR = 0xffffffff;

	disable_fpga_irq(0x3ff);
	clear_fpga_int(0x3ff);

	CommonSetUpGAInterruptBeforeSuspend(THIS_IS_STANDBY_MODE);

	clear_fpga_int(0x3ff);

	CommonEnableGAInterruptBeforeSuspend(THIS_IS_STANDBY_MODE);
	CommonSetUpRTCInterruptBeforeSuspend(THIS_IS_STANDBY_MODE);

	/* keyboard */
	IO_KBSBSR = ( 1 << 0 ) | ( 1 << 2 ) | ( 1 << 7 ); /* COL0(Phone) , COL2(Disp) , COL7(Main) are activated on suspend */
	IO_KBDMR = (1 << 12); /* GPIO Mode */
	IO_KBDR = ( 1 << 0 ) | ( 1 << 2 ) | ( 1 << 7 ); /* COL0(Phone) , COL2(Disp) , COL7(Main) are activated as GPIO */


	/* force Cut GPO */
	gpo_suspend_save = IO_FPGA_GPO & GPO_FORCE_DISABLE_BITS;
	IO_FPGA_GPO &= ~(GPO_FORCE_DISABLE_BITS);
	
	/* enable nINTF and RTCALARM */
	IO_IRQENABLESET = (1 << IRQ_INTF) | (1 << IRQ_RTC_ALARM) | ( 1 << IRQ_BAT_LO );

	if( IO_FPGA_Status & bitWSRC0 ){
	  IO_FPGA_ECL |= bitWSRC0;  /* clear WSRC0 edge int. */
	  IO_FPGA_CLEX = bitWSRC0;  /* clear WSRC0 edge int. */
	}

#if 1 /* 01.07.19 part2 */
	if( IO_IRQSTATUS ){
	  IO_FPGA_GPO |= gpo_suspend_save;
	  goto chip_resumed_to_run_mode;
	}
#endif

	/* Into standby mode */
	sys_config_next = IO_SYS_CONFIG_NEXT;
	sys_config_next &= ~(PMU_TRANSOP_MASK | PMU_CLOCK_RECOVERY_MASK 
				| PMU_INTRET | PMU_OSCEN | PMU_PLLEN 
				| PMU_SYSCLKEN | PMU_SDRB_SEL);
	sys_config_next |= (PMU_TRANSOP(3) | PMU_INTRET 
				//| PMU_OSCEN 
				| PMU_CLOCK_RECOVERY(0x7f));
	IO_SYS_CONFIG_NEXT = sys_config_next;
	flush_cache_all();
	//sti();
	IO_SYS_CONFIG_COMM = 0;
	//cli();

 chip_resumed_to_run_mode:

	if( IO_FPGA_Status & bitWSRC0 ){
	  IO_FPGA_ECL |= bitWSRC0;  /* clear WSRC0 edge int. */
	  IO_FPGA_CLEX = bitWSRC0;  /* clear WSRC0 edge int. */
	}

	/* Resume */
	fpga_stat = IO_FPGA_Status;
	{
	  udelay(20);
	  fpga_stat = IO_FPGA_Status;
	}

	disable_fpga_irq(0x3ff);

	/* restore GPO */
	IO_FPGA_GPO |= gpo_suspend_save;

 check_battery_cover_sw:
	if( IO_IRQSTATUS & ( 1 << IRQ_BAT_LO ) ){
	  /* BATOK failed while CPU is sleeping... So, enter eternal standby
	     state and wait for user-reset */
	  IO_IRIS1_DEBUG_LED = 0xee; /* 10101100 */
	  /* =========== Call Common Shutdown Call ========== */
	  emerge_off_common();
	}

	apm_wakeup_factor = 0;

	/* ============== FUX-ME !!!!!  Battery Cover Open While Standby
	   must resume and turn off ================= */

	if( CommonCheckBatteryCoverIsClosed(THIS_IS_STANDBY_MODE) ){
	  /* Battery Cover Closed */
	}else{
	  /* Battery Cover Open */
	  apm_wakeup_factor |= WAKEUP_BATCOV;
	  /* goto do_resume; */ /* <========= is this true ? ======== */
	  goto set_registers_and_suspend;
	}


 check_wakeup_src:

	if( CommonCheckWakeupSrcAndCheckIfResume(THIS_IS_STANDBY_MODE,fpga_stat) ){
	  if( apm_wakeup_factor & WAKEUP_BATCRIT ){
	    goto do_resume;
	  }else if( CommonCheckIfBatteryStatusIsEnough() ){
	    goto do_resume;
	  }else{
	    goto set_registers_and_suspend;
	  }
	}else{
	  goto set_registers_and_suspend;
	}

 do_resume:

	IO_SYS_INT_CTLR = 0x7; /* clear all bits ( especially for BATOK ) */

	CommonRestoreRegisters();
	xtime.tv_sec = IO_RTCDR;

	clear_fpga_int(0x3ff);
	enable_fpga_irq(save_regs.fpga_enable);

#ifdef OFFCHARGE_PROC_ON_POWER_C
	/* finish off-charge forcelly */
	offcharge_shutdown();
	/* restore RTCMR */
	restore_rtcregs();
#endif /* OFFCHARGE_PROC_ON_POWER_C */

	return 0;
}



/*
 *   BATOK Emergency OFF Routine
 *
 */

static void emerge_off_common(void)
{
  /* =========== Going Down System IRQ ... ============= */
  IO_IRQENABLECLEAR = 0xffffffff; /* disable all irq */
  IO_IRQHENABLECLEAR = 0xffffffff; /* disable all irq */ 
  IO_FIQENABLECLEAR = 0xffffffff; /* disable all irq */
  barrier();
  /* =========== Disable GA IRQ ======== */
  disable_fpga_irq(0x3ff);
  clear_fpga_int(0x3ff);
  /* =========== save RTC ================ */
   /* this should not be done. because , xtime.tv_sec may not be initialized */
  //IO_RTCDR = xtime.tv_sec;
  //barrier();
  /* =========== force current DMA off ========== */
  IO_DMA_TERM = 0xff; /* terminate at the end of current transfer */
  barrier();
  /* =========== Going Down GPIOs ========= */
  IO_PASBSR = 0x0;
  IO_PBSBSR = 0x0;
  IO_PCSBSR = 0x0; /* 6,7 : RF */
  //IO_PDSBSR = 0x0; /* not necessary. */
  IO_PEDDR = ( bitPE0 | bitPE1 | bitPE2 | bitPE3 | bitPE4 );
  IO_PESBSR = (MMCPOWER_BIT|TABLET_CS_BIT|THERM_STB_BIT); /* 2: MMC , 3,4 : adc */
  /* =========== Going Down Audio ========== */
  IO_SICR0 = 0x0; /* clear SICR0_ENB ( but... reset ALL ! ) */
  barrier();
  /* =========== Going Down Buzzer ========== */
  IO_TIMER1CONTROL = 0x0; /* clear TIMER_ENABLE and BUZZER_MODE_UNDERFLOW ( but... reset ALL ! ) */
  barrier();
  IO_TIMER1CONTROL = 0x0; /* set BZTOG bit to 0 */
  barrier();
  /* =========== Going Down MMC ============== */
  IO_MMCCT = 0x0; /* clear MMCCT_ENABLE and MMCCT_PUSHPULL ( but... reset ALL ! ) */
  barrier();
  /* =========== Go Down ALL bits on GA ========== */
  IO_FPGA_GPO = 0x0; /* clear all bits to shutoff LCD and front-light */
  IO_FPGA_PANEL = 0x3;
  barrier();
  /* =========== Going Down All Keyboard ... ============= */
  //IO_KBKSR = 0x1; /* all pins low */
#ifdef KEY_CHECK_AND_REBOOT_AFTER_BATOK
  IO_IRQENABLESET = (1 << IRQ_INTF);
  IO_KBSBSR = ( 1 << 0 ); /* COL0(Phone) are activated on suspend */
  set_fpga_int_trigger_highlevel(bitWSRC7);
  enable_fpga_irq(bitWSRC7);
#else /*KEY_CHECK_AND_REBOOT_AFTER_BATOK */
  IO_KBSBSR = 0x0; /* all pins low on standby */
#endif /*KEY_CHECK_AND_REBOOT_AFTER_BATOK */
  barrier();
  /* ======== Mark Up Sign =========== */
  IO_IRIS1_DEBUG_LED = 0x0f;
  /* ======== Going To Standby-mode ======== */
  while(1){ /* sleep on eternally unless reset ... */
    IO_IRIS1_DEBUG_LED = 0x0c;
    IO_SYS_INT_CTLR = 0x7; /* clear all bits ... but... this is not so usable. */
    IO_SYS_CONFIG_NEXT = ( IO_SYS_CONFIG_CURRENT & ~(0x7ff0160) ) | 0x6fc0000;
    barrier();
    IO_SYS_CONFIG_COMM = 0xffffffff;
    barrier();
    __asm__(
	  "nop" "\n"
	  "nop" "\n"
	  "nop" "\n"
	  "nop" "\n"
	  "nop" "\n"
	  "nop" "\n"
	  );
    IO_IRIS1_DEBUG_LED = 0x0d;
    barrier();

#ifdef KEY_CHECK_AND_REBOOT_AFTER_BATOK
    {
      IO_KBDMR &= ~(1 << 12); /* keyboard mode */
      IO_KBKSR = 1; /* all pins low , and discharge */
      udelay(10); /* wait discharge delay */
      IO_KBKSR = 4; /* activate KBCOL0 , which has Phone(Power) key at RAW5 */
      udelay(10); /* wait activate delay */
      if( IO_PADR & ( 1 << 5 ) ){ /* check RAW5 */
	int c;
	/* Power Key Is Pressed. Check for hold-timeout */
	IO_KBKSR = 1; /* all pins low , and discharge */
	for(c=0;c<2000;c++) udelay(1000); /* wait for 1sec */
	IO_KBKSR = 4; /* activate KBCOL0 , which has Phone(Power) key at RAW5 */
	udelay(10); /* wait activate delay */
	if( IO_PADR & ( 1 << 5 ) ){ /* check RAW5 */
	  IO_KBKSR = 1; /* all pins low , and discharge */
	  udelay(10); /* wait discharge delay */
	  goto do_reboot;
	}
      }
      IO_KBKSR = 1; /* all pins low , and discharge */
      udelay(10); /* wait discharge delay */
    }
#endif /*KEY_CHECK_AND_REBOOT_AFTER_BATOK */
  }
 do_reboot:
#define MMCCT_ENABLE      0x01
#define SYSCLOCK_UARTM_EN ((__u32)0x80)
#define SYSCLOCK_MIRM_EN  ((__u32)0x40)
#define SYSCLOCK_SIBADC_EN  ((__u32)0x100)
  IO_MMCCT &= ~MMCCT_ENABLE;
  IO_SYS_CLOCK_ENABLE &= ~SYSCLOCK_UARTM_EN;
  IO_SYS_CLOCK_ENABLE &= ~SYSCLOCK_MIRM_EN;
  IO_SYS_CLOCK_ENABLE &= ~SYSCLOCK_SIBADC_EN;
  IO_FPGA_PANEL = 0x3;
  machine_restart(NULL);
}

void emerge_off(void)
{
  IO_IRIS1_DEBUG_LED = 0x10;

  /* =========== Going Down CPU IRQ ======= */
  __asm__(
	  "mov r0,#0x13+0x80+0x40" "\n"
	  "msr cpsr,r0" "\n"
          );
  barrier();
  /* =========== Call Common Shutdown Call ========== */
  emerge_off_common();
}

#ifdef USE_INT0_INTERRUPT

#define INT0_INTERVAL_THRESHOLD  (HZ/2)  /* 0.5sec */

static void power_int0_handler(int irq, void *dev_id, struct pt_regs *regs)
{
  static int int0_occurred = 0;
  static unsigned long oldval = 0;
  static unsigned long diff = 0;
  disable_irq(IRQ_INT0);
  if( ! int0_occurred ){
    oldval = jiffies; /* save current value */
    int0_occurred = 1;
    enable_irq(IRQ_INT0);
    return;
  }
  diff = jiffies - oldval;
  if( diff < INT0_INTERVAL_THRESHOLD ){
    /* Critical Condition Occurred */
    int0_occurred = 0;
    emerge_off();
    return;
  }
  /* re-init counter */
  oldval = jiffies; /* save current value */
  int0_occurred = 1;
  enable_irq(IRQ_INT0);
  return;
}

#endif /* USE_INT0_INTERRUPT */

EXPORT_SYMBOL(emerge_off);

/*
 *   FIQ register
 */

#define USE_BATOK_FIQ

#ifdef USE_BATOK_FIQ

extern unsigned char iris_batok_fiqhandler,iris_batok_fiqhandler_end;
 
static void enable_fiq_batok(void)
{
  unsigned long flags;
  printk("enable_fiq_batok start\n");
  save_flags(flags); cli();
  IO_FIQENABLECLEAR = 0xffffffff;
  IO_FIQENABLESET = ( 1 << IRQ_BAT_LO );
  restore_flags(flags);
  printk("enable_fiq_batok done\n");
}

#endif /* USE_BATOK_FIQ */


/*
 * operations....
 */

#include <linux/miscdevice.h>

#define IRIS_FORCE_POWER_CUT 0x10

static int do_ioctl(struct inode *inode,struct file *file,
		    unsigned int cmd,unsigned long arg)
{
  switch( cmd ) {
  case IRIS_FORCE_POWER_CUT:
    {
      emerge_off();
      /* never returns ! */
    }
    break;
  default:
    return -EINVAL;
  }
  return 0;
}

static int do_open(struct inode *inode, struct file *filp)
{
  return 0;
}

static int do_release(struct inode *inode, struct file *filp)
{
  return 0;
}

#ifndef POWERCUT_MINOR_DEV
#define POWERCUT_MINOR_DEV  190
#endif /* ! POWERCUT_MINOR_DEV */

static struct file_operations iris_powercut_fops = {
  owner:	THIS_MODULE,
  ioctl:	do_ioctl,
  open:		do_open,
  release:	do_release,
};

static struct miscdevice powercut_device = {
  POWERCUT_MINOR_DEV,
  "powercut",
  &iris_powercut_fops
};


#else	// CONFIG_IRIS

int l7200_suspend(void)
{
	long sys_config_next;

	/* Into deep sleep mode */
	sys_config_next = IO_SYS_CONFIG_NEXT;
	sys_config_next &= ~(PMU_TRANSOP_MASK | PMU_CLOCK_RECOVERY_MASK 
				| PMU_INTRET | PMU_OSCEN | PMU_PLLEN 
				| PMU_SYSCLKEN | PMU_SDRB_SEL);
	sys_config_next |= (PMU_TRANSOP(3) | PMU_CLOCK_RECOVERY(0x7f));
	IO_SYS_CONFIG_NEXT = sys_config_next;
	flush_cache_all();
	IO_SYS_CONFIG_COMM = 0;

	return 0;
}
#endif	// CONFIG_IRIS

int l7200_idle(void)
{
	long sys_config_next;

	/* Into idle mode */
	sys_config_next = IO_SYS_CONFIG_NEXT;
	sys_config_next &= ~(PMU_TRANSOP_MASK | PMU_CLOCK_RECOVERY_MASK 
				| PMU_INTRET | PMU_OSCEN | PMU_PLLEN 
				| PMU_SYSCLKEN | PMU_SDRB_SEL);
	sys_config_next |= (PMU_TRANSOP(1) | PMU_INTRET 
				| PMU_OSCEN | PMU_PLLEN | PMU_SYSCLKEN
				| PMU_CLOCK_RECOVERY(0x7f));
	IO_SYS_CONFIG_NEXT = sys_config_next;
	IO_SYS_CONFIG_COMM = 0;

	return 0;
}

int pm_do_suspend(void)
{
	int retval;
	
	DPRINTK("pm_do_suspend\n");
	
	retval = pm_send_all(PM_SUSPEND, (void *)2);
	if (retval) 
		return retval;

	retval = l7200_suspend();

	retval = pm_send_all(PM_RESUME, (void *)0);
	if (retval)
		return retval;

	return 0;
}

EXPORT_SYMBOL(l7200_suspend);
EXPORT_SYMBOL(l7200_idle);
EXPORT_SYMBOL(pm_do_suspend);


static struct ctl_table pm_table[] = 
{
	{ACPI_S1_SLP_TYP, "suspend", NULL, 0, 0600, NULL, &pm_do_suspend},
	{0}
};

static struct ctl_table pm_dir_table[] =
{
	{CTL_ACPI, "pm", NULL, 0, 0555, pm_table},
	{0}
};

/*
 * Initialize power interface
 */
static int __init pm_init(void)
{
	register_sysctl_table(pm_dir_table, 1);
#ifdef CONFIG_IRIS
	/* this registration can be done in init/main.c. */
	if(0){
	  int err;
	  err = request_irq(IRQ_BAT_LO,emerge_off , SA_INTERRUPT, "batok", NULL);
	  if( err ){
	    printk("batok install error %d\n",err);
	  }else{
	    enable_irq(IRQ_BAT_LO);
	    printk("batok installed 2\n");
	  }
	}
	/* register powercut device */
	misc_register(&powercut_device);

	printk("batok FIQ installing\n");
	enable_fiq_batok();
	printk("batok FIQ installed\n");

#ifdef USE_INT0_INTERRUPT
	{
	  int err;
	  err = request_irq(IRQ_INT0,power_int0_handler, SA_INTERRUPT, "int0", NULL);
	  if( err ){
	    printk("int0 install error %d\n",err);
	  }else{
	    enable_irq(IRQ_INT0);
	    printk("batok installed 2\n");
	  }
	}
#endif /* USE_INT0_INTERRUPT */

#endif /* CONFIG_IRIS */
	return 0;
}

__initcall(pm_init);


