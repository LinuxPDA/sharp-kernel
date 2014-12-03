/*
 *  MQK_spi.h : MediaQ MQ9000 SPI module header.
 *
 *  Copyright (c) 2002 by MediaQ, Incorporated.
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

#ifndef	_MQK_SPI_H_
#define	_MQK_SPI_H_

// Define values 
#define SAMPLES_RATE_LOW	100	// 10 ms
#define SAMPLES_RATE_HIGH	125	// 8 ms
#define POWER_ON		FALSE
#define POWER_OFF		TRUE
#define uREGW			4

// Revision ID
#define MQ122_REV_B0		0x0
#define MQ122_REV_B1		0x2
#define MQ122_REV_B2		0x4

//********************************************************************
// CPU control registers
//********************************************************************

// 32 bits Registers
#define GPIO_CNTRL0_REG			(0x02 * uREGW)

//-----------------------------------------------------------------------
// GPIO_CNTRL0_REG			(0x02 * uREGW)	// CC02R GPIO Control Register
//-----------------------------------------------------------------------
#define GPIO2_IN_ENABLE			0x00000100 // Enable input mode for GPIO2	
#define GPIO2_IN_DATA                   0x00000800 // Data bit
					
//********************************************************************
// PCI registers
//********************************************************************
#define PCI_REV_CLASS			(0x02 * uREGW)

//********************************************************************
// Interrupt control registers
//********************************************************************

// 32 bits Registers
// #define INT_CNTRL_REG		(0x00 * uREGW)
// #define INT_MASK_REG			(0x01 * uREGW)
// #define INT_STATUS_REG		(0x02 * uREGW)
// #define INT_RAW_STATUS_REG		(0x03 * uREGW)

//-----------------------------------------------------------------------
// INT_CNTRL_REG -- Global Interrupt Control Register (0x00 * uREGW) // IN00R
//-----------------------------------------------------------------------
#define IRQ_ENABLE		 	0x00000001 // Enables nIRQ Interrupt to ARM CPU
#define FIQ_ENABLE		 	0x00000002 // Enables nFIQ Interrupt to ARM CPU

/// #define INT_POLARITY_HIGH	0x00000002 // Interrupt is active high
/// #define INT_POLARITY_LOW    0x00000000 // Interrupt is active low
/// #define INT_GPIO2_LEVEL_1   0x00000010 // Interrupt as GPIO2 is at level 1
/// #define INT_GPIO2_LEVEL_0   0x00000000 // Interrupt as GPIO2 is at level 0

//-----------------------------------------------------------------------
// IRQ Interrupt Mask Register (0x01 * uREGW) // IN01R
//-----------------------------------------------------------------------
#define UM_TIMER			0x00002000 // Timer, GP interrupt, GPIO119 for Touch Panel 
#define UM_SPI				0x00010000 // SPI interrupt mask

//-----------------------------------------------------------------------
// INT_STATUS_REG -- Interrupt Status Register (0x04 * uREGW) // IN04R
//-----------------------------------------------------------------------
#define ST_TIMER			0x00002000 // Interrupt event GPIO119 is true
#define ST_SPI				0x00010000 // SPI interrupt event is true

//********************************************************************
// Timer, GPIO module
//********************************************************************
//
//-----------------------------------------------------------------------
// GP00R 
//-----------------------------------------------------------------------
#define TIMER_ENABLE			0x00000001L
//-----------------------------------------------------------------------
// GP17R 
//-----------------------------------------------------------------------
#define GPIO119_IN_ENABLE		0x00000000L
#define GPIO119_OUT_ENABLE		0x00000008L
#define GPIO119_DATA			0x00008000L
#define GPIO119_PULLUP_ENABLE		0x00080000L
#define GPIO119_PULLDOWN_ENABLE		0x00800000L
#define GPIO119_GP17R_BITS		0x88888C08L

//-----------------------------------------------------------------------
// GP18R 
//-----------------------------------------------------------------------
#define GPIO119_INT_ENABLE		0x00000008L
#define GPIO119_INT_POLARITY_10		0x00000000L
#define GPIO119_INT_POLARITY_01		0x00000400L
#define GPIO119_INT_POLARITY_01_10	0x00000800L
#define GPIO119_GP18R_BITS		0x00000C08L

//-----------------------------------------------------------------------
// GP19R 
//-----------------------------------------------------------------------
#define GPIO119_INT_STATUS		0x00000008L

//-----------------------------------------------------------------------
// GP1AR 
//-----------------------------------------------------------------------
#define GPIO119_ENABLE			0x00100000L

//********************************************************************
// SPI module
//********************************************************************

// 32 bits Regsiters
#define SPIM_CNTRL_REG			(0x00 * uREGW)				// SP00R Control Register
#define SPIM_ENABLE_MASK_REG		(0x01 * uREGW)				// SP01R Status Register
#define SPIM_DATA_REG			(0x02 * uREGW)				// SP02R Data Register
#define SPIM_TGCMP_REG			(0x03 * uREGW)				// SP03R Time Gap Comprare Register
#define SPIM_GPIO_REG			(0x04 * uREGW)				// SP04R GPIO Mode Data /Direction Register
#define SPIM_DCNT_REG			(0x05 * uREGW)				// SP05R Data Count Register
#define SPIM_STATUS_REG			(0x06 * uREGW)				// SP06R FIFO Threshold Register
#define SPIM_LEDCTL_REG			(0x07 * uREGW)				// SP07R LED Control Register


//-----------------------------------------------------------------------
// SPIM_CNTRL_REG		(0x00 * uREGW)	// SP00R Control Register
//-----------------------------------------------------------------------
#define TX_FIFO_SW_RESET_ENABLE         0x00000001L     // SPI SW reset for transmit FIFO enabled
#define RV_FIFO_SW_RESET_ENABLE         0x00000002L     // SPI SW reset for receive FIFO enabled
#define MASTER_ENABLE                   0x00000000L     // SPIM is in master mode
#define SLAVE_ENABLE			0x00000004L     // SPIM is in slave mode
#define LSB_TX_FIRST			0x00000000L     // LSB is first bit transferred
#define MSB_TX_FIRST			0x00000008L     // MSB is first bit transferred
#define DTS_8BITS			0x00000000L		// Transfer size is a byte
#define DTS_16BITS			0x00000010L     // Transfer size is a word
#define DTS_16BITS_MASK			0xFFFFFFEFL     // Data size 16 bits mask
#define SCK_POLARITY_LOW		0x00000000L     // SCK pin is low when inactive
#define SCK_POLARITY_HIGH               0x00000020L     // SCK pin is high when inactive
#define SCK_PHASE_0			0x00000000L     // SCK phase 0
#define SCK_PHASE_1			0x00000040L     // SCK phase 1
#define SS_MODE				0x00000080L     // Mode selection bit for SS Pin
#define SS_FAULT_MODE                   0x00000100L     // Mode fault selection bit for SS Pin
#define SINGLE_TX			0x00000000L	// Single transfer mode enabled
#define CONTINUOUS_TX			0x00000200L     // Continuous transfer mode disabled
#define SRCLK_PLL1                      0x00000000L     // Select PLL1 clock
#define SRCLK_PLL2			0x00000400L	// Select PLL2 clock
#define SPI_FDx_1			0x00000000L	// FD = 1
#define SPI_FDx_2                       0x00010000L     // FD = 2
#define SPI_FDx_3                       0x00020000L     // FD = 3
#define SPI_FDx_4                       0x00030000L     // FD = 4
#define SPI_FDx_5                       0x00040000L     // FD = 5
#define SPI_FDx_6                       0x00050000L     // FD = 6
#define SPI_FDx_7                       0x00060000L     // FD = 7
#define SPI_FDx_8                       0x00070000L     // FD = 8
#define SPI_FDx_9                       0x00080000L     // FD = 9
#define SPI_FDx_10                      0x00090000L     // FD = 10
#define SPI_FDx_11                      0x000A0000L     // FD = 11
#define SPI_FDx_12                      0x000B0000L     // FD = 12
#define SPI_FDx_13                      0x000C0000L     // FD = 13
#define SPI_FDx_14                      0x000D0000L     // FD = 14
#define SPI_FDx_15                      0x000E0000L     // FD = 15
#define SPI_FDx_16                      0x000F0000L     // FD = 16
#define SPI_SDx_1                       0x00000000L     // SD = 1
#define SPI_SDx_2                       0x00100000L     // SD = 2
#define SPI_SDx_3                       0x00200000L     // SD = 3
#define SPI_SDx_4                       0x00300000L     // SD = 4
#define SPI_SDx_5                       0x00400000L     // SD = 5
#define SPI_SDx_6                       0x00500000L     // SD = 6
#define SPI_SDx_7                       0x00600000L     // SD = 7
#define SPI_SDx_8                       0x00700000L     // SD = 8
#define SPI_SDx_9                       0x00800000L     // SD = 9
#define SPI_SDx_10                      0x00900000L     // SD = 10
#define SPI_SDx_11                      0x00A00000L     // SD = 11
#define SPI_SDx_12                      0x00B00000L     // SD = 12
#define SPI_SDx_13                      0x00C00000L     // SD = 13
#define SPI_SDx_14                      0x00D00000L     // SD = 14
#define SPI_SDx_15                      0x00E00000L     // SD = 15
#define SPI_SDx_16                      0x00F00000L     // SD = 16


//-----------------------------------------------------------------------
// SPIM_ENABLE_MASK_REG		(0x01 * uREGW)	// SP01R Enable Mask Register
//-----------------------------------------------------------------------
#define GPIO_MODE_ENABLE	 	0x00000000L	// Enable GPIO mode
#define SPI_MODE_ENABLE			0x00000001L	// Enable SPI mode
#define TEST_MODE1_ENABLE		0x00000002L     // Enable test mode 1
#define TEST_MODE2_ENABLE		0x00000003L     // Enable test mode 2
#define TX_PAD_DISABLE			0x00000000L     // Transmit PAD disabled
#define TX_PAD_ENABLE			0x00000004L     // Transmit PAD enabled
#define RV_FUNC_DISABLE			0x00000000L     // Receive function disabled
#define RV_FUNC_ENABLE			0x00000008L     // Receive function enabled
#define SPI_TRANS_ENABLE		0x00000010L  	// SPI transfer enabled
#define SPI_INT_DISABLE			0x00000000L	// SPI interrupt disabled
#define SPI_INT_ENABLE			0x00000040L     // SPI interrupt enabled
#define UM_SPI_TX_COMPLETE		0x00000100L	// SPI transfer complete not masked
#define UM_FAULT_MODE			0x00000200L     // SPI fault not masked
#define TX_FTHR_INT_NO_MASK   		0x00000400L	// Transmit FIFO Threshold Interrupt not masked
#define TX_FIFO_UNDERFLOW_INT_NO_MASK   0x00000800L     // Transmit FIFO Underflow Interrupt not masked
#define TX_FTHR_FULL			0x00000000L     // Transmit FIFO Threshold is full
#define TX_FTHR_EMPTY_16BIT_MODE 	0x00200000L     // Transmit FIFO Threshold empty, 32x16 bits free
#define TX_FTHR_EMPTY_8BIT_MODE		0x00400000L     // Transmit FIFO Threshold empty, 64x8 bits free
#define RV_FTHR_EMPTY			0x00000000L     // Receive FIFO Threshold is full
#define RV_FTHR_FULL_16BIT_MODE 	0x20000000L     // Receive FIFO Threshold full, 32x16 bits free
#define RV_FTHR_FULL_8BIT_MODE		0x40000000L 	// Receive FIFO Threshold full, 64x8 bits free


//-----------------------------------------------------------------------
// SPIM_DAT_REG		(0x02 * uREGW)	// SP02R Data Register
//-----------------------------------------------------------------------
#define LOWER_BYTE_MASK			0x0000000FL
#define UPPER_BYTE_MASK			0x000000F0L
#define DATA_REG_MASK			0x000000FFL


//============================================================================
// SPIM_TGCMP_REG			(0x03 * uREGW)  // SP03R Time Gap Compare Register
//============================================================================


//============================================================================
// SPIM_GPIO_REG			(0x04 * uREGW)  // SP04R GPIO Mode Data/Direction Register
//============================================================================
#define SS_OUT_ENABLE			0x00000010L     // Slave select pin output enable
#define SS_IN_OUT_ENABLE		0x00000030L     // Slave select pin output enable
#define SS_DATA_HIGH			0x00000008L
#define SCK_OUT_ENABLE          	0x00000040L     // Serial clock pin output enable
#define SCK_IN_OUT_ENABLE       	0x000000C0L     // Serial clock pin output enable
#define SCK_DATA_HIGH           	0x00000001L
#define MISO_OUT_ENABLE         	0x00000400L     // MISO pin output enable
#define MISO_IN_ENABLE          	0x00000800L     // MISO pin input enable
#define MISO_DATA_HIGH          	0x00000004L
#define MOSI_OUT_ENABLE         	0x00000100L     // MOSI pin output enable
#define MOSI_IN_ENABLE 			0x00000200L     // MOSI pin input enable
#define MOSI_IN_OUT_ENABLE      	0x00000300L     // MOSI pin input and output enable
#define MOSI_DATA_HIGH          	0x00000002L
#define GPIO74_IN_OUT_DISABLE		0x00000000L
#define GPIO74_OUT_ENABLE		0x00040000L
#define GPIO74_IN_ENABLE		0x00080000L
#define GPIO74_IN_OUT_ENABLE		0x000C0000L
#define GPIO74_DATA_HIGH        	0x00010000L
#define PULLUP_REGS_INACTIVE    	0x0000F000L     // Pull up registers are inactive

#define MISO_DATA_IN			0x00100000L	// MISO data in
#define SCK_DATA_IN			0x00200000L	// SCLK data in
#define MOSI_DATA_IN			0x00400000L	// MOSI data in
//============================================================================
// SPIM_DCNT_REG			(0x05 * uREGW) // SP05R Data Count Register
//============================================================================
#define CONT_TRANS_MODE			0x00000080L     // Continuous transfer mode bit


//============================================================================
// SPIM_STATUS_REG			(0x06 * uREGW) // SP06R Status Register
//============================================================================
#define SPI_TRANS_COMPLETE		0x00000100L 	// SPI transfer is completed
#define FAULT_MODE		 	0x00000200L	// Fault mode is detected
#define TX_FTHR_INT			0x00000400L	// Transmit FIFO Threshold Interrupt True
#define TX_FIFO_UNDERFLOW		0x00000800L     // Transmit FIFO Underflow True
#define TX_FIFO_FULL			0x00000000L     // Transmit FIFO is full
#define TX_FIFO_NOT_FULL		0x007F0000L	// Transmit FIFO is not full
#define TX_FIFO_COUNT_MASK		0x007F0000L     // Transmit FIFO count mask
#define TX_FIFO_EMPTY_16BIT_MODE	0x00200000L	// Transmit FIFO empty, 32x16 bits free
#define TX_FIFO_EMPTY_8BIT_MODE		0x00400000L     // Transmit FIFO empty, 64x8 bits free
#define RV_FIFO_EMPTY			0x00000000L     // Receive FIFO is empty
#define RV_FIFO_EMPTY_16BIT_MODE	0x00000000L	// Receive FIFO empty, 32x16 bits free
#define RV_FIFO_EMPTY_8BIT_MODE		0x00000000L     // Receive FIFO empty, 64x8 bits free
#define RV_FIFO_COUNT_MASK		0x7F000000L     // Receive FIFO count mask
#define RV_FIFO_OCCUPY_3		0x03000000L
#define RV_FIFO_OCCUPY_13		0x0D000000L

//============================================================================
// LED_CTRL_REG				(0x07 * uREGW)	// SP07R LED Control Register
//============================================================================
#define LED_MODE_ENABLE			0x00000001L 	// LED mode enabled
#define LED1_FDx_1			0x00000000L     // LED1 FD = 1
#define LED1_FDx_2			0x00000010L	// LED1 FD = 2
#define LED1_FDx_3			0x00000020L     // LED1 FD = 3
#define LED1_FDx_4			0x00000030L     // LED1 FD = 4
#define LED1_FDx_5			0x00000040L     // LED1 FD = 5
#define LED1_FDx_6			0x00000050L     // LED1 FD = 6
#define LED1_FDx_7			0x00000060L     // LED1 FD = 7
#define LED1_FDx_8			0x00000070L     // LED1 FD = 8
#define LED1_FDx_9			0x00000080L     // LED1 FD = 9
#define LED1_FDx_10			0x00000090L     // LED1 FD = 10
#define LED1_FDx_11			0x000000A0L     // LED1 FD = 11
#define LED1_FDx_12			0x000000B0L     // LED1 FD = 12
#define LED1_FDx_13			0x000000C0L     // LED1 FD = 13
#define LED1_FDx_14			0x000000D0L     // LED1 FD = 14
#define LED1_FDx_15			0x000000E0L     // LED1 FD = 15
#define LED1_FDx_16			0x000000F0L     // LED1 FD = 16
#define LED2_FDx_1			0x00000000L     // LED2 FD = 1
#define LED2_FDx_2			0x00000100L	// LED2 FD = 2
#define LED2_FDx_3			0x00000200L     // LED2 FD = 3
#define LED2_FDx_4			0x00000300L     // LED2 FD = 4
#define LED2_FDx_5			0x00000400L     // LED2 FD = 5
#define LED2_FDx_6			0x00000500L     // LED2 FD = 6
#define LED2_FDx_7			0x00000600L     // LED2 FD = 7
#define LED2_FDx_8			0x00000700L     // LED2 FD = 8
#define LED2_FDx_9			0x00000800L     // LED2 FD = 9
#define LED2_FDx_10			0x00000900L     // LED2 FD = 10
#define LED2_FDx_11			0x00000A00L     // LED2 FD = 11
#define LED2_FDx_12			0x00000B00L     // LED2 FD = 12
#define LED2_FDx_13			0x00000C00L     // LED2 FD = 13
#define LED2_FDx_14			0x00000D00L     // LED2 FD = 14
#define LED2_FDx_15			0x00000E00L     // LED2 FD = 15
#define LED2_FDx_16			0x00000F00L     // LED2 FD = 16
#define LED3_FDx_1			0x00000000L     // LED3 FD = 1
#define LED3_FDx_2			0x00001000L	// LED3 FD = 2
#define LED3_FDx_3			0x00002000L     // LED3 FD = 3
#define LED3_FDx_4			0x00003000L     // LED3 FD = 4
#define LED3_FDx_5			0x00004000L     // LED3 FD = 5
#define LED3_FDx_6			0x00005000L     // LED3 FD = 6
#define LED3_FDx_7			0x00006000L     // LED3 FD = 7
#define LED3_FDx_8			0x00007000L     // LED3 FD = 8
#define LED3_FDx_9			0x00008000L     // LED3 FD = 9
#define LED3_FDx_10			0x00009000L     // LED3 FD = 10
#define LED3_FDx_11			0x0000A000L     // LED3 FD = 11
#define LED3_FDx_12			0x0000B000L     // LED3 FD = 12
#define LED3_FDx_13			0x0000C000L     // LED3 FD = 13
#define LED3_FDx_14			0x0000D000L     // LED3 FD = 14
#define LED3_FDx_15			0x0000E000L     // LED3 FD = 15
#define LED3_FDx_16			0x0000F000L     // LED3 FD = 16
#define LED4_FDx_1			0x00000000L     // LED4 FD = 1
#define LED4_FDx_2			0x00010000L	// LED4 FD = 2
#define LED4_FDx_3			0x00020000L     // LED4 FD = 3
#define LED4_FDx_4			0x00030000L     // LED4 FD = 4
#define LED4_FDx_5			0x00040000L     // LED4 FD = 5
#define LED4_FDx_6			0x00050000L     // LED4 FD = 6
#define LED4_FDx_7			0x00060000L     // LED4 FD = 7
#define LED4_FDx_8			0x00070000L     // LED4 FD = 8
#define LED4_FDx_9			0x00080000L     // LED4 FD = 9
#define LED4_FDx_10			0x00090000L     // LED4 FD = 10
#define LED4_FDx_11			0x000A0000L     // LED4 FD = 11
#define LED4_FDx_12			0x000B0000L     // LED4 FD = 12
#define LED4_FDx_13			0x000C0000L     // LED4 FD = 13
#define LED4_FDx_14			0x000D0000L     // LED4 FD = 14
#define LED4_FDx_15			0x000E0000L     // LED4 FD = 15
#define LED4_FDx_16			0x000F0000L     // LED4 FD = 16
#define LED5_FDx_1			0x00000000L     // LED5 FD = 1
#define LED5_FDx_2			0x00100000L	// LED5 FD = 2
#define LED5_FDx_3			0x00200000L     // LED5 FD = 3
#define LED5_FDx_4			0x00300000L     // LED5 FD = 4
#define LED5_FDx_5			0x00400000L     // LED5 FD = 5
#define LED5_FDx_6			0x00500000L     // LED5 FD = 6
#define LED5_FDx_7			0x00600000L     // LED5 FD = 7
#define LED5_FDx_8			0x00700000L     // LED5 FD = 8
#define LED5_FDx_9			0x00800000L     // LED5 FD = 9
#define LED5_FDx_10			0x00900000L     // LED5 FD = 10
#define LED5_FDx_11			0x00A00000L     // LED5 FD = 11
#define LED5_FDx_12			0x00B00000L     // LED5 FD = 12
#define LED5_FDx_13			0x00C00000L     // LED5 FD = 13
#define LED5_FDx_14			0x00D00000L     // LED5 FD = 14
#define LED5_FDx_15			0x00E00000L     // LED5 FD = 15
#define LED5_FDx_16			0x00F00000L     // LED5 FD = 16
#define LED6_FDx_1			0x00000000L     // LED6 FD = 1
#define LED6_FDx_2			0x01000000L	// LED6 FD = 2
#define LED6_FDx_3			0x02000000L     // LED6 FD = 3
#define LED6_FDx_4			0x03000000L     // LED6 FD = 4
#define LED6_FDx_5			0x04000000L     // LED6 FD = 5
#define LED6_FDx_6			0x05000000L     // LED6 FD = 6
#define LED6_FDx_7			0x06000000L     // LED6 FD = 7
#define LED6_FDx_8			0x07000000L     // LED6 FD = 8
#define LED6_FDx_9			0x08000000L     // LED6 FD = 9
#define LED6_FDx_10			0x09000000L     // LED6 FD = 10
#define LED6_FDx_11			0x0A000000L     // LED6 FD = 11
#define LED6_FDx_12			0x0B000000L     // LED6 FD = 12
#define LED6_FDx_13			0x0C000000L     // LED6 FD = 13
#define LED6_FDx_14			0x0D000000L     // LED6 FD = 14
#define LED6_FDx_15			0x0E000000L     // LED6 FD = 15
#define LED6_FDx_16			0x0F000000L     // LED6 FD = 16
#define LED7_FDx_1			0x00000000L     // LED7 FD = 1
#define LED7_FDx_2			0x10000000L	// LED7 FD = 2
#define LED7_FDx_3			0x20000000L     // LED7 FD = 3
#define LED7_FDx_4			0x30000000L     // LED7 FD = 4
#define LED7_FDx_5			0x40000000L     // LED7 FD = 5
#define LED7_FDx_6			0x50000000L     // LED7 FD = 6
#define LED7_FDx_7			0x60000000L     // LED7 FD = 7
#define LED7_FDx_8			0x70000000L     // LED7 FD = 8
#define LED7_FDx_9			0x80000000L     // LED7 FD = 9
#define LED7_FDx_10			0x90000000L     // LED7 FD = 10
#define LED7_FDx_11			0xA0000000L     // LED7 FD = 11
#define LED7_FDx_12			0xB0000000L     // LED7 FD = 12
#define LED7_FDx_13			0xC0000000L     // LED7 FD = 13
#define LED7_FDx_14			0xD0000000L     // LED7 FD = 14
#define LED7_FDx_15			0xE0000000L     // LED7 FD = 15
#define LED7_FDx_16			0xF0000000L     // LED7 FD = 16

//============================================================================
// ADS7846 Control Register 
//============================================================================
#define SPI_ADS7846_S_BIT_MASK		0x00000080L
#define SPI_ADS7846_START		0x00000080L

#define SPI_ADS7846_CHANNEL_SELECT_MASK	0x00000070L
#define SPI_ADS7846_A2			0x00000040L
#define SPI_ADS7846_A1			0x00000020L
#define SPI_ADS7846_A0			0x00000010L

#define SPI_ADS7846_MODE_MASK		0x00000008L
#define SPI_ADS7846_8BIT		0x00000008L 	// 8 bit conversion bit
#define SPI_ADS7846_12BIT		0x00000000L     // 12 bit conversion bit

#define SPI_ADS7846_SER_DFR_MASK	0x00000004L
#define SPI_ADS7846_SER			0x00000004L
#define SPI_ADS7846_DFR			0x00000000L

#define SPI_ADS7846_PD1_MASK		0x00000002L
#define SPI_ADS7846_PD0_MASK		0x00000001L
#define SPI_ADS7846_ALWAYS_POWERED	0x00000003L
#define SPI_ADS7846_REF_ON_ADC_OFF	0x00000002L
#define SPI_ADS7846_REF_OFF_ADC_ON	0x00000001L
#define SPI_ADS7846_BETWEEN_CONVERSIONS	0x00000000L

#endif // _MQK_SPI_H_
