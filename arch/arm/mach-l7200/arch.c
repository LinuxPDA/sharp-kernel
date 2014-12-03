/*
 * linux/arch/arm/mach-l7200/arch.c
 *
 * Copyright (C) Lineo, Inc.
 * 
 * May be copied or modified under the terms of the GNU General Public
 * License.  See linux/COPYING for more information.
 *
 *  Changelog:
 *    03-19-2001 Lineo Japan, Inc.
 */
#include <linux/config.h>
#include <linux/tty.h>
#include <linux/delay.h>
#include <linux/pm.h>
#include <linux/init.h>

#include <asm/elf.h>
#include <asm/hardware.h>
#include <asm/setup.h>
#include <asm/mach-types.h>

#include <asm/mach/arch.h>

extern void genarch_init_irq(void);

extern void __init l7200_map_io(void);

static void __init
fixup_l7200(struct machine_desc* desc,
	    struct param_struct* params,
	    char** cmdline,
	    struct meminfo* mi)
{
#if defined(CONFIG_L7205SDB)
        mi->nr_banks      = 1;
        mi->bank[0].start = PHYS_OFFSET;
        mi->bank[0].size  = (32*1024*1024);
        mi->bank[0].node  = 0;

        ROOT_DEV = MKDEV(RAMDISK_MAJOR, 0);
        setup_ramdisk(1, 0, 0, CONFIG_BLK_DEV_RAM_SIZE);
        setup_initrd(__phys_to_virt(0xf1d00000), 0x00300000);
	/* strcpy(*cmdline, "console=ttyLU0,9600"); */

#elif defined(CONFIG_IRIS)
#if defined(CONFIG_IRIS_32MB)
        mi->nr_banks      = 1;
        mi->bank[0].start = PHYS_OFFSET;
        mi->bank[0].size  = (32*1024*1024);
        mi->bank[0].node  = 0;
#else
        mi->nr_banks      = 1;
        mi->bank[0].start = PHYS_OFFSET;
        mi->bank[0].size  = (16*1024*1024);
        mi->bank[0].node  = 0;
#endif
        ROOT_DEV = MKDEV(RAMDISK_MAJOR, 0);
        setup_ramdisk(1, 0, 0, CONFIG_BLK_DEV_RAM_SIZE);
        setup_initrd(__phys_to_virt(0xf0d00000), 0x00300000);
#ifndef CONFIG_NO_FLASH_BOOT_PARAM
	if(1){
	  struct param_struct* pBootParams;
	  pBootParams = (struct param_struct*)__phys_to_virt(0xf0010000);
	  printk("Overriding BootParams by param_struct at %p\n",pBootParams);
	  if( pBootParams->u1.s.initrd_start && pBootParams->u1.s.initrd_size ){
	    printk("initrd_start 0x%lx size 0x%lx\n",pBootParams->u1.s.initrd_start,pBootParams->u1.s.initrd_size);
	    setup_initrd(__phys_to_virt(pBootParams->u1.s.initrd_start),pBootParams->u1.s.initrd_size);
	  }
	  if( pBootParams->u1.s.ramdisk_size ){
	    printk("ramdisk_size 0x%lx\n",pBootParams->u1.s.ramdisk_size);
	    setup_ramdisk(1, 0, 0, pBootParams->u1.s.ramdisk_size);
	  }
	  /* !!! CAUTION !!! default command line is defined by config -> General Setup */
	  if( *(pBootParams->commandline) ){
	    printk("commandline: %s\n",pBootParams->commandline);
	    memcpy( *cmdline , (char*) pBootParams->commandline , COMMAND_LINE_SIZE );
	  }
	  /* !!! temporary change.... !!!! */
	  if( ! strstr(*cmdline,"root=") ){
	    printk("commandline: added %s\n",CONFIG_CMDLINE);
	    strcat(*cmdline," " CONFIG_CMDLINE);
	  }
	  printk("Overriding Done\n");
	}
#else
	strcpy(*cmdline, CONFIG_CMDLINE);
#endif
	/* strcpy(*cmdline, "console=ttyLU0,9600"); */
#else
        mi->nr_banks      = 1;
        mi->bank[0].start = PHYS_OFFSET;
        mi->bank[0].size  = (32*1024*1024);
        mi->bank[0].node  = 0;

        ROOT_DEV = MKDEV(RAMDISK_MAJOR,0);
        setup_ramdisk( 1, 0, 0, CONFIG_BLK_DEV_RAM_SIZE);
        setup_initrd( __phys_to_virt(0xf1000000), 0x005dac7b);

        /* Serial Console COM2 and LCD */
	strcpy( *cmdline, "console=tty0 console=ttyLU1,115200");

        /* Serial Console COM1 and LCD */
	//strcpy( *cmdline, "console=tty0 console=ttyLU0,115200");

        /* Console on LCD */
	//strcpy( *cmdline, "console=tty0");
#endif
}

#if defined(CONFIG_L7205SDB)
MACHINE_START(L7200, "LinkUp Systems L7205SDB")
	MAINTAINER("Steve Hill / Scott McConnell / Lineo, Inc.")
	BOOT_MEM(0xf0000000, 0x80040000, 0xd0000000)
	FIXUP(fixup_l7200)
	MAPIO(l7200_map_io)
	INITIRQ(genarch_init_irq)
MACHINE_END
#elif defined(CONFIG_IRIS)
MACHINE_START(L7200, "Sharp/IRIS")
	MAINTAINER("Steve Hill / Scott McConnell / Sharp Corp. / Lineo, Inc.")
	BOOT_MEM(0xf0000000, 0x80040000, 0xd0000000)
	FIXUP(fixup_l7200)
	MAPIO(l7200_map_io)
	INITIRQ(genarch_init_irq)
MACHINE_END
#else
MACHINE_START(L7200, "LinkUp Systems L7200")
	MAINTAINER("Steve Hill / Scott McConnell")
	MAINTAINER("Steve Hill / Scott McConnell / Lineo, Inc.")
	BOOT_MEM(0xf0000000, 0x80040000, 0xd0000000)
	FIXUP(fixup_l7200)
	MAPIO(l7200_map_io)
	INITIRQ(genarch_init_irq)
MACHINE_END
#endif
