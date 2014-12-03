/*
 * SA1110 Power Management Routines
 *
 * (C) Copyright 2001 Lineo Japan, Inc.
 *
 * May be copied or modified under the terms of the GNU General Public
 * License.  See linux/COPYING for more information.
 *
 * Based on linux/arch/arm/mach-sa1100/power.c
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
 *	14-Nov-2001 SHARP 
 *	13-Jul-2002 SHARP      OSMR0 is set to OSCR + 10msec in sabinal_suspend()
 *	27-AUG-2002 Steve Lin for Discovery
 *
 */


/*
 *  Debug macros 
 */
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

#ifdef CONFIG_ARCH_COTULLA
#include <unistd.h>
#include <asm/irq.h>
#include <asm/arch/irqs.h>
#include <asm/arch/cotulla.h>
#include "cotulla_param.h"
#include <asm/arch/discovery_asic3.h>
#include <asm/sharp_apm.h>
#endif

static unsigned int apm_wakeup_src_mask = 0;
static unsigned int apm_wakeup_factor = 0;

static struct {
        u32 osmr0, osmr1, osmr2, osmr3, oscr;
        u32 ower, oier;

        u32 gpdr0, gpdr1, gpdr2, grer, gfer, gafr;
        u32 icmr, iclr, iccr;
        u32 ppdr, ppsr, ppar, psdr;
        u32 mecr;
		/* FIXME this part should in the serial port driver ,Control register*/
        u32 serutcr0, serutcr1, serutcr2, serutcr3;
		u32 lcm_tasd, lcm_spict, lcm_gpo;
        u32 lcm_gpe, lcm_spimd;
        u32 scp_gpwr;
        
        u16 pstsa,pstsb,pstsc,pstsd;
} sys_ctx;

extern unsigned long *sleep_param;	/* virtual address */
extern unsigned long  sleep_param_p;	/* physical address */

extern void cpu_sabinal_resume(void);	/* function the bootloader will call back */

extern int cpu_sabinal_do_suspend(void);
extern unsigned short chkFatalBatt(void);

void PrintParamTable(void);

static u32 alarm_enable=0;

int discovery_restart(void)
{
  int flag = 1;

  OSMR3 = OSCR+0x100;
  OWER = 0x01;
  OIER |= 0x08;

  while(1) {
    if ( flag++ > 0x20000000 ) break;
  }

}


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

int sabinal_suspend(void)
{
	unsigned long   flags;
	unsigned long	RTAR_buffer;
	unsigned int	wakeupsrc;
	
	/* set up pointer to sleep parameters */
	sleep_param = kmalloc (SLEEPDATA_SIZE*sizeof(long), GFP_ATOMIC);
	if (!sleep_param)
		return -ENOMEM;
	sleep_param_p = virt_to_phys(sleep_param);
	
#if defined (CONFIG_ARCH_COTULLA)
	
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
		
		if (alarm_enable && (RTSR & RTSR_ALE)) {
			RTAR_buffer = RTAR;
			ASIC3_SLEEP_PARAM6 = (RTAR_buffer & 0x00000fff);
			ASIC3_SLEEP_PARAM7 = ((RTAR_buffer & 0x00fff000) >> 12);
			ASIC3_SLEEP_PARAM8 = (((RTAR_buffer & 0xff000000) >> 24) | 0x1000);
		} else {
			ASIC3_SLEEP_PARAM6 = 0x00;
			ASIC3_SLEEP_PARAM7 = 0x00;
			ASIC3_SLEEP_PARAM8 = 0x00;
		}
	}

	save_flags_cli(flags);

    sys_ctx.gpdr0  = GPDR(0);
	sys_ctx.gpdr1 = GPDR(1);
    sys_ctx.gpdr2 = GPDR(2);

    sys_ctx.osmr0 = OSMR0;
    sys_ctx.osmr1 = OSMR1;
    sys_ctx.osmr2 = OSMR2;
    sys_ctx.osmr3 = OSMR3;
    sys_ctx.oscr  = OSCR;
	sys_ctx.ower = OWER;
    sys_ctx.oier = OIER;
    
    sys_ctx.pstsa=ASIC3_GPIO_PSTS_A;
    sys_ctx.pstsb=ASIC3_GPIO_PSTS_B;
    sys_ctx.pstsc=ASIC3_GPIO_PSTS_C;
    sys_ctx.pstsd=ASIC3_GPIO_PSTS_D;

	ASIC3_GPIO_INTSTAT_A = 0;
	ASIC3_GPIO_INTSTAT_B = 0;
	ASIC3_GPIO_INTSTAT_C = 0;
	ASIC3_GPIO_INTSTAT_D = 0;

	    
    cli();
    
#else
	cli();
#endif

	/* set resume return address */
#ifdef CONFIG_XIP_ROM
	PSPR = (unsigned long)cpu_sabinal_resume & 0x00ffffff;
#else
	PSPR = virt_to_phys(cpu_sabinal_resume);
#endif

	cpu_sabinal_do_suspend();
	

#if defined (CONFIG_ARCH_COTULLA)
	
	//make sure clear RCSR
	RCSR = 0x00000004;
	
//    OSMR0 = sys_ctx.osmr0;
    OSMR0 = sys_ctx.oscr + LATCH;
    OSMR1 = sys_ctx.osmr1;
    OSMR2 = sys_ctx.osmr2;
    OSMR3 = sys_ctx.osmr3;
    OSCR = sys_ctx.oscr;
    OWER = sys_ctx.ower;
    OIER = sys_ctx.oier;

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
 
 	xtime.tv_sec = RCNR;

    GPDR(0) = sys_ctx.gpdr0 ;
	GPDR(1) = sys_ctx.gpdr1 ;
	GPDR(2) = sys_ctx.gpdr2 ;

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
       
	restore_flags(flags);
	sti();
#else
	sti();
#endif
	
	kfree (sleep_param);
	return 0;
}

int pm_do_suspend(void)
{
	int retval;
	
	DPRINTK("yea\n");
	
	retval = pm_send_all(PM_SUSPEND, (void *)2);
	if (retval) 
		return retval;

	retval = sabinal_suspend();

	retval = pm_send_all(PM_RESUME, (void *)0);
	if (retval)
		return retval;

	return 0;
}

EXPORT_SYMBOL(sabinal_suspend);
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

/*
 * Initialize power interface
 */
static int __init pm_init(void)
{
	register_sysctl_table(pm_dir_table, 1);

	return 0;
}

__initcall(pm_init);


void PrintParamTable(void) {
	int i;
	
	for (i=0; i < SLEEPDATA_SIZE; i=i+4) {
	
		printk("[%d] %8x %8x %8x %8x\n", i, *(unsigned long *)(sleep_param+i)
											, *(unsigned long *)(sleep_param+i+1)
											, *(unsigned long *)(sleep_param+i+2)
											, *(unsigned long *)(sleep_param+i+3));
		
	}
	
	return;
}

void PrintRegs(void) {
	int i;
	
	for (i=0; i < SLEEPDATA_SIZE; i=i+4) {
	
		printk("[%d] %8x %8x %8x %8x\n", i, *(unsigned long *)(sleep_param+i)
											, *(unsigned long *)(sleep_param+i+1)
											, *(unsigned long *)(sleep_param+i+2)
											, *(unsigned long *)(sleep_param+i+3));
		
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
	apm_wakeup_src_mask	= SetValue;
}

u32 get_apm_wakeup_src_mask(void)
{
	return (u32)apm_wakeup_src_mask;
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

