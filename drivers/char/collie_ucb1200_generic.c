/*
 *  linux/drivers/char/collie_ucb1200_generic.c
 *
 * (C) Copyright 2001 Lineo Japan, Inc.
 *
 * May be copied or modified under the terms of the GNU General Public
 * License.  See linux/COPYING for more information.
 *
 * Based on linux/drivers/char/ucb1200_generic.c
 *
 *  Copyright (C) 2000 G.Mate, Inc.
 *
 *  Author: Tred Lim
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * 18 December 2000	: created.
 * 14 November 2001     : modify for SL-5000D SHARP Corporation
 */
#include <linux/config.h>
#include <linux/module.h>
#include <linux/sched.h>
#include <linux/malloc.h>
#include <linux/interrupt.h>
#include <linux/random.h>
#include <linux/proc_fs.h>
#include <linux/delay.h>
#include <linux/init.h>

#include <asm/hardware.h>
#include <asm/ucb1200.h>

#ifdef CONFIG_SA1100_COLLIE
#include <asm/arch/tc35143.h>
#endif


#define DPRINTK(s...)	printk(s)


/*
 * UCB1200 register 11: ADC data register
 */
#define ADC_DATA_SHIFT_VAL (5)
#define GET_ADC_DATA(x) (((x) >> ADC_DATA_SHIFT_VAL) & 0x3ff)
#define ADC_DAT_VAL (1 << 15)
#define ADC_ENA_TYPE (ADC_SYNC_ENA | ADC_ENA)


static struct ucb1200_arch_info	ucb1200_arch = { NO_IRQ, };
static struct ucb1200_low_level *ucb1200_low_level/* = NULL */;
extern struct ucb1200_low_level arch_ucb1200_ops;

static spinlock_t ucb1200_lock = SPIN_LOCK_UNLOCKED;
#define ucb1200_irq_lock	ucb1200_lock

static unsigned int ucb1200_irqs[NR_CPUS][UCB1200_NR_IRQS];

static struct irqdesc irq_desc[UCB1200_NR_IRQS];

static u16 ucb1200_irq_enabled = 0;
static u16 ucb1200_irq_rising_edge = 0;
static u16 ucb1200_irq_falling_edge = 0;



#define ucb1200_arch_init(arch)	ucb1200_low_level->arch_init(arch)
#define ucb1200_arch_exit	ucb1200_low_level->arch_exit

static int sib_count = 0;

static inline void sib_enable(void)
{
	if (sib_count++ == 0)
		ucb1200_low_level->sib_enable();
}

static inline void sib_disable(void)
{
	if (--sib_count == 0)
		ucb1200_low_level->sib_disable();
}

#define sib_read_codec(addr) \
	ucb1200_low_level->sib_read_codec(addr)

#define sib_write_codec(addr, data) \
	ucb1200_low_level->sib_write_codec(addr, data)

#define ucb1200_set_adc_sync \
	ucb1200_low_level->set_adc_sync

#define ucb1200_lock_enable(flags) \
	spin_lock_irqsave(&ucb1200_lock, flags); \
	sib_enable();

#define ucb1200_unlock_disable(flags) \
	sib_disable(); \
	spin_unlock_irqrestore(&ucb1200_lock, flags);

static void ucb1200_mask_and_ack_irq(unsigned int irq)
{
	u16 mask = (1 << irq);

	sib_enable();

	ucb1200_irq_enabled &= ~mask;

	sib_write_codec(UCB1200_REG_RISE_INT_ENABLE,
			ucb1200_irq_rising_edge & ucb1200_irq_enabled);
	sib_write_codec(UCB1200_REG_FALL_INT_ENABLE,
			ucb1200_irq_falling_edge & ucb1200_irq_enabled);

	sib_write_codec(UCB1200_REG_INT_STATUS, 0);
	sib_write_codec(UCB1200_REG_INT_STATUS, mask);

	sib_disable();
}

static void ucb1200_mask_irq(unsigned int irq)
{
	u16 mask = (1 << irq);

	sib_enable();

	ucb1200_irq_enabled &= ~mask;

	sib_write_codec(UCB1200_REG_RISE_INT_ENABLE,
			ucb1200_irq_rising_edge & ucb1200_irq_enabled);
	sib_write_codec(UCB1200_REG_FALL_INT_ENABLE,
			ucb1200_irq_falling_edge & ucb1200_irq_enabled);

	sib_disable();
}

static void ucb1200_unmask_irq(unsigned int irq)
{
	u16 mask = (1 << irq);

	sib_enable();

	ucb1200_irq_enabled |= mask;

	sib_write_codec(UCB1200_REG_RISE_INT_ENABLE,
			ucb1200_irq_rising_edge & ucb1200_irq_enabled);
	sib_write_codec(UCB1200_REG_FALL_INT_ENABLE,
			ucb1200_irq_falling_edge & ucb1200_irq_enabled);

	sib_disable();
}

static void do_UCB1200_IRQ(int irq, struct pt_regs *regs)
{
	struct irqdesc *desc;
	struct irqaction *action;
	int cpu;

	desc = irq_desc + irq;

	spin_lock(&ucb1200_irq_lock);
	desc->mask_ack(irq);
	spin_unlock(&ucb1200_irq_lock);

	cpu = smp_processor_id();
#if 0
	irq_enter(cpu, irq);
#endif
	ucb1200_irqs[cpu][irq]++;
	desc->triggered = 1;

	action = desc->action;

	if (action) {
		int status = 0;

		if (desc->nomask) {
			spin_lock(&ucb1200_irq_lock);
			desc->unmask(irq);
			spin_unlock(&ucb1200_irq_lock);
		}

		if (!(action->flags & SA_INTERRUPT))
			__sti();

		do {
			status |= action->flags;
			action->handler(UCB1200_IRQ(irq), action->dev_id, regs);
			action = action->next;
		} while(action);

#if 0
		/* XXX this won't work! */
		if (status & SA_SAMPLE_RANDOM)
			add_interrupt_randomness(irq);
#endif
		__cli();

		if (!desc->nomask && desc->enabled) {
			spin_lock(&ucb1200_irq_lock);
			desc->unmask(irq);
			spin_unlock(&ucb1200_irq_lock);
		}
	}

#if 0
	check_irq_lock(desc, irq, regs);

	irq_exit(cpu, irq);

	if (softirq_active(cpu) & softirq_mask(cpu))
		do_softirq();
#endif
}

static void ucb1200_irq_demux(int irq, void *dev_id, struct pt_regs *regs)
{
	u16 isr;
	int i;

	while (1) {
		spin_lock(&ucb1200_lock);
		sib_enable();

		isr = sib_read_codec(UCB1200_REG_INT_STATUS);

		sib_disable();
		spin_unlock(&ucb1200_lock);

		if (!isr)
			break;

		for (i = 0; i < UCB1200_NR_IRQS; i++) {
			if (isr & (1 << i)) {
				do_UCB1200_IRQ( i, regs );
			}
		}
	}
}

void ucb1200_set_irq_edge(int irq_mask, int edge)
{
	if (edge & GPIO_FALLING_EDGE)
		ucb1200_irq_falling_edge |= irq_mask;
	else
		ucb1200_irq_falling_edge &= ~irq_mask;

	if (edge & GPIO_RISING_EDGE)
		ucb1200_irq_rising_edge |= irq_mask;
	else
		ucb1200_irq_rising_edge &= ~irq_mask;
}

void ucb1200_disable_irq(unsigned int irq)
{
	unsigned long flags;

	irq = UCB1200_IRQ_TO_IO(irq);

	spin_lock_irqsave(&ucb1200_irq_lock, flags);
	irq_desc[irq].enabled = 0;
	irq_desc[irq].mask(irq);
	spin_unlock_irqrestore(&ucb1200_irq_lock, flags);
}

void ucb1200_enable_irq(unsigned int irq)
{
	unsigned long flags;

	irq = UCB1200_IRQ_TO_IO(irq);

	spin_lock_irqsave(&ucb1200_irq_lock, flags);
	irq_desc[irq].probing = 0;
	irq_desc[irq].triggered = 0;
	irq_desc[irq].enabled = 1;
	irq_desc[irq].unmask(irq);
	spin_unlock_irqrestore(&ucb1200_irq_lock, flags);
}

static inline int ucb1200_setup_irq(int irq, struct irqaction * new)
{
	int shared = 0;
	struct irqaction *old, **p;
	unsigned long flags;
	struct irqdesc *desc;

#if 0
	/* XXX this doesn't work! */
	if (new->flags & SA_SAMPLE_RANDOM) {
		rand_initialize_irq(irq);
	}
#endif

	desc = irq_desc + irq;
	spin_lock_irqsave(&ucb1200_irq_lock, flags);
	p = &desc->action;
	if ((old = *p) != NULL) {
		if (!(old->flags & new->flags & SA_SHIRQ)) {
			spin_unlock_irqrestore(&ucb1200_irq_lock, flags);
			return -EBUSY;
		}

		do {
			p = &old->next;
			old = *p;
		} while (old);
		shared = 1;
	}

	*p = new;

	if (!shared) {
		desc->nomask = (new->flags & SA_IRQNOMASK) ? 1 : 0;
		desc->probing = 0;
		if (!desc->noautoenable) {
			desc->enabled = 1;
			desc->unmask(irq);
		}
	}

	spin_unlock_irqrestore(&ucb1200_irq_lock, flags);
	return 0;
}

int ucb1200_request_irq(unsigned int irq,
			void (*handler)(int, void *, struct pt_regs *),
			unsigned long irq_flags, const char *devname,
			void *dev_id)
{
	unsigned long retval;
	struct irqaction *action;

	irq = UCB1200_IRQ_TO_IO(irq);

	if (irq >= UCB1200_NR_IRQS || !irq_desc[irq].valid || !handler ||
	    (irq_flags & SA_SHIRQ && !dev_id))
		return -EINVAL;

	action = (struct irqaction *)kmalloc(sizeof(struct irqaction), GFP_KERNEL);
	if (!action)
		return -ENOMEM;

	action->handler = handler;
	action->flags = irq_flags;
	action->mask = 0;
	action->name = devname;
	action->next = NULL;
	action->dev_id = dev_id;

	retval = ucb1200_setup_irq(irq, action);
	if (retval)
		kfree(action);

	MOD_INC_USE_COUNT;

	return retval;
}

void ucb1200_free_irq(unsigned int irq, void *dev_id)
{
	struct irqaction * action, **p;
	unsigned long flags;

	irq = UCB1200_IRQ_TO_IO(irq);

	if (irq >= UCB1200_NR_IRQS || !irq_desc[irq].valid) {
		printk(KERN_ERR "Trying to free IRQ%d\n",irq);
		return;
	}

	spin_lock_irqsave(&ucb1200_irq_lock, flags);
	for (p = &irq_desc[irq].action; (action = *p) != NULL; p = &action->next) {
		if (action->dev_id != dev_id)
			continue;

		/* Found it - now free it */
		*p = action->next;
		kfree(action);
		MOD_DEC_USE_COUNT;
		goto out;
	}
	printk(KERN_ERR "Trying to free free IRQ%d\n",irq);
out:
	spin_unlock_irqrestore(&ucb1200_irq_lock, flags);
}

void ucb1200_set_io_direction(int io, int dir)
{
	u16 data;
	u16 mask = (1 << io) & 0x03ff;
	unsigned long flags;

	ucb1200_lock_enable(flags);

	data = sib_read_codec(UCB1200_REG_IO_DIRECTION);

	if (dir)
		data |= mask;
	else
		data &= ~mask;

	sib_write_codec(UCB1200_REG_IO_DIRECTION, data);

	ucb1200_unlock_disable(flags);
}

int ucb1200_test_io(int io)
{
	u16 data;
	unsigned long flags;

	ucb1200_lock_enable(flags);

	data = sib_read_codec(UCB1200_REG_IO_DATA);

	ucb1200_unlock_disable(flags);

	return ((data & ((1 << io) & 0x03ff)) != 0);
}

void ucb1200_set_io(int io, int level)
{
	u16 data;
	u16 mask = (1 << io) & 0x03ff;
	unsigned long flags;

	ucb1200_lock_enable(flags);

	data = sib_read_codec(UCB1200_REG_IO_DATA);

	if (level)
		data |= mask;
	else
		data &= ~mask;

#ifdef CONFIG_SA1100_COLLIE
	if( !( data & TC35143_GPIO_AMP_ON ) ) {
	  //	  printk("error! force on %8x\n",data);
	  data |= TC35143_GPIO_AMP_ON;
	}
#endif

	sib_write_codec(UCB1200_REG_IO_DATA, data);

#ifdef CONFIG_SA1100_COLLIE
	{
	  u16 data2;
	  data2 = sib_read_codec(UCB1200_REG_IO_DATA);
	  if ( data != data2 ) {
	    //	    printk("error %8x\n",data2);
	    sib_write_codec(UCB1200_REG_IO_DATA, data);
	  }
	}
#endif


	ucb1200_unlock_disable(flags);
}

u16 ucb1200_read_reg(u16 reg)
{
	u16 data;
	unsigned long flags;

	ucb1200_lock_enable(flags);

	data = sib_read_codec(reg);

	ucb1200_unlock_disable(flags);

	return data;
}

void ucb1200_write_reg(u16 reg, u16 data)
{
	unsigned long flags;

	ucb1200_lock_enable(flags);

	sib_write_codec(reg, data);

	ucb1200_unlock_disable(flags);
}

void ucb1200_enable_adc_sync()
{
	ucb1200_arch.flags |= UCB1200_ADC_SYNC;
}

void ucb1200_disable_adc_sync()
{
	ucb1200_arch.flags &= ~UCB1200_ADC_SYNC;
}

void ucb1200_start_adc(u16 input)
{
	u16 reg = (input & 0x001c) | ADC_ENA;
	unsigned long flags;

	ucb1200_lock_enable(flags);

#ifdef CONFIG_SA1100_COLLIE
	if (ucb1200_arch.flags & UCB1200_ADC_SYNC & !( reg & 0x0010 ) )
#else
	if (ucb1200_arch.flags & UCB1200_ADC_SYNC)
#endif
		reg |= ADC_SYNC_ENA;

	sib_write_codec(UCB1200_REG_ADC_CTL, reg);
	udelay(50);

#ifdef CONFIG_SA1100_COLLIE
	if ( input == ADC_INPUT_AD1 )
	  //	  udelay(700);
	  udelay(1500);
#endif

	/* 0->1 transition of the ADC_START bit arms the ADC */
	sib_write_codec(UCB1200_REG_ADC_CTL, reg | ADC_START);

	sib_write_codec(UCB1200_REG_ADC_CTL, reg);

#ifdef CONFIG_SA1100_COLLIE
	if (ucb1200_arch.flags & UCB1200_ADC_SYNC & !( reg & 0x0010 ) ) {
#else
	if (ucb1200_arch.flags & UCB1200_ADC_SYNC) {
#endif
		ucb1200_set_adc_sync(1);
		ucb1200_set_adc_sync(0);
	}

	ucb1200_unlock_disable(flags);
}

void ucb1200_stop_adc(void)
{
	ucb1200_write_reg(UCB1200_REG_ADC_CTL, 0);
}

u16 ucb1200_read_adc(void)
{
	u16 data;
	unsigned long flags;

	ucb1200_lock_enable(flags);

	while (!((data = sib_read_codec(UCB1200_REG_ADC_DATA)) & ADC_DAT_VAL));

	data = GET_ADC_DATA(data);

	ucb1200_unlock_disable(flags);

	return data;
}

void ucb1200_start_audio(void)
{
	sib_enable();
}

void ucb1200_stop_audio(void)
{
	sib_disable();
}

#ifdef CONFIG_PROC_FS
static inline int ucb1200_stat_irqs(int irq)
{
	int i, sum=0;

	for (i = 0 ; i < smp_num_cpus ; i++)
		sum += ucb1200_irqs[cpu_logical_map(i)][irq];

	return sum;
}

static inline int ucb1200_get_irq_list(char *buf)
{
	int i;
	struct irqaction *action;
	char *p = buf;

	for (i = 0 ; i < UCB1200_NR_IRQS ; i++) {
	    	action = irq_desc[i].action;
		if (!action)
			continue;
		p += sprintf(p, "%3d: %10u ", i, ucb1200_stat_irqs(i));
		p += sprintf(p, "  %s", action->name);
		for (action = action->next; action; action = action->next) {
			p += sprintf(p, ", %s", action->name);
		}
		*p++ = '\n';
	}

#if 0
	p += sprintf(p, "Err: %10lu\n", irq_err_count);
#endif
	return p - buf;
}

struct proc_dir_entry *proc_ucb1200;

static int ucb1200_reg_read_proc(char *buf, char **start, off_t offset, int len)
{
	char *p = buf;
	int i;

	for (i = 0; i <= 13; i++) {
		p += sprintf(p, "%2d: 0x%04x\n", i, ucb1200_read_reg(i));
	}

	return p - buf;
}

static int ucb1200_irq_read_proc(char *buf, char **start, off_t offset, int len)
{
	int count = ucb1200_get_irq_list(buf);
	return count;
}

static int ucb1200_io_read_proc(char *buf, char **start, off_t offset, int len)
{
	char *p = buf;
	int i;
	u16 dir = ucb1200_read_reg(UCB1200_REG_IO_DIRECTION);
	u16 data = ucb1200_read_reg(UCB1200_REG_IO_DATA);

	for (i = 0; i < 10; i++) {
		p += sprintf(p, "%d\t%s\t%s\n", i,
			     dir & (1 << i) ? "out" : "in",
			     data & (1 << i) ? "set" : "clear");
	}

	return p - buf;
}
#endif

int __init ucb1200_init(void)
{
	int irq, ret, i;
	unsigned long flags;
	static int ucb1200_initialized = 0;


	if (ucb1200_initialized)
		return 0;

	ucb1200_low_level = &arch_ucb1200_ops;

	if (ucb1200_low_level == NULL) {
		return -ENODEV;
	}

	/* Initialize arch specific SIB(Serial Interface Bus) */
	if (ucb1200_arch_init(&ucb1200_arch) || (ucb1200_arch.irq == NO_IRQ)) {
		return -ENODEV;
	}

	for (irq = 0; irq < UCB1200_NR_IRQS; irq++) {
		irq_desc[irq].valid    = 1;
		irq_desc[irq].probe_ok = 1;
		irq_desc[irq].mask_ack = ucb1200_mask_and_ack_irq;
		irq_desc[irq].mask     = ucb1200_mask_irq;
		irq_desc[irq].unmask   = ucb1200_unmask_irq;
	}

#ifdef CONFIG_PROC_FS
	proc_ucb1200 = proc_mkdir("driver/ucb1200", NULL);
	proc_ucb1200->owner = THIS_MODULE;
	create_proc_info_entry("registers", 0, proc_ucb1200,
			       ucb1200_reg_read_proc);
	create_proc_info_entry("interrupts", 0, proc_ucb1200,
			       ucb1200_irq_read_proc);
	create_proc_info_entry("io", 0, proc_ucb1200,
			       ucb1200_io_read_proc);
#endif

	ret = request_irq(ucb1200_arch.irq, ucb1200_irq_demux, SA_INTERRUPT,
			  "UCB1200", NULL);
	if (ret != 0) {
		printk("Failed to allocate irq %d.\n", ucb1200_arch.irq);
		ucb1200_arch_exit();
		return ret;
	}

	/* Init all registers to reasonable defaults */
	ucb1200_lock_enable(flags);

	for (i = 2; i <= 10; i++) {
		sib_write_codec(i, 0);
	}
	sib_write_codec(UCB1200_REG_INT_STATUS, 0xffff);
#ifndef CONFIG_SA1100_COLLIE
	sib_write_codec(UCB1200_REG_TELECOM_CTL_A, 127);
	sib_write_codec(UCB1200_REG_AUDIO_CTL_A, 127);
#endif
	sib_write_codec(UCB1200_REG_MODE, 0);

	ucb1200_unlock_disable(flags);
	
	printk("UCB1200 generic module installed\n");

	ucb1200_initialized = 1;

	return 0;
}

void __init ucb1200_cleanup(void)
{

	free_irq(ucb1200_arch.irq, NULL);

	ucb1200_arch_exit();

#ifdef CONFIG_PROC_FS
	remove_proc_entry("io", proc_ucb1200);
	remove_proc_entry("interrupts", proc_ucb1200);
	remove_proc_entry("registers", proc_ucb1200);
	remove_proc_entry("driver/ucb1200", NULL);
#endif
	printk("UCB1200 generic module removed\n");

}

void ucb1200_suspend(void)
{
	ucb1200_arch_exit();
}


int ucb1200_resume(void)
{
	int i;
	unsigned long flags;


	/* Initialize arch specific SIB(Serial Interface Bus) */
	if (ucb1200_arch_init(&ucb1200_arch) || (ucb1200_arch.irq == NO_IRQ)) {
		return -ENODEV;
	}

	/* Init all registers to reasonable defaults */
	ucb1200_lock_enable(flags);

	for (i = 2; i <= 10; i++) {
		sib_write_codec(i, 0);
	}
	sib_write_codec(UCB1200_REG_INT_STATUS, 0xffff);
	sib_write_codec(UCB1200_REG_TELECOM_CTL_A, 127);
	sib_write_codec(UCB1200_REG_AUDIO_CTL_A, 127);
	sib_write_codec(UCB1200_REG_MODE, 0);

	ucb1200_unlock_disable(flags);


	return 0;
}



module_init(ucb1200_init);
module_exit(ucb1200_cleanup);

EXPORT_SYMBOL(ucb1200_suspend);
EXPORT_SYMBOL(ucb1200_resume);

EXPORT_SYMBOL(ucb1200_disable_irq);
EXPORT_SYMBOL(ucb1200_enable_irq);
EXPORT_SYMBOL(ucb1200_request_irq);
EXPORT_SYMBOL(ucb1200_free_irq);

EXPORT_SYMBOL(ucb1200_set_io_direction);
EXPORT_SYMBOL(ucb1200_test_io);
EXPORT_SYMBOL(ucb1200_set_io);

EXPORT_SYMBOL(ucb1200_read_reg);
EXPORT_SYMBOL(ucb1200_write_reg);

EXPORT_SYMBOL(ucb1200_start_adc);
EXPORT_SYMBOL(ucb1200_read_adc);
EXPORT_SYMBOL(ucb1200_stop_adc);

EXPORT_SYMBOL(ucb1200_start_audio);
EXPORT_SYMBOL(ucb1200_stop_audio);

EXPORT_SYMBOL(proc_ucb1200);
