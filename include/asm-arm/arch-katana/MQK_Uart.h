/*-----------------------------------------------------------------------------
 *  MQKT UART Block Specification 
 * ----------------------------------------------------------------------------
 *  UART Register Offsets & Definitions.
 *  
 *  Copyright (c) 2003 MediaQ, Inc.
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
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 */

#ifndef _MQUART_H
#define _MQUART_H

#include <asm/arch/MQ9000Brd.h>

#define MQ_UART_TIMEOUT			10000

/* Emulation base registers */
#define MQUART0_BASE			0xC0000000
#define MQUART1_BASE			0xC0004800
#define MQUART2_BASE			0x30000

/* UART Registers */
#define MQ_UART_CLK_CNTRL1	0x00     /*  UART Clock Control 1 Register. */
#define MQ_UART_CLK_CNTRL2	0x04     /*  UART Clock Control 2 Register. */
#define MQ_UART_TXCNTRL		0x08     /*  UART Transmit Control Register. */
#define MQ_UART_RXCNTRL		0x0C     /*  UART Receive Control Register. */
#define MQ_UART_INTR_CNTRL	0x10     /*  UART Interrupt Control Register. */
#define MQ_UART_INTR_STAT	0x14     /*  UART Interrupt Status Register. */
#define MQ_UART_TXDATA		0x18     /*  UART Transmit Data Register. */
#define MQ_UART_TXBUF_START	0x1C     /*  UART Transmit Buffer Start Addr Register. */
#define MQ_UART_TXBUF_SIZE	0x20     /*  UART Transmit Buffer Size. */
#define MQ_UART_RXDATA		0x24     /*  UART Receive Data Register (Read Only). */
#define MQ_UART_RXBUF_START	0x28     /*  UART Receive Buffer Start Addr Register. */
#define MQ_UART_RXBUF_SIZE	0x2C     /*  UART Receive Buffer End Addr Register. */

/* Clock mask */
#define MQ_BAUD_PS_MASK			0x0000F000
#define MQ_BAUD_MP_MASK			0x00000F00
#define MQ_BAUD_TDCLK_MASK		0x001C0000
#define MQ_BAUD_RDCLK_MASK		0x00E00000
#define MQ_BAUD_TDIV_MASK		0x0000FFFF
#define MQ_BAUD_RDIV_MASK		0xFFFF0000

/* RX/TX FIFO Thresholds */
#define MQ_RX_THRESHOLD		1   /* Max FIFO entries: 64 11-bits */
#define MQ_TX_THRESHOLD		0   /* Reserved? (for U204R[0]) */
//#define MQ_TX_THRESHOLD	16  /* Max FIFO entries: 32 dwords */

/* RX/TX FIFO Depth */
#define MQ_RXFIFO_DEPTH		64   /* Max FIFO entries: 64 11-bits */
#define MQ_TXFIFO_DEPTH		32   /* Max FIFO entries: 32 dwords = 128 bytes */

/* UART Clock Control 1 */
#define MQ_FOSC_UART_RCLK		0x00000000
#define MQ_PLL1_UART_RCLK		0x00000001
#define MQ_PLL2_UART_RCLK		0x00000002
#define MQ_TXBAUD_TXCLK			0x00000000
#define MQ_DCD1_TXCLK			0x00000004
#define MQ_CTS1_TXCLK			0x00000008
#define MQ_TXBAUD_RXCLK			0x00000000
#define MQ_RXBAUD_RXCLK			0x00000010
#define MQ_CTS1_RXCLK			0x00000020
#define MQ_UMCLK_TXBAUD			0x00000000
#define MQ_DCD1_TXBAUD			0x00000040
#define MQ_UMCLK_RXBAUD			0x00000000
#define MQ_DCD1_RXBAUD			0x00000080
#define MQ_UMCLK_MP_1_5			0x00000000
#define MQ_UMCLK_MP_2			0x00000100
#define MQ_UMCLK_MP_2_5			0x00000200
#define MQ_UMCLK_MP_3			0x00000300
#define MQ_UMCLK_MP_3_5			0x00000400
#define MQ_UMCLK_MP_4			0x00000500
#define MQ_UMCLK_MP_4_5			0x00000600
#define MQ_UMCLK_MP_5			0x00000700
#define MQ_UMCLK_MP_5_5			0x00000800
#define MQ_UMCLK_MP_6			0x00000900
#define MQ_UMCLK_MP_6_5			0x00000A00
#define MQ_UMCLK_MP_7			0x00000B00
#define MQ_UMCLK_MP_8			0x00000C00
#define MQ_UMCLK_MP_9			0x00000D00
#define MQ_UMCLK_URCLK			0x00000000
#define MQ_UMCLK_PS_1			0x00001000
#define MQ_UMCLK_PS_2			0x00002000
#define MQ_UMCLK_PS_3			0x00003000
#define MQ_UMCLK_PS_4			0x00004000
#define MQ_UMCLK_PS_6			0x00005000
#define MQ_UMCLK_PS_8			0x00006000
#define MQ_UMCLK_PS_12			0x00007000
#define MQ_UMCLK_PS_16			0x00008000
#define MQ_UMCLK_PS_24			0x00009000
#define MQ_UMCLK_PS_32			0x0000A000
#define MQ_UMCLK_PS_48			0x0000B000
#define MQ_UMCLK_PS_64			0x0000C000
#define MQ_UMCLK_PS_96			0x0000D000
#define MQ_UMCLK_PS_LOW			0x0000E000
#define MQ_UMCLK_PS_HIGH		0x0000F000
#define MQ_TXCLK_1X			0x00000000
#define MQ_TXCLK_4X			0x00040000
#define MQ_TXCLK_8X			0x00080000
#define MQ_TXCLK_16X			0x000C0000
#define MQ_TXCLK_32X			0x00100000
#define MQ_TXCLK_64X			0x00140000
#define MQ_RXCLK_1X			0x00000000
#define MQ_RXCLK_4X			0x00200000
#define MQ_RXCLK_8X			0x00400000
#define MQ_RXCLK_16X			0x00600000
#define MQ_RXCLK_32X			0x00800000
#define MQ_RXCLK_64X			0x00A00000
#define MQ_TXBAUD_ON 			0x01000000
#define MQ_RXBAUD_ON 			0x02000000
#define MQ_UART_MODE			0x00000000
#define MQ_GPIO_MODE			0x20000000
#define MQ_UART_ENABLE			0x80000000

/* UART Clock Control 2 */
#define MQ_TXCLK_DIV_MASK		0x0000FFFF
#define MQ_RXCLK_DIV_MASK		0xFFFF0000

/* UART Transmit Control Register */
#define MQ_TXCHAR_MASK			0x0000000F
#define MQ_TXCHAR_8			0x00000000
#define MQ_TXCHAR_7			0x00000001
#define MQ_TXCHAR_6			0x00000002
#define MQ_TXCHAR_5			0x00000003
#define MQ_TXSTOP_BIT1			0x00000000
#define MQ_TXSTOP_BIT2			0x00000008
#define MQ_TXSTOP_MASK			0x0000000C
#define MQ_TXPARITY_ON			0x00000010
#define MQ_TXODD_PARITY			0x00000000
#define MQ_TXEVEN_PARITY		0x00000020
#define MQ_TXBREAK_ON			0x00000040
#define MQ_TXCTS_ON			0x00000080
#define MQ_TXFIFO_THLD_MASK		0x00003F00
#define MQ_TXDMA_RESET			0x00008000
#define MQ_TXFIFO_RESET			0x00010000
#define MQ_TXECHO_ON			0x00020000
#define MQ_TX_DISABLE			0x00000000
#define MQ_TX_ENABLE			0x00040000
#define MQ_TXDMA_ON			0x00080000
#define MQ_TX_RESET			0x00100000

/* UART Receive Control Register */
#define MQ_RXCHAR_MASK			0x0000000F
#define MQ_RXCHAR_8			0x00000000
#define MQ_RXCHAR_7			0x00000001
#define MQ_RXCHAR_6			0x00000002
#define MQ_RXCHAR_5			0x00000003
#define MQ_RXPARITY_ON			0x00000004
#define MQ_RXODD_PARITY			0x00000000
#define MQ_RXEVEN_PARITY		0x00000008
#define MQ_RXRTS_ON			0x00000010
#define MQ_RXRTS_THLD_1			0x00000000
#define MQ_RXRTS_THLD_64		0x00000020
#define MQ_RXDCD_ON			0x00000040
#define MQ_RXFIFO_THLD_MASK		0x00003F00
#define MQ_RXFIFO_RESET			0x00004000
#define MQ_RXDMA_RESET			0x00008000
#define MQ_RX_RESET			0x10000000
#define MQ_RXLOOP_ON			0x20000000
#define MQ_RXDMA_ON			0x40000000
#define MQ_RX_ENABLE			0x80000000

/* UART Interrupt Control/Status Registers */
#define MQ_TXFIFO_INTR			0x00000001
#define MQ_TXUNDERRUN_INTR		0x00000002
#define MQ_RXFIFO_INTR			0x00000004
#define MQ_RXMISC_INTR			0x00000008
#define MQ_RXBREAK_INTR			0x00000010
#define MQ_MODEM_INTR			0x00000020
//#define MQ_TIMEOUT_INTR		0x00000040
#define MQ_TXDONE_INTR			0x00000080
#define MQ_TXFIFO_EMPTY			0x00000100
#define MQ_RXFIFO_EMPTY			0x00000200
#define MQ_CTS_INTR			0x00000400
#define MQ_DCD_INTR			0x00000800
#define MQ_TXFIFO_NSLOT_MASK		0x0003F000
#define MQ_RXFIFO_NSLOT_MASK		0x01FC0000
#define MQ_TXFIFO_FULL			0x00020000
#define MQ_RXFIFO_FULL			0x01000000
#define MQ_CTS_PIN_LOW			0x02000000
#define MQ_DCD_PIN_LOW			0x04000000

/* UART Data & Buffer Registers */
#define MQ_RXPARITY_ERR			0x00000100
#define MQ_RXFRAME_ERR			0x00000200
#define MQ_RXBREAK_ERR			0x00000400
#define MQ_TXDATA_MASK			0xFFFFFFFF
#define MQ_RXDATA_MASK			0x000008FF
#define MQ_TRBUF_START_MASK		0x00FFFFFF
#define MQ_TRBUF_SIZE_MASK		0x00000FFF

typedef volatile struct MQKTUARTRegBlock {
	unsigned long	clock1;
	unsigned long	clock2;
	unsigned long	txCntrl;		// transmiter
	unsigned long	rxCntrl;		// receiver
	unsigned long	inCntrl;		// interrupt
	unsigned long 	inStat;
	unsigned long	txData;
	unsigned long	txBufStart;
	unsigned long	txBufSize;
	unsigned long	rxData;
	unsigned long	rxBufStart;
	unsigned long	rxBufSize;
} MQKTUARTReg;

typedef struct MQKTBaudRateType {
	int baudrate;					// COM baud rate
	unsigned long   pValue; 		// programmed value   
} MQKTBaudRate;

#endif
