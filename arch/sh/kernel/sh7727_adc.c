/*
 * linux/arch/sh/kernel/sh7727_adc.c
 *
 * Copyright (c) 2003 Lineo uSolutions, Inc.
 * Copyright (c) 2001, 2002 Lineo Japan, Inc.
 * 
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * Hitachi SH7727 ADC support.
 *
 */
#include <linux/config.h>
#include <linux/init.h>
#include <linux/irq.h>
#include <linux/errno.h>
#include <linux/sched.h>
#include <linux/kernel.h>
#include <linux/param.h>
#include <linux/string.h>
#include <linux/mm.h>
#include <linux/vmalloc.h>
#include <linux/interrupt.h>
#include <linux/time.h>
#include <linux/delay.h>
#include <linux/init.h>
#include <linux/smp.h>
#include <linux/pci.h>

#include <asm/sh7727.h>
#include <asm/io.h>
#include <asm/irq.h>
#include <asm/pgtable.h>
#include <asm/pgalloc.h>

/* #define DEBUG */
#undef DEBUG

static int sh7727_adc_interrupt_handler_installed = 0;
static adc_callback_t sh7727_adc_callback_handler[MAX_ADC_CHANNELS+1];

static void sh7727_adc_interrupt(int irq, void* ptr, struct pt_regs* regs)
{
	int ch;
#ifdef DEBUG
	printk("sh7727_adc_interrupt(%d): ADCSR=%02X\n",
	       irq, ctrl_inb(ADCSR));
#endif
	ch = (ctrl_inb(ADCSR) & 0x07);
	if (sh7727_adc_callback_handler[ch]) {
		(*sh7727_adc_callback_handler[ch])(irq, ptr, regs);
	}
}

void sh7727_adc_set_callback(unsigned int ch,
			     void (*handler)(int, void*, struct pt_regs*))
{
	if (ch > MAX_ADC_CHANNELS)
		return;
	sh7727_adc_callback_handler[ch] = handler;
}

int sh7727_adc_start(unsigned int ch)
{
	int ret = 0;

	if (sh7727_adc_interrupt_handler_installed == 0) {
		if ((ret = request_irq(ADC_IRQ, sh7727_adc_interrupt,
				SA_INTERRUPT, "adc", NULL)) < 0) {
#ifdef DEBUG
			printk("sh7727_adc_start: "
			       "failed to register ADC_IRQ\n");
#endif
			return ret;
		}
		sh7727_adc_interrupt_handler_installed = 1;
	}

	ctrl_outb(ADCSR_ADIE|ADCSR_ADST|ch, ADCSR);

	return ret;
}

void sh7727_adc_stop(void)
{
	ctrl_outb(0, ADCSR);
}

unsigned int sh7727_adc_read(unsigned int ch)
{
	unsigned int value = 0;

	switch (ch) {
	case ADC_AN4:
		return ((((unsigned int)ctrl_inb(ADDRAH)) << 2) +
			(((unsigned int)ctrl_inb(ADDRAL)) >> 6));
	case ADC_AN5:
		return ((((unsigned int)ctrl_inb(ADDRBH)) << 2) +
			(((unsigned int)ctrl_inb(ADDRBL)) >> 6));
	case ADC_AN2:
	case ADC_AN6:
		return ((((unsigned int)ctrl_inb(ADDRCH)) << 2) +
			(((unsigned int)ctrl_inb(ADDRCL)) >> 6));
	case ADC_AN3:
	case ADC_AN7:
		return ((((unsigned int)ctrl_inb(ADDRDH)) << 2) +
			(((unsigned int)ctrl_inb(ADDRDL)) >> 6));
	}	  
	return value;
}

void __init sh7727_adc_init(void)
{
	int i;

	for (i = 0; i <= MAX_ADC_CHANNELS; i++) {
		sh7727_adc_callback_handler[i] = NULL;
	}
	ctrl_outb(ADCR_TRGE1|ADCR_TRGE0, ADCR);
	//ctrl_outb(0, ADCR);
}
