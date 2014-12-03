/*
 * linux/include/asm-arm/arch-cotulla/io.h
 *
 *  Copyright (C) 2001  LINEO Japan, inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * Change Log
 *  05-Jun-2002 SHARP Corporation
 *
 */

#ifndef __ASM_ARM_ARCH_IO_H
#define __ASM_ARM_ARCH_IO_H

#define IO_SPACE_LIMIT 0xffffffff

#define __io_pci(a)		(PCIO_BASE + (a))
#define __mem_pci(a)		((unsigned long)(a))
#define __mem_isa(a)		((unsigned long)(a))


/*
 * We have to define our own here b/c XScale has some sideaffects
 * dealing with data aborts that we have to workaround here.
 * Basically after each I/O access, we do a NOP to protect
 * the access from imprecise aborts.  See section 2.3.4.4 of the
 * XScale developer's manual for more info.
 *
 */

/*
#undef __arch_getb
#undef __arch_getl

extern __inline__ unsigned char __arch_getb(unsigned long a)
{
	unsigned int value;
	__asm__ __volatile__("ldr%?b	%0, [%1, #0]	@ getw"
		: "=&r" (value)
		: "r" (a));
	__asm__ __volatile__("mov r0, r0");
	return (unsigned char)value;
}

extern __inline__ unsigned int __arch_getl(unsigned long a)
{
	unsigned int value;
	__asm__ __volatile__("ldr%?	%0, [%1, #0]	@ getw"
		: "=&r" (value)
		: "r" (a));
	__asm__ __volatile__("mov r0, r0");
	return value;
}
*/

extern __inline__ unsigned int __arch_getw(unsigned long a)
{
	unsigned int value;
	__asm__ __volatile__("ldr%?h	%0, [%1, #0]	@ getw"
		: "=&r" (value)
		: "r" (a));
	__asm__ __volatile__("mov r0, r0");
	return value;
}


/*
#undef __arch_putb(v,a)	
#undef __arch_putl(v,a)	

extern __inline__ void __arch_putb(unsigned int value, unsigned long a)
{
	__asm__ __volatile__("str%?b	%0, [%1, #0]	@ putw"
		: : "r" (value), "r" (a));
	__asm__ __volatile__("mov r0, r0");
}

extern __inline__ void __arch_putl(unsigned int value, unsigned long a)
{
	__asm__ __volatile__("str%?	%0, [%1, #0]	@ putw"
		: : "r" (value), "r" (a));
	__asm__ __volatile__("mov r0, r0");
}
*/

extern __inline__ void __arch_putw(unsigned int value, unsigned long a)
{
	__asm__ __volatile__("str%?h	%0, [%1, #0]	@ putw"
		: : "r" (value), "r" (a));
	__asm__ __volatile__("mov r0, r0");
}

#define inb(p)			__arch_getb(__io_pci(p))
#define inw(p)			__arch_getw(__io_pci(p))
#define inl(p)			__arch_getl(__io_pci(p))

#define outb(v,p)		__arch_putb(v,__io_pci(p))
#define outw(v,p)		__arch_putw(v,__io_pci(p))
#define outl(v,p)		__arch_putl(v,__io_pci(p))


#define outsb(p,d,l)		__raw_writesb(__io_pci(p),d,l)
#define outsw(p,d,l)		__raw_writesw(__io_pci(p),d,l)
#define outsl(p,d,l)		__raw_writesl(__io_pci(p),d,l)

#define insb(p,d,l)		__raw_readsb(__io_pci(p),d,l)
#define insw(p,d,l)		__raw_readsw(__io_pci(p),d,l)
#define insl(p,d,l)		__raw_readsl(__io_pci(p),d,l)

#define __arch_ioremap		__ioremap
#define __arch_iounmap		__iounmap

#if defined(CONFIG_SABINAL_DISCOVERY)
#define	IOB8_ADRTRANS(a) (((a)&1)?((unsigned long)(a)+(1<<11)-1):(a))
extern __inline__ unsigned char __arch_getb_cf8bit(unsigned long a)
{
	a = IOB8_ADRTRANS(a);
	return __arch_getb(a);
}
extern __inline__ void __arch_putb_cf8bit(unsigned int v, unsigned long a)
{
	a = IOB8_ADRTRANS(a);
	__arch_putb(v,a);
}
extern __inline__ void __raw_readsb_cf8bit(unsigned long a, void *d, int l)
{
	unsigned char *p=d;
	a = IOB8_ADRTRANS(a);
	while(l--) *p++ = __arch_getb(a);
}
extern __inline__ void __raw_writesb_cf8bit(unsigned long a, void *d, int l)
{
	unsigned char *p=d;
	a = IOB8_ADRTRANS(a);
	while(l--) {__arch_putb(*p,a);p++;}
}
#define	inb_8(p)		__arch_getb_cf8bit(__io_pci(p))
#define	outb_8(v,p)		__arch_putb_cf8bit(v,__io_pci(p))
#define	insb_8(p,d,l)		__raw_readsb_cf8bit(__io_pci(p),d,l)
#define	outsb_8(p,d,l)		__raw_writesb_cf8bit(__io_pci(p),d,l)
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
