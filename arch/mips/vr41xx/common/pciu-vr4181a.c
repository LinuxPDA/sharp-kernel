/*
 * FILE NAME
 *	arch/mips/vr41xx/common/pciu.c
 *
 * BRIEF MODULE DESCRIPTION
 *	PCI Control Unit routines for the NEC VR4100 series.
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
 *  Paul Mundt <lethal@chaoticdreams.org>
 *  - Fix deadlock-causing PCIU access race for VR4131.
 *
 *  MontaVista Software Inc. <yyuasa@mvista.com> or <source@mvista.com>
 *  - New creation, NEC VR4122 and VR4131 are supported.
 */
#include <linux/config.h>
#include <linux/init.h>
#include <linux/pci.h>
#include <linux/types.h>
#include <linux/delay.h>

#include <asm/cpu.h>
#include <asm/io.h>
#include <asm/pci_channel.h>
#include <asm/vr41xx/vr41xx.h>
#include <asm/vr41xx/toadkk-tcs8000.h>

#include "pciu.h"

extern unsigned long vr41xx_vtclock;

static u32 config_base = KSEG1ADDR(VR4181A_PCI_MEM_BASE);

#define ADDR_CONFIG	5
#define ADDR_IO		1
#define ADDR_MEM	3

#define CONFIG_BASE	config_base

#if 0
#define TRACE printk(KERN_ERR __FUNCTION__ ": %d\n", __LINE__ )
#else
#define TRACE /**/
#endif

static int map_config(struct pci_dev *dev)
{
	unsigned char bus = dev->bus->number;
	unsigned int dev_fn = dev->devfn;
	unsigned int pciinit;
	int config_type = (bus == 0) ? 0 : 1;

#if 0
	if ( (PCI_SLOT(dev_fn) < 29 || PCI_SLOT(dev_fn) > 31 )
	     | ( (readl(VR4181A_PCIINIT11)>>1)&0x07 == ADDR_CONFIG ) ) {
		/* not exist DEVICE ID */
		/* another PCI window already mapped CONFIG space */
		return -1;
	}
#else
	if ( PCI_SLOT(dev_fn) < 29 || PCI_SLOT(dev_fn) > 31  ) {
		/* not exist DEVICE ID */
		return -1;
	}
#endif
	TRACE;

	
	pciinit = (1<<PCI_SLOT(dev_fn))
		| (config_type<<9)
		| (1<<4)
		| (ADDR_CONFIG<<1);
	//writel ( pciinit, VR4181A_PCIINIT01 );
	writel ( pciinit, VR4181A_PCIINIT11 );

	return 0;
}


static inline void save_pciinit( u32 *reg0, u32 *reg1 )
{
	*reg0 = readl (VR4181A_PCIINIT01);
	*reg1 = readl (VR4181A_PCIINIT11);
}

static inline void restore_pciinit( u32 *reg0, u32 *reg1 )
{
	writel ( *reg0, VR4181A_PCIINIT01); 
	writel ( *reg1, VR4181A_PCIINIT11); 
}


static int vr41xx_pci_read_config_byte(struct pci_dev *dev, int where, u8 *val)
{
	u32 reg0, reg1;
	u32 data;

	TRACE;
	save_pciinit ( &reg0, &reg1 );

	*val = 0xff;
	if (map_config(dev) < 0)
		return PCIBIOS_DEVICE_NOT_FOUND;

	data = readl(CONFIG_BASE+(where & ~3));
	*val = (u8)(data >> ((where & 3)*8));

	restore_pciinit ( &reg0, &reg1 );

	//printk ( KERN_ERR __FUNCTION__ ": val =%08x\n", *val );
	return PCIBIOS_SUCCESSFUL;

}

static int vr41xx_pci_read_config_word(struct pci_dev *dev, int where, u16 *val)
{
	u32 reg0, reg1;
	u32 data;

	TRACE;
	*val = 0xffff;
	if (where & 1)
		return PCIBIOS_BAD_REGISTER_NUMBER;

	save_pciinit ( &reg0, &reg1 );

	if ( map_config(dev) < 0 )
		return PCIBIOS_DEVICE_NOT_FOUND;

	data = readl( CONFIG_BASE + (where & ~3) );
	*val = (u16)(data >> ((where & 2)*8));

	restore_pciinit ( &reg0, &reg1 );

	//printk ( KERN_ERR __FUNCTION__ ": val =%08x\n", *val );
	return PCIBIOS_SUCCESSFUL;
}

static int vr41xx_pci_read_config_dword(struct pci_dev *dev, int where, u32 *val)
{
	u32 reg0, reg1;

	TRACE;
	*val = 0xffffffff;
	if (where & 3)
		return PCIBIOS_BAD_REGISTER_NUMBER;

	save_pciinit ( &reg0, &reg1 );
	if ( map_config(dev) < 0)
		return PCIBIOS_DEVICE_NOT_FOUND;

	*val = readl(CONFIG_BASE + where);

	restore_pciinit ( &reg0, &reg1 );

	//printk ( KERN_ERR __FUNCTION__ ": val =%08x\n", *val );
	return PCIBIOS_SUCCESSFUL;
}

static int vr41xx_pci_write_config_byte(struct pci_dev *dev, int where, u8 val)
{
	u32 reg0, reg1;
	u32 data;
	int shift;

	TRACE;
	save_pciinit ( &reg0, &reg1 );
	if ( map_config(dev) < 0)
		return PCIBIOS_DEVICE_NOT_FOUND;

	data = readl( CONFIG_BASE + (where & ~3) );
	shift = (where & 3)*8;
	data &= ~(0xff << shift);
	data |= (((u32)val) << shift);

	writel(data, CONFIG_BASE + (where & ~3) );

	restore_pciinit ( &reg0, &reg1 );

	TRACE;
	return PCIBIOS_SUCCESSFUL;
}

static int vr41xx_pci_write_config_word(struct pci_dev *dev, int where, u16 val)
{
	u32 reg0, reg1;
	u32 data;
	int shift;

	TRACE;
	if (where & 1)
		return PCIBIOS_BAD_REGISTER_NUMBER;

	save_pciinit ( &reg0, &reg1 );
	if (map_config(dev) < 0)
		return PCIBIOS_DEVICE_NOT_FOUND;

	data = readl( CONFIG_BASE + (where & ~3) );
	shift = (where & 2)*8;
	data &= ~(0xffff << shift);
	data |= (((u32)val) << shift);
	writel(data, CONFIG_BASE + (where & ~3) );

	restore_pciinit ( &reg0, &reg1 );

	TRACE;
	return PCIBIOS_SUCCESSFUL;
}

static int vr41xx_pci_write_config_dword(struct pci_dev *dev, int where, u32 val)
{
	u32 reg0, reg1;

	TRACE;
	if (where & 3)
		return PCIBIOS_BAD_REGISTER_NUMBER;

	save_pciinit ( &reg0, &reg1 );
	if (map_config(dev) < 0)
		return PCIBIOS_DEVICE_NOT_FOUND;

	writel(val, CONFIG_BASE + where );
	restore_pciinit ( &reg0, &reg1 );

	TRACE;
	return PCIBIOS_SUCCESSFUL;
}

struct pci_ops vr41xx_pci_ops = {
	vr41xx_pci_read_config_byte,
	vr41xx_pci_read_config_word,
	vr41xx_pci_read_config_dword,
	vr41xx_pci_write_config_byte,
	vr41xx_pci_write_config_word,
	vr41xx_pci_write_config_dword
};


static inline int set_t_bus_window ( u32 reg, u32 base, u32 mask )
{
	u32 pciw_mask;
	int i = 0;

	TRACE;
	mask &= 0xffe00000;

	if ( mask == 0 ) {
		printk ( KERN_ERR __FUNCTION__ ": invalid address mask.\n" );
		return -1;
	}

	while ( (mask & 1) == 0 ) {
		++i;
		mask >>= 1;
	}

	/*
	 *         i: 21 ... 28 ... 31
	 * pciw_mask: 15 ...  8 ...  5
	 */
	
	pciw_mask = 36 - i;
	writel ( (base & 0xffe00000)
		 | (2<<6) /* 32bit bus width */
		 | pciw_mask,
		 reg );

	return 0;
}


static inline int set_pci_bus_window ( u32 reg, u32 base, u32 mask,
				       u32 space_type )
{
	u32 addr = base & mask & 0xffe00000;

	TRACE;
	writel ( addr | space_type<<1, reg );

	return 0;
}

void __init vr41xx_pciu_init(struct vr41xx_pci_address_map *map)
{
	struct vr41xx_pci_address_space *s;
	u32 config, d;
	int n;

	TRACE;
	if (!map)
		return;

	TRACE;
	/* Disable PCI interrupt */
	//writew(0, MPCIINTREG);
	writew ( 0, VR4181A_MSYSINT2REG );
	writew ( 0, VR4181A_MSYSINT3REG );

	/* Supply VTClock to PCIU */
	d = readw ( VR4181A_CLKDIVCTRL );
	d = (d & ~(3<<8)) | (3<<8);
	writew ( d, VR4181A_CLKDIVCTRL );
	udelay ( 100 );

	d = readw ( VR4181A_PINMODE2 );
	d = (d & ~(3<<14)) | (1<<14);
	writew ( d, VR4181A_PINMODE2 );

	writew ( (1<<4)|(1<<12)|(1<<13), VR4181A_CMUCLKMSK1 );

	writel ( (1<<1)|(1<<3)|(1<<4)|(1<<5), VR4181A_CMUCLKMSK3 );

	writew ( 3, VR4181A_USBSIGCTRL );
	/*
	 * Sleep for 1us after setting MSKPPCIU bit in CMUCLKMSK
	 * before doing any PCIU access to avoid deadlock on VR4131.
	 */
	udelay(1);

	/* do cold and warm reset */
	writel ( (1<<31)|(1<<30), VR4181A_PCICTRL_H );
	//writel ( (1<<31), VR4181A_PCICTRL_H );
	writel ( 0, VR4181A_PCICTRL_H );

	/* dummy read to wait reset done. */
#if 1
 loop:
	while ( readl ( VR4181A_PCICTRL_H ) & (1<<30) ) {
		writel ( 0, VR4181A_PCICTRL_H );
		readw ( VR4181A_NVREG0 );
	}
	readw ( VR4181A_NVREG0 );
	if ( readl ( VR4181A_PCICTRL_H ) & (1<<30) )
		goto loop;
#endif
	readw ( VR4181A_NVREG0 );

#if 1
	printk ( KERN_ERR __FUNCTION__ ": PCICTRL_H = %08x\n",
		 readl ( VR4181A_PCICTRL_H ) );
#endif

#if 1
	//writel ( 0x81000000, VR4181A_PCICTRL_L );
	writel ( 0x00000000, VR4181A_PCICTRL_L );
	writel ( 0, VR4181A_PCIARB );
	//writel ( (1<<16)|(1<<17)|(1<<18)|(1<<20), VR4181A_PCICTRL_H );
#endif

	/*
	 * Set PCI memory & I/O space address conversion registers
	 * for master transaction.
	 */
	if (map->mem1 != NULL) {
		set_t_bus_window ( VR4181A_PCIW0,
				   map->mem1->internal_base,
				   map->mem1->address_mask );
		set_pci_bus_window ( VR4181A_PCIINIT01,
				     map->mem1->pci_base,
				     map->mem1->address_mask,
				     ADDR_MEM );
		//config_base = KSEG1ADDR(map->mem1->internal_base);
	}
	if (map->mem2 != NULL) {
		printk ( KERN_ERR __FUNCTION__
			 ": cannot setup mem2 window.\n" );
	}
	if (map->io != NULL) {
		set_t_bus_window ( VR4181A_PCIW1,
				   map->io->internal_base,
				   map->io->address_mask );
		set_pci_bus_window ( VR4181A_PCIINIT11,
				     map->io->pci_base,
				     map->io->address_mask,
				     ADDR_IO );
		config_base = KSEG1ADDR(map->io->internal_base);
	}
	/*
	 * at this time, config registers of PCI bridge is seen.
	 */

	{
		u32 d;

		/* set and enable memory window */
		d = readl( VR4181A_SDRAM );
		writel ( ( d & 0xfe000000 ) | (0<<3), /* prefetch */
			 VR4181A_BAR_SDRAM );
		writel ( d | (1<<5), VR4181A_SDRAM );

		/* disable another window */
#define disable_win(addr) writel(readl(addr)&~(1<<5), addr)

		disable_win(VR4181A_SFLASH);
		disable_win(VR4181A_PCS0);
		disable_win(VR4181A_PCS1);
		disable_win(VR4181A_PCS2);
		disable_win(VR4181A_PCS3);
		disable_win(VR4181A_PCS4);
		disable_win(VR4181A_ISAW);
		disable_win(VR4181A_INTCS);
		disable_win(VR4181A_ROMCS);
	}

	pciu_write_config_dword(PCI_COMMAND,
	                        PCI_COMMAND_IO |
	                        PCI_COMMAND_MEMORY |
	                        PCI_COMMAND_MASTER |
	                        PCI_COMMAND_PARITY |
	                        PCI_COMMAND_SERR);
}
