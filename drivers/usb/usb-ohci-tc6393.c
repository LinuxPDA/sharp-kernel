/*
 * linux/drivers/usb/usb-ohci-tc6393.c
 * 
 * (C) Copyright 2004 Lineo Solutions, Inc.
 *
 * May be copied or modified under the terms of the GNU General Public
 * License.  See linux/COPYING for more information.
 * 
 * Based on:
 *
 *  linux/drivers/usb/usb-ohci-pci.c
 *
 * and
 *
 *  linux/drivers/usb/usb-ohci-sa1111.c
 *
 *  The outline of this code was taken from Brad Parkers <brad@heeltoe.com>
 *  original OHCI driver modifications, and reworked into a cleaner form
 *  by Russell King <rmk@arm.linux.org.uk>.
 */
#include <linux/config.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/sched.h>
#include <linux/ioport.h>
#include <linux/interrupt.h>
#include <linux/slab.h>
#include <linux/usb.h>
#include <linux/pm.h>
#include <linux/delay.h>
#include <linux/ioctl.h>
#include <linux/miscdevice.h>
#include <asm/uaccess.h>

#include <asm/hardware.h>
#include <asm/irq.h>
#include <asm/io.h>
#include <asm/sharp_tc6393_usb.h>

#include "usb-ohci.h"

#define TC6393_OHCI_CTL_MINOR_DEV	222

static int tc6393_usb_internal_port_enable = 0; // default is disable
static int tc6393_usb_external_port_enable = 1; // default is enable

//#define THREAD_RESUME
#ifdef THREAD_RESUME
static int resume_thread_alive = 0;
static int tc6393_ohci_pm_resume(void);
#endif

int __devinit
hc_add_ohci(struct pci_dev *dev, int irq, void *membase, unsigned long flags,
	    ohci_t **ohci, const char *name, const char *slot_name);
extern void hc_remove_ohci(ohci_t *ohci);

static ohci_t *tc6393_ohci;

#ifdef CONFIG_PM
static int tc6393_ohci_pm_callback(
		struct pm_dev *pm_device, pm_request_t rqst, void *data);
#endif


static int do_ioctl(struct inode * inode,
			struct file *filp, u_int cmd, u_long arg)
{
	long val,res;

#ifdef THREAD_RESUME
	while(resume_thread_alive) udelay(100);
#endif
	switch (cmd) {
	case TC6393_OHCI_IOC_PDOWN:	/* Power Down */
		//printk("TC6393_OHCI_IOC_PDOWN: %d\n",arg);
		if (arg == 0) {
			TC6393_USB_REG(TC6393_USB_SVPMCS)
						|= TC6393_USB_PM_USPW1;
			tc6393_usb_external_port_enable = 0;
		} else if (arg == 1) {
			TC6393_USB_REG(TC6393_USB_SVPMCS)
						|= TC6393_USB_PM_USPW2;
			tc6393_usb_internal_port_enable = 0;
		} else
			return -EINVAL;
		return 0;
	case TC6393_OHCI_IOC_PON:	/* Power On */
		//printk("TC6393_OHCI_IOC_PON: %d\n",arg);
		if (arg == 0) {
			TC6393_USB_REG(TC6393_USB_SVPMCS)
						&= ~TC6393_USB_PM_USPW1;
			tc6393_usb_external_port_enable = 1;
		} else if (arg == 1) {
			TC6393_USB_REG(TC6393_USB_SVPMCS)
						&= ~TC6393_USB_PM_USPW2;
			tc6393_usb_internal_port_enable = 1;
		} else
			return -EINVAL;
		return 0;
	case TC6393_OHCI_IOC_PCHECK:	/* Check Power */
		if ( get_user(val, (long *) arg) )
			return -EFAULT;
		if (val == 0) {
			res = (TC6393_USB_REG(TC6393_USB_SVPMCS)&TC6393_USB_PM_USPW1)?0:1;
		} else if (val == 1) {
			res = (TC6393_USB_REG(TC6393_USB_SVPMCS)&TC6393_USB_PM_USPW2)?0:1;
		} else return -EINVAL;
		return put_user(res, (int *)arg);
	default:
		break;
	}
	return -EOPNOTSUPP;
}

static struct file_operations tc6393_ohci_fops = {
	owner:	THIS_MODULE,
	ioctl:	do_ioctl
};

static struct miscdevice tc6393_ohci_ctl_device = {
        TC6393_OHCI_CTL_MINOR_DEV,
        "tc6393_ohci_ctl",
        &tc6393_ohci_fops
};


static void tc6393_ohci_configure(void)
{
	u16 portval;
#if 0
printk("TC6393_SYS_PLL2CR: %04x\n", TC6393_SYS_REG(TC6393_SYS_PLL2CR));
TC6393_SYS_REG(TC6393_SYS_PLL2CR) = 0x0cc1;
#endif

	TC6393_SYS_REG(TC6393_SYS_CCR) |= 2;	/* clock enable */
	TC6393_SYS_REG(TC6393_SYS_FER) |= 1;	/* USB enable */

	//TC6393_USB_REG(TC6393_USB_SPPCNF) |= 1;
	TC6393_USB_REG(TC6393_USB_SPBA1)
			= TC6393_USB_OHCI_OP_REG_BASE & 0xffff;	/* LW */
	TC6393_USB_REG(TC6393_USB_SPBA2)
			= TC6393_USB_OHCI_OP_REG_BASE >> 16;	/* HW */
	TC6393_USB_REG(TC6393_USB_ILME) = 1;

	portval = TC6393_USB_PM_PMES | TC6393_USB_PM_PMEE | TC6393_USB_PM_CKRNEN | TC6393_USB_PM_GCKEN;
	if (!tc6393_usb_external_port_enable) portval |= TC6393_USB_PM_USPW1;
	if (!tc6393_usb_internal_port_enable) portval |= TC6393_USB_PM_USPW2;
	TC6393_USB_REG(TC6393_USB_SVPMCS) = portval;

	TC6393_USB_REG(TC6393_USB_INTC) = 2;

#if 0
printk("TC6393_SYS_ConfigCR: %04x\n", TC6393_SYS_REG(TC6393_SYS_ConfigCR));
printk("TC6393_SYS_CCR: %04x\n", TC6393_SYS_REG(TC6393_SYS_CCR));
printk("TC6393_SYS_PLL2CR: %04x\n", TC6393_SYS_REG(TC6393_SYS_PLL2CR));
printk("TC6393_SYS_PLL1CR: %08x\n", (*(int *)(TC6393_SYS_BASE+TC6393_SYS_PLL1CR1)));
printk("TC6393_SYS_MCR: %04x\n", TC6393_SYS_REG(TC6393_SYS_MCR));
printk("TC6393_SYS_FER: %04x\n", TC6393_SYS_REG(TC6393_SYS_FER));
printk("TC6393_USB_Rev: %d\n", TC6393_USB_REG(TC6393_USB_SPRID));
printk("TC6393_USB_SPBA: %08x\n", (*(int *)(TC6393_USB_BASE+TC6393_USB_SPBA1)));
printk("TC6393_USB_ILME: %04x\n", TC6393_USB_REG(TC6393_USB_ILME));
printk("TC6393_USB_SVPMCS: %04x\n", TC6393_USB_REG(TC6393_USB_SVPMCS));
printk("TC6393_USB_INTC: %04x\n", TC6393_USB_REG(TC6393_USB_INTC));
printk("OHCI Rev: %08x\n", (*(int *)(TC6393_SYS_BASE+TC6393_USB_OHCI_OP_REG_BASE)));
#endif
#if 0
	{
	unsigned int top = TC6393_RAM0_BASE + 0x100;
	volatile unsigned int *ip = (volatile unsigned int *)(top); 
	volatile unsigned short *sp = (volatile unsigned short *)(top+4); 
	volatile unsigned char *cp = (volatile unsigned char *)(top+8); 
	*ip = 0x12345678;
	printk("int %08x\n", *ip);
	sp[0] = 0x9876;
	sp[1] = 0x5432;
	printk("short %04x %04x\n", sp[0], sp[1]);
	cp[0] = 0x12;
	cp[1] = 0x23;
	cp[2] = 0x34;
	cp[3] = 0x45;
	printk("byte: %02x %02x %02x %02x\n", cp[0], cp[1], cp[2], cp[3]);
	}
#endif
}

static int __init tc6393_ohci_init(void)
{
	int ret;

	/*
	 * Request memory resources.
	 */
//	if (!request_mem_region(_USB_OHCI_OP_BASE, _USB_EXTENT, "usb-ohci"))
//		return -EBUSY;

	tc6393_ohci_configure();
	oc_mem_init(TC6393_RAM0_BASE, TC6393_RAM0_SIZE);

	/*
	 * Initialise the generic OHCI driver.
	 */
	ret = hc_add_ohci((struct pci_dev *)1, TC6393_IRQ_USBINT,
			  (void *)(TC6393_SYS_BASE+TC6393_USB_OHCI_OP_REG_BASE),
			  0, &tc6393_ohci, "usb-ohci", "tc6393");

#ifdef CONFIG_PM
	pm_register(PM_SYS_DEV, PM_SYS_UNKNOWN, tc6393_ohci_pm_callback);
#endif

	if (misc_register(&tc6393_ohci_ctl_device))
		printk("failed to register tc6393_ohci_ctl_device\n");

//	if (ret)
//		release_mem_region(_USB_OHCI_OP_BASE, _USB_EXTENT);

	return ret;
}

static void __exit tc6393_ohci_exit(void)
{
	misc_deregister(&tc6393_ohci_ctl_device);

#ifdef CONFIG_PM
	pm_unregister_all(tc6393_ohci_pm_callback);
#endif
	hc_remove_ohci(tc6393_ohci);

	/*
	 * Put the USB host controller into reset.
	 */
	TC6393_SYS_REG(TC6393_SYS_FER) &= ~1;

	/*
	 * Stop the USB clock.
	 */
	TC6393_SYS_REG(TC6393_SYS_CCR) &= ~2;

	/*
	 * Release memory resources.
	 */
//	release_mem_region(_USB_OHCI_OP_BASE, _USB_EXTENT);

}

module_init(tc6393_ohci_init);
module_exit(tc6393_ohci_exit);


#ifdef   CONFIG_PM
static void* tc6393_sram0_backup;
static dma_addr_t tc6393_sram0_backup_phys;

/* usb-ohci.c */
#define OHCI_CONTROL_INIT \
        (OHCI_CTRL_CBSR & 0x3) | OHCI_CTRL_IE | OHCI_CTRL_PLE
extern spinlock_t usb_ed_lock;

extern td_t * dl_reverse_done_list (ohci_t * ohci);
extern void dl_done_list (ohci_t * ohci, td_t * td_list);
extern int hc_start (ohci_t * ohci);
extern int hc_reset (ohci_t * ohci);


/* controller died; cleanup debris, then restart */
/* must not be called from interrupt context */

static void hc_restart (ohci_t *ohci)
{
	int temp;
	int i;

	ohci->disabled = 1;
	ohci->sleeping = 0;
	if (ohci->bus->root_hub)
		usb_disconnect (&ohci->bus->root_hub);
	
	/* empty the interrupt branches */
	for (i = 0; i < NUM_INTS; i++) ohci->ohci_int_load[i] = 0;
	for (i = 0; i < NUM_INTS; i++) ohci->hcca->int_table[i] = 0;
	
	/* no EDs to remove */
	ohci->ed_rm_list [0] = NULL;
	ohci->ed_rm_list [1] = NULL;

	/* empty control and bulk lists */	 
	ohci->ed_isotail     = NULL;
	ohci->ed_controltail = NULL;
	ohci->ed_bulktail    = NULL;

	if ((temp = hc_reset (ohci)) < 0 || (temp = hc_start (ohci)) < 0) {
		err ("can't restart usb-%s, %d", ohci->ohci_dev->slot_name, temp);
	} else
		dbg ("restart usb-%s completed", ohci->ohci_dev->slot_name);
}

static unsigned int tc6393_usb_pm_uspw = 0;

static int tc6393_ohci_pm_suspend(u32 state)
{
	ohci_t *ohci = tc6393_ohci;
	unsigned long flags;

	/* bus suspend */
	if ((ohci->hc_control & OHCI_CTRL_HCFS) != OHCI_USB_OPER) {
		dbg ("can't suspend usb-%s (state is %s)", ohci->slot_name,
			hcfs2string (ohci->hc_control & OHCI_CTRL_HCFS));
		return -EIO;
	}

	/* act as if usb suspend can always be used */
	info ("USB suspend: usb-%s", ohci->slot_name);
	ohci->sleeping = 1;

	/* First stop processing */
  	spin_lock_irqsave (&usb_ed_lock, flags);
	ohci->hc_control &= ~(OHCI_CTRL_PLE|OHCI_CTRL_CLE|OHCI_CTRL_BLE|OHCI_CTRL_IE);
	writel (ohci->hc_control, &ohci->regs->control);
	writel (OHCI_INTR_SF, &ohci->regs->intrstatus);
	(void) readl (&ohci->regs->intrstatus);
  	spin_unlock_irqrestore (&usb_ed_lock, flags);

	/* Wait a frame or two */
	mdelay(1);
	if (!readl (&ohci->regs->intrstatus) & OHCI_INTR_SF)
		mdelay (1);
		
	/* Enable remote wakeup */
	writel (readl(&ohci->regs->intrenable) | OHCI_INTR_RD, &ohci->regs->intrenable);

	/* Suspend chip and let things settle down a bit */
	ohci->hc_control = OHCI_USB_SUSPEND;
	writel (ohci->hc_control, &ohci->regs->control);
#if 0
	(void) readl (&ohci->regs->control);
	mdelay (500); /* No schedule here ! */
	switch (readl (&ohci->regs->control) & OHCI_CTRL_HCFS) {
		case OHCI_USB_RESET:
			dbg("Bus in reset phase ???");
			break;
		case OHCI_USB_RESUME:
			dbg("Bus in resume phase ???");
			break;
		case OHCI_USB_OPER:
			dbg("Bus in operational phase ???");
			break;
		case OHCI_USB_SUSPEND:
			dbg("Bus suspended");
			break;
	}
	tc6393_usb_pm_uspw = TC6393_USB_REG(TC6393_USB_SVPMCS);
#else
	/* Power Down */
	tc6393_usb_pm_uspw = TC6393_USB_REG(TC6393_USB_SVPMCS);
	TC6393_USB_REG(TC6393_USB_SVPMCS) |= TC6393_USB_PM_USPW1;
	TC6393_USB_REG(TC6393_USB_SVPMCS) |= TC6393_USB_PM_USPW2;
#endif
	
	/* store SRAM */
	tc6393_sram0_backup = consistent_alloc(GFP_KERNEL | GFP_DMA,
                        TC6393_RAM0_SIZE, &tc6393_sram0_backup_phys);
	memcpy(tc6393_sram0_backup , TC6393_RAM0_BASE, TC6393_RAM0_SIZE);
//memset(TC6393_RAM0_BASE, 0xff, TC6393_RAM0_SIZE);
	/* device suspend */
	TC6393_SYS_REG(TC6393_SYS_FER) &= ~1;
	TC6393_SYS_REG(TC6393_SYS_CCR) &= ~2;

        return 0;
}
                                                                                
static int tc6393_ohci_pm_resume(void)
{
	ohci_t *ohci = tc6393_ohci;
	int		temp;
	unsigned long	flags;
	int ret = 0;

#ifdef THREAD_RESUME
	  resume_thread_alive = 1;
#endif

	/* device resume */
	tc6393_ohci_configure();

	/* restore SRAM */
	memcpy(TC6393_RAM0_BASE, tc6393_sram0_backup, TC6393_RAM0_SIZE);
	consistent_free(tc6393_sram0_backup,
			TC6393_RAM0_SIZE, tc6393_sram0_backup_phys);

	/* Power On */
	if ( !(tc6393_usb_pm_uspw & TC6393_USB_PM_USPW1) )
		TC6393_USB_REG(TC6393_USB_SVPMCS) &= ~TC6393_USB_PM_USPW1;
	if ( !(tc6393_usb_pm_uspw & TC6393_USB_PM_USPW2) )
		TC6393_USB_REG(TC6393_USB_SVPMCS) &= ~TC6393_USB_PM_USPW2;

	/* bus resume */
	/* guard against multiple resumes */
	atomic_inc (&ohci->resume_count);
	if (atomic_read (&ohci->resume_count) != 1) {
		err ("concurrent resumes for usb-%s", ohci->slot_name);
		atomic_dec (&ohci->resume_count);
#ifdef THREAD_RESUME
		resume_thread_alive = 0;
#endif
		return 0;
	}

	/* did we suspend, or were we powered off? */
	ohci->hc_control = readl (&ohci->regs->control);
	temp = ohci->hc_control & OHCI_CTRL_HCFS;

#ifdef DEBUG
	/* the registers may look crazy here */
	ohci_dump_status (ohci);
#endif

	switch (temp) {

	case OHCI_USB_RESET:	// lost power
		info ("USB restart: usb-%s", ohci->slot_name);
		hc_restart (ohci);
		break;

	case OHCI_USB_SUSPEND:	// host wakeup
	case OHCI_USB_RESUME:	// remote wakeup
		info ("USB continue: usb-%s from %s wakeup", ohci->slot_name,
			(temp == OHCI_USB_SUSPEND)
				? "host" : "remote");
		ohci->hc_control = OHCI_USB_RESUME;
		writel (ohci->hc_control, &ohci->regs->control);
		(void) readl (&ohci->regs->control);
		mdelay (20); /* no schedule here ! */
		/* Some controllers (lucent) need a longer delay here */
		mdelay (15);
		temp = readl (&ohci->regs->control);
		temp = ohci->hc_control & OHCI_CTRL_HCFS;
		if (temp != OHCI_USB_RESUME) {
			err ("controller usb-%s won't resume", ohci->slot_name);
			ohci->disabled = 1;
#ifdef THREAD_RESUME
			resume_thread_alive = 0;
#endif
			return -EIO;
		}

		/* Some chips likes being resumed first */
		writel (OHCI_USB_OPER, &ohci->regs->control);
		(void) readl (&ohci->regs->control);
		mdelay (3);

		/* Then re-enable operations */
		spin_lock_irqsave (&usb_ed_lock, flags);
		ohci->disabled = 0;
		ohci->sleeping = 0;
		ohci->hc_control = OHCI_CONTROL_INIT | OHCI_USB_OPER;
		if (!ohci->ed_rm_list[0] && !ohci->ed_rm_list[1]) {
			if (ohci->ed_controltail)
				ohci->hc_control |= OHCI_CTRL_CLE;
			if (ohci->ed_bulktail)
				ohci->hc_control |= OHCI_CTRL_BLE;
		}
		writel (ohci->hc_control, &ohci->regs->control);
		writel (OHCI_INTR_SF, &ohci->regs->intrstatus);
		writel (OHCI_INTR_SF, &ohci->regs->intrenable);
		/* Check for a pending done list */
		writel (OHCI_INTR_WDH, &ohci->regs->intrdisable);	
		(void) readl (&ohci->regs->intrdisable);
		spin_unlock_irqrestore (&usb_ed_lock, flags);
		if (ohci->hcca->done_head)
			dl_done_list (ohci, dl_reverse_done_list (ohci));
		writel (OHCI_INTR_WDH, &ohci->regs->intrenable); 
		writel (OHCI_BLF, &ohci->regs->cmdstatus); /* start bulk list */
		writel (OHCI_CLF, &ohci->regs->cmdstatus); /* start Control list */
		break;

	default:
		warn ("odd resume for usb-%s", ohci->slot_name);
	}

	/* controller is operational, extra resumes are harmless */
	atomic_dec (&ohci->resume_count);

#ifdef THREAD_RESUME
	resume_thread_alive = 0;
#endif
        return ret;
}

static int
tc6393_ohci_pm_callback(struct pm_dev *pm_device, pm_request_t rqst, void *data)
{
	int error = 0;
                                                                                
	switch (rqst) {
		case PM_SAVE_STATE:
			break;
		case PM_SUSPEND:
#ifdef THREAD_RESUME
			while(resume_thread_alive) udelay(100);
#endif
			error = tc6393_ohci_pm_suspend((u32)data);
			break;
		case PM_RESUME:
#ifdef THREAD_RESUME
			kernel_thread(tc6393_ohci_pm_resume,  NULL, CLONE_FS | CLONE_FILES | CLONE_SIGHAND | SIGCHLD);
			error = 0;
#else
			error = tc6393_ohci_pm_resume();
#endif
			break;
		default:
			break;
        }
        return error;
}
#endif
