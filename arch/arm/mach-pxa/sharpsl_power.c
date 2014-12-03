/*
 * XScale Power Management Routines
 *
 * (C) Copyright 2001 Lineo Japan, Inc.
 *
 * May be copied or modified under the terms of the GNU General Public
 * License.  See linux/COPYING for more information.
 *
 * Based on
 *  linux/arch/arm/mach-pxa/discovery_power.c
 *  linux/arch/arm/mach-sa1100/power.c
 *
 * Copyright (c) 2001 Cliff Brake <cbrake@accelent.com>
 *
 * This program is free software; you can redistribute it and/or 
 * modify it under the terms of the GNU General Public License.
 *
 * History:
 *
 * 2001-02-06: Cliff Brake         Initial code
 *
 * 2001-02-25: Sukjae Cho <sjcho@east.isi.edu> & 
 * 			   Chester Kuo <chester@linux.org.tw>
 * 			   Save more value for the resume function! Support
 * 			   Bitsy/Assabet/Freebird board
 *
 * Change Log
 *	12-Nov-2001 Lineo Japan, Inc.
 *      14-Nov-2001 SHARP 
 *	13-Jul-2002 SHARP    OSMR0 is set to OSCR + 10msec in sabinal_suspend()
 *	31-Jul-2002 Lineo Japan, Inc.  for ARCH_PXA
 *	27-AUG-2002 Steve Lin for Discovery
 *	12-Dec-2002 Lineo Japan, Inc.
 *      12-Dec-2002 Sharp Corporation for Poodle and Corgi
 *	16-Jan-2003 SHARP sleep_on -> interruptible_sleep_on
 *      28-Nov-2003 Sharp Corporation for Tosa
 *      26-Feb-2004 Lineo Solutions, Inc.  for Tosa
 *
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

//#define WUPSRC_DEBUG
#ifdef WUPSRC_DEBUG
unsigned int debug1,debug2,debug3,debug4;
#endif

#include <linux/config.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/pm.h>
#include <linux/slab.h>
#include <linux/sysctl.h>
#include <linux/acpi.h>
#include <linux/delay.h>

#include <asm/mach-types.h>
#include <asm/hardware.h>
#include <asm/memory.h>
#include <asm/system.h>
#include <asm/proc-fns.h>

#ifdef CONFIG_SABINAL_DISCOVERY
#include <linux/unistd.h>
#include <asm/irq.h>
#include <asm/arch/irqs.h>
#include <asm/arch/discovery_asic3.h>
#include <asm/sharp_apm.h>
#else
#include <asm/arch/irqs.h>
#include <asm/hardirq.h>
#endif


#include <asm/sharp_char.h>
#include <asm/sharp_keycode.h>
#if defined(CONFIG_ARCH_PXA_POODLE)
#include <asm/arch/keyboard_poodle.h>
#elif defined(CONFIG_ARCH_PXA_CORGI)
#include <asm/arch/keyboard_corgi.h>
#elif defined(CONFIG_ARCH_PXA_TOSA)
#include <asm/arch/keyboard_tosa.h>
#include <linux/ac97_codec.h>
#endif

#ifdef LOGICAL_WAKEUP_SRC
#include <asm/arch/sharpsl_wakeup.h>
unsigned long logical_wakeup_src_mask = IDPM_WAKEUP_REC|IDPM_WAKEUP_SYNC;
#endif
#include "sharpsl_param.h"


static struct {
        u32 osmr0, osmr1, osmr2, osmr3, oscr;
        u32 ower, oier;

        u32 gpdr0, gpdr1, gpdr2, grer0, grer1, grer2, gfer0, gfer1, gfer2;
	u32 gafr0_l, gafr1_l, gafr2_l, gafr0_u, gafr1_u, gafr2_u;
	u32 gplr0, gplr1, gplr2;
        u32 icmr, iclr, iccr;
        u32 mecr, cken;
#ifndef CONFIG_ARCH_SABINAL
	u32 mcmem0, mcmem1, mcatt0, mcatt1, mcio0, mcio1;
#endif
		/* FIXME this part should in the serial port driver ,Control register*/
	u32 ffier, fflcr, ffmcr, ffspr, ffisr, ffdll, ffdlh;
	u32 lcm_tasd, lcm_spict, lcm_gpo;
        u32 lcm_gpe, lcm_spimd;
        u32 scp_gpwr;
        
        u16 pstsa,pstsb,pstsc,pstsd;
} sys_ctx;

extern unsigned long *sleep_param;	/* virtual address */
extern unsigned long  sleep_param_p;	/* physical address */

extern void cpu_pxa_resume(void);	/* function the bootloader will call back */

extern int cpu_pxa_do_suspend(void);
extern unsigned short chkFatalBatt(void);
extern int sharpsl_off_charge_battery(void);
extern int charge_status;

#if defined(CONFIG_ARCH_PXA_POODLE)
void pxa_ssp_init(void);
#elif defined(CONFIG_ARCH_PXA_TOSA)
void tosa_ac97_init(void);
int pxa_ac97_get(struct ac97_codec **codec, unsigned char *ac97_on);
static unsigned char ac97_on = 0;
#endif
#if defined(CONFIG_ARCH_PXA_POODLE)
#define PWER_RTC	0x80000000
#define R_WAKEUP_SRC	(GPIO_bit(GPIO_AC_IN) | GPIO_bit(GPIO_CF_STSCHG) /*|
			 GPIO_bit(GPIO_CHRG_FULL) */ )
#define F_WAKEUP_SRC	(GPIO_bit(GPIO_ON_KEY) | GPIO_bit(GPIO_WAKEUP) | \
			 GPIO_bit(GPIO_AC_IN) | GPIO_bit(GPIO_HP_IN) | \
			 GPIO_bit(GPIO_GA_INT)| GPIO_bit(GPIO_nSD_INT) )
#define WAKEUP_SRC	( R_WAKEUP_SRC | F_WAKEUP_SRC | PWER_RTC )
#define WAKEUP_DEF_SRC	( GPIO_bit(GPIO_AC_IN) | GPIO_bit(GPIO_ON_KEY) | \
			  PWER_RTC | GPIO_bit(GPIO_WAKEUP) | GPIO_bit(GPIO_GA_INT) )
#elif defined(CONFIG_ARCH_PXA_CORGI)
#define PWER_RTC	0x80000000
#if defined(CONFIG_ARCH_PXA_SHEPHERD)
#define R_WAKEUP_SRC	(GPIO_bit(GPIO_AC_IN) | GPIO_bit(GPIO_AK_INT) | GPIO_bit(GPIO_MAIN_BAT_LOW))
#else
#define R_WAKEUP_SRC	(GPIO_bit(GPIO_AC_IN) | GPIO_bit(GPIO_AK_INT))
#endif
#define F_WAKEUP_SRC	(GPIO_bit(GPIO_KEY_INT) | GPIO_bit(GPIO_WAKEUP) | \
			 GPIO_bit(GPIO_AC_IN) | GPIO_bit(GPIO_MAIN_BAT_LOW))
#define WAKEUP_SRC	( R_WAKEUP_SRC | F_WAKEUP_SRC | PWER_RTC )
#define WAKEUP_DEF_SRC	( WAKEUP_SRC );

#elif defined(CONFIG_ARCH_PXA_TOSA)
/*
<F_WAKEUP_SRC>
GPIO_POWERON           (0)
GPIO_AC_IN             (2)
GPIO_RECORD_BTN        (3)
GPIO_SYNC              (4)
GPIO_USB_IN            (5)
GPIO_JACKET_DETECT     (7)
GPIO_nSD_DETECT        (9)
GPIO_nSD_INT           (10)
GPIO_BAT1_CRG          (12)
GPIO_CF_CD             (13)
GPIO_BAT0_CRG          (14)
*/
#define PWER_RTC	0x80000000
#define R_WAKEUP_SRC (GPIO_bit(GPIO_AC_IN) | GPIO_bit(GPIO_JACKET_DETECT))
#define F_WAKEUP_SRC	(GPIO_bit(GPIO_POWERON) | GPIO_bit(GPIO_RESET) | \
			 GPIO_bit(GPIO_AC_IN) | GPIO_bit(GPIO_RECORD_BTN) | \
			 GPIO_bit(GPIO_SYNC) | GPIO_bit(GPIO_USB_IN) | \
			 GPIO_bit(GPIO_JACKET_DETECT) )
#define WAKEUP_SRC	( R_WAKEUP_SRC | F_WAKEUP_SRC | PWER_RTC )
#define WAKEUP_DEF_SRC	( GPIO_bit(GPIO_POWERON) | GPIO_bit(GPIO_RESET) | \
			  GPIO_bit(GPIO_AC_IN) | GPIO_bit(GPIO_JACKET_DETECT) | \
			  GPIO_bit(GPIO_RECORD_BTN) | GPIO_bit(GPIO_SYNC) )
#endif

#if defined(CONFIG_SABINAL_DISCOVERY)
unsigned int apm_wakeup_src_mask = 0;
#else
unsigned int apm_wakeup_src_mask = WAKEUP_DEF_SRC;
u32 sharpsl_emergency_off = 0;
unsigned int sharpsl_chg_freq = 0x0145;
static DECLARE_WAIT_QUEUE_HEAD(wq_off);
int sharpsl_off_mode = 0;
int sharpsl_off_state = 0;
int pass_charge_flag = 0;
#endif
unsigned int apm_wakeup_factor = 0;
extern void sharpsl_battery_charge_hook(int mode);
#ifdef CONFIG_ARCH_PXA_CORGI
extern int corgi_wakeup_remocon_hook(void);
#endif

#if defined(CONFIG_ARCH_PXA_POODLE) || defined(CONFIG_ARCH_PXA_CORGI) || defined(CONFIG_ARCH_PXA_TOSA)
extern unsigned long cccr_reg;
static int sharpsl_alarm_flag;
#endif

//@@#if defined(CONFIG_ARCH_PXA_TOSA)
//@@extern int sharpsl_get_jacket_status(void);
//@@#endif

void PrintParamTable(void);

#ifdef CONFIG_SABINAL_DISCOVERY
static u32 alarm_enable=0;
#endif

#if defined(CONFIG_ARCH_PXA_SHEPHERD) || defined(CONFIG_ARCH_PXA_TOSA)
int sharpsl_restart(void)
{
	return sharpsl_restart_core(0);
}

int sharpsl_restart_nonstop(void)
{
	return sharpsl_restart_core(1);
}

int sharpsl_restart_core(int nonstop_flag)
{
	int flag = 1;
#if !defined(CONFIG_ARCH_PXA_TOSA)
	if (nonstop_flag) {
		SCP_REG_GPWR |= SCP_LED_GREEN;
	}
	else {
		SCP_REG_GPWR &= ~SCP_LED_GREEN;
	}
#endif
	RCSR = 0xf;

#if defined(CONFIG_ARCH_PXA_TOSA)
	if( nonstop_flag && ((MSC0 & 0xffff0000) == 0x7ff00000) )
	    MSC0 = (MSC0 & 0xffff)|0x7ee00000;
 						// GPIO reset
 	set_GPIO_mode(GPIO_ON_RESET | GPIO_OUT);
 	GPCR(GPIO_ON_RESET) |= GPIO_bit(GPIO_ON_RESET);
#else
	OSMR3 = OSCR+0x100;
	OWER = 0x01;
	OIER |= 0x08;

	while(1) {
		if ( flag++ > 0x20000000 ) break;
	}
#endif
	return 0;
}
#else
int sharpsl_restart(void)
{
	int flag = 1;

	RCSR = 0xf;

	OSMR3 = OSCR+0x100;
	OWER = 0x01;
	OIER |= 0x08;

	while(1) {
		if ( flag++ > 0x20000000 ) break;
	}

	return 0;
}
#endif

#ifdef CONFIG_SABINAL_DISCOVERY
static int discovery_wakeup_hook(void)
{
	
	ASIC3_GPIO_MASK_A = 0x1f8; //mask navigation key interrupt
	
	if (apm_wakeup_src_mask & WAKEUP_RTC) {
		alarm_enable = 1;			
	} else {
		alarm_enable = 0;
	}

	if (apm_wakeup_src_mask & WAKEUP_HOME_KEY) {
		ASIC3_GPIO_MASK_A &= ~0x2;	//0: enable
	} else {
		ASIC3_GPIO_MASK_A |= 0x2; //1: mask
	}

	if (apm_wakeup_src_mask & WAKEUP_OK_KEY) {
		ASIC3_GPIO_MASK_A &= ~0x4; 
	} else {
		ASIC3_GPIO_MASK_A |= 0x4; 
	}

	if (apm_wakeup_src_mask & WAKEUP_MENU_KEY) {
		ASIC3_GPIO_MASK_A &= ~0x1;
	} else {
		ASIC3_GPIO_MASK_A |= 0x1;
	}

	return 0;
}
#endif

#if !defined(CONFIG_SABINAL_DISCOVERY)

#if defined(CONFIG_ARCH_PXA_CORGI) || defined(CONFIG_ARCH_PXA_TOSA)
int sharpsl_wakeup_check_charge(void)
{
#if defined(CONFIG_ARCH_PXA_TOSA)
  unsigned int pfer;
  unsigned int prer;

  pfer = F_WAKEUP_SRC & apm_wakeup_src_mask & WAKEUP_SRC;
  prer = R_WAKEUP_SRC & apm_wakeup_src_mask & WAKEUP_SRC;
  //printk("pfer = %08x",pfer);
  //printk("prer = %08x",prer);

  pfer &= ~(F_WAKEUP_SRC & R_WAKEUP_SRC);
  prer &= ~(F_WAKEUP_SRC & R_WAKEUP_SRC);
  //printk("pfer = %08x",pfer);
  //printk("prer = %08x",prer);

  pfer = ~GPLR0 & pfer;
  prer = GPLR0 & prer;
  //printk("pfer = %08x",pfer);
  //printk("prer = %08x",prer);

  if(pfer != 0 || prer != 0){
    apm_wakeup_factor = pfer | prer;
    return 0; // wakeup
  }

  if (  ( ( RTAR - RCNR ) <  20 ) && ( RTSR & RTSR_ALE )  ) {
    return -1; // go off.
  }

  return 1; // continue.
#else
  unsigned int temp;

  temp = ~GPLR0 & ( GPIO_bit(GPIO_AC_IN) | GPIO_bit(GPIO_KEY_INT) | GPIO_bit(GPIO_WAKEUP) );
  if ( temp != 0 ) {
    apm_wakeup_factor = temp;
  }
  return temp;
#endif
}
#endif

static int sharpsl_wakeup_check(void)
{
	int i;
	u32 gplr;
#if defined(CONFIG_ARCH_PXA_TOSA)
	int batt_fault = ((PSSR & 0x02) != 0);
#endif

	/* setting GPIO */
	GPDR0 = sys_ctx.gpdr0;
	GAFR0_L = sys_ctx.gafr0_l;
	GPDR0 &= ~WAKEUP_SRC;
//	GAFR0_L &= ~WAKEUP_SRC;
	gplr = GPLR0;

#ifdef WUPSRC_DEBUG
	debug1 = PEDR;
	debug2 = WAKEUP_SRC;
	debug3 = apm_wakeup_src_mask;
	debug4 = PSSR;
#endif
	apm_wakeup_factor = PEDR & WAKEUP_SRC & apm_wakeup_src_mask;
#if defined(CONFIG_ARCH_PXA_POODLE) || defined(CONFIG_ARCH_PXA_CORGI) || defined(CONFIG_ARCH_PXA_TOSA)
	apm_wakeup_factor &= ~0x80000000;		/* clear ALARM */
	if ( ( RTSR & 0x1 ) && ( RTSR & RTSR_ALE ) )
#else
	if ( RTSR & 0x1 )
#endif
		apm_wakeup_factor |= 0x80000000;		/* ALARM */


	PEDR = WAKEUP_SRC;

#if defined(CONFIG_ARCH_PXA_TOSA)
	if(batt_fault) PSSR = 0x02; /* clear BFS bit */
#endif

	if ( !apm_wakeup_factor ){
		return 0;			/* no wakeup factor */
	}

#if defined(CONFIG_ARCH_PXA_CORGI)
	gplr &= ~GPIO_bit(GPIO_KEY_INT);
#endif

#if defined(CONFIG_ARCH_PXA_TOSA)
	//if(PSSR & 0x02){
	if(batt_fault){
	  // Asserted battery fault.
	  // It must have changed the main battery.
	  if(GPIO_bit(GPIO_POWERON) & apm_wakeup_factor){
	    int change_ac_status = 0;
	    if ( apm_wakeup_src_mask & GPIO_bit(GPIO_AC_IN)){
	      if ( !charge_status && ( gplr & GPIO_bit(GPIO_AC_IN) ) == 0)
		change_ac_status = 1;

	      if ( charge_status && ( gplr & GPIO_bit(GPIO_AC_IN) ) != 0)
		change_ac_status = 1;
	    }
	    if(change_ac_status)
	      apm_wakeup_factor |= GPIO_bit(GPIO_AC_IN);

	    return apm_wakeup_factor;
	  }
	  if(GPIO_bit(GPIO_RESET) & apm_wakeup_factor){
	    return apm_wakeup_factor;
	  }
	}
#endif
	/* Faulty operation check */
	for (i = 0; i <= 15; i++) {
#if defined(CONFIG_ARCH_PXA_CORGI)
		if( i == GPIO_AK_INT ) continue;
#endif
		if ( apm_wakeup_factor & GPIO_bit(i) ) {
			if ( (PRER & apm_wakeup_src_mask & GPIO_bit(i)) 
			     && !(gplr & GPIO_bit(i)) ) {
				apm_wakeup_factor &= ~GPIO_bit(i);
			}
			if ( (PFER & apm_wakeup_src_mask & GPIO_bit(i)) 
			     && (gplr & GPIO_bit(i)) ) {
				apm_wakeup_factor &= ~GPIO_bit(i);
			}
		}
	}

	if ( !apm_wakeup_factor )
		return 0;			/* no wakeup factor */

	return apm_wakeup_factor;
}

static int sharpsl_wakeup_hook(void)
{
	u32 gplr = GPLR0;
	int is_resume = 0;

	DPRINTK("GPLR0 = %x\n",gplr);
#ifdef WUPSRC_DEBUG
	printk("PEDR=%08x\n",debug1);
	printk("src=%08x\n",debug2);
	printk("src_mask=%08x\n",debug3);
	printk("PSSR=%08x->%08x\n",debug4,PSSR);
#endif
	if ( (apm_wakeup_factor & GPIO_bit(GPIO_AC_IN))  &&
	     sharpsl_battery_charge_hook ) {

#if defined(CONFIG_ARCH_PXA_TOSA)
		if ( !(gplr & GPIO_bit(GPIO_AC_IN)) ) {
#else
		if ( gplr & GPIO_bit(GPIO_AC_IN) ) {
#endif
			sharpsl_battery_charge_hook(2);	/* charge on */
		} else 
#if defined(CONFIG_ARCH_PXA_TOSA)
		if ( gplr & GPIO_bit(GPIO_AC_IN) ) {
#else
		if ( !( gplr & GPIO_bit(GPIO_AC_IN) ) ) {
#endif
			sharpsl_battery_charge_hook(1);	/* charge off */
		}
	}
#if defined(CONFIG_ARCH_PXA_TOSA)
	if ( (apm_wakeup_factor & GPIO_bit(GPIO_BAT0_CRG)) &&
	     sharpsl_battery_charge_hook ) {
		sharpsl_battery_charge_hook(0);  /* tosa: main battery full */
	}
	if ( (apm_wakeup_factor & GPIO_bit(GPIO_BAT1_CRG)) &&
	     sharpsl_battery_charge_hook ) {
		sharpsl_battery_charge_hook(3);  /* tosa: jacket battery full */
	}
#else
	if ( (apm_wakeup_factor & GPIO_bit(GPIO_CHRG_FULL)) &&
	     sharpsl_battery_charge_hook ) {
		sharpsl_battery_charge_hook(0);  /* charge off */
	}
#endif

#if defined(CONFIG_ARCH_PXA_POODLE)
	if ( apm_wakeup_factor & GPIO_bit(GPIO_ON_KEY) )
		is_resume |= GPIO_bit(GPIO_ON_KEY);
#endif
#if defined(CONFIG_ARCH_PXA_CORGI)
	if ( apm_wakeup_factor & GPIO_bit(GPIO_KEY_INT) ) {
		if (sharppda_kbd_is_wakeup()){
			is_resume |= GPIO_bit(GPIO_KEY_INT);
		}
	}
#if defined(CONFIG_ARCH_PXA_SHEPHERD)
	if ((apm_wakeup_factor & GPIO_bit(GPIO_MAIN_BAT_LOW)) &&
		(GPLR(GPIO_MAIN_BAT_LOW) & GPIO_bit(GPIO_MAIN_BAT_LOW)) &&
		sharppda_kbd_resetcheck()) {
		/* apm_wakeup_src_mask = 0; */
		sharpsl_off_mode = 1;
		is_resume |= GPIO_bit(GPIO_MAIN_BAT_LOW);
	}

	// lock
	if ((apm_wakeup_factor & GPIO_bit(GPIO_MAIN_BAT_LOW)) &&
	    (GPLR(GPIO_MAIN_BAT_LOW) & GPIO_bit(GPIO_MAIN_BAT_LOW)) &&
	    ( sharpsl_battery_charge_hook )) {
	  sharpsl_battery_charge_hook(2);	/* charge on */
	}

	// unlock
	if ((apm_wakeup_factor & GPIO_bit(GPIO_MAIN_BAT_LOW)) &&
		!(GPLR(GPIO_MAIN_BAT_LOW) & GPIO_bit(GPIO_MAIN_BAT_LOW)) &&
	    ( sharpsl_battery_charge_hook )) {
	  sharpsl_battery_charge_hook(1);	/* charge off */
	}
#else
	if ( apm_wakeup_factor & GPIO_bit(GPIO_MAIN_BAT_LOW) )
		apm_wakeup_src_mask = 0;
#endif
#endif
#if !defined(CONFIG_ARCH_PXA_TOSA)
	if ( apm_wakeup_factor & GPIO_bit(GPIO_WAKEUP) )
		is_resume |= GPIO_bit(GPIO_WAKEUP);
#endif
#if defined(CONFIG_ARCH_PXA_POODLE)
	if ( (apm_wakeup_factor & GPIO_bit(GPIO_GA_INT)) && (LCM_KIC & 1) ) {
		LCM_KIC &= ~0x100;
		LCM_ICR &= ~0x100;
		if (sharppda_kbd_is_wakeup())
			is_resume |= GPIO_bit(GPIO_GA_INT);
	}
	if ( apm_wakeup_factor & GPIO_bit(GPIO_CF_STSCHG) )
		is_resume |= GPIO_bit(GPIO_CF_STSCHG);
	if ( apm_wakeup_factor & GPIO_bit(GPIO_nSD_INT) )
		is_resume |= GPIO_bit(GPIO_nSD_INT);
	if ( apm_wakeup_factor & GPIO_bit(GPIO_HP_IN) )
		is_resume |= GPIO_bit(GPIO_HP_IN);
#endif
#if defined(CONFIG_ARCH_PXA_CORGI)
	if ( apm_wakeup_factor & GPIO_bit(GPIO_AK_INT) ) {
		if ( corgi_wakeup_remocon_hook() ){
			is_resume |= GPIO_bit(GPIO_AK_INT);
		}
	}
#endif

#if defined(CONFIG_ARCH_PXA_TOSA)
	if ( (apm_wakeup_factor & GPIO_bit(GPIO_AC_IN)) ) {
#ifdef LOGICAL_WAKEUP_SRC
		if (logical_wakeup_src_mask&IDPM_WAKEUP_AC) {
			is_resume |= GPIO_bit(GPIO_AC_IN);
		}
#endif
	}
	if (apm_wakeup_factor & GPIO_bit(GPIO_POWERON)) { // function key
		if (sharppda_kbd_is_wakeup()){
			is_resume |= GPIO_bit(GPIO_POWERON);
		}
	}
	if (apm_wakeup_factor & GPIO_bit(GPIO_RECORD_BTN)) { // rec key
		if (sharppda_kbd_is_wakeup()){
			is_resume |= GPIO_bit(GPIO_RECORD_BTN);
		}
	}
	if (apm_wakeup_factor & GPIO_bit(GPIO_SYNC)) { // sync key
		if (sharppda_kbd_is_wakeup()){
			is_resume |= GPIO_bit(GPIO_SYNC);
		}
	}
	if (apm_wakeup_factor & GPIO_bit(GPIO_USB_IN)) {
#ifdef LOGICAL_WAKEUP_SRC
		if (logical_wakeup_src_mask&IDPM_WAKEUP_USBD) {
			is_resume |= GPIO_bit(GPIO_USB_IN);
		}
#endif
	}
	if (apm_wakeup_factor & GPIO_bit(GPIO_JACKET_DETECT)){
	        sharpsl_battery_charge_hook(4);
#ifdef LOGICAL_WAKEUP_SRC
		if (logical_wakeup_src_mask&IDPM_WAKEUP_JACKET) {
			is_resume |= GPIO_bit(GPIO_JACKET_DETECT);
		}
#endif
	}
#endif

	if ( ( apm_wakeup_factor & PWER_RTC ) && !sharpsl_alarm_flag){
		is_resume |= PWER_RTC;
	}

	return is_resume;
}
#endif

#if defined(CONFIG_ARCH_PXA_TOSA)
#define BATTERY_CHECK_TIME	(60*5)	// 5 min
#else
#define BATTERY_CHECK_TIME	60*10	// 10 min
#endif

//extern int charge_status;
extern int sharpsl_off_charge;


#if defined(CONFIG_ARCH_PXA_TOSA)
#if 0 /////////////////////////////////////////////////////////
static void tc6393_susx(void)
{
#if 1
  reset_scoop_jc_gpio(SCP_JC_TC3693_L3V_ON);
//  reset_scoop_jc_gpio(SCP_JC_TC6393_SUSPEND);
//  reset_scoop_gpio(SCP_TC6393_REST_IN);
#endif

//  TC6393_SYS_REG(TC6393_SYS_GPOOECR1) = TC6393_CARD_VCC_ON;
//  TC6393_SYS_REG(TC6393_SYS_GPODSR1) = 0; /* CARD_VCC_OFF */
#if 1
//  TC6393_SYS_REG(TC6393_SYS_GPOOECR1) = TC6393_GPO_OE;
//  TC6393_SYS_REG(TC6393_SYS_GPOOECR1) = TC6393_CARD_VCC_ON;
  TC6393_SYS_REG(TC6393_SYS_GPODSR1) |= TC6393_CARD_VCC_ON;
  TC6393_SYS_REG(TC6393_SYS_GPOOECR1) = TC6393_GPO_OE;
#endif
}

static void tc6393_resx(void)
{
  set_GPIO_mode(GPIO11_3_6MHz_MD);
  set_GPIO_mode(GPIO18_RDY_MD);
  mdelay(1);
  set_scoop_jc_gpio(SCP_JC_TC6393_SUSPEND);
  mdelay(10);
  set_scoop_gpio(SCP_TC6393_REST_IN);
  //set_scoop_jc_gpio(SCP_JC_TC3693_L3V_ON);
  TC6393_SYS_REG(TC6393_SYS_FER) = 0;
												/* clock setting */
  TC6393_SYS_REG(TC6393_SYS_PLL2CR) = 0x0cc1;
  TC6393_SYS_REG(TC6393_SYS_CCR) = 0x1310;
  TC6393_SYS_REG(TC6393_SYS_MCR) = 0x80AA;
												/* GPIO */
  TC6393_SYS_REG(TC6393_SYS_GPER) = 0x0030;		/* 15-0 GPO */
  TC6393_SYS_REG(TC6393_SYS_GPOOECR1) = TC6393_GPO_OE;
												/* 15-0 GPO set H */
  TC6393_SYS_REG(TC6393_SYS_GPODSR1) = TC6393_CARD_VCC_ON;
}
#endif ///////////////////////////////////////////////
#endif

int pxa_suspend(void)
{

	unsigned long   flags;
#if defined (CONFIG_SABINAL_DISCOVERY)
	unsigned long	RTAR_buffer;
	unsigned int	wakeupsrc;
#else
	unsigned long RTAR_buffer;
	unsigned long RTAR_buffer2;
#if defined(CONFIG_ARCH_PXA_TOSA)
	unsigned long RCNR_buffer;
#endif
#endif

#ifndef CONFIG_SABINAL_DISCOVERY
	sharpsl_off_state = 1;

	// flag clear
	charge_status = 0;
	sharpsl_off_charge = 1;

	/* enable alarm int and so alarm time is near , so not suspend. */
	if ( sharpsl_emergency_off == 0 ) {
	  if ( ( ( RTAR - RCNR ) < 10 ) || ( ( RCNR - RTAR ) < 10  ) ) {
	    printk("you have alarm.\n");
	    return 0;
	  }
	} else {
	  sharpsl_emergency_off = 0;
	}

	if ( ( sharpsl_chg_freq & 0xffff0000 ) != 0 ) {
	  switch (sharpsl_chg_freq & 0xffff ) {
	  case 0x241:
	    if ( CCCR == 0x0241 ) break;
		cpu_xscale_sl_change_speed_241_without_lcd();
	    cccr_reg = CCCR;
	    break;
	  case 0x145:
	    if ( CCCR == 0x0145 ) break;
		cpu_xscale_sl_change_speed_145_without_lcd();
	    cccr_reg = CCCR;
	    break;
#if defined(CONFIG_ARCH_PXA_SHEPHERD) || defined(CONFIG_ARCH_PXA_TOSA)
	  case 0x161:
	    if ( CCCR == 0x0161 ) break;
		cpu_xscale_sl_change_speed_161();
	    cccr_reg = CCCR;
	    break;
#endif
	  }
	  sharpsl_chg_freq &= 0x0000ffff;
	  mdelay(500);
	  return 0;
	}
#endif

#if defined(CONFIG_ARCH_PXA_SHEPHERD) || defined(CONFIG_ARCH_PXA_TOSA)
	if (sharpsl_off_mode == 2) sharpsl_restart();	/* off in maintenance */
#endif

	/* set up pointer to sleep parameters */
	sleep_param = kmalloc (SLEEPDATA_SIZE*sizeof(long), GFP_ATOMIC);
	if (!sleep_param)
		return -ENOMEM;
	sleep_param_p = virt_to_phys(sleep_param);
	
#if defined (CONFIG_SABINAL_DISCOVERY)
	
	discovery_wakeup_hook();
	
	if ((ASIC3_GPIO_PSTS_D &  AC_IN)) {
		
		ASIC3_SLEEP_PARAM6 = 0x00;
		ASIC3_SLEEP_PARAM7 = 0x00;
		ASIC3_SLEEP_PARAM8 = 0x00;

		if (alarm_enable && (RTSR & RTSR_ALE)) {
			RTAR_buffer = RTAR;
			ASIC3_SLEEP_PARAM6 = (RTAR_buffer & 0x00000fff);
			ASIC3_SLEEP_PARAM7 = ((RTAR_buffer & 0x00fff000) >> 12);
			ASIC3_SLEEP_PARAM8 = (((RTAR_buffer & 0xff000000) >> 24) | 0x1100);
		} else {
			ASIC3_SLEEP_PARAM8 = 0x00;			
		}
		RTAR = RCNR + 5;
		udelay(100);

	} else {
		
		if (alarm_enable && (RTSR & RTSR_ALE)) {;
			ASIC3_SLEEP_PARAM6 = (RTAR_buffer & 0x00000fff);
			ASIC3_SLEEP_PARAM7 = ((RTAR_buffer & 0x00fff000) >> 12);
			ASIC3_SLEEP_PARAM8 = (((RTAR_buffer & 0xff000000) >> 24) | 0x1000);
		} else {
			ASIC3_SLEEP_PARAM6 = 0x00;
			ASIC3_SLEEP_PARAM7 = 0x00;
			ASIC3_SLEEP_PARAM8 = 0x00;
		}
	}
#elif defined(CONFIG_ARCH_PXA_POODLE)

#if 0
	if ( !in_interrupt() ) {
	  __set_current_state(TASK_UNINTERRUPTIBLE);
	  schedule_timeout( 1 * HZ);

	  /* wait for Power On key release */
	  while ( !(GPLR(GPIO_ON_KEY) & GPIO_bit(GPIO_ON_KEY)) ) ;
	}
#endif

#endif

	save_flags_cli(flags);
	clf();

	sys_ctx.osmr0 = OSMR0;
	sys_ctx.osmr1 = OSMR1;
	sys_ctx.osmr2 = OSMR2;
	sys_ctx.osmr3 = OSMR3;
	sys_ctx.oscr  = OSCR;
	//sys_ctx.ower = OWER;
	sys_ctx.oier = OIER;
    
#if defined (CONFIG_SABINAL_DISCOVERY)
    sys_ctx.pstsa=ASIC3_GPIO_PSTS_A;
    sys_ctx.pstsb=ASIC3_GPIO_PSTS_B;
    sys_ctx.pstsc=ASIC3_GPIO_PSTS_C;
    sys_ctx.pstsd=ASIC3_GPIO_PSTS_D;

	ASIC3_GPIO_INTSTAT_A = 0;
	ASIC3_GPIO_INTSTAT_B = 0;
	ASIC3_GPIO_INTSTAT_C = 0;
	ASIC3_GPIO_INTSTAT_D = 0;
#endif

#if defined(CONFIG_ARCH_PXA_POODLE) || defined(CONFIG_ARCH_PXA_CORGI) || defined(CONFIG_ARCH_PXA_TOSA)
	if ( CCCR != 0x241 ) {
	  cpu_xscale_sl_change_speed_241_without_lcd();
	}
	cccr_reg = CCCR;
	DPRINTK("FCS : CCCR = %x\n",cccr_reg);
#endif

#if defined(CONFIG_ARCH_PXA_TOSA)
	// check jacket status.
	sharpsl_request_dac_init();     // in sharpsl_battery.c
	if(sharpsl_battery_charge_hook)
	  sharpsl_battery_charge_hook(4); // check jacket.
#endif

DO_SUSPEND:

#if defined(CONFIG_ARCH_PXA_POODLE) || defined(CONFIG_ARCH_PXA_CORGI) || defined(CONFIG_ARCH_PXA_TOSA)
	DPRINTK("pass_charge_flag = %d\n",pass_charge_flag);
	if ( !pass_charge_flag ) {
	  // not charging and AC-IN !

#if defined(CONFIG_ARCH_PXA_TOSA)
	  DPRINTK("check AC-adaptor\n");
	  if ( !charge_status && ( GPLR(GPIO_AC_IN) & GPIO_bit(GPIO_AC_IN) ) == 0){
#else
	  if ( !charge_status && ( GPLR(GPIO_AC_IN) & GPIO_bit(GPIO_AC_IN)) != 0){
#endif
	    DPRINTK("kick charging\n");
	    charge_status = 1;
	    sharpsl_off_charge_battery();
	  }
	}

#if defined(CONFIG_ARCH_PXA_TOSA)
	  tosa_check_ac_and_jacket_state();
	  DPRINTK("ac97&tc6393 down\n");
	  tc6393_suspend();
	  tosa_ac97_exit();
#endif

	if ( charge_status ) {
	  if (  ( ( RTAR - RCNR ) < ( BATTERY_CHECK_TIME + 30 ) ) && ( RTSR & RTSR_ALE )  ) {
	    // maybe alarm will occur
	    sharpsl_alarm_flag = 0;
	    DPRINTK("RTAR alarm = %8x\n",RTAR);
	    DPRINTK("RTSR = %8x\n",RTSR);
	    DPRINTK("RCNR = %8x\n",RCNR);
	  } else {
	    RTAR_buffer = RTAR;
	    RTAR = RCNR + BATTERY_CHECK_TIME;
	    sharpsl_alarm_flag = 1;
	    DPRINTK("RTAR charge = %8x\n",RTAR);
	  }
	} else {
	  sharpsl_alarm_flag = 0;
	  DPRINTK("RTAR not charge = %8x\n",RTAR);
	}

	// charging , so CHARGE_ON bit is HIGH during OFF.
	if ( GPLR(GPIO38_FFRI) & GPIO_bit(GPIO38_FFRI) ) {
	  PGSR1 |= GPIO_bit(GPIO38_FFRI);
	} else {
	  PGSR1 &= ~GPIO_bit(GPIO38_FFRI);
	}

#if defined(CONFIG_ARCH_PXA_CORGI)
	if ( GPLR(GPIO_LED_ORANGE) & GPIO_bit(GPIO_LED_ORANGE) ) {
	  PGSR0 |= GPIO_bit(GPIO_LED_ORANGE);
	} else {
	  PGSR0 &= ~GPIO_bit(GPIO_LED_ORANGE);
	}
	if ( GPLR(GPIO43_BTTXD) & GPIO_bit(GPIO43_BTTXD) ) {
	  PGSR1 |= GPIO_bit(GPIO43_BTTXD);
	} else {
	  PGSR1 &= ~GPIO_bit(GPIO43_BTTXD);
	}
#elif defined(CONFIG_ARCH_PXA_TOSA)
// non
#endif

#endif
    
#if defined(CONFIG_ARCH_PXA_POODLE)
	/* ----- hardware suspend ----- */
	/* LOCOMO suspend */
	sys_ctx.lcm_gpo   = LCM_GPO;	/* GPIO */
	if ( charge_status ) {
	    LCM_GPO = ( LCM_GPIO_232VCC_ON | LCM_GPIO_AMP_ON | LCM_GPIO_JK_B );
	} else {
	    LCM_GPO = ( LCM_GPIO_232VCC_ON | LCM_GPIO_AMP_ON );
	}
	sys_ctx.lcm_spict = LCM_SPICT;	/* SPI */
	LCM_SPICT = 0x40;
	sys_ctx.lcm_gpe = LCM_GPE;
	LCM_GPE=0;

	sys_ctx.lcm_tasd  = LCM_ASD;	/* ADSTART */
	LCM_ASD = 0x0000;

	sys_ctx.lcm_spimd  = LCM_SPIMD;	/* SPI */
	LCM_SPIMD = 0x3c14;


	LCM_PAIF = 0x00;	/* AUDIO */
	LCM_DAC = 0x00;		/* DAC */
	LCM_TC = 0x0000;	/* CPS */

	if ( ( LCM_LPT0 & 0x88 ) && ( LCM_LPT1 & 0x88 ) ) {
		LCM_C32K = 0x00;	/* CLK32 off */
	} else {
		/* 18MHz already enabled , so no wait */
		LCM_C32K = 0xc1;	/* CLK32 on */
	}

	LCM_TADC = 0x00;	/* 18MHz clock off */
	LCM_ACC = 0x00;		/* 22MHz/24MHz clock off*/
	LCM_ALS = 0x00;		/* FL */

	LCM_KIC |= 0x0010;
	LCM_ICR |= 0x0010;
	LCM_KIC &= ~0x100;
	LCM_ICR &= ~0x100;
#endif

#if !defined(CONFIG_SABINAL_DISCOVERY) 
	/* Scoop suspend */
	sys_ctx.scp_gpwr = SCP_REG_GPWR;
	SCP_REG_GPWR = 0;

#endif
	sys_ctx.gpdr0 = GPDR0;
	sys_ctx.gpdr1 = GPDR1;
	sys_ctx.gpdr2 = GPDR2;
        sys_ctx.grer0 = GRER0;
        sys_ctx.grer1 = GRER1;
        sys_ctx.grer2 = GRER2;
        sys_ctx.gfer0 = GFER0;
        sys_ctx.gfer1 = GFER1;
        sys_ctx.gfer2 = GFER2;
        sys_ctx.gafr0_l = GAFR0_L;
        sys_ctx.gafr1_l = GAFR1_L;
        sys_ctx.gafr2_l = GAFR2_L;
        sys_ctx.gafr0_u = GAFR0_U;
        sys_ctx.gafr1_u = GAFR1_U;
        sys_ctx.gafr2_u = GAFR2_U;
	sys_ctx.gplr0 = GPLR0;
	sys_ctx.gplr1 = GPLR1;
	sys_ctx.gplr2 = GPLR2;

        sys_ctx.iclr = ICLR;
        sys_ctx.icmr = ICMR;
        sys_ctx.iccr = ICCR;
	ICMR = 0;

        sys_ctx.mecr = MECR;
#ifndef CONFIG_ARCH_SABINAL
        sys_ctx.mcmem0 = MCMEM0;
        sys_ctx.mcmem1 = MCMEM1;
        sys_ctx.mcatt0 = MCATT0;
        sys_ctx.mcatt1 = MCATT1;
        sys_ctx.mcio0 = MCIO0;
        sys_ctx.mcio1 = MCIO1;
#endif

#if !defined(CONFIG_SABINAL_DISCOVERY)
        sys_ctx.ffier = FFIER;
        sys_ctx.fflcr = FFLCR;
        sys_ctx.ffmcr = FFMCR;
        sys_ctx.ffspr = FFSPR;
        sys_ctx.ffisr = FFISR;
	FFLCR |= 0x80;
        sys_ctx.ffdll = FFDLL;
        sys_ctx.ffdlh = FFDLH;
	FFLCR &= 0xef;
#endif

        sys_ctx.cken = CKEN;
	CKEN = 0;

#if !defined(CONFIG_SABINAL_DISCOVERY)
	{
		int i;
		PWER = WAKEUP_SRC & apm_wakeup_src_mask;
		PRER = R_WAKEUP_SRC & apm_wakeup_src_mask;
		PFER = F_WAKEUP_SRC & apm_wakeup_src_mask;

		PEDR = WAKEUP_SRC & apm_wakeup_src_mask;

		for (i = 0; i <=15; i++) {
			if ( PRER & PFER & GPIO_bit(i) ) {
				if ( GPLR0 & GPIO_bit(i) )
					PRER &= ~GPIO_bit(i); 
				else
					PFER &= ~GPIO_bit(i); 
			}
		}

		/* Clear reset status */
		RCSR = RCSR_HWR | RCSR_WDR | RCSR_SMR | RCSR_GPR;

		/* Stop 3.6MHz and drive HIGH to PCMCIA and CS */
		PCFR = PCFR_OPDE;

#if defined(CONFIG_ARCH_PXA_CORGI)
		/* GPIO Sleep Register */
		PGSR2 = (PGSR2 & ~GPIO_ALL_STROBE_BIT) | GPIO_STROBE_BIT(0);
		GPDR0 = 0xD3F8B040;
		GPDR1 = 0x00FFAFC3;
		GPDR2 = 0x0001C004;
#elif defined(CONFIG_ARCH_PXA_TOSA)
		/* GPIO Sleep Register */
		PGSR1 = (PGSR1 & ~GPIO_LOW_STROBE_BIT);
		PGSR2 = (PGSR2 & ~GPIO_HIGH_STROBE_BIT);
#ifdef LOGICAL_WAKEUP_SRC
		if (logical_wakeup_src_mask &
		    (IDPM_WAKEUP_ADDRESSBOOK|IDPM_WAKEUP_CALENDAR|IDPM_WAKEUP_MENU|IDPM_WAKEUP_MAIL)) {
		  PGSR1 |= GPIO_STROBE_BIT(4);
		}
#endif
		GPDR0 = 0xC3810940;
		GPDR1 = 0xFCFFAB82;
		GPDR2 = 0x000F501f;
#endif
	}
#endif

	/* set resume return address */
#ifdef CONFIG_XIP_ROM
	PSPR = (unsigned long)cpu_pxa_resume & 0x00ffffff;
#else
	PSPR = virt_to_phys(cpu_pxa_resume);
#endif

	cpu_pxa_do_suspend();

	PSPR = 0;

#if !defined(CONFIG_SABINAL_DISCOVERY)
	sharpsl_wakeup_check();
        FFMCR = sys_ctx.ffmcr;
        FFSPR = sys_ctx.ffspr;
        FFLCR = sys_ctx.fflcr;
	FFLCR |= 0x80;
        FFDLH = sys_ctx.ffdlh;
        FFDLL = sys_ctx.ffdll;
        FFLCR = sys_ctx.fflcr;
        FFISR = sys_ctx.ffisr;
	FFLCR = 0x07;
        FFIER = sys_ctx.ffier;
#endif

	/* restore registers */
        GPDR0 = sys_ctx.gpdr0;
        GPDR1 = sys_ctx.gpdr1;
        GPDR2 = sys_ctx.gpdr2;
        GRER0 = sys_ctx.grer0;
        GRER1 = sys_ctx.grer1;
        GRER2 = sys_ctx.grer2;
        GFER0 = sys_ctx.gfer0;
        GFER1 = sys_ctx.gfer1;
        GFER2 = sys_ctx.gfer2;
        GAFR0_L = sys_ctx.gafr0_l;
        GAFR1_L = sys_ctx.gafr1_l;
        GAFR2_L = sys_ctx.gafr2_l;
        GAFR0_U = sys_ctx.gafr0_u;
        GAFR1_U = sys_ctx.gafr1_u;
        GAFR2_U = sys_ctx.gafr2_u;
        GPSR0 = sys_ctx.gplr0 & sys_ctx.gpdr0;
        GPSR1 = sys_ctx.gplr1 & sys_ctx.gpdr1;
        GPSR2 = sys_ctx.gplr2 & sys_ctx.gpdr2;
        GPCR0 = ~sys_ctx.gplr0 & sys_ctx.gpdr0;
        GPCR1 = ~sys_ctx.gplr1 & sys_ctx.gpdr1;
        GPCR2 = ~sys_ctx.gplr2 & sys_ctx.gpdr2;

        MECR = sys_ctx.mecr;
#ifndef CONFIG_ARCH_SABINAL
        MCMEM0 = sys_ctx.mcmem0;
        MCMEM1 = sys_ctx.mcmem1;
        MCATT0 = sys_ctx.mcatt0;
        MCATT1 = sys_ctx.mcatt1;
        MCIO0 = sys_ctx.mcio0;
        MCIO1 = sys_ctx.mcio1;
#endif

        CKEN = sys_ctx.cken;

//        ICLR = sys_ctx.iclr;
//        ICCR = sys_ctx.iccr;
	ICLR = 0;
	ICCR = 1;
        ICMR = sys_ctx.icmr;


	if (ICIP & 0x01) { 
		PEDR = GPIO_bit(0); 
       		ICIP = GPIO_bit(0); 
	}

#if defined(CONFIG_ARCH_PXA_POODLE)
	/* ------ hardware resume ----- */
	/* LOCOMO resume */
	LCM_SPIMD = sys_ctx.lcm_spimd;
	LCM_SPICT = sys_ctx.lcm_spict;
	LCM_ASD   = sys_ctx.lcm_tasd;
	LCM_GPO   = sys_ctx.lcm_gpo;
	LCM_GPE = sys_ctx.lcm_gpe;

	LCM_TADC = 0x80;   /* XON */
	udelay(1000);
	LCM_TADC |= 0x10;  /* CLK9MEN */
	udelay(100);

	LCM_C32K = 0x00;   /* CLK32 off */
#endif

#if !defined(CONFIG_SABINAL_DISCOVERY)
	/* Scoop resume */
	SCP_REG_GPWR = sys_ctx.scp_gpwr;
#endif

#if !defined(CONFIG_SABINAL_DISCOVERY)
#if defined(CONFIG_ARCH_PXA_TOSA)
	if ( sharpsl_alarm_flag ) {
	  RCNR_buffer = RCNR;
	}
	DPRINTK("ac97&tc6393 up\n");
	tosa_ac97_init();
	pxa_ac97_get(&codec,&ac97_on); // initialize 'codec' pointer
	i2c_init(1);
	tc6393_resume();

	sharpsl_request_dac_init(); // in sharpsl_battery.c
#else
	pxa_ssp_init();
#endif

#if defined(CONFIG_ARCH_PXA_POODLE) || defined(CONFIG_ARCH_PXA_CORGI) || defined(CONFIG_ARCH_PXA_TOSA)
	if ( sharpsl_alarm_flag ) {
	  RTAR_buffer2 = RTAR;
	  RTAR = RTAR_buffer;
	  DPRINTK("back the ALARM Time\n");
	}

#if defined(CONFIG_ARCH_PXA_TOSA)
	if ( sharpsl_alarm_flag && ( RCNR_buffer == RTAR_buffer2 ) ) {
	  //printk("RCNR_buffer=%d\n",RCNR_buffer);
	  if(sharpsl_battery_charge_hook)
	    sharpsl_battery_charge_hook(5);
#if 1
	  if(tosa_check_charge_full(BATTERY_CHECK_TIME) < 0) {
	    goto DO_SUSPEND;
	  }
#else
	  goto DO_SUSPEND;
#endif
	}
#else
	if ( sharpsl_alarm_flag && ( RCNR == RTAR_buffer2 ) ) {
	  if ( sharpsl_off_charge_battery() ) {
	    DPRINTK("charge timer \n");
	    goto DO_SUSPEND;
	  }
	}
#endif
#endif

	/* ----- hardware resume ----- */
	if ( !sharpsl_wakeup_hook() ) {
	        printk("return to suspend ....\n");
		goto DO_SUSPEND;
	}

#if !defined(CONFIG_POODLE_TR0) && !defined(CONFIG_CORGI_TR0)
#if defined(CONFIG_ARCH_PXA_SHEPHERD)
	if ( ( (GPLR(GPIO_MAIN_BAT_LOW) & GPIO_bit(GPIO_MAIN_BAT_LOW)) == 0 )
	     || ( !sharpsl_off_mode && chkFatalBatt() == 0 ) ) {
	        printk("return to suspend (fatal) ....\n");
		goto DO_SUSPEND;
	}
#elif defined(CONFIG_ARCH_PXA_TOSA)
	if ( (GPLR(GPIO_AC_IN) & GPIO_bit(GPIO_AC_IN)) == 0){
	  if(sharpsl_off_charge_battery() < 0){
	    printk("return to suspend (no main batt) ....\n");
	    goto DO_SUSPEND;
	  }
	}

	if ( ( (GPLR(GPIO_BAT_LOCKED) & GPIO_bit(GPIO_BAT_LOCKED)) == 0 )
	     || ( !sharpsl_off_mode && chkFatalBatt() == 0 ) ) {
	        printk("return to suspend (fatal) ....\n");
		goto DO_SUSPEND;
	}
#else
	if ( ( (GPLR(GPIO_MAIN_BAT_LOW) & GPIO_bit(GPIO_MAIN_BAT_LOW)) == 0 )
	     || ( chkFatalBatt() == 0 ) ) {
	        printk("return to suspend (fatal) ....\n");
		goto DO_SUSPEND;
	}
#endif
#endif
#endif


#if defined(CONFIG_ARCH_PXA_POODLE) || defined(CONFIG_ARCH_PXA_CORGI) || defined(CONFIG_ARCH_PXA_TOSA)
	PMCR = 0x01;

	if ( sharpsl_off_mode )
	  sharpsl_restart();
#endif


#if defined(CONFIG_ARCH_PXA_POODLE) || defined(CONFIG_ARCH_PXA_CORGI) || defined(CONFIG_ARCH_PXA_TOSA)
	if ( sharpsl_chg_freq == 0x0145 ) {
	  cpu_xscale_sl_change_speed_145_without_lcd();
	}
#if defined(CONFIG_ARCH_PXA_SHEPHERD) || defined(CONFIG_ARCH_PXA_TOSA)
	else if ( sharpsl_chg_freq == 0x0161 ) {
	  cpu_xscale_sl_change_speed_161();
	}
#endif
	cccr_reg = CCCR;
	printk("FCS : CCCR = %x\n",cccr_reg);

	sharpsl_off_charge = 0;
#endif


#if 1 // ensure that OS Timer irq occurs
    OSMR0 = sys_ctx.oscr + LATCH;
#else
    OSMR0 = sys_ctx.osmr0;
#endif
    OSMR1 = sys_ctx.osmr1;
    OSMR2 = sys_ctx.osmr2;
    OSMR3 = sys_ctx.osmr3;
    OSCR = sys_ctx.oscr;
    //OWER = sys_ctx.ower;
    OIER = sys_ctx.oier;

#if defined(CONFIG_ARCH_PXA_POODLE)
	/* i2c initialize */
	i2c_init();
#endif

#if defined (CONFIG_SABINAL_DISCOVERY)

    ASIC3_GPIO_PIOD_A=sys_ctx.pstsa;
    ASIC3_GPIO_PIOD_B=sys_ctx.pstsb;
    ASIC3_GPIO_PIOD_C=sys_ctx.pstsc;
    ASIC3_GPIO_PIOD_D=sys_ctx.pstsd;

	//clear ASIC3 interrupt 
	ASIC3_GPIO_INTSTAT_A = 0;
	ASIC3_GPIO_INTSTAT_B = 0;
	ASIC3_GPIO_INTSTAT_C = 0;
	ASIC3_GPIO_INTSTAT_D = 0;
	
	ASIC3_GPIO_MASK_A = 0x0;
	
	OSSR = ( OSSR_M0 | OSSR_M1 | OSSR_M2 | OSSR_M3 );
 
#endif

 	xtime.tv_sec = RCNR;

#if defined (CONFIG_SABINAL_DISCOVERY)

	wakeupsrc = ((ASIC3_SLEEP_PARAM8 >> 9) & 0x07);
	if (wakeupsrc == 0x01) {
		apm_wakeup_factor = WAKEUP_MENU_KEY;
	} else if (wakeupsrc == 0x02) {
		apm_wakeup_factor = WAKEUP_HOME_KEY;
	} else if (wakeupsrc == 0x03) {
		apm_wakeup_factor = WAKEUP_OK_KEY;
	} else if (wakeupsrc == 0x04) {
		apm_wakeup_factor = WAKEUP_POWER_KEY;
	} else if (wakeupsrc == 0x05) {
		apm_wakeup_factor = WAKEUP_SYNC_KEY;
	} else if (wakeupsrc == 0x06) {
		apm_wakeup_factor = WAKEUP_RTC;
	} else if (wakeupsrc == 0x07) {
		apm_wakeup_factor = WAKEUP_PHS;
	} else 
		apm_wakeup_factor = 0;
	
	if (alarm_enable && (RTSR & RTSR_ALE)) {

		RTAR_buffer = ASIC3_SLEEP_PARAM8;
		RTAR_buffer = RTAR_buffer << 12;
		RTAR_buffer |= ASIC3_SLEEP_PARAM7;
		RTAR_buffer = RTAR_buffer << 12;
		RTAR_buffer |= ASIC3_SLEEP_PARAM6;

		RTAR = RTAR_buffer;
	}
	
	ASIC3_SLEEP_PARAM6 = 0x00;
	ASIC3_SLEEP_PARAM7 = 0x00;
	ASIC3_SLEEP_PARAM8 = 0x00;
       
#endif

	restore_flags(flags);
	
	kfree (sleep_param);


#ifndef CONFIG_SABINAL_DISCOVERY
	sharpsl_off_state = 0;
#endif

	return 0;
}

int pm_do_suspend(void)
{
	int retval;
	retval = pm_send_all(PM_SUSPEND, (void *)2);
	if (retval)
		return retval;

	retval = pxa_suspend();

	retval = pm_send_all(PM_RESUME, (void *)0);

	if (retval)
		return retval;

	return 0;
}

EXPORT_SYMBOL(pxa_suspend);
EXPORT_SYMBOL(pm_do_suspend);


static struct ctl_table pm_table[] = 
{
	{ACPI_S1_SLP_TYP, "suspend", NULL, 0, 0600, NULL, (proc_handler *)&pm_do_suspend},
	{0}
};

static struct ctl_table pm_dir_table[] =
{
	{CTL_ACPI, "pm", NULL, 0, 0555, pm_table},
	{0}
};

#ifndef CONFIG_SABINAL_DISCOVERY



static int pxa_fatal_suspend(void)
{
	unsigned long   flags;
	unsigned long	RTAR_buffer;
	unsigned long	RTAR_buffer2;

	
	sharpsl_off_state = 1;

	// flag clear
	charge_status = 0;
	sharpsl_off_charge = 1;

	save_flags_cli(flags);
	clf();

#if defined(CONFIG_ARCH_PXA_POODLE) || defined(CONFIG_ARCH_PXA_CORGI)  || defined(CONFIG_ARCH_PXA_TOSA)
	if ( CCCR != 0x241 ) {
	  cpu_xscale_sl_change_speed_241_without_lcd();
	}
	cccr_reg = CCCR;
	printk("FCS : CCCR = %x\n",cccr_reg);
#endif

	/* set up pointer to sleep parameters */
	sleep_param = kmalloc (SLEEPDATA_SIZE*sizeof(long), GFP_ATOMIC);
	if (!sleep_param)
		return -ENOMEM;
	sleep_param_p = virt_to_phys(sleep_param);

	    
DO_SUSPEND:
#if defined(CONFIG_ARCH_PXA_POODLE) || defined(CONFIG_ARCH_PXA_CORGI)
	// charging , so CHARGE_ON bit is HIGH during OFF.
	if ( GPLR(GPIO38_FFRI) & GPIO_bit(GPIO38_FFRI) ) {
	  PGSR1 |= GPIO_bit(GPIO38_FFRI);
	} else {
	  PGSR1 &= ~GPIO_bit(GPIO38_FFRI);
	}

#if defined(CONFIG_ARCH_PXA_CORGI)
	if ( GPLR(GPIO_LED_ORANGE) & GPIO_bit(GPIO_LED_ORANGE) ) {
	  PGSR0 |= GPIO_bit(GPIO_LED_ORANGE);
	} else {
	  PGSR0 &= ~GPIO_bit(GPIO_LED_ORANGE);
	}
	if ( GPLR(GPIO43_BTTXD) & GPIO_bit(GPIO43_BTTXD) ) {
	  PGSR1 |= GPIO_bit(GPIO43_BTTXD);
	} else {
	  PGSR1 &= ~GPIO_bit(GPIO43_BTTXD);
	}
#endif
#elif defined(CONFIG_ARCH_PXA_TOSA)
// non
#endif
    
#if defined(CONFIG_ARCH_PXA_POODLE)
	/* ----- hardware suspend ----- */
	/* LOCOMO suspend */
	LCM_GPO = ( LCM_GPIO_232VCC_ON | LCM_GPIO_AMP_ON );
	LCM_SPICT = 0x40;
	LCM_GPE=0;
	LCM_ASD = 0x0000;
	LCM_SPIMD = 0x3c14;
	LCM_PAIF = 0x00;	/* AUDIO */
	LCM_DAC = 0x00;		/* DAC */
	LCM_TC = 0x0000;	/* CPS */

	if ( ( LCM_LPT0 & 0x88 ) && ( LCM_LPT1 & 0x88 ) ) {
		LCM_C32K = 0x00;	/* CLK32 off */
	} else {
		/* 18MHz already enabled , so no wait */
		LCM_C32K = 0xc1;	/* CLK32 on */
	}

	LCM_TADC = 0x00;	/* 18MHz clock off */
	LCM_ACC = 0x00;		/* 22MHz/24MHz clock off*/
	LCM_ALS = 0x00;		/* FL */

	LCM_KIC |= 0x0010;
	LCM_ICR |= 0x0010;
	LCM_KIC &= ~0x100;
	LCM_ICR &= ~0x100;
#endif

#if !defined(CONFIG_SABINAL_DISCOVERY)
	/* Scoop suspend */
	SCP_REG_GPWR = 0;
#endif

	ICMR = 0;


#if !defined(CONFIG_SABINAL_DISCOVERY)
	FFLCR |= 0x80;
	FFLCR &= 0xef;
#endif

	CKEN = 0;

#if !defined(CONFIG_SABINAL_DISCOVERY)
	{
		int i;
		PWER = WAKEUP_SRC & apm_wakeup_src_mask;
		PRER = R_WAKEUP_SRC & apm_wakeup_src_mask;
		PFER = F_WAKEUP_SRC & apm_wakeup_src_mask;
		PEDR = WAKEUP_SRC & apm_wakeup_src_mask;

		for (i = 0; i <=15; i++) {
			if ( PRER & PFER & GPIO_bit(i) ) {
				if ( GPLR0 & GPIO_bit(i) )
					PRER &= ~GPIO_bit(i); 
				else
					PFER &= ~GPIO_bit(i); 
			}
		}

		/* Clear reset status */
		RCSR = RCSR_HWR | RCSR_WDR | RCSR_SMR | RCSR_GPR;

		/* Stop 3.6MHz and drive HIGH to PCMCIA and CS */
		PCFR = PCFR_OPDE;


#if defined(CONFIG_ARCH_PXA_CORGI)
		/* GPIO Sleep Register */
		PGSR2 = (PGSR2 & ~GPIO_ALL_STROBE_BIT) | GPIO_STROBE_BIT(0);
		GPDR0 = 0xD3F8B040;
		GPDR1 = 0x00FFAFC3;
		GPDR2 = 0x0001C004;
#elif defined(CONFIG_ARCH_PXA_TOSA)
		/* GPIO Sleep Register */
		PGSR1 = (PGSR1 & ~GPIO_LOW_STROBE_BIT);
		PGSR2 = (PGSR2 & ~GPIO_HIGH_STROBE_BIT);
#ifdef LOGICAL_WAKEUP_SRC
		if (logical_wakeup_src_mask &
		    (IDPM_WAKEUP_ADDRESSBOOK|IDPM_WAKEUP_CALENDAR|IDPM_WAKEUP_MENU|IDPM_WAKEUP_MAIL)) {
		  PGSR1 |= GPIO_STROBE_BIT(4);
		}
#endif
		GPDR0 = 0xC3810940;
		GPDR1 = 0xFCFFAB82;
		GPDR2 = 0x000F501f;
#endif
	}
#endif

	/* set resume return address */
#ifdef CONFIG_XIP_ROM
	PSPR = (unsigned long)cpu_pxa_resume & 0x00ffffff;
#else
	PSPR = virt_to_phys(cpu_pxa_resume);
#endif
	cpu_pxa_do_suspend();

	PSPR = 0;

	return 0;
}



#if defined(CONFIG_ARCH_PXA_POODLE)
#define	SCP_INIT_DATA(adr,dat)	(((adr)<<16)|(dat))
#define	SCP_INIT_DATA_END	((unsigned long)-1)

void sharpsl_poodle_fataloff(void)
{
  int i;

#ifdef CONFIG_PM
  poodlefl_blank(1);
#endif
  cotulla_fb_disable_lcd_controller_blank();

  {
	static const unsigned long scp_init[] =
	{
		SCP_INIT_DATA(SCP_MCR,0x0100),
		SCP_INIT_DATA(SCP_CDR,0x0000),  // 04
		SCP_INIT_DATA(SCP_CPR,0x0000),  // 0C
		SCP_INIT_DATA(SCP_CCR,0x0000),  // 10
		SCP_INIT_DATA(SCP_IMR,0x0000),  // 18
		SCP_INIT_DATA(SCP_IRM,0x00FF),  // 14
		SCP_INIT_DATA(SCP_ISR,0x0000),  // 1C
		SCP_INIT_DATA(SCP_IRM,0x0000),
		SCP_INIT_DATA_END
	};
	int	i;
	for(i=0; scp_init[i] != SCP_INIT_DATA_END; i++)
	{
		int	adr = scp_init[i] >> 16;
		SCP_REG(adr) = scp_init[i] & 0xFFFF;
	}
  }

}
#endif

#if defined(CONFIG_ARCH_PXA_CORGI) || defined(CONFIG_ARCH_PXA_TOSA)
#define	SCP_INIT_DATA(adr,dat)	(((adr)<<16)|(dat))
#define	SCP_INIT_DATA_END	((unsigned long)-1)
void sharpsl_corgi_fataloff(void)
{
#ifdef CONFIG_PM
#if defined(CONFIG_ARCH_PXA_TOSA)
  tc6393_fatal_off();
  tc6393_suspend();
  tosa_ac97_exit();
#else
  w100_fatal_off();
#endif
#endif

  {
	static const unsigned long scp_init[] =
	{
		SCP_INIT_DATA(SCP_MCR,0x0140),  // 00
		SCP_INIT_DATA(SCP_MCR,0x0100),
		SCP_INIT_DATA(SCP_CDR,0x0000),  // 04
		SCP_INIT_DATA(SCP_CPR,0x0000),  // 0C
		SCP_INIT_DATA(SCP_CCR,0x0000),  // 10
		SCP_INIT_DATA(SCP_IMR,0x0000),  // 18
		SCP_INIT_DATA(SCP_IRM,0x00FF),  // 14
		SCP_INIT_DATA(SCP_ISR,0x0000),  // 1C
		SCP_INIT_DATA(SCP_IRM,0x0000),
		SCP_INIT_DATA(SCP_GPCR,SCP_IO_DIR),  // 20
		SCP_INIT_DATA(SCP_GPWR,SCP_IO_OUT),  // 24
		SCP_INIT_DATA_END
	};
#if defined(CONFIG_ARCH_PXA_TOSA)
	static const unsigned long scp_jc_init[] =
	{
		SCP_INIT_DATA(SCP_MCR,0x0140),  // 00
		SCP_INIT_DATA(SCP_MCR,0x0100),
		SCP_INIT_DATA(SCP_CDR,0x0000),  // 04
		SCP_INIT_DATA(SCP_CPR,0x0000),  // 0C
		SCP_INIT_DATA(SCP_CCR,0x0000),  // 10
		SCP_INIT_DATA(SCP_IMR,0x0000),  // 18
		SCP_INIT_DATA(SCP_IRM,0x00FF),  // 14
		SCP_INIT_DATA(SCP_ISR,0x0000),  // 1C
		SCP_INIT_DATA(SCP_IRM,0x0000),
		SCP_INIT_DATA(SCP_GPCR,SCP_JC_IO_DIR),  // 20
		SCP_INIT_DATA(SCP_GPWR,SCP_JC_IO_OUT),  // 24
		SCP_INIT_DATA_END
	};
#endif
	int	i;
	for(i=0; scp_init[i] != SCP_INIT_DATA_END; i++)
	{
		int	adr = scp_init[i] >> 16;
		SCP_REG(adr) = scp_init[i] & 0xFFFF;
	}
#if defined(CONFIG_ARCH_PXA_TOSA)
	for(i=0; scp_jc_init[i] != SCP_INIT_DATA_END; i++)
	{
		int	adr = scp_jc_init[i] >> 16;
		SCP_JC_REG(adr) = scp_jc_init[i] & 0xFFFF;
	}
#endif

  }

}
#endif


void sharpsl_fataloff(void)
{
#if defined(CONFIG_ARCH_PXA_TOSA)
    return;
#endif

	if ( PSSR & 0x02 ) {

	  printk("PSSR = %8x\n",PSSR);
	  printk("RCSR = %8x\n",RCSR);

#if defined(CONFIG_ARCH_PXA_POODLE)
	  sharpsl_poodle_fataloff();

	  PSPR = 0x00;
	  RCSR = 0xf;

	  pxa_fatal_suspend();
#endif
#if defined(CONFIG_ARCH_PXA_CORGI) || defined(CONFIG_ARCH_PXA_TOSA)
	  sharpsl_corgi_fataloff();

	  PSPR = 0x00;
	  RCSR = 0xf;

	  pxa_fatal_suspend();
#if defined(CONFIG_ARCH_PXA_TOSA)
	  sharpsl_restart();
#endif
#endif
	}
}


static int sharpsl_off_thread(void* unused)
{
	int time_cnt = 0;

	// daemonize();
	strcpy(current->comm, "off_thread");
	sigfillset(&current->blocked);

	while(1) {
		interruptible_sleep_on(&wq_off);
		DPRINTK("start off sequence ! \n");

#if !defined(CONFIG_ARCH_PXA_SHEPHERD) && !defined(CONFIG_ARCH_PXA_TOSA)
		sharpsl_off_mode = 1;
#endif
		handle_scancode(SLKEY_OFF|KBDOWN , 1);
		mdelay(10);
		handle_scancode(SLKEY_OFF|KBUP   , 0);

#if !defined(CONFIG_ARCH_PXA_SHEPHERD) && !defined(CONFIG_ARCH_PXA_TOSA)
		// wait off signal
		// if can not recieve off siganl , so force off.
		time_cnt = jiffies;
		while(1) {
		  if ( ( jiffies - time_cnt )  > 500 ) break;
		  schedule();
		}

		// maybe apps is dead, so we have to restart.
		pm_do_suspend();
		sharpsl_restart();
#endif
	}
	return 0;
}

#if defined(CONFIG_ARCH_PXA_SHEPHERD) || defined(CONFIG_ARCH_PXA_TOSA)
static struct timer_list bat_switch_timer;

void sharpsl_emerge_restart(void)
{
	unsigned long   flags;
	save_flags_cli(flags);
	sharpsl_restart();
}
#endif

void sharpsl_emerge_off(int irq, void *dev_id, struct pt_regs *regs)
{

  DPRINTK("DBG:sharpsl_emerge_off [non-battery]\n");

  /* noise ? */
  mdelay(10);
#if defined(CONFIG_ARCH_PXA_TOSA)
  if ( 0x1234ABCD != regs && GPLR(GPIO_BAT_LOCKED) & GPIO_bit(GPIO_BAT_LOCKED) )
#else
  if ( 0x1234ABCD != regs && GPLR(GPIO_MAIN_BAT_LOW) & GPIO_bit(GPIO_MAIN_BAT_LOW) )
#endif
    {
#if defined(CONFIG_ARCH_PXA_SHEPHERD)
	  sharpsl_battery_charge_hook(1);	/* charge off */
	  if (sharppda_kbd_resetcheck()) {
		  sharpsl_off_mode = 2;			/* off in maintenance */
		  mod_timer(&bat_switch_timer, jiffies + HZ * 5);
		  return;
	  }
#else
    printk("cancel emergency off\n");
#endif
    return;
  }

#if (defined(CONFIG_ARCH_PXA_CORGI)) && !defined(CONFIG_ARCH_PXA_SHEPHERD) && !defined(CONFIG_ARCH_PXA_TOSA)
  if (!(GPLR(GPIO_MAIN_BAT_LOW) & GPIO_bit(GPIO_MAIN_BAT_LOW)))
    apm_wakeup_src_mask = 0;
#endif
  wake_up(&wq_off);

}

#endif

/*
 * Initialize power interface
 */
static int __init pm_init(void)
{
	register_sysctl_table(pm_dir_table, 1);

#ifndef CONFIG_SABINAL_DISCOVERY
	/* Set transition detect */
#ifdef CONFIG_ARCH_PXA_SHEPHERD
	set_GPIO_IRQ_edge( GPIO_MAIN_BAT_LOW  , GPIO_BOTH_EDGES );
#elif defined(CONFIG_ARCH_PXA_TOSA)
	set_GPIO_IRQ_edge( GPIO_BAT_LOCKED  , GPIO_BOTH_EDGES );
#else
	set_GPIO_IRQ_edge( GPIO_MAIN_BAT_LOW  , GPIO_FALLING_EDGE );
#endif


	/* this registration can be done in init/main.c. */
	if(1){
	  int err;
#if defined(CONFIG_ARCH_PXA_TOSA)
	  err = request_irq(IRQ_GPIO_BAT_LOCKED,sharpsl_emerge_off , SA_INTERRUPT, "batok", NULL);
#else
	  err = request_irq(IRQ_GPIO_MAIN_BAT_LOW,sharpsl_emerge_off , SA_INTERRUPT, "batok", NULL);
#endif
	  if( err ){
	    printk("batok install error %d\n",err);
	  }else{
#if defined(CONFIG_ARCH_PXA_TOSA)
	    enable_irq(IRQ_GPIO_BAT_LOCKED);
#else
	    enable_irq(IRQ_GPIO_MAIN_BAT_LOW);
#endif
	    printk("batok installed\n");
	  }
	}

	kernel_thread(sharpsl_off_thread,  NULL, CLONE_FS | CLONE_FILES | CLONE_SIGHAND | SIGCHLD);
#if defined(CONFIG_ARCH_PXA_SHEPHERD) || defined(CONFIG_ARCH_PXA_TOSA)
//#if defined(CONFIG_ARCH_PXA_SHEPHERD) 
	init_timer(&bat_switch_timer);
	bat_switch_timer.function = sharpsl_emerge_restart;
#endif
#endif
	return 0;
}

__initcall(pm_init);


void PrintParamTable(void) {
	int i;
	
	for (i=0; i < SLEEPDATA_SIZE; i=i+4) {
	
		printk("[%d] %8x %8x %8x %8x\n", i,
			(unsigned int)*(unsigned long *)(sleep_param+i),
			(unsigned int)*(unsigned long *)(sleep_param+i+1),
			(unsigned int)*(unsigned long *)(sleep_param+i+2),
			(unsigned int)*(unsigned long *)(sleep_param+i+3));

	}
	
	return;
}

void PrintRegs(void) {
	int i;
	
	for (i=0; i < SLEEPDATA_SIZE; i=i+4) {
	
		printk("[%d] %8x %8x %8x %8x\n", i,
			(unsigned int)*(unsigned long *)(sleep_param+i),
			(unsigned int)*(unsigned long *)(sleep_param+i+1),
			(unsigned int)*(unsigned long *)(sleep_param+i+2),
			(unsigned int)*(unsigned long *)(sleep_param+i+3));

	}
	
	return;
}

void PrintParamTable_P(void) {
	int i;
	
	for (i=0; i < SLEEPDATA_SIZE; i++) {
		* (unsigned long *)0x16000fe0 = i;
		* (unsigned long *)0x16000fe0 = ' ';
		* (unsigned long *)0x16000fe0 = *(unsigned long *)sleep_param_p + i;
		* (unsigned long *)0x16000fe0 = '\r';
		* (unsigned long *)0x16000fe0 = '\n';
		
	}
	
	return;
}

void set_apm_wakeup_src_mask(u32 SetValue)
{
#ifdef LOGICAL_WAKEUP_SRC
	logical_wakeup_src_mask = 0;
	// RTC
	if (SetValue&IDPM_WAKEUP_RTC) {
	  apm_wakeup_src_mask |= GPIO_bit(31);
	  logical_wakeup_src_mask |= IDPM_WAKEUP_RTC;
	} else apm_wakeup_src_mask &= ~GPIO_bit(31);
#if defined(CONFIG_ARCH_PXA_TOSA)
	// Sync key
	if (SetValue&IDPM_WAKEUP_SYNC) {
	  apm_wakeup_src_mask |= GPIO_bit(GPIO_SYNC);
	  logical_wakeup_src_mask |= IDPM_WAKEUP_SYNC;
	} else apm_wakeup_src_mask &= ~GPIO_bit(GPIO_SYNC);
	// Record key
	if (SetValue&IDPM_WAKEUP_REC) {
	  apm_wakeup_src_mask |= GPIO_bit(GPIO_RECORD_BTN);
	  logical_wakeup_src_mask |= IDPM_WAKEUP_REC; 
	} else apm_wakeup_src_mask &= ~GPIO_bit(GPIO_RECORD_BTN);
	// USBD
	if (SetValue&IDPM_WAKEUP_USBD) {
	  apm_wakeup_src_mask |= GPIO_bit(GPIO_USB_IN);
	  logical_wakeup_src_mask |= IDPM_WAKEUP_USBD;
	} else apm_wakeup_src_mask &= ~GPIO_bit(GPIO_USB_IN);
	// only Logical
	if (SetValue&IDPM_WAKEUP_AC)		logical_wakeup_src_mask |= IDPM_WAKEUP_AC;
	if (SetValue&IDPM_WAKEUP_JACKET)	logical_wakeup_src_mask |= IDPM_WAKEUP_JACKET;
	if (SetValue&IDPM_WAKEUP_CALENDAR)	logical_wakeup_src_mask |= IDPM_WAKEUP_CALENDAR;
	if (SetValue&IDPM_WAKEUP_ADDRESSBOOK)	logical_wakeup_src_mask |= IDPM_WAKEUP_ADDRESSBOOK;
	if (SetValue&IDPM_WAKEUP_MAIL)		logical_wakeup_src_mask |= IDPM_WAKEUP_MAIL;
	if (SetValue&IDPM_WAKEUP_MENU)		logical_wakeup_src_mask |= IDPM_WAKEUP_MENU;
#ifdef WUPSRC_DEBUG
	printk("set_wakeup_src=%08x->%08x[%08x]\n",SetValue,logical_wakeup_src_mask,apm_wakeup_src_mask);
#endif
#endif		
#else
	apm_wakeup_src_mask	= SetValue;
#endif
}

u32 get_apm_wakeup_src_mask(void)
{
#ifdef LOGICAL_WAKEUP_SRC
#ifdef WUPSRC_DEBUG
  printk("get_wakeup_src=%08x\n",logical_wakeup_src_mask);
#endif
	return (u32)logical_wakeup_src_mask;
#else
	return (u32)apm_wakeup_src_mask;
#endif
}

u32 get_apm_wakeup_factor(void)
{
	return (u32)apm_wakeup_factor;
}


EXPORT_SYMBOL(PrintParamTable);
EXPORT_SYMBOL(PrintParamTable_P);

EXPORT_SYMBOL(set_apm_wakeup_src_mask);
EXPORT_SYMBOL(get_apm_wakeup_src_mask);
EXPORT_SYMBOL(get_apm_wakeup_factor);

