/*
 *  linux/include/asm-arm/arch-pxa/keyboard.h
 *
 *  This file contains the architecture specific keyboard definitions
 *
 * ChangLog:
 *	12-Dec-2002 Lineo Japan, Inc.
 *	26-Feb-2004 Lineo Solutions, Inc.  for Tosa
 */

#ifndef _PXA_KEYBOARD_H
#define _PXA_KEYBOARD_H

#include <linux/config.h>
#include <asm/mach-types.h>
#include <asm/hardware.h>

extern struct kbd_ops_struct *kbd_ops;

#if defined(CONFIG_SABINAL_DISCOVERY)
#include <asm/arch/keyboard_discovery.h>
#elif defined(CONFIG_ARCH_PXA_POODLE)
#include <asm/arch/keyboard_poodle.h>
#elif defined(CONFIG_ARCH_PXA_CORGI)
#include <asm/arch/keyboard_corgi.h>
#elif defined(CONFIG_ARCH_PXA_TOSA)
#include <asm/arch/keyboard_tosa.h>
#else

#define kbd_disable_irq()	do { } while(0);
#define kbd_enable_irq()	do { } while(0);

extern int sa1111_kbd_init_hw(void);

static inline void kbd_init_hw(void)
{
	if (machine_is_lubbock())
		sa1111_kbd_init_hw();
}

#endif


#endif  /* _PXA_KEYBOARD_H */

