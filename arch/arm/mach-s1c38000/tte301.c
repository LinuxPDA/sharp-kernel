/*
 * Copyright (c) 2003 Lineo uSolutions, Inc.
 *
 * arch/arm/mach-s1c38000/tte301.c
 *
 */

#include <linux/config.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/major.h>

#include <asm/hardware.h>
#include <asm/setup.h>
#include <asm/irq.h>
#include <asm/io.h>

#include <asm/mach/arch.h>
#include <asm/mach/map.h>

extern void __init s1c38000_init_irq(void);
extern void __init s1c38000_map_io(void);

#define SET_BANK(__nr, __start, __size) \
	mi->bank[__nr].start = (__start), \
	mi->bank[__nr].size  = (__size), \
	mi->bank[__nr].node  = (((unsigned)(__start) - PHYS_OFFSET) >> 27)

static void __init fixup_tte301(struct machine_desc* desc,
				struct param_struct* params,
				char** cmdline,
				struct meminfo* mi)
{
	SET_BANK(0, PHYS_OFFSET, 32*1024*1024);
	mi->nr_banks = 1;

#ifdef CONFIG_BLK_DEV_INITRD
	ROOT_DEV = MKDEV(RAMDISK_MAJOR,0);
	setup_ramdisk(1,  0, 0, 8192);
	setup_initrd(0xc0500000, 3*1024*1024);
#endif
}

static struct map_desc tte301_io_desc[] __initdata = {
 /* virtual       physical      length        domain     r  w  c  b */
  { IO_BASE,      IO_START,     IO_SIZE,      DOMAIN_IO, 0, 1, 0, 0 },
  { IO_BASE_2,    IO_START_2,   IO_SIZE_2,    DOMAIN_IO, 0, 1, 0, 0 },
  { FLASH_BASE,   FLASH_START,  FLASH_SIZE,   DOMAIN_IO, 0, 1, 0, 0 },
  { LCD_IO_BASE,  LCD_IO_START, LCD_IO_SIZE,  DOMAIN_IO, 0, 1, 0, 0 },
  LAST_DESC
};

static void __init tte301_map_io(void)
{
	s1c38000_map_io();
	iotable_init(tte301_io_desc);
}

MACHINE_START(TTE301, "YDC TTE301-STD")
	BOOT_MEM(0x10000000, 0xf8000000, 0xf8000000)
	BOOT_PARAMS(0x10010100)
	FIXUP(fixup_tte301)
	MAPIO(tte301_map_io)
	INITIRQ(s1c38000_init_irq)
MACHINE_END

