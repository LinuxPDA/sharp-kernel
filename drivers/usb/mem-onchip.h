/*
 * linux/drivers/usb/mem-onchip.h
 * 
 * Copyright (c) 2003 Lineo Solutions, Inc.
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License. See the file "COPYING" in the main directory of this archive
 * for more details.
 *
 */

#ifndef _MEM_ON_CHIP_H_
#define _MEM_ON_CHIP_H_       1

#include <linux/pci.h>

#define OC_MEM_PCI_MAP_DEBUG
#define OC_MEM_PCI_MAP_USE_MEMCPY

#if defined(CONFIG_CPU_PXA27X)
#define OC_MEM_VAR_TO_OFFSET(x) (((unsigned int)x - OC_MEM_TOP) + OC_MEM_PHY)
#define OC_MEM_OFFSET_TO_VAR(x) (((unsigned int)x - OC_MEM_PHY) + OC_MEM_TOP)
#else
#define OC_MEM_VAR_TO_OFFSET(x) ((unsigned int)x - OC_MEM_TOP)
#define OC_MEM_OFFSET_TO_VAR(x) ((unsigned int)x + OC_MEM_TOP)
#endif /* CONFIG_CPU_PXA27X */

#ifdef __LINUX_USB_H
#define pci_alloc_consistent(dev, size, dma_addr) \
	oc_hcca_alloc(dev, size, dma_addr)
#define pci_free_consistent(dev, size, var, dma_addr) \
	oc_hcca_free(dev, size, var, dma_addr)
#else
#define pci_alloc_consistent(dev, size, dma_addr) \
	PCI_ALLOC_CONSISTENT(dev, size, dma_addr)
#define pci_free_consistent(dev, size, var, dma_addr) \
        KFREE(var)
#endif

#define pci_map_single(hwdev, ptr, size, direction) \
	PCI_MAP_SINGLE(hwdev, ptr, size, direction)
#define pci_unmap_single(hwdev, dma_addr, size, direction) \
	PCI_UNMAP_SINGLE(hwdev, dma_addr, size, direction)

extern u32 OC_MEM_TOP;
extern u32 OC_MEM_PHY;

#if defined(CONFIG_CPU_PXA27X)
void oc_mem_init(unsigned int top, size_t size, dma_addr_t phy);
#else
void oc_mem_init(unsigned int top, size_t size);
#endif /* CONFIG_CPU_PXA27X */
void* KMALLOC(size_t s, int v);
void KFREE(void* p);
void* oc_hcca_alloc(struct pci_dev *hwdev, size_t size, dma_addr_t *handle);
void oc_hcca_free(struct pci_dev *hwdev, 
			size_t size, void *vaddr, dma_addr_t dma_handle);


/*
 * replace PCI function
*/
static inline void* PCI_ALLOC_CONSISTENT(struct pci_dev *hwdev, size_t size, dma_addr_t *handle)
{
	void *p = KMALLOC(size, GFP_KERNEL | GFP_DMA);
	*handle = (dma_addr_t)OC_MEM_VAR_TO_OFFSET(p);
	return p;
}

#ifdef OC_MEM_PCI_MAP_DEBUG
static inline char* PCI_DMA_FLAG_DESC(int direction)
{
	switch (direction) {
	case PCI_DMA_NONE:		return "NONE";
	case PCI_DMA_FROMDEVICE:	return "FROMDEVICE";
	case PCI_DMA_TODEVICE:		return "TODEVICE";
	case PCI_DMA_BIDIRECTIONAL:	return "BIDIRECTIONAL";
	}
	return "unknown";
}
#endif

static inline dma_addr_t
PCI_MAP_SINGLE(struct pci_dev *hwdev, void *ptr, size_t size, int direction)
{
#ifndef OC_MEM_PCI_MAP_USE_MEMCPY
        return (dma_addr_t)OC_MEM_VAR_TO_OFFSET(ptr);
#else
	void *p;
	size_t l = ((size + 3) / 4) * 4 + 4;

	switch (direction) {
	case PCI_DMA_NONE:
		BUG();
		break;
	case PCI_DMA_FROMDEVICE:        /* invalidate only */
		p = KMALLOC(l, GFP_KERNEL | GFP_DMA);
		break;
	case PCI_DMA_TODEVICE:          /* writeback only */
	case PCI_DMA_BIDIRECTIONAL:     /* writeback and invalidate */
		p = KMALLOC(l, GFP_KERNEL | GFP_DMA);
		memcpy(p, ptr, size);
		break;
	}

	*((volatile unsigned int *)((unsigned int)p + l - 4))
							= (unsigned int)ptr;
#ifdef OC_MEM_PCI_MAP_DEBUG
#if 0
	printk("PCI_MAP_SINGLE:   %s %08x - %04x , %08x - %04x\n", 
			PCI_DMA_FLAG_DESC(direction),
			ptr, size, OC_MEM_VAR_TO_OFFSET(p), l);
#endif
#endif
        return (dma_addr_t)OC_MEM_VAR_TO_OFFSET(p);
#endif
}

static inline void
PCI_UNMAP_SINGLE(struct pci_dev *hwdev, dma_addr_t dma_addr, size_t size, int direction)
{
#ifndef OC_MEM_PCI_MAP_USE_MEMCPY
        /* nothing to do */
#else
	void *p, *ptr;
	size_t l = ((size + 3) / 4) * 4 + 4;
	p = (void *)OC_MEM_OFFSET_TO_VAR(dma_addr);
	ptr = (void *)*((volatile unsigned int *)((unsigned int)p + l - 4));

	switch (direction) {
	case PCI_DMA_NONE:
		BUG();
		break;
	case PCI_DMA_FROMDEVICE:        /* invalidate only */
	case PCI_DMA_BIDIRECTIONAL:     /* writeback and invalidate */
		memcpy(ptr, p, size);
		KFREE(p);
		break;
	case PCI_DMA_TODEVICE:          /* writeback only */
		KFREE(p);
		break;
	}
#ifdef OC_MEM_PCI_MAP_DEBUG
#if 0
	printk("PCI_UNMAP_SINGLE: %s %08x - %04x , %08x\n",
			PCI_DMA_FLAG_DESC(direction),  ptr, size, dma_addr);
#endif
#endif
#endif
}

#endif	/* _MEM_ON_CHIP_H_ */
