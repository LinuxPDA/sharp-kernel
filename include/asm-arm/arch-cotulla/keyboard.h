#ifndef __ASM_ARM_ARCH_KEYBOARD_H
#define __ASM_ARM_ARCH_KEYBOARD_H

#define NR_SCANCODES 128

#define kbd_setkeycode(sc,kc)		(-EINVAL)
#define kbd_getkeycode(sc)		(-EINVAL)

/* Prototype: int kbd_pretranslate(scancode, raw_mode)
 * Returns  : 0 to ignore scancode
 */
#define kbd_pretranslate(sc,rm)		(1)

/* Prototype: int kbd_translate(scancode, *keycode, *up_flag, raw_mode)
 * Returns  : 0 to ignore scancode, *keycode set to keycode, *up_flag
 *            set to 0200 if scancode indicates release
 */
#define kbd_leds(leds)			do { } while (0)
#define kbd_init_hw()			do { } while (0)
#define kbd_disable_irq()		do { } while (0)
#define kbd_enable_irq()		do { } while (0)

#endif
