/*
 * linux/include/asm-arm/arch-pxa/io.h
 *
 * Author:	Nicolas Pitre
 * Created:	Jun 15, 2001
 * Copyright:	MontaVista Software Inc.
 * 
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * ChangLog:
 *	12-Dec-2002 Lineo Japan, Inc.
 */
#ifndef __ASM_ARM_ARCH_IO_H
#define __ASM_ARM_ARCH_IO_H

#define IO_SPACE_LIMIT 0xffffffff

/*
 * We don't actually have real ISA nor PCI buses, but there is so many 
 * drivers out there that might just work if we fake them...
 */
#define __io(a)			(a)
#define __mem_pci(a)		((unsigned long)(a))
#define __mem_isa(a)		((unsigned long)(a))

/*
 * Generic virtual read/write
 */
#define __arch_getw(a)		(*(volatile unsigned short *)(a))
#define __arch_putw(v,a)	(*(volatile unsigned short *)(a) = (v))

#define iomem_valid_addr(iomem,sz)	(1)
#define iomem_to_phys(iomem)		(iomem)

#if defined(CONFIG_SABINAL_DISCOVERY)
#define	IOB8_ADRTRANS(a) (((a)&1)?((unsigned long)(a)+(1<<11)-1):(a))
extern __inline__ unsigned char __arch_getb_cf8bit(unsigned long a)
{
	a = IOB8_ADRTRANS(a);
	return __raw_readb(a);
}
extern __inline__ void __arch_putb_cf8bit(unsigned int v, unsigned long a)
{
	a = IOB8_ADRTRANS(a);
	__raw_writeb(v,a);
}
extern __inline__ void __raw_readsb_cf8bit(unsigned long a, void *d, int l)
{
	unsigned char *p=d;
	a = IOB8_ADRTRANS(a);
	while(l--) *p++ = __raw_readb(a);
}
extern __inline__ void __raw_writesb_cf8bit(unsigned long a, void *d, int l)
{
	unsigned char *p=d;
	a = IOB8_ADRTRANS(a);
	while(l--) {__raw_writeb(*p,a);p++;}
}
#define	inb_8(p)		__arch_getb_cf8bit(p)
#define	outb_8(v,p)		__arch_putb_cf8bit(v,p)
#define	insb_8(p,d,l)		__raw_readsb_cf8bit(p,d,l)
#define	outsb_8(p,d,l)		__raw_writesb_cf8bit(p,d,l)
 #ifdef CONFIG_REDEFINE_IO8BIT
  #undef  inb
  #define inb(p)	inb_8(p)
  #undef  outb
  #define outb(v,p)	outb_8(v,p)
  #undef  insb
  #define insb(p,d,l)	insb_8(p,d,l)
  #undef  outsb
  #define outsb(p,d,l)	outsb_8(p,d,l)
 #endif
#endif

#endif
