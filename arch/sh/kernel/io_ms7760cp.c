/* 
 * arch/sh/kernel/io_ms7760cp.c
 * Copyrigjt (c) 2003 Lineo Solutions, Inc. 
 *
 * Based on linux/arch/sh/kernel/io_ms7760cp.c
 * Copyright (C) 2003  Takeo Takahashi
 * Based on io_se.c.
 *
 * I/O routine for MS7760CP01.
 *
 */

#include <linux/kernel.h>
#include <linux/types.h>
#include <asm/io.h>
#include <asm/ms7760cp.h>
#include <asm/addrspace.h>

#if defined(CONFIG_IDE) && defined(__SH4__)
extern void *cf_io_base;
#endif

extern int debug_ms7760cp_flag;
#define maybebadio(name,port) \
  if (debug_ms7760cp_flag != 0) \
	printk("bad PC-like io %s for port 0x%lx at 0x%08x\n", \
	 #name, (port), (__u32) __builtin_return_address(0))

#if defined(CONFIG_SH_7760_SOLUTION_ENGINE_NB)
#define IDE_PCMCIA_IF           1
#define PIO0                    0
#define PIO1                    1
#define PIO2                    2
#define PIO3                    3
#define PIO4                    4
#define PIO_MODE                PIO4
#endif

#if defined(IDE_PCMCIA_IF) && defined(CONFIG_CF_ENABLER)
#define IDE_SECOND_SUPPORT	1	/* IDE secondary */
#endif
#define UART_CHA_SUPPORT	0	/* UART Ch.A */
#define ETH_SECOND_SUPPORT	0

#if 1
#define IDE_NEED_WAIT		0
#else
#define IDE_NEED_WAIT		1
#endif
#define ETH_NEED_WAIT		1

#define IDE_DUMP		0

static inline void delay(void)
{
	(void)ctrl_inw(0xa0000000);
}

#if defined(CONFIG_SH_7760_SOLUTION_ENGINE_NB)
/*
 * ATA access clock when CPU is 200MHz.
 *     t2i(DIOR/DIOW recovery time) - t9(address hold time)
 */
static inline void ide_delay(void)
{
#if (PIO_MODE == PIO0) || (PIO_MODE == PIO1) || \
    (PIO_MODE == PIO2) || (PIO_MODE == PIO3)
		  +       /* t2i:70 - t9:10 = 60ns */
       asm volatile("nop;nop;nop;nop;nop;");
       asm volatile("nop;nop;nop;nop;nop;");
       asm volatile("nop;nop;");
#elif (PIO_MODE == PIO4)
#if 0
 +       /* t2i:25 - t9:10 = 15ns*/
       asm volatile("nop;nop;nop;");
#endif
#else
#error "unknown PIO mode"
#endif
}
#endif

static inline int is_uart(unsigned long port)
{
#if UART_CHA_SUPPORT
	if ((port >= MS7760CP_UART_A) && (port <= (MS7760CP_UART_A + 7)))
		return 1;
#endif
	if ((port >= MS7760CP_UART_B) && (port <= (MS7760CP_UART_B + 7)))
		return 1;
	if ((port >= MS7760CP_UART_C) && (port <= (MS7760CP_UART_C + 7)))
		return 1;
	if ((port >= MS7760CP_UART_D) && (port <= (MS7760CP_UART_D + 7)))
		return 1;
#if UART_CHA_SUPPORT
	if (port >= 0x3f8 && port <= 0x3ff) 	/* Ch.A */
		return 1;
#endif
	if (port >= 0x2f8 && port <= 0x2ff) 	/* Ch.B */
		return 1;
	/* LAN board Ch.A */
	if (port >= ST16_UART_C_BASE && port <= (ST16_UART_C_BASE + 7))
		return 1;
	/* LAN board Ch.B */
	if (port >= ST16_UART_D_BASE && port <= (ST16_UART_D_BASE + 7))
		return 1;
	return 0;
}

static inline int is_ide(unsigned long port)
{
#if IDE_SECOND_SUPPORT
	if ((port >= 0x170 && port <= 0x177) || port == 0x376) 	/* secondary */
		return 2;
#endif
	if ((port >= 0x1f0 && port <= 0x1f7) || port == 0x3f6){	/* primary */
		return 1;
	}
	return 0;
}

static inline int is_eth(unsigned long port)
{
	if (port >= 0x200 && port <= 0x20e) return 1;
#if ETH_SECOND_SUPPORT 
	if (port >= 0x220 && port <= 0x22e) return 1;
#endif
	return 0;
}

static inline int is_noncache_seg(unsigned long port)
{
#if !defined(CONFIG_SH_7760_SOLUTION_ENGINE_NB)
	if ((port >= 0xa8000000) && (port < 0xac000000)) {	
		maybebadio(noncache_seg, port);
		return 0;
	}
	if ((port >= 0xb4000000) && (port < 0xb8000000)) {	
		maybebadio(noncache_seg, port);
		return 0;
	}
#endif
	if ((port >= 0xa0000000) && (port < 0xc0000000)) return 1;
	if (port >= 0xe0000000) return 1;
	return 0;
}

static inline unsigned long port2adr_uart(unsigned long port)
{
	unsigned long offset;

#if UART_CHA_SUPPORT
	if ((port >= MS7760CP_UART_A)&&(port <= (MS7760CP_UART_A + 7))) {
		offset = port & 0x07;
		return (unsigned long)(MS7760CP_UART_A + (offset << 1));
	}
#endif
	if ((port >= MS7760CP_UART_B)&&(port <= (MS7760CP_UART_B + 7))) {
		offset = port & 0x07;
		return (unsigned long)(MS7760CP_UART_B + (offset << 1));
	}
	if ((port >= MS7760CP_UART_C)&&(port <= (MS7760CP_UART_C + 7))) {
		offset = port & 0x07;
		return (unsigned long)(MS7760CP_UART_C + (offset << 1));
	}
	if ((port >= MS7760CP_UART_D)&&(port <= (MS7760CP_UART_D + 7))) {
		offset = port & 0x07;
		return (unsigned long)(MS7760CP_UART_D + (offset << 1));
	}
#if UART_CHA_SUPPORT
	if (port >= 0x3f8 && port <= 0x3ff)
		return (unsigned long)(ST16_UART_A_BASE +
			((port - 0x3f8) << ST16_UART_SHIFT));
#endif
	if (port >= 0x2f8 && port <= 0x2ff)
		return (unsigned long)(ST16_UART_B_BASE +
			((port - 0x2f8) << ST16_UART_SHIFT));
	/* LAN board Ch.A */
	if (port >= ST16_UART_C_BASE && port <= (ST16_UART_C_BASE + 7)) {
		offset = port & 0x7;
		return (unsigned long)(ST16_UART_C_BASE +
			(offset << ST16_UART_SHIFT));
	}
	/* LAN board Ch.B */
	if (port >= ST16_UART_D_BASE && port <= (ST16_UART_D_BASE + 7)) {
		offset = port & 0x7;
		return (unsigned long)(ST16_UART_D_BASE +
			(offset << ST16_UART_SHIFT));
	}
	/* NOT REACHED */
	maybebadio(port2adr_uart, port);
}

static inline unsigned long port2adr_ide(unsigned long port)
{
#if IDE_SECOND_SUPPORT
	/*
	 * IDE extended board is mapped EXT5(CS5 area).
	 * DA0-DA2 is assigned A1-A3, so the registers of ATA
	 * are aligned 2 bytes boundary.
	 */
#if defined(IDE_PCMCIA_IF) && defined(CONFIG_CF_ENABLER) && defined(CONFIG_IDE)
	if (port >= 0x170 && port <= 0x177) {   /* secondary */
	  return (unsigned long)(cf_io_base + ((port - 0x170) << 1));
	} else if (port == 0x376) {             /* secondary */
	  return (unsigned long)(cf_io_base + 0x1c); /* Alternate Status */
	}
#else  /* ! IDE_PCMCIA_IF */
	if (port >= 0x170 && port <= 0x177) {	/* secondary */
		return (unsigned long)(ioremap_nocache(PA_EXT5, 0) +
			 			((port - 0x170) << 1));
	}
	else if (port == 0x376) {		/* secondary */
		return (unsigned long)(ioremap_nocache(PA_EXT5, 0) +
						0x1c /* Alternate Status */);
	}
#endif /* IDE_PCMCIA_IF */
#endif

#if defined(CONFIG_IDE) && defined(__SH4__) && !defined(CONFIG_CF_ENABLER)
	if (port >= 0x1f0 && port <= 0x1f7) { /* primary */
		return (unsigned long)(cf_io_base + ((port - 0x1f0) << 1));
	}else if (port == 0x3f6) {   /* primary */
		return (unsigned long)(cf_io_base + 0x1c); /* Alternate Status */
	}
#else
	return (unsigned long)(PA_MRSHPC_IO + port);
#endif
	/* NOT REACHED */
	maybebadio(port2adr_ide, port);
}

static inline unsigned long port2adr_eth(unsigned long port)
{
	if (port >= 0x200 && port <= 0x20e)
		return (unsigned long)(0xb0000000 + (port - 0x200)) & ~1;
#if ETH_SECOND_SUPPORT
#error "base address should be fixed"
	if (port >= 0x220 && port <= 0x22e)
		return (unsigned long)(0xb0000000 + (port - 0x220)) & ~1;
#endif
	/* NOT REACHED */
	maybebadio(port2adr_eth, port);
}

unsigned char ms7760cp_inb(unsigned long port)
{
	if ((port >= PA_MRSHPC_MW1)&&(port < PA_MRSHPC_MW1 + 0x00400000))
		return *(volatile unsigned char*)(port + 0x00040000);

	if (is_ide(port)) {
#if IDE_DUMP
		unsigned char v;
		v = (*(volatile unsigned char *)port2adr_ide(port) & 0xff);
		if (is_ide(port) == 2 && debug_ms7760cp_flag) {
			printk("%s: port(%lx)=0x%02x\n", __func__, port, v);
		}
		else if (port != 0x1f0 && port != 0x1f7 && debug_ms7760cp_flag) {
			printk("%s: port(%lx)=0x%02x\n", __func__, port, v);
		}
		return v;
#else
		return (unsigned char)(*(volatile unsigned char *)port2adr_ide(port) & 0xff);
#endif
	}
	if (is_eth(port)) {
		unsigned short v = ms7760cp_inw(port);
		if (port & 1) return (unsigned char)((v >> 8) & 0xff);
		return (unsigned char)(v & 0xff);
	}
	if (is_uart(port)) {
		return (unsigned char)(*(volatile unsigned short *)port2adr_uart(port) & 0xff);
	}
	if (is_noncache_seg(port)) {
		return *(volatile unsigned char *)port;
	}
	maybebadio(ms7760cp_inb, port);
	return 0;
}

unsigned char ms7760cp_inb_p(unsigned long port)
{
	unsigned char v;

	v = ms7760cp_inb(port);
	delay();
	return v;
}

unsigned short ms7760cp_inw(unsigned long port)
{
	if (is_ide(port)) {
		unsigned short v;
#if IDE_DUMP
		v = *(volatile unsigned short *)port2adr_ide(port);
		if (is_ide(port) == 2 && debug_ms7760cp_flag) {
			printk("%s: port(%lx)=0x%04x\n", __func__, port, v);
		}
		else if (port != 0x1f0 && port != 0x1f7 && debug_ms7760cp_flag) {
			printk("%s: port(%lx)=0x%04x\n", __func__, port, v);
		}
		return v;
#else
		v = *(volatile unsigned short *)port2adr_ide(port);
		return v;
#endif
	}
	if (is_eth(port)) {
		volatile unsigned short *p =
		  (unsigned short*)port2adr_eth(port);
#if ETH_NEED_WAIT
		delay();
#endif
		return *p;
	}
	if (is_noncache_seg(port)) {
		return *(volatile unsigned short *)port;
	}
	maybebadio(ms7760cp_inw, port);
	return 0;
}

unsigned int ms7760cp_inl(unsigned long port)
{
	if (is_noncache_seg(port))
		return *(volatile unsigned int *)port;
	maybebadio(ms7760cp_inl, port);
	return 0;
}

void ms7760cp_outb(unsigned char value, unsigned long port)
{
	if (is_ide(port)) {
#if IDE_DUMP
		if (debug_ms7760cp_flag)
			printk("%s: port(%lx) = %x\n", port2adr_ide(port), __func__, value);
#endif
		*(volatile unsigned char *)port2adr_ide(port) = value;
	}
	else if (is_eth(port)) {
		unsigned short v = ms7760cp_inw(port);
		if (port & 1) v = (v & 0x00ff) | ((unsigned short)value << 8);
		else v = (v & 0xff00) | value;
		*(volatile unsigned short *)port2adr_eth(port) = v;
#if ETH_NEED_WAIT
		delay();
#endif
	}
	else if (is_uart(port)) {
		*(volatile unsigned short*)port2adr_uart(port) =
		  (unsigned char)value;
	}
	else if (is_noncache_seg(port)) {
		*(volatile unsigned char *)port = value;
	}
	else {
		maybebadio(ms7760cp_outb, port);
	}
}

void ms7760cp_outb_p(unsigned char value, unsigned long port)
{
	ms7760cp_outb(value, port);
	delay();
}

void ms7760cp_outw(unsigned short value, unsigned long port)
{
	if (is_ide(port)) {
		*(volatile unsigned short *)port2adr_ide(port) = value;
	}
	else if (is_eth(port)) {
		volatile unsigned short *p =
		  (unsigned short*)port2adr_eth(port);
#if ETH_NEED_WAIT
		delay();
#endif
		*p = value;
	}
	else if (is_noncache_seg(port)) {
		*(volatile unsigned short *)port = value;
	}
	else {
		maybebadio(ms7760cp_outw, port);
	}
}

void ms7760cp_outl(unsigned int value, unsigned long port)
{
	if (is_noncache_seg(port)) {
		*(volatile unsigned int *)port = value;
	}
	else {
		maybebadio(ms7760cp_inl, port);
	}
}

void ms7760cp_insb(unsigned long port, void *addr, unsigned long count)
{
	unsigned char *p = (unsigned char *)addr;

	while (count--) *p++ = ms7760cp_inb(port);
}

void ms7760cp_insw(unsigned long port, void *addr, unsigned long count)
{
#if IDE_NEED_WAIT
        unsigned short *p = (unsigned short *)addr;
        while (count--) *p++ = ms7760cp_inw(port);
#else
	unsigned short *p = (unsigned short *)addr;
	volatile unsigned short *v;

	if (is_ide(port)) {
		v = (volatile unsigned short *)(port2adr_ide(port));
		while (count--) {
			*p++ = *v;
		}
	}
	else if (is_eth(port)) {
		v = (volatile unsigned short *)(port2adr_eth(port));
		while (count--) {
			*p++ = *v;
#if ETH_NEED_WAIT
			delay();
#endif
		}
	} 
	else if (is_noncache_seg(port)) {
		v = (volatile unsigned short *)port;
		while (count--) {
			*p++ = *v;
		}
	}
	else {
		maybebadio(ms7760cp_insw, port);
	}
#endif
}

void ms7760cp_insl(unsigned long port, void *addr, unsigned long count)
{
	maybebadio(insl, port);
}

void ms7760cp_outsb(unsigned long port, const void *addr, unsigned long count)
{
	unsigned char *p = (unsigned char *)addr;
	while (count--) ms7760cp_outb(*p++, port);
}

void ms7760cp_outsw(unsigned long port, const void *addr, unsigned long count)
{
#if IDE_NEED_WAIT
        unsigned short *p = (unsigned short *)addr;
        while (count--) ms7760cp_outw(*p++, port);
#else
	unsigned short *p = (unsigned short *)addr;
	volatile unsigned short *v;

	if (is_ide(port)) {
		v = (volatile unsigned short *)port2adr_ide(port);
		while (count--) {
			*v = *p++;
		}
	}
	else if (is_eth(port)) {
		v = (volatile unsigned short *)port2adr_eth(port);
		while (count--) {
			*v = *p++;
#if ETH_NEED_WAIT
			delay();
#endif
		}
	}
	else if (is_noncache_seg(port)) {
		v = (volatile unsigned short *)port;
		while (count--) {
			*v = *p++;
		}
	}
	else {
		maybebadio(ms7760cp_outw, port);
	}
#endif
}

void ms7760cp_outsl(unsigned long port, const void *addr, unsigned long count)
{
	maybebadio(outsw, port);
}

unsigned char ms7760cp_readb(unsigned long addr)
{
	if ((addr >= 0xb8400000)&&(addr < 0xb8400000 + 0x00400000))
		return *(volatile unsigned char*)(addr + 0x00040000);

	return *(volatile unsigned char *)addr;
}

unsigned short ms7760cp_readw(unsigned long addr)
{
	return *(volatile unsigned short *)addr;
}

unsigned int ms7760cp_readl(unsigned long addr)
{
	return *(volatile unsigned long *)addr;
}

void ms7760cp_writeb(unsigned char b, unsigned long addr)
{
	*(volatile unsigned char *)addr = b;
}

void ms7760cp_writew(unsigned short b, unsigned long addr)
{
	*(volatile unsigned short *)addr = b;
}

void ms7760cp_writel(unsigned int b, unsigned long addr)
{
        *(volatile unsigned int *)addr = b;
}

/* Map ISA bus address to the real address. Only for PCMCIA.  */

/* ISA page descriptor.  */
static __u32 sh_isa_memmap[256];

#if 0
static int sh_isa_mmap(__u32 start, __u32 length, __u32 offset)
{
	int idx;

	if (start >= 0x100000 || (start & 0xfff) || (length != 0x1000))
		return -1;

	idx = start >> 12;
	sh_isa_memmap[idx] = 0xb8000000 + (offset &~ 0xfff);
	printk("sh_isa_mmap: start %x len %x offset %x (idx %x paddr %x)\n",
	       start, length, offset, idx, sh_isa_memmap[idx]);
	return 0;
}
#endif

unsigned long ms7760cp_isa_port2addr(unsigned long offset)
{
	int idx;

	idx = (offset >> 12) & 0xff;
	offset &= 0xfff;
	return sh_isa_memmap[idx] + offset;
}

/*
 * I/O routines to communicate with H8/3048
 */
static __inline__ void h8_outb(unsigned char ch)
{
	volatile unsigned short status;
	int timeout;

	/* wait for buffer empty */
	timeout = 0x1000000;
	while (1) {
		status = inw_p(ST16_UART_A_BASE + ST16_LSR);
		if (status & 0x0020) break;
		timeout--;
		if (timeout <= 0) {
			printk("h8_outb timeout\n");
			break;
		}
	}

	if (status & 0x000e) {
		printk("%s: serial error (0x%02x)\n", __func__, status & 0xff);
		(void)inw_p(ST16_UART_A_BASE + ST16_LSR);
	}
#if DEBUG
	printk("%s: ch = 0x%02x\n", __func__, ch);
#endif
	outw_p((unsigned short)ch, ST16_UART_A_BASE + ST16_THR);
}

static __inline__ unsigned char h8_inb(void)
{
	unsigned char ch;
	volatile unsigned short status;
	int timeout;

	/* wait for receive data ready */
	timeout = 0x1000000;
	while (1) {
		status = inw_p(ST16_UART_A_BASE + ST16_LSR);
		if (status & 0x0001) break;
		timeout--;
		if (timeout <= 0) {
			printk("h8_inb timeout\n");
			break;
		}
	}

	if (status & 0x000e) {
		printk("%s: serial error (0x%02x)\n", __func__, status & 0xff);
		(void)inw_p(ST16_UART_A_BASE + ST16_LSR);
	}

	ch = (char)inw_p(ST16_UART_A_BASE + ST16_RHR);
#if DEBUG
	printk("%s: ch = 0x%02x\n", __func__, ch);
#endif
	return ch;
}

int h8_rw(int rw, unsigned char *buf, unsigned int len, unsigned short reg)
{
	unsigned char ch, fn;
	unsigned int i;
	unsigned char *p;
	unsigned long flags;

	if (len > 0xfff) {
		printk("%s:data length too long\n", __func__);
		return -1;
	}
	
	fn = ((rw == H8_READ)? 0x00: 0x40);

	save_flags(flags); cli();
again:
	/* start code */
	h8_outb(0x02);

	/* function code */
	if (len > 0xf) {
		h8_outb((unsigned char)(0x90 | fn | len >> 8));
		h8_outb((unsigned char)len);
	}
	else {
		h8_outb((unsigned char)(0x80 | fn | len));
	}

	/* register address */
	h8_outb((unsigned char)(reg >> 8));
	h8_outb((unsigned char)reg);

	if (rw == H8_WRITE) {
		p = buf;
		for (i = 0; i < len; i++) {
			h8_outb(*p);
			p++;
		}
	}

	/* ACK/NAK */
	ch = h8_inb();
	if (ch == 0x15) {
		ch = h8_inb();
		printk("%s: Received NAK (%02x)\n", __func__, ch);
		goto again;
	}

	if (ch != 0x06) {
		printk("%s: Received invalid ACK code(0x%02x)\n",
		       __func__, ch);
		goto again;
	}

	/* receive function code */
	if (len > 0xf) {	/* 2 bytes */
		/* ignore function code */
		(void)h8_inb();
		(void)h8_inb();
		/* ignore register address */
		(void)h8_inb();
		(void)h8_inb();
	}
	else {		/* 1 bytes */
		/* ignore function code */
		(void)h8_inb();
		/* ignore register address */
		(void)h8_inb();
		(void)h8_inb();
	}

	if (rw == H8_READ) {
		p = buf;
		for (i = 0; i < len; i++) {
			*p = h8_inb();
			p++;
		}
	}
	else if ((reg != SH7760SE_IR_IRRSFDR) &&
		 !((reg >= SH7760SE_EE_EEPDR) && 
		   (reg <= (SH7760SE_EE_EEPDR + SH7760SE_EE_EEPDR_SIZE)))) {
		for (i = 0; i < len; i++) {
			(void)h8_inb();
		}
	}

	restore_flags(flags);

	return len;
}
