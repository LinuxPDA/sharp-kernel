#ifndef __PPC64_PCI_H
#define __PPC64_PCI_H
#ifdef __KERNEL__

/*
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version
 * 2 of the License, or (at your option) any later version.
 */

/* Values for the `which' argument to sys_pciconfig_iobase syscall.  */
#define IOBASE_BRIDGE_NUMBER	0
#define IOBASE_MEMORY		1
#define IOBASE_IO		2
#define IOBASE_ISA_IO           3
#define IOBASE_ISA_MEM          4

/* Can be used to override the logic in pci_scan_bus for skipping
 * already-configured bus numbers - to be used for buggy BIOSes
 * or architectures with incomplete PCI setup by the loader.
 */
extern int pcibios_assign_all_busses(void);

#define PCIBIOS_MIN_IO		0x1000
#define PCIBIOS_MIN_MEM		0x10000000

extern inline void pcibios_set_master(struct pci_dev *dev)
{
	/* No special bus mastering setup handling */
}

extern inline void pcibios_penalize_isa_irq(int irq)
{
	/* We don't do dynamic PCI IRQ allocation */
}

/* Dynamic DMA Mapping stuff
 * 	++ajoshi
 */

#include <linux/types.h>
#include <linux/slab.h>
#include <linux/string.h>
#include <asm/scatterlist.h>
#include <asm/io.h>
#include <asm/prom.h>

struct pci_dev;

extern void *pci_alloc_consistent(struct pci_dev *hwdev, size_t size,
				  dma_addr_t *dma_handle);
extern void pci_free_consistent(struct pci_dev *hwdev, size_t size,
				void *vaddr, dma_addr_t dma_handle);

extern dma_addr_t pci_map_single(struct pci_dev *hwdev, void *ptr,
					size_t size, int direction);
extern void pci_unmap_single(struct pci_dev *hwdev, dma_addr_t dma_addr,
                             size_t size, int direction);
extern int pci_map_sg(struct pci_dev *hwdev, struct scatterlist *sg,
                      int nents, int direction);
extern void pci_unmap_sg(struct pci_dev *hwdev, struct scatterlist *sg,
                         int nents, int direction);

extern inline void pci_dma_sync_single(struct pci_dev *hwdev,
				       dma_addr_t dma_handle,
				       size_t size, int direction)
{
	if (direction == PCI_DMA_NONE)
		BUG();
	/* nothing to do */
}

extern inline void pci_dma_sync_sg(struct pci_dev *hwdev,
				   struct scatterlist *sg,
				   int nelems, int direction)
{
	if (direction == PCI_DMA_NONE)
		BUG();
	/* nothing to do */
}

/* Return whether the given PCI device DMA address mask can
 * be supported properly.  For example, if your device can
 * only drive the low 24-bits during PCI bus mastering, then
 * you would pass 0x00ffffff as the mask to this function.
 */
extern inline int pci_dma_supported(struct pci_dev *hwdev, dma_addr_t mask)
{
	return 1;
}

/* Return the index of the PCI controller for device PDEV. */
extern int pci_controller_num(struct pci_dev *pdev);

/* Map a range of PCI memory or I/O space for a device into user space */
int pci_mmap_page_range(struct pci_dev *pdev, struct vm_area_struct *vma,
			enum pci_mmap_state mmap_state, int write_combine);

/* Tell drivers/pci/proc.c that we have pci_mmap_page_range() */
#define HAVE_PCI_MMAP	1

#define sg_dma_address(sg)	((sg)->dma_address)
#define sg_dma_len(sg)		((sg)->dma_length)

#endif	/* __KERNEL__ */

#endif /* __PPC64_PCI_H */
