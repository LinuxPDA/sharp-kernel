/*
 * linux/arch/sh/kernel/pci-sh7751_auto.c
 *
 * Based largely on linux/drivers/pci.c
 * Copyright (C) 2003 Kuniki Nakamura
 *
 * PCIBIOS for Hitachi SH7751 series
 */

#include <linux/config.h>
#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/pci.h>
#include <linux/sched.h>
#include <linux/ioport.h>
#include <linux/errno.h>
#include <linux/irq.h>
#include <asm/pci.h>

#define DEVFN_MAX	0x80
#define BUS_MAX		3
#define BASE_ADDR	0x10	

// #define DEBUG
#undef DEBUG
#ifdef DEBUG
#define Dprintk		printk
#else
#define Dprintk
#endif

#define PCIBIOS_MEM_SIZE		0x1000000	/* 16MB */
#define PCIBIOS_IO_SIZE			0x40000		/* 256KB */

typedef struct sh7751_pci_resource{
	u32 start;
	u32 end;
} sh7751_pcires;

typedef struct sh7751_pci{
	sh7751_pcires	mem_resource[BUS_MAX];
	sh7751_pcires	io_resource[BUS_MAX];
	u32 io_p;
	u32 mem_p;
	int bnum[BUS_MAX];
} sh7751_pci;

extern struct pci_ops *pci_root_ops;
static int bridge_place = 0;

static u32 sh7751_pci_size(u32 sz, unsigned long mask)
{
	unsigned long size = mask & sz;
	size = size & ~(size - 1);
//	return size - 1;
	return size;
}	

static u32 sh7751_pci_align(u32 addr, u32 a_size)
{
	addr += (addr % a_size == 0) ? 0 : a_size - (addr % a_size);
	return addr;
}

static struct pci_bus * sh7751_pci_alloc_bus(void)
{
	struct pci_bus *b;
	b = kmalloc(sizeof(*b), GFP_KERNEL);
	if(b){
		memset(b, 0, sizeof(*b));
		INIT_LIST_HEAD(&b->children);
		INIT_LIST_HEAD(&b->devices);
	}
	return b;
}

static struct pci_bus * sh7751_pci_alloc_primary_bus(int bus)
{
	struct pci_bus *b;
	b = sh7751_pci_alloc_bus();
	if(!b)
		return NULL;
	b->number = b->secondary = bus;
	return b;
}

void sh7751_init_pci_resource(sh7751_pci *pci, int bus)
{
	memset(pci, 0, sizeof(*pci));
	pci->mem_resource[bus].start = PCIBIOS_MIN_MEM;
	pci->mem_resource[bus].end = PCIBIOS_MIN_MEM + PCIBIOS_MEM_SIZE;
//	pci->io_resource[bus].start = PCIBIOS_MIN_IO;
	pci->io_resource[bus].start = 0x1000;
	pci->io_resource[bus].end = PCIBIOS_MIN_IO + PCIBIOS_IO_SIZE;
}
	
static u32 sh7751_pci_check_mem_resource(sh7751_pci *pci, struct pci_dev *dev, u32 size)
{
	struct pci_bus *bus = dev->bus;
	sh7751_pcires *rs;
	u32 r_start, r_end, r_size, l;
//	l = (dev->device << 16) | dev->vendor;
	rs = &pci->mem_resource[bus->number];
	r_start = (pci->mem_p == 0) ? rs->start : pci->mem_p;
	r_end = rs->end;

	r_start = sh7751_pci_align(r_start, size);
	r_size = r_end - r_start;
	if(size >= r_size){
		printk("Unable to allocate pci memory!!\n");
		return -EBUSY;
	}
	pci->mem_p = r_start + size;
	return r_start;
}
	
static u32 sh7751_pci_check_io_resource(sh7751_pci *pci, struct pci_dev *dev, unsigned long size)
{
	struct pci_bus *bus = dev->bus;
	sh7751_pcires *rs;
	u32 r_start, r_end, r_size;
	rs = &pci->io_resource[bus->number];
	r_start = (pci->io_p == 0) ? rs->start : pci->io_p;
	r_end = rs->end;

	r_start = sh7751_pci_align(r_start, size);
	r_size = r_end - r_start;
	if(size >= r_size){
		printk("Unable to allocate pci i/o!!\n");
		return -EBUSY;
	}
	pci->io_p = r_start + size;
	return r_start;
}

static int sh7751_pci_assign_devices(sh7751_pci *pci, struct pci_dev *dev, int howmany)
{
	u32 l, size;
	int pos, regno;
	for(pos = 0; pos < howmany; pos++){
		regno = BASE_ADDR + (pos << 2);
		pci_read_config_dword(dev, regno, &l);
		pci_write_config_dword(dev, regno, ~0);
		pci_read_config_dword(dev, regno, &size);
		pci_write_config_dword(dev, regno, l);
		if (!size || size == 0xffffffff)
                        continue;
                if (l == 0xffffffff)
                        l = 0;
                if ((l & PCI_BASE_ADDRESS_SPACE) == PCI_BASE_ADDRESS_SPACE_MEMORY
) {
                        size = sh7751_pci_size(size, PCI_BASE_ADDRESS_MEM_MASK);
			l = sh7751_pci_check_mem_resource(pci, dev, size);
			if (l < 0) 
				return -EBUSY; 
			Dprintk("Allocate mem resource VendorID = 0x%x DeviceID = 0x%x regno=0x%x resource = 0x%x\n", dev->vendor, dev->device, regno, l);
			pci_write_config_dword(dev, regno, l);
                } else {
                        size = sh7751_pci_size(size, PCI_BASE_ADDRESS_IO_MASK & 0xffff);
			l = sh7751_pci_check_io_resource(pci, dev, size);
			if (l < 0) 
				return -EBUSY; 
			Dprintk("Allocate i/o resource VendorID = 0x%x DeviceID = 0x%x regno=0x%x resource = 0x%x\n", dev->vendor, dev->device, regno, l);
			pci_write_config_dword(dev, regno, l);
		}
	}	
	return 0;	
}

static int sh7751_pci_setup_devices(sh7751_pci *pci, struct pci_dev *dev)
{
	int ret = 0;
	switch(dev->hdr_type){
        case PCI_HEADER_TYPE_NORMAL:                /* standard header */
                ret = sh7751_pci_assign_devices(pci, dev, 6);

        case PCI_HEADER_TYPE_BRIDGE:                /* bridge header */
	//	ret = sh7751_pci_assign_devices(pci, dev, 2);
	default:
		break;
	}
	return ret;
}

static struct pci_dev * sh7751_pci_scan_device(sh7751_pci *pci, struct pci_dev *temp)
{
	struct pci_dev *dev;
	u32 l;
	pci_read_config_dword(temp, PCI_VENDOR_ID, &l);	
        if (l == 0xffffffff || l == 0x00000000 || l == 0x0000ffff || l == 0xffff0000)
                return NULL;
	dev = kmalloc(sizeof(*dev), GFP_KERNEL);
	if (!dev)
		return NULL;
	memcpy(dev, temp, sizeof(*dev));
	dev->vendor = l & 0xffff;
	dev->device = (l >> 16) & 0xffff;
	Dprintk("VendorID = 0x%x DeviceID = 0x%x\n", dev->vendor, dev->device);
	return dev;
}

int sh7751_pci_scan_slot(sh7751_pci *pci, struct pci_dev *temp)
{
	struct pci_bus *bus = temp->bus;
	struct pci_dev *dev;
	unsigned char hdr_type;
	int func, is_multi = 0, ret = 0;
	for(func = 0; func < 8; func++, temp->devfn++){
		if (func && !is_multi)  /* not a multi-function device */
			continue;
		if (pci_read_config_byte(temp, PCI_HEADER_TYPE, &hdr_type))
			continue;
		temp->hdr_type = hdr_type & 0x7f;
		dev = sh7751_pci_scan_device(pci, temp);
	
		if (!dev)
			continue;

		if ((ret == sh7751_pci_setup_devices(pci, dev)) < 0){
			kfree(dev);
			dev = NULL;
			return ret;
		}
		if(!func) 
			is_multi = hdr_type & 0x80;
		list_add_tail(&dev->bus_list, &bus->devices);
	}
	return ret;
}

static struct pci_bus * sh7751_pci_add_new_bus(struct pci_bus *parent, struct pci_dev *dev, int busnr)
{
	struct pci_bus *child;
	child = sh7751_pci_alloc_bus();
	if (!child)
		return NULL;
	list_add_tail(&child->node, &parent->children);
	child->self = dev;
	dev->subordinate = child;
	child->parent = parent;
	child->ops = parent->ops;
	
	child->number = child->secondary = busnr;
	child->primary = parent->secondary;
	child->subordinate = 0xff; 
	return child;
}

void sh7751_pci_setup_bridge(sh7751_pci *pci, struct pci_bus *child)
{
	struct pci_dev *dev = child->self;
	unsigned short io_base_hi, io_limit_hi, mem_base, mem_limit;
	unsigned char io_base_lo, io_limit_lo, tmp_io_base_lo;
	u32 io_start, io_end, mem_start, mem_end;

	if (!dev){
		return;
	}
	io_start = pci->io_resource[child->number].start;
	io_end = pci->io_resource[child->number].end;
	mem_start = pci->mem_resource[child->number].start;
	mem_end = pci->mem_resource[child->number].end;

	Dprintk("io_start = 0x%x io_end = 0x%x mem_start = 0x%x mem_end = 0x%x\n", io_start, io_end, mem_start, mem_end);
	io_base_lo = (unsigned char)((io_start >> 8) & 0xf0);
	io_limit_lo = (unsigned char)((io_end >> 8) & 0xf0);
	io_base_hi = (unsigned short)(io_start >> 16);	
	io_limit_hi = (unsigned short)(io_end >> 16);	
	mem_base = (unsigned short)(mem_start >> 16);
	mem_limit = (unsigned short)(mem_end >> 16);
	pci_read_config_byte(dev, PCI_IO_BASE, &tmp_io_base_lo);
	pci_write_config_byte(dev, PCI_IO_BASE, io_base_lo);
	pci_write_config_byte(dev, PCI_IO_LIMIT, io_limit_lo);
        if ((tmp_io_base_lo & PCI_IO_RANGE_TYPE_MASK) == PCI_IO_RANGE_TYPE_32) {
                pci_write_config_word(dev, PCI_IO_BASE_UPPER16, io_base_hi);
                pci_write_config_word(dev, PCI_IO_LIMIT_UPPER16, io_limit_hi);
        }

	pci_write_config_word(dev, PCI_MEMORY_BASE, mem_base);
	pci_write_config_word(dev, PCI_MEMORY_LIMIT, mem_limit);
}
 
int sh7751_pci_scan_bus(sh7751_pci *pci, struct pci_bus *bus);

int sh7751_pci_scan_bridge(sh7751_pci *pci, struct pci_bus *bus, struct pci_dev *dev, int max)
{
	struct pci_bus *child;
	u32 buses;
	pci_read_config_dword(dev, PCI_PRIMARY_BUS, &buses);
	pci_write_config_dword(dev, PCI_COMMAND, 0xFB900047);
	child = sh7751_pci_add_new_bus(bus, dev, ++max);
	if (!child)
		return -EBUSY;
	buses = (buses & 0xff000000)
		| ((unsigned long)(child->primary) << 0)
		| ((unsigned long)(child->secondary) << 8)
		| ((unsigned long)(child->subordinate) << 16);
	Dprintk("PCI-PCI Bridge: primary = %d secondary = %d subordinate = %d\n", child->primary, child->secondary, child->subordinate);
	
	pci_write_config_dword(dev, PCI_PRIMARY_BUS, buses);

	/* setup PCI-PCI Bridge io/memory */
	pci->io_resource[child->number].start = pci->io_p = sh7751_pci_align(pci->io_p, 0x1000); 
	pci->io_resource[child->number].end = pci->io_resource[bus->number].end;
	pci->mem_resource[child->number].start = pci->mem_p = sh7751_pci_align(pci->mem_p, 0x100000); 
	pci->mem_resource[child->number].end = pci->mem_resource[bus->number].end;
		
	max = sh7751_pci_scan_bus(pci, child);
	pci_write_config_byte(dev, PCI_SUBORDINATE_BUS, max);
	return max;
}

int sh7751_pci_scan_bus(sh7751_pci *pci, struct pci_bus *bus)
{
	int max, ret = 0;
	u32 devfn;
	struct pci_dev *dev, dev0;
	struct list_head *ln; 
	max = bus->secondary;
	dev0.bus = bus;
	for(devfn = 0; devfn <= DEVFN_MAX; devfn +=8){
		dev0.devfn = devfn;
		if((ret = sh7751_pci_scan_slot(pci, &dev0)) < 0)
			return -EBUSY;
	}
	sh7751_pci_setup_bridge(pci, bus);
	for (ln = bus->devices.next; ln != &bus->devices; ln=ln->next) {
		dev = pci_dev_b(ln);
		if (dev->hdr_type == PCI_HEADER_TYPE_BRIDGE){
			if (!pci->bnum[bus->number]){
				if (!bus->self)
					bridge_place = (PCI_SLOT(dev->devfn) + 1);
				pci->bnum[bus->number] = 1;
				max = sh7751_pci_scan_bridge(pci, bus, dev, max);
			}
			else
				printk("It is not supporting that there are two or more PCI-PCI bridges in the same bus\n");
			if (max < 0)
				break;
		}
	}
	return max;
} 
				
int sh7751_pci_auto_config(void)
{
	sh7751_pci *pci;	
	struct pci_bus *bus;
	int ret;
	pci = (sh7751_pci *)kmalloc(sizeof(*pci), GFP_KERNEL);
	sh7751_init_pci_resource(pci, 0);	
	bus = sh7751_pci_alloc_primary_bus(0);
	if(!bus){
		printk("allocate pci bus is failed\n");
		kfree(pci);
		return bridge_place;
	}
	bus->ops = pci_root_ops;
	ret = sh7751_pci_scan_bus(pci, bus);
	if(ret < 0)
		printk("pci initialized failed\n");
	kfree(pci);
	return bridge_place;
}
	
	
		
