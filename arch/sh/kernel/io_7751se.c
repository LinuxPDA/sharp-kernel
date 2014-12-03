/* 
 * linux/arch/sh/kernel/io_7751se.c
 *
 * Copyright (C) 2001  Ian da Silva, Jeremy Siegel
 * Copyright (C) 2003  Takashi Kusuda
 * Based largely on io_se.c.
 *
 * Modified to support PCI devices on 7751(R) Solution Engine
 *
 * I/O routine for Hitachi 7751 SolutionEngine.
 *
 * Initial version only to support LAN access; some
 * placeholder code from io_se.c left in with the
 * expectation of later SuperIO and PCMCIA access.
 */

#include <linux/kernel.h>
#include <linux/types.h>
#include <asm/io.h>
#include <asm/hitachi_7751se.h>
#include <asm/addrspace.h>

#include <linux/pci.h>
#include <asm/pci-sh7751.h>

#if 0
/******************************************************************
 * Variables from io_se.c, related to PCMCIA (not PCI); we're not
 * compiling them in, and have removed references from functions
 * which follow.  [Many checked for IO ports in the range bounded
 * by sh_pcic_io_start/stop, and used sh_pcic_io_wbase as offset.
 * As start/stop are uninitialized, only port 0x0 would match?]
 * When used, remember to adjust names to avoid clash with io_se?
 *****************************************************************/
/* SH pcmcia io window base, start and end.  */
int sh_pcic_io_wbase = 0xb8400000;
int sh_pcic_io_start;
int sh_pcic_io_stop;
int sh_pcic_io_type;
int sh_pcic_io_dummy;
/*************************************************************/
#endif

/*
 * The 7751 Solution Engine uses the built-in PCI controller (PCIC)
 * of the 7751 processor, and has a SuperIO accessible via the PCI.
 * The board also includes a PCMCIA controller on its memory bus,
 * like the other Solution Engine boards.
 */ 

#define PCIIOBR		(volatile long *)PCI_REG(SH7751_PCIIOBR)
#define PCIMBR          (volatile long *)PCI_REG(SH7751_PCIMBR)
#define PCI_IO_AREA	SH7751_PCI_IO_BASE
#define PCI_MEM_AREA	SH7751_PCI_CONFIG_BASE

#define PCI_IOMAP(adr)	(PCI_IO_AREA + (adr & ~SH7751_PCIIOBR_MASK))

#define maybebadio(name,port) \
  printk("bad PC-like io %s for port 0x%lx at 0x%08x\n", \
	 #name, (port), (__u32) __builtin_return_address(0))

static inline void delay(void)
{
	ctrl_inw(0xa0000000);
}

static inline volatile __u16 *
port2adr(unsigned int port)
{
	if (port >= 0x2000)
		return (volatile __u16 *) (PA_MRSHPC + (port - 0x2000));
	maybebadio(name,(unsigned long)port);
	return (volatile __u16*)port;
}

/* In case you use Compact Flash as Primary IDE Drive,          */
/* you need check Primary IDE Drive Port(0x1f0-0x1f7 and 0x3f6) */
#if defined(CONFIG_CF_ENABLER)
#define CHECK_CF_PRIMARY_IDE_PORT(port)  \
	((0x1f0 <= port && port <= 0x1f7) || port == 0x3f6)
#else
#define CHECK_CF_PRIMARY_IDE_PORT(port)	(0)
#endif

/* In case someone configures the kernel w/o PCI support: in that */
/* scenario, don't ever bother to check for PCI-window addresses */

/* NOTE: WINDOW CHECK MAY BE A BIT OFF, HIGH PCIBIOS_MIN_IO WRAPS? */
#if defined(CONFIG_PCI)
#define CHECK_SH7751_PCIIO(port) \
  ((port >= PCIBIOS_MIN_IO) && (port < (PCIBIOS_MIN_IO + SH7751_PCI_IO_SIZE)))
#else
#define CHECK_SH7751_PCIIO(port) (0)
#endif

/*
 * General outline: remap really low stuff [eventually] to SuperIO,
 * stuff in PCI IO space (at or above window at pci.h:PCIBIOS_MIN_IO)
 * is mapped through the PCI IO window.  Stuff with high bits (PXSEG)
 * should be way beyond the window, and is used  w/o translation for
 * compatibility.
 */
unsigned char sh7751se_inb(unsigned long port)
{
	if (CHECK_CF_PRIMARY_IDE_PORT(port))
                return *(volatile unsigned char *)(PA_MRSHPC_IO + port + 0x40000);
	else if (PXSEG(port))
		return *(volatile unsigned char *)port;
	else if (CHECK_SH7751_PCIIO(port))
		return *(volatile unsigned char *)PCI_IOMAP(port);
	else
		return (*port2adr(port))&0xff; 
}

unsigned char sh7751se_inb_p(unsigned long port)
{
	unsigned char v;

	if (CHECK_CF_PRIMARY_IDE_PORT(port))
                v = *(volatile unsigned char *)(PA_MRSHPC_IO + port + 0x40000);
       	else if (PXSEG(port))
               	v = *(volatile unsigned char *)port;
	else if (CHECK_SH7751_PCIIO(port))
               	v = *(volatile unsigned char *)PCI_IOMAP(port);
	else
		v = (*port2adr(port))&0xff;
	delay();
	return v;
}

unsigned short sh7751se_inw(unsigned long port)
{
	if (CHECK_CF_PRIMARY_IDE_PORT(port))
                return *(volatile unsigned short *)(PA_MRSHPC_IO + port);
        else if (PXSEG(port))
                return *(volatile unsigned short *)port;
	else if (CHECK_SH7751_PCIIO(port))
                return *(volatile unsigned short *)PCI_IOMAP(port);
	else if (port >= 0x2000)
		return *port2adr(port);
	else
		maybebadio(inw, port);
	return 0;
}

unsigned int sh7751se_inl(unsigned long port)
{
        if (PXSEG(port))
                return *(volatile unsigned long *)port;
	else if (CHECK_SH7751_PCIIO(port))
                return *(volatile unsigned int *)PCI_IOMAP(port);
	else if (port >= 0x2000)
		return *port2adr(port);
	else
		maybebadio(inl, port);
	return 0;
}

void sh7751se_outb(unsigned char value, unsigned long port)
{
	if (CHECK_CF_PRIMARY_IDE_PORT(port))
                *(volatile unsigned char *)(PA_MRSHPC_IO + port + 0x40000) = value;
       	else if (PXSEG(port))
               	*(volatile unsigned char *)port = value;
	else if (CHECK_SH7751_PCIIO(port))
        	*((unsigned char*)PCI_IOMAP(port)) = value;
	else
		*(port2adr(port)) = value;
}

void sh7751se_outb_p(unsigned char value, unsigned long port)
{
	if (CHECK_CF_PRIMARY_IDE_PORT(port))
                *(volatile unsigned char *)(PA_MRSHPC_IO + port + 0x40000) = value;
        else if (PXSEG(port))
               	*(volatile unsigned char *)port = value;
	else if (CHECK_SH7751_PCIIO(port))
        	*((unsigned char*)PCI_IOMAP(port)) = value;
	else
		*(port2adr(port)) = value;
	delay();
}

void sh7751se_outw(unsigned short value, unsigned long port)
{
	if (CHECK_CF_PRIMARY_IDE_PORT(port))
                *(volatile unsigned short *)(PA_MRSHPC_IO + port) = value;
	else if (PXSEG(port))
               	*(volatile unsigned short *)port = value;
	else if (CHECK_SH7751_PCIIO(port))
       		*((unsigned short *)PCI_IOMAP(port)) = value;
	else if (port >= 0x2000)
		*port2adr(port) = value;
	else
		maybebadio(outw, port);
}

void sh7751se_outl(unsigned int value, unsigned long port)
{
        if (PXSEG(port))
                *(volatile unsigned long *)port = value;
	else if (CHECK_SH7751_PCIIO(port))
        	*((unsigned long*)PCI_IOMAP(port)) = value;
	else
		maybebadio(outl, port);
}

void sh7751se_insb(unsigned long port, void *addr, unsigned long count)
{
	unsigned char *p = addr;
	while (count--) *p++ = sh7751se_inb(port);
}

void sh7751se_insw(unsigned long port, void *addr, unsigned long count)
{
	unsigned short *p = addr;
	while (count--) *p++ = sh7751se_inw(port);
}

void sh7751se_insl(unsigned long port, void *addr, unsigned long count)
{
	maybebadio(insl, port);
}

void sh7751se_outsb(unsigned long port, const void *addr, unsigned long count)
{
	unsigned char *p = (unsigned char*)addr;
	while (count--) sh7751se_outb(*p++, port);
}

void sh7751se_outsw(unsigned long port, const void *addr, unsigned long count)
{
	unsigned short *p = (unsigned short*)addr;
	while (count--) sh7751se_outw(*p++, port);
}

void sh7751se_outsl(unsigned long port, const void *addr, unsigned long count)
{
	maybebadio(outsw, port);
}

/* For read/write calls, just copy generic (pass-thru); PCIMBR is  */
/* already set up.  For a larger memory space, these would need to */
/* reset PCIMBR as needed on a per-call basis...                   */

unsigned char sh7751se_readb(unsigned long addr)
{
	return *(volatile unsigned char*)addr;
}

unsigned short sh7751se_readw(unsigned long addr)
{
	return *(volatile unsigned short*)addr;
}

unsigned int sh7751se_readl(unsigned long addr)
{
	return *(volatile unsigned long*)addr;
}

void sh7751se_writeb(unsigned char b, unsigned long addr)
{
	*(volatile unsigned char*)addr = b;
}

void sh7751se_writew(unsigned short b, unsigned long addr)
{
	*(volatile unsigned short*)addr = b;
}

void sh7751se_writel(unsigned int b, unsigned long addr)
{
        *(volatile unsigned long*)addr = b;
}

void * sh7751se_ioremap(unsigned long offset, unsigned long size)
{
        if(offset >= 0xfd000000)
                return (void *)offset;
        else
                return (void *)P2SEGADDR(offset);
}

void sh7751se_iounmap(void *addr)
{
}

/* Map ISA bus address to the real address. Only for PCMCIA.  */

/* ISA page descriptor.  */
static __u32 sh_isa_memmap[256];

unsigned long
sh7751se_isa_port2addr(unsigned long offset)
{
	int idx;

	idx = (offset >> 12) & 0xff;
	offset &= 0xfff;
	return sh_isa_memmap[idx] + offset;
}
