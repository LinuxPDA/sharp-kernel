/*
 * FILE NAME
 *	arch/mips/vr41xx/nec-eagle/setup.c
 *
 * BRIEF MODULE DESCRIPTION
 *	Setup for the NEC Eagle/Hawk board.
 *
 * Author: Yoichi Yuasa
 *         yyuasa@mvista.com or source@mvista.com
 *
 * Copyright 2001,2002 MontaVista Software Inc.
 *
 *  This program is free software; you can redistribute it and/or modify it
 *  under the terms of the GNU General Public License as published by the
 *  Free Software Foundation; either version 2 of the License, or (at your
 *  option) any later version.
 *
 *  THIS SOFTWARE IS PROVIDED ``AS IS'' AND ANY EXPRESS OR IMPLIED
 *  WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 *  MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 *  IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 *  INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 *  BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS
 *  OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 *  ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR
 *  TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE
 *  USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 *  You should have received a copy of the GNU General Public License along
 *  with this program; if not, write to the Free Software Foundation, Inc.,
 *  675 Mass Ave, Cambridge, MA 02139, USA.
 */
/*
 * Changes:
 *  MontaVista Software Inc. <yyuasa@mvista.com> or <source@mvista.com>
 *  - Moved mips_pci_channels[] from arch/mips/vr41xx/vr4122/eagle/setup.c.
 *  - Added support for NEC Hawk.
 *
 *  MontaVista Software Inc. <yyuasa@mvista.com> or <source@mvista.com>
 *  - New creation, NEC Eagle is supported.
 */
#if 0 /*@@@@@*/
#define TRACE printk(KERN_ERR __FUNCTION__ ": %d\n", __LINE__ )
#else
#define TRACE /**/
#endif

#include <linux/config.h>
#include <linux/init.h>
#include <linux/console.h>
#include <linux/ide.h>
#include <linux/ioport.h>

#include <asm/pci_channel.h>
#include <asm/reboot.h>
#include <asm/time.h>
#include <asm/vr41xx/vr41xx.h>
#include <asm/vr41xx/toadkk-tcs8000.h>

#ifdef CONFIG_BLK_DEV_INITRD
extern unsigned long initrd_start, initrd_end;
extern void * __rd_start, * __rd_end;
#endif

#ifdef CONFIG_BLK_DEV_IDE
extern struct ide_ops vr4181a_ide_ops;
#endif

extern void tcs8000_irq_init(void);

#ifdef CONFIG_PCI
static struct resource vr41xx_pci_io_resource = {
	"PCI I/O space",
	VR4181A_PCI_IO_START,
	VR4181A_PCI_IO_END,
	IORESOURCE_IO,
};

static struct resource vr41xx_pci_mem_resource = {
	"PCI memory space",
	VR4181A_PCI_MEM_START,
	VR4181A_PCI_MEM_END,
	IORESOURCE_MEM,
};

extern struct pci_ops vr41xx_pci_ops;

struct pci_channel mips_pci_channels[] = {
	{&vr41xx_pci_ops, &vr41xx_pci_io_resource, &vr41xx_pci_mem_resource, 0, 256},
	{NULL, NULL, NULL, 0, 0}
};

struct vr41xx_pci_address_space vr41xx_pci_mem1 = {
	internal_base:	VR4181A_PCI_MEM_BASE,
	address_mask:	VR4181A_PCI_MEM_MASK,
	pci_base:	IO_MEM_RESOURCE_START
};

static struct vr41xx_pci_address_space vr41xx_pci_io = {
	VR4181A_PCI_IO_BASE,
	VR4181A_PCI_IO_MASK,
	IO_PORT_RESOURCE_START
};

static struct vr41xx_pci_address_map pci_address_map = {
	&vr41xx_pci_mem1,
	NULL,
	&vr41xx_pci_io
};
#endif

void tcs8000_ether_addr ( u8 *buf )
{
	u8 *bp = (u8 *)0xbfc3fff0;
	int i;

	for ( i = 0; i < 6; ++i ) {
		buf[i] = *bp++;
	}
	if ( buf[0] == 0xff && buf[1] == 0xff && buf[2] == 0xff &&
	     buf[3] == 0xff && buf[4] == 0xff && buf[5] == 0xff ) {
		printk ( KERN_WARNING __FUNCTION__
			 ": not found valid MAC address. use default.\n" );
		buf[0] = 0x00;
		buf[1] = 0xc0;
		buf[2] = 0x9c;
		buf[3] = 0x00;
		buf[4] = 0x10;
		buf[5] = 0xa6;
	}
}


void __init toadkk_tcs8000_setup(void)
{
#if 1 /*@@@@@*/
	TRACE;
#endif

#if 0
	set_io_port_base(IO_PORT_BASE);
#else
	set_io_port_base(0);
#endif

#if 1 /*@@@@@*/
	TRACE;
#endif
	ioport_resource.start = 0;
#if 1 /*@@@@@*/
	TRACE;
#endif
	ioport_resource.end = 0xffffffff;
#if 1 /*@@@@@*/
	TRACE;
#endif
	iomem_resource.start = 0;
#if 1 /*@@@@@*/
	TRACE;
#endif
	iomem_resource.end = 0xffffffff;
#if 1 /*@@@@@*/
	TRACE;
#endif

#ifdef CONFIG_BLK_DEV_INITRD
	ROOT_DEV = MKDEV(RAMDISK_MAJOR, 0);
	initrd_start = (unsigned long)&__rd_start;
	initrd_end = (unsigned long)&__rd_end;
#endif
#if 1 /*@@@@@*/
	TRACE;
#endif

	_machine_restart = vr41xx_restart;
	_machine_halt = vr41xx_halt;
	_machine_power_off = vr41xx_power_off;

	board_time_init = vr41xx_time_init;
	board_timer_setup = vr41xx_timer_setup;

	board_irq_init = tcs8000_irq_init;

#ifdef CONFIG_FB
	conswitchp = &dummy_con;
#endif

#ifdef CONFIG_BLK_DEV_IDE
	ide_ops = &vr4181a_ide_ops;
#endif
#if 1 /*@@@@@*/
	TRACE;
#endif

	vr41xx_bcu_init();
#if 1 /*@@@@@*/
	TRACE;
#endif

	vr41xx_cmu_init(0);
#if 1 /*@@@@@*/
	TRACE;
#endif

#if 1 /*@@@@@*/
	TRACE;
#endif
	/* serial setup */
		
	/*
	 * SIU0
	 * *RTS/GPIO19, *CTS/GPIO18
	 */
	writew ( readw (VR4181A_GPMODE2)&~0x00f0, VR4181A_GPMODE2 );

	/*
	 * SIU1
	 * TxD/GPIO13, RxD/GPIO14, *RTS/GPIO17, *CTS/GPIO15
	 */
	writew ( readw (VR4181A_GPMODE1)&~0xfc00, VR4181A_GPMODE1 );
	writew ( readw (VR4181A_GPMODE2)&~0x000c, VR4181A_GPMODE2 );
	writew ( readw (VR4181A_PINMODE0)|0x0003, VR4181A_PINMODE0 );
	writew ( readw (VR4181A_PINMODE1)|0x0018, VR4181A_PINMODE1 );

	/*
	 * SIU2
	 * TxD/IRDOUT, RxD/IRDIN, *RTS/WS/SYNC, *CTS/SCLK/BITCLK
	 */
	{
		u16 d;

		d = readw (VR4181A_PINMODE2);
		d &= ~0x00ff;
		d |= 0x0055;
		writew ( d, VR4181A_PINMODE2 );
	}

	vr41xx_siu_init(0, 0);
	vr41xx_siu_init(1, 0);
	vr41xx_siu_init(2, 0);
#if 1 /*@@@@@*/
	TRACE;
#endif

#ifdef CONFIG_PCI
	vr41xx_pciu_init(&pci_address_map);
#endif
#if 1 /*@@@@@*/
	TRACE;
#endif

#if 1 /*@@@@@*/
	{
		u32 pcs3 = VR4181A_INTCS_BASE + 0x000028;
		u32 d = 0x0920006f;

		/*
		  d &= ~(3<<6); 
		  d |=  (1<<6); // 16bit
		*/
		writel ( d, pcs3 );
		printk ( KERN_ERR __FUNCTION__ ": pcs3 = %08x\n",
			 d );
	}
	{
		mips_io_port_base = 0;
		isa_slot_offset = 0x10000000;
	}
	{
		u32 pcs3tim = VR4181A_INTCS_BASE + 0x000128;
		u32 d;

		d =
			(0<<1) // CONSET
			| (256<<3) // RDYOUT
			| (2<<15) // CSOFF
			| (0<<17) // RMINWID
			| (7<<19) // BUSIDLE
			| (1<<22) // RDYMODE
			| (1<<23) // RDYSYN
			| (2<<24) // CONOFF
			| (0<<26) // CS_POL
			| (0<<27) // CON_POL
			| (3<<28) //BSTLGH
			;
		d = 0x00c8800A;
		writel ( d, pcs3tim );
		printk ( KERN_ERR __FUNCTION__ ": pcs3tim = %08x\n",
			 d );
	}
#ifdef CONFIG_PCMCIA_VR4181
	{
		writel ( 0x1000006a, VR4181A_ISAW );
		writeb ( 0x00, VR4181A_ECUIDE0 );
	}
#endif
#endif
}
