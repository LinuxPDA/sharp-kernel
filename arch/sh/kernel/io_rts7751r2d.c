/* 
 * linux/arch/sh/kernel/io_rts7751r2d.c
 *
 * Copyright (C) 2001  Ian da Silva, Jeremy Siegel
 * Based largely on io_se.c.
 *
 * I/O routine for Renesas Technology sales RTS7751R2D.
 *
 * Initial version only to support LAN access; some
 * placeholder code from io_rts7751r2d.c left in with the
 * expectation of later SuperIO and PCMCIA access.
 */

#include <linux/config.h>
#include <linux/kernel.h>
#include <linux/types.h>
#include <asm/io.h>
#include <asm/renesas_rts7751r2d.h>
#include <asm/addrspace.h>

#include <linux/module.h>
#include <linux/pci.h>
#include <asm/pci-sh7751.h>

#if defined(CONFIG_PCMCIA_BUSTYPE)
extern void *area5_io_base;	/* Area 5 16bit I/O Base address */
extern void *area6_io_base;	/* Area 6 16bit I/O Base address */
#endif

/*
 * The 7751R RTS7751R2D uses the built-in PCI controller (PCIC)
 * of the 7751R processor, and has a SuperIO accessible via the PCI.
 * The board also includes a PCMCIA controller on its memory bus,
 * like the other Solution Engine boards.
 */ 

#define PCIIOBR		(volatile long *)PCI_REG(SH7751_PCIIOBR)
#define PCIMBR          (volatile long *)PCI_REG(SH7751_PCIMBR)
#define PCI_IO_AREA	SH7751_PCI_IO_BASE
#define PCI_MEM_AREA	SH7751_PCI_CONFIG_BASE

#define PCI_IOMAP(adr)	(PCI_IO_AREA + (adr & ~SH7751_PCIIOBR_MASK))
#define PCI_MEMMAP(adr)	(PCI_MEM_AREA + (adr & ~SH7751_PCIMBR_MASK))

#define maybebadio(name,port) \
  printk("bad PC-like io %s for port 0x%lx at 0x%08x\n", \
	 #name, (port), (__u32) __builtin_return_address(0))

static inline void delay(void)
{
	ctrl_inw(0xa0000000);
}

static inline unsigned long port2adr(unsigned int port)
{
	if ((0x1f0 <= port && port < 0x1f8) || port == 0x3f6)
		if (port == 0x3f6)
#if defined(CONFIG_PCMCIA_BUSTYPE)
			return ((unsigned long)area5_io_base + 0x0c);
#else
			return (PA_AREA5_IO + 0x80c);
#endif
		else
#if defined(CONFIG_PCMCIA_BUSTYPE)
			return ((unsigned long)area5_io_base + 0x800 + ((port-0x1f0) << 1));
#else
			return (PA_AREA5_IO + 0x1000 + ((port-0x1f0) << 1));
#endif
	else
		maybebadio(port2adr, (unsigned long)port);
	return port;
}

/* The 7751R RTS7751R2D seems to have everything hooked */
/* up pretty normally (nothing on high-bytes only...) so this */
/* shouldn't be needed */
static inline int
shifted_port(unsigned long port)
{
	/* For IDE registers, value is not shifted */
	if ((0x1f0 <= port && port < 0x1f8) || port == 0x3f6)
		return 0;
	else
		return 1;
}

/* In case someone configures the kernel w/o PCI support: in that */
/* scenario, don't ever bother to check for PCI-window addresses */

/* NOTE: WINDOW CHECK MAY BE A BIT OFF, HIGH PCIBIOS_MIN_IO WRAPS? */
#if defined(CONFIG_PCI)
#define CHECK_SH7751_PCIIO(port) \
  ((port >= PCIBIOS_MIN_IO) && (port < (PCIBIOS_MIN_IO + SH7751_PCI_IO_SIZE)))
#else
#define CHECK_SH_7751_PCIIO(port) (0)
#endif

/*
 * General outline: remap really low stuff [eventually] to SuperIO,
 * stuff in PCI IO space (at or above window at pci.h:PCIBIOS_MIN_IO)
 * is mapped through the PCI IO window.  Stuff with high bits (PXSEG)
 * should be way beyond the window, and is used  w/o translation for
 * compatibility.
 */
unsigned char rts7751r2d_inb(unsigned long port)
{
	if (PXSEG(port))
		return *(volatile unsigned char *)port;
	else if (CHECK_SH7751_PCIIO(port) || shifted_port(port))
		return *(volatile unsigned char *)PCI_IOMAP(port);
	else
		return (*(volatile unsigned short *)port2adr(port) & 0xff);
}

unsigned char rts7751r2d_inb_p(unsigned long port)
{
	unsigned char v;

        if (PXSEG(port))
                v = *(volatile unsigned char *)port;
	else if (CHECK_SH7751_PCIIO(port) || shifted_port(port))
                v = *(volatile unsigned char *)PCI_IOMAP(port);
	else
		v = (*(volatile unsigned short *)port2adr(port) & 0xff);
	delay();
	return v;
}

unsigned short rts7751r2d_inw(unsigned long port)
{
        if (PXSEG(port))
                return *(volatile unsigned short *)port;
	else if (CHECK_SH7751_PCIIO(port) || shifted_port(port))
                return *(volatile unsigned short *)PCI_IOMAP(port);
	else
		maybebadio(inw, port);
	return 0;
}

unsigned int rts7751r2d_inl(unsigned long port)
{
        if (PXSEG(port))
                return *(volatile unsigned long *)port;
	else if (CHECK_SH7751_PCIIO(port) || shifted_port(port))
                return *(volatile unsigned long *)PCI_IOMAP(port);
	else
		maybebadio(inl, port);
	return 0;
}

void rts7751r2d_outb(unsigned char value, unsigned long port)
{

        if (PXSEG(port))
                *(volatile unsigned char *)port = value;
	else if (CHECK_SH7751_PCIIO(port) || shifted_port(port))
        	*(unsigned char *)PCI_IOMAP(port) = value;
	else
		*(volatile unsigned short *)port2adr(port) = value;
}

void rts7751r2d_outb_p(unsigned char value, unsigned long port)
{
        if (PXSEG(port))
                *(volatile unsigned char *)port = value;
	else if (CHECK_SH7751_PCIIO(port) || shifted_port(port))
        	*(unsigned char *)PCI_IOMAP(port) = value;
	else
		*(volatile unsigned short *)port2adr(port) = value;
	delay();
}

void rts7751r2d_outw(unsigned short value, unsigned long port)
{
        if (PXSEG(port))
                *(volatile unsigned short *)port = value;
	else if (CHECK_SH7751_PCIIO(port) || shifted_port(port))
        	*(unsigned short *)PCI_IOMAP(port) = value;
	else
		maybebadio(outw, port);
}

void rts7751r2d_outl(unsigned int value, unsigned long port)
{
        if (PXSEG(port))
                *(volatile unsigned long *)port = value;
	else if (CHECK_SH7751_PCIIO(port) || shifted_port(port))
        	*((unsigned long *)PCI_IOMAP(port)) = value;
	else
		maybebadio(outl, port);
}

void rts7751r2d_insb(unsigned long port, void *addr, unsigned long count)
{
	if (PXSEG(port))
		while (count--) *((unsigned char *) addr)++ = *(volatile unsigned char *)port;
	else if (CHECK_SH7751_PCIIO(port) || shifted_port(port)) {
		volatile __u8 *bp = (__u8 *)PCI_IOMAP(port);

		while (count--) *((volatile unsigned char *) addr)++ = *bp;
	} else {
		volatile __u16 *p = (volatile unsigned short *)port2adr(port);

		while (count--) *((unsigned char *) addr)++ = *p & 0xff;
	}
}

void rts7751r2d_insw(unsigned long port, void *addr, unsigned long count)
{
	volatile __u16 *p;

	if (PXSEG(port))
		p = (volatile unsigned short *)port;
	else if (CHECK_SH7751_PCIIO(port) || shifted_port(port))
		p = (volatile unsigned short *)PCI_IOMAP(port);
	else
		p = (volatile unsigned short *)port2adr(port);
	while (count--) *((__u16 *) addr)++ = *p;
}

void rts7751r2d_insl(unsigned long port, void *addr, unsigned long count)
{
	if (CHECK_SH7751_PCIIO(port) || shifted_port(port)) {
		volatile __u32 *p = (__u32 *)PCI_IOMAP(port);

		while (count--) *((__u32 *) addr)++ = *p;
	} else
		maybebadio(insl, port);
}

void rts7751r2d_outsb(unsigned long port, const void *addr, unsigned long count)
{
	if (PXSEG(port))
		while (count--) *(volatile unsigned char *)port = *((unsigned char *) addr)++;
	else if (CHECK_SH7751_PCIIO(port) || shifted_port(port)) {
		volatile __u8 *bp = (__u8 *)PCI_IOMAP(port);

		while (count--) *bp = *((volatile unsigned char *) addr)++;
	} else {
		volatile __u16 *p = (volatile unsigned short *)port2adr(port);

		while (count--) *p = *((unsigned char *) addr)++;
	}
}

void rts7751r2d_outsw(unsigned long port, const void *addr, unsigned long count)
{
	volatile __u16 *p;

	if (PXSEG(port))
		p = (volatile unsigned short *)port;
	else if (CHECK_SH7751_PCIIO(port) || shifted_port(port))
		p = (volatile unsigned short *)PCI_IOMAP(port);
	else
		p = (volatile unsigned short *)port2adr(port);
	while (count--) *p = *((__u16 *) addr)++;
}

void rts7751r2d_outsl(unsigned long port, const void *addr, unsigned long count)
{
	if (CHECK_SH7751_PCIIO(port) || shifted_port(port)) {
		volatile __u32 *p = (__u32 *)PCI_IOMAP(port);

		while (count--) *p = *((__u32 *) addr)++;
	} else
		maybebadio(outsl, port);
}

/* For read/write calls, just copy generic (pass-thru); PCIMBR is  */
/* already set up.  For a larger memory space, these would need to */
/* reset PCIMBR as needed on a per-call basis...                   */

unsigned char rts7751r2d_readb(unsigned long addr)
{
	return *(volatile unsigned char *)addr;
}

unsigned short rts7751r2d_readw(unsigned long addr)
{
	return *(volatile unsigned short *)addr;
}

unsigned int rts7751r2d_readl(unsigned long addr)
{
	return *(volatile unsigned long *)addr;
}

void rts7751r2d_writeb(unsigned char b, unsigned long addr)
{
	*(volatile unsigned char *)addr = b;
}

void rts7751r2d_writew(unsigned short b, unsigned long addr)
{
	*(volatile unsigned short *)addr = b;
}

void rts7751r2d_writel(unsigned int b, unsigned long addr)
{
        *(volatile unsigned long *)addr = b;
}

void *rts7751r2d_ioremap(unsigned long offset, unsigned long size)
{
	if (offset >= 0xfd000000)
		return (void *)offset;
	else
		return (void *)P2SEGADDR(offset);
}
EXPORT_SYMBOL(rts7751r2d_ioremap);

void rts7751r2d_iounmap(void *addr)
{
}
EXPORT_SYMBOL(rts7751r2d_iounmap);

unsigned long rts7751r2d_isa_port2addr(unsigned long offset)
{
	return port2adr(offset);
}
