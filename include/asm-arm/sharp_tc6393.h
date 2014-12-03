/*
 *  linux/include/asm-arm/sharp_tc6393.h
 *  
 *  Copyright (C) 2003  SHARP
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *  
 *  May be copied or modified under the terms of the GNU General Public
 *  License.  See linux/COPYING for more information.
 *
 * ChangeLog:
 *   05-Aug-2003 SHARP Corporation
 *
 */

#ifndef _TC6393XB_H_
#define _TC6393XB_H_

//TC6393XB Resource Area Map (Offset)
//  System Configration Register Area         0x00000000 - 0x000000FF (Size 0x00000100)
//  NAND Flash Host Controller Register Area  0x00000100 - 0x000001FF (Size 0x00000100)
//  USB Host Controller Register Area         0x00000300 - 0x000003FF (Size 0x00000100)
//  LCD Host Controller Register Area         0x00000500 - 0x000005FF (Size 0x00000100)
//  NAND Flash Control Register               0x00001000 - 0x00001007 (Size 0x00000008)
//  USB Control Register                      0x00003000 - 0x000031FF (Size 0x00000200)
//  LCD Control Register                      0x00005000 - 0x000051FF (Size 0x00000200)
//  Local Memory 0 (32KB)                     0x00010000 - 0x00017FFF (Size 0x00008000)
//  Local Memory 0 (32KB) alias               0x00018000 - 0x0001FFFF (Size 0x00008000)
//  Local Memory 1 (1MB)                      0x00100000 - 0x001FFFFF (Size 0x00100000)


#define TC6393XB_BASE		(0xf1000000)

typedef struct {
    volatile unsigned char	reserved1[8];
    // Revision ID
    volatile unsigned char	warid;
    volatile unsigned char	reserved2[71];
    // Interrupt Status
    volatile unsigned char	r_itrsts;
    volatile unsigned char	reserved3;
    // Interrupt Mask
    volatile unsigned char	r_itrmsk;
    volatile unsigned char	reserved4;
    // Interrupt Routing
    volatile unsigned char	r_itrrot;
    volatile unsigned char	reserved5[11];
    // GP Enable
    volatile unsigned char	gpioen;
    // GP Alternative Enable
    volatile unsigned char	gpaioen;
    volatile unsigned char	reserved6[2];
    // GPI Status 0
    volatile unsigned char	gpists0;
    // GPI Status 1
    volatile unsigned char	gpists1;
    // GPI Status 2
    volatile unsigned char	gpists2;
    volatile unsigned char	reserved7;
    // GPI INT Mask 0
    volatile unsigned char	gpintmsk0;
    // GPI INT Mask 1
    volatile unsigned char	gpintmsk1;
    // GPI INT Mask 2
    volatile unsigned char	gpintmsk2;
    volatile unsigned char	reserved8;
    // GPI Edge Detect Enable 0
    volatile unsigned char	gpedgen0;
    // GPI Edge Detect Enable 1
    volatile unsigned char	gpedgen1;
    // GPI Edge Detect Enable 2
    volatile unsigned char	gpedgen2;
    volatile unsigned char	reserved9;
    // GPI Level Invert 0
    volatile unsigned char	gplvinv0;
    // GPI Level Invert 1
    volatile unsigned char	gplvinv1;
    // GPI Level Invert 2
    volatile unsigned char	gplvinv2;
    volatile unsigned char	reserved10[5];
    // GPO Data set 0
    volatile unsigned char	gpodt0;
    // GPO Data set 1
    volatile unsigned char	gpodt1;
    // GPO Data set 2
    volatile unsigned char	gpodt2;
    volatile unsigned char	reserved11;
    // GPO Data OE Contorol 0
    volatile unsigned char	gpooe0;
    // GPO Data OE Contorol 1
    volatile unsigned char	gpooe1;
    // GPO Data OE Contorol 2
    volatile unsigned char	gpooe2;
    volatile unsigned char	reserved12;
    // GP Internal Active Register Contorol 0
    volatile unsigned char	gpapulc0;
    // GP Internal Active Register Contorol 1
    volatile unsigned char	gpapulc1;
    // GP Internal Active Register Contorol 2
    volatile unsigned char	gpIapulc2;
    volatile unsigned char	reserved13;
    // GP Internal Active Register Level Contorol 0
    volatile unsigned char	gparlv0;
    // GP Internal Active Register Level Contorol 1
    volatile unsigned char	gparlv1;
    // GP Internal Active Register Level Contorol 2
    volatile unsigned char	gparlv2;
    volatile unsigned char	reserved14;
    // GPI Buffer Contorol 0
    volatile unsigned char	gpibfc0;
    // GPI Buffer Contorol 1
    volatile unsigned char	gpibfc1;
    // GPI Buffer Contorol 2
    volatile unsigned char	gpibfc2;
    volatile unsigned char	reserved15;
    // GPa Internal Activ Register Contorol 0
    volatile unsigned char	gpaapulc0;
    // GPa Internal Activ Register Contorol 1
    volatile unsigned char	gpaapulc1;
    volatile unsigned char	reserved16[2];
    // GPa Internal Activ Register Level Contorol 0
    volatile unsigned char	gpaarlv0;
    // GPa Internal Activ Register Level Contorol 1
    volatile unsigned char	gpaarlv1;
    volatile unsigned char	reserved17[2];
    // GPa Buffer Contorol 0
    volatile unsigned char	gpaibfc0;
    // GPa Buffer Contorol 1
    volatile unsigned char	gpaibfc1;
    volatile unsigned char	reserved18[2];
    // Clock Control
    volatile unsigned short	clkctl;
    // PLL2 Control
    volatile unsigned short	pll2ctl;
    // PLL1 Control
    volatile unsigned short	pll1ctl;
    // Device Internal Avtive Register Contorol
    volatile unsigned char	diarctl;
    // Device Buffer Contorol
    volatile unsigned char	dvbctl;
    volatile unsigned char	reserved19[62];
    // Function Enable
    volatile unsigned char	funcebl;
    volatile unsigned char	reserved20[3];
    // Mode Contorol 0
    volatile unsigned char	modectl0;
    // Mode Contorol 1
    volatile unsigned char	modectl1;
    volatile unsigned char	reserved21[22];
    // Configuraation Control
    volatile unsigned char	cfctl;
    volatile unsigned char	reserved22[2];
    // Debug
    volatile unsigned char	debug;
} TC6393XB_SysConfig;

#define TC6393XB_SYSCONFIG_BASE		((TC6393XB_SysConfig *)TC6393XB_BASE)

typedef struct {
    //  Vendor ID Register
    volatile unsigned short	spvid;
    //  Device ID Register
    volatile unsigned short	spdid;
    //  Command Register
    volatile unsigned short	spcmd;
    //  Status Register
    volatile unsigned short	spst; 
    //  Revision ID Register / Class Code Register
    volatile unsigned short	sprid_spcc;
    volatile unsigned char	reserved1[4];
    //  Header Tyep Register
    volatile unsigned char	spht;
    volatile unsigned char	reserved2;
    //  SmartMedia Controller Register Base Address Register
    union {
	volatile unsigned short	spba1;
	volatile unsigned short	spba2;
    } spba;
    volatile unsigned char	reserved3[22];
    //  CIS Pointer Register
    volatile unsigned short	scisp;
    //  Subsystem Vender ID Register
    volatile unsigned short	spsvid;
    //  Subsystem Deviece ID Register
    volatile unsigned short	spsdid;
    volatile unsigned char	reserved4[4];
    //  Capability Pointer Register
    volatile unsigned char	sid;
    volatile unsigned char	reserved5[7];
    //  Interrupt Ling Register
    volatile unsigned char	spitrl;
    //  Interrupt Pin Register
    volatile unsigned char	spitrp;
    volatile unsigned char	reserved6[10];
    //  INT Enable Register
    volatile unsigned char	spintc;
    //  PME Enable Register
    volatile unsigned char	sppmec;
    //  Event Control Register
    volatile unsigned char	sevntcnt;
    volatile unsigned char	reserved7;
    //  CLKRUN Control Register
    volatile unsigned char	spcrunc;
    volatile unsigned char	reserved8[14];
    //  Debug Register
    volatile unsigned char	spdbg;
    volatile unsigned char	reserved9[4];
    //  SmartMedia Transaction Control Register
    volatile unsigned char	smtrcnt;
    //  SmartMedia Monitor Register
    volatile unsigned char	smsts;
    //  SmartMedia Power Supply Control Register
    volatile unsigned char	ssmpwc;
    //  SmartMedia Detect Control Register
    volatile unsigned char	ssmdtc;
    volatile unsigned char	reserved10[28];
    //  Capability ID Register
    volatile unsigned char	svcid;
    //  Next Item Ptr Register
    volatile unsigned char	svniptr;
    //  Power Management Capabilities (PMC) Register
    volatile unsigned short	svpmc;
    //  Power Management Control/Status (PMCSR) Register
    volatile unsigned short	svpmcs;
    //  PMCSR PCI to PCI Bridge Support Extensions Register
    volatile unsigned char	svppcbs;
    //  Data Register
    volatile unsigned char	svdata;
    volatile unsigned char	reserved11[24];
    //  CIS Register	SCISRx
    volatile unsigned char	scisr[80];
    //  ROM Data Port Register
    volatile unsigned short	srmdp;
    //  ROM Index Port Register
    volatile unsigned char	srmip;
    //  ROM Control Register
    volatile unsigned char	srmcr;
    volatile unsigned char	reserved12[8];
    //  Configration Control Register
    volatile unsigned char	sppcnf;
    volatile unsigned char	reserved13[2];
    //  Monitor Select Register
    volatile unsigned char	pfadb;
} TC6393XB_NandConfig;

#define TC6393XB_NANDCONFIG_BASE	((TC6393XB_NandConfig *)(TC6393XB_BASE+0x100))


typedef struct {
    union {
	volatile struct {
	    unsigned char	sdata0;	// Data Register0
	    unsigned char	sdata1;	// Data Register1
	    unsigned char	sdata2;	// Data Register2
	    unsigned char	sdata3;	// Data Register3
	} data8;
	volatile struct {
	    unsigned short	sdata0_1; // Data Register0,1
	    unsigned short	sdata2_3; // Data Register2,3
	} data16;
	volatile struct {
	    unsigned long	sdata0_3; // Data Register 0,1,2,3
	} data32;
    } sdata;
    // Mode Register
    volatile unsigned char	smode;
    // Status Register
    volatile unsigned char	sustus;
    // Interrupt Status Register
    volatile unsigned char	sintst;
    // Interrupt Mask Register
    volatile unsigned char	sintmsk;
} TC6393XB_NandCtrl;

#define TC6393XB_NANDCTRL_OFFSET	0x1000
#define TC6393XB_NANDCTRL_BASE		((TC6393XB_NandCtrl *)(TC6393XB_BASE+TC6393XB_NANDCTRL_OFFSET))


/* SMODE Register Command List */
#define SMODE_READ_COMMAND		0x15	// DataRead Command_Mode
#define SMODE_READ_ADDRESS		0x16	// DataRead Address_Mode
#define SMODE_READ_DATAREAD		0x14	// DataRead Data_Mode
#define SMODE_WRITE_COMMAND		0x95	// DataWrite Command_Mode
#define SMODE_WRITE_ADDRESS		0x96	// DataWrite Address_Mode
#define SMODE_WRITE_DATAWRITE		0x94	// DataWrite Data_Mode

#define SMODE_POWER_ON			0x0C	// Power Supply ON to SSFDC card
#define SMODE_POWER_OFF			0x08	// Power Supply OFF to SSFDC card

#define SMODE_LED_OFF			0x00	// LED OFF
#define SMODE_LED_ON			0x04	// LED ON

#define SMODE_EJECT_ON			0x68	// Ejection Demand from Penguin is Advanced
#define SMODE_EJECT_OFF			0x08	// Ejection Demand from Penguin is Not Advanced

#define SMODE_LOCK			0x6C	// Operates By Lock_Mode. Ejection Switch is Invalid
#define SMODE_UNLOCK			0x0C	// Operates By UnLock_Mode.Ejection Switch is Effective

#define SMODE_HWECC_READ_ECCCALC	0x34	// HW-ECC DataRead
#define SMODE_HWECC_READ_RESET_ECC	0x74	// HW-ECC ResetMode
#define SMODE_HWECC_READ_CALC_RESULT	0x54	// HW-ECC Calculation Result Read_Mode

#define SMODE_HWECC_WRITE_ECCCALC	0xB4	// HW-ECC DataWrite
#define SMODE_HWECC_WRITE_RESET_ECC	0xF4	// HW-ECC ResetMode
#define SMODE_HWECC_WRITE_CALC_RESULT	0xD4	// HW-ECC Calculation Result Read_Mode

#define SMODE_CONTROLLER_ID_READ	0x40	// Controller ID Read
#define SMODE_STANDBY			0x00	// SSFDC card Changes Standby State

#define SMODE_WE			0x80
#define SMODE_ECC1			0x40
#define SMODE_ECC0			0x20
#define SMODE_CE			0x10
#define SMODE_PCNT1			0x08
#define SMODE_PCNT0			0x04
#define SMODE_ALE			0x02
#define SMODE_CLE			0x01

#define SMODE_SELECT			(SMODE_WE|SMODE_CE|SMODE_PCNT0)
#define SMODE_DESELECT			0x00 //Changes Standby State

/* SUSTUS Register */
#define SUSTUS_BUSY			0x80

#endif
