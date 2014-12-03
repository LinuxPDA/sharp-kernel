/*
 * iSeries_pci.c
 *
 * Copyright (C) 2001 Allan Trautman, IBM Corporation
 *
 * iSeries specific routines for PCI.
 * 
 * Based on code from pci.c and iSeries_pci.c 32bit
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
#include <linux/list.h> 
#include <linux/string.h>
#include <linux/init.h>
#include <linux/ide.h>
#include <linux/bootmem.h>
#include <linux/pci.h>

#include <asm/io.h>
#include <asm/pgtable.h>
#include <asm/irq.h>
#include <asm/prom.h>
//#include <asm/gg2.h>
#include <asm/machdep.h>
#include <asm/init.h>
#include <asm/pci-bridge.h>
#include <asm/ppcdebug.h>
#include <asm/Naca.h>
//#include <asm/pci_dma.h>
#include <asm/iSeries/iSeries_dma.h>

#include <asm/flight_recorder.h>

#include <asm/iSeries/HvCallXm.h>
#include <asm/iSeries/HvCallSm.h>
#include <asm/iSeries/HvCallPci.h>
#include <asm/iSeries/LparData.h>
#include <asm/iSeries/iSeries_irq.h>
#include <asm/iSeries/iSeries_pci.h>

#include "pci.h"

extern struct pci_controller* hose_head;
extern struct pci_controller** hose_tail;
extern struct Naca *naca;
extern struct device_node *allnodes;
extern unsigned long phb_tce_table_init(struct pci_controller *phb);

extern struct pci_ops iSeries_pci_ops;
extern struct flightRecorder* PciFr;

int    PciTraceFlag = 0;

struct pci_dev;

struct i_device_node {
	struct list_head iDevice_Chain;
	char*           Name;
	char*           Type;
	struct pci_dev* PciDev;
	int             BusNumber;
	int             SubBus;
	int             ReturnCode;
	int             DevFn;
};

LIST_HEAD(iDevice_List);

/*******************************************************************
 * Forward declares of prototypes. 
 *******************************************************************/
unsigned long find_and_init_phbs(void);
void          fixup_resources(struct pci_dev *dev);
void          iSeries_pcibios_fixup(void);
struct        pci_controller* alloc_phb(struct device_node *dev, char *model, unsigned int addr_size_words) ;

void  iSeries_Scan_PHBs_Slots(struct pci_controller* Phb);
void  iSeries_Scan_EADs_Bridge(HvBusNumber Bus, HvSubBusNumber SubBus, int IdSel);
int   iSeries_Scan_Bridge_Slot(HvBusNumber Bus, HvSubBusNumber SubBus, int MaxAgents);

/**********************************************************************************
 *
 *
 **********************************************************************************/
#define ISERIES_PCI_READ_OP(size, call, type) \
/***********************************************************************************/ \
int iSeries_pci_read_config_##size(struct pci_dev* PciDev, int PciOffset, type ReadValue) { \
	struct i_device_node* DeviceNode = PciDev->sysdata; \
	if(DeviceNode->BusNumber == 0xFF) DeviceNode->ReturnCode = 0x301; return DeviceNode->ReturnCode; \
	DeviceNode->ReturnCode = HvCallPci_config##call(DeviceNode->BusNumber, DeviceNode->SubBus, \
                                                     DeviceNode->DevFn, PciOffset, ReadValue); \
	if(DeviceNode->ReturnCode != 0 ) { \
		PCIFR("RC##size: %02X,%02X,%02X,%04X Rtn: %04X", \
		                  DeviceNode->BusNumber, DeviceNode->SubBus, DeviceNode->DevFn, PciOffset,DeviceNode->ReturnCode); \
	} \
	return DeviceNode->ReturnCode; \
}
/***********************************************************************************/ \
#define ISERIES_PCI_WRITE_OP(size, call, type) \
/***********************************************************************************/ \
int iSeries_pci_write_config_##size(struct pci_dev* PciDev, int PciOffset, type WriteValue) { \
	struct i_device_node* DeviceNode = PciDev->sysdata; \
	DeviceNode->ReturnCode = HvCallPci_config##call(DeviceNode->BusNumber, DeviceNode->SubBus, \
                                                     DeviceNode->DevFn, PciOffset, WriteValue); \
	if(DeviceNode->ReturnCode != 0 ) { \
		PCIFR("RC##size: %02X,%02X,%02X,%04X Rtn: %04X", \
		                  DeviceNode->BusNumber, DeviceNode->SubBus, DeviceNode->DevFn, PciOffset,DeviceNode->ReturnCode); \
	} \
	return DeviceNode->ReturnCode; \
}

  ISERIES_PCI_READ_OP( byte, Load8,  u8*)
  ISERIES_PCI_READ_OP( word, Load16, u16*)
  ISERIES_PCI_READ_OP( dword,Load32, u32*)
  ISERIES_PCI_WRITE_OP(byte, Store8, u8)
  ISERIES_PCI_WRITE_OP(word, Store16,u16)
  ISERIES_PCI_WRITE_OP(dword,Store32,u32)

/************************************************************************/
/* Branch Table                                                         */
/************************************************************************/
struct pci_ops iSeries_pci_ops = {
	iSeries_pci_read_config_byte,
	iSeries_pci_read_config_word,
	iSeries_pci_read_config_dword,
	iSeries_pci_write_config_byte,
	iSeries_pci_write_config_word,
	iSeries_pci_write_config_dword 
};
	

/****************************************************************************
 *
 * unsigned int __init find_and_init_phbs(void)
 *
 * Description:
 *   This function checks for all possible system PCI host bridges that connect
 *   PCI buses.  The system hypervisor is queried as to the guest partition
 *   ownership status.  A pci_controller is build for any bus which is partially
 *   owned or fully owned by this guest partition.
 ****************************************************************************/
unsigned long __init find_and_init_phbs(void) {
    struct      pci_controller* hose;
    HvBusNumber BusNumber;
    int         LxBusNumber = 0;		/* Linux Bus number for grins */

	PPCDBG(PPCDBG_BUSWALK,__FUNCTION__" Entry\n");

    /* Check to make sure the device probing will work on this iSeries Release. */
    if(hvReleaseData.xVrmIndex !=3) {
		printk("PCI: iSeries Lpar and Linux native PCI I/O code is incompatible.\n");
		printk("PCI: A newer version of the Linux kernel is need for this iSeries release.\n");
		return -1;
    }
    /* Check all possible buses. */
	for (BusNumber = 0; BusNumber < 256; BusNumber++) { 
		int RtnCode = HvCallXm_testBus(BusNumber);
		if (RtnCode == 0) {
			PPCDBG(PPCDBG_BUSWALK, "Create iSeries PHB controller: %04X\n",BusNumber);
			PCIFR("Create iSeries PHB controller: %04X",BusNumber);

			PPCDBG(PPCDBG_BUSWALK, "PCI: Allocate pci_controller for PBH HV\n");
			hose = (struct pci_controller *)kmalloc(sizeof(struct pci_controller),GFP_KERNEL);
			if(hose == NULL) {
				printk("PCI: Allocate pci_controller failed.\n");
				PCIFR( "PCI: Allocate pci_controller failed.\n");
				return -1;		
			}
			memset(hose, 0, sizeof(struct pci_controller));
        		*hose_tail = hose;
			hose_tail = &hose->next;
			strcpy(hose->what,"PHB HV");
			hose->type = phb_type_hypervisor;
			hose->global_number = BusNumber;
        		*hose_tail = hose;
			hose_tail = &hose->next;

	    		hose->local_number = BusNumber;	/* Stuff HV bus number away.            */
	    		hose->first_busno  = LxBusNumber;/* This just for debug.   pcibios will  */
	    		hose->last_busno   = 0xff;		/* assign the bus numbers later.        */
	    		hose->ops = &iSeries_pci_ops;	/* Cnfg Reg Ops routines.               */
	    		LxBusNumber += 1;			     /* Keep track for debug.                */

			/*********************/
			/* TCE Table for Bus */
			/*********************/
		    //create_pci_bus_tce_table(BusNumber);

			/*************************************************************************
			 * Find and connect the devices. 
			 *************************************************************************/
			iSeries_Scan_PHBs_Slots(hose);

		}
	}
	return 0;
}
/*********************************************************************** 
 * ppc64_pcibios_init
 *  
 * Chance to initialize and structures or variable before PCI Bus walk.
 *  
 ***********************************************************************/
void iSeries_pcibios_init(void) {
	PPCDBG(PPCDBG_BUSWALK,__FUNCTION__" Entry.\n"); 
	/* find_and_init_phbs();  comment out until debugged. */
	PPCDBG(PPCDBG_BUSWALK,__FUNCTION__" Exit.\n"); 
}

/***********************************************************************
 * iSeries_pcibios_fixup(void)  
 *	
 *	
 *	
 ***********************************************************************/
void __init iSeries_pcibios_fixup(void) {
	struct pci_dev *dev;
	PPCDBG(PPCDBG_BUSWALK,__FUNCTION__" Entry.\n"); 
	return;   /* Just return until debugged. */

	pci_assign_all_busses = 0;

	pci_for_each_dev(dev) {
		PPCDBGCALL(PPCDBG_BUSWALK, dumpPci_Dev(dev) );
	}
}
/***********************************************************************
 * find_floppy(void) 
 *	
 * Finds the default floppy device, if the system has one, and returns 
 * the pci_dev for the isa bridge for the floppy device.  
 *
 * Note: On iSeries there will only be a virtual diskette. 
 ***********************************************************************/
struct pci_dev*
find_floppy() {	
	PPCDBG(PPCDBG_BUSWALK,"\tFind Floppy pci_dev.. None on iSeries.\n");
	return NULL;
}


/***********************************************************************
 * fixup_resources(struct pci_dev *dev) 
 *	
 *	
 *	
 *	
 ***********************************************************************/
void 
fixup_resources(struct pci_dev *dev) {
 	struct pci_controller* phb = (struct pci_controller *)dev->sysdata;

	PPCDBG(PPCDBG_BUSWALK, "fixup_resources:\n"); 
	PPCDBG(PPCDBG_BUSWALK, "\tphb                 = 0x%016LX\n", phb); 
	PPCDBG(PPCDBG_BUSWALK, "\tphb->local_number   = 0x%004X \n", phb->local_number); 
}   


/***********************************************************************
 * build_device_node(u16 Bus, int SubBus, u8 DevFn)
 *
 *
 *   char*         loc-code	
 *     int           vendor-id
 *     int           device-id..
 *   int           BusNumber
 *   int           SubBus
 *   int           ReturnCode
 *	
 *	 int          regs
 *	int           interrupts 
 *   char*         loc-code	
 ***********************************************************************/
void 
build_device_node(HvBusNumber BusNumber, HvSubBusNumber  SubBus, u8 DevFn) {\
	struct i_device_node* iDevice = kmalloc(sizeof(struct i_device_node), GFP_KERNEL);
	list_add_tail(&iDevice->iDevice_Chain,&iDevice_List);



}

void list_device_nodes(void) { 
	struct list_head* iDevice_Node_Ptr = iDevice_List.next;
	int    index = 1;
	PPCDBG(PPCDBG_BUSWALK,__FUNCTION__" Entry\n");
	while(iDevice_Node_Ptr != &iDevice_List) { 
		iDevice_Node_Ptr = iDevice_Node_Ptr->next;
		++index;
	}
}

	
/********************************************************************************
* Loop through each node function to find usable bridges.  
*********************************************************************************/
void  iSeries_Scan_PHBs_Slots(struct pci_controller* Phb) {
	struct HvCallPci_DeviceInfo* DevInfo;
	HvBusNumber    Bus       = Phb->local_number;       /* System Bus        */	
	HvSubBusNumber SubBus    = 0;                       /* EADs is always 0. */
	int            HvRc      = 0;
	int            IdSel     = 1;	
	int            MaxAgents = 8;

	PPCDBG(PPCDBG_BUSWALK,__FUNCTION__" Entry\n");

	DevInfo    = (struct HvCallPci_DeviceInfo*)kmalloc(sizeof(struct HvCallPci_DeviceInfo), GFP_KERNEL);
	if(DevInfo == NULL) return;

	/********************************************************************************
	 * Probe for EADs Bridges      
	 ********************************************************************************/
	for (IdSel=1; IdSel < MaxAgents; ++IdSel) {
    		HvRc = HvCallPci_getDeviceInfo(Bus, SubBus, IdSel,REALADDR(DevInfo), sizeof(struct HvCallPci_DeviceInfo));
		if (HvRc == 0) {
			PPCDBG(PPCDBG_BUSWALK,"\tFound Device 0x%02X\n",DevInfo->deviceType);
			if(DevInfo->deviceType == HvCallPci_NodeDevice) {
				PPCDBG(PPCDBG_BUSWALK,"\tFound EADs Bridge(NodeDevice)\n");
				iSeries_Scan_EADs_Bridge(Bus, SubBus, IdSel);
			}
			else {
				printk("PCI: Invalid System Configuration. \n");
			}
		}
	}
	kfree(DevInfo);
	PPCDBG(PPCDBG_BUSWALK,__FUNCTION__" Exit\n");
}


/********************************************************************************
* 
*********************************************************************************/
void  iSeries_Scan_EADs_Bridge(HvBusNumber Bus, HvSubBusNumber SubBus, int IdSel) {
	struct HvCallPci_BridgeInfo* BridgeInfo;
	HvAgentId      AgentId;
	int            Function;
	int            HvRc;

	PPCDBG(PPCDBG_BUSWALK,__FUNCTION__" Entry\n");

	BridgeInfo = (struct HvCallPci_BridgeInfo*)kmalloc(sizeof(struct HvCallPci_BridgeInfo), GFP_KERNEL);
	if(BridgeInfo == NULL) return;

	/*********************************************************************
	 * Note: hvSubBus and irq is always be 0 at this level!
	 *********************************************************************/
	for (Function=0; Function < 8; ++Function) {
	  	AgentId = ISERIES_PCI_AGENTID(IdSel, Function);
		HvRc = HvCallXm_connectBusUnit(Bus, SubBus, AgentId, 0);
 		if (HvRc == 0) {
			PPCDBG(PPCDBG_BUSWALK,"Connect EADs: 0x%02X.%02X.%02X = 0x%02X\n",Bus, SubBus, AgentId);
	    		HvRc = HvCallPci_getBusUnitInfo(Bus, SubBus, AgentId, 
			                                REALADDR(BridgeInfo), sizeof(struct HvCallPci_BridgeInfo));
	 		if (HvRc == 0) {
				PPCDBG(PPCDBG_BUSWALK,"\tBridgeInfo..: 0x%02X.%02X.%02X = 0x%02X\n",Bus, SubBus, AgentId,
				                                                                    BridgeInfo->busUnitInfo.deviceType);

				if (BridgeInfo->busUnitInfo.deviceType == HvCallPci_BridgeDevice)  {
					PPCDBG(PPCDBG_BUSWALK,"\tScan_Bridge_Slot...: 0x%02X.%02X.%02X\n",Bus, SubBus, AgentId);
					iSeries_Scan_Bridge_Slot(Bus, BridgeInfo->subBusNumber,BridgeInfo->maxAgents);
	      		}
			}
    		}
	}
	kfree(BridgeInfo);
	PPCDBG(PPCDBG_BUSWALK,__FUNCTION__" Exit\n");
}

/********************************************************************************
* 
* This assumes that the node slot is always on the primary bus!
*********************************************************************************/
int iSeries_Scan_Bridge_Slot(HvBusNumber Bus, HvSubBusNumber SubBus, int MaxAgents) {
	u16       VendorId   = 0;
	int       HvRc       = 0;
	int       Irq        = 0;
	int       IdSel      = ISERIES_GET_DEVICE_FROM_SUBBUS(SubBus);
	int       Function   = ISERIES_GET_FUNCTION_FROM_SUBBUS(SubBus);
	HvAgentId AgentId    = ISERIES_PCI_AGENTID(IdSel, Function);
	int       ValidSlots = 0; 	

	PPCDBG(PPCDBG_BUSWALK,__FUNCTION__" Entry\n");
	
  	Irq   = iSeries_allocate_IRQ(Bus, 0, AgentId);
	PPCDBG(PPCDBG_BUSWALK,"\tiSeries_allocate_IRQ.: 0x%02X.%02X.%02X(0x%02X)\n", Bus, SubBus, AgentId, Irq);

	/****************************************************************************
	 * Connect all functions of any device found.  
	 ****************************************************************************/
  	for (IdSel = 1; IdSel <= MaxAgents; ++IdSel) {
    		for (Function = 0; Function < 8; ++Function) {
			AgentId = ISERIES_PCI_AGENTID(IdSel, Function);
      		HvRc    = HvCallXm_connectBusUnit(Bus, SubBus, AgentId, Irq);
      		if (HvRc == 0) {
				PPCDBG(PPCDBG_BUSWALK,"\tConnect....: 0x%02X.%02X.%02X Irq: 0x%02X\n",Bus, SubBus, AgentId, Irq);
				HvRc = HvCallPci_configLoad16(Bus, SubBus, AgentId, PCI_VENDOR_ID, &VendorId);  
	      		if (HvRc == 0) {
		     		PPCDBG(PPCDBG_BUSWALK,"\tVendorId...: 0x%02X.%02X.%02X = 0x%04X\n",Bus, SubBus, AgentId, VendorId);
					++ValidSlots;
				}
      		}
			if (HvRc != 0) {
				PPCDBG(PPCDBG_BUSWALK,"\tConnect/Read Vendor failed: 0x%02X.%02X.%02X = 0x%02X\n",Bus, SubBus, AgentId, HvRc);
			}
    		}
		/****************************************************************************
		 * If a device is found, assign irq to slot
		 ****************************************************************************/
    		if (ValidSlots > 0) {
			PPCDBG(PPCDBG_BUSWALK,"\tiSeries_assign_IRQ 0x%02X.%02X.%02X = 0x%02X\n",Bus, SubBus, AgentId, Irq );
      		iSeries_assign_IRQ(Irq, Bus, SubBus, AgentId);
    		}
  	}
	PPCDBG(PPCDBG_BUSWALK,__FUNCTION__" Exit\n");
	return HvRc;
}
