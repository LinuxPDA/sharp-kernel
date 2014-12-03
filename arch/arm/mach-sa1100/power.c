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
 */

/*
 *  Debug macros 
 */
#define DEBUG 1
#ifdef DEBUG
#  define DPRINTK(fmt, args...)	printk("%s: " fmt, __FUNCTION__ , ## args)
#else
#  define DPRINTK(fmt, args...)
#endif


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
#include <asm/arch/power.h>

static struct {
        u32 osmr0, osmr1, osmr2, osmr3, oscr;
        u32 ower, oier;

        u32 gpdr, grer, gfer, gafr;
        u32 icmr, iclr, iccr;
        u32 ppdr, ppsr, ppar, psdr;
		/* FIXME this part should in the serial port driver ,Control register*/
        u32 serutcr0, serutcr1, serutcr2, serutcr3;

} sys_ctx;

extern unsigned long *sleep_param;	/* virtual address */
extern unsigned long  sleep_param_p;	/* physical address */
extern void cpu_sa1100_resume(void);	/* function the bootloader will call back */

extern int cpu_sa1100_do_suspend(void);

int sa1110_suspend(void)
{

	/* set up pointer to sleep parameters */
	sleep_param = kmalloc (SLEEP_PARAM_SIZE*sizeof(long), GFP_ATOMIC);
	if (!sleep_param)
		return -ENOMEM;
	sleep_param_p = virt_to_phys(sleep_param);
	
	DPRINTK("*** sa1100_suspend()\n");
	DPRINTK("*** sleep_param_p = 0x%08lx\n", sleep_param_p);
	DPRINTK("*** resume_return() = 0x%08lx\n", virt_to_phys(cpu_sa1100_resume));

	/* disable interrupts */
	cli();
	RCNR = xtime.tv_sec;

	/* save vital registers */
        sys_ctx.osmr0 = OSMR0;
        sys_ctx.osmr1 = OSMR1;
        sys_ctx.osmr2 = OSMR2;
        sys_ctx.osmr3 = OSMR3;
        sys_ctx.oscr  = OSCR;
        sys_ctx.ower = OWER;
        sys_ctx.oier = OIER;

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

	/* XXX Should be in serial driver */
	/* Save serial port context for quick restore */
#if defined(CONFIG_SA1100_ASSABET) || defined(CONFIG_SA1100_FREEBIRD)
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
		PWER = 0x1;
		GRER = 0x1;
		GFER = 0x0;
		GEDR = 0x1;

		RCSR = RCSR_HWR | RCSR_SWR | RCSR_WDR | RCSR_SMR;
		/* Clear reset status */
		PCFR = PCFR_OPDE | PCFR_FP | PCFR_FS;
		PGSR = 0x00000000;
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
	PSPR = virt_to_phys(cpu_sa1100_resume);

	cpu_sa1100_do_suspend();

	DPRINTK("*** made it back from resume\n");

	/* restore registers */
        GPDR = sys_ctx.gpdr;
        GRER = sys_ctx.grer;
        GFER = sys_ctx.gfer;
        GAFR = sys_ctx.gafr;

	/* XXX Should be in serial driver */
#if defined(CONFIG_SA1100_ASSABET) || defined (CONFIG_SA1100_FREEBIRD)
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

        OSMR0 = sys_ctx.osmr0;
        OSMR1 = sys_ctx.osmr1;
        OSMR2 = sys_ctx.osmr2;
        OSMR3 = sys_ctx.osmr3;
        OSCR = sys_ctx.oscr;
        OWER = sys_ctx.ower;
        OIER = sys_ctx.oier;

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
#if defined(CONFIG_SA1100_FREEBIRD)
       if (machine_is_freebird()) {
               if (ICIP & IC_GPIO0) {
                       GEDR = GPIO_GPIO0;
               ICIP = GPIO_GPIO0;
               }
       }
#endif

	xtime.tv_sec = RCNR;
	sti();
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

