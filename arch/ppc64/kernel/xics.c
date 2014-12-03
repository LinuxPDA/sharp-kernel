/*
 * arch/ppc/kernel/xics.c
 *
 * Copyright 2000 IBM Corporation.
 *
 *  This program is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU General Public License
 *  as published by the Free Software Foundation; either version
 *  2 of the License, or (at your option) any later version.
 */
#include <linux/config.h>
#include <linux/types.h>
#include <linux/threads.h>
#include <linux/kernel.h>
#include <linux/sched.h>
#include <asm/prom.h>
#include <asm/io.h>
#include <asm/smp.h>
#include <asm/Naca.h>
#include "i8259.h"
#include "xics.h"
#include <asm/ppcdebug.h>

extern struct Naca *naca;

void xics_enable_irq(u_int irq);
void xics_disable_irq(u_int irq);
void xics_mask_and_ack_irq(u_int irq);
void xics_end_irq(u_int irq);

struct hw_interrupt_type xics_pic = {
	" XICS     ",
	NULL,
	NULL,
	xics_enable_irq,
	xics_disable_irq,
	xics_mask_and_ack_irq,
	xics_end_irq
};

struct hw_interrupt_type xics_8259_pic = {
	" XICS/8259",
	NULL,
	NULL,
	NULL,
	NULL,
	xics_mask_and_ack_irq,
	NULL
};

#define XICS_IPI		2
#define XICS_IRQ_OFFSET		0x10

#define XICS_IRQ_SPURIOUS	0

#define	DEFAULT_PRIORITY	0

struct xics_ipl {
	union {
		u32	word;
		u8	bytes[4];
	} xirr_poll;
	union {
		u32 word;
		u8	bytes[4];
	} xirr;
	u32	dummy;
	union {
		u32	word;
		u8	bytes[4];
	} qirr;
};

struct xics_info {
	volatile struct xics_ipl *	per_cpu[NR_CPUS];
};

struct xics_info	xics_info;

#define xirr_info(n_cpu)	(xics_info.per_cpu[n_cpu]->xirr.word)
#define cppr_info(n_cpu)	(xics_info.per_cpu[n_cpu]->xirr.bytes[0])
#define poll_info(n_cpu)	(xics_info.per_cpu[n_cpu]->xirr_poll.word)
#define qirr_info(n_cpu)	(xics_info.per_cpu[n_cpu]->qirr.bytes[0])

unsigned long long intr_base = 0;
unsigned int xics_irq_8259_cascade = 0;

struct xics_interrupt_node {
	unsigned long long addr;
	unsigned long long size;
} inodes[NR_CPUS*2];	 

void
xics_enable_irq(
	u_int	irq
	)
{
	unsigned long	status;
	long	        call_status;

	irq -= XICS_IRQ_OFFSET;
	if (irq == XICS_IPI)
		return;
	call_status = call_rtas("ibm,set-xive", 3, 1, (unsigned long*)&status,
				irq, cpu_hw_index[0], DEFAULT_PRIORITY);
	if( call_status != 0 ) {
		printk("xics_enable_irq: irq=%x: call_rtas failed; retn=%x, status=%x\n",
		       irq, call_status, status);
		return;
	}
}

void
xics_disable_irq(
	u_int	irq
	)
{
	unsigned long 	status;
	long 	        call_status;

	irq -= XICS_IRQ_OFFSET;
	call_status = call_rtas("ibm,int-off", 1, 1, (unsigned long*)&status, 
				irq);
	if( call_status != 0 ) {
		printk("xics_disable_irq: irq=%x: call_rtas failed, retn=%x\n",
		       irq, call_status);
		return;
	}
}

void
xics_end_irq(
	u_int	irq
	)
{
	int cpu = smp_processor_id();

	cppr_info(cpu) = 0; /* actually the value overwritten by ack */
	iosync();
	xirr_info(cpu) = (0xff<<24) | (irq-XICS_IRQ_OFFSET);
	iosync();
}

void
xics_mask_and_ack_irq(
	u_int	irq
	)
{
	int cpu = smp_processor_id();

	if( irq < XICS_IRQ_OFFSET ) {
		i8259_pic.ack(irq);
		iosync();
		xirr_info(cpu) = (0xff<<24) | xics_irq_8259_cascade;
		iosync();
	}
	else {
		cppr_info(cpu) = 0xff;
		iosync();
	}
}

int
xics_get_irq(struct pt_regs *regs)
{
	u_int	cpu = smp_processor_id();
	u_int	vec;
	int irq;
  
	vec = xirr_info(cpu);
	/*  (vec >> 24) == old priority */
	vec &= 0x00ffffff;
	/* for sanity, this had better be < NR_IRQS - 16 */
	if( vec == xics_irq_8259_cascade ) {
		irq = i8259_irq(cpu);
		if(irq == -1) {
			/* Spurious cascaded interrupt.  Still must ack xics */
                        xics_end_irq(XICS_IRQ_OFFSET + xics_irq_8259_cascade);
		}
	} else if( vec == XICS_IRQ_SPURIOUS )
		irq = -1;
	else
		irq = vec + XICS_IRQ_OFFSET;
	return irq;
}

int
xics_to_irq(int line)
{
	int irq;

	irq =  line + XICS_IRQ_OFFSET;
	return irq;
}

#ifdef CONFIG_SMP
void xics_ipi_action(int irq, void *dev_id, struct pt_regs *regs)
{
	extern volatile unsigned long xics_ipi_message[];
	int cpu = smp_processor_id();
       
        qirr_info(cpu) = 0xff;

	while (xics_ipi_message[cpu]) {
		if (test_and_clear_bit(PPC_MSG_CALL_FUNCTION, &xics_ipi_message[cpu])) {
			mb();
			smp_message_recv(PPC_MSG_CALL_FUNCTION, regs);
		}
		if (test_and_clear_bit(PPC_MSG_RESCHEDULE, &xics_ipi_message[cpu])) {
			mb();
			smp_message_recv(PPC_MSG_RESCHEDULE, regs);
		}
	}
	
}

void xics_cause_IPI(int cpu)
{
	qirr_info(cpu) = 0;
}

void xics_setup_cpu(void)
{
	int cpu = smp_processor_id();

	cppr_info(cpu) = 0xff;
	iosync();
}
#endif /* CONFIG_SMP */

void
xics_init_IRQ( void )
{
	int i;
	unsigned long intr_size = 0;
	struct device_node *np;
	uint *ireg, ilen, indx=0;

	np = find_type_devices("PowerPC-External-Interrupt-Presentation");
	if (!np) {
		printk(KERN_WARNING "Can't find Interrupt Presentation\n");
		udbg_printf("Can't find Interrupt Presentation\n");
		while (1);
	}
nextnode:
	ireg = (uint *)get_property(np, "ibm,interrupt-server-ranges", 0);
	if (ireg) {
		/*
		 * set node starting index for this node
		 */
		indx = *ireg;
	}

	ireg = (uint *)get_property(np, "reg", &ilen);
	if (!ireg) {
		printk(KERN_WARNING "Can't find Interrupt Reg Property\n");
		udbg_printf("Can't find Interrupt Reg Property\n");
		while (1);
	}
	
	while (ilen) {
		inodes[indx].addr = (unsigned long long)*ireg++ << 32;
		ilen -= sizeof(uint);
		inodes[indx].addr |= *ireg++;
		ilen -= sizeof(uint);
		inodes[indx].size = (unsigned long long)*ireg++ << 32;
		ilen -= sizeof(uint);
		inodes[indx].size |= *ireg++;
		ilen -= sizeof(uint);
		indx++;
		if (indx >= NR_CPUS) break;
	}

	np = np->next;
	if ((indx < NR_CPUS) && np) goto nextnode;

	/*
	 * XXX Assume for now that nodes are in order
	 *     We could (and should) get the "interrupt-server-ranges" property
	 *     for each "presentation" node, and for a given CPU, look at
	 *     the CPU's "ibm,ppc-interrupt-server#s" property to see which
	 *     "presentation" node this CPU is routed to.  But for now, the
	 *     server numbers appear to match the physical cpu index, and
	 *     are sequentially assigned to the nodes in order.
	 * XXX Also a gross assumption in the reload handler (hashtable.S)
	 *     that all CPU's XICS nodes addresses have the same upper 16 bits
	 */
	intr_base = inodes[0].addr;
	intr_size = (ulong)inodes[0].size;

	np = find_type_devices("interrupt-controller");
	if (!np) {
		printk(KERN_WARNING "Can't find ISA Interrupt Controller\n");
		udbg_printf("Can't find ISA Interrupt Controller\n");
		while (1);
	}
	ireg = (uint *) get_property(np, "interrupts", 0);
	if (!ireg) {
		printk(KERN_WARNING "Can't find ISA Interrupts Property\n");
		udbg_printf("Can't find ISA Interrupts Property\n");
		while (1);
	}
	
	xics_irq_8259_cascade = *ireg;

#ifdef CONFIG_SMP
	for (i = 0; i < naca->processorCount; ++i) {
	    xics_info.per_cpu[i] =
		ioremap((ulong)inodes[cpu_hw_index[i]].addr, 
					(ulong)inodes[cpu_hw_index[i]].size);
	}
#else
	xics_info.per_cpu[0] = ioremap((ulong)intr_base, intr_size);
#endif /* CONFIG_SMP */

	xics_8259_pic.enable = i8259_pic.enable;
	xics_8259_pic.disable = i8259_pic.disable;
	for (i = 0; i < 16; ++i)
		irq_desc[i].handler = &xics_8259_pic;
	for (; i < NR_IRQS; ++i)
		irq_desc[i].handler = &xics_pic;

	cppr_info(0) = 0xff;
	iosync();
	if (request_irq(xics_irq_8259_cascade + XICS_IRQ_OFFSET, no_action,
			0, "8259 cascade", 0))
		printk(KERN_ERR "xics_init_IRQ: couldn't get 8259 cascade\n");
	i8259_init();

#ifdef CONFIG_SMP
	request_irq(XICS_IPI + XICS_IRQ_OFFSET, xics_ipi_action, 0, "IPI", 0);
	irq_desc[XICS_IPI+XICS_IRQ_OFFSET].status |= IRQ_PER_CPU;
#endif
}

void xics_isa_init(void)
{
	return;
	if (request_irq(xics_irq_8259_cascade + XICS_IRQ_OFFSET, no_action,
			0, "8259 cascade", 0))
		printk(KERN_ERR "xics_init_IRQ: couldn't get 8259 cascade\n");
	i8259_init();
}
