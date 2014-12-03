#ifndef	__ASM_SH_KEYBOARD_H
#define	__ASM_SH_KEYBOARD_H
/*
 *	$Id: keyboard.h,v 1.12 2001/09/06 04:01:41 gniibe Exp $
 */

#include <linux/kd.h>
#include <linux/config.h>
#include <asm/machvec.h>

#ifdef CONFIG_SH_EC3104
#include <asm/keyboard-ec3104.h>
#else
static __inline__ int kbd_setkeycode(unsigned int scancode,
				     unsigned int keycode)
{
    return -EOPNOTSUPP;
}

static __inline__ int kbd_getkeycode(unsigned int scancode)
{
    return scancode > 127 ? -EINVAL : scancode;
}

#ifdef CONFIG_SH_DREAMCAST
extern int kbd_translate(unsigned char scancode, unsigned char *keycode,
			 char raw_mode);
#else
static __inline__ int kbd_translate(unsigned char scancode,
				    unsigned char *keycode, char raw_mode)
{
    *keycode = scancode;
    return 1;
}
#endif

static __inline__ char kbd_unexpected_up(unsigned char keycode)
{
    return 0200;
}

static __inline__ void kbd_leds(unsigned char leds)
{
}

extern void hp600_kbd_init_hw(void);
extern void dreamcast_kbd_init_hw(void);

#if defined(CONFIG_SH_SOLUTION_ENGINE)
#if defined(CONFIG_PC_KEYB)
#define kbd_init_hw()	pckbd_init_hw()
#else
#define kbd_init_hw()
#endif
#else
static __inline__ void kbd_init_hw(void)
{
	if (MACH_HP600) {
		hp600_kbd_init_hw();
	}
}
#endif

#define kbd_enable_irq()  do {} while(0)
#define kbd_disable_irq() do {} while(0)

#if defined(CONFIG_SH_SOLUTION_ENGINE)

#define KEYBOARD_IRQ	1

#define kbd_request_region()
#define kbd_request_irq(handler)	\
		request_irq(KEYBOARD_IRQ, handler, 0, "keyboard",NULL)

#define kbd_read_status()	inb(KBD_STATUS_REG)
#define kbd_read_input()	inb(KBD_DATA_REG);
#define kbd_write_output(val)	outb(val,KBD_DATA_REG)
#define kbd_write_command(val)	outb(val, KBD_CNTL_REG)

#if defined(CONFIG_CPU_SUBTYPE_SH7751)
#define AUX_IRQ		9
#else
#define AUX_IRQ		12
#endif

#define aux_request_irq(hand, dev_id)	\
		request_irq(AUX_IRQ, hand, SA_SHIRQ, "PS/2 Mouse", dev_id)

#define aux_free_irq(dev_id)	free_irq(AUX_IRQ, dev_id)

#endif	/* CONFIG_SH_SOLUTION_ENGINE */

#endif
#endif
