/* -*- linux-c -*- */
/*
 *
 * busops.c: a set of bus operation handle for PCMCIA
 *
 * 2002 by LASER5 Co.,Ltd. <kimai@laser5.co.jp>
 *
 * Time-stamp: <02/08/13 15:10:13 imai>
 *
 */
#include <linux/module.h>
#include <linux/init.h>
#include <linux/config.h>
#include <linux/types.h>
#include <linux/kernel.h>

#include <asm/io.h>
#include <asm/vr41xx/vr41xx.h>

#include <pcmcia/bus_ops.h>

#ifdef CONFIG_TOADKK_TCS8000
#include <asm/vr41xx/toadkk-tcs8000.h>
#endif

#if 1 /*@@@@@*/
#define DEBUG(args...) printk(KERN_ERR args)
#endif

static u32 vr4181a_b_in(void *bus, u32 port, s32 sz)
{
	port = (u32)KSEG1ADDR(VR4181A_PCMCIA_IO_BASE_0 + port);

	DEBUG ( __FUNCTION__ ": port = %08x, sz = %d\n", port, sz );

	switch (sz) {
	case 0:
		return *(volatile u8 *)port;
	case 1:
		return *(volatile u16 *)port;
	case 2:
		return *(volatile u32 *)port;
	default:
		return 0;
	}
}

static void vr4181a_b_ins(void *bus, u32 port, void *buf, u32 count, s32 sz)
{
	port = (u32)KSEG1ADDR(VR4181A_PCMCIA_IO_BASE_0 + port);

	DEBUG ( __FUNCTION__ ": port = %08x, count = %d, sz = %d\n",
		port, count, sz );

	switch (sz) {
	case 0:
		while ( count > 0 ) {
			*(u8 *)buf = *(volatile u8 *)port;
			buf += sizeof(u8);
			port += sizeof(u8);
			count -= sizeof(u8);
		}
		return;
	case 1:
		while ( count > 0 ) {
			*(u16 *)buf = *(volatile u16 *)port;
			buf += sizeof(u16);
			port += sizeof(u16);
			count -= sizeof(u16);
		}
		return;
	case 2:
		while ( count > 0 ) {
			*(u32 *)buf = *(volatile u32 *)port;
			buf += sizeof(u32);
			port += sizeof(u32);
			count -= sizeof(u32);
		}
		return;
	default:
		return;
	}
}



static void vr4181a_b_out(void *bus, u32 val, u32 port, s32 sz)
{
	port = (u32)KSEG1ADDR(VR4181A_PCMCIA_IO_BASE_0 + port);
	
	DEBUG ( __FUNCTION__ ": port = %08x, val = %08x, sz = %d\n",
		port, val, sz );

	switch (sz) {
	case 0:
		*(volatile u8 *)port = val;
		return;
	case 1:
		*(volatile u16 *)port = val;
		return;
	case 2:
		*(volatile u32 *)port = val;
		return;
	default:
		return;
	}
	
}


static void vr4181a_b_outs(void *bus, u32 port, void *buf, u32 count, s32 sz)
{
	port = (u32)KSEG1ADDR(VR4181A_PCMCIA_IO_BASE_0 + port);

	DEBUG ( __FUNCTION__ ": port = %08x, count = %d, sz = %d\n",
		port, count, sz );

	switch (sz) {
	case 0:
		while ( count > 0 ) {
			*(volatile u8 *)port = *(u8 *)buf;
			buf += sizeof(u8);
			port += sizeof(u8);
			count -= sizeof(u8);
		}
		return;
	case 1:
		while ( count > 0 ) {
			*(volatile u16 *)port = *(u16 *)buf;
			buf += sizeof(u16);
			port += sizeof(u16);
			count -= sizeof(u16);
		}
		return;
	case 2:
		while ( count > 0 ) {
			*(volatile u32 *)port = *(u32 *)buf;
			buf += sizeof(u32);
			port += sizeof(u32);
			count -= sizeof(u32);
		}
		return;
	default:
		return;
	}
}



static void *vr4181a_b_ioremap(void *bus, u_long ofs, u_long sz)
{
	//return ioremap (VR4181A_PCMCIA_IO_BASE_0 + ofs, sz);
	return ioremap (VR4181A_PCMCIA_MEM_BASE_0 + ofs, sz);
}


static void vr4181a_b_iounmap(void *bus, void *addr)
{
	iounmap (addr);
}

static u32 vr4181a_b_read(void *bus, void *addr, s32 sz)
{
	//addr += VR4181A_PCMCIA_MEM_BASE_0;

	switch (sz) {
	case 0:
		return *(volatile u8 *)addr;
	case 1:
		return *(volatile u16 *)addr;
	case 2:
		return *(volatile u32 *)addr;
	default:
		return 0;
	}
}

static void vr4181a_b_write(void *bus, u32 val, void *addr, s32 sz)
{
	//addr += VR4181A_PCMCIA_MEM_BASE_0;

	switch (sz) {
	case 0:
		*(volatile u8 *)addr = val;
		return;
	case 1:
		*(volatile u16 *)addr = val;
		return;
	case 2:
		*(volatile u32 *)addr = val;
		return;
	default:
		return;
	}
}

static void vr4181a_b_copy_from(void *bus, void *d, void *s, u32 count)
{
	memcpy_fromio( d, (void *)KSEG1ADDR(VR4181A_PCMCIA_IO_BASE_0 + s),
		       count );
}

static void vr4181a_b_copy_to(void *bus, void *d, void *s, u32 count)
{
	memcpy_toio( (void *)KSEG1ADDR(VR4181A_PCMCIA_IO_BASE_0 + d), s,
		     count );
}



static int vr4181a_b_request_irq(void *bus, u_int irq,
				void (*handler)(int, void *, struct pt_regs *),
				u_long flags, const char *device,
				void *dev_id)
{
	return request_irq ( VR4181A_ECU0_INT,
			     handler, flags, device, dev_id );
}

static void vr4181a_b_free_irq(void *bus, u_int irq, void *dev_id)
{
	free_irq ( VR4181A_ECU0_INT, dev_id );
}

bus_operations vr4181a_bus_ops = {
	priv:		NULL,
	b_in:		vr4181a_b_in,
	b_ins:		vr4181a_b_ins,
	b_out:		vr4181a_b_out,
	b_outs:		vr4181a_b_outs,
	b_ioremap:	vr4181a_b_ioremap,
	b_iounmap:	vr4181a_b_iounmap,
	b_read:		vr4181a_b_read,
	b_write:	vr4181a_b_write,
	b_copy_from:	vr4181a_b_copy_from,
	b_copy_to:	vr4181a_b_copy_to,
	b_request_irq:	vr4181a_b_request_irq,
	b_free_irq:	vr4181a_b_free_irq,
};

EXPORT_SYMBOL(vr4181a_bus_ops);
