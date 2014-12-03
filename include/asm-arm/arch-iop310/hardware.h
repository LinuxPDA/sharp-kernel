/*
 * linux/include/asm-arm/arch-iop80310/hardware.h
 */

#ifndef __ASM_ARCH_HARDWARE_H
#define __ASM_ARCH_HARDWARE_H

#include <linux/config.h>

#define PCIO_BASE	0x6e000000

#define pcibios_assign_all_busses() 1

/*
 * these are the values for the secondary PCI bus on the 80312 chip.  I will
 * have to do some fixup in the bus/dev fixup code
 */ 
#define PCIBIOS_MIN_IO      0x90010000
#define PCIBIOS_MIN_MEM     0x88000000

// Generic chipset bits
#include "iop310.h"

// Board specific
#if defined(CONFIG_ARCH_IQ80310)
#include "iq80310.h"
#endif

#endif  /* _ASM_ARCH_HARDWARE_H */
