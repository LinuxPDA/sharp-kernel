/*
 * MQ9000(KATANA) Power Management Routines
 *
 * (C) Copyright 2003 Lineo uSolutions, Inc.
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

extern void print_debug(char* argv);

#include <linux/config.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/pm.h>
#include <linux/slab.h>
#include <linux/sysctl.h>
#include <linux/acpi.h>
#include <linux/delay.h>

#include <asm/hardware.h>
#include <asm/memory.h>
#include <asm/system.h>
#include <asm/irq.h>
#include <asm/hardirq.h>
#include <asm/arch/irqs.h>
#include <asm/arch/power.h>

static struct {
	u32 pm00, pm01, pm02, pm03, pm07, pm08;
	u32 sb00, sb01, sb02, sb03, sb04, sb05, sb07, sb08, sb09, sb0A, sb0B;
	u32 sb0C, sb0D, sb0E, sb0F, sb10, sb11, sb14, sb15, sb16, sb17, sb18;
	u32 gp00, gp01, gp02, gp03, gp04, gp05, gp06, gp07, gp0B, gp0C, gp0D;
	u32 gp0E, gp10, gp11, gp12, gp14, gp15, gp16, gp17, gp18, gp1A;
	u32 in00, in01, in02;
	u32 u100, u101, u102, u103;
	u32 u200, u201, u202, u203, u204, u207, u208, u20A, u20B, u20C;
	u32 u300, u301, u302, u303, u304, u307, u308, u30A, u30B, u30C;
	u32 cc00;
	u32 mm00, mm01, mm05;
} sys_ctx;

extern unsigned long *sleep_param;	/* virtual address */
extern unsigned long  sleep_param_p;	/* physical address */
extern void cpu_sa1100_resume(void);	/* function the bootloader will call back */
extern int cpu_katana_do_suspend(void);


int katana_suspend(void)
{
	unsigned long   flags;
	volatile MQ9000REGS *pMQRegs = (MQ9000REGS *)MQ9000_REGS_VBASE;

	/* set up pointer to sleep parameters */
	sleep_param = kmalloc (SLEEP_PARAM_SIZE*sizeof(long), GFP_ATOMIC);
	if (!sleep_param)
		return -ENOMEM;
	sleep_param_p = virt_to_phys(sleep_param);
	
	if ( !in_interrupt() ) {
		__set_current_state(TASK_UNINTERRUPTIBLE);
		schedule_timeout( 1 * HZ);

		/* wait for Power On key release */
		while (!(pMQRegs->gp.gp0B.GpioData & 0x00000001));
	}

	/* disable interrupts */
	save_flags_cli(flags);

	/* save vital registers */
	sys_ctx.pm00 = pMQRegs->pm.pm00.ActiveCtrl & ~0xbc007c04;
	sys_ctx.pm01 = pMQRegs->pm.pm01.IdleCtrl & ~0xfffffc3c;
	sys_ctx.pm02 = pMQRegs->pm.pm02.PLL1 & ~0xf010088c;
	sys_ctx.pm03 = pMQRegs->pm.pm03.PLL2 & ~0xff10088c;
	sys_ctx.pm07 = pMQRegs->pm.pm07.Clock & ~0xffffff08;
	sys_ctx.pm08 = pMQRegs->pm.pm08.GEConfig & ~0xffff8f00;

	sys_ctx.sb00 = pMQRegs->sb.sb00.ExtMemEn & ~0xffffffc0;
	sys_ctx.sb01 = pMQRegs->sb.sb01.CPUAccCtrl & ~0xfff000f0;
	sys_ctx.sb02 = pMQRegs->sb.sb02.CPUAcc1Start & ~0xff000000;
	sys_ctx.sb03 = pMQRegs->sb.sb03.CPUAcc2Start & ~0xff000000;
	sys_ctx.sb04 = pMQRegs->sb.sb04.CPUReqCtrl & ~0xfffff800;
	sys_ctx.sb05 = pMQRegs->sb.sb05.FlashCtrl & ~0xc00000c0;
	sys_ctx.sb07 = pMQRegs->sb.sb07.SRAMCtrl & ~0xfc000008;
	sys_ctx.sb08 = pMQRegs->sb.sb08.ExtPerf0Ctrl & ~0xe00000c0;
	sys_ctx.sb09 = pMQRegs->sb.sb09.ExtPerf1Ctrl & ~0xe00000c0;
	sys_ctx.sb0A = pMQRegs->sb.sb0A.SDRAMCtrl1 & ~0xfe000000;
	sys_ctx.sb0B = pMQRegs->sb.sb0B.SDRAMCtrl2 & ~0xffc0c000;
	sys_ctx.sb0C = pMQRegs->sb.sb0C.SDRAMCtrl3 & ~0xffffe000;
	sys_ctx.sb0D = 0x00000001;
	sys_ctx.sb0E = pMQRegs->sb.sb0E.EmSRAMCtrl & ~0xfffffffc;
	sys_ctx.sb0F = pMQRegs->sb.sb0F.PerfDMACtrl1 & ~0xfffffe00;
	sys_ctx.sb10 = pMQRegs->sb.sb10.PerfDMACtrl2 & ~0xff000000;
	sys_ctx.sb11 = pMQRegs->sb.sb11.PerfDMACtrl3 & ~0xfff00000;
	sys_ctx.sb14 = pMQRegs->sb.sb14.NANDFlashDMA1 & ~0xfffe0000;
	sys_ctx.sb15 = pMQRegs->sb.sb15.NANDFlashDMA2 & ~0xfc000000;
	sys_ctx.sb16 = pMQRegs->sb.sb16.EBIUNANDFlashCtrl & ~0xe0000000;
	sys_ctx.sb17 = pMQRegs->sb.sb17.EBIUNANDFlashAddr & ~0xf8000000;
	sys_ctx.sb18 = pMQRegs->sb.sb18.EBIUNANDFlashCmd & ~0xfffffff8;

	sys_ctx.gp00 = pMQRegs->gp.gp00.TimerCtrl;
	sys_ctx.gp01 = pMQRegs->gp.gp01.GpioDir;
	sys_ctx.gp02 = pMQRegs->gp.gp02.GpioSelect;
	sys_ctx.gp03 = pMQRegs->gp.gp03.GpioWakeEn;
	sys_ctx.gp04 = pMQRegs->gp.gp04.GpioEnable;
	sys_ctx.gp05 = pMQRegs->gp.gp05.Timer0Cmp;
	sys_ctx.gp06 = pMQRegs->gp.gp06.Timer1Cmp;
	sys_ctx.gp07 = pMQRegs->gp.gp07.Timer2Cmp;
	sys_ctx.gp0B = pMQRegs->gp.gp0B.GpioData & ~0x00f00000;
	sys_ctx.gp0C = pMQRegs->gp.gp0C.TimerClock & ~0xfffff084;
	sys_ctx.gp0D = pMQRegs->gp.gp0D.TimerLEDInt1;
	sys_ctx.gp0E = pMQRegs->gp.gp0E.TimerLEDInt2;
	sys_ctx.gp10 = pMQRegs->gp.gp10.LEDCyleCnt & ~0xffc00000;
	sys_ctx.gp11 = pMQRegs->gp.gp11.LEDActiveTime & ~0xffff0000;
	sys_ctx.gp12 = pMQRegs->gp.gp12.LEDInActiveTime & ~0xffff0000;
	sys_ctx.gp14 = pMQRegs->gp.gp14.GpioPinDir;
	sys_ctx.gp15 = pMQRegs->gp.gp15.GpioPinEn;
	sys_ctx.gp16 = pMQRegs->gp.gp16.GpioPinInt & ~0xff000000;
	sys_ctx.gp17 = pMQRegs->gp.gp17.Gpio116_119;
	sys_ctx.gp18 = pMQRegs->gp.gp18.Gpio116_119Int & ~0xfffff000;
	sys_ctx.gp1A = pMQRegs->gp.gp1A.GpioModeEnable;

	sys_ctx.in00 = pMQRegs->in.in00.Control & ~0xfffffffc;
	sys_ctx.in01 = pMQRegs->in.in01.IRQMask & ~0xf0000000;
	sys_ctx.in02 = pMQRegs->in.in02.FIQMask & ~0xf0000000;

	sys_ctx.u100 = pMQRegs->u1.ut00.Cntr0 & ~0xffffc440;
	sys_ctx.u101 = pMQRegs->u1.ut01.Cntr1 & ~0xe0000000;
	sys_ctx.u102 = pMQRegs->u1.ut02.Cntr2 & ~0xe0000000;
	sys_ctx.u103 = pMQRegs->u1.ut03.Data & ~0xffffffbc;

	sys_ctx.u200 = pMQRegs->u2.ua00.Clock1 & ~0x10030000;
	sys_ctx.u201 = pMQRegs->u2.ua01.Clock2;
	sys_ctx.u202 = pMQRegs->u2.ua02.TxCtrl & ~0xffe04000;
	sys_ctx.u203 = pMQRegs->u2.ua03.RxCtrl & ~0x1fffc0a0;
	sys_ctx.u204 = pMQRegs->u2.ua04.IntCtrl & ~0xffffff40;
	sys_ctx.u207 = pMQRegs->u2.ua07.TxBufStart & ~0xff000000;
	sys_ctx.u208 = pMQRegs->u2.ua08.TxBufSize & ~0xffffc000;
	sys_ctx.u20A = pMQRegs->u2.ua0A.RxBufStart & ~0xff000000;
	sys_ctx.u20B = pMQRegs->u2.ua0B.RxBufSize & ~0xffffe000;
	sys_ctx.u20C = pMQRegs->u2.ua0C.Gpio & ~0x800083e0;

	sys_ctx.u300 = pMQRegs->u3.ua00.Clock1 & ~0x10030000;
	sys_ctx.u301 = pMQRegs->u3.ua01.Clock2;
	sys_ctx.u302 = pMQRegs->u3.ua02.TxCtrl & ~0xffe04000;
	sys_ctx.u303 = pMQRegs->u3.ua03.RxCtrl & ~0x1fffc0a0;
	sys_ctx.u304 = pMQRegs->u3.ua04.IntCtrl & ~0xffffff40;
	sys_ctx.u307 = pMQRegs->u3.ua07.TxBufStart & ~0xff000000;
	sys_ctx.u308 = pMQRegs->u3.ua08.TxBufSize & ~0xffffc000;
	sys_ctx.u30A = pMQRegs->u3.ua0A.RxBufStart & ~0xff000000;
	sys_ctx.u30B = pMQRegs->u3.ua0B.RxBufSize & ~0xffffe000;
	sys_ctx.u30C = pMQRegs->u3.ua0C.Gpio & ~0x800083e0;

	sys_ctx.cc00 = pMQRegs->cc.cc00.Control & ~0xfffff001;

	sys_ctx.mm00 = pMQRegs->mm.mm00.Control1 & ~0xfffffffc;
	sys_ctx.mm01 = pMQRegs->mm.mm01.Control2 & ~0xfc008180;
	sys_ctx.mm05 = pMQRegs->mm.mm05.ClockCtrl & ~0xfff1f80c;

print_debug("do_suspend\n");
	cpu_katana_do_suspend();

	//DPRINTK("*** made it back from resume\n");

	/* restore registers */
	pMQRegs->u1.ut00.Cntr0 = sys_ctx.u100;
	pMQRegs->u1.ut01.Cntr1 = sys_ctx.u101;
	pMQRegs->u1.ut02.Cntr2 = sys_ctx.u102;
	pMQRegs->u1.ut03.Data = sys_ctx.u103;

	restore_flags(flags);
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

	retval = katana_suspend();

	retval = pm_send_all(PM_RESUME, (void *)0);
	if (retval)
		return retval;

	return 0;
}

EXPORT_SYMBOL(katana_suspend);
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

