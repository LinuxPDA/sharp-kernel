/*
 * Copyright (c) 2003 Lineo uSolutions, Inc.
 *
 * include/asm-arm/arch-s1c38000/io.h
 *
 */

#ifndef __ASM_ARM_ARCH_S1C38000_IO_H
#define __ASM_ARM_ARCH_S1C38000_IO_H 1

#define IO_SPACE_LIMIT 0xffffffff

#define __io(a)           (PCIO_BASE + (a))
#define __mem_pci(a)      ((unsigned long)(a))
#define __mem_isa(a)      ((unsigned long)(a))

#define __arch_getw(a)    (*(volatile unsigned short*)(a))
#define __arch_putw(v, a) (*(volatile unsigned short*)(a) = (v))

#define iomem_valid_addr(iomem, sz) (1)
#define iomem_to_phys(iomem)        (iomem)

#endif
