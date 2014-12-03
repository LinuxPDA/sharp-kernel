/*
 *  linux/arch/arm/mach-dbmx2/mx2ads.c
 *
 *  Copyright (C) 2003 Lineo Solutions, Inc.
 *
 * Based on:
 *  linux/arch/arm/mach-mx2ads/arch.c
 *
 *  Copyright (C) 2000 Deep Blue Solutions Ltd
 *  Copyright (C) 2002 Shane Nay (shane@minirl.com)
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */
#include <linux/config.h>
#include <linux/types.h>
#include <linux/sched.h>
#include <linux/interrupt.h>
#include <linux/init.h>

#include <asm/hardware.h>
#include <asm/irq.h>
#include <asm/setup.h>
#include <asm/mach-types.h>

#include <asm/mach/arch.h>
#include <asm/mach/map.h>
#include <asm/mach/amba_kmi.h>

#include <asm/arch/mx2.h>

extern void dbmx2_init_irq(void);

static void __init mx2ads_hw_init(void)
{
    _reg_AIPI1_PSR0 = 0x00040304;
    _reg_AIPI2_PSR0 = 0x00000000;
    _reg_AIPI1_PSR1 = 0xfffbfcfb;
    _reg_AIPI2_PSR1 = 0xffffffff;

    /* PLL 133MHz */
    //_reg_CRM_CSCR = 0x17000607;
    _reg_CRM_CSCR = 0x17000a07;
    // PLL non bypass mode
    if (_reg_SYS_FMCR & 0xc0000000)
	_reg_CRM_MPCTL0 = 0x007b1c73; //266M
    //_reg_CRM_CSCR |= 0x00200000;
    _reg_CRM_PCDR = 0x65d93307;

    /* CS Setting */
    _reg_WEIM_CSU(WEIM_CS0) = 0x00002400;
    _reg_WEIM_CSL(WEIM_CS0) = 0x00000e01;
    _reg_WEIM_CSU(WEIM_CS1) = 0x00003e00;
    _reg_WEIM_CSL(WEIM_CS1) = 0x11118501;
    _reg_WEIM_CSU(WEIM_CS3) = 0x00003e00;
    _reg_WEIM_CSL(WEIM_CS3) = 0x11110601;

    /* Select CS3 and CSD0 */
    _reg_SYS_FMCR = 0xffffffc9;

    /* Keep LCDC as the highest priority */
    _reg_MAX_SLV_MPR(3)   = 0x00123056;
    _reg_MAX_SLV_SGPCR(3) = 0;
}

static void __init
mx2ads_fixup(struct machine_desc *desc, struct param_struct *params,
		 char **cmdline, struct meminfo *mi)
{
#ifdef CONFIG_BLK_DEV_INITRD
    ROOT_DEV = MKDEV(RAMDISK_MAJOR,0);
    setup_ramdisk(1,  0, 0, 8192);
    setup_initrd(0xc0500000, 3*1024*1024);
#endif

    if (params->u1.s.page_size != PAGE_SIZE) {
	params->u1.s.page_size = PAGE_SIZE;
	params->u1.s.nr_pages = 64 * 1024 * 1024 / PAGE_SIZE;
	params->u1.s.ramdisk_size = 0;
	params->u1.s.flags = FLAG_READONLY | FLAG_RDLOAD | FLAG_RDPROMPT;
	params->u1.s.rootdev = ROOT_DEV;
	params->u1.s.initrd_start = 0;
	params->u1.s.initrd_size = 0;
	params->u1.s.rd_start = 0;
	params->u1.s.system_rev = 0;
	params->u1.s.system_serial_low = 0;
	params->u1.s.system_serial_high = 0;
	strcpy(params->commandline, CONFIG_CMDLINE);
    } else {
	int len;
	char cmdline[COMMAND_LINE_SIZE];

	if ((len = strlen(CONFIG_CMDLINE)) > 0) {
	    strcpy(cmdline, params->commandline);
	    strcpy(params->commandline, CONFIG_CMDLINE);
	    params->commandline[len] = ' ';
	    cmdline[COMMAND_LINE_SIZE - len - 1] = '\0';
	    strcpy(params->commandline + len + 1, cmdline);
	}
    }
}

static struct map_desc mx2ads_io_desc[] __initdata = {

  { MX2ADS_SDRAM_DISK_IOBASE,       MX2ADS_SDRAM_DISK_BASE,        SZ_16M   , DOMAIN_IO, 0, 1},
#ifdef CONFIG_XIP_KERNEL
  { MX2ADS_BURSTFLASH_IOBASE+0x00400000,       MX2ADS_BURSTFLASH_BASE+0x00400000,        0x01c00000   , DOMAIN_IO, 0, 1},
#else
  { MX2ADS_BURSTFLASH_IOBASE,       MX2ADS_BURSTFLASH_BASE,        SZ_32M   , DOMAIN_IO, 0, 1},
#endif
  { MX2ADS_PER_IOBASE,       	    MX2ADS_PER_BASE,        	   SZ_16M   , DOMAIN_IO, 0, 1},  
  { MX2ADS_IO_IOBASE,  		    	MX2ADS_IO_BASE, 		   	   SZ_1M    , DOMAIN_IO, 0, 1},
  { MX2ADS_CSI_IOBASE,              MX2ADS_CSI_BASE,               SZ_1M    , DOMAIN_IO, 0, 1}, 
  { MX2ADS_EMI_IOBASE,  	    MX2ADS_EMI_BASE, 		   SZ_1M    , DOMAIN_IO, 0, 1},  
  LAST_DESC
};

void __init mx2ads_map_io(void)
{
    iotable_init(mx2ads_io_desc);

    mx2ads_hw_init();
}

MACHINE_START(MX2ADS, "Motorola MX2ADS")
/* 
Now our MX2 physical address is from 0xc0000000, io register is from 0x10000000, 
we map it to 0xe0000000. jimmy
*/

	BOOT_MEM(0xc0000000, 0x10000000, 0xE4000000)
	BOOT_PARAMS(0xc0000100)

	FIXUP(mx2ads_fixup)
	MAPIO(mx2ads_map_io)
	INITIRQ(dbmx2_init_irq)
MACHINE_END
