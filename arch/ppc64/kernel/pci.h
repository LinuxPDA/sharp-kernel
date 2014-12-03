/*
 * c 2001 PPC 64 Team, IBM Corp
 *
 *      This program is free software; you can redistribute it and/or
 *      modify it under the terms of the GNU General Public License
 *      as published by the Free Software Foundation; either version
 *      2 of the License, or (at your option) any later version.
 */
#ifndef __PPC_KERNEL_PCI_H__
#define __PPC_KERNEL_PCI_H__

/* Include these to prevent strange compile warnings if user forgets 
 * to include them in their .c file.                                */
#include <linux/pci.h>
#include <asm/pci-bridge.h>

extern unsigned long isa_io_base;
extern unsigned long isa_mem_base;
extern unsigned long pci_dram_offset;

/*******************************************************************
 * Platform independant variables referenced.
 *******************************************************************
 * Set pci_assign_all_busses to 1 if you want the kernel to re-assign
 * all PCI bus numbers.  
 *******************************************************************/
extern int pci_assign_all_busses;

extern struct pci_controller* pci_alloc_pci_controller(char *model, enum phb_types controller_type);
extern struct pci_controller* pci_find_hose_for_OF_device(struct device_node* node);

/*******************************************************************
 * Platform functions that are brand specific implementation. 
 *******************************************************************/
extern unsigned long find_and_init_phbs(void);

extern int    pci_read_irq_line(struct pci_dev* PciDev);
extern int    pci_read_bar_registers(struct pci_dev* PciDev, int Count, int RomFlag);

extern void   fixup_resources(struct pci_dev *dev);
extern void   ppc64_pcibios_init(void);

extern int    pci_reset_device(struct pci_dev*);
extern int    pci_get_location(struct pci_dev*, char*, int); 

extern struct pci_dev* find_floppy(void);
extern struct pci_dev *ppc64_floppy_dev;
/*******************************************************************
 * Platform configuration flags.. (Live in pci.c)
 *******************************************************************/
extern int  Pci_Large_Bus_System;      /* System has > 256 buses   */
extern int  Pci_Set_IOA_Address;       /* Set IOA BARs from OF     */
extern int  Pci_Manage_Phb_Space;      /* Manage Phb Space for IOAs*/

/*******************************************************************
 * Helper macros for extracting data from pci structures.  
 *   PCI_GET_PHB_PTR(struct pci_dev*)    returns the Phb pointer.
 *   PCI_GET_PHB_NUMBER(struct pci_dev*) returns the Phb number.
 *   PCI_GET_BUS_NUMBER(struct pci_dev*) returns the bus number.
 *******************************************************************/
#define PCI_GET_PHB_PTR(PCIDEV)    ((struct pci_controller*)##PCIDEV##->sysdata)
#define PCI_GET_PHB_NUMBER(PCIDEV) ((##PCIDEV##->bus->number&0x00FFFF00)>>8)
#define PCI_GET_BUS_NUMBER(PCIDEV) ( ##PCIDEV##->bus->number&0x0000FF)

/*******************************************************************
 * Pci Flight Recorder support.
 *******************************************************************/
#define PCIFR(...) fr_Log_Entry(PciFr,__VA_ARGS__);
extern struct flightRecorder* PciFr;

/*******************************************************************
 * Debugging  Routines.
 *******************************************************************/
extern void dumpResources(struct resource* Resource);
extern void dumpPci_Controller(struct pci_controller* phb);
extern void dumpPci_Bus(struct pci_bus* Pci_Bus);
extern void dumpPci_Dev(struct pci_dev* Pci_Dev);

extern void dump_Phb_tree(void);
extern void dump_Bus_tree(void);
extern void dump_Dev_tree(void);

#endif /* __PPC_KERNEL_PCI_H__ */
