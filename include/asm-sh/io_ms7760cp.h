/*
 * include/asm-sh/io_ms7760cp.h
 * Based on include/asm-sh/io_7760se.h
 *
 * May be copied or modified under the terms of the GNU General Public
 * License.  See linux/COPYING for more information.
 *
 * IO functions for an MS7760CP01.
 */

#ifndef __ASM_SH_IO_MS7760CP_H
#define __ASM_SH_IO_MS7760CP_H

#include <asm/io_generic.h>

extern unsigned char ms7760cp_inb(unsigned long port);
extern unsigned short ms7760cp_inw(unsigned long port);
extern unsigned int ms7760cp_inl(unsigned long port);

extern void ms7760cp_outb(unsigned char value, unsigned long port);
extern void ms7760cp_outw(unsigned short value, unsigned long port);
extern void ms7760cp_outl(unsigned int value, unsigned long port);

extern unsigned char ms7760cp_inb_p(unsigned long port);
extern void ms7760cp_outb_p(unsigned char value, unsigned long port);

extern void ms7760cp_insb(unsigned long port, void *addr, unsigned long count);
extern void ms7760cp_insw(unsigned long port, void *addr, unsigned long count);
extern void ms7760cp_insl(unsigned long port, void *addr, unsigned long count);
extern void ms7760cp_outsb(unsigned long port, const void *addr, unsigned long count);
extern void ms7760cp_outsw(unsigned long port, const void *addr, unsigned long count);
extern void ms7760cp_outsl(unsigned long port, const void *addr, unsigned long count);

extern unsigned char ms7760cp_readb(unsigned long addr);
extern unsigned short ms7760cp_readw(unsigned long addr);
extern unsigned int ms7760cp_readl(unsigned long addr);
extern void ms7760cp_writeb(unsigned char b, unsigned long addr);
extern void ms7760cp_writew(unsigned short b, unsigned long addr);
extern void ms7760cp_writel(unsigned int b, unsigned long addr);

extern unsigned long ms7760cp_isa_port2addr(unsigned long offset);

#ifdef __WANT_IO_DEF

# define __inb			ms7760cp_inb
# define __inw			ms7760cp_inw
# define __inl			ms7760cp_inl
# define __outb			ms7760cp_outb
# define __outw			ms7760cp_outw
# define __outl			ms7760cp_outl

# define __inb_p		ms7760cp_inb_p
# define __inw_p		ms7760cp_inw
# define __inl_p		ms7760cp_inl
# define __outb_p		ms7760cp_outb_p
# define __outw_p		ms7760cp_outw
# define __outl_p		ms7760cp_outl

# define __insb			ms7760cp_insb
# define __insw			ms7760cp_insw
# define __insl			ms7760cp_insl
# define __outsb		ms7760cp_outsb
# define __outsw		ms7760cp_outsw
# define __outsl		ms7760cp_outsl

# define __readb		ms7760cp_readb
# define __readw		ms7760cp_readw
# define __readl		ms7760cp_readl
# define __writeb		ms7760cp_writeb
# define __writew		ms7760cp_writew
# define __writel		ms7760cp_writel

# define __isa_port2addr	ms7760cp_isa_port2addr
# define __ioremap		generic_ioremap
# define __iounmap		generic_iounmap

#endif

/*
 * I/O routines to communicate with H8/3048
 */
#define H8_READ		0
#define H8_WRITE	1

#define h8_in(buf, len, reg)	h8_rw(H8_READ, buf, len, reg)
#define h8_out(buf, len, reg)	h8_rw(H8_WRITE, buf, len, reg)

extern int h8_rw(int rw, unsigned char *buf, unsigned int len, unsigned short reg);

#endif /* __ASM_SH_IO_MS7760CP_H */
