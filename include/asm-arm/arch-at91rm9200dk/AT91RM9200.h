// ----------------------------------------------------------------------------
//          ATMEL Microcontroller Software Support  -  ROUSSET  -
// ----------------------------------------------------------------------------
//  The software is delivered "AS IS" without warranty or condition of any
//  kind, either express, implied or statutory. This includes without
//  limitation any warranty or condition with respect to merchantability or
//  fitness for any particular purpose, or against the infringements of
//  intellectual property rights of others.
// ----------------------------------------------------------------------------
// File Name           : AT91RM9200.h
// Object              : AT91RM9200 / SYS definitions
// Generated           : AT91 SW Application Group  01/17/2003 (13:41:22)
//
// ----------------------------------------------------------------------------

#ifndef AT91RM9200_H
#define AT91RM9200_H


#ifndef __ASSEMBLY__

typedef volatile unsigned int AT91_REG;// Hardware register definition

//              SOFTWARE API DEFINITION  FOR System Peripherals

typedef struct _AT91S_SYS {
	AT91_REG	 AIC_SMR[32]; 	// Source Mode Register
	AT91_REG	 AIC_SVR[32]; 	// Source Vector Register
	AT91_REG	 AIC_IVR; 	// IRQ Vector Register
	AT91_REG	 AIC_FVR; 	// FIQ Vector Register
	AT91_REG	 AIC_ISR; 	// Interrupt Status Register
	AT91_REG	 AIC_IPR; 	// Interrupt Pending Register
	AT91_REG	 AIC_IMR; 	// Interrupt Mask Register
	AT91_REG	 AIC_CISR; 	// Core Interrupt Status Register
	AT91_REG	 Reserved0[2]; 	//
	AT91_REG	 AIC_IECR; 	// Interrupt Enable Command Register
	AT91_REG	 AIC_IDCR; 	// Interrupt Disable Command Register
	AT91_REG	 AIC_ICCR; 	// Interrupt Clear Command Register
	AT91_REG	 AIC_ISCR; 	// Interrupt Set Command Register
	AT91_REG	 AIC_EOICR; 	// End of Interrupt Command Register
	AT91_REG	 AIC_SPU; 	// Spurious Vector Register
	AT91_REG	 AIC_DCR; 	// Debug Control Register (Protect)
	AT91_REG	 Reserved1[1]; 	//
	AT91_REG	 AIC_FFER; 	// Fast Forcing Enable Register
	AT91_REG	 AIC_FFDR; 	// Fast Forcing Disable Register
	AT91_REG	 AIC_FFSR; 	// Fast Forcing Status Register
	AT91_REG	 Reserved2[45]; 	//
	AT91_REG	 DBGU_CR; 	// Control Register
	AT91_REG	 DBGU_MR; 	// Mode Register
	AT91_REG	 DBGU_IER; 	// Interrupt Enable Register
	AT91_REG	 DBGU_IDR; 	// Interrupt Disable Register
	AT91_REG	 DBGU_IMR; 	// Interrupt Mask Register
	AT91_REG	 DBGU_CSR; 	// Channel Status Register
	AT91_REG	 DBGU_RHR; 	// Receiver Holding Register
	AT91_REG	 DBGU_THR; 	// Transmitter Holding Register
	AT91_REG	 DBGU_BRGR; 	// Baud Rate Generator Register
	AT91_REG	 Reserved3[7]; 	//
	AT91_REG	 DBGU_C1R; 	// Chip ID1 Register
	AT91_REG	 DBGU_C2R; 	// Chip ID2 Register
	AT91_REG	 DBGU_FNTR; 	// Force NTRST Register
	AT91_REG	 Reserved4[45]; 	//
	AT91_REG	 DBGU_RPR; 	// Receive Pointer Register
	AT91_REG	 DBGU_RCR; 	// Receive Counter Register
	AT91_REG	 DBGU_TPR; 	// Transmit Pointer Register
	AT91_REG	 DBGU_TCR; 	// Transmit Counter Register
	AT91_REG	 DBGU_RNPR; 	// Receive Next Pointer Register
	AT91_REG	 DBGU_RNCR; 	// Receive Next Counter Register
	AT91_REG	 DBGU_TNPR; 	// Transmit Next Pointer Register
	AT91_REG	 DBGU_TNCR; 	// Transmit Next Counter Register
	AT91_REG	 DBGU_PTCR; 	// PDC Transfer Control Register
	AT91_REG	 DBGU_PTSR; 	// PDC Transfer Status Register
	AT91_REG	 Reserved5[54]; 	//
	AT91_REG	 PIOA_PER; 	// PIO Enable Register
	AT91_REG	 PIOA_PDR; 	// PIO Disable Register
	AT91_REG	 PIOA_PSR; 	// PIO Status Register
	AT91_REG	 Reserved6[1]; 	//
	AT91_REG	 PIOA_OER; 	// Output Enable Register
	AT91_REG	 PIOA_ODR; 	// Output Disable Registerr
	AT91_REG	 PIOA_OSR; 	// Output Status Register
	AT91_REG	 Reserved7[1]; 	//
	AT91_REG	 PIOA_IFER; 	// Input Filter Enable Register
	AT91_REG	 PIOA_IFDR; 	// Input Filter Disable Register
	AT91_REG	 PIOA_IFSR; 	// Input Filter Status Register
	AT91_REG	 Reserved8[1]; 	//
	AT91_REG	 PIOA_SODR; 	// Set Output Data Register
	AT91_REG	 PIOA_CODR; 	// Clear Output Data Register
	AT91_REG	 PIOA_ODSR; 	// Output Data Status Register
	AT91_REG	 PIOA_PDSR; 	// Pin Data Status Register
	AT91_REG	 PIOA_IER; 	// Interrupt Enable Register
	AT91_REG	 PIOA_IDR; 	// Interrupt Disable Register
	AT91_REG	 PIOA_IMR; 	// Interrupt Mask Register
	AT91_REG	 PIOA_ISR; 	// Interrupt Status Register
	AT91_REG	 PIOA_MDER; 	// Multi-driver Enable Register
	AT91_REG	 PIOA_MDDR; 	// Multi-driver Disable Register
	AT91_REG	 PIOA_MDSR; 	// Multi-driver Status Register
	AT91_REG	 Reserved9[1]; 	//
	AT91_REG	 PIOA_PPUDR; 	// Pull-up Disable Register
	AT91_REG	 PIOA_PPUER; 	// Pull-up Enable Register
	AT91_REG	 PIOA_PPUSR; 	// Pad Pull-up Status Register
	AT91_REG	 Reserved10[1]; 	//
	AT91_REG	 PIOA_ASR; 	// Select A Register
	AT91_REG	 PIOA_BSR; 	// Select B Register
	AT91_REG	 PIOA_ABSR; 	// AB Select Status Register
	AT91_REG	 Reserved11[9]; 	//
	AT91_REG	 PIOA_OWER; 	// Output Write Enable Register
	AT91_REG	 PIOA_OWDR; 	// Output Write Disable Register
	AT91_REG	 PIOA_OWSR; 	// Output Write Status Register
	AT91_REG	 Reserved12[85]; 	//
	AT91_REG	 PIOB_PER; 	// PIO Enable Register
	AT91_REG	 PIOB_PDR; 	// PIO Disable Register
	AT91_REG	 PIOB_PSR; 	// PIO Status Register
	AT91_REG	 Reserved13[1]; 	//
	AT91_REG	 PIOB_OER; 	// Output Enable Register
	AT91_REG	 PIOB_ODR; 	// Output Disable Registerr
	AT91_REG	 PIOB_OSR; 	// Output Status Register
	AT91_REG	 Reserved14[1]; 	//
	AT91_REG	 PIOB_IFER; 	// Input Filter Enable Register
	AT91_REG	 PIOB_IFDR; 	// Input Filter Disable Register
	AT91_REG	 PIOB_IFSR; 	// Input Filter Status Register
	AT91_REG	 Reserved15[1]; 	//
	AT91_REG	 PIOB_SODR; 	// Set Output Data Register
	AT91_REG	 PIOB_CODR; 	// Clear Output Data Register
	AT91_REG	 PIOB_ODSR; 	// Output Data Status Register
	AT91_REG	 PIOB_PDSR; 	// Pin Data Status Register
	AT91_REG	 PIOB_IER; 	// Interrupt Enable Register
	AT91_REG	 PIOB_IDR; 	// Interrupt Disable Register
	AT91_REG	 PIOB_IMR; 	// Interrupt Mask Register
	AT91_REG	 PIOB_ISR; 	// Interrupt Status Register
	AT91_REG	 PIOB_MDER; 	// Multi-driver Enable Register
	AT91_REG	 PIOB_MDDR; 	// Multi-driver Disable Register
	AT91_REG	 PIOB_MDSR; 	// Multi-driver Status Register
	AT91_REG	 Reserved16[1]; 	//
	AT91_REG	 PIOB_PPUDR; 	// Pull-up Disable Register
	AT91_REG	 PIOB_PPUER; 	// Pull-up Enable Register
	AT91_REG	 PIOB_PPUSR; 	// Pad Pull-up Status Register
	AT91_REG	 Reserved17[1]; 	//
	AT91_REG	 PIOB_ASR; 	// Select A Register
	AT91_REG	 PIOB_BSR; 	// Select B Register
	AT91_REG	 PIOB_ABSR; 	// AB Select Status Register
	AT91_REG	 Reserved18[9]; 	//
	AT91_REG	 PIOB_OWER; 	// Output Write Enable Register
	AT91_REG	 PIOB_OWDR; 	// Output Write Disable Register
	AT91_REG	 PIOB_OWSR; 	// Output Write Status Register
	AT91_REG	 Reserved19[85]; 	//
	AT91_REG	 PIOC_PER; 	// PIO Enable Register
	AT91_REG	 PIOC_PDR; 	// PIO Disable Register
	AT91_REG	 PIOC_PSR; 	// PIO Status Register
	AT91_REG	 Reserved20[1]; 	//
	AT91_REG	 PIOC_OER; 	// Output Enable Register
	AT91_REG	 PIOC_ODR; 	// Output Disable Registerr
	AT91_REG	 PIOC_OSR; 	// Output Status Register
	AT91_REG	 Reserved21[1]; 	//
	AT91_REG	 PIOC_IFER; 	// Input Filter Enable Register
	AT91_REG	 PIOC_IFDR; 	// Input Filter Disable Register
	AT91_REG	 PIOC_IFSR; 	// Input Filter Status Register
	AT91_REG	 Reserved22[1]; 	//
	AT91_REG	 PIOC_SODR; 	// Set Output Data Register
	AT91_REG	 PIOC_CODR; 	// Clear Output Data Register
	AT91_REG	 PIOC_ODSR; 	// Output Data Status Register
	AT91_REG	 PIOC_PDSR; 	// Pin Data Status Register
	AT91_REG	 PIOC_IER; 	// Interrupt Enable Register
	AT91_REG	 PIOC_IDR; 	// Interrupt Disable Register
	AT91_REG	 PIOC_IMR; 	// Interrupt Mask Register
	AT91_REG	 PIOC_ISR; 	// Interrupt Status Register
	AT91_REG	 PIOC_MDER; 	// Multi-driver Enable Register
	AT91_REG	 PIOC_MDDR; 	// Multi-driver Disable Register
	AT91_REG	 PIOC_MDSR; 	// Multi-driver Status Register
	AT91_REG	 Reserved23[1]; 	//
	AT91_REG	 PIOC_PPUDR; 	// Pull-up Disable Register
	AT91_REG	 PIOC_PPUER; 	// Pull-up Enable Register
	AT91_REG	 PIOC_PPUSR; 	// Pad Pull-up Status Register
	AT91_REG	 Reserved24[1]; 	//
	AT91_REG	 PIOC_ASR; 	// Select A Register
	AT91_REG	 PIOC_BSR; 	// Select B Register
	AT91_REG	 PIOC_ABSR; 	// AB Select Status Register
	AT91_REG	 Reserved25[9]; 	//
	AT91_REG	 PIOC_OWER; 	// Output Write Enable Register
	AT91_REG	 PIOC_OWDR; 	// Output Write Disable Register
	AT91_REG	 PIOC_OWSR; 	// Output Write Status Register
	AT91_REG	 Reserved26[85]; 	//
	AT91_REG	 PIOD_PER; 	// PIO Enable Register
	AT91_REG	 PIOD_PDR; 	// PIO Disable Register
	AT91_REG	 PIOD_PSR; 	// PIO Status Register
	AT91_REG	 Reserved27[1]; 	//
	AT91_REG	 PIOD_OER; 	// Output Enable Register
	AT91_REG	 PIOD_ODR; 	// Output Disable Registerr
	AT91_REG	 PIOD_OSR; 	// Output Status Register
	AT91_REG	 Reserved28[1]; 	//
	AT91_REG	 PIOD_IFER; 	// Input Filter Enable Register
	AT91_REG	 PIOD_IFDR; 	// Input Filter Disable Register
	AT91_REG	 PIOD_IFSR; 	// Input Filter Status Register
	AT91_REG	 Reserved29[1]; 	//
	AT91_REG	 PIOD_SODR; 	// Set Output Data Register
	AT91_REG	 PIOD_CODR; 	// Clear Output Data Register
	AT91_REG	 PIOD_ODSR; 	// Output Data Status Register
	AT91_REG	 PIOD_PDSR; 	// Pin Data Status Register
	AT91_REG	 PIOD_IER; 	// Interrupt Enable Register
	AT91_REG	 PIOD_IDR; 	// Interrupt Disable Register
	AT91_REG	 PIOD_IMR; 	// Interrupt Mask Register
	AT91_REG	 PIOD_ISR; 	// Interrupt Status Register
	AT91_REG	 PIOD_MDER; 	// Multi-driver Enable Register
	AT91_REG	 PIOD_MDDR; 	// Multi-driver Disable Register
	AT91_REG	 PIOD_MDSR; 	// Multi-driver Status Register
	AT91_REG	 Reserved30[1]; 	//
	AT91_REG	 PIOD_PPUDR; 	// Pull-up Disable Register
	AT91_REG	 PIOD_PPUER; 	// Pull-up Enable Register
	AT91_REG	 PIOD_PPUSR; 	// Pad Pull-up Status Register
	AT91_REG	 Reserved31[1]; 	//
	AT91_REG	 PIOD_ASR; 	// Select A Register
	AT91_REG	 PIOD_BSR; 	// Select B Register
	AT91_REG	 PIOD_ABSR; 	// AB Select Status Register
	AT91_REG	 Reserved32[9]; 	//
	AT91_REG	 PIOD_OWER; 	// Output Write Enable Register
	AT91_REG	 PIOD_OWDR; 	// Output Write Disable Register
	AT91_REG	 PIOD_OWSR; 	// Output Write Status Register
	AT91_REG	 Reserved33[85]; 	//
	AT91_REG	 PMC_SCER; 	// System Clock Enable Register
	AT91_REG	 PMC_SCDR; 	// System Clock Disable Register
	AT91_REG	 PMC_SCSR; 	// System Clock Status Register
	AT91_REG	 Reserved34[1]; 	//
	AT91_REG	 PMC_PCER; 	// Peripheral Clock Enable Register
	AT91_REG	 PMC_PCDR; 	// Peripheral Clock Disable Register
	AT91_REG	 PMC_PCSR; 	// Peripheral Clock Status Register
	AT91_REG	 Reserved35[1]; 	//
	AT91_REG	 CKGR_MOR; 	// Main Oscillator Register
	AT91_REG	 CKGR_MCFR; 	// Main Clock  Frequency Register
	AT91_REG	 CKGR_PLLAR; 	// PLL A Register
	AT91_REG	 CKGR_PLLBR; 	// PLL B Register
	AT91_REG	 PMC_MCKR; 	// Master Clock Register
	AT91_REG	 Reserved36[3]; 	//
	AT91_REG	 PMC_PCKR[8]; 	// Programmable Clock Register
	AT91_REG	 PMC_IER; 	// Interrupt Enable Register
	AT91_REG	 PMC_IDR; 	// Interrupt Disable Register
	AT91_REG	 PMC_SR; 	// Status Register
	AT91_REG	 PMC_IMR; 	// Interrupt Mask Register
	AT91_REG	 Reserved37[36]; 	//
	AT91_REG	 ST_CR; 	// Control Register
	AT91_REG	 ST_PIMR; 	// Period Interval Mode Register
	AT91_REG	 ST_WDMR; 	// Watchdog Mode Register
	AT91_REG	 ST_RTMR; 	// Real-time Mode Register
	AT91_REG	 ST_SR; 	// Status Register
	AT91_REG	 ST_IER; 	// Interrupt Enable Register
	AT91_REG	 ST_IDR; 	// Interrupt Disable Register
	AT91_REG	 ST_IMR; 	// Interrupt Mask Register
	AT91_REG	 ST_RTAR; 	// Real-time Alarm Register
	AT91_REG	 ST_CRTR; 	// Current Real-time Register
	AT91_REG	 Reserved38[54]; 	//
	AT91_REG	 RTC_CR; 	// Control Register
	AT91_REG	 RTC_MR; 	// Mode Register
	AT91_REG	 RTC_TIMR; 	// Time Register
	AT91_REG	 RTC_CALR; 	// Calendar Register
	AT91_REG	 RTC_TIMALR; 	// Time Alarm Register
	AT91_REG	 RTC_CALALR; 	// Calendar Alarm Register
	AT91_REG	 RTC_SR; 	// Status Register
	AT91_REG	 RTC_SCCR; 	// Status Clear Command Register
	AT91_REG	 RTC_IER; 	// Interrupt Enable Register
	AT91_REG	 RTC_IDR; 	// Interrupt Disable Register
	AT91_REG	 RTC_IMR; 	// Interrupt Mask Register
	AT91_REG	 RTC_VER; 	// Valid Entry Register
	AT91_REG	 Reserved39[52]; 	//
	AT91_REG	 MC_RCR; 	// MC Remap Control Register
	AT91_REG	 MC_ASR; 	// MC Abort Status Register
	AT91_REG	 MC_AASR; 	// MC Abort Address Status Register
	AT91_REG	 Reserved40[1]; 	//
	AT91_REG	 MC_PUIA[16]; 	// MC Protection Unit Area
	AT91_REG	 MC_PUP; 	// MC Protection Unit Peripherals
	AT91_REG	 MC_PUER; 	// MC Protection Unit Enable Register
	AT91_REG	 Reserved41[2]; 	//
	AT91_REG	 EBI_CSA; 	// Chip Select Assignment Register
	AT91_REG	 EBI_CFGR; 	// Configuration Register
	AT91_REG	 Reserved42[2]; 	//
	AT91_REG	 EBI_SMC2_CSR[8]; 	// SMC2 Chip Select Register
	AT91_REG	 EBI_SDRC_MR; 	// SDRAM Controller Mode Register
	AT91_REG	 EBI_SDRC_TR; 	// SDRAM Controller Refresh Timer Register
	AT91_REG	 EBI_SDRC_CR; 	// SDRAM Controller Configuration Register
	AT91_REG	 EBI_SDRC_SRR; 	// SDRAM Controller Self Refresh Register
	AT91_REG	 EBI_SDRC_LPR; 	// SDRAM Controller Low Power Register
	AT91_REG	 EBI_SDRC_IER; 	// SDRAM Controller Interrupt Enable Register
	AT91_REG	 EBI_SDRC_IDR; 	// SDRAM Controller Interrupt Disable Register
	AT91_REG	 EBI_SDRC_IMR; 	// SDRAM Controller Interrupt Mask Register
	AT91_REG	 EBI_SDRC_ISR; 	// SDRAM Controller Interrupt Mask Register
	AT91_REG	 Reserved43[3]; 	//
	AT91_REG	 EBI_BFC_MR; 	// BFC Mode Register
} AT91S_SYS, *AT91PS_SYS;

#define PCHAR        (char *)

#else

#define PCHAR

#endif


//               PIO DEFINITIONS FOR AT91RM9200

#define AT91C_PIO_PA0        ( 1 <<  0) // Pin Controlled by PA0
#define AT91C_PIO_PA1        ( 1 <<  1) // Pin Controlled by PA1
#define AT91C_PIO_PA10       ( 1 << 10) // Pin Controlled by PA10
#define AT91C_PIO_PA11       ( 1 << 11) // Pin Controlled by PA11
#define AT91C_PIO_PA12       ( 1 << 12) // Pin Controlled by PA12
#define AT91C_PIO_PA13       ( 1 << 13) // Pin Controlled by PA13
#define AT91C_PIO_PA14       ( 1 << 14) // Pin Controlled by PA14
#define AT91C_PIO_PA15       ( 1 << 15) // Pin Controlled by PA15
#define AT91C_PIO_PA16       ( 1 << 16) // Pin Controlled by PA16
#define AT91C_PIO_PA17       ( 1 << 17) // Pin Controlled by PA17
#define AT91C_PIO_PA18       ( 1 << 18) // Pin Controlled by PA18
#define AT91C_PIO_PA19       ( 1 << 19) // Pin Controlled by PA19
#define AT91C_PIO_PA2        ( 1 <<  2) // Pin Controlled by PA2
#define AT91C_PIO_PA20       ( 1 << 20) // Pin Controlled by PA20
#define AT91C_PIO_PA21       ( 1 << 21) // Pin Controlled by PA21
#define AT91C_PIO_PA22       ( 1 << 22) // Pin Controlled by PA22
#define AT91C_PIO_PA23       ( 1 << 23) // Pin Controlled by PA23
#define AT91C_PIO_PA24       ( 1 << 24) // Pin Controlled by PA24
#define AT91C_PIO_PA25       ( 1 << 25) // Pin Controlled by PA25
#define AT91C_PIO_PA26       ( 1 << 26) // Pin Controlled by PA26
#define AT91C_PIO_PA27       ( 1 << 27) // Pin Controlled by PA27
#define AT91C_PIO_PA28       ( 1 << 28) // Pin Controlled by PA28
#define AT91C_PIO_PA29       ( 1 << 29) // Pin Controlled by PA29
#define AT91C_PIO_PA3        ( 1 <<  3) // Pin Controlled by PA3
#define AT91C_PIO_PA30       ( 1 << 30) // Pin Controlled by PA30
#define AT91C_PIO_PA31       ( 1 << 31) // Pin Controlled by PA31
#define AT91C_PIO_PA4        ( 1 <<  4) // Pin Controlled by PA4
#define AT91C_PIO_PA5        ( 1 <<  5) // Pin Controlled by PA5
#define AT91C_PIO_PA6        ( 1 <<  6) // Pin Controlled by PA6
#define AT91C_PIO_PA7        ( 1 <<  7) // Pin Controlled by PA7
#define AT91C_PIO_PA8        ( 1 <<  8) // Pin Controlled by PA8
#define AT91C_PIO_PA9        ( 1 <<  9) // Pin Controlled by PA9
#define AT91C_PIO_PB0        ( 1 <<  0) // Pin Controlled by PB0
#define AT91C_PIO_PB1        ( 1 <<  1) // Pin Controlled by PB1
#define AT91C_PIO_PB10       ( 1 << 10) // Pin Controlled by PB10
#define AT91C_PIO_PB11       ( 1 << 11) // Pin Controlled by PB11
#define AT91C_PIO_PB12       ( 1 << 12) // Pin Controlled by PB12
#define AT91C_PIO_PB13       ( 1 << 13) // Pin Controlled by PB13
#define AT91C_PIO_PB14       ( 1 << 14) // Pin Controlled by PB14
#define AT91C_PIO_PB15       ( 1 << 15) // Pin Controlled by PB15
#define AT91C_PIO_PB16       ( 1 << 16) // Pin Controlled by PB16
#define AT91C_PIO_PB17       ( 1 << 17) // Pin Controlled by PB17
#define AT91C_PIO_PB18       ( 1 << 18) // Pin Controlled by PB18
#define AT91C_PIO_PB19       ( 1 << 19) // Pin Controlled by PB19
#define AT91C_PIO_PB2        ( 1 <<  2) // Pin Controlled by PB2
#define AT91C_PIO_PB20       ( 1 << 20) // Pin Controlled by PB20
#define AT91C_PIO_PB21       ( 1 << 21) // Pin Controlled by PB21
#define AT91C_PIO_PB22       ( 1 << 22) // Pin Controlled by PB22
#define AT91C_PIO_PB23       ( 1 << 23) // Pin Controlled by PB23
#define AT91C_PIO_PB24       ( 1 << 24) // Pin Controlled by PB24
#define AT91C_PIO_PB25       ( 1 << 25) // Pin Controlled by PB25
#define AT91C_PIO_PB26       ( 1 << 26) // Pin Controlled by PB26
#define AT91C_PIO_PB27       ( 1 << 27) // Pin Controlled by PB27
#define AT91C_PIO_PB28       ( 1 << 28) // Pin Controlled by PB28
#define AT91C_PIO_PB29       ( 1 << 29) // Pin Controlled by PB29
#define AT91C_PIO_PB3        ( 1 <<  3) // Pin Controlled by PB3
#define AT91C_PIO_PB4        ( 1 <<  4) // Pin Controlled by PB4
#define AT91C_PIO_PB5        ( 1 <<  5) // Pin Controlled by PB5
#define AT91C_PIO_PB6        ( 1 <<  6) // Pin Controlled by PB6
#define AT91C_PIO_PB7        ( 1 <<  7) // Pin Controlled by PB7
#define AT91C_PIO_PB8        ( 1 <<  8) // Pin Controlled by PB8
#define AT91C_PIO_PB9        ( 1 <<  9) // Pin Controlled by PB9
#define AT91C_PIO_PC0        ( 1 <<  0) // Pin Controlled by PC0
#define AT91C_PIO_PC1        ( 1 <<  1) // Pin Controlled by PC1
#define AT91C_PIO_PC10       ( 1 << 10) // Pin Controlled by PC10
#define AT91C_PIO_PC11       ( 1 << 11) // Pin Controlled by PC11
#define AT91C_PIO_PC12       ( 1 << 12) // Pin Controlled by PC12
#define AT91C_PIO_PC13       ( 1 << 13) // Pin Controlled by PC13
#define AT91C_PIO_PC14       ( 1 << 14) // Pin Controlled by PC14
#define AT91C_PIO_PC15       ( 1 << 15) // Pin Controlled by PC15
#define AT91C_PIO_PC16       ( 1 << 16) // Pin Controlled by PC16
#define AT91C_PIO_PC17       ( 1 << 17) // Pin Controlled by PC17
#define AT91C_PIO_PC18       ( 1 << 18) // Pin Controlled by PC18
#define AT91C_PIO_PC19       ( 1 << 19) // Pin Controlled by PC19
#define AT91C_PIO_PC2        ( 1 <<  2) // Pin Controlled by PC2
#define AT91C_PIO_PC20       ( 1 << 20) // Pin Controlled by PC20
#define AT91C_PIO_PC21       ( 1 << 21) // Pin Controlled by PC21
#define AT91C_PIO_PC22       ( 1 << 22) // Pin Controlled by PC22
#define AT91C_PIO_PC23       ( 1 << 23) // Pin Controlled by PC23
#define AT91C_PIO_PC24       ( 1 << 24) // Pin Controlled by PC24
#define AT91C_PIO_PC25       ( 1 << 25) // Pin Controlled by PC25
#define AT91C_PIO_PC26       ( 1 << 26) // Pin Controlled by PC26
#define AT91C_PIO_PC27       ( 1 << 27) // Pin Controlled by PC27
#define AT91C_PIO_PC28       ( 1 << 28) // Pin Controlled by PC28
#define AT91C_PIO_PC29       ( 1 << 29) // Pin Controlled by PC29
#define AT91C_PIO_PC3        ( 1 <<  3) // Pin Controlled by PC3
#define AT91C_PIO_PC30       ( 1 << 30) // Pin Controlled by PC30
#define AT91C_PIO_PC31       ( 1 << 31) // Pin Controlled by PC31
#define AT91C_PIO_PC4        ( 1 <<  4) // Pin Controlled by PC4
#define AT91C_PIO_PC5        ( 1 <<  5) // Pin Controlled by PC5
#define AT91C_PIO_PC6        ( 1 <<  6) // Pin Controlled by PC6
#define AT91C_PIO_PC7        ( 1 <<  7) // Pin Controlled by PC7
#define AT91C_PIO_PC8        ( 1 <<  8) // Pin Controlled by PC8
#define AT91C_PIO_PC9        ( 1 <<  9) // Pin Controlled by PC9
#define AT91C_PIO_PD0        ( 1 <<  0) // Pin Controlled by PD0
#define AT91C_PIO_PD1        ( 1 <<  1) // Pin Controlled by PD1
#define AT91C_PIO_PD10       ( 1 << 10) // Pin Controlled by PD10
#define AT91C_PIO_PD11       ( 1 << 11) // Pin Controlled by PD11
#define AT91C_PIO_PD12       ( 1 << 12) // Pin Controlled by PD12
#define AT91C_PIO_PD13       ( 1 << 13) // Pin Controlled by PD13
#define AT91C_PIO_PD14       ( 1 << 14) // Pin Controlled by PD14
#define AT91C_PIO_PD15       ( 1 << 15) // Pin Controlled by PD15
#define AT91C_PIO_PD16       ( 1 << 16) // Pin Controlled by PD16
#define AT91C_PIO_PD17       ( 1 << 17) // Pin Controlled by PD17
#define AT91C_PIO_PD18       ( 1 << 18) // Pin Controlled by PD18
#define AT91C_PIO_PD19       ( 1 << 19) // Pin Controlled by PD19
#define AT91C_PIO_PD2        ( 1 <<  2) // Pin Controlled by PD2
#define AT91C_PIO_PD20       ( 1 << 20) // Pin Controlled by PD20
#define AT91C_PIO_PD21       ( 1 << 21) // Pin Controlled by PD21
#define AT91C_PIO_PD22       ( 1 << 22) // Pin Controlled by PD22
#define AT91C_PIO_PD23       ( 1 << 23) // Pin Controlled by PD23
#define AT91C_PIO_PD24       ( 1 << 24) // Pin Controlled by PD24
#define AT91C_PIO_PD25       ( 1 << 25) // Pin Controlled by PD25
#define AT91C_PIO_PD26       ( 1 << 26) // Pin Controlled by PD26
#define AT91C_PIO_PD27       ( 1 << 27) // Pin Controlled by PD27
#define AT91C_PIO_PD3        ( 1 <<  3) // Pin Controlled by PD3
#define AT91C_PIO_PD4        ( 1 <<  4) // Pin Controlled by PD4
#define AT91C_PIO_PD5        ( 1 <<  5) // Pin Controlled by PD5
#define AT91C_PIO_PD6        ( 1 <<  6) // Pin Controlled by PD6
#define AT91C_PIO_PD7        ( 1 <<  7) // Pin Controlled by PD7
#define AT91C_PIO_PD8        ( 1 <<  8) // Pin Controlled by PD8
#define AT91C_PIO_PD9        ( 1 <<  9) // Pin Controlled by PD9

//              SOFTWARE API DEFINITION  FOR System Timer Interface

// -------- ST_CR : (ST Offset: 0x0) System Timer Control Register --------
#define AT91C_ST_WDRST        ( 0x1 <<  0) // (ST) Watchdog Timer Restart
// -------- ST_PIMR : (ST Offset: 0x4) System Timer Period Interval Mode Register --------
#define AT91C_ST_PIV          ( 0xFFFF <<  0) // (ST) Watchdog Timer Restart
// -------- ST_WDMR : (ST Offset: 0x8) System Timer Watchdog Mode Register --------
#define AT91C_ST_WDV          ( 0xFFFF <<  0) // (ST) Watchdog Timer Restart
#define AT91C_ST_RSTEN        ( 0x1 << 16) // (ST) Reset Enable
#define AT91C_ST_EXTEN        ( 0x1 << 17) // (ST) External Signal Assertion Enable
// -------- ST_RTMR : (ST Offset: 0xc) System Timer Real-time Mode Register --------
#define AT91C_ST_RTPRES       ( 0xFFFF <<  0) // (ST) Real-time Timer Prescaler Value
// -------- ST_SR : (ST Offset: 0x10) System Timer Status Register --------
#define AT91C_ST_PITS         ( 0x1 <<  0) // (ST) Period Interval Timer Interrupt
#define AT91C_ST_WDOVF        ( 0x1 <<  1) // (ST) Watchdog Overflow
#define AT91C_ST_RTTINC       ( 0x1 <<  2) // (ST) Real-time Timer Increment
#define AT91C_ST_ALMS         ( 0x1 <<  3) // (ST) Alarm Status
// -------- ST_IER : (ST Offset: 0x14) System Timer Interrupt Enable Register --------
// -------- ST_IDR : (ST Offset: 0x18) System Timer Interrupt Disable Register --------
// -------- ST_IMR : (ST Offset: 0x1c) System Timer Interrupt Mask Register --------
// -------- ST_RTAR : (ST Offset: 0x20) System Timer Real-time Alarm Register --------
#define AT91C_ST_ALMV         ( 0xFFFFF <<  0) // (ST) Alarm Value Value
// -------- ST_CRTR : (ST Offset: 0x24) System Timer Current Real-time Register --------
#define AT91C_ST_CRTV         ( 0xFFFFF <<  0) // (ST) Current Real-time Value


//              SOFTWARE API DEFINITION  FOR Advanced Interrupt Controller

// -------- AIC_SMR : (AIC Offset: 0x0) Control Register --------
#define AT91C_AIC_PRIOR       ( 0x7 <<  0) // (AIC) Priority Level
#define 	AT91C_AIC_PRIOR_LOWEST               ( 0x0) // (AIC) Lowest priority level
#define 	AT91C_AIC_PRIOR_HIGHEST              ( 0x7) // (AIC) Highest priority level
#define AT91C_AIC_SRCTYPE     ( 0x3 <<  5) // (AIC) Interrupt Source Type
#define 	AT91C_AIC_SRCTYPE_INT_LEVEL_SENSITIVE  ( 0x0 <<  5) // (AIC) Internal Sources Code Label Level Sensitive
#define 	AT91C_AIC_SRCTYPE_INT_EDGE_TRIGGERED   ( 0x1 <<  5) // (AIC) Internal Sources Code Label Edge triggered
#define 	AT91C_AIC_SRCTYPE_EXT_HIGH_LEVEL       ( 0x2 <<  5) // (AIC) External Sources Code Label High-level Sensitive
#define 	AT91C_AIC_SRCTYPE_EXT_POSITIVE_EDGE    ( 0x3 <<  5) // (AIC) External Sources Code Label Positive Edge triggered

#define AT91C_AIC_NFIQ        ( 0x1 <<  0) // (AIC) NFIQ Status
#define AT91C_AIC_NIRQ        ( 0x1 <<  1) // (AIC) NIRQ Status
// -------- AIC_DCR : (AIC Offset: 0x138) AIC Debug Control Register (Protect) --------
#define AT91C_AIC_DCR_PROT    ( 0x1 <<  0) // (AIC) Protection Mode
#define AT91C_AIC_DCR_GMSK    ( 0x1 <<  1) // (AIC) General Mask


//              SOFTWARE API DEFINITION  FOR Debug Unit

// -------- DBGU_CR : (DBGU Offset: 0x0) Debug Unit Control Register --------
#define AT91C_US_RSTRX        ( 0x1 <<  2) // (DBGU) Reset Receiver
#define AT91C_US_RSTTX        ( 0x1 <<  3) // (DBGU) Reset Transmitter
#define AT91C_US_RXEN         ( 0x1 <<  4) // (DBGU) Receiver Enable
#define AT91C_US_RXDIS        ( 0x1 <<  5) // (DBGU) Receiver Disable
#define AT91C_US_TXEN         ( 0x1 <<  6) // (DBGU) Transmitter Enable
#define AT91C_US_TXDIS        ( 0x1 <<  7) // (DBGU) Transmitter Disable
// -------- DBGU_MR : (DBGU Offset: 0x4) Debug Unit Mode Register --------
#define AT91C_US_PAR          ( 0x7 <<  9) // (DBGU) Parity type
#define 	AT91C_US_PAR_EVEN                 ( 0x0 <<  9) // (DBGU) Even Parity
#define 	AT91C_US_PAR_ODD                  ( 0x1 <<  9) // (DBGU) Odd Parity
#define 	AT91C_US_PAR_SPACE                ( 0x2 <<  9) // (DBGU) Parity forced to 0 (Space)
#define 	AT91C_US_PAR_MARK                 ( 0x3 <<  9) // (DBGU) Parity forced to 1 (Mark)
#define 	AT91C_US_PAR_NONE                 ( 0x4 <<  9) // (DBGU) No Parity
#define 	AT91C_US_PAR_MULTI_DROP           ( 0x6 <<  9) // (DBGU) Multi-drop mode
#define AT91C_US_CHMODE       ( 0x3 << 14) // (DBGU) Channel Mode
#define 	AT91C_US_CHMODE_NORMAL               ( 0x0 << 14) // (DBGU) Normal Mode: The USART channel operates as an RX/TX USART.
#define 	AT91C_US_CHMODE_AUTO                 ( 0x1 << 14) // (DBGU) Automatic Echo: Receiver Data Input is connected to the TXD pin.
#define 	AT91C_US_CHMODE_LOCAL                ( 0x2 << 14) // (DBGU) Local Loopback: Transmitter Output Signal is connected to Receiver Input Signal.
#define 	AT91C_US_CHMODE_REMOTE               ( 0x3 << 14) // (DBGU) Remote Loopback: RXD pin is internally connected to TXD pin.
// -------- DBGU_IER : (DBGU Offset: 0x8) Debug Unit Interrupt Enable Register --------
#define AT91C_US_RXRDY        ( 0x1 <<  0) // (DBGU) RXRDY Interrupt
#define AT91C_US_TXRDY        ( 0x1 <<  1) // (DBGU) TXRDY Interrupt
#define AT91C_US_ENDRX        ( 0x1 <<  3) // (DBGU) End of Receive Transfer Interrupt
#define AT91C_US_ENDTX        ( 0x1 <<  4) // (DBGU) End of Transmit Interrupt
#define AT91C_US_OVRE         ( 0x1 <<  5) // (DBGU) Overrun Interrupt
#define AT91C_US_FRAME        ( 0x1 <<  6) // (DBGU) Framing Error Interrupt
#define AT91C_US_PARE         ( 0x1 <<  7) // (DBGU) Parity Error Interrupt
#define AT91C_US_TXEMPTY      ( 0x1 <<  9) // (DBGU) TXEMPTY Interrupt
#define AT91C_US_TXBUFE       ( 0x1 << 11) // (DBGU) TXBUFE Interrupt
#define AT91C_US_RXBUFF       ( 0x1 << 12) // (DBGU) RXBUFF Interrupt
#define AT91C_US_COMM_TX      ( 0x1 << 30) // (DBGU) COMM_TX Interrupt
#define AT91C_US_COMM_RX      ( 0x1 << 31) // (DBGU) COMM_RX Interrupt

// -------- DBGU_FNTR : (DBGU Offset: 0x48) Debug Unit FORCE_NTRST Register --------
#define AT91C_US_FORCE_NTRST  ( 0x1 <<  0) // (DBGU) Force NTRST in JTAG

//               BASE ADDRESS DEFINITIONS FOR AT91RM9200
#define AT91C_BASE_SYS       (0xFFFFF000) // (SYS) Base Address

//               MEMORY MAPPING DEFINITIONS FOR AT91RM9200
#ifndef __ASSEMBLY__
#define AT91C_ISRAM	 (PCHAR 	0x00200000) // Internal SRAM base address
#define AT91C_ISRAM_SIZE	 ( 0x00004000) // Internal SRAM size in byte (16 Kbyte)
#define AT91C_IROM 	 (PCHAR 	0x00100000) // Internal ROM base address
#define AT91C_IROM_SIZE	 ( 0x00020000) // Internal ROM size in byte (128 Kbyte)
#endif

// *****************************************************************************
//               BASE ADDRESS DEFINITIONS FOR AT91RM9200
// *****************************************************************************

#define AT91C_BASE_MC             (0xFFFFFF00) // (MC) Base Address
#define AT91C_BASE_RTC            (0xFFFFFE00) // (RTC) Base Address
#define AT91C_BASE_ST             (0xFFFFFD00) // (ST) Base Address
#define AT91C_BASE_PMC            (0xFFFFFC00) // (PMC) Base Address
#define AT91C_BASE_CKGR           (0xFFFFFC20) // (CKGR) Base Address
#define AT91C_BASE_PIOD           (0xFFFFFA00) // (PIOD) Base Address
#define AT91C_BASE_PIOC           (0xFFFFF800) // (PIOC) Base Address
#define AT91C_BASE_PIOB           (0xFFFFF600) // (PIOB) Base Address
#define AT91C_BASE_PIOA           (0xFFFFF400) // (PIOA) Base Address
#define AT91C_BASE_DBGU           (0xFFFFF200) // (DBGU) Base Address
#define AT91C_BASE_AIC            (0xFFFFF000) // (AIC) Base Address
#define AT91C_BASE_SPI            (0xFFFE0000) // (SPI) Base Address
#define AT91C_BASE_SSC2           (0xFFFD8000) // (SSC2) Base Address
#define AT91C_BASE_SSC1           (0xFFFD4000) // (SSC1) Base Address
#define AT91C_BASE_SSC0           (0xFFFD0000) // (SSC0) Base Address
#define AT91C_BASE_US3            (0xFFFCC000) // (US3) Base Address
#define AT91C_BASE_US2            (0xFFFC8000) // (US2) Base Address
#define AT91C_BASE_US1            (0xFFFC4000) // (US1) Base Address
#define AT91C_BASE_US0            (0xFFFC0000) // (US0) Base Address
#define AT91C_BASE_EMAC           (0xFFFBC000) // (EMAC) Base Address
#define AT91C_BASE_TWI            (0xFFFB8000) // (TWI) Base Address
#define AT91C_BASE_MCI            (0xFFFB4000) // (MCI) Base Address
#define AT91C_BASE_UDP            (0xFFFB0000) // (UDP) Base Address
#define AT91C_BASE_TC5            (0xFFFA4080) // (TC5) Base Address
#define AT91C_BASE_TC4            (0xFFFA4040) // (TC4) Base Address
#define AT91C_BASE_TC3            (0xFFFA4000) // (TC3) Base Address
#define AT91C_BASE_TCB1           (0xFFFA4080) // (TCB1) Base Address
#define AT91C_BASE_TC2            (0xFFFA0080) // (TC2) Base Address
#define AT91C_BASE_TC1            (0xFFFA0040) // (TC1) Base Address
#define AT91C_BASE_TC0            (0xFFFA0000) // (TC0) Base Address
#define AT91C_BASE_TCB0           (0xFFFA0000) // (TCB0) Base Address

#define AT91C_BASE_UHP            (0x00300000) // (UHP) Base Address

// *****************************************************************************
//               PERIPHERAL ID DEFINITIONS FOR AT91RM9200
// *****************************************************************************
#define AT91C_ID_FIQ              ( 0) // Advanced Interrupt Controller (FIQ)
#define AT91C_ID_SYS              ( 1) // System Peripheral
#define AT91C_ID_PIOA             ( 2) // Parallel IO Controller A
#define AT91C_ID_PIOB             ( 3) // Parallel IO Controller B
#define AT91C_ID_PIOC             ( 4) // Parallel IO Controller C
#define AT91C_ID_PIOD             ( 5) // Parallel IO Controller D
#define AT91C_ID_US0              ( 6) // USART 0
#define AT91C_ID_US1              ( 7) // USART 1
#define AT91C_ID_US2              ( 8) // USART 2
#define AT91C_ID_US3              ( 9) // USART 3
#define AT91C_ID_MCI              (10) // Multimedia Card Interface
#define AT91C_ID_UDP              (11) // USB Device Port
#define AT91C_ID_TWI              (12) // Two-Wire Interface
#define AT91C_ID_SPI              (13) // Serial Peripheral Interface
#define AT91C_ID_SSC0             (14) // Serial Synchronous Controller 0
#define AT91C_ID_SSC1             (15) // Serial Synchronous Controller 1
#define AT91C_ID_SSC2             (16) // Serial Synchronous Controller 2
#define AT91C_ID_TC0              (17) // Timer Counter 0
#define AT91C_ID_TC1              (18) // Timer Counter 1
#define AT91C_ID_TC2              (19) // Timer Counter 2
#define AT91C_ID_TC3              (20) // Timer Counter 3
#define AT91C_ID_TC4              (21) // Timer Counter 4
#define AT91C_ID_TC5              (22) // Timer Counter 5
#define AT91C_ID_UHP              (23) // USB Host port
#define AT91C_ID_EMAC             (24) // Ethernet MAC
#define AT91C_ID_IRQ0             (25) // Advanced Interrupt Controller (IRQ0)
#define AT91C_ID_IRQ1             (26) // Advanced Interrupt Controller (IRQ1)
#define AT91C_ID_IRQ2             (27) // Advanced Interrupt Controller (IRQ2)
#define AT91C_ID_IRQ3             (28) // Advanced Interrupt Controller (IRQ3)
#define AT91C_ID_IRQ4             (29) // Advanced Interrupt Controller (IRQ4)
#define AT91C_ID_IRQ5             (30) // Advanced Interrupt Controller (IRQ5)
#define AT91C_ID_IRQ6             (31) // Advanced Interrupt Controller (IRQ6)

#endif

#ifdef __ASSEMBLY__
// *****************************************************************************
//              SOFTWARE API DEFINITION  FOR Debug Unit
// *****************************************************************************
// *** Register offset in AT91S_DBGU structure ***
#define DBGU_CR         ( 0) // Control Register
#define DBGU_MR         ( 4) // Mode Register
#define DBGU_IER        ( 8) // Interrupt Enable Register
#define DBGU_IDR        (12) // Interrupt Disable Register
#define DBGU_IMR        (16) // Interrupt Mask Register
#define DBGU_CSR        (20) // Channel Status Register
#define DBGU_RHR        (24) // Receiver Holding Register
#define DBGU_THR        (28) // Transmitter Holding Register
#define DBGU_BRGR       (32) // Baud Rate Generator Register
#define DBGU_C1R        (64) // Chip ID1 Register
#define DBGU_C2R        (68) // Chip ID2 Register
#define DBGU_FNTR       (72) // Force NTRST Register
#define DBGU_RPR        (256) // Receive Pointer Register
#define DBGU_RCR        (260) // Receive Counter Register
#define DBGU_TPR        (264) // Transmit Pointer Register
#define DBGU_TCR        (268) // Transmit Counter Register
#define DBGU_RNPR       (272) // Receive Next Pointer Register
#define DBGU_RNCR       (276) // Receive Next Counter Register
#define DBGU_TNPR       (280) // Transmit Next Pointer Register
#define DBGU_TNCR       (284) // Transmit Next Counter Register
#define DBGU_PTCR       (288) // PDC Transfer Control Register
#define DBGU_PTSR       (292) // PDC Transfer Status Register

// *****************************************************************************
//              SOFTWARE API DEFINITION  FOR Advanced Interrupt Controller
// *****************************************************************************
// *** Register offset in AT91S_AIC structure ***
#define AIC_SMR         ( 0) // Source Mode Register
#define AIC_SVR         (128) // Source Vector Register
#define AIC_IVR         (256) // IRQ Vector Register
#define AIC_FVR         (260) // FIQ Vector Register
#define AIC_ISR         (264) // Interrupt Status Register
#define AIC_IPR         (268) // Interrupt Pending Register
#define AIC_IMR         (272) // Interrupt Mask Register
#define AIC_CISR        (276) // Core Interrupt Status Register
#define AIC_IECR        (288) // Interrupt Enable Command Register
#define AIC_IDCR        (292) // Interrupt Disable Command Register
#define AIC_ICCR        (296) // Interrupt Clear Command Register
#define AIC_ISCR        (300) // Interrupt Set Command Register
#define AIC_EOICR       (304) // End of Interrupt Command Register
#define AIC_SPU         (308) // Spurious Vector Register
#define AIC_DCR         (312) // Debug Control Register (Protect)
#define AIC_FFER        (320) // Fast Forcing Enable Register
#define AIC_FFDR        (324) // Fast Forcing Disable Register
#define AIC_FFSR        (328) // Fast Forcing Status Register

#endif
