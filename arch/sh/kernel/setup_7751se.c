/* 
 * linux/arch/sh/kernel/setup_7751se.c
 *
 * Copyright (C) 2000  Kazumoto Kojima
 * Copyright (C) 2003  Takashi Kusuda(kusuda-takashi@hitachi-ul.co.jp)
 *
 * Hitachi SolutionEngine Support.
 *
 * Modified for 7751 Solution Engine by
 * Ian da Silva and Jeremy Siegel, 2001.
 */

#include <linux/config.h>
#include <linux/init.h>
#include <linux/irq.h>

#include <linux/hdreg.h>
#include <linux/ide.h>
#include <asm/io.h>
#include <asm/hitachi_7751se.h>
#include <asm/m1543c.h>

static unsigned char m_irq_mask = 0xfb;
static unsigned char s_irq_mask = 0xff;
volatile unsigned long irq_err_count;

static void disable_se51_irq(unsigned int irq)
{
        unsigned long flags;

        save_and_cli(flags);
        if( irq < 8) {
                m_irq_mask |= (1 << irq);
                outb(m_irq_mask,I8259_M_MR);
        } else {
                s_irq_mask |= (1 << (irq - 8));
                outb(s_irq_mask,I8259_S_MR);
        }
        restore_flags(flags);

}

static void enable_se51_irq(unsigned int irq)
{
        unsigned long flags;

        save_and_cli(flags);

        if( irq < 8) {
                m_irq_mask &= ~(1 << irq);
                outb(m_irq_mask,I8259_M_MR);
        } else {
                s_irq_mask &= ~(1 << (irq - 8));
                outb(s_irq_mask,I8259_S_MR);
        }
        restore_flags(flags);
}

static inline int se51_irq_real(unsigned int irq)
{
        int value;
        int irqmask;

        if ( irq < 8) {
                irqmask = 1<<irq;
                outb(0x0b,I8259_M_CR);          /* ISR register */
                value = inb(I8259_M_CR) & irqmask;
                outb(0x0a,I8259_M_CR);          /* back ro the IPR reg */
                return value;
        }
        irqmask = 1<<(irq - 8);
        outb(0x0b,I8259_S_CR);          /* ISR register */
        value = inb(I8259_S_CR) & irqmask;
        outb(0x0a,I8259_S_CR);          /* back ro the IPR reg */
        return value;
}

static void mask_and_ack_se51(unsigned int irq)
{
        unsigned long flags;

        save_and_cli(flags);

        if(irq < 8) {
                if(m_irq_mask & (1<<irq)){
                  if(!se51_irq_real(irq)){
                    irq_err_count++;
                    printk("spurious 8259A interrupt: IRQ %x\n",irq);
                   }
                } else {
                        m_irq_mask |= (1<<irq);
                }
                inb(I8259_M_MR);                /* DUMMY */
                outb(m_irq_mask,I8259_M_MR);    /* disable */
                outb(0x60+irq,I8259_M_CR);      /* EOI */

        } else {
                if(s_irq_mask & (1<<(irq - 8))){
                  if(!se51_irq_real(irq)){
                    irq_err_count++;
                    printk("spurious 8259A interrupt: IRQ %x\n",irq);
                  }
                } else {
                        s_irq_mask |= (1<<(irq - 8));
                }
                inb(I8259_S_MR);                /* DUMMY */
                outb(s_irq_mask,I8259_S_MR);    /* disable */
                outb(0x60+(irq-8),I8259_S_CR);  /* EOI */
                outb(0x60+2,I8259_M_CR);
        }
        restore_flags(flags);
}

static void end_se51_irq(unsigned int irq)
{
        enable_se51_irq(irq);
}

static unsigned int startup_se51_irq(unsigned int irq)
{
        enable_se51_irq(irq);
        return 0;
}

static void shutdown_se51_irq(unsigned int irq)
{
        disable_se51_irq(irq);
}

static struct hw_interrupt_type se51_irq_type = {
        "MS7751SE-IRQ",
        startup_se51_irq,
        shutdown_se51_irq,
        enable_se51_irq,
        disable_se51_irq,
        mask_and_ack_se51,
        end_se51_irq
};

static void make_se51_irq(unsigned int irq)
{
        irq_desc[irq].handler = &se51_irq_type;
        irq_desc[irq].status  = IRQ_DISABLED;
        irq_desc[irq].action  = 0;
        irq_desc[irq].depth   = 1;
        disable_se51_irq(irq);
}

int se51_irq_demux(int irq)
{
        unsigned int poll;

        if( irq == 2 ) {
                outb(0x0c,I8259_M_CR);
                poll = inb(I8259_M_CR);
                if(poll & 0x80) {
                        irq = (poll & 0x07);
                }
                if( irq == 2) {
                        outb(0x0c,I8259_S_CR);
                        poll = inb(I8259_S_CR);
                        irq = (poll & 0x07) + 8;
                }
        }
        return irq;
}

/*
 * Initialize IRQ setting
 */
void __init init_7751se_IRQ(void)
{
        int i;

        *(unsigned short *)BCR_ILCRA = 0x0000;/* disable SLOT_IRQ8/7/6/5 */
        *(unsigned short *)BCR_ILCRB = 0x0000;/* disable SLOT_IRQ4/3/2/1 */
        *(unsigned short *)BCR_ILCRC = 0x0000;/* disable PCIRQ3/2/1/0 */
        *(unsigned short *)BCR_ILCRD = 0x0000;/* disable PCINTA/B/C/D */
        *(unsigned short *)BCR_ILCRE = 0x000d;/* enable M8259 INTR */

        outb(0x11, I8259_M_CR);         /* mater icw1 edge trigger  */
        outb(0x11, I8259_S_CR);         /* slave icw1 edge trigger  */
        outb(0x20, I8259_M_MR);         /* m icw2 base vec 0x08     */
        outb(0x28, I8259_S_MR);         /* s icw2 base vec 0x70     */
        outb(0x04, I8259_M_MR);         /* m icw3 slave irq2        */
        outb(0x02, I8259_S_MR);         /* s icw3 slave id          */
        outb(0x01, I8259_M_MR);         /* m icw4 non buf normal eoi*/
        outb(0x01, I8259_S_MR);         /* s icw4 non buf normal eo1*/
        outb(0xfb, I8259_M_MR);         /* disable irq0--irq7  */
        outb(0xff, I8259_S_MR);         /* disable irq8--irq15 */

	/*
	 * IRQ0:
	 * IRQ1:  PS/2 Keyboard
	 * IRQ2:  Slot IRQ1
	 * IRQ3:  UART3(COM2)
	 * IRQ4:  UART1(COM1)
	 * IRQ5:  Parallel Port
	 * IRQ6:  Floppy
	 * IRQ7:  LAN AM79C973
	 * IRQ8:
	 * IRQ9:  PCI Slot 1
	 * IRQ10: PCI Slot 2
	 * IRQ11: USB
	 * IRQ12: PS/2 Mouse
	 * IRQ13:
	 * IRQ14: IDE0 or CF
	 * IRQ15: IDE1
	 */
        for (i = 0; i < 16; i++){
		switch(i){
		    case 2:
			make_ipr_irq(2, BCR_ILCRB, 0, 0x0f-2); /* SLOT IRQ1 */
			break;
		    case 7:
			make_ipr_irq(7, BCR_ILCRD, 3, 0x0f-7); /* LAN AM79C973 */
			break;
		    case 9:
			make_ipr_irq(9, BCR_ILCRD, 2, 0x0f-9); /* PCI Slot 1 */
			break;
		    case 10:
			make_ipr_irq(10, BCR_ILCRD, 1, 0x0f-10); /* PCI Slot 2 */
			break;
#if defined(CONFIG_CF_ENABLER)
		    case 14:
			make_ipr_irq(14, BCR_ILCRC, 0, 0x0f-14); /* PCIRQ0 */
			break;
#endif
		    default:
                        make_se51_irq(i);
			break;
		}
        }

        /* Add additional calls to make_ipr_irq() as drivers are added
         * and tested.
         */
}

/*
 * Initialize the board
 */
void __init setup_7751se(void)
{
	/* XXX: RTC setting comes here */
}
