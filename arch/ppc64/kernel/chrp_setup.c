/*
 *  linux/arch/ppc/kernel/setup.c
 *
 *  Copyright (C) 1995  Linus Torvalds
 *  Adapted from 'alpha' version by Gary Thomas
 *  Modified by Cort Dougan (cort@cs.nmt.edu)
 *  Modified by PPC64 Team, IBM Corp
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version
 * 2 of the License, or (at your option) any later version.
 */

/*
 * bootup setup stuff..
 */

#include <linux/config.h>
#include <linux/errno.h>
#include <linux/sched.h>
#include <linux/kernel.h>
#include <linux/mm.h>
#include <linux/stddef.h>
#include <linux/unistd.h>
#include <linux/ptrace.h>
#include <linux/slab.h>
#include <linux/user.h>
#include <linux/a.out.h>
#include <linux/tty.h>
#include <linux/major.h>
#include <linux/interrupt.h>
#include <linux/reboot.h>
#include <linux/init.h>
#include <linux/blk.h>
#include <linux/ioport.h>
#include <linux/console.h>
#include <linux/pci.h>
#include <linux/version.h>
#include <linux/adb.h>
#include <linux/module.h>
#include <linux/delay.h>
#include <linux/ide.h>

#include <linux/irq.h>

#include <asm/mmu.h>
#include <asm/processor.h>
#include <asm/io.h>
#include <asm/pgtable.h>
#include <asm/prom.h>
#include <asm/rtas.h>
#include <asm/pci-bridge.h>
#include <asm/dma.h>
#include <asm/machdep.h>
#include <asm/irq.h>
#include <asm/keyboard.h>
#include <asm/init.h>
#include <asm/Naca.h>

/*#include "time.h" */
#include "local_irq.h"
#include "i8259.h"
#include "open_pic.h"
#include "xics.h"
#include <asm/ppcdebug.h>

extern volatile unsigned char *chrp_int_ack_special;
extern struct Naca *naca;

unsigned long chrp_get_rtc_time(void);
int chrp_set_rtc_time(unsigned long nowtime);
void chrp_calibrate_decr(void);
long chrp_time_init(void);

void chrp_setup_pci_ptrs(void);
void chrp_progress(char *, unsigned short);
void chrp_request_regions(void);

extern int pckbd_setkeycode(unsigned int scancode, unsigned int keycode);
extern int pckbd_getkeycode(unsigned int scancode);
extern int pckbd_translate(unsigned char scancode, unsigned char *keycode,
			   char raw_mode);
extern char pckbd_unexpected_up(unsigned char keycode);
extern void pckbd_leds(unsigned char leds);
extern int pckbd_rate(struct kbd_repeat *rep);
extern void pckbd_init_hw(void);
extern unsigned char pckbd_sysrq_xlate[128];
extern void openpic_init_IRQ(void);

extern void find_and_init_phbs(void);
extern void pSeries_pcibios_fixup(void);
extern void iSeries_pcibios_fixup(void);

kdev_t boot_dev;
unsigned long  virtPython0Facilities = 0;  // python0 facility area (memory mapped io) (64-bit format) VIRTUAL address.

extern HPTE *Hash, *Hash_end;
extern unsigned long Hash_size, Hash_mask;
extern int probingmem;
extern unsigned long loops_per_jiffy;

extern void *comport1;

#ifdef CONFIG_BLK_DEV_RAM
extern int rd_doload;		/* 1 = load ramdisk, 0 = don't load */
extern int rd_prompt;		/* 1 = prompt for ramdisk, 0 = don't prompt */
extern int rd_image_start;	/* starting block # of image */
#endif

int __chrp
chrp_get_cpuinfo(char *buffer)
{
	long len; /* i --Unused */
	/* unsigned int t;  --Unused */
	struct device_node *root;
	const char *model = "";

	root = find_path_device("/");
	if (root)
		model = get_property(root, "model", NULL);
	len = sprintf(buffer,"machine\t\t: CHRP %s\n", model);

	return len;
}

/*
 *  Fixes for the National Semiconductor PC78308VUL SuperI/O
 *
 *  Some versions of Open Firmware incorrectly initialize the IRQ settings
 *  for keyboard and mouse
 */
static inline void __init sio_write(u8 val, u8 index)
{
	outb(index, 0x15c);
	outb(val, 0x15d);
}

static inline u8 __init sio_read(u8 index)
{
	outb(index, 0x15c);
	return inb(0x15d);
}

static void __init sio_fixup_irq(const char *name, u8 device, u8 level,
				     u8 type)
{
	u8 level0, type0, active;
	struct device_node *root;
	
	root = find_path_device("/");
	if (root &&
	    !strncmp(get_property(root, "model", NULL), "IBM,LongTrail", 13 ) )
	{
		/* select logical device */
		sio_write(device, 0x07);
		active = sio_read(0x30);
		level0 = sio_read(0x70);
		type0 = sio_read(0x71);
		printk("sio: %s irq level %d, type %d, %sactive: ", name, level0, type0,
		       !active ? "in" : "");
		if (level0 == level && type0 == type && active)
			printk("OK\n");
		else {
			printk("remapping to level %d, type %d, active\n", level, type);
			sio_write(0x01, 0x30);
			sio_write(level, 0x70);
			sio_write(type, 0x71);
		}
	}

}

void __init chrp_request_regions(void) {
	request_region(0x20,0x20,"pic1");
	request_region(0xa0,0x20,"pic2");
	request_region(0x00,0x20,"dma1");
	request_region(0x40,0x20,"timer");
	request_region(0x80,0x10,"dma page reg");
	request_region(0xc0,0x20,"dma2");
}

void __init
chrp_setup_arch(void)
{
	extern char cmd_line[];
	struct device_node *root;
	unsigned int *opprop;
	
	/* openpic global configuration register (64-bit format). */
	/* openpic Interrupt Source Unit pointer (64-bit format). */
	/* python0 facility area (mmio) (64-bit format) REAL address. */

	/* init to some ~sane value until calibrate_delay() runs */
	loops_per_jiffy = 50000000;

#ifdef CONFIG_BLK_DEV_INITRD
	/* this is fine for chrp */
	initrd_below_start_ok = 1;
	
	if (initrd_start)
		ROOT_DEV = MKDEV(RAMDISK_MAJOR, 0);
	else
#endif
	ROOT_DEV = to_kdev_t(0x0802); /* sda2 (sda1 is for the kernel) */

	printk("Boot arguments: %s\n", cmd_line);

	/* Find and initialize PCI host bridges */
	/* iSeries needs to be done much later. */
 	#ifndef CONFIG_PPC_ISERIES
		find_and_init_phbs();
 	#endif

	/* Find the Open PIC if present */
	root = find_path_device("/");
	opprop = (unsigned int *) get_property(root,
				"platform-open-pic", NULL);
	if (opprop != 0) {
		int n = prom_n_addr_cells(root);
		unsigned long openpic;

		for (openpic = 0; n > 0; --n)
			openpic = (openpic << 32) + *opprop++;
		printk(KERN_DEBUG "OpenPIC addr: %lx\n", openpic);
		udbg_printf("OpenPIC addr: %lx\n", openpic);
		OpenPIC_Addr = ioremap(openpic, 0x40000);
	}

#ifdef CONFIG_DUMMY_CONSOLE
	conswitchp = &dummy_con;
#endif

	/* Get the event scan rate for the rtas so we know how
	 * often it expects a heartbeat. -- Cort
	 */
	if (rtas.event_scan.rate) {
		ppc_md.heartbeat = rtas_event_scan;
		ppc_md.heartbeat_reset = (HZ/rtas.event_scan.rate*60)-1;
		ppc_md.heartbeat_count = 1;
		printk("RTAS Event Scan Rate: %lu (%lu jiffies)\n",
		       rtas.event_scan.rate, ppc_md.heartbeat_reset);
	}
}

void __init
chrp_init2(void)
{
	/*
	 * It is sensitive, when this is called (not too earlu)
	 * -- tibit
	 */
	chrp_request_regions();
	ppc_md.progress(UTS_RELEASE, 0x7777);
}

void __init
chrp_init(unsigned long r3, unsigned long r4, unsigned long r5,
	  unsigned long r6, unsigned long r7)
{
#ifdef CONFIG_BLK_DEV_INITRD
	/* take care of initrd if we have one */
	if ( r6 )
	{
		initrd_start = __va(r6);
		initrd_end = __va(r6 + r7);
	}
#endif /* CONFIG_BLK_DEV_INITRD */

        /* pci_dram_offset/isa_io_base/isa_mem_base set by setup_pci_ptrs() */

	ppc_md.ppc_machine = _machine;

	ppc_md.setup_arch     = chrp_setup_arch;
	ppc_md.setup_residual = NULL;
	ppc_md.get_cpuinfo    = chrp_get_cpuinfo;
	if(naca->interrupt_controller == IC_OPEN_PIC) {
		ppc_md.init_IRQ       = openpic_init_IRQ;
		ppc_md.get_irq        = openpic_get_irq;
		ppc_md.post_irq	      = NULL;
	} else {
		ppc_md.init_IRQ       = xics_init_IRQ;
		ppc_md.get_irq        = xics_get_irq;
		ppc_md.post_irq	      = NULL;
	}

 	#ifndef CONFIG_PPC_ISERIES
 		ppc_md.pcibios_fixup = pSeries_pcibios_fixup;
 	#else 
 		ppc_md.pcibios_fixup = NULL;
 		// ppc_md.pcibios_fixup = iSeries_pcibios_fixup;
 	#endif


	ppc_md.init           = chrp_init2;

	ppc_md.restart        = rtas_restart;
	ppc_md.power_off      = rtas_power_off;
	ppc_md.halt           = rtas_halt;

	ppc_md.time_init      = chrp_time_init;
	ppc_md.set_rtc_time   = chrp_set_rtc_time;
	ppc_md.get_rtc_time   = chrp_get_rtc_time;
	ppc_md.calibrate_decr = chrp_calibrate_decr;

#ifdef CONFIG_VT
	ppc_md.kbd_setkeycode    = pckbd_setkeycode;
	ppc_md.kbd_getkeycode    = pckbd_getkeycode;
	ppc_md.kbd_translate     = pckbd_translate;
	ppc_md.kbd_unexpected_up = pckbd_unexpected_up;
	ppc_md.kbd_leds          = pckbd_leds;
	ppc_md.kbd_init_hw       = pckbd_init_hw;
	ppc_md.kbd_rate          = pckbd_rate;
#ifdef CONFIG_MAGIC_SYSRQ
	ppc_md.ppc_kbd_sysrq_xlate	 = pckbd_sysrq_xlate;
	SYSRQ_KEY = 0x63;	/* Print Screen */
#endif /* CONFIG_MAGIC_SYSRQ */
#endif /* CONFIG_VT */

	ppc_md.progress = chrp_progress;
	
#if defined(CONFIG_BLK_DEV_IDE) || defined(CONFIG_BLK_DEV_IDE_MODULE)
        ppc_ide_md.insw = chrp_ide_insw;
        ppc_ide_md.outsw = chrp_ide_outsw;
        ppc_ide_md.default_irq = chrp_ide_default_irq;
        ppc_ide_md.default_io_base = chrp_ide_default_io_base;
        ppc_ide_md.ide_check_region = chrp_ide_check_region;
        ppc_ide_md.ide_request_region = chrp_ide_request_region;
        ppc_ide_md.ide_release_region = chrp_ide_release_region;
        ppc_ide_md.fix_driveid = chrp_ide_fix_driveid;
        ppc_ide_md.ide_init_hwif = chrp_ide_init_hwif_ports;

        ppc_ide_md.io_base = _IO_BASE;
#endif
	ppc_md.progress("Linux ppc64\n", 0x0);
}

void __chrp
chrp_progress(char *s, unsigned short hex)
{
	struct device_node *root;
	int width, *p;
	char *os;
	static int max_width;

	if ( (_machine != _MACH_chrp) || !rtas.base )
		return;

	if (hex)
		udbg_printf("<chrp_progress> %s\n", s);

	if (max_width == 0) {
		if ( (root = find_path_device("/rtas")) &&
		     (p = (unsigned int *)get_property(root,
						       "ibm,display-line-length",
						       NULL)) )
			max_width = *p;
		else
			max_width = 0x10;
	}

	if ( call_rtas( "display-character", 1, 1, NULL, '\r' ) )
	{
		/* assume no display-character RTAS method - use hex display */
		call_rtas("set-indicator", 3, 1, NULL, 6, 0, hex);
		return;
	}

	width = max_width;
	os = s;
	while ( *os )
	{
		if ( (*os == '\n') || (*os == '\r') )
			width = max_width;
		else
			width--;
		call_rtas( "display-character", 1, 1, NULL, *os++ );
		/* if we overwrite the screen length */
		if ( width == 0 )
			while ( (*os != 0) && (*os != '\n') && (*os != '\r') )
				os++;
	}

	/* Blank to end of line. */
	while ( width-- > 0 )
		call_rtas( "display-character", 1, 1, NULL, ' ' );
}

void chrp_init_map_io_space(void)
{
  /* naca->serialPortAddr is initialized earlier in prom.c */
  comport1 = (void *)ioremap(naca->serialPortAddr, PAGE_SIZE);
}

