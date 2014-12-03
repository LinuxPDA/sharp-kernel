/*==========================================================================+
|                                                                           |
|   Title:   SysRegs.h                                                      |
|                                                                           |
|   Author:  Michael Ngo                                                    |
|                                                                           |
|   Date:    09/10/00.                                                      |
|                                                                           |
+===========================================================================+
|   $Header:
+===========================================================================+
|                                                                           |
|   Description:                                                            |
|     This file defines Triad architecture specific constants.              |
|     add all register definetion and marco for all register - Phil Le	    |
|                                                                           |
|   Special Notes:                                                          |
|                                                                           |
|                                                                           |
+==========================================================================*/

#ifndef _BRECIS_SYS_REGS_H
#define _BRECIS_SYS_REGS_H
	
/*--------------------------------------------------------------------------+
| System/Device Identification Registers
+--------------------------------------------------------------------------*/
#define	SREG_BASE	0xBC000000	/* Base address to SLM Block       */



#ifdef __ASSEMBLER__
#define	DEV_ID_REG	(SREG_BASE + 0x000)  /* Device-ID Register         */
#define	FWR_ID_REG	(SREG_BASE + 0x004)  /* Firmware-ID Register       */
#define	SYS_ID_REG0	(SREG_BASE + 0x008)  /* System-ID Register-0       */
#define	SYS_ID_REG1	(SREG_BASE + 0x00C)  /* System-ID Register-1       */
#else
#define	DEV_ID_REG	((volatile unsigned int *) (SREG_BASE + 0x000))
#define	FWR_ID_REG	((volatile unsigned int *) (SREG_BASE + 0x004))
#define	SYS_ID_REG0	((volatile unsigned int *) (SREG_BASE + 0x008))
#define	SYS_ID_REG1	((volatile unsigned int *) (SREG_BASE + 0x00C))
#endif


/*--------------------------------------------------------------------------+
| Reset Registers
+--------------------------------------------------------------------------*/
#ifdef __ASSEMBLER__
#define	SYS_RST_REG	(SREG_BASE + 0x010)  /* system Reset Status Register RO */
#define	RST_SET_REG	(SREG_BASE + 0x014)  /* System set Reset Register    WO */
#define	RST_CLR_REG	(SREG_BASE + 0x018)  /* System Clear Reset Register  WO */
#else
#define	SYS_RST_REG	((volatile unsigned int *) (SREG_BASE + 0x010))
#define	RST_SET_REG	((volatile unsigned int *) (SREG_BASE + 0x014))
#define	RST_CLR_REG	((volatile unsigned int *) (SREG_BASE + 0x018))
#endif




/*--------------------------------------------------------------------------+
| Clock Generation Registers
+--------------------------------------------------------------------------*/
#ifdef __ASSEMBLER__
#define	PCM0_CLK_REG	(SREG_BASE + 0x020)  /* PCM0 Clock Generator       */
#define	PCM1_CLK_REG	(SREG_BASE + 0x024)  /* PCM1 Clock Generator       */
#define	PCM2_CLK_REG	(SREG_BASE + 0x028)  /* PCM2 Clock Generator       */
#define PCM3_CLK_REG	(SREG_BASE + 0x02C)  /* PCM3 Clock Generator	   */
#define	PLL_DIV_REG	(SREG_BASE + 0x030)  /* PLL Divider Value          */
#define	MIPS_CLK_REG	(SREG_BASE + 0x034)  /* MIPS Clock Generator       */
#define	VE_CLK_REG	(SREG_BASE + 0x038)  /* Voice Engine Clock Gen     */
#define	FE_CLK_REG	(SREG_BASE + 0x03C)  /* Framer Engine Clock Gen    */
#define	DVB_CLK_REG	(SREG_BASE + 0x040)  /* DV-Bus Clock Generator     */
#define	SMAC_CLK_REG	(SREG_BASE + 0x044)  /* SEC & MAC Clock Generator  */
#define	PERF_CLK_REG	(SREG_BASE + 0x048)  /* Peripheral & ADPCM Clk Gen */
#else
#define	PCM0_CLK_REG	((volatile unsigned int *) (SREG_BASE + 0x020))
#define	PCM1_CLK_REG	((volatile unsigned int *) (SREG_BASE + 0x024))
#define	PCM2_CLK_REG	((volatile unsigned int *) (SREG_BASE + 0x028))
#define	PCM3_CLK_REG	((volatile unsigned int *) (SREG_BASE + 0x02C))
#define	PLL_DIV_REG	((volatile unsigned int *) (SREG_BASE + 0x030))
#define	MIPS_CLK_REG	((volatile unsigned int *) (SREG_BASE + 0x034))
#define	VE_CLK_REG	((volatile unsigned int *) (SREG_BASE + 0x038))
#define	FE_CLK_REG	((volatile unsigned int *) (SREG_BASE + 0x03C))
#define	DVB_CLK_REG	((volatile unsigned int *) (SREG_BASE + 0x040))
#define	SMAC_CLK_REG	((volatile unsigned int *) (SREG_BASE + 0x044))
#define	PERF_CLK_REG	((volatile unsigned int *) (SREG_BASE + 0x048))
#endif

/*--------------------------------------------------------------------------+
| Watchdog and Programmable Timer/Counter Registers
+--------------------------------------------------------------------------*/
#ifdef __ASSEMBLER__
#define	WD_CTL_REG	(SREG_BASE + 0x04C)  /* Watchdog Timer Control Reg */
#define	WD_SVC_REG	(SREG_BASE + 0x050)  /* Watchdog Timer Service Reg */
#define	PIT_CTL_REG	(SREG_BASE + 0x054)  /* ProgTimer Control Reg      */
#define	PIT_CT0_REG	(SREG_BASE + 0x058)  /* Programmable Timer 0 Reg   */
#define	PIT_GCT0_REG	(SREG_BASE + 0x05C)  /* Programmable Timer Gate 0  */
#define	PIT_CT1_REG	(SREG_BASE + 0x060)  /* Programmable Timer 1 Reg   */
#define	PIT_GCT1_REG	(SREG_BASE + 0x064)  /* Programmable Timer Gate 1  */
#define	PIT_CT2_REG	(SREG_BASE + 0x068)  /* Programmable Timer 2 Reg   */
#define	PIT_GCT2_REG	(SREG_BASE + 0x06C)  /* Programmable Timer Gate 2  */
#else
#define	WD_CTL_REG	((volatile unsigned int *) (SREG_BASE + 0x04C))
#define	WD_SVC_REG	((volatile unsigned int *) (SREG_BASE + 0x050))
#define	PIT_CTL_REG	((volatile unsigned int *) (SREG_BASE + 0x054))
#define	PIT_CT0_REG	((volatile unsigned int *) (SREG_BASE + 0x058))
#define	PIT_GCT0_REG	((volatile unsigned int *) (SREG_BASE + 0x05C))
#define	PIT_CT1_REG	((volatile unsigned int *) (SREG_BASE + 0x060))
#define	PIT_GCT1_REG	((volatile unsigned int *) (SREG_BASE + 0x064))
#define	PIT_CT2_REG	((volatile unsigned int *) (SREG_BASE + 0x068))
#define	PIT_GCT2_REG	((volatile unsigned int *) (SREG_BASE + 0x06C))
#endif

/*--------------------------------------------------------------------------+
| Interrupt Controller
+--------------------------------------------------------------------------*/
#ifdef __ASSEMBLER__
#define	INT_STA_REG	(SREG_BASE + 0x070)  /* Interrupt Status Register  */
#define	INT_MSK_REG	(SREG_BASE + 0x074)  /* Interrupt Mask Register    */
#else
#define	INT_STA_REG	((volatile unsigned int *) (SREG_BASE + 0x070))
#define	INT_MSK_REG	((volatile unsigned int *) (SREG_BASE + 0x074))
#endif


/*--------------------------------------------------------------------------+
| Voice and Framer Mailbox.
+--------------------------------------------------------------------------*/
#ifdef __ASSEMBLER__
#define	VE_MBOX_REG	(SREG_BASE + 0x078)  /* Voice Engine Mailbox Reg   */
#define	FE_MBOX_REG	(SREG_BASE + 0x07C)  /* Framer Engine Mailbox Reg  */
#else
#define	VE_MBOX_REG	((volatile unsigned int *) (SREG_BASE + 0x078))
#define	FE_MBOX_REG	((volatile unsigned int *) (SREG_BASE + 0x07C))
#endif



/*--------------------------------------------------------------------------+
| System Error Registers.
+--------------------------------------------------------------------------*/
#ifdef __ASSEMBLER__
#define	SER_STA_REG	(SREG_BASE + 0x150)  /* System Error Status Reg    */
#define	SER_H1L_REG	(SREG_BASE + 0x154)  /* System Error Status Reg    */
#define	SER_H2L_REG	(SREG_BASE + 0x158)  /* System Error Status Reg    */
#define	SYS_IE_REG	(SREG_BASE + 0x15C)  /* System Interrupt Configuration    */
#else
#define	SER_STA_REG	((volatile unsigned int *) (SREG_BASE + 0x150))
#define	SER_H1L_REG	((volatile unsigned int *) (SREG_BASE + 0x154))
#define	SER_H2L_REG	((volatile unsigned int *) (SREG_BASE + 0x158))
#define	SYS_IE_REG	((volatile unsigned int *) (SREG_BASE + 0x15C))
#endif


/*------------------------------------------------------------------------*/
/* Register access for All triad Register				  */
/*------------------------------------------------------------------------*/
#ifdef __ASSEMBLER__
#define WRITE_TRIAD_REG(reg,value)	\
	li	t0,reg;			\
	li	t1,reg;			\
	sw	t1,0(t0)
#define READ_TRIAD_REG(reg)		\
	li	t0,reg;			\
	lw	t1,0(t0)
#else
#define WRITE_TRIAD_REG(reg,value)	(*reg = value)
#define READ_TRIAD_REG(reg)		(*reg)
#endif

#define ZSP_FE	 1
#define ZSP_VE	 2
#define BOOTOS	 1
#define BOOTMON  0

#endif /* !(_BRECIS_SYS_REGS_H) */
