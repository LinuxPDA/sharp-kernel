/*
 * include/asm-sh/ms7760cp.h
 * Copyright (c) 2003 Lineo Solutions, Inc.
 *
 * Based on include/asm-sh/hitachi_7760se.h
 * SH7760 Solution Engine2 version: Copyright (C) 2003 Takeo Takahashi
 * Copyright (C) 2000 Kazumoto Kojima
 *
 */

#ifndef __ASM_SH_MS7760CP_H
#define __ASM_SH_MS7760CP_H

#include <asm/sh7760.h>

#if defined(CONFIG_SH_7760_SOLUTION_ENGINE_NB)
#define CONFIG_7760_PCMCIA_IF_SUPPORT
#endif

#define MS7760CP_IRQ3          0      /*
				       * DO NOT USE !!!
				       *
				       * 0 usually means that IRQ number
				       * is not assigned, for example in
				       * IDE driver.
				       */
#define MS7760CP_SIRQ3         1      /* PCMCIA */
#define MS7760CP_H8IRQ         2      /* H8/3048 */
#define MS7760CP_UART_INTB     3      /* ST16C2550 CPU board ch.B */
#define MS7760CP_IRQ2          4      /* SMC91C111 LAN board */
#define MS7760CP_SIRQ2         5      /* PCMCIA */
#define MS7760CP_UART_INTA     6      /* ST16C2550 CPU board ch.A */
#define MS7760CP_IRQ1          7      /*
				       * ST16C2550 LAN board ch.A(D)
				       *           or
				       * Extended IDE board INTRQ
				       */
#define MS7760CP_SIRQ1         8      /* PCMCIA */
#define MS7760CP_IRQ0          9      /* ST16C2550 LAN board ch.B(C) */
#define MS7760CP_SIRQ0         10     /* PCMCIA */

#define MS7760CP_SYSCR         0xa4000000

/* UART */
#if defined(CONFIG_7760_PCMCIA_IF_SUPPORT)
#define MS7760CP_UART_A	       0xaa000000
#define MS7760CP_UART_B	       0xaa800000
#else /* CONFIG_7760_PCMCIA_IF_SUPPORT */
#define MS7760CP_UART_A	       0xba000000
#define MS7760CP_UART_B	       0xba800000
#endif /* CONFIG_7760_PCMCIA_IF_SUPPORT */
#define MS7760CP_UART_C        0xb1000000
#define MS7760CP_UART_D        0xb1800000

#define MS7760CP_UART_RHR      0x00
#define MS7760CP_UART_THR      0x00
#define MS7760CP_UART_IER      0x02
#define MS7760CP_UART_FCR      0x04
#define MS7760CP_UART_ISR      0x04
#define MS7760CP_UART_LCR      0x06
#define MS7760CP_UART_MCR      0x08
#define MS7760CP_UART_LSR      0x0a
#define MS7760CP_UART_MSR      0x0c
#define MS7760CP_UART_SPR      0x0e
#define MS7760CP_UART_DLL      0x00
#define MS7760CP_UART_DLM      0x02

#if defined(CONFIG_7760_PCMCIA_IF_SUPPORT)
#define PA_MRSHPC	       0xa83fffe0 /* MR-SHPC-01 PCMCIA controler */
#define PA_MRSHPC_MW1	       0xa8400000 /* MR-SHPC-01 memory window base */
#define PA_MRSHPC_MW2	       0xa8500000 /* MR-SHPC-01 attr window base */
#define PA_MRSHPC_IO	       0xa8600000 /* MR-SHPC-01 I/O window base */
#else /* CONFIG_7760_PCMCIA_IF_SUPPORT */
#define PA_MRSHPC	       0xb83fffe0 /* MR-SHPC-01 PCMCIA controler */
#define PA_MRSHPC_MW1	       0xb8400000 /* MR-SHPC-01 memory window base */
#define PA_MRSHPC_MW2	       0xb8500000 /* MR-SHPC-01 attr window base */
#define PA_MRSHPC_IO	       0xb8600000 /* MR-SHPC-01 I/O window base */
#endif /* CONFIG_7760_PCMCIA_IF_SUPPORT */

#define MRSHPC_MODE            (PA_MRSHPC + 4)
#define MRSHPC_OPTION          (PA_MRSHPC + 6)
#define MRSHPC_CSR             (PA_MRSHPC + 8)
#define MRSHPC_ISR             (PA_MRSHPC + 10)
#define MRSHPC_ICR             (PA_MRSHPC + 12)
#define MRSHPC_CPWCR           (PA_MRSHPC + 14)
#define MRSHPC_MW0CR1          (PA_MRSHPC + 16)
#define MRSHPC_MW1CR1          (PA_MRSHPC + 18)
#define MRSHPC_IOWCR1          (PA_MRSHPC + 20)
#define MRSHPC_MW0CR2          (PA_MRSHPC + 22)
#define MRSHPC_MW1CR2          (PA_MRSHPC + 24)
#define MRSHPC_IOWCR2          (PA_MRSHPC + 26)
#define MRSHPC_CDCR            (PA_MRSHPC + 28)
#define MRSHPC_PCIC_INFO       (PA_MRSHPC + 30)

/* Power */
#define MS7760CP_RTKISR        0x0090
#define   RTKISR_RTCFI         (1<<0)
#define   RTKISR_TPIF          (1<<1)
#define   RTKISR_KEYIF         (1<<2)
#define   RTKISR_POWERIF       (1<<3)
#define   RTKISR_IRRIF         (1<<4)

/* RTC */
#define MS7760CP_RTCCR         0x0000
#define   RTCCR_START          (1<<0)
#define   RTCCR_ARI            (1<<1)
#define   RTCCR_1SECL          (1<<2)
#define   RTCCR_05SECL         (1<<3)
#define   RTCCR_SEWCCAF        (1<<4)
#define   RTCCR_CNTS           (1<<5)
#define MS7760CP_RTCSR         0x0001
#define   RTCSR_ARF            (1<<1)
#define   RTCSR_1SECF          (1<<2)
#define   RTCSR_05SECF         (1<<3)
#define MS7760CP_SECCNT        0x0002
#define MS7760CP_MINCNT        0x0003
#define MS7760CP_HRCNT         0x0004
#define MS7760CP_WKCNT         0x0005
#define MS7760CP_DAYCNT        0x0006
#define MS7760CP_MONCNT        0x0007
#define MS7760CP_YRCNT         0x0008
#define MS7760CP_SECAR         0x0009
#define MS7760CP_MINAR         0x000a
#define MS7760CP_HRAR          0x000b
#define MS7760CP_WKAR          0x000c
#define MS7760CP_DAYAR         0x000d
#define MS7760CP_MONAR         0x000e

/* Touch Panel */
#define MS7760CP_TPLCR         0x0020
#define   TPLCR_TP_STR         (1<<0)
#define   TPLCR_PEN_ONI        (1<<1)
#define   TPLCR_PEN_OFFI       (1<<2)
#define   TPLCR_PEN_ONRE       (1<<3)
#define MS7760CP_TPLSR         0x0021
#define   TPLSR_PEN_ONIF       (1<<1)
#define   TPLSR_PNE_OFFIF      (1<<2)
#define MS7760CP_TPLSCR        0x0022
#define   TPLSCR_20MS          (1<<0)
#define   TPLSCR_40MS          (1<<1)
#define   TPLSCR_60MS          (1<<2)
#define   TPLSCR_80MS          (1<<3)
#define   TPLSCR_100MS         (1<<4)
#define   TPLSCR_120MS         (1<<5)
#define   TPLSCR_140MS         (1<<6)
#define   TPLSCR_160MS         (1<<7)
#define MS7760CP_XPAR          0x0024
#define MS7760CP_YPAR          0x0026
#define MS7760CP_XPDR          0x0028
#define MS7760CP_YPDR          0x002a
#define MS7760CP_XAPDR         0x002c
#define MS7760CP_YAPDR         0x002e
#define MS7760CP_XBPDR         0x0030
#define MS7760CP_YBPDR         0x0032
#define MS7760CP_XCPDR         0x0034
#define MS7760CP_YCPDR         0x0036
#define MS7760CP_XAPAR         0x0038
#define MS7760CP_YAPAR         0x003a
#define MS7760CP_XBPAR         0x003c
#define MS7760CP_YBPAR         0x003e
#define MS7760CP_XCPAR         0x0040
#define MS7760CP_YXPAR         0x0042
#define MS7760CP_DXDR          0x0044
#define MS7760CP_DYDR          0x0046
#define MS7760CP_XPARDOT       0x0048
#define MS7760CP_XPARDOT1      0x004a
#define MS7760CP_XPARDOT2      0x004c
#define MS7760CP_XPARDOT3      0x004e
#define MS7760CP_XPARDOT4      0x0050
#define MS7760CP_YPARDOT       0x0052
#define MS7760CP_YPARDOT1      0x0054
#define MS7760CP_YPARDOT2      0x0056
#define MS7760CP_YPARDOT3      0x0058
#define MS7760CP_YPARDOT4      0x005a

/* Key Switch */
#define MS7760CP_KEYCR         0x0060
#define   KEYCR_KEY_STR        (1<<0)
#define   KEYCR_KEY_ONI        (1<<1)
#define   KEYCR_KEY_OFFI       (1<<2)
#define   KEYCR_ARKEY          (1<<3)
#define   KEYCR_PONSWI         (1<<4)
#define   KEYCR_NMIE           (1<<5)
#define MS7760CP_KATIMER       0x0061
#define   KATIMER_100MS        (1<<0)
#define   KATIMER_150MS        (1<<1)
#define   KATIMER_200MS        (1<<2)
#define   KATIMER_250MS        (1<<3)
#define   KATIMER_300MS        (1<<4)
#define   KATIMER_350MS        (1<<5)
#define   KATIMER_400MS        (1<<6)
#define   KATIMER_450MS        (1<<7)
#define MS7760CP_KEYSR         0x0062
#define   KEYSR_KEY_ONF        (1<<1)
#define   KEYSR_KEY_OFFF       (1<<2)
#define   KEYSR_ARKEYF         (1<<3)
#define   KEYSR_PONSWF         (1<<4)
#define MS7760CP_KBITPR        0x0064
#define   KBITPR_SW1_RIGHT     (1<<0)
#define   KBITPR_SW1_LEFT      (1<<1)
#define   KBITPR_SW1_UP        (1<<2)
#define   KBITPR_SW1_DOWN      (1<<3)
#define   KBITPR_SW1_ENTER     (1<<4)
#define   KBITPR_SW3           (1<<8)
#define   KBITPR_SW2           (1<<10)

/* Power Control */
#define MS7760CP_SPOWCR1       0x0070
#define   SPOWCR1_SPOWER       (1<<0)
#define MS7760CP_SPOWCR2       0x0071
#define   SPOWCR2_SFPOWER      (1<<0)

/* LED */
#define MS7760CP_LEDR          0x00a0

/* LCD */
#define MS7760CP_LCDR          0x00a1
#define   LCDR_FRONTL          (1<<0)

/* Reset */
#define MS7760CP_RESTCR        0x00a2
#define   RESTCR_SORES         (1<<0)
#define   RESTCR_SWRES         (1<<1)

/* IR */
#define MS7760CP_IRRCR         0x00b0
#define   IRRCR_START          (1<<0)
#define   IRRCR_FORMAT         (1<<1)
#define   IRRCR_RDIE           (1<<2)
#define   IRRCR_TDIE           (1<<3)
#define MS7760CP_IRRSR         0x00b1
#define   IRRSR_RDBFER         (1<<0)
#define   IRRSR_RDI            (1<<2)
#define   IRRSR_TDI            (1<<3)
#define MS7760CP_IRRRDNR       0x00b2
#define MS7760CP_IRRSDNR       0x00b3
#define MS7760CP_IRRRFDR       0x00b4
#define MS7760CP_IRRSFDR       0x00b5

/* Serial EEPROM */
#define MS7760CP_EEPCR         0x00c0
#define   EEPCR_START          (1<<0)
#define MS7760CP_EEPSR         0x00c1
#define   EEPSR_EEPWER         (1<<0)
#define MS7760CP_EEPDR         0x0100
#define MS7760CP_EEPDR_SIZE    512

/* Volume */
#define MS7760CP_EVRDR         0x00d0
#define MS7760CP_EVLDR         0x00d1

/* Backword compat.. */

#define PA_ROM                 0x00000000 /* EPROM */
#define PA_ROM_SIZE            0x00400000 /* EPROM size 4M byte */
#define PA_FROM                0x01000000
#define PA_FROM_SIZE           0x00800000
#define PA_EXT1                0x04000000
#define PA_EXT1_SIZE           0x04000000
#define PA_EXT2                0x08000000
#define PA_EXT2_SIZE           0x04000000
#define PA_SDRAM               0x0c000000
#define PA_SDRAM_SIZE          0x04000000

#define PA_EXT4                0x10000000
#define PA_EXT4_SIZE           0x04000000
#define PA_EXT5                0x14000000
#define PA_EXT5_SIZE           0x04000000
#define PA_PCIC                0x18000000 /* MR-SHPC-01 PCMCIA */

#if defined(CONFIG_7760_PCMCIA_IF_SUPPORT)
#define PA_DIPSW0	0xab000000	/* Dip switch (SSWDR) */
#else
#define PA_DIPSW0	0xbb000000	/* Dip switch (SSWDR) */
#endif
//#define PA_BCR                 0xbb000000 /* FPGA on the MS7760SE01 */

#define ST16_UART_A_BASE       MS7760CP_UART_A
#define ST16_UART_B_BASE       MS7760CP_UART_B
#define ST16_UART_C_BASE       MS7760CP_UART_C
#define ST16_UART_D_BASE       MS7760CP_UART_D

#define ST16_UART_SHIFT        1

#define ST16_RHR               MS7760CP_UART_RHR
#define ST16_THR               MS7760CP_UART_THR
#define ST16_IER               MS7760CP_UART_IER
#define ST16_FCR               MS7760CP_UART_FCR
#define ST16_ISR               MS7760CP_UART_ISR
#define ST16_LCR               MS7760CP_UART_LCR
#define ST16_MCR               MS7760CP_UART_MCR
#define ST16_LSR               MS7760CP_UART_LSR
#define ST16_MSR               MS7760CP_UART_MSR
#define ST16_SPR               MS7760CP_UART_SPR
#define ST16_DLL               MS7760CP_UART_DLL
#define ST16_DLM               MS7760CP_UART_DLM

#define SH7760SE_IRQ_IRQ3      MS7760CP_IRQ3
#define SH7760SE_IRQ_SIRQ3     MS7760CP_SIRQ3
#define SH7760SE_IRQ_H8IRQ     MS7760CP_H8IRQ
#define SH7760SE_IRQ_UART_INTB MS7760CP_UART_INTB
#define SH7760SE_IRQ_IRQ2      MS7760CP_IRQ2
#define SH7760SE_IRQ_SIRQ2     MS7760CP_SIRQ2
#define SH7760SE_IRQ_UART_INTA MS7760CP_UART_INTA
#define SH7760SE_IRQ_IRQ1      MS7760CP_IRQ1
#define SH7760SE_IRQ_SIRQ1     MS7760CP_SIRQ1
#define SH7760SE_IRQ_IRQ0      MS7760CP_IRQ0
#define SH7760SE_IRQ_SIRQ0     MS7760CP_SIRQ0

#define SH7760SE_RTC_RTCCR     MS7760CP_RTCCR
#define SH7760SE_RTC_RTCSR     MS7760CP_RTCSR
#define SH7760SE_RTC_SECCNT    MS7760CP_SECCNT
#define SH7760SE_RTC_MINCNT    MS7760CP_MINCNT
#define SH7760SE_RTC_HRCNT     MS7760CP_HRCNT
#define SH7760SE_RTC_WKCNT     MS7760CP_WKCNT
#define SH7760SE_RTC_DAYCNT    MS7760CP_DAYCNT
#define SH7760SE_RTC_MONCNT    MS7760CP_MONCNT
#define SH7760SE_RTC_YRCNT     MS7760CP_YRCNT
#define SH7760SE_RTC_SECAR     MS7760CP_SECAR
#define SH7760SE_RTC_MINAR     MS7760CP_MINAR
#define SH7760SE_RTC_HRAR      MS7760CP_HRAR
#define SH7760SE_RTC_WKAR      MS7760CP_WKAR
#define SH7760SE_RTC_DAYAR     MS7760CP_DAYAR
#define SH7760SE_RTC_MONAR     MS7760CP_MONAR

#define SH7760SE_TPL_TPLCR     MS7760CP_TPLCR
#define SH7760SE_TPL_TPLSR     MS7760CP_TPLSR
#define SH7760SE_TPL_TPLSCR    MS7760CP_TPLSCR
#define SH7760SE_TPL_XPAR      MS7760CP_XPAR
#define SH7760SE_TPL_YPAR      MS7760CP_YPAR
#define SH7760SE_TPL_XPDR      MS7760CP_XPDR
#define SH7760SE_TPL_YPDR      MS7760CP_YPDR
#define SH7760SE_TPL_XAPDR     MS7760CP_XAPDR
#define SH7760SE_TPL_YAPDR     MS7760CP_YAPDR
#define SH7760SE_TPL_XBPDR     MS7760CP_XBPDR
#define SH7760SE_TPL_YBPDR     MS7760CP_YBPDR
#define SH7760SE_TPL_XCPDR     MS7760CP_XCPDR
#define SH7760SE_TPL_YCPDR     MS7760CP_YCPDR
#define SH7760SE_TPL_XAPAR     MS7760CP_XAPAR
#define SH7760SE_TPL_YAPAR     MS7760CP_YAPAR
#define SH7760SE_TPL_XBPAR     MS7760CP_XBPAR
#define SH7760SE_TPL_YBPAR     MS7760CP_YBPAR
#define SH7760SE_TPL_XCPAR     MS7760CP_XCPAR
#define SH7760SE_TPL_YXPAR     MS7760CP_YXPAR
#define SH7760SE_TPL_DXDR      MS7760CP_DXDR
#define SH7760SE_TPL_DYDR      MS7760CP_DYDR
#define SH7760SE_TPL_XPARDOT   MS7760CP_XPARDOT
#define SH7760SE_TPL_XPARDOT1  MS7760CP_XPARDOT1
#define SH7760SE_TPL_XPARDOT2  MS7760CP_XPARDOT2
#define SH7760SE_TPL_XPARDOT3  MS7760CP_XPARDOT3
#define SH7760SE_TPL_XPARDOT4  MS7760CP_XPARDOT4
#define SH7760SE_TPL_YPARDOT   MS7760CP_YPARDOT
#define SH7760SE_TPL_YPARDOT1  MS7760CP_YPARDOT1
#define SH7760SE_TPL_YPARDOT2  MS7760CP_YPARDOT2
#define SH7760SE_TPL_YPARDOT3  MS7760CP_YPARDOT3
#define SH7760SE_TPL_YPARDOT4  MS7760CP_YPARDOT4

#define SH7760SE_KEY_KEYCR     MS7760CP_KEYCR
#define SH7760SE_KEY_KATIMER   MS7760CP_KATIMER
#define SH7760SE_KEY_KEYSR     MS7760CP_KEYSR
#define SH7760SE_KEY_KBITPR    MS7760CP_KBITPR

#define SH7760SE_PWC_SPOWCR1   MS7760CP_SPOWCR1
#define SH7760SE_PWC_SPOWCR2   MS7760CP_SPOWCR2
#define SH7760SE_RTKISR        MS7760CP_RTKISR

#define SH7760SE_LED_LEDR      MS7760CP_LEDR

#define SH7760SE_LCD_LCDR      MS7760CP_LCDR

#define SH7760SE_RST_RESTCR    MS7760CP_RESTCR

#define SH7760SE_IR_IRRCR      MS7760CP_IRRCR
#define SH7760SE_IR_IRRSR      MS7760CP_IRRSR
#define SH7760SE_IR_IRRRDNR    MS7760CP_IRRRDNR
#define SH7760SE_IR_IRRSDNR    MS7760CP_IRRSDNR
#define SH7760SE_IR_IRRRFDR    MS7760CP_IRRRFDR
#define SH7760SE_IR_IRRSFDR    MS7760CP_IRRSFDR

#define SH7760SE_EE_EEPCR      MS7760CP_EEPCR
#define SH7760SE_EE_EEPSR      MS7760CP_EEPSR
#define SH7760SE_EE_EEPDR      MS7760CP_EEPDR
#define SH7760SE_EE_EEPDR_SIZE MS7760CP_EEPDR_SIZE

#define SH7760SE_VOL_EVRDR     MS7760CP_EVRDR
#define SH7760SE_VOL_EVLDR     MS7760CP_EVLDR

#endif  /* __ASM_SH_MS7760CP_H */

