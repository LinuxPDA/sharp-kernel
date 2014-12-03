/*
 * linux/arch/arm/mach-dbmx1/pm.c
 *
 * Power Management Routines
 *
 * (C) Copyright 2003 Lineo Japan, Inc.
 *
 * May be copied or modified under the terms of the GNU General Public
 * License.  See linux/COPYING for more information.
 *
 * Based on:
 *   arch/arm/mach-sa1100/pm.c
 *
 * Copyright (c) 2001 Cliff Brake <cbrake@accelent.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License.
 *
 * History:
 *
 * 2001-02-06:	Cliff Brake         Initial code
 *
 * 2001-02-25:	Sukjae Cho <sjcho@east.isi.edu> &
 * 		Chester Kuo <chester@linux.org.tw>
 * 			Save more value for the resume function! Support
 * 			Bitsy/Assabet/Freebird board
 *
 * 2001-08-29:	Nicolas Pitre <nico@cam.org>
 * 			Cleaned up, pushed platform dependent stuff
 * 			in the platform specific files.
 *
 * 2002-05-27:	Nicolas Pitre	Killed sleep.h and the kmalloced save array.
 * 				Storage is local on the stack now.
 */
#include <linux/config.h>
#include <linux/init.h>
#include <linux/pm.h>
#include <linux/sched.h>
#include <linux/interrupt.h>
#include <linux/sysctl.h>
#include <linux/errno.h>

#include <asm/hardware.h>
#include <asm/memory.h>
#include <asm/system.h>
#include <asm/leds.h>


/*
 * Debug macros
 */
#undef DEBUG

extern void dbmx1_cpu_suspend(void);
extern void dbmx1_cpu_standby(void);

static int suspend_handling = 0;
static int is_suspend;
static DECLARE_WAIT_QUEUE_HEAD(wq_off);


int pm_do_suspend(void)
{
    unsigned long intenableh, intenablel;

    cli();

    suspend_handling = 1;

    leds_event(led_stop);

    /* preserve current time */
//    RCNR = xtime.tv_sec;

#ifdef CONFIG_ARCH_APLAT
   intenableh = DBMX1_AITC_INTENABLEH;
   intenablel = DBMX1_AITC_INTENABLEL;
   DBMX1_AITC_INTENABLEH = 0x00000000;
   DBMX1_AITC_INTENABLEL = 0x00000800;
#endif

    /* go zzz */
    dbmx1_cpu_suspend();

#ifdef CONFIG_ARCH_APLAT
    DBMX1_AITC_INTENABLEH = intenableh;
    DBMX1_AITC_INTENABLEL = intenablel;
#endif

#ifdef DEBUG
    printk(KERN_DEBUG "*** made it back from resume\n");
#endif

    /* restore current time */
//   xtime.tv_sec = RCNR;

    leds_event(led_start);

    sti();

    /*
     * Restore the CPU frequency settings.
     */
#ifdef CONFIG_CPU_FREQ
    cpufreq_restore();
#endif

    return 0;
}

int pm_do_standby(void)
{
    unsigned long intenableh, intenablel;

    cli();

    suspend_handling = 1;

    leds_event(led_stop);

    /* preserve current time */
//    RCNR = xtime.tv_sec;

#ifdef CONFIG_ARCH_APLAT
   intenableh = DBMX1_AITC_INTENABLEH;
   intenablel = DBMX1_AITC_INTENABLEL;
   DBMX1_AITC_INTENABLEH = 0x00000000;
   DBMX1_AITC_INTENABLEL = 0x00000800;
#endif

    /* go zzz */
    dbmx1_cpu_standby();

#ifdef CONFIG_ARCH_APLAT
    DBMX1_AITC_INTENABLEH = intenableh;
    DBMX1_AITC_INTENABLEL = intenablel;
#endif

#ifdef DEBUG
    printk(KERN_DEBUG "*** made it back from resume\n");
#endif

    /* restore current time */
//    xtime.tv_sec = RCNR;

    leds_event(led_start);

    sti();

    /*
     * Restore the CPU frequency settings.
     */
#ifdef CONFIG_CPU_FREQ
    cpufreq_restore();
#endif

    return 0;
}

unsigned long sleep_phys_sp(void *sp)
{
    return virt_to_phys(sp);
}

#ifdef CONFIG_SYSCTL
/*
 * ARGH!  ACPI people defined CTL_ACPI in linux/acpi.h rather than
 * linux/sysctl.h.
 *
 * This means our interface here won't survive long - it needs a new
 * interface.  Quick hack to get this working - use sysctl id 9999.
 */
#warning ACPI broke the kernel, this interface needs to be fixed up.
#define CTL_ACPI 9999
#define ACPI_S1_SLP_TYP 19

/*
 * Send us to sleep.
 */
static int sysctl_pm_do_suspend(void)
{
    int retval;

    retval = pm_send_all(PM_SUSPEND, (void *)3);

    if (retval == 0) {
	retval = pm_do_suspend();

	pm_send_all(PM_RESUME, (void *)0);
    }

    return retval;
}

static void suspend_interrupt(void)
{
    if (suspend_handling) {
	suspend_handling = 0;
	return;
    }
    is_suspend = 1;
    wake_up(&wq_off);
}

static void standby_interrupt(void)
{
    if (suspend_handling) {
	suspend_handling = 0;
	return;
    }
    is_suspend = 0;
    wake_up(&wq_off);
}

static void wakeup_interrupt(void)
{
}

static int off_thread(void *unused)
{
    int time_cnt = 0;

    strcpy(current->comm, "off_thread");

    for (;;) {
	sleep_on(&wq_off);
	if (is_suspend)
	    sysctl_pm_do_suspend();
	else
	    pm_do_standby();
    }
    return 0;
}

static struct ctl_table pm_table[] =
{
    {ACPI_S1_SLP_TYP, "suspend", NULL, 0, 0600, NULL, (proc_handler *)&sysctl_pm_do_suspend},
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

#ifdef CONFIG_ARCH_APLAT
    if (request_irq(IRQ_PSW_DN, suspend_interrupt, SA_INTERRUPT,
		    "psw_dn", NULL)) {
	return -EBUSY;
    }
    if (request_irq(IRQ_ISW_DN, standby_interrupt, SA_INTERRUPT,
		    "isw_dn", NULL)) {
	free_irq(IRQ_PSW_DN, NULL);
	return -EBUSY;
    }
    if (request_irq(IRQ_WAKE, wakeup_interrupt, SA_INTERRUPT,
		    "wake", NULL)) {
	free_irq(IRQ_ISW_DN, NULL);
	free_irq(IRQ_PSW_DN, NULL);
	return -EBUSY;
    }
#endif
    kernel_thread(off_thread,  NULL,
		  CLONE_FS | CLONE_FILES | CLONE_SIGHAND | SIGCHLD);

    return 0;
}

__initcall(pm_init);

#endif
