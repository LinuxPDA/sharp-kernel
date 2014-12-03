/*
 * linux/drivers/char/mx2-digi.h
 *
 * Copyright (C) 2003 Lineo Solutions, Inc.
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file "COPYING" in the main directory of this archive
 * for more details.
 * 
 * Based on:
 * Motorola i.MX21ADS BSP elinux v0.2
 * mx21_rel_0.2/Drivers/digi/source/digi.h
 */
#ifndef __DIGI_H_MX2_
#define __DIGI_H_MX2_



//#define CONFIG_ARCH_MX1ADS 
//#define CONFIG_ARCH_MX2ADS
#ifdef CONFIG_ARCH_DBMX2
#define TPNL_IRQ 		8	//INT_GPIO
#define TPNL_INTR_MODE		SA_INTERRUPT|SA_SHIRQ
#define TPNL_PEN_UPVALUE	-999

#define SPI_TE_INT_BIT		0x00000001	
#define	SPI_TSHFE_INT_BIT	0x00000008
#define SPI_XCH_BIT		0x00000200	
#define SPI_RR_INT_BIT		0x00000010			
#define SPI_XCH_MASK		0xfffffdff
#define SPI_EN_BIT		0x00000400
#endif

#ifdef CONFIG_ARCH_MX1ADS
#define TPNL_IRQ 		62
#define TPNL_INTR_MODE		SA_INTERRUPT|SA_SHIRQ
#define TPNL_PEN_UPVALUE	-999


#define SPI_XCH_BIT		0x00000100	
#define SPI_RR_INT		0x00000008
#define SPI_XCH_MASK		0xfffffeff	//1 means initiate exchange,0 means idle
#define SPI_EN_BIT		0x00000200
#endif

#ifdef CONFIG_ARCH_MX1ADS
#define REG_SPI_RXDATA 		(*((volatile unsigned long*)(IO_ADDRESS(0x00213000))))
#define REG_SPI_TXDATA 		(*((volatile unsigned long*)(IO_ADDRESS(0x00213004))))
#define REG_SPI_CONTRL		(*((volatile unsigned long*)(IO_ADDRESS(0x00213008))))
#define	REG_SPI_INTER		(*((volatile unsigned long*)(IO_ADDRESS(0x0021300C))))
#define REG_SPI_SAMPLE		(*((volatile unsigned long*)(IO_ADDRESS(0x00213014))))
#define REG_SPI_SOFTRESET	(*((volatile unsigned long*)(IO_ADDRESS(0x0021301C))))
#define REG_PC_DDIR		(*((volatile unsigned long*)(IO_ADDRESS(0x0021C200))))
#define REG_PC_ICONFA1		(*((volatile unsigned long*)(IO_ADDRESS(0x0021C20C))))
#define REG_PC_ICONFA2		(*((volatile unsigned long*)(IO_ADDRESS(0x0021C210))))
#define REG_PC_DR		(*((volatile unsigned long*)(IO_ADDRESS(0x0021C21C))))
#define REG_PC_GIUS		(*((volatile unsigned long*)(IO_ADDRESS(0x0021C220))))
#define REG_PC_GPR		(*((volatile unsigned long*)(IO_ADDRESS(0x0021C238))))
#define REG_PC_PUEN		(*((volatile unsigned long*)(IO_ADDRESS(0x0021C240))))
#define REG_PC_OCR1		(*((volatile unsigned long*)(IO_ADDRESS(0x0021C204))))

#define REG_PA_DDIR		(*((volatile unsigned long*)(IO_ADDRESS(0x0021C000))))
#define REG_PA_ICONFA1		(*((volatile unsigned long*)(IO_ADDRESS(0x0021C00C))))
#define REG_PA_ICONFA2		(*((volatile unsigned long*)(IO_ADDRESS(0x0021C010))))
#define REG_PA_GIUS		(*((volatile unsigned long*)(IO_ADDRESS(0x0021C020))))
#define REG_PA_GPR		(*((volatile unsigned long*)(IO_ADDRESS(0x0021C038))))
#define REG_PA_PUEN		(*((volatile unsigned long*)(IO_ADDRESS(0x0021C040))))
#define REG_PD_DDIR		(*((volatile unsigned long*)(IO_ADDRESS(0x0021C300))))
#define REG_PD_ICONFA2		(*((volatile unsigned long*)(IO_ADDRESS(0x0021C310))))
#define REG_PD_ICONFB2		(*((volatile unsigned long*)(IO_ADDRESS(0x0021C318))))
#define REG_PD_SSR		(*((volatile unsigned long*)(IO_ADDRESS(0x0021C324))))
#define REG_PD_GIUS		(*((volatile unsigned long*)(IO_ADDRESS(0x0021C320))))
#define REG_PD_ICR2		(*((volatile unsigned long*)(IO_ADDRESS(0x0021C32C))))
#define REG_PD_IMR		(*((volatile unsigned long*)(IO_ADDRESS(0x0021C330))))
#define REG_PD_ISR		(*((volatile unsigned long*)(IO_ADDRESS(0x0021C334))))
#define REG_PD_GPR		(*((volatile unsigned long*)(IO_ADDRESS(0x0021C338))))
#define REG_PD_PUEN		(*((volatile unsigned long*)(IO_ADDRESS(0x0021C340))))
#define REG_PD_OCR2		(*((volatile unsigned long*)(IO_ADDRESS(0x0021C308))))

#define TOUCH_INT_MASK		0x7fffffff
#define TOUCH_INT_NEG_POLARITY	0x40000000
#define SPI_TE_INT		0x00000001
#define SPI_RR_INT		0x00000008

#endif


#define EDGE_MIN_LIMIT  (100)
#define EDGE_MAX_LIMIT  (4000)

#define QT_IPAQ	

typedef struct {
#ifdef QT_IPAQ
	unsigned short	pressure;
#else
	unsigned char	pressure;
#endif
	unsigned short	x;
	unsigned short	y;
#ifdef QT_IPAQ
	unsigned short	pad;
#endif
	long long	stamp;
} ts_event_t;


#define  MAX_ID 	0x14

#ifdef QT_IPAQ
#define PENUP		0x0
#define PENDOWN		0xffff
#else
#define PENUP		0x0
#define PENDOWN		0xff
#endif

#endif

