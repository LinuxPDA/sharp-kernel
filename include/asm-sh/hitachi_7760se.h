#ifndef __ASM_SH_HITACHI_7760SE_H
#define __ASM_SH_HITACHI_7760SE_H

/*
 * linux/include/asm-sh/hitachi_7760se.h
 *
 * Copyright (C) 2000  Kazumoto Kojima
 *
 * Hitachi SolutionEngine support
 *
 * SH7760 Solution Engine2 version: Copyright (C) 2003 Takeo Takahashi
 *
 * 2003-11-26:	Takeo Takahashi
 *		Support PCMCIA I/F mode if CONFIG_7760_PCMCIA_IF_SUPPORT is
 *		defined.
 */

#if defined(CONFIG_SH_7760_SOLUTION_ENGINE_NB)
#define CONFIG_7760_PCMCIA_IF_SUPPORT
#endif

#define SH7760_BCR1                0xFF800000    /* Memory BCR1 Register */
#define SH7760_BCR2                0xFF800004    /* Memory BCR2 Register */
#define SH7760_BCR3                0xFF800050    /* Memory BCR3 Register */
#define SH7760_BCR4                0xFE0A00F0    /* Memory BCR4 Register */
#define SH7760_WCR1                0xFF800008    /* Memory WCR1 Register */
#define SH7760_WCR2                0xFF80000C    /* Memory WCR2 Register */
#define SH7760_WCR3                0xFF800010    /* Memory WCR3 Register */
#define SH7760_WCR4                0xFE0A0028    /* Memory WCR4 Register */

/* Box specific addresses.  */

#define PA_ROM		0x00000000	/* EPROM */
#define PA_ROM_SIZE	0x00400000	/* EPROM size 4M byte */
#define PA_FROM		0x01000000	/* EPROM */
#define PA_FROM_SIZE	0x00800000	/* EPROM size 8M byte */
#define PA_EXT1		0x04000000
#define PA_EXT1_SIZE	0x04000000
#define PA_EXT2		0x08000000
#define PA_EXT2_SIZE	0x04000000
#define PA_SDRAM	0x0c000000
#define PA_SDRAM_SIZE	0x04000000

#define PA_EXT4		0x10000000
#define PA_EXT4_SIZE	0x04000000
#define PA_EXT5		0x14000000
#define PA_EXT5_SIZE	0x04000000
#define PA_PCIC		0x18000000	/* MR-SHPC-01 PCMCIA */

#if defined(CONFIG_7760_PCMCIA_IF_SUPPORT)
#define PA_DIPSW5	0xab000000	/* Dip switch (SSWDR) */
#else
#define PA_DIPSW5	0xbb000000	/* Dip switch (SSWDR) */
#endif
#if 0
#define PA_DIPSW1	0xb9000002	/* Dip switch 7,8 */
#define PA_LED		0xba000000	/* LED */
#define	PA_BCR		0xbb000000	/* FPGA on the MS7760SE01 */
#endif

#if defined(CONFIG_7760_PCMCIA_IF_SUPPORT)
#define PA_MRSHPC	0xa83fffe0	/* MR-SHPC-01 PCMCIA controler */
#define PA_MRSHPC_MW1	0xa8400000	/* MR-SHPC-01 memory window base */
#define PA_MRSHPC_MW2	0xa8500000	/* MR-SHPC-01 attribute window base */
#define PA_MRSHPC_IO	0xa8600000	/* MR-SHPC-01 I/O window base */
#else
#define PA_MRSHPC	0xb83fffe0	/* MR-SHPC-01 PCMCIA controler */
#define PA_MRSHPC_MW1	0xb8400000	/* MR-SHPC-01 memory window base */
#define PA_MRSHPC_MW2	0xb8500000	/* MR-SHPC-01 attribute window base */
#define PA_MRSHPC_IO	0xb8600000	/* MR-SHPC-01 I/O window base */
#endif
#define MRSHPC_MODE     (PA_MRSHPC + 4)
#define MRSHPC_OPTION   (PA_MRSHPC + 6)
#define MRSHPC_CSR      (PA_MRSHPC + 8)
#define MRSHPC_ISR      (PA_MRSHPC + 10)
#define MRSHPC_ICR      (PA_MRSHPC + 12)
#define MRSHPC_CPWCR    (PA_MRSHPC + 14)
#define MRSHPC_MW0CR1   (PA_MRSHPC + 16)
#define MRSHPC_MW1CR1   (PA_MRSHPC + 18)
#define MRSHPC_IOWCR1   (PA_MRSHPC + 20)
#define MRSHPC_MW0CR2   (PA_MRSHPC + 22)
#define MRSHPC_MW1CR2   (PA_MRSHPC + 24)
#define MRSHPC_IOWCR2   (PA_MRSHPC + 26)
#define MRSHPC_CDCR     (PA_MRSHPC + 28)
#define MRSHPC_PCIC_INFO (PA_MRSHPC + 30)

#if 0
#define BCR_ILCRA	(PA_BCR + 0)
#define BCR_ILCRB	(PA_BCR + 2)
#define BCR_ILCRC	(PA_BCR + 4)
#define BCR_ILCRD	(PA_BCR + 6)
#define BCR_ILCRE	(PA_BCR + 8)
#define BCR_ILCRF	(PA_BCR + 10)
#define BCR_ILCRG	(PA_BCR + 12)

#define IRQ_79C973	13
#endif

/* external interrupt */
#define SH7760SE_IRQ_IRQ3	0	/*
				 	 * DO NOT USE !!!
					 *
					 * 0 usually means that IRQ number
					 * is not assigned, for example in
					 * IDE driver.
					 */
#define SH7760SE_IRQ_SIRQ3	1	/* PCMCIA */
#define SH7760SE_IRQ_H8IRQ	2	/* H8/3048 */
#define SH7760SE_IRQ_UART_INTB	3	/* ST16C2550 CPU board ch.A */
#define SH7760SE_IRQ_IRQ2	4	/* SMC91C111 LAN board */
#define SH7760SE_IRQ_SIRQ2	5	/* PCMCIA */
#define SH7760SE_IRQ_UART_INTA	6	/* ST16C2550 CPU board ch.B */
#define SH7760SE_IRQ_IRQ1	7	/*
					 * ST16C2550 LAN board ch.A(D)
					 *           or
					 * Extended IDE board INTRQ
					 */
#define SH7760SE_IRQ_SIRQ1	8	/* PCMCIA */
#define SH7760SE_IRQ_IRQ0	9	/* ST16C2550 LAN board ch.B(C) */
#define SH7760SE_IRQ_SIRQ0	10	/* PCMCIA */

/*
 * UART details (EXAR ST16C2550 UART)
 */
#define ST16_UART_SHIFT		1
#if defined(CONFIG_7760_PCMCIA_IF_SUPPORT)
#define ST16_UART_A_BASE	0xaa000000
#define ST16_UART_B_BASE	0xaa800000
#else
#define ST16_UART_A_BASE	0xba000000
#define ST16_UART_B_BASE	0xba800000
#endif
#define ST16_UART_C_BASE	0xb1000000	/* area4, lab board ch.A */
#define ST16_UART_D_BASE	0xb1800000	/* area4, lan board ch.B */

#define ST16_RHR        (0x0000)
#define ST16_THR        (0x0000)
#define ST16_IER        (0x0002)
#define ST16_FCR        (0x0004)
#define ST16_ISR        (0x0004)
#define ST16_LCR        (0x0006)
#define ST16_MCR        (0x0008)
#define ST16_LSR        (0x000a)
#define ST16_MSR        (0x000c)
#define ST16_SPR        (0x000e)
#define ST16_DLL        (0x0000)
#define ST16_DLM        (0x0002)

/*
 * Power Controller
 */

/* Status Register */
#define SH7760SE_RTKISR		0x0090	/* 1 byte */

/* RTC */
#define SH7760SE_RTC_RTCCR	0x0000	/* 1 */
#define SH7760SE_RTC_RTCSR	0x0001	/* 1 */
#define SH7760SE_RTC_SECCNT	0x0002	/* 1 */
#define SH7760SE_RTC_MINCNT	0x0003	/* 1 */
#define SH7760SE_RTC_HRCNT	0x0004	/* 1 */
#define SH7760SE_RTC_WKCNT	0x0005	/* 1 */
#define SH7760SE_RTC_DAYCNT	0x0006	/* 1 */
#define SH7760SE_RTC_MONCNT	0x0007	/* 1 */
#define SH7760SE_RTC_YRCNT	0x0008	/* 1 */
#define SH7760SE_RTC_SECAR	0x0009	/* 1 */
#define SH7760SE_RTC_MINAR	0x000a	/* 1 */
#define SH7760SE_RTC_HRAR	0x000b	/* 1 */
#define SH7760SE_RTC_WKAR	0x000c	/* 1 */
#define SH7760SE_RTC_DAYAR	0x000d	/* 1 */
#define SH7760SE_RTC_MONAR	0x000e	/* 1 */

/* Touch Panel */
#define SH7760SE_TPL_TPLCR	0x0020	/* 1 */
#define SH7760SE_TPL_TPLSR	0x0021	/* 1 */
#define SH7760SE_TPL_TPLSCR	0x0022	/* 1 */
#define SH7760SE_TPL_XPAR	0x0024	/* 2 */
#define SH7760SE_TPL_YPAR	0x0026	/* 2 */
#define SH7760SE_TPL_XPDR	0x0028	/* 2 */
#define SH7760SE_TPL_YPDR	0x002a	/* 2 */
#define SH7760SE_TPL_XAPDR	0x002c	/* 2 */
#define SH7760SE_TPL_YAPDR	0x002e	/* 2 */
#define SH7760SE_TPL_XBPDR	0x0030	/* 2 */
#define SH7760SE_TPL_YBPDR	0x0032	/* 2 */
#define SH7760SE_TPL_XCPDR	0x0034	/* 2 */
#define SH7760SE_TPL_YCPDR	0x0036	/* 2 */
#define SH7760SE_TPL_XAPAR	0x0038	/* 2 */
#define SH7760SE_TPL_YAPAR	0x003a	/* 2 */
#define SH7760SE_TPL_XBPAR	0x003c	/* 2 */
#define SH7760SE_TPL_YBPAR	0x003e	/* 2 */
#define SH7760SE_TPL_XCPAR	0x0040	/* 2 */
#define SH7760SE_TPL_YXPAR	0x0042	/* 2 */
#define SH7760SE_TPL_DXDR	0x0044	/* 2 */
#define SH7760SE_TPL_DYDR	0x0046	/* 2 */
#define SH7760SE_TPL_XPARDOT	0x0048	/* 2 */
#define SH7760SE_TPL_XPARDOT1	0x004a	/* 2 */
#define SH7760SE_TPL_XPARDOT2	0x004c	/* 2 */
#define SH7760SE_TPL_XPARDOT3	0x004e	/* 2 */
#define SH7760SE_TPL_XPARDOT4	0x0050	/* 2 */
#define SH7760SE_TPL_YPARDOT	0x0052	/* 2 */
#define SH7760SE_TPL_YPARDOT1	0x0054	/* 2 */
#define SH7760SE_TPL_YPARDOT2	0x0056	/* 2 */
#define SH7760SE_TPL_YPARDOT3	0x0058	/* 2 */
#define SH7760SE_TPL_YPARDOT4	0x005a	/* 2 */

/* Key Switch */
#define SH7760SE_KEY_KEYCR	0x0060	/* 1 */
#define SH7760SE_KEY_KATIMER	0x0061	/* 1 */
#define SH7760SE_KEY_KEYSR	0x0062	/* 1 */
#define SH7760SE_KEY_KBITPR	0x0064	/* 2 */

/* Power Control */
#define SH7760SE_PWC_SPOWCR1	0x0070	/* 1 */
#define SH7760SE_PWC_SPOWCR2	0x0071	/* 1 */

/* LED */
#define SH7760SE_LED_LEDR	0x00a0	/* 1 */

/* LCD */
#define SH7760SE_LCD_LCDR	0x00a1	/* 1 */

/* Reset */
#define SH7760SE_RST_RESTCR	0x00a2	/* 1 */

/* IrDA */
#define SH7760SE_IR_IRRCR	0x00b0	/* 1 */
#define SH7760SE_IR_IRRSR	0x00b1	/* 1 */
#define SH7760SE_IR_IRRRDNR	0x00b2	/* 1 */
#define SH7760SE_IR_IRRSDNR	0x00b3	/* 1 */
#define SH7760SE_IR_IRRRFDR	0x00b4	/* 1 */
#define SH7760SE_IR_IRRSFDR	0x00b5	/* 1 */

/* Serial EEPROM */
#define SH7760SE_EE_EEPCR	0x00c0	/* 1 */
#define SH7760SE_EE_EEPSR	0x00c1	/* 1 */
#define SH7760SE_EE_EEPDR	0x0100	/* 1byte * 512 */
#define SH7760SE_EE_EEPDR_SIZE	512

/* Volume */
#define SH7760SE_VOL_EVRDR	0x00d0	/* 1 */
#define SH7760SE_VOL_EVLDR	0x00d1	/* 1 */

/*
 * SH7760 Peripherals
 */
/* LCDC */
#define LCD_LDICKR	0xfe300c00
#define LCD_LDMTR	0xfe300c02
#define LCD_LDDFR	0xfe300c04
#define LCD_LDSMR	0xfe300c06
#define LCD_LDSARU	0xfe300c08
#define LCD_LDSARL	0xfe300c0c
#define LCD_LDLAOR	0xfe300c10
#define LCD_LDPALCR	0xfe300c12
#define LCD_LDPR00	0xfe300800	/* 800-8ff */
#define LCD_LDHCNR	0xfe300c14
#define LCD_LDHSYNR	0xfe300c16
#define LCD_LDVDLNR	0xfe300c18
#define LCD_LDVTLNR	0xfe300c1a
#define LCD_LDVSYNR	0xfe300c1c
#define LCD_LDACLNR	0xfe300c1e
#define LCD_LDINTR	0xfe300c20
#define LCD_LDPMMR	0xfe300c24
#define LCD_LDPSPR	0xfe300c26
#define LCD_LDCNTR	0xfe300c28

#endif  /* __ASM_SH_HITACHI_7760SE_H */
