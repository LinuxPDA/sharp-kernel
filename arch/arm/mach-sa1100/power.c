/*
 * SA1110 Power Management Routines
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
#include <linux/malloc.h>
#include <linux/sysctl.h>
#include <linux/acpi.h>
#include <linux/delay.h>

#include <asm/mach-types.h>
#include <asm/hardware.h>
#include <asm/memory.h>
#include <asm/system.h>
#include <asm/proc-fns.h>
#include <asm/arch/power.h>

#if defined(CONFIG_SA1100_COLLIE)
#include <unistd.h>
#include <asm/irq.h>
#include <asm/arch/irqs.h>
#include <asm/ucb1200.h>
#include <asm/arch/tc35143.h>
#endif

static struct {
        u32 osmr0, osmr1, osmr2, osmr3, oscr;
        u32 ower, oier;

        u32 gpdr, grer, gfer, gafr;
        u32 icmr, iclr, iccr;
        u32 ppdr, ppsr, ppar, psdr;
        u32 mecr;
		/* FIXME this part should in the serial port driver ,Control register*/
        u32 serutcr0, serutcr1, serutcr2, serutcr3;
	u32 lcm_tasd, lcm_spict, lcm_gpo;
        u32 lcm_gpe, lcm_spimd;
        u32 scp_gpwr;
} sys_ctx;

extern unsigned long *sleep_param;	/* virtual address */
extern unsigned long  sleep_param_p;	/* physical address */
extern void cpu_sa1100_resume(void);	/* function the bootloader will call back */

extern int cpu_sa1100_do_suspend(void);
extern unsigned short chkFatalBatt(void);

#if defined(CONFIG_SA1100_COLLIE)
#define R_WAKEUP_SRC	(GPIO_AC_IN | GPIO_CO)
#define F_WAKEUP_SRC	(GPIO_ON_KEY | GPIO_WAKEUP \
			| GPIO_nREMOCON_INT | GPIO_GA_INT | GPIO_CF_IRQ)
#define WAKEUP_SRC	(R_WAKEUP_SRC | F_WAKEUP_SRC | PWER_RTC)
#define WAKEUP_DEF_SRC	( GPIO_ON_KEY | GPIO_AC_IN | GPIO_CO | PWER_RTC );

#ifdef DEBUG
static char wakeup_check_log[512] = { 0 };
static char *wakeup_check_log_p;
#define DLOGSTART()	wakeup_check_log_p = wakeup_check_log
#define DPUTLOG(fmt, args...)	\
{\
       	sprintf(wakeup_check_log_p, "%s: " fmt, __FUNCTION__ , ## args);\
       	wakeup_check_log_p = wakeup_check_log + strlen(wakeup_check_log);\
}
#define DPRINTLOG()	DPRINTK("%s", wakeup_check_log)
#else
#define DLOGSTART()
#define DPUTLOG(fmt, args...)
#define DPRINTLOG()
#endif

u32 apm_wakeup_src_mask = WAKEUP_DEF_SRC;
u32 apm_wakeup_factor = 0;

u32 collie_emergency_off = 0;

extern collie_battery_charge_hook(int mode);
//void (*collie_battery_charge_hook)(int) = NULL;
int (*collie_wakeup_key_scan_hook)(void) = NULL;
int (*collie_wakeup_remote_control_hook)(void) = NULL;



static int collie_wakeup_check(void)
{
	int i;
	u32 gplr;

	DLOGSTART();
	DPUTLOG("apm_wakeup_src_mask=%08x\n", apm_wakeup_src_mask);

	/* setting GPIO */
	GPDR = sys_ctx.gpdr;
	GAFR = sys_ctx.gafr;
	GPDR &= ~WAKEUP_SRC;
	GAFR &= ~WAKEUP_SRC;
	gplr = GPLR;

	DPUTLOG("GAFR=%08x\n", GAFR);
	DPUTLOG("GPDR=%08x\n", GPDR);
	DPUTLOG("GEDR=%08x\n", GEDR);
	DPUTLOG("GRER=%08x\n", GRER);
	DPUTLOG("GFER=%08x\n", GFER);
	DPUTLOG("GPLR=%08x\n", gplr);
	DPUTLOG("ICIP=%08x\n", ICIP);
#if 1
	apm_wakeup_factor = GEDR & WAKEUP_SRC & apm_wakeup_src_mask;
	if ( RTSR & 0x1 )
		apm_wakeup_factor |= 0x80000000;		/* ALARM */
	GEDR = WAKEUP_SRC;
#else
	apm_wakeup_factor = ICIP & 0x7ff;	/* GPIO0 - GPIO10 */
	if ( ICIP & IC_GPIO11_27 ) {		/* GPIO11 - GPIO27 */
		for (i = 11; i <= 27; i++) {
			if ( GEDR & GPIO_GPIO(i) )
				apm_wakeup_factor |= GPIO_GPIO(i);
		}	
	}
	apm_wakeup_factor &= WAKEUP_SRC & apm_wakeup_src_mask;
#endif
	DPUTLOG("apm_wakeup_factor=%08x\n", apm_wakeup_factor);
	if ( !apm_wakeup_factor )
		return 0;			/* no wakeup factor */

	/* Faulty operation check */
	for (i = 0; i <= 27; i++) {
		if ( apm_wakeup_factor & GPIO_GPIO(i) ) {
			if ( (GRER & apm_wakeup_src_mask & GPIO_GPIO(i)) 
							&& !(gplr & GPIO_GPIO(i)) ) {
				DPUTLOG("Faulty rising operation GPIO=%d\n", i);
				apm_wakeup_factor &= ~GPIO_GPIO(i);
			}
			if ( (GFER & apm_wakeup_src_mask & GPIO_GPIO(i)) 
							&& (gplr & GPIO_GPIO(i)) ) {
				DPUTLOG("Faulty falling operation GPIO=%d\n", i);
				apm_wakeup_factor &= ~GPIO_GPIO(i);
			}
		}
	}

	DPUTLOG("apm_wakeup_factor=%08x\n", apm_wakeup_factor);
	if ( !apm_wakeup_factor )
		return 0;			/* no wakeup factor */

}


static int collie_wakeup_hook(void)
{
	u32 gplr = GPLR;
	int is_resume = 0;



	if ( (apm_wakeup_factor & GPIO_AC_IN)  && collie_battery_charge_hook ) {
		if ( gplr & GPIO_AC_IN ) {
			collie_battery_charge_hook(2);	/* charge on */
		} else 
		if ( !( gplr & GPIO_AC_IN ) ) {
			collie_battery_charge_hook(1);	/* charge off */
		}
	}
	if ( (apm_wakeup_factor & GPIO_CO) && collie_battery_charge_hook ) {
		collie_battery_charge_hook(0);  /* charge off */
	}
	if ( apm_wakeup_factor & GPIO_ON_KEY )
		is_resume |= GPIO_ON_KEY;
	if ( apm_wakeup_factor & GPIO_WAKEUP )
		is_resume |= GPIO_WAKEUP;
	if ( apm_wakeup_factor & GPIO_nREMOCON_INT ) {
		if( !collie_wakeup_remote_control_hook )
			is_resume |= GPIO_nREMOCON_INT;
		else if( collie_wakeup_remote_control_hook() )
			is_resume |= GPIO_nREMOCON_INT;
	}
	if ( (apm_wakeup_factor & GPIO_GA_INT) && (LCM_KIC & 1) 
				&& collie_wakeup_key_scan_hook )
		is_resume |= collie_wakeup_key_scan_hook();	/* wake up key check */
	if ( apm_wakeup_factor & GPIO_CF_IRQ )
		is_resume |= GPIO_CF_IRQ;
	if ( apm_wakeup_factor & PWER_RTC )
		is_resume |= PWER_RTC;
	
	DPUTLOG("is_resume=%d\n", is_resume);
	return is_resume;
}
#endif


int sa1110_suspend(void)
{
	unsigned long   flags;



#if defined(CONFIG_SA1100_COLLIE)
	/* enable alarm int and so alarm time is near , so not suspend. */
	if ( collie_emergency_off == 0 ) {
	  if ( ( ( RTAR - RCNR ) < 10 ) || ( ( RCNR - RTAR ) < 10  ) ) {
	    printk("you have alarm.\n");
	    return 0;
	  }
	} else {
	  collie_emergency_off = 0;
	}
#endif

	/* set up pointer to sleep parameters */
	sleep_param = kmalloc (SLEEP_PARAM_SIZE*sizeof(long), GFP_ATOMIC);
	if (!sleep_param)
		return -ENOMEM;
	sleep_param_p = virt_to_phys(sleep_param);
	
	DPRINTK("*** sa1100_suspend()\n");
	DPRINTK("*** sleep_param_p = 0x%08lx\n", sleep_param_p);
	DPRINTK("*** resume_return() = 0x%08lx\n", virt_to_phys(cpu_sa1100_resume));


	/* disable interrupts */
#if defined(CONFIG_SA1100_COLLIE)


	if ( !in_interrupt() ) {
	  __set_current_state(TASK_UNINTERRUPTIBLE);
	  schedule_timeout( 1 * HZ);

	  /* wait for Power On key release */
	  while ( !(GPLR & GPIO_ON_KEY) ) ;
	}

	save_flags_cli(flags);


	// save timer registers
	//RCNR = xtime.tv_sec;

	/* save vital registers */
        sys_ctx.osmr0 = OSMR0;
        sys_ctx.osmr1 = OSMR1;
        sys_ctx.osmr2 = OSMR2;
        sys_ctx.osmr3 = OSMR3;
        sys_ctx.oscr  = OSCR;
	//sys_ctx.ower = OWER;
        sys_ctx.oier = OIER;
DO_SUSPEND:
#else
	cli();
#endif


#if defined(CONFIG_SA1100_COLLIE)
	/* ----- hardware suspend ----- */
	/* LOCOMO suspend */
	sys_ctx.lcm_gpo   = LCM_GPO;	/* GPIO */
	LCM_GPO = 0x00;
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

	/* TC35143F suspend */
	ucb1200_suspend();

	/* Scoop suspend */
	SCP_REG_GPWR &= ~SCP_AMP_ON;
	sys_ctx.scp_gpwr = SCP_REG_GPWR;
	SCP_REG_GPWR = ( sys_ctx.scp_gpwr & SCP_CHARGE_ON );

	/* ----- hardware suspend ----- */
#endif


#ifndef CONFIG_SA1100_COLLIE
	RCNR = xtime.tv_sec;

	/* save vital registers */
        sys_ctx.osmr0 = OSMR0;
        sys_ctx.osmr1 = OSMR1;
        sys_ctx.osmr2 = OSMR2;
        sys_ctx.osmr3 = OSMR3;
        sys_ctx.oscr  = OSCR;
        sys_ctx.ower = OWER;
        sys_ctx.oier = OIER;
#endif

        sys_ctx.gpdr = GPDR;
        sys_ctx.grer = GRER;
        sys_ctx.gfer = GFER;
        sys_ctx.gafr = GAFR;

        sys_ctx.iclr = ICLR;
        sys_ctx.icmr = ICMR;
        sys_ctx.iccr = ICCR;

        sys_ctx.ppdr = PPDR;
        sys_ctx.ppsr = PPSR;
        sys_ctx.ppar = PPAR;
        sys_ctx.psdr = PSDR;

        sys_ctx.mecr = MECR;


	/* XXX Should be in serial driver */
	/* Save serial port context for quick restore */
#if defined(CONFIG_SA1100_ASSABET)
	if (!machine_has_neponset()) {
            sys_ctx.serutcr0 = Ser1UTCR0;
            sys_ctx.serutcr1 = Ser1UTCR1;
            sys_ctx.serutcr2 = Ser1UTCR2;
            sys_ctx.serutcr3 = Ser1UTCR3;
	} else {
            sys_ctx.serutcr0 = Ser3UTCR0;
            sys_ctx.serutcr1 = Ser3UTCR1;
            sys_ctx.serutcr2 = Ser3UTCR2;
            sys_ctx.serutcr3 = Ser3UTCR3;
	}
	DPRINTK("save serial port 1 value\n");
#elif defined(CONFIG_SA1100_COLLIE)
#if defined(CONFIG_COLLIE_TS)
        sys_ctx.serutcr0 = Ser1UTCR0;
        sys_ctx.serutcr1 = Ser1UTCR1;
        sys_ctx.serutcr2 = Ser1UTCR2;
        sys_ctx.serutcr3 = Ser1UTCR3;
#else
        sys_ctx.serutcr0 = Ser3UTCR0;
        sys_ctx.serutcr1 = Ser3UTCR1;
        sys_ctx.serutcr2 = Ser3UTCR2;
        sys_ctx.serutcr3 = Ser3UTCR3;
#endif
        DPRINTK("save serial port 1 value\n");
#elif defined(CONFIG_SA1100_FREEBIRD)
        sys_ctx.serutcr0 = Ser1UTCR0;
        sys_ctx.serutcr1 = Ser1UTCR1;
        sys_ctx.serutcr2 = Ser1UTCR2;
        sys_ctx.serutcr3 = Ser1UTCR3;
              DPRINTK("save serial port 1 value\n");
#else
        sys_ctx.serutcr0 = Ser3UTCR0;
        sys_ctx.serutcr1 = Ser3UTCR1;
        sys_ctx.serutcr2 = Ser3UTCR2;
        sys_ctx.serutcr3 = Ser3UTCR3;
#endif


	/* set wake up sources and power management registers */
	if (machine_is_accelent_sa()) {
		PWER = 0x1;
		GRER = 0x1;
		GFER = 0x1;
		GEDR = 0x1;
	} else if (machine_is_bitsy()) {
                /* will add more sources later */		
	
		/* Wake up by releasing the power button or by RTC */
		PWER = 0x1 | (1 << 31); 
		GRER = 0x1; 
		/* Wakened by pushing button will cause cpu 
		 * by following button release,
		 * so disable falling edge detection */
		GFER = 0x0;
		GEDR = 0x1; 

		/* Clear previous reset status */
		RCSR = RCSR_HWR | RCSR_SWR | RCSR_WDR | RCSR_SMR;

        	PCFR = PCFR_OPDE | PCFR_FP | PCFR_FS;
        	PGSR = 0x00000000;
	} else if (machine_is_assabet()) {
		PWER = 0x3 | (1 << 31); 
		GRER = 0x3;
		GFER = 0x0;
		GEDR = 0x3;

		RCSR = RCSR_HWR | RCSR_SWR | RCSR_WDR | RCSR_SMR;
		/* Clear reset status */
		PCFR = PCFR_OPDE | PCFR_FP | PCFR_FS;
		PGSR = 0x00000000;
	} else if (machine_is_collie()) {
		int i;
		PWER = WAKEUP_SRC & apm_wakeup_src_mask;
		GRER = R_WAKEUP_SRC & apm_wakeup_src_mask;
		GFER = F_WAKEUP_SRC & apm_wakeup_src_mask;
		GEDR = WAKEUP_SRC & apm_wakeup_src_mask;
		for (i = 0; i <=27; i++) {
			if ( GRER & GFER & GPIO_GPIO(i) ) {
				if ( GPLR & GPIO_GPIO(i) )
					GRER &= ~GPIO_GPIO(i); 
				else
					GFER &= ~GPIO_GPIO(i); 
			}
		}


		/* Clear reset status */
		RCSR = RCSR_HWR | RCSR_SWR | RCSR_WDR | RCSR_SMR;

		/* Stop 3.6MHz and drive HIGH to PCMCIA and CS */
		PCFR = PCFR_OPDE;

		/* GPIO Sleep Register */
		PGSR = 0x00040000;

		/* PSDR setting */
		PSDR = 0x000aa000;

	} else if (machine_is_freebird()) {
		PWER = 0x1;
		GRER = 0x1;
		GFER = 0x0;
		GEDR = 0x1;

		RCSR = RCSR_HWR | RCSR_SWR | RCSR_WDR | RCSR_SMR;
		PCFR = PCFR_OPDE | PCFR_FP | PCFR_FS;
		PGSR = 0x0;
		DPRINTK("just print at freebird board\n");
	}

	/* set resume return address */
#ifdef CONFIG_XIP_ROM
	PSPR = (unsigned long)cpu_sa1100_resume & 0x00ffffff;
#else
	PSPR = virt_to_phys(cpu_sa1100_resume);
#endif

	cpu_sa1100_do_suspend();
#if defined(CONFIG_SA1100_COLLIE)
	collie_wakeup_check();
#endif


	//DPRINTK("*** made it back from resume\n");

	/* restore registers */
        GPDR = sys_ctx.gpdr;
        GRER = sys_ctx.grer;
        GFER = sys_ctx.gfer;
        GAFR = sys_ctx.gafr;

	/* XXX Should be in serial driver */
#if defined(CONFIG_SA1100_ASSABET)
	if (!machine_has_neponset()) {
            Ser1SDCR0 = 1;
            Ser1UTSR0 = 0xFF;
            Ser1UTCR3 = 0;
            Ser1UTCR0 = sys_ctx.serutcr0;
            Ser1UTCR1 = sys_ctx.serutcr1;
            Ser1UTCR2 = sys_ctx.serutcr2;
            Ser1UTCR3 = sys_ctx.serutcr3;
	} else {
            Ser3UTCR3 = 0;
            Ser3UTSR0 = 0xFF;
            Ser3UTCR0 = sys_ctx.serutcr0;
            Ser3UTCR1 = sys_ctx.serutcr1;
            Ser3UTCR2 = sys_ctx.serutcr2;
            Ser3UTCR3 = sys_ctx.serutcr3;
	}
        DPRINTK("restore serial port 1 value\n");
#elif defined(CONFIG_SA1100_COLLIE)
#if defined(CONFIG_COLLIE_TS)
        Ser1SDCR0 = 1;
        Ser1UTSR0 = 0xFF;
        Ser1UTCR3 = 0;
        Ser1UTCR0 = sys_ctx.serutcr0;
        Ser1UTCR1 = sys_ctx.serutcr1;
        Ser1UTCR2 = sys_ctx.serutcr2;
        Ser1UTCR3 = sys_ctx.serutcr3;
#else
        Ser3UTCR3 = 0;
        Ser3UTSR0 = 0xFF;
        Ser3UTCR0 = sys_ctx.serutcr0;
        Ser3UTCR1 = sys_ctx.serutcr1;
        Ser3UTCR2 = sys_ctx.serutcr2;
        Ser3UTCR3 = sys_ctx.serutcr3;
#endif
        //DPRINTK("restore serial port 1 value\n");
	DPRINTLOG();
#elif defined (CONFIG_SA1100_FREEBIRD)
        Ser1UTCR0 = sys_ctx.serutcr0;
        Ser1UTCR1 = sys_ctx.serutcr1;
        Ser1UTCR2 = sys_ctx.serutcr2;
        Ser1UTCR3 = sys_ctx.serutcr3;
              DPRINTK("restore serial port 1 value\n");
#else
        Ser3UTCR0 = sys_ctx.serutcr0;
        Ser3UTCR1 = sys_ctx.serutcr1;
        Ser3UTCR2 = sys_ctx.serutcr2;
        Ser3UTCR3 = sys_ctx.serutcr3;
#endif

        PPDR = sys_ctx.ppdr;
        PPSR = sys_ctx.ppsr;
        PPAR = sys_ctx.ppar;
        PSDR = sys_ctx.psdr;

        MECR = sys_ctx.mecr;


#ifndef CONFIG_SA1100_COLLIE
        OSMR0 = sys_ctx.osmr0;
        OSMR1 = sys_ctx.osmr1;
        OSMR2 = sys_ctx.osmr2;
        OSMR3 = sys_ctx.osmr3;
        OSCR = sys_ctx.oscr;
        OWER = sys_ctx.ower;
        OIER = sys_ctx.oier;
#endif

        ICLR = sys_ctx.iclr;
        ICCR = sys_ctx.iccr;
        ICMR = sys_ctx.icmr;


#if defined(CONFIG_SA1100_BITSY)
	if (machine_is_bitsy()) {
		if (ICIP & IC_GPIO0) { 
			/* if wakeup source is power button, clear interrupt 
				 flag to prevent sleeping again */
			GEDR = GPIO_GPIO0; 
        		ICIP = GPIO_GPIO0; 
		}
	}
#endif
#if defined(CONFIG_SA1100_ASSABET)
       if (machine_is_assabet()) {
               if (ICIP & IC_GPIO0) {
                       GEDR = GPIO_GPIO0;
                       ICIP = GPIO_GPIO0;
               }
       }
#endif
#if defined(CONFIG_SA1100_COLLIE)
	if (machine_is_collie()) {
		if (ICIP & IC_GPIO0) { 
			GEDR = GPIO_GPIO0; 
        		ICIP = GPIO_GPIO0; 
		}
	}
#endif
#if defined(CONFIG_SA1100_FREEBIRD)
       if (machine_is_freebird()) {
               if (ICIP & IC_GPIO0) {
                       GEDR = GPIO_GPIO0;
                       ICIP = GPIO_GPIO0;
               }
       }
#endif

#ifndef CONFIG_SA1100_COLLIE
	xtime.tv_sec = RCNR;
#endif

#if defined(CONFIG_SA1100_COLLIE)
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

	/* TC35143F resume */
	ucb1200_resume();

        /* 35143F io port initialize */
	ucb1200_set_io_direction(TC35143_GPIO_VERSION0,TC35143_IODIR_INPUT );
	ucb1200_set_io_direction(TC35143_GPIO_TBL_CHK, TC35143_IODIR_OUTPUT);
#if defined(CONFIG_COLLIE_TS) || defined(CONFIG_COLLIE_TR0) || \
    defined(CONFIG_COLLIE_TR1) || defined(CONFIG_COLLIE_DEV)
	ucb1200_set_io_direction(TC35143_GPIO_VPEN_ON, TC35143_IODIR_OUTPUT);
#else
	ucb1200_set_io_direction(TC35143_GPIO_VPEN_ON, TC35143_IODIR_INPUT);
#endif
	ucb1200_set_io_direction(TC35143_GPIO_IR_ON,   TC35143_IODIR_OUTPUT);
	ucb1200_set_io_direction(TC35143_GPIO_AMP_ON,  TC35143_IODIR_OUTPUT);
	ucb1200_set_io_direction(TC35143_GPIO_VERSION1,TC35143_IODIR_INPUT );
	ucb1200_set_io_direction(TC35143_GPIO_CHRG_ON, TC35143_IODIR_OUTPUT);
	ucb1200_set_io_direction(TC35143_GPIO_MBAT_ON, TC35143_IODIR_OUTPUT);
	ucb1200_set_io_direction(TC35143_GPIO_BBAT_ON, TC35143_IODIR_OUTPUT);
	ucb1200_set_io_direction(TC35143_GPIO_TMP_ON,  TC35143_IODIR_OUTPUT);

	/* Scoop resume */
	SCP_REG_GPWR = sys_ctx.scp_gpwr;

	/* ----- hardware resume ----- */
	if ( !collie_wakeup_hook() ) {
	        printk("return to suspend ....\n");
		goto DO_SUSPEND;
	}

	if ( ( (GPLR & GPIO_MAIN_BAT_LOW) == 0 ) || ( chkFatalBatt() == 0 ) ) {
	        printk("return to suspend (fatal) ....\n");
		goto DO_SUSPEND;
	}

#endif




#if defined(CONFIG_SA1100_COLLIE)

	// restore timer registers
	xtime.tv_sec = RCNR;

        OSMR0 = sys_ctx.osmr0;
        OSMR1 = sys_ctx.osmr1;
        OSMR2 = sys_ctx.osmr2;
	OSMR3 = sys_ctx.osmr3;
        OSCR = sys_ctx.oscr;
	//OWER = sys_ctx.ower;
        OIER = sys_ctx.oier;

	OSSR = ( OSSR_M0 | OSSR_M1 | OSSR_M2 | OSSR_M3 );

	restore_flags(flags);
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

	retval = sa1110_suspend();

	retval = pm_send_all(PM_RESUME, (void *)0);
	if (retval)
		return retval;

	return 0;
}

#if defined(CONFIG_SA1100_COLLIE)
EXPORT_SYMBOL(apm_wakeup_src_mask);
EXPORT_SYMBOL(apm_wakeup_factor);
EXPORT_SYMBOL(collie_battery_charge_hook);
EXPORT_SYMBOL(collie_wakeup_key_scan_hook);
EXPORT_SYMBOL(collie_wakeup_remote_control_hook);
#endif
EXPORT_SYMBOL(sa1110_suspend);
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

#if defined(CONFIG_SA1100_COLLIE)


void collie_restart(void)
{
  RCSR = 0x0f;
  RSRR = RSRR_SWR;

#if 0
  while(1) {
    if ( ( GPLR & GPIO_MAIN_BAT_LOW) == 0 ) {
      printk("return to suspend 2 ....\n");
#ifdef CONFIG_XIP_ROM
      PSPR = (unsigned long)collie_restart & 0x00ffffff;
#else
      PSPR = virt_to_phys(collie_restart);
#endif
      cpu_sa1100_do_suspend();
    } else {
      RCSR = 0x0f;
      RSRR = RSRR_SWR;
    }
  }
#endif

}

void collie_emerge_off(int irq, void *dev_id, struct pt_regs *regs)
{

  unsigned long   flags;


  /* noise ? */
  mdelay(10);
  if ( 0x1234ABCD != regs && GPLR & GPIO_MAIN_BAT_LOW ) {
    printk("cancel emergency off\n");
    return;
  }

  colliefb_disable_lcd_controller_emergency();
#if 1
  /* DMA stop */
  ClrDCSR1 = ( DCSR_RUN | DCSR_IE );
  ClrDCSR2 = ( DCSR_RUN | DCSR_IE );
  ClrDCSR3 = ( DCSR_RUN | DCSR_IE );
  ClrDCSR4 = ( DCSR_RUN | DCSR_IE );
  ClrDCSR5 = ( DCSR_RUN | DCSR_IE );



  LCM_GPO = 0x00;
  LCM_SPICT = 0x40;
  LCM_GPE=0;
  LCM_ASD = 0x0000;
  LCM_PAIF = 0x00;	/* AUDIO */
  LCM_DAC = 0x00;	/* DAC */
  LCM_TC = 0x0000;	/* CPS */
  LCM_TADC = 0x00;	/* 18MHz clock off */
  LCM_ACC = 0x00;	/* 22MHz/24MHz clock off*/
  LCM_ALS = 0x00;	/* FL */
  LCM_SPIMD = 0x3c14;   /* SPI clock off */
  /* Scoop suspend */
  SCP_REG_GPWR &= SCP_CHARGE_ON;              // not charge off


  {
#ifdef CONFIG_COLLIE_TS
#else	/* ! CONFIG_COLLIE_TS */
#define	SCP_INIT_DATA(adr,dat)	(((adr)<<16)|(dat))
#define	SCP_INIT_DATA_END	((unsigned long)-1)
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
#endif
  }

  collie_emergency_off = 1;

  sa1110_suspend();
  collie_restart();

#else
  /* set up pointer to sleep parameters */
  sleep_param = kmalloc (SLEEP_PARAM_SIZE*sizeof(long), GFP_ATOMIC);
  sleep_param_p = virt_to_phys(sleep_param);

  save_flags_cli(flags);

  /* DMA stop */
  ClrDCSR1 = ( DCSR_RUN | DCSR_IE );
  ClrDCSR2 = ( DCSR_RUN | DCSR_IE );
  ClrDCSR3 = ( DCSR_RUN | DCSR_IE );
  ClrDCSR4 = ( DCSR_RUN | DCSR_IE );
  ClrDCSR5 = ( DCSR_RUN | DCSR_IE );


  /* ----- hardware suspend ----- */
  /* LOCOMO suspend */
  LCM_GPO = 0x00;
  LCM_SPICT = 0x40;
  LCM_GPE=0;
  LCM_ASD = 0x0000;
  LCM_PAIF = 0x00;	/* AUDIO */
  LCM_DAC = 0x00;	/* DAC */
  LCM_TC = 0x0000;	/* CPS */
  LCM_C32K = 0x00;	/* CLK32 off */
  LCM_TADC = 0x00;	/* 18MHz clock off */
  LCM_ACC = 0x00;	/* 22MHz/24MHz clock off*/
  LCM_ALS = 0x00;	/* FL */
  //  LCM_LPT0 =LCM_LPT_TOFL; /* LED OFF */   // not charge off
  LCM_LPT1 =LCM_LPT_TOFL; /* LED OFF */

  /* Scoop suspend */
  SCP_REG_GPWR &= SCP_CHARGE_ON;              // not charge off

  /* ----- hardware suspend ----- */
  PWER = ( GPIO_AC_IN | GPIO_ON_KEY );
  GRER = ( GPIO_AC_IN | GPIO_ON_KEY );
  GFER = ( GPIO_ON_KEY );
  GEDR = ( GPIO_AC_IN | GPIO_ON_KEY );

  /* Clear reset status */
  RCSR = RCSR_HWR | RCSR_SWR | RCSR_WDR | RCSR_SMR;

  /* Stop 3.6MHz and drive HIGH to PCMCIA and CS */
  PCFR = PCFR_OPDE;

  /* GPIO Sleep Register */
  PGSR = 0x00040000;

  /* PSDR setting */
  PSDR = 0x000aa000;


  /* set resume return address */
#ifdef CONFIG_XIP_ROM
  PSPR = (unsigned long)collie_restart & 0x00ffffff;
#else
  PSPR = virt_to_phys(collie_restart);
#endif
  cpu_sa1100_do_suspend();

#endif

}


#endif

/*
 * Initialize power interface
 */
static int __init pm_init(void)
{
	register_sysctl_table(pm_dir_table, 1);


#ifndef CONFIG_COLLIE_TR0
#if defined(CONFIG_SA1100_COLLIE)
	/* Set transition detect */
	set_GPIO_IRQ_edge( GPIO_MAIN_BAT_LOW  , GPIO_FALLING_EDGE );


	/* this registration can be done in init/main.c. */
	if(1){
	  int err;
	  err = request_irq(IRQ_GPIO_MAIN_BAT_LOW,collie_emerge_off , SA_INTERRUPT, "batok", NULL);
	  if( err ){
	    printk("batok install error %d\n",err);
	  }else{
	    enable_irq(IRQ_GPIO_MAIN_BAT_LOW);
	    printk("batok installed\n");
	  }
	}

#endif
#endif

	return 0;
}

__initcall(pm_init);

