/*
 * pSeries_pci.c
 *
 * pSeries_pcibios_init(void)opyright (C) 2001 Dave Engebretsen, IBM Corporation
 *
 * pSeries specific routines for PCI.
 * 
 * Based on code from pci.c and chrp_pci.c
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *    
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307 USA
 */

#include <linux/config.h>
#include <linux/kernel.h>
#include <linux/pci.h>
#include <linux/delay.h>
#include <linux/string.h>
#include <linux/init.h>
#include <linux/ide.h>
#include <linux/bootmem.h>

#include <asm/io.h>
#include <asm/pgtable.h>
#include <asm/irq.h>
#include <asm/prom.h>
#include <asm/machdep.h>
#include <asm/init.h>
#include <asm/pci-bridge.h>
#include <asm/ppcdebug.h>
#include <asm/Naca.h>
#include <asm/pci_dma.h>

#include "xics.h"
#include "open_pic.h"
#include "pci.h"

/* Supporting macros *********************************************/
#define TESTBIT(value,bits)    ((value&bits) == bits)
#define TRUE  1
#define FALSE 0

extern struct pci_controller* hose_head;
extern struct pci_controller** hose_tail;
extern struct Naca *naca;
extern struct pci_ops rtas_pci_ops;
extern struct pci_ops ibm_phb_pci_ops;
extern struct device_node *allnodes;
extern unsigned long phb_tce_table_init(struct pci_controller *phb);

/*******************************************************************
 * Forward declares of prototypes. 
 *******************************************************************/
unsigned long find_and_init_phbs(void);
struct pci_controller* alloc_phb(struct device_node *dev, char *model, unsigned int addr_size_words) ;
void   pSeries_pcibios_fixup(void);
void   pci_build_bar_resources(struct pci_dev* Pci_Dev, int BarCount);
void   pci_build_rom_resources(struct pci_dev* Pci_Dev);
void   pci_set_BARS(struct pci_dev* Pci_Dev);

/**********************************************************************************
 *
 * pSeries I/O Operations to access the PCI configuration space.
 *
 **********************************************************************************/
#define RTAS_PCI_READ_OP(size, type, nbytes)	  \
int __chrp								  \
rtas_read_config_##size(struct pci_dev *dev, int offset, type val) {  \
	unsigned long ReturnValue, ConfigAddr; \
	int           ReturnCode; \
	ConfigAddr = ((dev->bus->number&0x0000FF) << 16) | (dev->devfn << 8) | (offset & 0xff); \
	ReturnCode = call_rtas("read-pci-config", 2, 2, &ReturnValue, ConfigAddr, nbytes);\
	*val = ReturnValue; \
	return ReturnCode;  \
}
#define RTAS_PCI_WRITE_OP(size, type, nbytes)		                  \
int __chrp								                       \
rtas_write_config_##size(struct pci_dev *dev, int offset, type val) { \
	unsigned long ConfigAddr; \
	int           ReturnCode; \
	ConfigAddr = ((dev->bus->number&0x0000FF) << 16) | (dev->devfn << 8) | (offset & 0xff); \
	ReturnCode = call_rtas("write-pci-config", 3, 1, NULL, ConfigAddr, nbytes, (ulong)val);	 \
	return ReturnCode; \
}

RTAS_PCI_READ_OP(byte, u8 *, 1)
RTAS_PCI_READ_OP(word, u16 *, 2)
RTAS_PCI_READ_OP(dword, u32 *, 4)
RTAS_PCI_WRITE_OP(byte, u8, 1)
RTAS_PCI_WRITE_OP(word, u16, 2)
RTAS_PCI_WRITE_OP(dword, u32, 4)

struct pci_ops rtas_pci_ops = {
	rtas_read_config_byte,
	rtas_read_config_word,
	rtas_read_config_dword,
	rtas_write_config_byte,
 	rtas_write_config_word,
	rtas_write_config_dword,
	pci_read_bar_registers,
	pci_read_irq_line
};

/**********************************************************************************
 *
 * pSeries I/O Operations to access the PCI configuration space.
 * This is the support for Large Systems(>256 buses).
 *
 **********************************************************************************/
#define RTAS64_PCI_READ_OP(size, type, nbytes)			    	  \
int __chrp								  \
rtas64_read_config_##size(struct pci_dev *dev, int offset, type val) 	  \
{									  \
	struct pci_controller *hose = dev->sysdata;			  \
	unsigned long addr = (offset & 0xff) | ((dev->devfn & 0xff) << 8) \
		| (((dev->bus->number) & 0xff) << 16);			  \
	unsigned long ret = ~0UL;					  \
	int rval;							  \
	int buidhi = hose->buid >> 32;				          \
	int buidlo = hose->buid & 0xffffffff;			          \
									  \
	rval = call_rtas("ibm,read-pci-config", 4, 2, &ret,               \
                         addr, buidhi, buidlo, nbytes);               	  \
	*val = ret;		                  			  \
	/*	udbg_printf("%08x %08x SYM Read Config %d\n",addr, ret,rval);   */            \
	return rval? PCIBIOS_DEVICE_NOT_FOUND: PCIBIOS_SUCCESSFUL;    	  \
}

#define RTAS64_PCI_WRITE_OP(size, type, nbytes)				  \
int __chrp								  \
rtas64_write_config_##size(struct pci_dev *dev, int offset, type val)	  \
{									  \
	struct pci_controller *hose = dev->sysdata;			  \
	unsigned long addr = (offset & 0xff) | ((dev->devfn & 0xff) << 8) \
		| (((dev->bus->number) & 0xff) << 16);			  \
	int rval;							  \
	int buidhi = hose->buid >> 32;				          \
	int buidlo = hose->buid & 0xffffffff;			          \
									  \
	rval = call_rtas("ibm,write-pci-config", 5, 1, NULL,		  \
			 addr, buidhi, buidlo, nbytes, (ulong)val);	  \
	/*	udbg_printf("%08x %08x SYM Write Config %d\n",addr, val,rval); */               \
	return rval? PCIBIOS_DEVICE_NOT_FOUND: PCIBIOS_SUCCESSFUL;	  \
}

RTAS64_PCI_READ_OP(byte, u8 *, 1)
RTAS64_PCI_READ_OP(word, u16 *, 2)
RTAS64_PCI_READ_OP(dword, u32 *, 4)
RTAS64_PCI_WRITE_OP(byte, u8, 1)
RTAS64_PCI_WRITE_OP(word, u16, 2)
RTAS64_PCI_WRITE_OP(dword, u32, 4)

struct pci_ops rtas64_pci_ops = {
	rtas64_read_config_byte,
	rtas64_read_config_word,
	rtas64_read_config_dword,
	rtas64_write_config_byte,
 	rtas64_write_config_word,
	rtas64_write_config_dword
};


/******************************************************************
 * pci_read_irq_line
 *
 * Reads the Interrupt Pin to determine if interrupt is use by card.
 * If the interrupt is used, then gets the interrupt line from the 
 * openfirmware and sets it in the pci_dev and pci_config line.
 *
 ******************************************************************/
int 
pci_read_irq_line(struct pci_dev* Pci_Dev) {
   	u8 InterruptPin;
	struct device_node* Node;

    	pci_read_config_byte(Pci_Dev, PCI_INTERRUPT_PIN, &InterruptPin);
	if(InterruptPin == 0) {
		PPCDBG(PPCDBG_BUSWALK,"\tDevice: %s No Interrupt used by device.\n",Pci_Dev->slot_name);
		return 0;	
	}
	Node = pci_device_to_OF_node(Pci_Dev);
	if( Node == NULL) { 
		PPCDBG(PPCDBG_BUSWALK,"\tDevice: %s Device Node not found.\n",Pci_Dev->slot_name);
		return -1;	
	}
	if(Node->n_intrs == 0) 	{
		PPCDBG(PPCDBG_BUSWALK,"\tDevice: %s No Device OF interrupts defined.\n",Pci_Dev->slot_name);
		return -1;	
	}
	Pci_Dev->irq = Node->intrs[0].line;
	pci_write_config_byte(Pci_Dev, PCI_INTERRUPT_LINE, Pci_Dev->irq);
	
	PPCDBG(PPCDBG_BUSWALK,"\tDevice: %s pci_dev->irq = 0x%02X\n",Pci_Dev->slot_name,Pci_Dev->irq);
	return 0;
}
/******************************************************************
 * pci_set_BARS
 *
 * Sets the IOA BAR registers from the values from the register 
 * values found in the OpenFirmware node for the device.  Or in 
 * the future, assigns space in phb and sets the values.
 ******************************************************************/
void 
pci_set_BARS(struct pci_dev* Pci_Dev) {
	int AddrIndex;
	struct device_node *Device_Node = pci_device_to_OF_node(Pci_Dev);

	if(Device_Node == 0 || Device_Node->n_addrs == 0 || Device_Node->addrs == NULL) {
		PPCDBG(PPCDBG_BUSWALK, "\tDevice Node was not found or no defined bars\n");
		return;
	}
	PPCDBG(PPCDBG_BUSWALK, "\tSet Bar Regs(%d)\n",Device_Node->n_addrs);
	for(AddrIndex = 0; AddrIndex < Device_Node->n_addrs; ++AddrIndex) {
		int PciReg = (Device_Node->addrs[AddrIndex].space & 0x000000FF);
		u32 PciBar = Device_Node->addrs[AddrIndex].address;
		int BarSze = Device_Node->addrs[AddrIndex].size;
		
		PPCDBG(PPCDBG_BUSWALK, "\tPciBar 0x%02X, 0x%08X, 0x%08X\n",PciReg,PciBar,BarSze);
		pci_write_config_dword(Pci_Dev, PciReg,PciBar);
	}
}
/******************************************************************
 *
 * pci_build_bar_registers
 *
 * This function will build the resources based on the information 
 * that is extracted from the BAR register.  The original BAR value
 * is saved and restored.  
 *
 * Note: This is pSeries brand specific code.  
 *       In the future, this code will be setting the initial BAR 
 *       value and handing out the memory space in the PHB. 
 ******************************************************************/
void 
pci_build_bar_resources(struct pci_dev* Pci_Dev, int Count) {
	int  BarRegister   = PCI_BASE_ADDRESS_0;
	int  EndingBar     = BarRegister + (Count*4);
	int  ResourceIndex = 0;
	PPCDBG(PPCDBG_BUSWALK, "\npci_read_bar_registers %s\n",Pci_Dev->slot_name); 

    /**************************************************************
	* Read Bars until the ending bar has been read. 
    **************************************************************/
	for(BarRegister = PCI_BASE_ADDRESS_0;BarRegister <= EndingBar; BarRegister += 4) {
		struct resource* Resource = &Pci_Dev->resource[ResourceIndex];
		u32  BarSaveArea, BarSizeBits, BarSize;
	    	u64  BarStart;
		/**********************************************************
		* Save the original bar value and check for success.
		**********************************************************/
    		if((pci_read_config_dword( Pci_Dev, BarRegister, &BarSaveArea) != 0) ||
     	  (BarSaveArea == 0xFFFFFFFF )) {		        
	    		BarRegister += 4;
			continue;
    		}
		/**********************************************************
		* Write all ones to register to get the size of area.
		**********************************************************/
		pci_write_config_dword(Pci_Dev, BarRegister, 0xFFFFFFFF);
		pci_read_config_dword( Pci_Dev, BarRegister, &BarSizeBits);
		pci_write_config_dword(Pci_Dev, BarRegister, BarSaveArea);

		/**********************************************************
		* Error reading bar(all ones back) or unimplemented BAR(zero)
		**********************************************************/
		if(( BarSizeBits == 0xFFFFFFFF ) || BarSizeBits == 0 ) {
			BarRegister += 4;
			continue;
	    	}
		Resource->name  = Pci_Dev->name;
		/**********************************************************
		* Test and read Io Space, It is always 32 bits.
		**********************************************************/
		if(TESTBIT(BarSizeBits,PCI_BASE_ADDRESS_SPACE_IO) == TRUE) {
			BarSize         = (~(BarSizeBits&PCI_BASE_ADDRESS_IO_MASK)) +1;
			Resource->flags = IORESOURCE_IO | PCI_BASE_ADDRESS_SPACE_IO;
			Resource->start = BarSaveArea & PCI_BASE_ADDRESS_IO_MASK;
			Resource->end   = Resource->start + BarSize - 1;
			++ResourceIndex;
    		}
		/**********************************************************
		* Memory Space, could be 32 bits or 64 bits   
		**********************************************************/
		else {
			Resource->flags = IORESOURCE_MEM;
			if( TESTBIT(BarSizeBits,PCI_BASE_ADDRESS_MEM_PREFETCH) == TRUE) {
	    			Resource->flags  = IORESOURCE_MEM | IORESOURCE_PREFETCH;
			}
			BarStart        = BarSaveArea    &PCI_BASE_ADDRESS_MEM_MASK;
			BarSize         = ~((BarSizeBits)&PCI_BASE_ADDRESS_MEM_MASK)+1;
			/**********************************************************
			* 64 bit register, read next register to get the high 32 
			* bits. PCI Spec says to write both and THEN read.      
			**********************************************************/
			if( TESTBIT(BarSizeBits,PCI_BASE_ADDRESS_MEM_TYPE_64) == TRUE) {
				u64 High64Bits;
				u32 BarHigh64Save; 
				Resource->flags |= PCI_BASE_ADDRESS_MEM_TYPE_64;
				BarRegister    += 4;          /* Index to next Bar  */
	    			pci_read_config_dword( Pci_Dev, BarRegister,  &BarHigh64Save);
	    			High64Bits = BarHigh64Save;
	    			BarStart  += (High64Bits<< 32);
	    			pci_write_config_dword(Pci_Dev, BarRegister-4,0xFFFFFFFF);
	    			pci_write_config_dword(Pci_Dev, BarRegister,  0xFFFFFFFF);
	    			pci_read_config_dword( Pci_Dev, BarRegister-4,&BarSizeBits);
	    			BarSize    = ~((BarSizeBits)&PCI_BASE_ADDRESS_MEM_MASK)+1;
	    			pci_read_config_dword( Pci_Dev, BarRegister,  &BarSizeBits);
	    			High64Bits = ~(BarSizeBits);
	    			BarSize   += (High64Bits << 32);
	    			pci_write_config_dword(Pci_Dev, BarRegister-4,BarSaveArea);
	    			pci_write_config_dword(Pci_Dev, BarRegister,  BarHigh64Save);
				++ResourceIndex;		/* Skip next resource */
			}
			/**********************************************************
			* Set resource fields with values.
			**********************************************************/
			Resource->start = BarStart;
			Resource->end   = BarStart + BarSize - 1;
			++ResourceIndex;
		}
	}
	PPCDBGCALL(PPCDBG_BUSWALK, dumpPci_Dev(Pci_Dev) );
}
/******************************************************************
 *
 * Read the rom register
 *
 ******************************************************************/
void 
pci_build_rom_resources(struct pci_dev* Pci_Dev) {
    u32              RomSaveArea, RomSizeBits;
    u64              RomSize     = 0;
    struct resource* Resource    = &Pci_Dev->resource[PCI_ROM_RESOURCE];
	PPCDBG(PPCDBG_BUSWALK, "\tpci_read_bar_Rom \n"); 

    pci_read_config_dword( Pci_Dev, PCI_ROM_ADDRESS, &RomSaveArea);
    pci_write_config_dword(Pci_Dev, PCI_ROM_ADDRESS, 0XFFFFFFFF);
    pci_read_config_dword( Pci_Dev, PCI_ROM_ADDRESS, &RomSizeBits);
    pci_write_config_dword(Pci_Dev, PCI_ROM_ADDRESS, RomSaveArea);

    if( (RomSizeBits&PCI_ROM_ADDRESS_ENABLE) == PCI_ROM_ADDRESS_ENABLE)  {
		RomSize = ~(RomSizeBits) & PCI_ROM_ADDRESS_MASK;
	    if(RomSize > 0) {
			Resource->name  = Pci_Dev->name;
			Resource->flags = IORESOURCE_MEM | IORESOURCE_READONLY | IORESOURCE_PREFETCH;
			Resource->start = RomSaveArea;
			Resource->end   = Resource->start + RomSize - 1;
    		}
    }
}
/******************************************************************
 *
 * pci_read_bar_registers
 *
 * This function is the hook from the independant code to the arch 
 * dependant code to set and read the BAR registers. 
 *
 * Note: This is pSeries brand specific code.  
 *       In the future, this code will be setting the initial BAR 
 *       value and handing out the PBH memory space. 
 ******************************************************************/
int  
pci_read_bar_registers(struct pci_dev* Pci_Dev, int Count, int RomFlag) {
	u16 Pci_Command_Register_Save = 0;
	u16 Pci_Mask_Reg;
    /**************************************************************
	* If Io or Memory is enabled, disable while the BARs are being
	* changed to avoid any side affects.
    **************************************************************/
	if(( pci_read_config_word( Pci_Dev, PCI_COMMAND, &Pci_Command_Register_Save) != 0 ) ||
	   (Pci_Command_Register_Save == (u16)0xFFFF) ) {
		printk("PCI: Device %s Read Command Failed.\n",Pci_Dev->slot_name);
		PPCDBG(PPCDBG_BUSWALK,"\tDevice %s Read Command Failed.\n",Pci_Dev->slot_name);
		return -1;
	}
	Pci_Mask_Reg = Pci_Command_Register_Save &(PCI_COMMAND_IO|PCI_COMMAND_MEMORY);
	if( Pci_Mask_Reg != 0) {
		u16 ResetCmdReg = Pci_Command_Register_Save & (~Pci_Mask_Reg);
		pci_write_config_word( Pci_Dev, PCI_COMMAND, ResetCmdReg);
	}
	else {
		Pci_Command_Register_Save = 0;
	}
    /**************************************************************
	* Do the base BARS.
    **************************************************************/
	pci_build_bar_resources(Pci_Dev, Count);

    /**************************************************************
	* Rom Flag on, read ROM BARS.
    **************************************************************/
	if(RomFlag != 0) {
		pci_build_rom_resources(Pci_Dev);
	}

    /**************************************************************
	* If the Command Register was modifed, restore it. 
    **************************************************************/
	if(Pci_Command_Register_Save != 0) {
		pci_write_config_word( Pci_Dev, PCI_COMMAND, Pci_Command_Register_Save);
		PPCDBG(PPCDBG_BUSWALK,"\tPCI_COMMAND Register Restored 0x%04X\n",
		                         Pci_Command_Register_Save);
	}
	return 0;
}

/******************************************************************
 * Find all PHBs in the system and initialize a set of data 
 * structures to represent them.
 ******************************************************************/
unsigned long __init
find_and_init_phbs(void)
{
        struct device_node    *Pci_Node;
        struct pci_controller *phb;
        unsigned int root_addr_size_words = 0, this_addr_size_words = 0;
	unsigned int this_addr_count = 0, range_stride;
        unsigned int *ui_ptr = NULL, *ranges;
        char *model;
	struct pci_range64 range;
	struct resource *res;
	unsigned int memno, rlen, i, index;
	unsigned int *opprop;
              
        PPCDBG(PPCDBG_PHBINIT, "find_and_init_phbs\n"); 

	if(naca->interrupt_controller == IC_OPEN_PIC) {
		opprop = (unsigned int *)
			get_property(find_path_device("/"),"platform-open-pic", NULL);
	}

	/* Get the root address word size. */
	ui_ptr = (unsigned int *) get_property(find_path_device("/"), 
					       "#size-cells", NULL);
	if(ui_ptr) root_addr_size_words = *ui_ptr;
	else {
		PPCDBG(PPCDBG_PHBINIT, "\tget #size-cells failed.\n"); 
		return(-1);
	}

	index = 0;

	/******************************************************************
	* Find all PHB devices and create an object for them.
	******************************************************************/
	for( Pci_Node = find_devices("pci");Pci_Node != NULL;Pci_Node = Pci_Node->next) {
		model = (char *) get_property(Pci_Node, "model", NULL);
		if(model != NULL)  {
			phb = alloc_phb(Pci_Node, model, root_addr_size_words);
			if(phb == NULL) return(-1);
		}
		else {
         		continue;
		}
		
		/* Get this node's address word size. */
		ui_ptr = (unsigned int *) get_property(Pci_Node, "#size-cells", NULL);
		if(ui_ptr) this_addr_size_words = *ui_ptr;
		else       this_addr_size_words = 1;
		/* Get this node's address word count. */
		ui_ptr = (unsigned int *) get_property(Pci_Node, "#address-cells", NULL);
		if(ui_ptr) this_addr_count = *ui_ptr;
		else       this_addr_count = 3;
		
		range_stride = this_addr_count + root_addr_size_words + this_addr_size_words;
	      
		memno = 0;
         phb->io_base_phys = 0;
         
		ranges = (unsigned int *) get_property(Pci_Node, "ranges", &rlen);
		PPCDBG(PPCDBG_PHBINIT, "\trange_stride = 0x%lx, rlen = 0x%x\n", range_stride, rlen);  
                
		for(i = 0; i < (rlen/sizeof(*ranges)); i+=range_stride) {
		  	/* Put the PCI addr part of the current element into a 
			 * '64' struct. 
			 */
		  	range = *((struct pci_range64 *)(ranges + i));

			/* If this is a '32' element, map into a 64 struct. */
			if((range_stride * sizeof(int)) == 
			   sizeof(struct pci_range32)) {
				range.parent_addr = 
					(unsigned long)(*(ranges + i + 3));
				range.size = 
					(((unsigned long)(*(ranges + i + 4)))<<32) | 
					(*(ranges + i + 5));
			} else {
				range.parent_addr = 
					(((unsigned long)(*(ranges + i + 3)))<<32) | 
					(*(ranges + i + 4));
				range.size = 
					(((unsigned long)(*(ranges + i + 5)))<<32) | 
					(*(ranges + i + 6));
			}
			
			PPCDBG(PPCDBG_PHBINIT, "\trange.parent_addr    = 0x%lx\n", 
			       range.parent_addr);
			PPCDBG(PPCDBG_PHBINIT, "\trange.child_addr.hi  = 0x%lx\n", 
			       range.child_addr.a_hi);
			PPCDBG(PPCDBG_PHBINIT, "\trange.child_addr.mid = 0x%lx\n", 
			       range.child_addr.a_mid);
			PPCDBG(PPCDBG_PHBINIT, "\trange.child_addr.lo  = 0x%lx\n", 
			       range.child_addr.a_lo);
			PPCDBG(PPCDBG_PHBINIT, "\trange.size           = 0x%lx\n", 
			       range.size);

			res = NULL;
		        switch ((range.child_addr.a_hi >> 24) & 0x3) {
			case 1:		/* I/O space */
				phb->io_base_phys = range.parent_addr;
				res = &phb->io_resource;
				res->flags = IORESOURCE_IO;
				if (isa_io_base == 0) {
					isa_io_base = (unsigned long) 
						ioremap(phb->io_base_phys, range.size);
					PPCDBG(PPCDBG_PHBINIT, "\tisa_io_base    = 0x%lx\n", isa_io_base);  
				}
				phb->pci_io_offset = range.parent_addr - 
					((((unsigned long)
					   range.child_addr.a_mid) << 32) | 
					 (range.child_addr.a_lo));
				PPCDBG(PPCDBG_PHBINIT, "\tpci_io_offset  = 0x%lx\n", 
				       phb->pci_io_offset);

			  	break;
			case 2:		/* mem space */
				PPCDBG(PPCDBG_PHBINIT, "\tMem Space\n");
				phb->pci_mem_offset = range.parent_addr - 
					((((unsigned long)
					   range.child_addr.a_mid) << 32) | 
					 (range.child_addr.a_lo));
				PPCDBG(PPCDBG_PHBINIT, "\tpci_mem_offset = 0x%lx\n", 
				       phb->pci_mem_offset);
				res = &(phb->mem_resources[memno]);
				res->flags = IORESOURCE_MEM;
				++memno;
			  	break;
			}
			if (res) {
				res->name = Pci_Node->full_name;
				res->start = range.parent_addr;
				res->end =   range.parent_addr + range.size - 1;
				res->parent = NULL;
				res->sibling = NULL;
				res->child = NULL;
			}
		}
		PPCDBG(PPCDBG_PHBINIT, "\tphb->io_base_phys   = 0x%lx\n", 
		       phb->io_base_phys); 
		PPCDBG(PPCDBG_PHBINIT, "\tphb->pci_mem_offset = 0x%lx\n", 
		       phb->pci_mem_offset); 

                if(naca->interrupt_controller == IC_OPEN_PIC) {
                        if(root_addr_size_words == 1) {
                                openpic_setup_ISU(index, opprop[index+1]);
                        } else {
                                openpic_setup_ISU(index,
                                  ((unsigned long)opprop[(index+1)*2]) << 32 | 
                                  opprop[(index+1)*2+1]);
                        }
                }

		index++;
	}
	return 0;	 /*Success */
}

/******************************************************************
 *
 * Allocate and partially initialize a structure to represent a PHB.
 *
 ******************************************************************/
struct pci_controller *
alloc_phb(struct device_node *dev, char *model, unsigned int addr_size_words)
{
	struct pci_controller *phb;
	unsigned int *ui_ptr = NULL, len;
	struct reg_property64 reg_struct;
	int          *bus_range;

	PPCDBG(PPCDBG_PHBINIT, "alloc_phb: %s\n", dev->full_name); 
	PPCDBG(PPCDBG_PHBINIT, "\tdev             = 0x%lx\n", dev); 
	PPCDBG(PPCDBG_PHBINIT, "\tmodel           = 0x%lx\n", model); 
	PPCDBG(PPCDBG_PHBINIT, "\taddr_size_words = 0x%lx\n", addr_size_words); 
  
	/* Found a PHB, now figure out where his registers are mapped. */
	ui_ptr = (unsigned int *) get_property(dev, "reg", &len);
	if(ui_ptr == NULL) {
		PPCDBG(PPCDBG_PHBINIT, "\tget reg failed.\n"); 
		return(NULL);
	}

	if(addr_size_words == 1) {
		reg_struct.address = ((struct reg_property32 *)ui_ptr)->address;
		reg_struct.size    = ((struct reg_property32 *)ui_ptr)->size;
	} else {
		reg_struct = *((struct reg_property64 *)ui_ptr);
	}

	PPCDBG(PPCDBG_PHBINIT, "\treg_struct.address = 0x%lx\n", reg_struct.address);
	PPCDBG(PPCDBG_PHBINIT, "\treg_struct.size    = 0x%lx\n", reg_struct.size); 

	/***************************************************************
	* Set chip specific data in the phb, including types & 
	* register pointers.
	***************************************************************/

	/****************************************************************
	* Python
	***************************************************************/
	if(strstr(model, "Python")) {
		PPCDBG(PPCDBG_PHBINIT, "\tCreate python\n");
	        phb = pci_alloc_pci_controller("PHB PY",phb_type_python);
		if(phb == NULL) return NULL;
	
       		phb->cfg_addr = (volatile unsigned long *) 
			ioremap(reg_struct.address + 0xf8000, PAGE_SIZE);
		PPCDBG(PPCDBG_PHBINIT, "\tcfg_addr_r = 0x%lx\n", 
		       reg_struct.address + 0xf8000);
		PPCDBG(PPCDBG_PHBINIT, "\tcfg_addr_v = 0x%lx\n", 
		       phb->cfg_addr);
		phb->cfg_data = (char*)(phb->cfg_addr + 0x02);
       		phb->phb_regs = (volatile unsigned long *) 
			ioremap(reg_struct.address + 0xf7000, PAGE_SIZE);
		/* Python's register file is 1 MB in size. */
		phb->chip_regs = ioremap(reg_struct.address & ~(0xfffffUL), 
					 0x100000); 
	/***************************************************************
	* Speedwagon
	***************************************************************/
	} else if(strstr(model, "Speedwagon")) {
		PPCDBG(PPCDBG_PHBINIT, "\tCreate speedwagon\n");
	        phb = pci_alloc_pci_controller("PHB SW",phb_type_speedwagon);
		if(phb == NULL) return NULL;

       		phb->cfg_addr = (volatile unsigned long *) 
			ioremap(reg_struct.address + 0x140, PAGE_SIZE);
		phb->cfg_data = (char*)(phb->cfg_addr - 0x02); /* minus is correct */
       		phb->phb_regs = (volatile unsigned long *) 
			ioremap(reg_struct.address, PAGE_SIZE);

		phb->local_number = ((reg_struct.address >> 12) & 0xf) - 0x8;

		/* Speedwagon's register file is 1 MB in size. */
		phb->chip_regs = ioremap(reg_struct.address & ~(0xfffffUL),
					 0x100000); 
		PPCDBG(PPCDBG_PHBINIT, "\tmapping chip_regs from 0x%lx -> 0x%lx\n", 
		       reg_struct.address & 0xfffff, phb->chip_regs);
	/***************************************************************
	* Trying to build a known just gets the code in trouble.
	***************************************************************/
	} else { 
		PPCDBG(PPCDBG_PHBINIT, "\tUnknown PHB Type!\n");
		printk("PCI: Unknown Phb Type!\n");
		return NULL;
	}

	bus_range = (int *) get_property(dev, "bus-range", &len);
	if (bus_range == NULL || len < 2 * sizeof(int)) {
		PPCDBG(PPCDBG_PHBINIT, "Can't get bus-range for %s\n", dev->full_name);
		kfree(phb);
		return(NULL);
	}

	/***************************************************************
	* Finished with the initialization
	***************************************************************/
	phb->first_busno =  bus_range[0];
	phb->last_busno  =  bus_range[1];
	//phb->first_busno = (phb->global_number <<8) + bus_range[0];
	//phb->last_busno  = (phb->global_number <<8) + bus_range[1];

	phb->arch_data   = dev;
	if( Pci_Large_Bus_System == 0 ) 	phb->ops = &rtas_pci_ops;
	else                             phb->ops = &rtas64_pci_ops;

	/***************************************************************
	* Build tce table for phb
	***************************************************************/
	phb_tce_table_init(phb);

	/* Dump PHB information for Debug */
	PPCDBGCALL(PPCDBG_PHBINIT,dumpPci_Controller(phb) );

	return phb;
}

void 
fixup_resources(struct pci_dev *dev) {
 	int i;
 	unsigned long size;
 	struct pci_controller* phb = (struct pci_controller *)dev->sysdata;

	PPCDBG(PPCDBG_PHBINIT, "fixup_resources:\n"); 
	PPCDBG(PPCDBG_PHBINIT, "\tphb                 = 0x%016LX\n", phb); 
	PPCDBG(PPCDBG_PHBINIT, "\tphb->pci_io_offset  = 0x%016LX\n", phb->pci_io_offset); 
	PPCDBG(PPCDBG_PHBINIT, "\tphb->pci_mem_offset = 0x%016LX\n", phb->pci_mem_offset); 

	PPCDBG(PPCDBG_PHBINIT, "\tdev->name   = %s\n", dev->name);
	PPCDBG(PPCDBG_PHBINIT, "\tdev->vendor:device = 0x%04X : 0x%04X\n", dev->vendor, dev->device);

	if(phb == NULL) return;

 	for (i = 0; i < 6; ++i) {
		PPCDBG(PPCDBG_PHBINIT, "\tdevice %x.%x[%d] (flags %x) [%lx..%lx]\n",
			    dev->bus->number, dev->devfn, i,
			    dev->resource[i].flags,
			    dev->resource[i].start,
			    dev->resource[i].end);

		if ((dev->resource[i].start == 0) && (dev->resource[i].end == 0)) {
			continue;
		}

		if (dev->resource[i].flags & IORESOURCE_IO) {
			dev->resource[i].start += phb->pci_io_offset;
			dev->resource[i].end += phb->pci_io_offset;
			size =  dev->resource[i].end -  dev->resource[i].start;
			dev->resource[i].start = 
				((unsigned long) ioremap(dev->resource[i].start, 
							 size)) - isa_io_base;
			dev->resource[i].end = dev->resource[i].start + size;  

			PPCDBG(PPCDBG_PHBINIT, "\t\t-> now [%lx (%lx) .. %lx (%lx)]\n",
			       dev->resource[i].start, ___pa(dev->resource[i].start + isa_io_base),
			       dev->resource[i].end, ___pa(dev->resource[i].end + isa_io_base));
		} else if (dev->resource[i].flags & IORESOURCE_MEM) {
			dev->resource[i].start += phb->pci_mem_offset;
			dev->resource[i].end += phb->pci_mem_offset;
			PPCDBG(PPCDBG_PHBINIT, "\t\t-> now [%lx..%lx]\n",
			       dev->resource[i].start, dev->resource[i].end);

		} else {
			continue;
		}

 		/* zap the 2nd function of the winbond chip */
 		if (dev->resource[i].flags & IORESOURCE_IO
 		    && dev->bus->number == 0 && dev->devfn == 0x81)
 			dev->resource[i].flags &= ~IORESOURCE_IO;
 	}
}   
 
void __init
pSeries_pcibios_fixup(void) {
	struct pci_dev *dev;

	PPCDBG(PPCDBG_PHBINIT, "pSeries_pcibios_fixup: start\n");
	pci_assign_all_busses = 0;

	pci_for_each_dev(dev) {
		PPCDBGCALL(PPCDBG_PHBINIT, dumpPci_Dev(dev) );
	}

	if(naca->interrupt_controller == IC_PPC_XIC) {
		xics_isa_init(); 
	}
}

/*********************************************************************** 
 * pci_find_hose_for_OF_device
 *
 * This function finds the PHB that matching device_node in the 
 * OpenFirmware by scanning all the pci_controllers.
 * 
 ***********************************************************************/
struct pci_controller*
pci_find_hose_for_OF_device(struct device_node* node) {
	while(node) {
		struct pci_controller* hose;
		for (hose=hose_head;hose;hose=hose->next)
			if (hose->arch_data == node)
				return hose;
		node=node->parent;
	}
	return NULL;
}

/*********************************************************************** 
 *
 * scan_OF_childs_for_device
 *
 * The function is a helper function for the pci_device_to_OF_node.  It 
 * walks down the passed node, looking for a node entry that matches the
 * requested bus and device function.  NOTE: If a bridge is found in the
 * scan, it will recurse this the function to to scan that bridge looking
 * for a match.  If none found, the return will continue down the orginal
 * tree.  
 * 
 * Return:
 *  Node matching the bus and devfn passed or NULL.
 ***********************************************************************/
static struct device_node*
scan_OF_childs_for_device(struct device_node* node, u8 bus, u8 dev_fn)
{
	struct device_node* CurrentNode;	 /* Current node being scanned.   */
	struct device_node* DeviceNode;   /* Node of Device               */
	u32*                Register;	 /* Pointer to Register Array     */
	u32*                Class_Code;   /* Pointer to ClassCode property */
	CurrentNode = node;	
	DeviceNode  = NULL;
	while(CurrentNode != NULL && DeviceNode == NULL) {
		u32 IoaAddress;    
		u8  IoaBus, IoaDevFn;

		Register = (unsigned int *) get_property(CurrentNode,"reg", 0);
		if(Register != NULL) {
			/* 1st register entry is the Ioa Address = 00BBSS00  */
			IoaAddress = Register[0];
			IoaBus   = (IoaAddress & 0x00FF0000) >> 16;
			IoaDevFn = (IoaAddress & 0x0000FF00) >>  8;
			if( (IoaBus  == bus) && (IoaDevFn == dev_fn ) ) {
				DeviceNode = CurrentNode;
				return DeviceNode;
			}
		}
		/*************************************************************** 
		* check for a bridge, if so scan the branch of the tree for a match.
		***************************************************************/
		Class_Code = (unsigned int*) get_property(CurrentNode,"class-code", 0);
		if(Class_Code != NULL) {
			u32 PciClassCode = ((*Class_Code)&0x00FFFF00)>>8;

			if(( PciClassCode == PCI_CLASS_BRIDGE_PCI) ||  
			   ( PciClassCode == PCI_CLASS_BRIDGE_CARDBUS) ) {

				PPCDBG(PPCDBG_BUSWALK,"\tScan OF behind bridge\n");
				DeviceNode = scan_OF_childs_for_device(CurrentNode->child, bus, dev_fn);
				if(DeviceNode != NULL) {
					return DeviceNode;
				}
			}
		}
		/*************************************************************** 
		* Try the next node.
		***************************************************************/
		CurrentNode = CurrentNode->sibling;	
	}
	return NULL;
}
/*********************************************************************** 
 * pci_device_to_OF_node
 *  
 * This function Finds the Open Firmware node for the passed in pci_dev.
 * It starts at the Phb's node for the device and calls the 
 * scan_OF_childs_for_device to looking for the matching entry.
 *
 * Return:
 *  Node matching the device or NULL if it was not found.
 ***********************************************************************/
struct device_node* 
pci_device_to_OF_node(struct pci_dev* Pci_Dev)
{
	struct pci_controller* Phb;
	struct device_node*    PhbNode;
	struct device_node*    DeviceNode;
	Phb        = PCI_GET_PHB_PTR(Pci_Dev);
	PhbNode    = (struct device_node *) Phb->arch_data;
	DeviceNode = scan_OF_childs_for_device(PhbNode->child, PCI_GET_BUS_NUMBER(Pci_Dev), Pci_Dev->devfn);
	return DeviceNode;
}
/***********************************************************************
 * find_floppy(void) 
 *	
 * Finds the default floppy device, if the system has one, and returns 
 * the pci_dev for the isa bridge for the floppy device.  
 *
 * Note: This functions finds the first "fdc" device and then looks to 
 * the parent device which should be the isa bridge device.  If there 
 * is more than one floppy on the system, it will find the first one 
 * and maybe that is okay. 
 ***********************************************************************/
struct pci_dev*
find_floppy() {	
	struct device_node *FloppyParent, *FloppyNode;
	struct pci_dev     *floppy_dev = NULL;
	int   *Register; 

	FloppyNode = find_type_devices("fdc");
	if(FloppyNode != NULL && FloppyNode->parent != NULL) {
		FloppyParent = FloppyNode->parent;
		Register = (unsigned int *)get_property(FloppyParent,"reg", 0);
		if(Register != NULL) {
			u8 IoaBus   = (Register[0] & 0x00FF0000) >> 16;
			u8 IoaDevFn = (Register[0] & 0x0000FF00) >>  8;
			floppy_dev  = pci_find_slot(IoaBus, IoaDevFn);
		}
	}
	PPCDBG(PPCDBG_BUSWALK,"\tFloppy pci_dev\n");
	PPCDBGCALL(PPCDBG_BUSWALK, dumpPci_Dev(floppy_dev) );
	return floppy_dev;
}

/*********************************************************************** 
 * ppc64_pcibios_init
 *  
 * Chance to initialize and structures or variable before PCI Bus walk.
 *  
 ***********************************************************************/
void 
pSeries_pcibios_init(void) {
	PPCDBG(PPCDBG_PHBINIT, "\tppc64_pcibios_init Entry.\n"); 

	if(get_property(find_path_device("/rtas"),"read-pc-config",NULL) != NULL) {
		PPCDBG(PPCDBG_PHBINIT, "\tFound: read-pc-config\n"); 
	}
	else PPCDBG(PPCDBG_PHBINIT, "\tNOT Found: read-pc-config\n"); 
	

	if(get_property(find_path_device("/rtas"),"ibm,read-pc-config",NULL) != NULL) {
		PPCDBG(PPCDBG_PHBINIT, "\tFound: ibm,read-pc-config\n"); 
		Pci_Set_IOA_Address  = 1;
	}
	if(get_property(find_path_device("/rtas"),"ibm,fw-phb-id",NULL) != NULL) {
		PPCDBG(PPCDBG_PHBINIT, "\tFound: ibm,fw-phb-id\n"); 
		Pci_Large_Bus_System = 1;
	}
}
