/*
 * XScale Power Management Routines
 *
 * (C) Copyright 2001 Lineo Japan, Inc.
 *
 * May be copied or modified under the terms of the GNU General Public
 * License.  See linux/COPYING for more information.
 *
 * Based on
 *  linux/arch/arm/mach-pxa/sharpsl_power.c
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
 *	09-Apr-2003 SHARP for Shaphard (software reset)
 *	October-2003 SHARP for boxer
 *      28-Nov-2003 Sharp Corporation for Tosa
 *      26-Feb-2004 Lineo Solutions, Inc.  for Tosa
 *      20-Aug-2004 Sharp Corporation for Spitz
 *      28-Feb-2005 Sharp Corporation for Akita
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

#include <asm/arch/irqs.h>
#include <asm/hardirq.h>

#include <asm/sharp_char.h>
#include <asm/sharp_keycode.h>
#include <asm/arch/keyboard.h>
#include <asm/arch/sharpsl_wakeup.h>
#include "sharpsl_param.h"
#include <asm/arch/i2sc.h>


static struct {
        u32 osmr0, osmr1, osmr2, osmr3, oscr;
        u32 ower, oier;

        u32 gpdr0, gpdr1, gpdr2, gpdr3;
        u32 grer0, grer1, grer2, grer3;
        u32 gfer0, gfer1, gfer2, gfer3;
	u32 gafr0_l, gafr1_l, gafr2_l, gafr3_l;
	u32 gafr0_u, gafr1_u, gafr2_u, gafr3_u;
	u32 gplr0, gplr1, gplr2, gplr3;
        u32 icmr, iclr, iccr;
        u32 mecr, cken;

	u32 mcmem0, mcmem1, mcatt0, mcatt1, mcio0, mcio1;

	/* FIXME this part should in the serial port driver ,Control register*/
	u32 ffier, fflcr, ffmcr, ffspr, ffisr, ffdll, ffdlh;
	u32 lcm_tasd, lcm_spict, lcm_gpo;
        u32 lcm_gpe, lcm_spimd;

	/* Scoop registers */
        u32 scp_gpwr;
#if defined(CONFIG_ARCH_PXA_AKITA)
        u8 ioexp_port;
#else
        u32 scp2_gpwr;
#endif
        u16 pstsa,pstsb,pstsc,pstsd;
} sys_ctx;

extern unsigned long *sleep_param;	/* virtual address */
extern unsigned long  sleep_param_p;	/* physical address */
extern int charge_status;
extern int sharpsl_off_charge;
extern unsigned long cccr_reg;

extern void cpu_pxa_resume(void);	/* function the bootloader will call back */
extern int cpu_pxa_do_suspend(void);
extern unsigned short chkFatalBatt(void);
extern int sharpsl_off_charge_battery(int);
extern void pxa_ssp_init(void);
extern void sharpsl_set_charge_full(void);

#if defined(CONFIG_ARCH_PXA_SPITZ)
#ifdef DEBUG
#define BATTERY_CHECK_TIME	(10)  // 1sec (for debug)
#define BATTERY_CHARGE_FULL_COUNT  (6) // 1min (for debug)
#else
#define BATTERY_CHECK_TIME	(60*10)	// 10 min
#define BATTERY_CHARGE_FULL_COUNT  (36) // 10min x 36 = 6h
#endif
#endif

#if defined(CONFIG_ARCH_PXA_SPITZ)
#define PXA27X_SUSPEND
#endif

int sharpsl_off_mode = 0;
int pass_charge_flag = 0;
u32 sharpsl_emergency_off = 0;
unsigned int sharpsl_chg_freq = 0x02000210;
int off_charge_full_counter = 0;
int off_charge_check_counter = 0;

static int sharpsl_alarm_flag = 0;
static int sharpsl_off_state = 0;
static DECLARE_WAIT_QUEUE_HEAD(wq_off);

extern void sharpsl_battery_charge_hook(int mode);
extern int corgi_wakeup_remocon_hook(void);

extern int sharpsl_main_bk_flag;
int sharpsl_request_off = 0;

void PrintParamTable(void);

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

	RCSR = 0xf;

	printk("reboot the kernel (%d)\n",nonstop_flag);
	if( nonstop_flag && ((MSC0 & 0xffff0000) == 0x7ff00000) )
		MSC0 = (MSC0 & 0xffff)|0x7ee00000;

	/* reboot by gpio reset */
	set_GPIO_mode(GPIO_ON_RESET | GPIO_OUT);
	GPSR(GPIO_ON_RESET) |= GPIO_bit(GPIO_ON_RESET);
	while(1) {
		if ( flag++ > 0x20000000 ) break;
	}	
	return 0;
}

static int sharpsl_wakeup_hook(void)
{
	u32 logical_factor = get_logical_wakeup_factor();
	u32 virtual_factor = get_virtual_wakeup_factor();
	int is_resume = logical_factor;

	if ( virtual_factor & VIO_AC ) {
		if ( GPLR(GPIO_AC_IN) & GPIO_bit(GPIO_AC_IN) ) {
			sharpsl_battery_charge_hook(1);	/* charge off */
		} else { 
			sharpsl_battery_charge_hook(2);	/* charge on */
		}
	}

	if ( virtual_factor & VIO_FULLCHARGED ) {
		sharpsl_battery_charge_hook(0);  /* charge off */
	}

	if ( virtual_factor & VIO_LOCKSW ) {
	  if ( AC_IN_STATUS ) {
	    // ac-in
	    if ( GPLR(GPIO_BAT_COVER ) & GPIO_bit(GPIO_BAT_COVER) ) {
	      // unlocked
	      // printk("unlocked: charge off\n");
	      sharpsl_battery_charge_hook(1);	/* charge off */	      
	    } else { 
	      // locked
	      // printk("locked: charge on\n");
	      sharpsl_battery_charge_hook(2);	/* charge on */
	    }
	  }
	}
//	printk("is_resume=%d\n",is_resume);

	return is_resume;
}

static void sharpsl_check_scoop_reg(void)
{
     if((SCP_REG_MCR & 0x100) == 0){
       printk("SCP[MCR] = %x->101\n",SCP_REG_MCR);
       SCP_REG_MCR = 0x0101;
     }
#if !defined(CONFIG_ARCH_PXA_AKITA)
     if((SCP2_REG_MCR & 0x100) == 0){
       printk("SCP2[MCR] = %x->101\n",SCP2_REG_MCR);
       SCP2_REG_MCR = 0x0101;
     }
#endif
}

#if defined(PXA27X_SUSPEND)
static int pwr_i2c_open(void)
{
  PCFR |= PCFR_PI2C_EN;
  PCFR &= ~PCFR_FVC;
  udelay(1);

  CKEN |= CKEN15_PMI2C;
  udelay(1);
  return 0;
}

static int pwr_i2c_close(void)
{

  PMICR = I2C_ICR_UR;
  PMISAR = 0x00;
  udelay(1);

  CKEN &= ~CKEN15_PMI2C;
  udelay(1);

  PCFR &= ~(PCFR_PI2C_EN|PCFR_FVC);
  udelay(1);
  return 0;
}

static int pwr_i2c_write( unsigned char device, unsigned char value )
{
  int timeout;
  int retry = 10;

 Retry:

  PMICR = I2C_ICR_UR;
  PMISAR = 0x00;
  udelay(1);
  PMICR = (I2C_ICR_IUE|I2C_ICR_SCLE);

  /* Write slave device address & start */
  PMIDBR = (device<<1);
  PMICR |= I2C_ICR_START;
  PMICR &= ~I2C_ICR_STOP;
  PMICR |= I2C_ICR_TB;

  timeout = 10*1000; /* 10ms */
  while((PMISR & I2C_ISR_ITE)==0){
    if(timeout-- <= 0){
      //printk("%s:timeout error\n",__func__);
      goto Error;
    }
    udelay(1);
  }
  if(PMISR & I2C_ISR_ACK){
    //printk("%s:nack error\n",__func__);
    goto Error; /* recived NACK. */
  }
  PMISR = I2C_ISR_ITE;

  /* Write data */
  PMICR &= ~I2C_ICR_START;
  PMICR |= I2C_ICR_STOP;
  PMIDBR = value;
  PMICR |= I2C_ICR_TB;

  timeout = 10*1000; /* 10ms */
  while((PMISR & I2C_ISR_ITE)==0){
    if(timeout-- <= 0){
      //printk("%s:timeout error\n",__func__);
      goto Error;
    }
    udelay(1);
  }
  if(PMISR & I2C_ISR_ACK){
    //printk("%s:nack error\n",__func__);
    goto Error; /* recived NACK. */
  }
  PMISR = I2C_ISR_ITE;

  PMICR &= ~I2C_ICR_STOP;
  return 0;

 Error:
  PMISR = I2C_ISR_ITE; /* need!! */
  PMICR = I2C_ICR_UR;
  PMISAR = 0;
  PMICR = (I2C_ICR_IUE|I2C_ICR_SCLE);
  retry--;
  //printk("%s: retry!\n",__func__);
  if(retry > 0) goto Retry;
  printk("%s: error!\n",__func__);
  return -1;
}

#ifdef DEBUG
int pwr_i2c_read( unsigned char device, unsigned char *value )
{
  unsigned long	r;
  int timeout;
  int retry = 10;

  if(value == NULL) return -2;

 Retry:

  PMICR = I2C_ICR_UR;
  PMISAR = 0x00;
  udelay(1);
  PMICR = (I2C_ICR_IUE|I2C_ICR_SCLE);

  // Write slave device address & start
  PMIDBR = ((device<<1)|1);
  PMICR |= I2C_ICR_START;
  PMICR &= ~I2C_ICR_STOP;
  PMICR |= I2C_ICR_TB;

  timeout = 10*1000; /* 10ms */
  while((PMISR&I2C_ISR_ITE)==0){
    if(timeout-- <= 0){
      PMISR = I2C_ISR_ITE;
      goto ErrorRet;
    }
    udelay(1);
  }

  PMISR = I2C_ISR_ITE;

  // Write sub-address
  PMICR &= ~I2C_ICR_START;

  // Read data
  PMICR |= (I2C_ICR_STOP|I2C_ICR_ACK);
  PMICR |= I2C_ICR_TB;

  timeout = 10*1000; /* 10ms */
  while((PMISR&I2C_ISR_IRF)==0){
    if(timeout-- <= 0){
      PMISR = I2C_ISR_IRF;
      goto ErrorRet;
    }
    udelay(1);
  }

  PMISR = I2C_ISR_IRF;
  r = PMIDBR;
  *value = (unsigned char)r;
  PMICR &= ~(I2C_ICR_STOP|I2C_ICR_ACK);

  return 0;

 ErrorRet:

  PMICR = I2C_ICR_UR;
  PMISAR = 0;
  PMICR = (I2C_ICR_IUE|I2C_ICR_SCLE);
  retry--;
  //printk("%s: retry!\n",__func__);
  if(retry > 0) goto Retry;
  printk("%s: error!\n",__func__);
  return -1;
}
#endif //DEBUG

static void cpu_xscale_voltage_low(void)
{

  pwr_i2c_open();
  pwr_i2c_write( 0x0c, 0x13 );  // 1.00V - SlewRate 1
  //pwr_i2c_write( 0x0c, 0x12 );  // 0.95V - SlewRate 1
  pwr_i2c_close();
}

static void cpu_xscale_voltage_high(void)
{
  pwr_i2c_open();
  pwr_i2c_write( 0x0c, 0x1a );  // 1.35V - SlewRate 1
  pwr_i2c_close();
}

#ifdef DEBUG
static unsigned char cpu_xscale_voltage_read(void)
{
  unsigned char value=0xff;
  pwr_i2c_open();
  pwr_i2c_read( 0x0c, &value );
  pwr_i2c_close();
  return value;
}

#define CLKCFG(a)	asm("mrc p14, 0, %0, C6, C0, 0" : "=r"(a))
static void __debug_printk(void)
{
  unsigned long regClkcfg;

  CLKCFG(regClkcfg);
  printk(" - CCCR=%x\n",CCCR);
  printk(" - CLKCFG=%x\n",regClkcfg);
  printk(" - MDREFR=%x\n",MDREFR);
  printk(" - MSC0=%x\n",MSC0);
  printk(" - MSC1=%x\n",MSC1);
  printk(" - MSC2=%x\n",MSC2);
  printk(" - VOLTAGE=%x\n",cpu_xscale_voltage_read());
}
#else
#define __debug_printk()
#endif //DEBUG
#endif //PXA27X_SUSPEND

int pxa_suspend(void)
{
	unsigned long flags;
	unsigned long RTAR_buffer;
	unsigned long RTAR_buffer2;


	DPRINTK("%s\n",__func__);
	
	charge_status = 0;
	sharpsl_off_charge = 1;
	sharpsl_off_state = 1;
	off_charge_full_counter = 0;

	/* enable alarm int and so alarm time is near , so not suspednd. */
	if ( sharpsl_emergency_off == 0 ) {
	  if ( ( ( RTAR - RCNR ) < 10 ) || ( ( RCNR - RTAR ) < 10  ) ) {
	    printk("you have alarm.\n");
	    return 0;
	  }
	} else {
	  sharpsl_emergency_off = 0;
	}

#if defined(CONFIG_CPU_PXA27X)
	if ( ( sharpsl_chg_freq & 0x00010000 ) != 0 ) {
	  cpu_xscale_sl_change_speed_high();
	  cccr_reg = CCCR;
	  sharpsl_chg_freq &= ~0x00010000;
	  mdelay(500);
	  return 0;
	}
#endif

	/* off in maintenance */
	if (sharpsl_off_mode == 2) sharpsl_restart();

	/* set up pointer to sleep parameters */
	sleep_param = kmalloc (SLEEPDATA_SIZE*sizeof(long), GFP_ATOMIC);
	if (!sleep_param)
		return -ENOMEM;
	sleep_param_p = virt_to_phys(sleep_param);

	save_flags_cli(flags);
	clf();

	sys_ctx.osmr0 = OSMR0;
	sys_ctx.osmr1 = OSMR1;
	sys_ctx.osmr2 = OSMR2;
	sys_ctx.osmr3 = OSMR3;
	sys_ctx.oscr  = OSCR;
	//sys_ctx.ower = OWER;
	sys_ctx.oier = OIER;
    
#if defined(CONFIG_CPU_PXA27X)
	if ( CCCR != 0x02000210 ) {
	  cpu_xscale_sl_change_speed_high();
	}
#endif
	cccr_reg = CCCR;
	DPRINTK("FCS : CCCR = %x\n",cccr_reg);

	sharpsl_check_scoop_reg();

DO_SUSPEND:

	DPRINTK("pass_charge_flag = %d\n",pass_charge_flag);
	if ( !pass_charge_flag ) {
	  DPRINTK("check AC-adaptor\n");
	  if ( !charge_status && AC_IN_STATUS != 0){
	    DPRINTK("kick charging\n");
	    charge_status = 1;
#if defined(CONFIG_ARCH_PXA_SPITZ)
	    off_charge_check_counter = 0;
	    sharpsl_off_charge_battery( 1 );
#else
	    sharpsl_off_charge_battery();
#endif
	  }
	}

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

	wakeup_ready_to_suspend();

#if defined(CONFIG_ARCH_PXA_AKITA)
	sys_ctx.ioexp_port = get_port_ioexp();
	reset_port_ioexp(IOEXP_RESERVED_0 | IOEXP_RESERVED_1 | 
			 IOEXP_RESERVED_7 | IOEXP_BACKLIGHT_CONT | 
			 IOEXP_BACKLIGHT_ON | IOEXP_MIC_BIAS);
	set_port_ioexp(IOEXP_IR_ON);
	set_output_ioexp(IOEXP_ALL);
#endif

#if defined(PXA27X_SUSPEND)
	cpu_xscale_runmode();
	cpu_xscale_sl_disable_fastbus_mode();
	udelay(1);
	cpu_xscale_sl_change_speed_91();
	cpu_xscale_voltage_low();
#endif

	/* Scoop suspend */
	sharpsl_check_scoop_reg();

	/* SCP_LED_GREEN(PA11)  Keep */
	/* SCP_JK_B(PA12)       Keep */
	/* SCP_CHRG_ON(PA13)    Keep */
	/* SCP_MUTE_L(PA14)     Low  */
	/* SCP_MUTE_R(PA15)     Low  */
	/* SCP_CF_POWER(PA16)   Keep */
	/* SCP_LED_ORANGE(PA17) Keep */
	/* SCP_JK_A(PA18)	Low  */
	/* SCP_ADC_TEMP_ON(PA19)Low  */
	sys_ctx.scp_gpwr = SCP_REG_GPWR;
	SCP_REG_GPWR &= ~(SCP_MUTE_L|SCP_MUTE_R|SCP_JK_A|SCP_ADC_TEMP_ON);

	/* SCP2_IR_ON(PA11)          High */
	/* SCP2_AKIN_PULLUP(PA12)    Keep */
	/* SCP2_RESERVED_1(PA13)     High */
	/* SCP2_RESERVED_2(PA14)     Low  */
	/* SCP2_RESERVED_3(PA15)     Low  */
	/* SCP2_RESERVED_4(PA16)     Low  */
	/* SCP2_BACKLIGHT_CONT(PA17) Low  */
	/* SCP2_BACKLIGHT_ON(PA18)   Low  */
	/* SCP2_MIC_BIAS(PA19)       Low  */
#if !defined(CONFIG_ARCH_PXA_AKITA)
	sys_ctx.scp2_gpwr = SCP2_REG_GPWR;
	SCP2_REG_GPWR &= ~(SCP2_RESERVED_2 | SCP2_RESERVED_3 | 
			   SCP2_RESERVED_4 | SCP2_BACKLIGHT_CONT |
			   SCP2_BACKLIGHT_ON | SCP2_MIC_BIAS);
	SCP2_REG_GPWR |= (SCP2_IR_ON | SCP2_RESERVED_1);
#endif

	DPRINTK("PWER - %08x\n",PWER);
	DPRINTK("PRER - %08x\n",PRER);
	DPRINTK("PFER - %08x\n",PFER);
	DPRINTK("PEDR - %08x\n",PEDR);
	DPRINTK("PSLR - %08x\n",PSLR);
	DPRINTK("PKWR - %08x\n",PKWR);
	DPRINTK("PKSR - %08x\n",PKSR);
	DPRINTK("SCP_REG_GPCR - %08x\n",SCP_REG_GPCR);
	DPRINTK("SCP_REG_GPWR - %08x\n",SCP_REG_GPWR);
	DPRINTK("SCP2_REG_GPCR - %08x\n",SCP2_REG_GPCR);
	DPRINTK("SCP2_REG_GPWR - %08x\n",SCP2_REG_GPWR);

	sys_ctx.gpdr0 = GPDR0;
	sys_ctx.gpdr1 = GPDR1;
	sys_ctx.gpdr2 = GPDR2;
	sys_ctx.gpdr3 = GPDR3;

        sys_ctx.grer0 = GRER0;
        sys_ctx.grer1 = GRER1;
        sys_ctx.grer2 = GRER2;
        sys_ctx.grer3 = GRER3;

        sys_ctx.gfer0 = GFER0;
        sys_ctx.gfer1 = GFER1;
        sys_ctx.gfer2 = GFER2;
        sys_ctx.gfer3 = GFER3;

        sys_ctx.gafr0_l = GAFR0_L;
        sys_ctx.gafr1_l = GAFR1_L;
        sys_ctx.gafr2_l = GAFR2_L;
        sys_ctx.gafr3_l = GAFR3_L;

        sys_ctx.gafr0_u = GAFR0_U;
        sys_ctx.gafr1_u = GAFR1_U;
        sys_ctx.gafr2_u = GAFR2_U;
        sys_ctx.gafr3_u = GAFR3_U;

	sys_ctx.gplr0 = GPLR0;
	sys_ctx.gplr1 = GPLR1;
	sys_ctx.gplr2 = GPLR2;
	sys_ctx.gplr3 = GPLR3;

        sys_ctx.iclr = ICLR;
        sys_ctx.icmr = ICMR;
        sys_ctx.iccr = ICCR;
	ICMR = 0;

        sys_ctx.mecr = MECR;
        sys_ctx.mcmem0 = MCMEM0;
        sys_ctx.mcmem1 = MCMEM1;
        sys_ctx.mcatt0 = MCATT0;
        sys_ctx.mcatt1 = MCATT1;
        sys_ctx.mcio0 = MCIO0;
        sys_ctx.mcio1 = MCIO1;

        sys_ctx.ffier = FFIER;
        sys_ctx.fflcr = FFLCR;
        sys_ctx.ffmcr = FFMCR;
        sys_ctx.ffspr = FFSPR;
        sys_ctx.ffisr = FFISR;
	FFLCR |= 0x80;
        sys_ctx.ffdll = FFDLL;
        sys_ctx.ffdlh = FFDLH;
	FFLCR &= 0xef;

        sys_ctx.cken = CKEN;
	CKEN = CKEN22_MEM | CKEN19_KEY;

	/* nRESET_OUT Disable */
	PSLR |= PSLR_SL_ROD;

	/* Clear reset status */
	RCSR = RCSR_HWR | RCSR_WDR | RCSR_SMR | RCSR_GPR;

	/* Stop 3.6MHz and drive HIGH to PCMCIA and CS */
	PCFR = PCFR_GPR_EN | PCFR_OPDE;

	/* GPIO Sleep Register */
	PGSR0 = 0x00144018;
	PGSR1 = 0x00EF0000;
#if defined(CONFIG_ARCH_PXA_AKITA)
	PGSR2 = 0x2121C000;
	PGSR3 = 0x00600400;
#else
	PGSR2 = 0x0121C000;
	PGSR3 = 0x00600000;
#endif
	PGSR0 &= ~GPIO_G0_STROBE_BIT;
	PGSR1 &= ~GPIO_G1_STROBE_BIT;
	PGSR2 &= ~GPIO_G2_STROBE_BIT;
	PGSR3 &= ~GPIO_G3_STROBE_BIT;
	PGSR2 |= GPIO_bit(GPIO_KEY_STROBE0);

	GPSR(GPIO18_RDY) = GPIO_bit(GPIO18_RDY);
	set_GPIO_mode(GPIO18_RDY|GPIO_OUT);
	GPDR0 = 0xD01C4418;
	GPDR1 = 0xFCEFBD21;
#if defined(CONFIG_ARCH_PXA_AKITA)
	GPDR2 = 0x33A5FFFF;
	GPDR3 = 0x01E3E50C;
#else
	GPDR2 = 0x13A5FFFF;
	GPDR3 = 0x01E3E10C;
#endif

	/* set resume return address */
#ifdef CONFIG_XIP_ROM
	PSPR = (unsigned long)cpu_pxa_resume & 0x00ffffff;
#else
	PSPR = virt_to_phys(cpu_pxa_resume);
#endif
	cpu_pxa_do_suspend();

	PSPR = 0;

//X	sharpsl_wakeup_check();
	check_wakeup_virtual();

        CKEN |= CKEN6_FFUART;

        FFMCR = sys_ctx.ffmcr;
        FFSPR = sys_ctx.ffspr;
        FFLCR = sys_ctx.fflcr;
	FFLCR |= 0x80;
        FFDLH = sys_ctx.ffdlh;
        FFDLL = sys_ctx.ffdll;
        FFLCR = sys_ctx.fflcr;
        FFISR = sys_ctx.ffisr;
        FFFCR = 0x07;
        FFIER = sys_ctx.ffier;

	/* restore registers */
        GPDR0 = sys_ctx.gpdr0;
        GPDR1 = sys_ctx.gpdr1;
        GPDR2 = sys_ctx.gpdr2;
        GPDR3 = sys_ctx.gpdr3;

        GRER0 = sys_ctx.grer0;
        GRER1 = sys_ctx.grer1;
        GRER2 = sys_ctx.grer2;
        GRER3 = sys_ctx.grer3;

        GFER0 = sys_ctx.gfer0;
        GFER1 = sys_ctx.gfer1;
        GFER2 = sys_ctx.gfer2;
        GFER3 = sys_ctx.gfer3;

        GAFR0_L = sys_ctx.gafr0_l;
        GAFR1_L = sys_ctx.gafr1_l;
        GAFR2_L = sys_ctx.gafr2_l;
        GAFR3_L = sys_ctx.gafr3_l;

        GAFR0_U = sys_ctx.gafr0_u;
        GAFR1_U = sys_ctx.gafr1_u;
        GAFR2_U = sys_ctx.gafr2_u;
        GAFR3_U = sys_ctx.gafr3_u;

        GPSR0 = sys_ctx.gplr0 & sys_ctx.gpdr0;
        GPSR1 = sys_ctx.gplr1 & sys_ctx.gpdr1;
        GPSR2 = sys_ctx.gplr2 & sys_ctx.gpdr2;
        GPSR3 = sys_ctx.gplr3 & sys_ctx.gpdr3;

        GPCR0 = ~sys_ctx.gplr0 & sys_ctx.gpdr0;
        GPCR1 = ~sys_ctx.gplr1 & sys_ctx.gpdr1;
        GPCR2 = ~sys_ctx.gplr2 & sys_ctx.gpdr2;
        GPCR3 = ~sys_ctx.gplr3 & sys_ctx.gpdr3;

        MECR = sys_ctx.mecr;
        MCMEM0 = sys_ctx.mcmem0;
        MCMEM1 = sys_ctx.mcmem1;
        MCATT0 = sys_ctx.mcatt0;
        MCATT1 = sys_ctx.mcatt1;
        MCIO0 = sys_ctx.mcio0;
        MCIO1 = sys_ctx.mcio1;

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

	/* Scoop resume */
	sharpsl_check_scoop_reg();

	SCP_REG_GPWR = sys_ctx.scp_gpwr;
#if !defined(CONFIG_ARCH_PXA_AKITA)
	SCP2_REG_GPWR = sys_ctx.scp2_gpwr;
#endif

#if defined(PXA27X_SUSPEND)
	cpu_xscale_voltage_high();
	cpu_xscale_sl_change_speed_208();
	udelay(1);
	cpu_xscale_sl_enable_fastbus_mode();
	/* cpu_xscale_turbomode(); */
#endif

#if defined(CONFIG_ARCH_PXA_AKITA)
        i2c_init();
	set_port_ioexp(sys_ctx.ioexp_port);
	reset_port_ioexp(~sys_ctx.ioexp_port);
	set_output_ioexp(IOEXP_ALL);
#endif

	pxa_ssp_init();

#if defined(CONFIG_CPU_PXA27X)
	usbh_interrupt_init();
#endif

	DPRINTK("PKWR - %08x\n",PKWR);
	DPRINTK("PKSR - %08x\n",PKSR);

	if ( sharpsl_alarm_flag ) {
	  RTAR_buffer2 = RTAR;
	  RTAR = RTAR_buffer;
	  DPRINTK("back the ALARM Time\n");
	}

	if ( sharpsl_alarm_flag && ( RCNR == RTAR_buffer2 ) ) {
#if defined(CONFIG_ARCH_PXA_SPITZ)
	  off_charge_check_counter++;
	  //printk("charge time out counter = %d\n",off_charge_check_counter);
	  if ( off_charge_check_counter >= BATTERY_CHARGE_FULL_COUNT ) {
	    sharpsl_set_charge_full();
	    off_charge_check_counter = 0;
	    printk("charge time out!\n");
	    goto DO_SUSPEND;
	  }

	  if ( sharpsl_off_charge_battery( 0 ) ) {
#else
	  if ( sharpsl_off_charge_battery() ) {
#endif
	    DPRINTK("charge timer \n");
	    goto DO_SUSPEND;
	  }
	}

	check_wakeup_logical();

	/* ----- hardware resume ----- */
	if ( !sharpsl_wakeup_hook() ) {
	        printk("return to suspend ....\n");
		goto DO_SUSPEND;
	}

	if ( ( (GPLR(GPIO_BAT_COVER) & GPIO_bit(GPIO_BAT_COVER)) == 0 )
	     || ( !sharpsl_off_mode && chkFatalBatt() == 0 ) ) {
	        printk("return to suspend (fatal) ....\n");
		goto DO_SUSPEND;
	}

#if defined(CONFIG_CPU_PXA27X)
	PMCR = 0x00;
#else
	PMCR = 0x01;
#endif

	if ( sharpsl_off_mode )
	  sharpsl_restart();

#if defined(CONFIG_CPU_PXA27X)
	cpu_xscale_sl_change_speed_high();
#endif
	cccr_reg = CCCR;
	printk("FCS : CCCR = %x\n",cccr_reg);
	sharpsl_off_charge = 0;

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

 	xtime.tv_sec = RCNR;

	restore_flags(flags);
	
	kfree (sleep_param);

	sharpsl_off_state = 0;

	return 0;
}

int pm_do_suspend(void)
{
	int retval;
	
	DPRINTK("yea\n");
	
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

static int sharpsl_off_thread(void* unused)
{
	int time_cnt = 0;

	// daemonize();
	strcpy(current->comm, "off_thread");
	sigfillset(&current->blocked);

	while(1) {
		interruptible_sleep_on(&wq_off);
		DPRINTK("start off sequence ! \n");

		if(sharpsl_main_bk_flag){
		  printk("%s: suspend at once!\n",__func__);
		  handle_scancode(SLKEY_OFF|KBDOWN , 1);
		  mdelay(10);
		  handle_scancode(SLKEY_OFF|KBUP   , 0);
		}else{
		  printk("%s: later suspend !\n",__func__);
		  if(!sharpsl_request_off)
		    sharpsl_request_off = 1;
		}
	}
	return 0;
}

void sharpsl_emerge_off(int irq, void *dev_id, struct pt_regs *regs)
{
  //printk("%s:\n",__func__);

#if defined(CONFIG_ARCH_PXA_SPITZ)
  if( (GPLR(GPIO_FATAL_BAT) & GPIO_bit(GPIO_FATAL_BAT)) == 0 ) {
    mdelay(1); /* noise? */
    if( (GPLR(GPIO_FATAL_BAT) & GPIO_bit(GPIO_FATAL_BAT)) == 0 ) {
      printk("fatal battery is detected\n");      
      wake_up(&wq_off);
      return;
    }
  }
#endif

  /* noise ? */
  mdelay(10);
  if ( 0x1234ABCD != regs && GPLR(GPIO_BAT_COVER) & GPIO_bit(GPIO_BAT_COVER) ) {
    printk("cancel emergency off\n");
    return;
  }

  wake_up(&wq_off);
}


/*
 * Initialize power interface
 */
static int __init pm_init(void)
{
  int err;
  register_sysctl_table(pm_dir_table, 1);

  /* Set transition detect */
  set_GPIO_IRQ_edge( GPIO_BAT_COVER , GPIO_FALLING_EDGE );
#if defined(CONFIG_ARCH_PXA_SPITZ)
  set_GPIO_IRQ_edge( GPIO_FATAL_BAT , GPIO_FALLING_EDGE );
#endif

  /* this registration can be done in init/main.c. */
  if( err = request_irq(IRQ_GPIO_BAT_COVER, sharpsl_emerge_off , 
			SA_INTERRUPT, "batok", NULL) ){
    printk("batok install error %d\n",err);
  }else{
    enable_irq(IRQ_GPIO_BAT_COVER);
    printk("batok installed\n");
  }

#if defined(CONFIG_ARCH_PXA_SPITZ)
  if (err = request_irq(IRQ_GPIO_FATAL_BAT, sharpsl_emerge_off,
			 SA_INTERRUPT, "fatal", NULL)) {
    printk("Could not get irq %d.\n", IRQ_GPIO_FATAL_BAT);
    return;
  }
#endif

  kernel_thread(sharpsl_off_thread,  NULL, CLONE_FS | CLONE_FILES | CLONE_SIGHAND | SIGCHLD);
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



EXPORT_SYMBOL(PrintParamTable);
EXPORT_SYMBOL(PrintParamTable_P);


