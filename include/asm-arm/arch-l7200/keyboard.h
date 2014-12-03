/*
 *  linux/include/asm-arm/arch-l7200/keyboard.h
 *
 *  Keyboard driver definitions for LinkUp Systems L7200 architecture
 *
 *  Copyright (C) 2000 Scott A McConnell (samcconn@cotw.com)
 *                     Steve Hill (sjhill@cotw.com)
 *
 *  This file is subject to the terms and conditions of the GNU General Public
 *  License. See the file COPYING in the main directory of this archive for
 *  more details.
 *
 * Changelog:
 *   07-18-2000	SAM	Created file
 *   07-28-2000	SJH	Complete rewrite
 *   03-04-2001 Lineo Japan, Inc.       Changed for L7205SDB and IRIS.
 *
 */
#ifndef __ASM_ARCH_KEYBOARD_H
#define __ASM_ARCH_KEYBOARD_H

#if defined(CONFIG_IRIS)
#include <asm/arch/keyboard_iris.h>
#else

#include <asm/irq.h>

#define SYSRQ_KEY 13

extern void l7200_kbd_init_hw(void);
extern void l7200_kbd_disable_irq(void);
extern void l7200_kbd_enable_irq(void);
extern int  l7200_kbd_translate(unsigned char, unsigned char*, int);

#define kbd_setkeycode(sc,kc)		(-EINVAL)
#define kbd_getkeycode(sc)		(-EINVAL)

#define kbd_unexpected_up(kc)           (0200)
#define kbd_leds(leds)                  do {} while (0)
#define kbd_init_hw()                   l7200_kbd_init_hw()
#define kbd_sysrq_xlate                 do {} while (0)

#define kbd_disable_irq()               l7200_kbd_disable_irq()
#define kbd_enable_irq()                l7200_kbd_enable_irq()

#define kbd_translate(sc, kcp, rm)      l7200_kbd_translate((sc), (kcp), (rm))

#endif

#endif
