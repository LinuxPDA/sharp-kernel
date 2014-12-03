/*
 * linux/include/asm-armnommu/arch-dsc21/io.h
 *
 * Copyright (C) 1997-1999 Russell King
 *
 * Modifications:
 *  06-12-1997	RMK	Created.
 *  07-04-1999	RMK	Major cleanup
 *  02-19-2001  gjm     Leveraged for armnommu/dsc21
 */
#ifndef __ASM_ARM_ARCH_IO_H
#define __ASM_ARM_ARCH_IO_H

/*
 * kernel/resource.c uses this to initialize the global ioport_resource struct
 * which is used in all calls to request_resource(), allocate_resource(), etc.
 * --gmcnutt
 */
#define IO_SPACE_LIMIT 0xffffffff

/*
 * If we define __io then asm/io.h will take care of most of the inb & friends
 * macros. It still leaves us some 16bit macros to deal with ourselves, though.
 * We don't have PCI or ISA on the dsc21 so I dropped __mem_pci & __mem_isa.
 * --gmcnutt
 */
#define __io(a) (CONFIG_IO16_BASE + (a))
#define __iob(a) (CONFIG_IO8_BASE + (a))	// byte io address

#define __arch_getw(a) (*(volatile unsigned short *)(a))
#define __arch_putw(v,a) (*(volatile unsigned short *)(a) = (v))

/*
 * Defining these two gives us ioremap for free. See asm/io.h.
 * --gmcnutt
 */
#define iomem_valid_addr(iomem,sz) (1)
#define iomem_to_phys(iomem) (iomem)


/*
 * These functions are needed for mtd/maps/physmap.c
 * --rp
 */
#ifndef memset_io
#define memset_io(a,b,c)                _memset_io((a),(b),(c))
#endif
#ifndef memcpy_fromio
#define memcpy_fromio(a,b,c)            _memcpy_fromio((a),(b),(c))
#endif
#ifndef memcpy_toio
#define memcpy_toio(a,b,c)              _memcpy_toio((a),(b),(c))
#endif

/** needed for PCMCIA .... */

u8  __readb(void *addr);
u16 __readw(void *addr);
u32 __readl(void *addr);

#define readb(b)                __readb(b)
#define readw(b)                __readw(b)
#define readl(b)                __readl(b)

void __writeb(u8  val, void *addr);
void __writew(u16 val, void *addr);
void __writel(u32 val, void *addr);

#define writeb(v,b)             __writeb(v,b)
#define writew(v,b)             __writew(v,b)
#define writel(v,b)             __writel(v,b)

#endif
