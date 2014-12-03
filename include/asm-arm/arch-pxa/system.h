/*
 * linux/include/asm-arm/arch-pxa/system.h
 *
 * Author:	Nicolas Pitre
 * Created:	Jun 15, 2001
 * Copyright:	MontaVista Software Inc.
 * 
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * Change Log
 *     17-Sep-2002 Lineo Japan, Inc.
 *     26-Feb-2004 Lineo Solutions, Inc.  for Tosa
 */

#include <linux/config.h>
#include "hardware.h"

static inline void arch_idle(void)
{
	if (!hlt_counter) {
		int flags;
		local_irq_save(flags);
		if(!current->need_resched)
			cpu_do_idle(0);
		local_irq_restore(flags);
	}
}


static inline void arch_reset(char mode)
{
#ifdef CONFIG_ARCH_SHARP_SL
#ifdef CONFIG_PM
#if defined(CONFIG_ARCH_PXA_SHEPHERD) || defined(CONFIG_ARCH_PXA_TOSA)
	sharpsl_restart_nonstop();
#else
	sharpsl_restart();
#endif
#endif
#else
	if (mode == 's') {
		/* Jump into ROM at address 0 */
		cpu_reset(0);
	} else {
		/* Initialize the watchdog and let it fire */
#ifdef CONFIG_ARCH_SHARP_SL
		RCSR = 0xf;
#endif
		OWER = OWER_WME;
		OSSR = OSSR_M3;
		OSMR3 = OSCR + 36864;	/* ... in 10 ms */
	}
#endif
}

