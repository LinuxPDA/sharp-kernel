/*
 *  linux/kernel/cpufreq.c
 *
 *  Copyright (C) 2001 Russell King
 *
 *  $Id: cpufreq.c,v 1.3 2001/06/19 21:05:18 rmk Exp $
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * CPU speed changing core functionality.  We provide the following
 * services to the system:
 *  - notifier lists to inform other code of the freq change both
 *    before and after the freq change.
 *  - the ability to change the freq speed
 *
 * This core is designed with the intention that it should be useful for
 * both Intel StrongARM and Intel SpeedStep processors, as well as any
 * other processor that supports variable clock rates.
 */
#include <linux/config.h>
#include <linux/types.h>
#include <linux/notifier.h>
#include <linux/cpufreq.h>
#include <linux/kernel.h>
#include <linux/delay.h>
#include <linux/init.h>
#include <linux/smp.h>

#include <asm/semaphore.h>
#include <asm/system.h>
#include <linux/interrupt.h>	/* requires system.h */

/*
 * This list is for kernel code that needs to handle
 * changes to devices when the CPU clock speed changes.
 */
static struct notifier_block *cpufreq_notifier_list;
static struct semaphore cpufreq_sem;
static unsigned long cpufreq_ref_loops;
static unsigned int cpufreq_ref_freq;
static int cpufreq_initialised;

#ifndef CONFIG_SMP
static unsigned int __cpufreq_max;
static unsigned int __cpufreq_cur;
#endif


static unsigned long scale(unsigned long ref, u_int div, u_int mult)
{
	unsigned long new_jiffy_l, new_jiffy_h;

	/*
	 * Recalculate loops_per_jiffy.  We do it this way to
	 * avoid math overflow on 32-bit machines.  Maybe we
	 * should make this architecture dependent?  If you have
	 * a better way of doing this, please replace!
	 *
	 *    new = old * mult / div
	 */
	new_jiffy_h = ref / div;
	new_jiffy_l = (ref % div) / 100;
	new_jiffy_h *= mult;
	new_jiffy_l = new_jiffy_l * mult / div;

	return new_jiffy_h + new_jiffy_l * 100;
}

/*
 * cpufreq_max command line parameter.  Use:
 *  cpufreq_max=221000
 * to set the CPU maximum frequency to 221MHz.
 */
static int __init cpufreq_max_setup(char *str)
{
	int i;

	for (i = 0; i < smp_num_cpus; i++)
		cpufreq_max(i) = simple_strtoul(str, NULL, 0);

	return 1;
}

__setup("cpufreq_max=", cpufreq_max_setup);

/**
 * cpufreq_register_notifier - Register function to be called when clock speed changes
 * @nb: Info about notifier function to be called
 *
 * Registers a function with the list of functions to be called when
 * the CPU clock changes speed.
 *
 * This function has the same return conditions as notifier_chain_register.
 */
int cpufreq_register_notifier(struct notifier_block *nb)
{
	int ret;

	down(&cpufreq_sem);
	ret = notifier_chain_register(&cpufreq_notifier_list, nb);
	up(&cpufreq_sem);

	return ret;
}

/**
 * cpufreq_unregister_notifier - Unregister previously registered clock speed change notifier
 * @nb: notifier block to be unregistered
 *
 * Unregisters a previously registered clock change function.
 *
 * This function has the same return conditions as notifier_chain_unregister.
 */
int cpufreq_unregister_notifier(struct notifier_block *nb)
{
	int ret;

	down(&cpufreq_sem);
	ret = notifier_chain_unregister(&cpufreq_notifier_list, nb);
	up(&cpufreq_sem);

	return ret;
}

/*
 * This notifier alters the system "loops_per_jiffy" for the clock
 * speed change.  We ignore CPUFREQ_MINMAX here.
 */
static int
jiffy_notifier(struct notifier_block *nb, unsigned long val, void *data)
{
	struct cpufreq_info *ci = data;

	if ((val == CPUFREQ_PRECHANGE  && ci->old_freq > ci->new_freq) ||
	    (val == CPUFREQ_POSTCHANGE && ci->old_freq < ci->new_freq))
	    	loops_per_jiffy = scale(cpufreq_ref_loops, cpufreq_ref_freq, ci->new_freq);

	return 0;
}

static struct notifier_block jiffy_block = {
	notifier_call:	jiffy_notifier,
};

/**
 * cpu_setfreq - change the CPU clock frequency.
 * @freq: frequency (in steps of 1kHz) at which we should run.
 *
 * You need to call this on the CPU that you want the speed changed.
 */
int cpufreq_set(unsigned int freq)
{
	struct cpufreq_info clkinfo;
	struct cpufreq_minmax minmax = { min: 0, max: -1 };
	int cpu = smp_processor_id();

	if (!cpufreq_initialised)
		panic("cpufreq_set() called before initialisation!");
	if (in_interrupt())
		panic("cpufreq_set() called from interrupt context!");

	/*
	 * Don't allow the CPU to be clocked over the limit.
	 */
	if (freq > cpufreq_max(cpu))
		freq = cpufreq_max(cpu);

	/*
	 * Find out what the registered devices will currently tollerate,
	 * and limit the requested clock rate to these values.
	 */
	down(&cpufreq_sem);
	notifier_call_chain(&cpufreq_notifier_list, CPUFREQ_MINMAX, &minmax);
	if (freq < minmax.min)
		freq = minmax.min;
	if (freq > minmax.max)
		freq = minmax.max;

	freq = cpufreq_validatespeed(freq);

	if (cpufreq_current(cpu) != freq) {
		clkinfo.old_freq = cpufreq_current(cpu);
		clkinfo.new_freq = freq;

		notifier_call_chain(&cpufreq_notifier_list, CPUFREQ_PRECHANGE,
				    &clkinfo);

		cpufreq_setspeed(freq);
		cpufreq_current(cpu) = freq;

		notifier_call_chain(&cpufreq_notifier_list, CPUFREQ_POSTCHANGE,
				    &clkinfo);
	}
	up(&cpufreq_sem);

	return 0;
}

/**
 * cpufreq_setmax - set the CPU to maximum frequency
 *
 * Call this to switch the CPU to maximum frequency.
 */
int cpufreq_setmax(void)
{
	return cpufreq_set(cpufreq_max(smp_processor_id()));
}

/**
 * cpufreq_get - return the current CPU clock frequency in kHz
 */
unsigned int cpufreq_get(int cpu)
{
	if (!cpufreq_initialised)
		panic("cpufreq_get() called before initialisation!");
	return cpufreq_current(cpu);
}

#ifdef CONFIG_PROC_FS
/*
 * This is a temporary debugging hack.  It is intended that something else
 * will control the CPU clock via cpufreq_set().
 */
#include <linux/fs.h>		/**/
#include <linux/proc_fs.h>	/**/

#include <asm/uaccess.h>	/**/

static int
cpufreq_read(char *page, char **start, off_t off, int count, int *eof,
	       void *data)
{
	int len;

	len = sprintf(page, "%d\n", cpufreq_get(0));

	len -= off;
	*eof = (len <= count) ? 1 : 0;
	*start = page + off;
	return len;
}

static int
cpufreq_write(struct file *file, const char *buffer, unsigned long count,
		void *data)
{
	char tmpbuf[16];
	unsigned long freq;

	if (file->f_pos != 0)
		return 0;

	if (count > 16)
		count = 16;

	if (copy_from_user(tmpbuf, buffer, count))
		return -EFAULT;
	tmpbuf[15] = '\0';

	freq = simple_strtoul(tmpbuf, NULL, 0);
	cpufreq_set(freq);

	file->f_pos += count;

	return count;
}

static void cpufreq_proc_init(void)
{
	struct proc_dir_entry *ent, *dir;

	dir = proc_mkdir("cpu", NULL);
	if (dir)
		dir = proc_mkdir("clock", dir);
	if (dir) {
		ent = create_proc_entry("current", S_IWUSR | S_IRUGO, dir);
		if (ent) {
			ent->read_proc = cpufreq_read;
			ent->write_proc = cpufreq_write;
		}
	}
}
#else
#define cpufreq_proc_init()
#endif

void __init cpufreq_init(unsigned int freq)
{
	sema_init(&cpufreq_sem, 1);

	/*
	 * If the user doesn't tell us their maximum frequency,
	 * then we default to the current CPU frequency.
	 */
	if (cpufreq_max(0) == 0)
		cpufreq_max(0) = freq;

	printk(KERN_INFO "CPU clock: %d.%03d MHz (max %d.%03d MHz)\n",
		freq / 1000, freq % 1000,
		cpufreq_max(0) / 1000, cpufreq_max(0) % 1000);

	cpufreq_ref_loops = loops_per_jiffy;
	cpufreq_ref_freq = freq;
	cpufreq_current(smp_processor_id()) = freq;

	cpufreq_register_notifier(&jiffy_block);
	cpufreq_initialised = 1;
	up(&cpufreq_sem);

	cpufreq_proc_init();
}
