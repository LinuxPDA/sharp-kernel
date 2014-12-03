
#include <linux/config.h>
#include <linux/init.h>
#include <linux/sched.h>
#include <linux/spinlock.h>
#include <linux/slab.h>
#include <linux/errno.h>

#include <asm/system.h>
#include <asm/irq.h>
#include <asm/hardware.h>
#include <asm/io.h>
#include <asm/dma.h>

static int __init s1c38000_init_dma(void)
{
	return 0;
}

__initcall(s1c38000_init_dma);
