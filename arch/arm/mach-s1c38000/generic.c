/*
 * Copyright (c) 2003 Lineo uSolutions, Inc.
 *
 * arch/arm/mach-s1c38000/generic.c
 *
 */
#include <linux/config.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/delay.h>

#include <asm/hardware.h>
#include <asm/system.h>
#include <asm/pgtable.h>
#include <asm/mach/map.h>

static struct map_desc s1c38000_io_desc[] __initdata = {
  LAST_DESC
};

void __init s1c38000_map_io(void)
{
	iotable_init(s1c38000_io_desc);
}

static int __init s1c38000_init(void)
{
	return 0;
}

__initcall(s1c38000_init);
