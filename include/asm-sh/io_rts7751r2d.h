/*
 * include/asm-sh/io_rts7751r2d.h
 *
 * Modified version of io_se.h for the rts7751r2d-specific functions.
 *
 * May be copied or modified under the terms of the GNU General Public
 * License.  See linux/COPYING for more information.
 *
 * IO functions for an Renesas Technology sales RTS7751R2D
 */

#ifndef _ASM_SH_RENESAS_RTS7751R2D_H
#define _ASM_SH_RENESAS_RTS7751R2D_H

#include <asm/io_generic.h>

extern unsigned char rts7751r2d_inb(unsigned long port);
extern unsigned short rts7751r2d_inw(unsigned long port);
extern unsigned int rts7751r2d_inl(unsigned long port);

extern void rts7751r2d_outb(unsigned char value, unsigned long port);
extern void rts7751r2d_outw(unsigned short value, unsigned long port);
extern void rts7751r2d_outl(unsigned int value, unsigned long port);

extern unsigned char rts7751r2d_inb_p(unsigned long port);
extern void rts7751r2d_outb_p(unsigned char value, unsigned long port);

extern void rts7751r2d_insb(unsigned long port, void *addr, unsigned long count);
extern void rts7751r2d_insw(unsigned long port, void *addr, unsigned long count);
extern void rts7751r2d_insl(unsigned long port, void *addr, unsigned long count);
extern void rts7751r2d_outsb(unsigned long port, const void *addr, unsigned long count);
extern void rts7751r2d_outsw(unsigned long port, const void *addr, unsigned long count);
extern void rts7751r2d_outsl(unsigned long port, const void *addr, unsigned long count);

extern unsigned char rts7751r2d_readb(unsigned long addr);
extern unsigned short rts7751r2d_readw(unsigned long addr);
extern unsigned int rts7751r2d_readl(unsigned long addr);
extern void rts7751r2d_writeb(unsigned char b, unsigned long addr);
extern void rts7751r2d_writew(unsigned short b, unsigned long addr);
extern void rts7751r2d_writel(unsigned int b, unsigned long addr);

extern void *rts7751r2d_ioremap(unsigned long offset, unsigned long size);
extern void rts7751r2d_iounmap(void *addr);

extern unsigned long rts7751r2d_isa_port2addr(unsigned long offset);

#ifdef __WANT_IO_DEF

# define __inb			rts7751r2d_inb
# define __inw			rts7751r2d_inw
# define __inl			rts7751r2d_inl
# define __outb			rts7751r2d_outb
# define __outw			rts7751r2d_outw
# define __outl			rts7751r2d_outl

# define __inb_p		rts7751r2d_inb_p
# define __inw_p		rts7751r2d_inw
# define __inl_p		rts7751r2d_inl
# define __outb_p		rts7751r2d_outb_p
# define __outw_p		rts7751r2d_outw
# define __outl_p		rts7751r2d_outl

# define __insb			rts7751r2d_insb
# define __insw			rts7751r2d_insw
# define __insl			rts7751r2d_insl
# define __outsb		rts7751r2d_outsb
# define __outsw		rts7751r2d_outsw
# define __outsl		rts7751r2d_outsl

# define __readb		rts7751r2d_readb
# define __readw		rts7751r2d_readw
# define __readl		rts7751r2d_readl
# define __writeb		rts7751r2d_writeb
# define __writew		rts7751r2d_writew
# define __writel		rts7751r2d_writel

# define __isa_port2addr	rts7751r2d_isa_port2addr
# define __ioremap		rts7751r2d_ioremap
# define __iounmap		rts7751r2d_iounmap

#endif

#endif /* _ASM_SH_RENESAS_RTS7751R2D_H */
