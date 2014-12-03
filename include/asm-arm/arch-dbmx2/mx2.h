// This Is a generated file. Source is an Excel sheet.  DO NOT EDIT		
//############################################################################		
//#                                                                          #		
//#                    Tahiti specific tht_memory_map.equ                    #		
//#                     Motorola Confidential Proprietary                    #		
//# Purpose:                                                                 #		
//#   This file is used by the ARM ADS assembler to decode the memory map    #		
//#                                                                          #		
//# Description                                                              #		
//#   This file list out each individual memory location for decoding        #		
//#                                                                          #		
//# Initial version writen By                                                #		
//#   JM Kam (jmkam@motorola.com) SPS/WBSG SNP                               #		
//#                                                                          #		
//# Date                                                                     #		
//#   13 Jan 2003                                                            #		
//#                                                                          #		
//# Status                                                                   #		
//#   clock rest module: Added sys control registsers temporarily            #		
//#   Chapters Missing: HAB          EMMA DEC  EMMA ENC                      #		
//#                                                                          #		
//# Revisions                                                                #		
//#    Date       By           Description of changes                        #		
//#  20 Nov 2002  JM Kam       Initial release for Tahiti IC Spec 0.3        #		
//#  21 Dec 2002  JM Kam       Changed over to Tahiti-Lite IC Spec 0.1       #		
//#                            - filled in SCM SMN ROMPATCH  SLCDC  AUDMUX   #		
//#                            - renamed EIM to WEIM                         #		
//#                            - updated DMA  CCM  LCDC  EMMA  RTC  WDOG     #		
//#                                      PWM  UART  FIRI  GPIO               #		
//#                            - temp allocated AIPI1 en 22 to AUDMUX        #		
//#  17 Jan 2003  JM Kam       Changed over to Tahiti-Lite IC Spec 0.2       #		
//#                            - Additions for CRM  NFC  BMI  EMMA  SSI      #		
//#                                     PCMCIA GPIO USBOTG BMI SAHARA        #		
//#                            - Split system control regs away from CRM     #		
//#                              - left in CRM reg space and tagged SYS      #		
//#                            Corrections for (from v0.1)                   #		
//#                            -CRM  FIRI  SLCDC  LCDC                       #		
//#                            Confirm allocated AIPI1 en 22 to AUDMUX       #		
//#  14 Feb 2003  JM Kam       Emma memory map updated for enc and dec       #		
//#                            Added new registers for DMA module            #		
//#  19 Feb 2003  JM Kam       Added one register for CSI  CSIRXCNT          #		
//#  20 Feb 2003  JM Kam       Added LCDC_LIER and LCDC_LGWDCR               #		
//#  18 Mar 2003  JM Kam       Updated USBOTG 0x848 0x84C                    #		
//#  19 Mar 2003  JM Kam       Changed VRAM to reflect 6k memory             #		
//#  01 Apr 2003  JM Kam       Changed SAHARA DESA to be same as AESA        #		
//#  28 Apr 2003  JM Kam       Updated USBOTG I2C registers                  #		
//#                                                                          #		
//############################################################################		
//Memory Map		
//----------		
//$0000_0000 - $0000_3FFF BROM     ()		
//$0000_4000 - $0040_3FFF reserved		
//$0040_4000 - $007F_FFFF BROM     ()		
//$0080_0000 - $0FFF_FFFF reserved		
		
//$1000_0000 - $1000_0FFF AIPI1    ()		
//$1000_1000 - $1000_1FFF DMA      ()		
//$1000_2000 - $1000_2FFF WDOG     ()		
//$1000_3000 - $1000_3FFF GPT1     ()		
//$1000_4000 - $1000_4FFF GPT2     ()		
//$1000_5000 - $1000_5FFF GPT3     ()		
//$1000_6000 - $1000_6FFF PWM      ()		
//$1000_7000 - $1000_7FFF RTC      ()		
//$1000_8000 - $1000_8FFF KPP      ()		
//$1000_9000 - $1000_9FFF OWIRE    ()		
//$1000_A000 - $1000_AFFF UART1    ()		
//$1000_B000 - $1000_BFFF UART2    ()		
//$1000_C000 - $1000_CFFF UART3    ()		
//$1000_D000 - $1000_DFFF UART4    ()		
//$1000_E000 - $1000_EFFF CSPI1    ()		
//$1000_F000 - $1000_FFFF CSPI2    ()		
		
//$1001_0000 - $1001_0FFF SSI1     ()		
//$1001_1000 - $1001_1FFF SSI2     ()		
//$1001_2000 - $1001_2FFF I2C      ()		
//$1001_3000 - $1001_3FFF SDHC1    ()		
//$1001_4000 - $1001_4FFF SDHC2    ()		
//$1001_5000 - $1001_5FFF GPIO     ()		
//$1001_6000 - $1001_6FFF AUDMUX   ()		
//$1001_7000 - $1001_7FFF reserved		
//$1001_8000 - $1001_8FFF reserved		
//$1001_9000 - $1001_9FFF reserved		
//$1001_A000 - $1001_AFFF reserved		
//$1001_B000 - $1001_BFFF reserved		
//$1001_C000 - $1001_CFFF reserved		
//$1001_D000 - $1001_DFFF reserved		
//$1001_E000 - $1001_EFFF reserved		
//$1001_F000 - $1001_FFFF reserved		
		
//$1002_0000 - $1002_0FFF AIPI2    ()		
//$1002_1000 - $1002_1FFF LCDC     ()		
//$1002_2000 - $1002_2FFF SLCDC    ()		
		
//$1002_4000 - $1002_4FFF USBOTG   ()		
//$1002_5000 - $1002_5FFF USBOTG   ()		
//$1002_6000 - $1002_6FFF EMMA     ()		
//$1002_7000 - $1002_7FFF CRM and SYS ()		
//$1002_8000 - $1002_8FFF FIRI     ()		
//$1002_9000 - $1002_9FFF reserved		
//$1002_A000 - $1002_AFFF reserved		
//$1002_B000 - $1002_BFFF reserved		
//$1002_C000 - $1002_CFFF reserved		
//$1002_D000 - $1002_DFFF reserved		
//$1002_E000 - $1002_EFFF reserved		
		
//$1003_0000 - $1003_0FFF reserved		
//$1003_1000 - $1003_1FFF reserved		
//$1003_2000 - $1003_2FFF reserved		
//$1003_3000 - $1003_3FFF reserved		
		
//$1003_5000 - $1003_5FFF reserved		
//$1003_6000 - $1003_6FFF reserved		
//$1003_7000 - $1003_7FFF reserved		
//$1003_8000 - $1003_8FFF reserved		
//$1003_9000 - $1003_9FFF reserved		
//$1003_A000 - $1003_AFFF reserved		
//$1003_B000 - $1003_BFFF reserved		
//$1003_C000 - $1003_CFFF reserved		
//$1003_D000 - $1003_DFFF reserved		
//$1003_E000 - $1003_EFFF JAM      ()		
//$1003_F000 - $1003_FFFF MAX      ()		
		
//$1004_0000 - $1004_0FFF AITC     ()		
//$1004_1000 - $1004_1FFF ROMPATCH ()		
//$1004_2000 - $1004_2FFF SMN      ()		
//$1004_3000 - $1004_3FFF SCM      ()		
		
//$1004_4000 - $7FFF_FFFF reserved		
		
//$8000_0000 - $8000_0FFF CSI      ()		
//$8000_1000 - $9FFF_FFFF reserved		
		
//$A000_0000 - $A000_0FFF BMI      ()		
//$A000_1000 - $BFFF_FFFF reserved		
		
//$C000_0000 - $C3FF_FFFF External Memory (CSD0)		
//$C400_0000 - $C7FF_FFFF External Memory (CSD1)		
//$C800_0000 - $CBFF_FFFF External Memory (CS0)		
//$CC00_0000 - $CFFF_FFFF External Memory (CS1)		
//$D000_0000 - $D0FF_FFFF External Memory (CS2)		
//$D100_0000 - $D1FF_FFFF External Memory (CS3)		
//$D200_0000 - $D2FF_FFFF External Memory (CS4)		
//$D300_0000 - $D3FF_FFFF External Memory (CS5)		
		
//$D400_0000 - $D7FF_FFFF External Memory (PCMCIA/CF)		
//$D800_0000 - $DEFF_FFFF reserved		
//$DF00_0000 - $DF00_0FFF SDRAMC		
//$DF00_1000 - $DF00_1FFF WEIM		
//$DF00_2000 - $DF00_2FFF PCMCIA		
//$DF00_3000 - $DF00_3FFF NFC		
//$DF00_4000 - $DFFF_FFFF reserved		
		
//$E000_0000 - $FFFF_E7FF reserved		
//$FFFF_E800 - $FFFF_FFFF VRAM		
		
//#########################################		
//# BOOT ROM                              #		
//# $0000_0000 to $0000_3FFF              #		
//# $0040_4000 to $007F_FFFF              #		
//#########################################
#ifndef __ASM_ARCH_MX2_H
#define __ASM_ARCH_MX2_H

#include <asm/arch/hardware.h>

//Frank Li Add it
#ifdef _STANDALONE_
#define MX2_IO_ADDRESS
#define NFC_MX2_IO_ADDRESS
#define MX2ADS_EMI_IOBASE 0xDF000000
#else
#define MX2_IO_ADDRESS IO_ADDRESS
#define NFC_MX2_IO_ADDRESS NFC_IO_ADDRESS
#endif 

#define IN
#define OUT
#define INOUT
		
#define BOOTROM1_ADDR_BOT	0x00000000	//  boot rom section 1 bottom address
#define BOOTROM1_PHY_SIZE	0x00004000	//  boot rom section 1 physical size
#define BOOTROM1_ASS_SIZE	0x00004000	//  boot rom section 1 assigned size
		
#define BOOTROM2_ADDR_BOT	0x00404000	//  boot rom section 2 bottom address
#define BOOTROM2_PHY_SIZE	0x003F4000	//  boot rom section 2 physical size
#define BOOTROM2_ASS_SIZE	0x003F4000	//  boot rom section 2 assigned size
		
		
		
		
//#########################################		
//# AIPI1                                 #		
//# $1000_0000 to $1000_0FFF              #		
//#########################################		
#define AIPI1_BASE_ADDR	0x10000000	
#define _reg_AIPI1_PSR0		(*((volatile unsigned long *)(MX2_IO_ADDRESS(AIPI1_BASE_ADDR+0x00))))	//  32bit Peripheral Size Reg 0
#define _reg_AIPI1_PSR1		(*((volatile unsigned long *)(MX2_IO_ADDRESS(AIPI1_BASE_ADDR+0x04))))	//  32bit Peripheral Size Reg 1
#define _reg_AIPI1_PAR		(*((volatile unsigned long *)(MX2_IO_ADDRESS(AIPI1_BASE_ADDR+0x08))))	//  32bit Peripheral Access Reg
		
//#########################################		
//# DMA                                   #		
//# $1000_1000 to $1000_1FFF              #		
//#########################################		
#define DMA_BASE_ADDR	0x10001000	
#define DMA_SYS_BASE	 DMA_BASE_ADDR	//  base location for system
#define DMA_M2D_BASE	(DMA_BASE_ADDR+0x040)	//  base location for 2D memory reg

#define DMA_CH_BASE(x)	(DMA_BASE_ADDR+0x080+0x040*(x))	//jimmy


		#if 0
		#define DMA_CH0_BASE	(DMA_BASE_ADDR+0x080)	//  base location for channel 0
		#define DMA_CH1_BASE	(DMA_BASE_ADDR+0x0C0)	//  base location for channel 1
		#define DMA_CH2_BASE	(DMA_BASE_ADDR+0x100)	//  base location for channel 2
		#define DMA_CH3_BASE	(DMA_BASE_ADDR+0x140)	//  base location for channel 3
		#define DMA_CH4_BASE	(DMA_BASE_ADDR+0x180)	//  base location for channel 4
		#define DMA_CH5_BASE	(DMA_BASE_ADDR+0x1C0)	//  base location for channel 5
		#define DMA_CH6_BASE	(DMA_BASE_ADDR+0x200)	//  base location for channel 6
		#define DMA_CH7_BASE	(DMA_BASE_ADDR+0x240)	//  base location for channel 7
		#define DMA_CH8_BASE	(DMA_BASE_ADDR+0x280)	//  base location for channel 8
		#define DMA_CH9_BASE	(DMA_BASE_ADDR+0x2C0)	//  base location for channel 9
		#define DMA_CH10_BASE	(DMA_BASE_ADDR+0x300)	//  base location for channel 10
		#define DMA_CH11_BASE	(DMA_BASE_ADDR+0x340)	//  base location for channel 11
		#define DMA_CH12_BASE	(DMA_BASE_ADDR+0x380)	//  base location for channel 12
		#define DMA_CH13_BASE	(DMA_BASE_ADDR+0x3C0)	//  base location for channel 13
		#define DMA_CH14_BASE	(DMA_BASE_ADDR+0x400)	//  base location for channel 14
		#define DMA_CH15_BASE	(DMA_BASE_ADDR+0x440)	//  base location for channel 15
		#endif

#define DMA_TEST_BASE	(DMA_BASE_ADDR+0x480)	//  base location for test registers

		
#define _reg_DMA_DCR		(*((volatile unsigned long *)(MX2_IO_ADDRESS(DMA_SYS_BASE))))	//  32bit dma control reg
#define _reg_DMA_DISR		(*((volatile unsigned long *)(MX2_IO_ADDRESS(DMA_SYS_BASE+0x004))))	//  32bit dma interrupt status reg
#define _reg_DMA_DIMR		(*((volatile unsigned long *)(MX2_IO_ADDRESS(DMA_SYS_BASE+0x008))))	//  32bit dma interrupt mask reg
#define _reg_DMA_DBTOSR		(*((volatile unsigned long *)(MX2_IO_ADDRESS(DMA_SYS_BASE+0x00C))))	//  32bit dma burst timeout stat reg
#define _reg_DMA_DRTOSR		(*((volatile unsigned long *)(MX2_IO_ADDRESS(DMA_SYS_BASE+0x010))))	//  32bit dma req timeout status reg
#define _reg_DMA_DSESR		(*((volatile unsigned long *)(MX2_IO_ADDRESS(DMA_SYS_BASE+0x014))))	//  32bit dma transfer err status reg
#define _reg_DMA_DBOSR		(*((volatile unsigned long *)(MX2_IO_ADDRESS(DMA_SYS_BASE+0x018))))	//  32bit dma buffer overflow stat reg
#define _reg_DMA_DBTOCR		(*((volatile unsigned long *)(MX2_IO_ADDRESS(DMA_SYS_BASE+0x01C))))	//  32bit dma burst timeout ctrl reg
		
#define _reg_DMA_WSRA		(*((volatile unsigned long *)(MX2_IO_ADDRESS(DMA_M2D_BASE+0x000))))	//  32bit dma W-size A reg
#define _reg_DMA_XSRA		(*((volatile unsigned long *)(MX2_IO_ADDRESS(DMA_M2D_BASE+0x004))))	//  32bit dma X-size A reg
#define _reg_DMA_YSRA		(*((volatile unsigned long *)(MX2_IO_ADDRESS(DMA_M2D_BASE+0x008))))	//  32bit dma Y-size A reg
#define _reg_DMA_WSRB		(*((volatile unsigned long *)(MX2_IO_ADDRESS(DMA_M2D_BASE+0x00C))))	//  32bit dma W-size B reg
#define _reg_DMA_XSRB		(*((volatile unsigned long *)(MX2_IO_ADDRESS(DMA_M2D_BASE+0x010))))	//  32bit dma X-size B reg
#define _reg_DMA_YSRB		(*((volatile unsigned long *)(MX2_IO_ADDRESS(DMA_M2D_BASE+0x014))))	//  32bit dma Y-size B reg

#define _reg_DMA_SAR(x)	 	(*((volatile unsigned long *)(MX2_IO_ADDRESS(DMA_CH_BASE(x)))))		//  jimmy
#define _reg_DMA_DAR(x)		(*((volatile unsigned long *)(MX2_IO_ADDRESS(DMA_CH_BASE(x)+0x004))))
#define _reg_DMA_CNTR(x)	(*((volatile unsigned long *)(MX2_IO_ADDRESS(DMA_CH_BASE(x)+0x008))))
#define _reg_DMA_CCR(x)		(*((volatile unsigned long *)(MX2_IO_ADDRESS(DMA_CH_BASE(x)+0x00C))))	//  32bit dma ch0 control reg
#define _reg_DMA_RSSR(x)	(*((volatile unsigned long *)(MX2_IO_ADDRESS(DMA_CH_BASE(x)+0x010))))	//  32bit dma ch0 req source sel reg
#define _reg_DMA_BLR(x)		(*((volatile unsigned long *)(MX2_IO_ADDRESS(DMA_CH_BASE(x)+0x014))))	//  32bit dma ch0 burst lenght reg
#define _reg_DMA_RTOR(x)	(*((volatile unsigned long *)(MX2_IO_ADDRESS(DMA_CH_BASE(x)+0x018))))	//  32bit dma ch0 req time out reg
#define _reg_DMA_BUCR(x)	(*((volatile unsigned long *)(MX2_IO_ADDRESS(DMA_CH_BASE(x)+0x018))))	//  32bit dma ch0 bus utilization reg
#define _reg_DMA_CCNR(x)	(*((volatile unsigned long *)(MX2_IO_ADDRESS(DMA_CH_BASE(x)+0x01C))))	//  32bit dma ch0

		#if 0		
		#define DMA_SAR0	(DMA_CH0_BASE+0x000)	//  32bit dma ch0 source addr reg
		#define DMA_DAR0	(DMA_CH0_BASE+0x004)	//  32bit dma ch0 dest addr reg
		#define DMA_CNTR0	(DMA_CH0_BASE+0x008)	//  32bit dma ch0 count reg
		#define DMA_CCR0	(DMA_CH0_BASE+0x00C)	//  32bit dma ch0 control reg
		#define DMA_RSSR0	(DMA_CH0_BASE+0x010)	//  32bit dma ch0 req source sel reg
		#define DMA_BLR0	(DMA_CH0_BASE+0x014)	//  32bit dma ch0 burst lenght reg
		#define DMA_RTOR0	(DMA_CH0_BASE+0x018)	//  32bit dma ch0 req time out reg
		#define DMA_BUCR0	(DMA_CH0_BASE+0x018)	//  32bit dma ch0 bus utilization reg
		#define DMA_CCNR0	(DMA_CH0_BASE+0x01C)	//  32bit dma ch0
				
		#define DMA_SAR1	(DMA_CH1_BASE+0x000)	//  32bit dma ch1 source addr reg
		#define DMA_DAR1	(DMA_CH1_BASE+0x004)	//  32bit dma ch1 dest addr reg
		#define DMA_CNTR1	(DMA_CH1_BASE+0x008)	//  32bit dma ch1 count reg
		#define DMA_CCR1	(DMA_CH1_BASE+0x00C)	//  32bit dma ch1 control reg
		#define DMA_RSSR1	(DMA_CH1_BASE+0x010)	//  32bit dma ch1 req source sel reg
		#define DMA_BLR1	(DMA_CH1_BASE+0x014)	//  32bit dma ch1 burst lenght reg
		#define DMA_RTOR1	(DMA_CH1_BASE+0x018)	//  32bit dma ch1 req time out reg
		#define DMA_BUCR1	(DMA_CH1_BASE+0x018)	//  32bit dma ch1 bus utilization reg
		#define DMA_CCNR1	(DMA_CH1_BASE+0x01C)	//  32bit dma ch1
				
		#define DMA_SAR2	(DMA_CH2_BASE+0x000)	//  32bit dma ch2 source addr reg
		#define DMA_DAR2	(DMA_CH2_BASE+0x004)	//  32bit dma ch2 dest addr reg
		#define DMA_CNTR2	(DMA_CH2_BASE+0x008)	//  32bit dma ch2 count reg
		#define DMA_CCR2	(DMA_CH2_BASE+0x00C)	//  32bit dma ch2 control reg
		#define DMA_RSSR2	(DMA_CH2_BASE+0x010)	//  32bit dma ch2 req source sel reg
		#define DMA_BLR2	(DMA_CH2_BASE+0x014)	//  32bit dma ch2 burst lenght reg
		#define DMA_RTOR2	(DMA_CH2_BASE+0x018)	//  32bit dma ch2 req time out reg
		#define DMA_BUCR2	(DMA_CH2_BASE+0x018)	//  32bit dma ch2 bus utilization reg
		#define DMA_CCNR2	(DMA_CH2_BASE+0x01C)	//  32bit dma ch2
				
		#define DMA_SAR3	(DMA_CH3_BASE+0x000)	//  32bit dma ch3 source addr reg
		#define DMA_DAR3	(DMA_CH3_BASE+0x004)	//  32bit dma ch3 dest addr reg
		#define DMA_CNTR3	(DMA_CH3_BASE+0x008)	//  32bit dma ch3 count reg
		#define DMA_CCR3	(DMA_CH3_BASE+0x00C)	//  32bit dma ch3 control reg
		#define DMA_RSSR3	(DMA_CH3_BASE+0x010)	//  32bit dma ch3 req source sel reg
		#define DMA_BLR3	(DMA_CH3_BASE+0x014)	//  32bit dma ch3 burst lenght reg
		#define DMA_RTOR3	(DMA_CH3_BASE+0x018)	//  32bit dma ch3 req time out reg
		#define DMA_BUCR3	(DMA_CH3_BASE+0x018)	//  32bit dma ch3 bus utilization reg
		#define DMA_CCNR3	(DMA_CH3_BASE+0x01C)	//  32bit dma ch3
				
		#define DMA_SAR4	(DMA_CH4_BASE+0x000)	//  32bit dma ch4 source addr reg
		#define DMA_DAR4	(DMA_CH4_BASE+0x004)	//  32bit dma ch4 dest addr reg
		#define DMA_CNTR4	(DMA_CH4_BASE+0x008)	//  32bit dma ch4 count reg
		#define DMA_CCR4	(DMA_CH4_BASE+0x00C)	//  32bit dma ch4 control reg
		#define DMA_RSSR4	(DMA_CH4_BASE+0x010)	//  32bit dma ch4 req source sel reg
		#define DMA_BLR4	(DMA_CH4_BASE+0x014)	//  32bit dma ch4 burst lenght reg
		#define DMA_RTOR4	(DMA_CH4_BASE+0x018)	//  32bit dma ch4 req time out reg
		#define DMA_BUCR4	(DMA_CH4_BASE+0x018)	//  32bit dma ch4 bus utilization reg
		#define DMA_CCNR4	(DMA_CH4_BASE+0x01C)	//  32bit dma ch4
				
		#define DMA_SAR5	(DMA_CH5_BASE+0x000)	//  32bit dma ch5 source addr reg
		#define DMA_DAR5	(DMA_CH5_BASE+0x004)	//  32bit dma ch5 dest addr reg
		#define DMA_CNTR5	(DMA_CH5_BASE+0x008)	//  32bit dma ch5 count reg
		#define DMA_CCR5	(DMA_CH5_BASE+0x00C)	//  32bit dma ch5 control reg
		#define DMA_RSSR5	(DMA_CH5_BASE+0x010)	//  32bit dma ch5 req source sel reg
		#define DMA_BLR5	(DMA_CH5_BASE+0x014)	//  32bit dma ch5 burst lenght reg
		#define DMA_RTOR5	(DMA_CH5_BASE+0x018)	//  32bit dma ch5 req time out reg
		#define DMA_BUCR5	(DMA_CH5_BASE+0x018)	//  32bit dma ch5 bus utilization reg
		#define DMA_CCNR5	(DMA_CH5_BASE+0x01C)	//  32bit dma ch5
				
		#define DMA_SAR6	(DMA_CH6_BASE+0x000)	//  32bit dma ch6 source addr reg
		#define DMA_DAR6	(DMA_CH6_BASE+0x004)	//  32bit dma ch6 dest addr reg
		#define DMA_CNTR6	(DMA_CH6_BASE+0x008)	//  32bit dma ch6 count reg
		#define DMA_CCR6	(DMA_CH6_BASE+0x00C)	//  32bit dma ch6 control reg
		#define DMA_RSSR6	(DMA_CH6_BASE+0x010)	//  32bit dma ch6 req source sel reg
		#define DMA_BLR6	(DMA_CH6_BASE+0x014)	//  32bit dma ch6 burst lenght reg
		#define DMA_RTOR6	(DMA_CH6_BASE+0x018)	//  32bit dma ch6 req time out reg
		#define DMA_BUCR6	(DMA_CH6_BASE+0x018)	//  32bit dma ch6 bus utilization reg
		#define DMA_CCNR6	(DMA_CH6_BASE+0x01C)	//  32bit dma ch6
				
		#define DMA_SAR7	(DMA_CH7_BASE+0x000)	//  32bit dma ch7 source addr reg
		#define DMA_DAR7	(DMA_CH7_BASE+0x004)	//  32bit dma ch7 dest addr reg
		#define DMA_CNTR7	(DMA_CH7_BASE+0x008)	//  32bit dma ch7 count reg
		#define DMA_CCR7	(DMA_CH7_BASE+0x00C)	//  32bit dma ch7 control reg
		#define DMA_RSSR7	(DMA_CH7_BASE+0x010)	//  32bit dma ch7 req source sel reg
		#define DMA_BLR7	(DMA_CH7_BASE+0x014)	//  32bit dma ch7 burst lenght reg
		#define DMA_RTOR7	(DMA_CH7_BASE+0x018)	//  32bit dma ch7 req time out reg
		#define DMA_BUCR7	(DMA_CH7_BASE+0x018)	//  32bit dma ch7 bus utilization reg
		#define DMA_CCNR7	(DMA_CH7_BASE+0x01C)	//  32bit dma ch7
				
		#define DMA_SAR8	(DMA_CH8_BASE+0x000)	//  32bit dma ch8 source addr reg
		#define DMA_DAR8	(DMA_CH8_BASE+0x004)	//  32bit dma ch8 dest addr reg
		#define DMA_CNTR8	(DMA_CH8_BASE+0x008)	//  32bit dma ch8 count reg
		#define DMA_CCR8	(DMA_CH8_BASE+0x00C)	//  32bit dma ch8 control reg
		#define DMA_RSSR8	(DMA_CH8_BASE+0x010)	//  32bit dma ch8 req source sel reg
		#define DMA_BLR8	(DMA_CH8_BASE+0x014)	//  32bit dma ch8 burst lenght reg
		#define DMA_RTOR8	(DMA_CH8_BASE+0x018)	//  32bit dma ch8 req time out reg
		#define DMA_BUCR8	(DMA_CH8_BASE+0x018)	//  32bit dma ch8 bus utilization reg
		#define DMA_CCNR8	(DMA_CH8_BASE+0x01C)	//  32bit dma ch8
				
		#define DMA_SAR9	(DMA_CH9_BASE+0x000)	//  32bit dma ch9 source addr reg
		#define DMA_DAR9	(DMA_CH9_BASE+0x004)	//  32bit dma ch9 dest addr reg
		#define DMA_CNTR9	(DMA_CH9_BASE+0x008)	//  32bit dma ch9 count reg
		#define DMA_CCR9	(DMA_CH9_BASE+0x00C)	//  32bit dma ch9 control reg
		#define DMA_RSSR9	(DMA_CH9_BASE+0x010)	//  32bit dma ch9 req source sel reg
		#define DMA_BLR9	(DMA_CH9_BASE+0x014)	//  32bit dma ch9 burst lenght reg
		#define DMA_RTOR9	(DMA_CH9_BASE+0x018)	//  32bit dma ch9 req time out reg
		#define DMA_BUCR9	(DMA_CH9_BASE+0x018)	//  32bit dma ch9 bus utilization reg
		#define DMA_CCNR9	(DMA_CH9_BASE+0x01C)	//  32bit dma ch9
				
		#define DMA_SAR10	(DMA_CH10_BASE+0x000)	//  32bit dma ch10 source addr reg
		#define DMA_DAR10	(DMA_CH10_BASE+0x004)	//  32bit dma ch10 dest addr reg
		#define DMA_CNTR10	(DMA_CH10_BASE+0x008)	//  32bit dma ch10 count reg
		#define DMA_CCR10	(DMA_CH10_BASE+0x00C)	//  32bit dma ch10 control reg
		#define DMA_RSSR10	(DMA_CH10_BASE+0x010)	//  32bit dma ch10 req source sel reg
		#define DMA_BLR10	(DMA_CH10_BASE+0x014)	//  32bit dma ch10 burst lenght reg
		#define DMA_RTOR10	(DMA_CH10_BASE+0x018)	//  32bit dma ch10 req time out reg
		#define DMA_BUCR10	(DMA_CH10_BASE+0x018)	//  32bit dma ch10 bus utilization reg
		#define DMA_CCNR10	(DMA_CH10_BASE+0x01C)	//  32bit dma ch10
				
		#define DMA_SAR11	(DMA_CH11_BASE+0x000)	//  32bit dma ch11 source addr reg
		#define DMA_DAR11	(DMA_CH11_BASE+0x004)	//  32bit dma ch11 dest addr reg
		#define DMA_CNTR11	(DMA_CH11_BASE+0x008)	//  32bit dma ch11 count reg
		#define DMA_CCR11	(DMA_CH11_BASE+0x00C)	//  32bit dma ch11 control reg
		#define DMA_RSSR11	(DMA_CH11_BASE+0x010)	//  32bit dma ch11 req source sel reg
		#define DMA_BLR11	(DMA_CH11_BASE+0x014)	//  32bit dma ch11 burst lenght reg
		#define DMA_RTOR11	(DMA_CH11_BASE+0x018)	//  32bit dma ch11 req time out reg
		#define DMA_BUCR11	(DMA_CH11_BASE+0x018)	//  32bit dma ch11 bus utilization reg
		#define DMA_CCNR11	(DMA_CH11_BASE+0x01C)	//  32bit dma ch11
				
		#define DMA_SAR12	(DMA_CH12_BASE+0x000)	//  32bit dma ch12 source addr reg
		#define DMA_DAR12	(DMA_CH12_BASE+0x004)	//  32bit dma ch12 dest addr reg
		#define DMA_CNTR12	(DMA_CH12_BASE+0x008)	//  32bit dma ch12 count reg
		#define DMA_CCR12	(DMA_CH12_BASE+0x00C)	//  32bit dma ch12 control reg
		#define DMA_RSSR12	(DMA_CH12_BASE+0x010)	//  32bit dma ch12 req source sel reg
		#define DMA_BLR12	(DMA_CH12_BASE+0x014)	//  32bit dma ch12 burst lenght reg
		#define DMA_RTOR12	(DMA_CH12_BASE+0x018)	//  32bit dma ch12 req time out reg
		#define DMA_BUCR12	(DMA_CH12_BASE+0x018)	//  32bit dma ch12 bus utilization reg
		#define DMA_CCNR12	(DMA_CH12_BASE+0x01C)	//  32bit dma ch12
				
		#define DMA_SAR13	(DMA_CH13_BASE+0x000)	//  32bit dma ch13 source addr reg
		#define DMA_DAR13	(DMA_CH13_BASE+0x004)	//  32bit dma ch13 dest addr reg
		#define DMA_CNTR13	(DMA_CH13_BASE+0x008)	//  32bit dma ch13 count reg
		#define DMA_CCR13	(DMA_CH13_BASE+0x00C)	//  32bit dma ch13 control reg
		#define DMA_RSSR13	(DMA_CH13_BASE+0x010)	//  32bit dma ch13 req source sel reg
		#define DMA_BLR13	(DMA_CH13_BASE+0x014)	//  32bit dma ch13 burst lenght reg
		#define DMA_RTOR13	(DMA_CH13_BASE+0x018)	//  32bit dma ch13 req time out reg
		#define DMA_BUCR13	(DMA_CH13_BASE+0x018)	//  32bit dma ch13 bus utilization reg
		#define DMA_CCNR13	(DMA_CH13_BASE+0x01C)	//  32bit dma ch13
				
		#define DMA_SAR14	(DMA_CH14_BASE+0x000)	//  32bit dma ch14 source addr reg
		#define DMA_DAR14	(DMA_CH14_BASE+0x004)	//  32bit dma ch14 dest addr reg
		#define DMA_CNTR14	(DMA_CH14_BASE+0x008)	//  32bit dma ch14 count reg
		#define DMA_CCR14	(DMA_CH14_BASE+0x00C)	//  32bit dma ch14 control reg
		#define DMA_RSSR14	(DMA_CH14_BASE+0x010)	//  32bit dma ch14 req source sel reg
		#define DMA_BLR14	(DMA_CH14_BASE+0x014)	//  32bit dma ch14 burst lenght reg
		#define DMA_RTOR14	(DMA_CH14_BASE+0x018)	//  32bit dma ch14 req time out reg
		#define DMA_BUCR14	(DMA_CH14_BASE+0x018)	//  32bit dma ch14 bus utilization reg
		#define DMA_CCNR14	(DMA_CH14_BASE+0x01C)	//  32bit dma ch14
				
		#define DMA_SAR15	(DMA_CH15_BASE+0x000)	//  32bit dma ch15 source addr reg
		#define DMA_DAR15	(DMA_CH15_BASE+0x004)	//  32bit dma ch15 dest addr reg
		#define DMA_CNTR15	(DMA_CH15_BASE+0x008)	//  32bit dma ch15 count reg
		#define DMA_CCR15	(DMA_CH15_BASE+0x00C)	//  32bit dma ch15 control reg
		#define DMA_RSSR15	(DMA_CH15_BASE+0x010)	//  32bit dma ch15 req source sel reg
		#define DMA_BLR15	(DMA_CH15_BASE+0x014)	//  32bit dma ch15 burst lenght reg
		#define DMA_RTOR15	(DMA_CH15_BASE+0x018)	//  32bit dma ch15 req time out reg
		#define DMA_BUCR15	(DMA_CH15_BASE+0x018)	//  32bit dma ch15 bus utilization reg
		#define DMA_CCNR15	(DMA_CH15_BASE+0x01C)	//  32bit dma ch15
		#endif 
		
#define _reg_DMA_TCR			(*((volatile unsigned long *)(MX2_IO_ADDRESS(DMA_TEST_BASE+0x000))))	//  32bit dma test control reg
#define _reg_DMA_TFIFOAR		(*((volatile unsigned long *)(MX2_IO_ADDRESS(DMA_TEST_BASE+0x004))))	//  32bit dma test fifo A reg
#define _reg_DMA_TDRR			(*((volatile unsigned long *)(MX2_IO_ADDRESS(DMA_TEST_BASE+0x008))))	//  32bit dma test request reg
#define _reg_DMA_TDIPR			(*((volatile unsigned long *)(MX2_IO_ADDRESS(DMA_TEST_BASE+0x00C))))	//  32bit dma test in progress reg
#define _reg_DMA_TFIFOBR		(*((volatile unsigned long *)(MX2_IO_ADDRESS(DMA_TEST_BASE+0x010))))	//  32bit dma test fifo B reg
		
//#########################################		
//# WDOG                                  #		
//# $1000_2000 to $1000_2FFF              #		
//#########################################		
#define GPT1 0
#define GPT2 1
#define GPT3 2

#define WDOG_BASE_ADDR	0x10002000	
#define _reg_WDOG_WCR			(*((volatile unsigned long *)(MX2_IO_ADDRESS(WDOG_BASE_ADDR+0x00))))	//  16bit watchdog control reg
#define _reg_WDOG_WSR			(*((volatile unsigned long *)(MX2_IO_ADDRESS(WDOG_BASE_ADDR+0x02))))	//  16bit watchdog service reg
#define _reg_WDOG_WRSR			(*((volatile unsigned long *)(MX2_IO_ADDRESS(WDOG_BASE_ADDR+0x04))))	//  16bit watchdog reset status reg
#define _reg_WDOG_WPR			(*((volatile unsigned long *)(MX2_IO_ADDRESS(WDOG_BASE_ADDR+0x06))))	//  16bit watchdog protect reg

#define GPT_BASE_ADDR(x)		(0x10003000+0x1000*x)
#define _reg_GPT_TCTL(x)		(*((volatile unsigned long *)(MX2_IO_ADDRESS(GPT_BASE_ADDR(x)+0x00))))	//  32bit timer  control reg
#define _reg_GPT_TPRER(x)		(*((volatile unsigned long *)(MX2_IO_ADDRESS(GPT_BASE_ADDR(x)+0x04))))	//  32bit timer  prescaler reg
#define _reg_GPT_TCMP(x)		(*((volatile unsigned long *)(MX2_IO_ADDRESS(GPT_BASE_ADDR(x)+0x08))))	//  32bit timer  compare reg
#define _reg_GPT_TCR(x)			(*((volatile unsigned long *)(MX2_IO_ADDRESS(GPT_BASE_ADDR(x)+0x0C))))	//  32bit timer  capture reg
#define _reg_GPT_TCN(x)			(*((volatile unsigned long *)(MX2_IO_ADDRESS(GPT_BASE_ADDR(x)+0x10))))	//  32bit timer  counter reg
#define _reg_GPT_TSTAT(x)		(*((volatile unsigned long *)(MX2_IO_ADDRESS(GPT_BASE_ADDR(x)+0x14))))	//  32bit timer  status reg

		#if 0		
		//#########################################		
		//# GPT1                                  #		
		//# $1000_3000 to $1000_3FFF              #		
		//#########################################		
		#define GPT1_BASE_ADDR	0x10003000	
		#define GPT1_TCTL1	(GPT1_BASE_ADDR+0x00)	//  32bit timer 1 control reg
		#define GPT1_TPRER1	(GPT1_BASE_ADDR+0x04)	//  32bit timer 1 prescaler reg
		#define GPT1_TCMP1	(GPT1_BASE_ADDR+0x08)	//  32bit timer 1 compare reg
		#define GPT1_TCR1	(GPT1_BASE_ADDR+0x0C)	//  32bit timer 1 capture reg
		#define GPT1_TCN1	(GPT1_BASE_ADDR+0x10)	//  32bit timer 1 counter reg
		#define GPT1_TSTAT1	(GPT1_BASE_ADDR+0x14)	//  32bit timer 1 status reg
				
		//#########################################		
		//# GPT2                                  #		
		//# $1000_4000 to $1000_4FFF              #		
		//#########################################		
		#define GPT2_BASE_ADDR	0x10004000	
		#define GPT2_TCTL2	(GPT2_BASE_ADDR+0x00)	//  32bit timer 2 control reg
		#define GPT2_TPRER2	(GPT2_BASE_ADDR+0x04)	//  32bit timer 2 prescaler reg
		#define GPT2_TCMP2	(GPT2_BASE_ADDR+0x08)	//  32bit timer 2 compare reg
		#define GPT2_TCR2	(GPT2_BASE_ADDR+0x0C)	//  32bit timer 2 capture reg
		#define GPT2_TCN2	(GPT2_BASE_ADDR+0x10)	//  32bit timer 2 counter reg
		#define GPT2_TSTAT2	(GPT2_BASE_ADDR+0x14)	//  32bit timer 2 status reg
				
		//#########################################		
		//# GPT3                                  #		
		//# $1000_5000 to $1000_5FFF              #		
		//#########################################		
		#define GPT3_BASE_ADDR	0x10005000	
		#define GPT3_TCTL3	(GPT3_BASE_ADDR+0x00)	//  32bit timer 3 control reg
		#define GPT3_TPRER3	(GPT3_BASE_ADDR+0x04)	//  32bit timer 3 prescaler reg
		#define GPT3_TCMP3	(GPT3_BASE_ADDR+0x08)	//  32bit timer 3 compare reg
		#define GPT3_TCR3	(GPT3_BASE_ADDR+0x0C)	//  32bit timer 3 capture reg
		#define GPT3_TCN3	(GPT3_BASE_ADDR+0x10)	//  32bit timer 3 counter reg
		#define GPT3_TSTAT3	(GPT3_BASE_ADDR+0x14)	//  32bit timer 3 status reg
		#endif
		
//#########################################		
//# PWM                                   #		
//# $1000_6000 to $1000_6FFF              #		
//#########################################		
#define PWM_BASE_ADDR	0x10006000	
#define _reg_PWM_PWMC			(*((volatile unsigned long *)(MX2_IO_ADDRESS(PWM_BASE_ADDR+0x00))))	//  32bit pwm control reg
#define _reg_PWM_PWMS			(*((volatile unsigned long *)(MX2_IO_ADDRESS(PWM_BASE_ADDR+0x04))))	//  32bit pwm sample reg
#define _reg_PWM_PWMP			(*((volatile unsigned long *)(MX2_IO_ADDRESS(PWM_BASE_ADDR+0x08))))	//  32bit pwm period reg
#define _reg_PWM_PWMCNT			(*((volatile unsigned long *)(MX2_IO_ADDRESS(PWM_BASE_ADDR+0x0C))))	//  32bit pwm counter reg
#define _reg_PWM_PWMTEST1		(*((volatile unsigned long *)(MX2_IO_ADDRESS(PWM_BASE_ADDR+0x10))))	//  32bit pwm test reg
		
//#########################################		
//# RTC                                   #		
//# $1000_7000 to $1000_7FFF              #		
//#########################################		
#define RTC_BASE_ADDR	0x10007000	
#define _reg_RTC_HOURMIN		(*((volatile unsigned long *)(MX2_IO_ADDRESS(RTC_BASE_ADDR+0x00))))	//  32bit rtc hour/min counter reg
#define _reg_RTC_SECOND			(*((volatile unsigned long *)(MX2_IO_ADDRESS(RTC_BASE_ADDR+0x04))))	//  32bit rtc seconds counter reg
#define _reg_RTC_ALRM_HM		(*((volatile unsigned long *)(MX2_IO_ADDRESS(RTC_BASE_ADDR+0x08))))	//  32bit rtc alarm hour/min reg
#define _reg_RTC_ALRM_SEC		(*((volatile unsigned long *)(MX2_IO_ADDRESS(RTC_BASE_ADDR+0x0C))))	//  32bit rtc alarm seconds reg
#define _reg_RTC_RTCCTL			(*((volatile unsigned long *)(MX2_IO_ADDRESS(RTC_BASE_ADDR+0x10))))	//  32bit rtc control reg
#define _reg_RTC_RTCISR			(*((volatile unsigned long *)(MX2_IO_ADDRESS(RTC_BASE_ADDR+0x14))))	//  32bit rtc interrupt status reg
#define _reg_RTC_RTCIENR		(*((volatile unsigned long *)(MX2_IO_ADDRESS(RTC_BASE_ADDR+0x18))))	//  32bit rtc interrupt enable reg
#define _reg_RTC_STPWCH			(*((volatile unsigned long *)(MX2_IO_ADDRESS(RTC_BASE_ADDR+0x1C))))	//  32bit rtc stopwatch min reg
#define _reg_RTC_DAYR			(*((volatile unsigned long *)(MX2_IO_ADDRESS(RTC_BASE_ADDR+0x20))))	//  32bit rtc days counter reg
#define _reg_RTC_DAYALARM		(*((volatile unsigned long *)(MX2_IO_ADDRESS(RTC_BASE_ADDR+0x24))))	//  32bit rtc day alarm reg
#define _reg_RTC_TEST1			(*((volatile unsigned long *)(MX2_IO_ADDRESS(RTC_BASE_ADDR+0x28))))	//  32bit rtc test reg 1
#define _reg_RTC_TEST2			(*((volatile unsigned long *)(MX2_IO_ADDRESS(RTC_BASE_ADDR+0x2C))))	//  32bit rtc test reg 2
#define _reg_RTC_TEST3			(*((volatile unsigned long *)(MX2_IO_ADDRESS(RTC_BASE_ADDR+0x30))))	//  32bit rtc test reg 3
		
//#########################################		
//# KPP                                   #		
//# $1000_8000 to $1000_8FFF              #		
//#########################################		
#define KPP_BASE_ADDR	0x10008000	
#define _reg_KPP_KPCR			(*((volatile unsigned short *)(MX2_IO_ADDRESS(KPP_BASE_ADDR+0x00))))	//  16bit kpp keypad control reg
#define _reg_KPP_KPSR			(*((volatile unsigned short *)(MX2_IO_ADDRESS(KPP_BASE_ADDR+0x02))))	//  16bit kpp keypad status reg
#define _reg_KPP_KDDR			(*((volatile unsigned short *)(MX2_IO_ADDRESS(KPP_BASE_ADDR+0x04))))	//  16bit kpp keypad data directon reg
#define _reg_KPP_KPDR			(*((volatile unsigned short *)(MX2_IO_ADDRESS(KPP_BASE_ADDR+0x06))))	//  16bit kpp keypad data reg
		
//#########################################		
//# OWIRE                                 #		
//# $1000_9000 to $1000_9FFF              #		
//#########################################		
#define OWIRE_BASE_ADDR	0x10009000	
#define _reg_OWIRE_CTRL			(*((volatile unsigned long *)(MX2_IO_ADDRESS(OWIRE_BASE_ADDR+0x00))))	//  16bit owire control reg
#define _reg_OWIRE_TIME_DIV		(*((volatile unsigned long *)(MX2_IO_ADDRESS(OWIRE_BASE_ADDR+0x02))))	//  16bit owire time divider reg
#define _reg_OWIRE_RESET		(*((volatile unsigned long *)(MX2_IO_ADDRESS(OWIRE_BASE_ADDR+0x04))))	//  16bit owire reset reg

#define UART_1	0
#define UART_2	1
#define UART_3	2
#define UART_4	3

/* UART1-UART4*/
#define UART_BASE_ADDR(x)		(0x1000A000+0x1000*(x))
#define _reg_UART_URXD(x)		(*((volatile unsigned long *)(MX2_IO_ADDRESS(UART_BASE_ADDR(x)+0x00))))	//  32bit uart1 receiver reg
#define _reg_UART_UTXD(x)		(*((volatile unsigned long *)(MX2_IO_ADDRESS(UART_BASE_ADDR(x)+0x40))))	//  32bit uart1 transmitter reg
#define _reg_UART_UCR1(x)		(*((volatile unsigned long *)(MX2_IO_ADDRESS(UART_BASE_ADDR(x)+0x80))))	//  32bit uart1 control 1 reg
#define _reg_UART_UCR2(x)		(*((volatile unsigned long *)(MX2_IO_ADDRESS(UART_BASE_ADDR(x)+0x84))))	//  32bit uart1 control 2 reg
#define _reg_UART_UCR3(x)		(*((volatile unsigned long *)(MX2_IO_ADDRESS(UART_BASE_ADDR(x)+0x88))))	//  32bit uart1 control 3 reg
#define _reg_UART_UCR4(x)		(*((volatile unsigned long *)(MX2_IO_ADDRESS(UART_BASE_ADDR(x)+0x8C))))	//  32bit uart1 control 4 reg
#define _reg_UART_UFCR(x)		(*((volatile unsigned long *)(MX2_IO_ADDRESS(UART_BASE_ADDR(x)+0x90))))	//  32bit uart1 fifo control reg
#define _reg_UART_USR1(x)		(*((volatile unsigned long *)(MX2_IO_ADDRESS(UART_BASE_ADDR(x)+0x94))))	//  32bit uart1 status 1 reg
#define _reg_UART_USR2(x)		(*((volatile unsigned long *)(MX2_IO_ADDRESS(UART_BASE_ADDR(x)+0x98))))	//  32bit uart1 status 2 reg
#define _reg_UART_UESC(x)		(*((volatile unsigned long *)(MX2_IO_ADDRESS(UART_BASE_ADDR(x)+0x9C))))	//  32bit uart1 escape char reg
#define _reg_UART_UTIM(x)		(*((volatile unsigned long *)(MX2_IO_ADDRESS(UART_BASE_ADDR(x)+0xA0))))	//  32bit uart1 escape timer reg
#define _reg_UART_UBIR(x)		(*((volatile unsigned long *)(MX2_IO_ADDRESS(UART_BASE_ADDR(x)+0xA4))))	//  32bit uart1 BRM incremental reg
#define _reg_UART_UBMR(x)		(*((volatile unsigned long *)(MX2_IO_ADDRESS(UART_BASE_ADDR(x)+0xA8))))	//  32bit uart1 BRM modulator reg
#define _reg_UART_UBRC(x)		(*((volatile unsigned long *)(MX2_IO_ADDRESS(UART_BASE_ADDR(x)+0xAC))))	//  32bit uart1 baud rate count reg
#define _reg_UART_ONEMS(x)		(*((volatile unsigned long *)(MX2_IO_ADDRESS(UART_BASE_ADDR(x)+0xB0))))	//  32bit uart1 one ms reg
#define _reg_UART_UTS(x)		(*((volatile unsigned long *)(MX2_IO_ADDRESS(UART_BASE_ADDR(x)+0xB4))))	//  32bit uart1 test reg

		#if 0		
		//#########################################		
		//# UART1                                 #		
		//# $1000_A000 to $1000_AFFF              #		
		//#########################################		
		#define UART1_BASE_ADDR	0x1000A000	
		#define UART1_URXD_1	(UART1_BASE_ADDR+0x00)	//  32bit uart1 receiver reg
		#define UART1_UTXD_1	(UART1_BASE_ADDR+0x40)	//  32bit uart1 transmitter reg
		#define UART1_UCR1_1	(UART1_BASE_ADDR+0x80)	//  32bit uart1 control 1 reg
		#define UART1_UCR2_1	(UART1_BASE_ADDR+0x84)	//  32bit uart1 control 2 reg
		#define UART1_UCR3_1	(UART1_BASE_ADDR+0x88)	//  32bit uart1 control 3 reg
		#define UART1_UCR4_1	(UART1_BASE_ADDR+0x8C)	//  32bit uart1 control 4 reg
		#define UART1_UFCR_1	(UART1_BASE_ADDR+0x90)	//  32bit uart1 fifo control reg
		#define UART1_USR1_1	(UART1_BASE_ADDR+0x94)	//  32bit uart1 status 1 reg
		#define UART1_USR2_1	(UART1_BASE_ADDR+0x98)	//  32bit uart1 status 2 reg
		#define UART1_UESC_1	(UART1_BASE_ADDR+0x9C)	//  32bit uart1 escape char reg
		#define UART1_UTIM_1	(UART1_BASE_ADDR+0xA0)	//  32bit uart1 escape timer reg
		#define UART1_UBIR_1	(UART1_BASE_ADDR+0xA4)	//  32bit uart1 BRM incremental reg
		#define UART1_UBMR_1	(UART1_BASE_ADDR+0xA8)	//  32bit uart1 BRM modulator reg
		#define UART1_UBRC_1	(UART1_BASE_ADDR+0xAC)	//  32bit uart1 baud rate count reg
		#define UART1_ONEMS_1	(UART1_BASE_ADDR+0xB0)	//  32bit uart1 one ms reg
		#define UART1_UTS_1	(UART1_BASE_ADDR+0xB4)	//  32bit uart1 test reg
				
		//#########################################		
		//# UART2                                 #		
		//# $1000_B000 to $1000_BFFF              #		
		//#########################################		
		#define UART2_BASE_ADDR	0x1000B000	
		#define UART2_URXD_2	(UART2_BASE_ADDR+0x00)	//  32bit uart2 receiver reg
		#define UART2_UTXD_2	(UART2_BASE_ADDR+0x40)	//  32bit uart2 transmitter reg
		#define UART2_UCR1_2	(UART2_BASE_ADDR+0x80)	//  32bit uart2 control 1 reg
		#define UART2_UCR2_2	(UART2_BASE_ADDR+0x84)	//  32bit uart2 control 2 reg
		#define UART2_UCR3_2	(UART2_BASE_ADDR+0x88)	//  32bit uart2 control 3 reg
		#define UART2_UCR4_2	(UART2_BASE_ADDR+0x8C)	//  32bit uart2 control 4 reg
		#define UART2_UFCR_2	(UART2_BASE_ADDR+0x90)	//  32bit uart2 fifo control reg
		#define UART2_USR1_2	(UART2_BASE_ADDR+0x94)	//  32bit uart2 status 1 reg
		#define UART2_USR2_2	(UART2_BASE_ADDR+0x98)	//  32bit uart2 status 2 reg
		#define UART2_UESC_2	(UART2_BASE_ADDR+0x9C)	//  32bit uart2 escape char reg
		#define UART2_UTIM_2	(UART2_BASE_ADDR+0xA0)	//  32bit uart2 escape timer reg
		#define UART2_UBIR_2	(UART2_BASE_ADDR+0xA4)	//  32bit uart2 BRM incremental reg
		#define UART2_UBMR_2	(UART2_BASE_ADDR+0xA8)	//  32bit uart2 BRM modulator reg
		#define UART2_UBRC_2	(UART2_BASE_ADDR+0xAC)	//  32bit uart2 baud rate count reg
		#define UART2_ONEMS_2	(UART2_BASE_ADDR+0xB0)	//  32bit uart2 one ms reg
		#define UART2_UTS_2	(UART2_BASE_ADDR+0xB4)	//  32bit uart2 test reg
				
		//#########################################		
		//# UART3                                 #		
		//# $1000_C000 to $1000_CFFF              #		
		//#########################################		
		#define UART3_BASE_ADDR	0x1000C000	
		#define UART3_URXD_3	(UART3_BASE_ADDR+0x00)	//  32bit uart3 receiver reg
		#define UART3_UTXD_3	(UART3_BASE_ADDR+0x40)	//  32bit uart3 transmitter reg
		#define UART3_UCR1_3	(UART3_BASE_ADDR+0x80)	//  32bit uart3 control 1 reg
		#define UART3_UCR2_3	(UART3_BASE_ADDR+0x84)	//  32bit uart3 control 2 reg
		#define UART3_UCR3_3	(UART3_BASE_ADDR+0x88)	//  32bit uart3 control 3 reg
		#define UART3_UCR4_3	(UART3_BASE_ADDR+0x8C)	//  32bit uart3 control 4 reg
		#define UART3_UFCR_3	(UART3_BASE_ADDR+0x90)	//  32bit uart3 fifo control reg
		#define UART3_USR1_3	(UART3_BASE_ADDR+0x94)	//  32bit uart3 status 1 reg
		#define UART3_USR2_3	(UART3_BASE_ADDR+0x98)	//  32bit uart3 status 2 reg
		#define UART3_UESC_3	(UART3_BASE_ADDR+0x9C)	//  32bit uart3 escape char reg
		#define UART3_UTIM_3	(UART3_BASE_ADDR+0xA0)	//  32bit uart3 escape timer reg
		#define UART3_UBIR_3	(UART3_BASE_ADDR+0xA4)	//  32bit uart3 BRM incremental reg
		#define UART3_UBMR_3	(UART3_BASE_ADDR+0xA8)	//  32bit uart3 BRM modulator reg
		#define UART3_UBRC_3	(UART3_BASE_ADDR+0xAC)	//  32bit uart3 baud rate count reg
		#define UART3_ONEMS_3	(UART3_BASE_ADDR+0xB0)	//  32bit uart3 one ms reg
		#define UART3_UTS_3	(UART3_BASE_ADDR+0xB4)	//  32bit uart3 test reg
				
		//#########################################		
		//# UART4                                 #		
		//# $1000_D000 to $1000_DFFF              #		
		//#########################################		
		#define UART4_BASE_ADDR	0x1000D000	
		#define UART4_URXD_4	(UART4_BASE_ADDR+0x00)	//  32bit uart4 receiver reg
		#define UART4_UTXD_4	(UART4_BASE_ADDR+0x40)	//  32bit uart4 transmitter reg
		#define UART4_UCR1_4	(UART4_BASE_ADDR+0x80)	//  32bit uart4 control 1 reg
		#define UART4_UCR2_4	(UART4_BASE_ADDR+0x84)	//  32bit uart4 control 2 reg
		#define UART4_UCR3_4	(UART4_BASE_ADDR+0x88)	//  32bit uart4 control 3 reg
		#define UART4_UCR4_4	(UART4_BASE_ADDR+0x8C)	//  32bit uart4 control 4 reg
		#define UART4_UFCR_4	(UART4_BASE_ADDR+0x90)	//  32bit uart4 fifo control reg
		#define UART4_USR1_4	(UART4_BASE_ADDR+0x94)	//  32bit uart4 status 1 reg
		#define UART4_USR2_4	(UART4_BASE_ADDR+0x98)	//  32bit uart4 status 2 reg
		#define UART4_UESC_4	(UART4_BASE_ADDR+0x9C)	//  32bit uart4 escape char reg
		#define UART4_UTIM_4	(UART4_BASE_ADDR+0xA0)	//  32bit uart4 escape timer reg
		#define UART4_UBIR_4	(UART4_BASE_ADDR+0xA4)	//  32bit uart4 BRM incremental reg
		#define UART4_UBMR_4	(UART4_BASE_ADDR+0xA8)	//  32bit uart4 BRM modulator reg
		#define UART4_UBRC_4	(UART4_BASE_ADDR+0xAC)	//  32bit uart4 baud rate count reg
		#define UART4_ONEMS_4	(UART4_BASE_ADDR+0xB0)	//  32bit uart4 one ms reg
		#define UART4_UTS_4	(UART4_BASE_ADDR+0xB4)	//  32bit uart4 test reg
		
		#endif		
//#########################################		
//# CSPI1-2                                 #		
//# 				             #		
//#########################################
#define CSPI_BASE_ADDR(x)		(0x1000E000+0x1000*(x-1))
#define _reg_CSPI_RXDATAREG(x)		(*((volatile unsigned long *)(MX2_IO_ADDRESS(CSPI_BASE_ADDR(x)+0x00))))	//  32bit cspi1 receive data reg
#define _reg_CSPI_TXDATAREG(x)		(*((volatile unsigned long *)(MX2_IO_ADDRESS(CSPI_BASE_ADDR(x)+0x04))))	//  32bit cspi1 transmit data reg
#define _reg_CSPI_CONTROLREG(x)		(*((volatile unsigned long *)(MX2_IO_ADDRESS(CSPI_BASE_ADDR(x)+0x08))))	//  32bit cspi1 control reg
#define _reg_CSPI_INTREG(x)		(*((volatile unsigned long *)(MX2_IO_ADDRESS(CSPI_BASE_ADDR(x)+0x0C))))	//  32bit cspi1 interrupt stat/ctr reg
#define _reg_CSPI_TESTREG(x)		(*((volatile unsigned long *)(MX2_IO_ADDRESS(CSPI_BASE_ADDR(x)+0x10))))	//  32bit cspi1 test reg
#define _reg_CSPI_PERIODREG(x)		(*((volatile unsigned long *)(MX2_IO_ADDRESS(CSPI_BASE_ADDR(x)+0x14))))	//  32bit cspi1 sample period ctrl reg
#define _reg_CSPI_DMAREG(x)		(*((volatile unsigned long *)(MX2_IO_ADDRESS(CSPI_BASE_ADDR(x)+0x18))))	//  32bit cspi1 dma ctrl reg
#define _reg_CSPI_RESETREG(x)		(*((volatile unsigned long *)(MX2_IO_ADDRESS(CSPI_BASE_ADDR(x)+0x1C))))	//  32bit cspi1 soft reset reg
		
		#if 0
		//#########################################		
		//# CSPI1                                 #		
		//# $1000_E000 to $1000_EFFF              #		
		//#########################################		
		#define CSPI1_BASE_ADDR	0x1000E000	
		#define CSPI1_RXDATAREG1	(CSPI1_BASE_ADDR+0x00)	//  32bit cspi1 receive data reg
		#define CSPI1_TXDATAREG1	(CSPI1_BASE_ADDR+0x04)	//  32bit cspi1 transmit data reg
		#define CSPI1_CONTROLREG1	(CSPI1_BASE_ADDR+0x08)	//  32bit cspi1 control reg
		#define CSPI1_INTREG1	(CSPI1_BASE_ADDR+0x0C)	//  32bit cspi1 interrupt stat/ctr reg
		#define CSPI1_TESTREG1	(CSPI1_BASE_ADDR+0x10)	//  32bit cspi1 test reg
		#define CSPI1_PERIODREG1	(CSPI1_BASE_ADDR+0x14)	//  32bit cspi1 sample period ctrl reg
		#define CSPI1_DMAREG1	(CSPI1_BASE_ADDR+0x18)	//  32bit cspi1 dma ctrl reg
		#define CSPI1_RESETREG1	(CSPI1_BASE_ADDR+0x1C)	//  32bit cspi1 soft reset reg
				
		//#########################################		
		//# CSPI2                                 #		
		//# $1000_F000 to $1000_FFFF              #		
		//#########################################		
		#define CSPI2_BASE_ADDR	0x1000F000	
		#define CSPI2_RXDATAREG1	(CSPI2_BASE_ADDR+0x00)	//  32bit cspi2 receive data reg
		#define CSPI2_TXDATAREG1	(CSPI2_BASE_ADDR+0x04)	//  32bit cspi2 transmit data reg
		#define CSPI2_CONTROLREG1	(CSPI2_BASE_ADDR+0x08)	//  32bit cspi2 control reg
		#define CSPI2_INTREG1		(CSPI2_BASE_ADDR+0x0C)	//  32bit cspi2 interrupt stat/ctr reg
		#define CSPI2_TESTREG1		(CSPI2_BASE_ADDR+0x10)	//  32bit cspi2 test reg
		#define CSPI2_PERIODREG1	(CSPI2_BASE_ADDR+0x14)	//  32bit cspi2 sample period ctrl reg
		#define CSPI2_DMAREG1		(CSPI2_BASE_ADDR+0x18)	//  32bit cspi2 dma ctrl reg
		#define CSPI2_RESETREG1		(CSPI2_BASE_ADDR+0x1C)	//  32bit cspi2 soft reset reg
		#endif		
		
/*SSI1 SSI2*/		
#define SSI_BASE_ADDR(x)		(0x10010000 + 0x1000*(x-1))		
#define _reg_SSI_STX0(x)		(*((volatile unsigned long *)(MX2_IO_ADDRESS(SSI_BASE_ADDR(x)+0x00))))	//  32bit ssi1 tx reg 0
#define _reg_SSI_STX1(x)		(*((volatile unsigned long *)(MX2_IO_ADDRESS(SSI_BASE_ADDR(x)+0x04))))	//  32bit ssi1 tx reg 1
#define _reg_SSI_SRX0(x)		(*((volatile unsigned long *)(MX2_IO_ADDRESS(SSI_BASE_ADDR(x)+0x08))))	//  32bit ssi1 rx reg 0
#define _reg_SSI_SRX1(x)		(*((volatile unsigned long *)(MX2_IO_ADDRESS(SSI_BASE_ADDR(x)+0x0C))))	//  32bit ssi1 rx reg 1
#define _reg_SSI_SCR(x)			(*((volatile unsigned long *)(MX2_IO_ADDRESS(SSI_BASE_ADDR(x)+0x10))))	//  32bit ssi1 control reg
#define _reg_SSI_SISR(x)		(*((volatile unsigned long *)(MX2_IO_ADDRESS(SSI_BASE_ADDR(x)+0x14))))	//  32bit ssi1 intr status reg
#define _reg_SSI_SIER(x)		(*((volatile unsigned long *)(MX2_IO_ADDRESS(SSI_BASE_ADDR(x)+0x18))))	//  32bit ssi1 intr enable reg
#define _reg_SSI_STCR(x)		(*((volatile unsigned long *)(MX2_IO_ADDRESS(SSI_BASE_ADDR(x)+0x1C))))	//  32bit ssi1 tx config reg
#define _reg_SSI_SRCR(x)		(*((volatile unsigned long *)(MX2_IO_ADDRESS(SSI_BASE_ADDR(x)+0x20))))	//  32bit ssi1 rx config reg
#define _reg_SSI_STCCR(x)		(*((volatile unsigned long *)(MX2_IO_ADDRESS(SSI_BASE_ADDR(x)+0x24))))	//  32bit ssi1 tx clock control reg
#define _reg_SSI_SRCCR(x)		(*((volatile unsigned long *)(MX2_IO_ADDRESS(SSI_BASE_ADDR(x)+0x28))))	//  32bit ssi1 rx clock control reg
#define _reg_SSI_SFCSR(x)		(*((volatile unsigned long *)(MX2_IO_ADDRESS(SSI_BASE_ADDR(x)+0x2C))))	//  32bit ssi1 fifo control/status reg
#define _reg_SSI_STR(x)			(*((volatile unsigned long *)(MX2_IO_ADDRESS(SSI_BASE_ADDR(x)+0x30))))	//  32bit ssi1 test reg
#define _reg_SSI_SOR(x)			(*((volatile unsigned long *)(MX2_IO_ADDRESS(SSI_BASE_ADDR(x)+0x34))))	//  32bit ssi1 option reg
#define _reg_SSI_SACNT(x)		(*((volatile unsigned long *)(MX2_IO_ADDRESS(SSI_BASE_ADDR(x)+0x38))))	//  32bit ssi1 ac97 control reg
#define _reg_SSI_SACADD(x)		(*((volatile unsigned long *)(MX2_IO_ADDRESS(SSI_BASE_ADDR(x)+0x3C))))	//  32bit ssi1 ac97 cmd addr reg
#define _reg_SSI_SACDAT(x)		(*((volatile unsigned long *)(MX2_IO_ADDRESS(SSI_BASE_ADDR(x)+0x40))))	//  32bit ssi1 ac97 cmd data reg
#define _reg_SSI_SATAG(x)		(*((volatile unsigned long *)(MX2_IO_ADDRESS(SSI_BASE_ADDR(x)+0x44))))	//  32bit ssi1 ac97 tag reg
#define _reg_SSI_STMSK(x)		(*((volatile unsigned long *)(MX2_IO_ADDRESS(SSI_BASE_ADDR(x)+0x48))))	//  32bit ssi1 tx time slot mask reg
#define _reg_SSI_SRMSK(x)		(*((volatile unsigned long *)(MX2_IO_ADDRESS(SSI_BASE_ADDR(x)+0x4C))))	//  32bit ssi1 rx time slot mask reg		

		#if 0		
		//#########################################		
		//# SSI1                                  #		
		//# $1001_0000 to $1001_0FFF              #		
		//#########################################		
		#define SSI1_BASE_ADDR	0x10010000	
		#define SSI1_STX0	(SSI1_BASE_ADDR+0x00)	//  32bit ssi1 tx reg 0
		#define SSI1_STX1	(SSI1_BASE_ADDR+0x04)	//  32bit ssi1 tx reg 1
		#define SSI1_SRX0	(SSI1_BASE_ADDR+0x08)	//  32bit ssi1 rx reg 0
		#define SSI1_SRX1	(SSI1_BASE_ADDR+0x0C)	//  32bit ssi1 rx reg 1
		#define SSI1_SCR	(SSI1_BASE_ADDR+0x10)	//  32bit ssi1 control reg
		#define SSI1_SISR	(SSI1_BASE_ADDR+0x14)	//  32bit ssi1 intr status reg
		#define SSI1_SIER	(SSI1_BASE_ADDR+0x18)	//  32bit ssi1 intr enable reg
		#define SSI1_STCR	(SSI1_BASE_ADDR+0x1C)	//  32bit ssi1 tx config reg
		#define SSI1_SRCR	(SSI1_BASE_ADDR+0x20)	//  32bit ssi1 rx config reg
		#define SSI1_STCCR	(SSI1_BASE_ADDR+0x24)	//  32bit ssi1 tx clock control reg
		#define SSI1_SRCCR	(SSI1_BASE_ADDR+0x28)	//  32bit ssi1 rx clock control reg
		#define SSI1_SFCSR	(SSI1_BASE_ADDR+0x2C)	//  32bit ssi1 fifo control/status reg
		#define SSI1_STR	(SSI1_BASE_ADDR+0x30)	//  32bit ssi1 test reg
		#define SSI1_SOR	(SSI1_BASE_ADDR+0x34)	//  32bit ssi1 option reg
		#define SSI1_SACNT	(SSI1_BASE_ADDR+0x38)	//  32bit ssi1 ac97 control reg
		#define SSI1_SACADD	(SSI1_BASE_ADDR+0x3C)	//  32bit ssi1 ac97 cmd addr reg
		#define SSI1_SACDAT	(SSI1_BASE_ADDR+0x40)	//  32bit ssi1 ac97 cmd data reg
		#define SSI1_SATAG	(SSI1_BASE_ADDR+0x44)	//  32bit ssi1 ac97 tag reg
		#define SSI1_STMSK	(SSI1_BASE_ADDR+0x48)	//  32bit ssi1 tx time slot mask reg
		#define SSI1_SRMSK	(SSI1_BASE_ADDR+0x4C)	//  32bit ssi1 rx time slot mask reg
				
				
		//#########################################		
		//# SSI2                                  #		
		//# $1001_1000 to $1001_1FFF              #		
		//#########################################		
		#define SSI2_BASE_ADDR	0x10011000	
		#define SSI2_STX0	(SSI2_BASE_ADDR+0x00)	//  32bit ssi2 tx reg 0
		#define SSI2_STX1	(SSI2_BASE_ADDR+0x04)	//  32bit ssi2 tx reg 1
		#define SSI2_SRX0	(SSI2_BASE_ADDR+0x08)	//  32bit ssi2 rx reg 0
		#define SSI2_SRX1	(SSI2_BASE_ADDR+0x0C)	//  32bit ssi2 rx reg 1
		#define SSI2_SCR	(SSI2_BASE_ADDR+0x10)	//  32bit ssi2 control reg
		#define SSI2_SISR	(SSI2_BASE_ADDR+0x14)	//  32bit ssi2 intr status reg
		#define SSI2_SIER	(SSI2_BASE_ADDR+0x18)	//  32bit ssi2 intr enable reg
		#define SSI2_STCR	(SSI2_BASE_ADDR+0x1C)	//  32bit ssi2 tx config reg
		#define SSI2_SRCR	(SSI2_BASE_ADDR+0x20)	//  32bit ssi2 rx config reg
		#define SSI2_STCCR	(SSI2_BASE_ADDR+0x24)	//  32bit ssi2 tx clock control reg
		#define SSI2_SRCCR	(SSI2_BASE_ADDR+0x28)	//  32bit ssi2 rx clock control reg
		#define SSI2_SFCSR	(SSI2_BASE_ADDR+0x2C)	//  32bit ssi2 fifo control/status reg
		#define SSI2_STR	(SSI2_BASE_ADDR+0x30)	//  32bit ssi2 test reg
		#define SSI2_SOR	(SSI2_BASE_ADDR+0x34)	//  32bit ssi2 option reg
		#define SSI2_SACNT	(SSI2_BASE_ADDR+0x38)	//  32bit ssi2 ac97 control reg
		#define SSI2_SACADD	(SSI2_BASE_ADDR+0x3C)	//  32bit ssi2 ac97 cmd addr reg
		#define SSI2_SACDAT	(SSI2_BASE_ADDR+0x40)	//  32bit ssi2 ac97 cmd data reg
		#define SSI2_SATAG	(SSI2_BASE_ADDR+0x44)	//  32bit ssi2 ac97 tag reg
		#define SSI2_STMSK	(SSI2_BASE_ADDR+0x48)	//  32bit ssi2 tx time slot mask reg
		#define SSI2_SRMSK	(SSI2_BASE_ADDR+0x4C)	//  32bit ssi2 rx time slot mask reg
		
		#endif

		
//#########################################		
//# I2C                                   #		
//# $1001_2000 to $1001_2FFF              #		
//#########################################		
#define I2C_BASE_ADDR	0x10012000	
#define _reg_I2C_IADR				(*((volatile unsigned long *)(MX2_IO_ADDRESS(I2C_BASE_ADDR+0x00))))	//  16bit i2c address reg
#define _reg_I2C_IFDR				(*((volatile unsigned long *)(MX2_IO_ADDRESS(I2C_BASE_ADDR+0x04))))	//  16bit i2c frequency divider reg
#define _reg_I2C_I2CR				(*((volatile unsigned long *)(MX2_IO_ADDRESS(I2C_BASE_ADDR+0x08))))	//  16bit i2c control reg
#define _reg_I2C_I2SR				(*((volatile unsigned long *)(MX2_IO_ADDRESS(I2C_BASE_ADDR+0x0C))))	//  16bit i2c status reg
#define _reg_I2C_I2DR				(*((volatile unsigned long *)(MX2_IO_ADDRESS(I2C_BASE_ADDR+0x10))))	//  16bit i2c data i/o reg
		
		
#define SDHC_BASE_ADDR(x)			(0x10013000+0x1000*(x-1))	
#define _reg_SDHC_STR_STP_CLK(x)		(*((volatile unsigned long *)(MX2_IO_ADDRESS(SDHC_BASE_ADDR(x)+0x00))))	//  32bit sdhc1 control reg
#define _reg_SDHC_STATUS(x)			(*((volatile unsigned long *)(MX2_IO_ADDRESS(SDHC_BASE_ADDR(x)+0x04))))	//  32bit sdhc1 status reg
#define _reg_SDHC_CLK_RATE(x)			(*((volatile unsigned long *)(MX2_IO_ADDRESS(SDHC_BASE_ADDR(x)+0x08))))	//  32bit sdhc1 clock rate reg
#define _reg_SDHC_CMD_DAT_CONT(x)		(*((volatile unsigned long *)(MX2_IO_ADDRESS(SDHC_BASE_ADDR(x)+0x0C))))	//  32bit sdhc1 cmd/data control reg
#define _reg_SDHC_RESPONSE_TO(x)		(*((volatile unsigned long *)(MX2_IO_ADDRESS(SDHC_BASE_ADDR(x)+0x10))))	//  32bit sdhc1 response time out reg
#define _reg_SDHC_READ_TO(x)			(*((volatile unsigned long *)(MX2_IO_ADDRESS(SDHC_BASE_ADDR(x)+0x14))))	//  32bit sdhc1 read time out reg
#define _reg_SDHC_BLK_LEN(x)			(*((volatile unsigned long *)(MX2_IO_ADDRESS(SDHC_BASE_ADDR(x)+0x18))))	//  32bit sdhc1 block length reg
#define _reg_SDHC_NOB(x)			(*((volatile unsigned long *)(MX2_IO_ADDRESS(SDHC_BASE_ADDR(x)+0x1C))))	//  32bit sdhc1 number of blocks reg
#define _reg_SDHC_REV_NO(x)			(*((volatile unsigned long *)(MX2_IO_ADDRESS(SDHC_BASE_ADDR(x)+0x20))))	//  32bit sdhc1 revision number reg
#define _reg_SDHC_INT_MASK(x)			(*((volatile unsigned long *)(MX2_IO_ADDRESS(SDHC_BASE_ADDR(x)+0x24))))	//  32bit sdhc1 interrupt mask reg
#define _reg_SDHC_CMD(x)			(*((volatile unsigned long *)(MX2_IO_ADDRESS(SDHC_BASE_ADDR(x)+0x28))))	//  32bit sdhc1 command code reg
#define _reg_SDHC_ARGH(x)			(*((volatile unsigned long *)(MX2_IO_ADDRESS(SDHC_BASE_ADDR(x)+0x2C))))	//  32bit sdhc1 argument high reg
#define _reg_SDHC_ARGL(x)			(*((volatile unsigned long *)(MX2_IO_ADDRESS(SDHC_BASE_ADDR(x)+0x30))))	//  32bit sdhc1 argument low reg
#define _reg_SDHC_RES_FIFO(x)			(*((volatile unsigned long *)(MX2_IO_ADDRESS(SDHC_BASE_ADDR(x)+0x34))))	//  32bit sdhc1 response fifo reg
#define _reg_SDHC_BUFFER_ACCESS(x)		(*((volatile unsigned long *)(MX2_IO_ADDRESS(SDHC_BASE_ADDR(x)+0x38))))	//  32bit sdhc1 buffer access reg		

		#if 0		
		//#########################################		
		//# SDHC1                                 #		
		//# $1001_3000 to $1001_3FFF              #		
		//#########################################		
		#define SDHC1_BASE_ADDR	0x10013000	
		#define SDHC1_STR_STP_CLK	(SDHC1_BASE_ADDR+0x00)	//  32bit sdhc1 control reg
		#define SDHC1_STATUS	(SDHC1_BASE_ADDR+0x04)	//  32bit sdhc1 status reg
		#define SDHC1_CLK_RATE	(SDHC1_BASE_ADDR+0x08)	//  32bit sdhc1 clock rate reg
		#define SDHC1_CMD_DAT_CONT	(SDHC1_BASE_ADDR+0x0C)	//  32bit sdhc1 cmd/data control reg
		#define SDHC1_RESPONSE_TO	(SDHC1_BASE_ADDR+0x10)	//  32bit sdhc1 response time out reg
		#define SDHC1_READ_TO	(SDHC1_BASE_ADDR+0x14)	//  32bit sdhc1 read time out reg
		#define SDHC1_BLK_LEN	(SDHC1_BASE_ADDR+0x18)	//  32bit sdhc1 block length reg
		#define SDHC1_NOB	(SDHC1_BASE_ADDR+0x1C)	//  32bit sdhc1 number of blocks reg
		#define SDHC1_REV_NO	(SDHC1_BASE_ADDR+0x20)	//  32bit sdhc1 revision number reg
		#define SDHC1_INT_MASK	(SDHC1_BASE_ADDR+0x24)	//  32bit sdhc1 interrupt mask reg
		#define SDHC1_CMD	(SDHC1_BASE_ADDR+0x28)	//  32bit sdhc1 command code reg
		#define SDHC1_ARGH	(SDHC1_BASE_ADDR+0x2C)	//  32bit sdhc1 argument high reg
		#define SDHC1_ARGL	(SDHC1_BASE_ADDR+0x30)	//  32bit sdhc1 argument low reg
		#define SDHC1_RES_FIFO	(SDHC1_BASE_ADDR+0x34)	//  32bit sdhc1 response fifo reg
		#define SDHC1_BUFFER_ACCESS	(SDHC1_BASE_ADDR+0x38)	//  32bit sdhc1 buffer access reg
				
		//#########################################		
		//# SDHC2                                 #		
		//# $1001_4000 to $1001_4FFF              #		
		//#########################################		
		#define SDHC2_BASE_ADDR	0x10014000	
		#define SDHC2_STR_STP_CLK	(SDHC2_BASE_ADDR+0x00)	//  32bit sdhc2 control reg 
		#define SDHC2_STATUS	(SDHC2_BASE_ADDR+0x04)	//  32bit sdhc2 status reg
		#define SDHC2_CLK_RATE	(SDHC2_BASE_ADDR+0x08)	//  32bit sdhc2 clock rate reg
		#define SDHC2_CMD_DAT_CONT	(SDHC2_BASE_ADDR+0x0C)	//  32bit sdhc2 cmd/data control reg
		#define SDHC2_RESPONSE_TO	(SDHC2_BASE_ADDR+0x10)	//  32bit sdhc2 response time out reg
		#define SDHC2_READ_TO	(SDHC2_BASE_ADDR+0x14)	//  32bit sdhc2 read time out reg
		#define SDHC2_BLK_LEN	(SDHC2_BASE_ADDR+0x18)	//  32bit sdhc2 block length reg
		#define SDHC2_NOB	(SDHC2_BASE_ADDR+0x1C)	//  32bit sdhc2 number of blocks reg
		#define SDHC2_REV_NO	(SDHC2_BASE_ADDR+0x20)	//  32bit sdhc2 revision number reg
		#define SDHC2_INT_MASK	(SDHC2_BASE_ADDR+0x24)	//  32bit sdhc2 interrupt mask reg
		#define SDHC2_CMD	(SDHC2_BASE_ADDR+0x28)	//  32bit sdhc2 command code reg
		#define SDHC2_ARGH	(SDHC2_BASE_ADDR+0x2C)	//  32bit sdhc2 argument high reg
		#define SDHC2_ARGL	(SDHC2_BASE_ADDR+0x30)	//  32bit sdhc2 argument low reg  
		#define SDHC2_RES_FIFO	(SDHC2_BASE_ADDR+0x34)	//  32bit sdhc2 response fifo reg
		#define SDHC2_BUFFER_ACCESS	(SDHC2_BASE_ADDR+0x38)	//  32bit sdhc2 buffer access reg
		#endif		
//#########################################		
//# GPIO                                  #		
//# $1001_5000 to $1001_5FFF              #		
//#########################################

#define GPIOA	0
#define GPIOB	1
#define GPIOC	2
#define GPIOD	3
#define GPIOE	4
#define GPIOF	5

/* Use as GPIO_BASE_ADDR(GPIOA)- GPIO_BASE_ADDR(GPIOF)*/
#define GPIO_BASE_ADDR(x)	(0x10015000+x*0x100)
	
#define _reg_GPIO_DDIR(x)	(*((volatile unsigned long *)(MX2_IO_ADDRESS(GPIO_BASE_ADDR(x)+0x00))))  //  32bit gpio pta data direction reg
#define _reg_GPIO_OCR1(x)	(*((volatile unsigned long *)(MX2_IO_ADDRESS(GPIO_BASE_ADDR(x)+0x04))))  //  32bit gpio pta output config 1 reg
#define _reg_GPIO_OCR2(x)	(*((volatile unsigned long *)(MX2_IO_ADDRESS(GPIO_BASE_ADDR(x)+0x08))))  //  32bit gpio pta output config 2 reg
#define _reg_GPIO_ICONFA1(x)	(*((volatile unsigned long *)(MX2_IO_ADDRESS(GPIO_BASE_ADDR(x)+0x0C))))  //  32bit gpio pta input config A1 reg
#define _reg_GPIO_ICONFA2(x)	(*((volatile unsigned long *)(MX2_IO_ADDRESS(GPIO_BASE_ADDR(x)+0x10))))  //  32bit gpio pta input config A2 reg
#define _reg_GPIO_ICONFB1(x)	(*((volatile unsigned long *)(MX2_IO_ADDRESS(GPIO_BASE_ADDR(x)+0x14))))  //  32bit gpio pta input config B1 reg
#define _reg_GPIO_ICONFB2(x)	(*((volatile unsigned long *)(MX2_IO_ADDRESS(GPIO_BASE_ADDR(x)+0x18))))  //  32bit gpio pta input config B2 reg
#define _reg_GPIO_DR(x)		(*((volatile unsigned long *)(MX2_IO_ADDRESS(GPIO_BASE_ADDR(x)+0x1C))))  //  32bit gpio pta data reg
#define _reg_GPIO_GIUS(x)	(*((volatile unsigned long *)(MX2_IO_ADDRESS(GPIO_BASE_ADDR(x)+0x20))))  //  32bit gpio pta in use reg
#define _reg_GPIO_SSR(x)	(*((volatile unsigned long *)(MX2_IO_ADDRESS(GPIO_BASE_ADDR(x)+0x24))))  //  32bit gpio pta sample status reg
#define _reg_GPIO_ICR1(x)	(*((volatile unsigned long *)(MX2_IO_ADDRESS(GPIO_BASE_ADDR(x)+0x28))))  //  32bit gpio pta interrupt ctrl 1 reg
#define _reg_GPIO_ICR2(x)	(*((volatile unsigned long *)(MX2_IO_ADDRESS(GPIO_BASE_ADDR(x)+0x2C))))  //  32bit gpio pta interrupt ctrl 2 reg
#define _reg_GPIO_IMR(x)	(*((volatile unsigned long *)(MX2_IO_ADDRESS(GPIO_BASE_ADDR(x)+0x30))))  //  32bit gpio pta interrupt mask reg
#define _reg_GPIO_ISR(x)	(*((volatile unsigned long *)(MX2_IO_ADDRESS(GPIO_BASE_ADDR(x)+0x34))))  //  32bit gpio pta interrupt status reg
#define _reg_GPIO_GPR(x)	(*((volatile unsigned long *)(MX2_IO_ADDRESS(GPIO_BASE_ADDR(x)+0x38))))  //  32bit gpio pta general purpose reg
#define _reg_GPIO_SWR(x)	(*((volatile unsigned long *)(MX2_IO_ADDRESS(GPIO_BASE_ADDR(x)+0x3C))))  //  32bit gpio pta software reset reg
#define _reg_GPIO_PUEN(x)	(*((volatile unsigned long *)(MX2_IO_ADDRESS(GPIO_BASE_ADDR(x)+0x40))))  //  32bit gpio pta pull up enable reg

		
#define GPIO_REG_BASE	0x10015600	
#define _reg_GPIO_PMASK		(*((volatile unsigned long *)(MX2_IO_ADDRESS(GPIO_REG_BASE+0x00))))  //  32bit gpio interrupt mask reg
		
//#########################################		
//# AUDMUX                                #		
//# $1001_6000 to $1001_6FFF              #		
//#########################################		
#define AUDMUX_BASE_ADDR	0x10016000	
#define _reg_AUDMUX_HPCR1	(*((volatile unsigned long *)(MX2_IO_ADDRESS(AUDMUX_BASE_ADDR+0x00))))  //  32bit audmux host config reg 1
#define _reg_AUDMUX_HPCR2	(*((volatile unsigned long *)(MX2_IO_ADDRESS(AUDMUX_BASE_ADDR+0x04))))  //  32bit audmux host config reg 2
#define _reg_AUDMUX_HPCR3	(*((volatile unsigned long *)(MX2_IO_ADDRESS(AUDMUX_BASE_ADDR+0x08))))  //  32bit audmux host config reg 3
#define _reg_AUDMUX_PPCR1	(*((volatile unsigned long *)(MX2_IO_ADDRESS(AUDMUX_BASE_ADDR+0x10))))  //  32bit audmux pripheral config 1
#define _reg_AUDMUX_PPCR2	(*((volatile unsigned long *)(MX2_IO_ADDRESS(AUDMUX_BASE_ADDR+0x14))))  //  32bit audmux pripheral config 2
#define _reg_AUDMUX_PPCR3	(*((volatile unsigned long *)(MX2_IO_ADDRESS(AUDMUX_BASE_ADDR+0x1C))))  //  32bit audmux pripheral config 3
		
		
		
//#########################################		
//# AIPI2                                 #		
//# $1002_0000 to $1002_0FFF              #		
//#########################################		
#define AIPI2_BASE_ADDR	0x10020000	
#define _reg_AIPI2_PSR0		(*((volatile unsigned long *)(MX2_IO_ADDRESS(AIPI2_BASE_ADDR+0x00))))  //  32bit Peripheral Size Reg 0
#define _reg_AIPI2_PSR1		(*((volatile unsigned long *)(MX2_IO_ADDRESS(AIPI2_BASE_ADDR+0x04))))  //  32bit Peripheral Size Reg 1
#define _reg_AIPI2_PAR		(*((volatile unsigned long *)(MX2_IO_ADDRESS(AIPI2_BASE_ADDR+0x08))))  //  32bit Peripheral Access Reg
		
//#########################################		
//# LCDC                                  #		
//# $1002_1000 to $1002_1FFF              #		
//#########################################		
#define LCDC_BASE_ADDR	0x10021000	
#define _reg_LCDC_LSSAR		(*((volatile unsigned long *)(MX2_IO_ADDRESS(LCDC_BASE_ADDR+0x00))))  //  32bit lcdc screen start addr reg
#define _reg_LCDC_LSR		(*((volatile unsigned long *)(MX2_IO_ADDRESS(LCDC_BASE_ADDR+0x04))))  //  32bit lcdc size reg
#define _reg_LCDC_LVPWR		(*((volatile unsigned long *)(MX2_IO_ADDRESS(LCDC_BASE_ADDR+0x08))))  //  32bit lcdc virtual page width reg
#define _reg_LCDC_LCPR		(*((volatile unsigned long *)(MX2_IO_ADDRESS(LCDC_BASE_ADDR+0x0C))))  //  32bit lcd cursor position reg
#define _reg_LCDC_LCWHBR	(*((volatile unsigned long *)(MX2_IO_ADDRESS(LCDC_BASE_ADDR+0x10))))  //  32bit lcd cursor width/heigh/blink
#define _reg_LCDC_LCCMR		(*((volatile unsigned long *)(MX2_IO_ADDRESS(LCDC_BASE_ADDR+0x14))))  //  32bit lcd color cursor mapping reg
#define _reg_LCDC_LPCR		(*((volatile unsigned long *)(MX2_IO_ADDRESS(LCDC_BASE_ADDR+0x18))))  //  32bit lcdc panel config reg
#define _reg_LCDC_LHCR		(*((volatile unsigned long *)(MX2_IO_ADDRESS(LCDC_BASE_ADDR+0x1C))))  //  32bit lcdc horizontal config reg
#define _reg_LCDC_LVCR		(*((volatile unsigned long *)(MX2_IO_ADDRESS(LCDC_BASE_ADDR+0x20))))  //  32bit lcdc vertical config reg
#define _reg_LCDC_LPOR		(*((volatile unsigned long *)(MX2_IO_ADDRESS(LCDC_BASE_ADDR+0x24))))  //  32bit lcdc panning offset reg
#define _reg_LCDC_LSCR		(*((volatile unsigned long *)(MX2_IO_ADDRESS(LCDC_BASE_ADDR+0x28))))  //  32bit lcdc sharp config 1 reg
#define _reg_LCDC_LPCCR		(*((volatile unsigned long *)(MX2_IO_ADDRESS(LCDC_BASE_ADDR+0x2C))))  //  32bit lcdc pwm contrast ctrl reg
#define _reg_LCDC_LDCR		(*((volatile unsigned long *)(MX2_IO_ADDRESS(LCDC_BASE_ADDR+0x30))))  //  32bit lcdc dma control reg
#define _reg_LCDC_LRMCR		(*((volatile unsigned long *)(MX2_IO_ADDRESS(LCDC_BASE_ADDR+0x34))))  //  32bit lcdc refresh mode ctrl reg
#define _reg_LCDC_LICR		(*((volatile unsigned long *)(MX2_IO_ADDRESS(LCDC_BASE_ADDR+0x38))))  //  32bit lcdc interrupt config reg
#define _reg_LCDC_LIER		(*((volatile unsigned long *)(MX2_IO_ADDRESS(LCDC_BASE_ADDR+0x3C))))  //  32bit lcdc interrupt enable reg
#define _reg_LCDC_LISR		(*((volatile unsigned long *)(MX2_IO_ADDRESS(LCDC_BASE_ADDR+0x40))))  //  32bit lcdc interrupt status reg
#define _reg_LCDC_LGWSAR	(*((volatile unsigned long *)(MX2_IO_ADDRESS(LCDC_BASE_ADDR+0x50))))  //  32bit lcdc graphic win start add
#define _reg_LCDC_LGWSR		(*((volatile unsigned long *)(MX2_IO_ADDRESS(LCDC_BASE_ADDR+0x54))))  //  32bit lcdc graphic win size reg
#define _reg_LCDC_LGWVPWR	(*((volatile unsigned long *)(MX2_IO_ADDRESS(LCDC_BASE_ADDR+0x58))))  //  32bit lcdc graphic win virtual pg
#define _reg_LCDC_LGWPOR	(*((volatile unsigned long *)(MX2_IO_ADDRESS(LCDC_BASE_ADDR+0x5C))))  //  32bit lcdc graphic win pan offset
#define _reg_LCDC_LGWPR		(*((volatile unsigned long *)(MX2_IO_ADDRESS(LCDC_BASE_ADDR+0x60))))  //  32bit lcdc graphic win positon reg
#define _reg_LCDC_LGWCR		(*((volatile unsigned long *)(MX2_IO_ADDRESS(LCDC_BASE_ADDR+0x64))))  //  32bit lcdc graphic win control reg
#define _reg_LCDC_LGWDCR	(*((volatile unsigned long *)(MX2_IO_ADDRESS(LCDC_BASE_ADDR+0x68))))  //  32bit lcdc graphic win DMA control reg
		
#define _reg_LCDC_BPLUT_BASE(regno)	(*((volatile unsigned long *)(MX2_IO_ADDRESS(LCDC_BASE_ADDR+0x800+4*(regno)))))  //  Background Plane LUT (800 - BFC)
#define _reg_LCDC_GWLUT_BASE(regno)	(*((volatile unsigned long *)(MX2_IO_ADDRESS(LCDC_BASE_ADDR+0xC00+4*(regno)))))  //  Background Plane LUT (C00 - FFC)
		
//#########################################		
//# SLCDC                                 #		
//# $1002_2000 to $1002_2FFF              #		
//#########################################		
#define SLCDC_BASE_ADDR	0x10022000	
#define _reg_SLCDC_DBADDR	(*((volatile unsigned long *)(MX2_IO_ADDRESS(SLCDC_BASE_ADDR+0x00))))  //  32bit slcdc data base addr
#define _reg_SLCDC_DBUF_SIZE	(*((volatile unsigned long *)(MX2_IO_ADDRESS(SLCDC_BASE_ADDR+0x04))))  //  32bit slcdc data buffer size high
#define _reg_SLCDC_CBADDR	(*((volatile unsigned long *)(MX2_IO_ADDRESS(SLCDC_BASE_ADDR+0x08))))  //  32bit slcdc cmd base addr high
#define _reg_SLCDC_CBUF_SIZE	(*((volatile unsigned long *)(MX2_IO_ADDRESS(SLCDC_BASE_ADDR+0x0C))))  //  32bit slcdc cmd buffer size high
#define _reg_SLCDC_CBUF_SSIZE	(*((volatile unsigned long *)(MX2_IO_ADDRESS(SLCDC_BASE_ADDR+0x10))))  //  32bit slcdc cmd string size
#define _reg_SLCDC_FIFO_CONFIG	(*((volatile unsigned long *)(MX2_IO_ADDRESS(SLCDC_BASE_ADDR+0x14))))  //  32bit slcdc fifo config reg
#define _reg_SLCDC_LCD_CONFIG	(*((volatile unsigned long *)(MX2_IO_ADDRESS(SLCDC_BASE_ADDR+0x18))))  //  32bit slcdc lcd controller config
#define _reg_SLCDC_LCD_TXCONFIG	(*((volatile unsigned long *)(MX2_IO_ADDRESS(SLCDC_BASE_ADDR+0x1C))))  //  32bit slcdc lcd transmit config reg
#define _reg_SLCDC_LCD_CTRL_STAT	(*((volatile unsigned long *)(MX2_IO_ADDRESS(SLCDC_BASE_ADDR+0x20))))  //  32bit slcdc lcd control/status reg
#define _reg_SLCDC_LCD_CLKCONFIG	(*((volatile unsigned long *)(MX2_IO_ADDRESS(SLCDC_BASE_ADDR+0x24))))  //  32bit slcdc lcd clock config reg
#define _reg_SLCDC_LCD_WR_DATA	(*((volatile unsigned long *)(MX2_IO_ADDRESS(SLCDC_BASE_ADDR+0x28))))  //  32bit slcdc lcd write data reg
		
		
		
		
		
//#########################################		
//# SAHARA                                #		
//# $1002_3000 to $1002_3FFF              #		
//#########################################		
#define SAHARA_BASE_ADDR	0x10023000	
		
//# CHA BASE ADDRESSES		
#define SAHARA_TOP	(SAHARA_BASE_ADDR+0x0000)
#define SAHARA_AESA	(SAHARA_BASE_ADDR+0x0100)	
#define SAHARA_AESA	(SAHARA_BASE_ADDR+0x0100)	
#define SAHARA_DESA	(SAHARA_BASE_ADDR+0x0100)	
#define SAHARA_MDHA	(SAHARA_BASE_ADDR+0x0200)	
#define SAHARA_RNGA	(SAHARA_BASE_ADDR+0x0300)	
#define SAHARA_FIDO	(SAHARA_BASE_ADDR+0x0400)	
#define SAHARA_I_FIDO	(SAHARA_BASE_ADDR+0x0400)	
#define SAHARA_O_FIDO	(SAHARA_BASE_ADDR+0x0500)	
#define SAHARA_PKHA	(SAHARA_BASE_ADDR+0x0800)	
		
//# SAHARA REGISTERS		
#define _reg_SAHARA_VER_ID	(*((volatile unsigned long *)(MX2_IO_ADDRESS(SAHARA_TOP+0x00))))	
#define _reg_SAHARA_DSC_ADR	(*((volatile unsigned long *)(MX2_IO_ADDRESS(SAHARA_TOP+0x04))))	
#define _reg_SAHARA_CONTROL	(*((volatile unsigned long *)(MX2_IO_ADDRESS(SAHARA_TOP+0x08))))	
#define _reg_SAHARA_COMMAND	(*((volatile unsigned long *)(MX2_IO_ADDRESS(SAHARA_TOP+0x0C))))	
#define _reg_SAHARA_STAT	(*((volatile unsigned long *)(MX2_IO_ADDRESS(SAHARA_TOP+0x10))))	
#define _reg_SAHARA_ERR_STAT	(*((volatile unsigned long *)(MX2_IO_ADDRESS(SAHARA_TOP+0x14))))	
#define _reg_SAHARA_FAULT_ADR	(*((volatile unsigned long *)(MX2_IO_ADDRESS(SAHARA_TOP+0x18))))	
#define _reg_SAHARA_C_DSC_ADR	(*((volatile unsigned long *)(MX2_IO_ADDRESS(SAHARA_TOP+0x1C))))	
#define _reg_SAHARA_I_DSC_ADR	(*((volatile unsigned long *)(MX2_IO_ADDRESS(SAHARA_TOP+0x20))))	
#define _reg_SAHARA_BUFF_LVL	(*((volatile unsigned long *)(MX2_IO_ADDRESS(SAHARA_TOP+0x24))))	
#define _reg_SAHARA_DSC_A	(*((volatile unsigned long *)(MX2_IO_ADDRESS(SAHARA_TOP+0x80))))	
#define _reg_SAHARA_DSC_B	(*((volatile unsigned long *)(MX2_IO_ADDRESS(SAHARA_TOP+0x84))))	
#define _reg_SAHARA_DSC_C	(*((volatile unsigned long *)(MX2_IO_ADDRESS(SAHARA_TOP+0x88))))	
#define _reg_SAHARA_DSC_D	(*((volatile unsigned long *)(MX2_IO_ADDRESS(SAHARA_TOP+0x8C))))	
#define _reg_SAHARA_DSC_E	(*((volatile unsigned long *)(MX2_IO_ADDRESS(SAHARA_TOP+0x90))))	
#define _reg_SAHARA_DSC_F	(*((volatile unsigned long *)(MX2_IO_ADDRESS(SAHARA_TOP+0x94))))	
#define _reg_SAHARA_LNK_1_A	(*((volatile unsigned long *)(MX2_IO_ADDRESS(SAHARA_TOP+0xA0))))	
#define _reg_SAHARA_LNK_1_B	(*((volatile unsigned long *)(MX2_IO_ADDRESS(SAHARA_TOP+0xA4))))	
#define _reg_SAHARA_LNK_1_C	(*((volatile unsigned long *)(MX2_IO_ADDRESS(SAHARA_TOP+0xA8))))	
#define _reg_SAHARA_LNK_2_A	(*((volatile unsigned long *)(MX2_IO_ADDRESS(SAHARA_TOP+0xB0))))	
#define _reg_SAHARA_LNK_2_B	(*((volatile unsigned long *)(MX2_IO_ADDRESS(SAHARA_TOP+0xB4))))	
#define _reg_SAHARA_LNK_2_C	(*((volatile unsigned long *)(MX2_IO_ADDRESS(SAHARA_TOP+0xB8))))	
#define _reg_SAHARA_FLOW_CTRL	(*((volatile unsigned long *)(MX2_IO_ADDRESS(SAHARA_TOP+0xC0))))	

//# COMMON CHA REGISTERS		
#define SAHARA_MODE	0x00	
#define SAHARA_KEY_SIZE	0x04	
#define SAHARA_DATA_SIZE	0x08	
#define SAHARA_STATUS	0x0C	
#define SAHARA_ERROR_STATUS	0x10	
#define SAHARA_CHA_GO	0x14	
#define SAHARA_CONTEXT	0x40	
#define SAHARA_KEY	0x80	

//# SAHARA_AESA REGISTERS		
#define _reg_SAHARA_AESA_MODE		(*((volatile unsigned long *)(MX2_IO_ADDRESS(SAHARA_AESA+SAHARA_MODE+0x00))))	
#define _reg_SAHARA_AESA_KEY_SIZE	(*((volatile unsigned long *)(MX2_IO_ADDRESS(SAHARA_AESA+SAHARA_KEY_SIZE+0x00))))	
#define _reg_SAHARA_AESA_DATA_SIZE	(*((volatile unsigned long *)(MX2_IO_ADDRESS(SAHARA_AESA+SAHARA_DATA_SIZE+0x00))))	
#define _reg_SAHARA_AESA_STAT		(*((volatile unsigned long *)(MX2_IO_ADDRESS(SAHARA_AESA+SAHARA_STATUS+0x00))))	
#define _reg_SAHARA_AESA_ERR_STAT	(*((volatile unsigned long *)(MX2_IO_ADDRESS(SAHARA_AESA+SAHARA_ERROR_STATUS+0x00))))	
#define _reg_SAHARA_AESA_CHA_GO		(*((volatile unsigned long *)(MX2_IO_ADDRESS(SAHARA_AESA+SAHARA_CHA_GO+0x00))))	
#define _reg_SAHARA_AESA_CXT		(*((volatile unsigned long *)(MX2_IO_ADDRESS(SAHARA_AESA+SAHARA_CONTEXT+0x00))))	
#define _reg_SAHARA_AESA_KEY_1		(*((volatile unsigned long *)(MX2_IO_ADDRESS(SAHARA_AESA+SAHARA_KEY+0x00))))	
#define _reg_SAHARA_AESA_KEY_2		(*((volatile unsigned long *)(MX2_IO_ADDRESS(SAHARA_AESA+SAHARA_KEY+0x04))))	
#define _reg_SAHARA_AESA_KEY_3		(*((volatile unsigned long *)(MX2_IO_ADDRESS(SAHARA_AESA+SAHARA_KEY+0x08))))	
#define _reg_SAHARA_AESA_KEY_4		(*((volatile unsigned long *)(MX2_IO_ADDRESS(SAHARA_AESA+SAHARA_KEY+0x0C))))	
#define _reg_SAHARA_AESA_IV_1		(*((volatile unsigned long *)(MX2_IO_ADDRESS(SAHARA_AESA+SAHARA_CONTEXT+0x00))))	
#define _reg_SAHARA_AESA_IV_2		(*((volatile unsigned long *)(MX2_IO_ADDRESS(SAHARA_AESA+SAHARA_CONTEXT+0x04))))	
#define _reg_SAHARA_AESA_IV_3		(*((volatile unsigned long *)(MX2_IO_ADDRESS(SAHARA_AESA+SAHARA_CONTEXT+0x08))))	
#define _reg_SAHARA_AESA_IV_4		(*((volatile unsigned long *)(MX2_IO_ADDRESS(SAHARA_AESA+SAHARA_CONTEXT+0x0C))))	
#define _reg_SAHARA_AESA_IV_5		(*((volatile unsigned long *)(MX2_IO_ADDRESS(SAHARA_AESA+SAHARA_CONTEXT+0x10))))	
#define _reg_SAHARA_AESA_IV_6		(*((volatile unsigned long *)(MX2_IO_ADDRESS(SAHARA_AESA+SAHARA_CONTEXT+0x14))))	
#define _reg_SAHARA_AESA_IV_7		(*((volatile unsigned long *)(MX2_IO_ADDRESS(SAHARA_AESA+SAHARA_CONTEXT+0x18))))	
#define _reg_SAHARA_AESA_IV_8		(*((volatile unsigned long *)(MX2_IO_ADDRESS(SAHARA_AESA+SAHARA_CONTEXT+0x1C))))	
#define _reg_SAHARA_AESA_IV_9		(*((volatile unsigned long *)(MX2_IO_ADDRESS(SAHARA_AESA+SAHARA_CONTEXT+0x20))))	
#define _reg_SAHARA_AESA_IV_10		(*((volatile unsigned long *)(MX2_IO_ADDRESS(SAHARA_AESA+SAHARA_CONTEXT+0x24))))	
#define _reg_SAHARA_AESA_IV_11		(*((volatile unsigned long *)(MX2_IO_ADDRESS(SAHARA_AESA+SAHARA_CONTEXT+0x28))))	
#define _reg_SAHARA_AESA_IV_12		(*((volatile unsigned long *)(MX2_IO_ADDRESS(SAHARA_AESA+SAHARA_CONTEXT+0x2C))))	
#define _reg_SAHARA_AESA_IV_13		(*((volatile unsigned long *)(MX2_IO_ADDRESS(SAHARA_AESA+SAHARA_CONTEXT+0x30))))	
#define _reg_SAHARA_AESA_IV_14		(*((volatile unsigned long *)(MX2_IO_ADDRESS(SAHARA_AESA+SAHARA_CONTEXT+0x34))))	
#define _reg_SAHARA_AESA_IV_15		(*((volatile unsigned long *)(MX2_IO_ADDRESS(SAHARA_AESA+SAHARA_CONTEXT+0x38))))	
#define _reg_SAHARA_AESA_IV_16		(*((volatile unsigned long *)(MX2_IO_ADDRESS(SAHARA_AESA+SAHARA_CONTEXT+0x3C))))	
		
//# SAHARA_DESA REGISTERS		
#define _reg_SAHARA_DESA_MODE			(*((volatile unsigned long *)(MX2_IO_ADDRESS(SAHARA_DESA+SAHARA_MODE+0x00))))	
#define _reg_SAHARA_DESA_KEY_SIZE		(*((volatile unsigned long *)(MX2_IO_ADDRESS(SAHARA_DESA+SAHARA_KEY_SIZE+0x00))))	
#define _reg_SAHARA_DESA_DATA_SIZE		(*((volatile unsigned long *)(MX2_IO_ADDRESS(SAHARA_DESA+SAHARA_DATA_SIZE+0x00))))	
#define _reg_SAHARA_DESA_STAT			(*((volatile unsigned long *)(MX2_IO_ADDRESS(SAHARA_DESA+SAHARA_STATUS+0x00))))	
#define _reg_SAHARA_DESA_ERR_STAT		(*((volatile unsigned long *)(MX2_IO_ADDRESS(SAHARA_DESA+SAHARA_ERROR_STATUS+0x00))))	
#define _reg_SAHARA_DESA_CHA_GO			(*((volatile unsigned long *)(MX2_IO_ADDRESS(SAHARA_DESA+SAHARA_CHA_GO+0x00))))	
#define _reg_SAHARA_DESA_KEY			(*((volatile unsigned long *)(MX2_IO_ADDRESS(SAHARA_DESA+SAHARA_KEY+0x00))))	
#define _reg_SAHARA_DESA_CXT			(*((volatile unsigned long *)(MX2_IO_ADDRESS(SAHARA_DESA+SAHARA_CONTEXT+0x00))))	
#define _reg_SAHARA_DESA_KEY_1			(*((volatile unsigned long *)(MX2_IO_ADDRESS(SAHARA_DESA+SAHARA_KEY+0x00))))	
#define _reg_SAHARA_DESA_KEY_2			(*((volatile unsigned long *)(MX2_IO_ADDRESS(SAHARA_DESA+SAHARA_KEY+0x04))))	
#define _reg_SAHARA_DESA_KEY_3			(*((volatile unsigned long *)(MX2_IO_ADDRESS(SAHARA_DESA+SAHARA_KEY+0x08))))	
#define _reg_SAHARA_DESA_KEY_4			(*((volatile unsigned long *)(MX2_IO_ADDRESS(SAHARA_DESA+SAHARA_KEY+0x0C))))	
#define _reg_SAHARA_DESA_KEY_5			(*((volatile unsigned long *)(MX2_IO_ADDRESS(SAHARA_DESA+SAHARA_KEY+0x10))))	
#define _reg_SAHARA_DESA_KEY_6			(*((volatile unsigned long *)(MX2_IO_ADDRESS(SAHARA_DESA+SAHARA_KEY+0x14))))	
#define _reg_SAHARA_DESA_IV_1			(*((volatile unsigned long *)(MX2_IO_ADDRESS(SAHARA_DESA+SAHARA_CONTEXT+0x00))))	
#define _reg_SAHARA_DESA_IV_2			(*((volatile unsigned long *)(MX2_IO_ADDRESS(SAHARA_DESA+SAHARA_CONTEXT+0x04))))	
		
//# SAHARA_MDHA REGISTERS		
#define _reg_SAHARA_MDHA_MODE			(*((volatile unsigned long *)(MX2_IO_ADDRESS(SAHARA_MDHA+SAHARA_MODE+0x00))))	
#define _reg_SAHARA_MDHA_KEY_SIZE		(*((volatile unsigned long *)(MX2_IO_ADDRESS(SAHARA_MDHA+SAHARA_KEY_SIZE+0x00))))	
#define _reg_SAHARA_MDHA_DATA_SIZE		(*((volatile unsigned long *)(MX2_IO_ADDRESS(SAHARA_MDHA+SAHARA_DATA_SIZE+0x00))))	
#define _reg_SAHARA_MDHA_STAT			(*((volatile unsigned long *)(MX2_IO_ADDRESS(SAHARA_MDHA+SAHARA_STATUS+0x00))))	
#define _reg_SAHARA_MDHA_ERR_STAT		(*((volatile unsigned long *)(MX2_IO_ADDRESS(SAHARA_MDHA+SAHARA_ERROR_STATUS+0x00))))	
#define _reg_SAHARA_MDHA_GO			(*((volatile unsigned long *)(MX2_IO_ADDRESS(SAHARA_MDHA+SAHARA_CHA_GO+0x00))))	
#define _reg_SAHARA_MDHA_KEY			(*((volatile unsigned long *)(MX2_IO_ADDRESS(SAHARA_MDHA+SAHARA_KEY+0x00))))	
#define _reg_SAHARA_MDHA_CXT			(*((volatile unsigned long *)(MX2_IO_ADDRESS(SAHARA_MDHA+SAHARA_CONTEXT+0x00))))	
#define _reg_SAHARA_MDHA_MD_A1			(*((volatile unsigned long *)(MX2_IO_ADDRESS(SAHARA_MDHA+SAHARA_KEY+0x00))))	
#define _reg_SAHARA_MDHA_MD_B1			(*((volatile unsigned long *)(MX2_IO_ADDRESS(SAHARA_MDHA+SAHARA_KEY+0x04))))	
#define _reg_SAHARA_MDHA_MD_C1			(*((volatile unsigned long *)(MX2_IO_ADDRESS(SAHARA_MDHA+SAHARA_KEY+0x08))))	
#define _reg_SAHARA_MDHA_MD_D1			(*((volatile unsigned long *)(MX2_IO_ADDRESS(SAHARA_MDHA+SAHARA_KEY+0x0C))))	
#define _reg_SAHARA_MDHA_MD_E1			(*((volatile unsigned long *)(MX2_IO_ADDRESS(SAHARA_MDHA+SAHARA_KEY+0x10))))	
#define _reg_SAHARA_MDHA_MD_A			(*((volatile unsigned long *)(MX2_IO_ADDRESS(SAHARA_MDHA+SAHARA_CONTEXT+0x00))))	
#define _reg_SAHARA_MDHA_MD_B			(*((volatile unsigned long *)(MX2_IO_ADDRESS(SAHARA_MDHA+SAHARA_CONTEXT+0x04))))	
#define _reg_SAHARA_MDHA_MD_C			(*((volatile unsigned long *)(MX2_IO_ADDRESS(SAHARA_MDHA+SAHARA_CONTEXT+0x08))))	
#define _reg_SAHARA_MDHA_MD_D			(*((volatile unsigned long *)(MX2_IO_ADDRESS(SAHARA_MDHA+SAHARA_CONTEXT+0x0C))))	
#define _reg_SAHARA_MDHA_MD_E			(*((volatile unsigned long *)(MX2_IO_ADDRESS(SAHARA_MDHA+SAHARA_CONTEXT+0x10))))	
#define _reg_SAHARA_MDHA_MD_CNT			(*((volatile unsigned long *)(MX2_IO_ADDRESS(SAHARA_MDHA+SAHARA_CONTEXT+0x14))))	
		
//# SAHARA_PKHA REGISTERS		
#define _reg_SAHARA_PKHA_PGM_COUNT			(*((volatile unsigned long *)(MX2_IO_ADDRESS(SAHARA_PKHA+SAHARA_MODE+0x000))))	
#define _reg_SAHARA_PKHA_KEY_SIZE			(*((volatile unsigned long *)(MX2_IO_ADDRESS(SAHARA_PKHA+SAHARA_KEY_SIZE+0x000))))	
#define _reg_SAHARA_PKHA_MOD_SIZE			(*((volatile unsigned long *)(MX2_IO_ADDRESS(SAHARA_PKHA+SAHARA_DATA_SIZE+0x000))))	
#define _reg_SAHARA_PKHA_STAT				(*((volatile unsigned long *)(MX2_IO_ADDRESS(SAHARA_PKHA+SAHARA_STATUS+0x000))))	
#define _reg_SAHARA_PKHA_ERR_STAT			(*((volatile unsigned long *)(MX2_IO_ADDRESS(SAHARA_PKHA+SAHARA_ERROR_STATUS+0x000))))	
#define _reg_SAHARA_PKHA_CHA_GO				(*((volatile unsigned long *)(MX2_IO_ADDRESS(SAHARA_PKHA+SAHARA_CHA_GO+0x000))))	
#define _reg_SAHARA_PKHA_A0_BASE			(*((volatile unsigned long *)(MX2_IO_ADDRESS(SAHARA_PKHA+0x400))))	
#define _reg_SAHARA_PKHA_A1_BASE			(*((volatile unsigned long *)(MX2_IO_ADDRESS(SAHARA_PKHA+0x440))))	
#define _reg_SAHARA_PKHA_A2_BASE			(*((volatile unsigned long *)(MX2_IO_ADDRESS(SAHARA_PKHA+0x480))))	
#define _reg_SAHARA_PKHA_A3_BASE			(*((volatile unsigned long *)(MX2_IO_ADDRESS(SAHARA_PKHA+0x4C0))))	
#define _reg_SAHARA_PKHA_B0_BASE			(*((volatile unsigned long *)(MX2_IO_ADDRESS(SAHARA_PKHA+0x500))))	
#define _reg_SAHARA_PKHA_B1_BASE			(*((volatile unsigned long *)(MX2_IO_ADDRESS(SAHARA_PKHA+0x540))))	
#define _reg_SAHARA_PKHA_B2_BASE			(*((volatile unsigned long *)(MX2_IO_ADDRESS(SAHARA_PKHA+0x580))))	
#define _reg_SAHARA_PKHA_B3_BASE			(*((volatile unsigned long *)(MX2_IO_ADDRESS(SAHARA_PKHA+0x5C0))))	
#define _reg_SAHARA_PKHA_N_BASE				(*((volatile unsigned long *)(MX2_IO_ADDRESS(SAHARA_PKHA+0x600))))	
#define _reg_SAHARA_PKHA_EXP_BASE			(*((volatile unsigned long *)(MX2_IO_ADDRESS(SAHARA_PKHA+0x700))))	
		
//# SAHARA_RNGA REGISTERS		
#define _reg_SAHARA_RNGA_MODE				(*((volatile unsigned long *)(MX2_IO_ADDRESS(SAHARA_RNGA+SAHARA_MODE+0x00))))	
#define _reg_SAHARA_RNGA_DATA_SIZE			(*((volatile unsigned long *)(MX2_IO_ADDRESS(SAHARA_RNGA+SAHARA_DATA_SIZE+0x00))))	
#define _reg_SAHARA_RNGA_STAT				(*((volatile unsigned long *)(MX2_IO_ADDRESS(SAHARA_RNGA+SAHARA_STATUS+0x00))))	
#define _reg_SAHARA_RNGA_ERR_STAT			(*((volatile unsigned long *)(MX2_IO_ADDRESS(SAHARA_RNGA+SAHARA_ERROR_STATUS+0x00))))	
#define _reg_SAHARA_RNGA_CHA_GO				(*((volatile unsigned long *)(MX2_IO_ADDRESS(SAHARA_RNGA+SAHARA_CHA_GO+0x00))))	
		
		
		
		
		
//#########################################		
//# USBOTG                                #		
//# $1002_4000 to $1002_5FFF              #		
//#########################################		
#define OTG_BASE_ADDR	0x10024000	
#define OTG_CORE_BASE	(OTG_BASE_ADDR+0x000)	//  base location for core
#define OTG_FUNC_BASE	(OTG_BASE_ADDR+0x040)	//  base location for function
#define OTG_HOST_BASE	(OTG_BASE_ADDR+0x080)	//  base location for host
#define OTG_I2C_BASE	(OTG_BASE_ADDR+0x100)	//  base location for I2C
#define OTG_DMA_BASE	(OTG_BASE_ADDR+0x800)	//  base location for dma
		
#define OTG_ETD_BASE	(OTG_BASE_ADDR+0x200)	//  base location for etd memory
#define OTG_EP_BASE	(OTG_BASE_ADDR+0x400)	//  base location for ep memory
#define OTG_SYS_BASE	(OTG_BASE_ADDR+0x600)	//  base location for system
#define OTG_DATA_BASE	(OTG_BASE_ADDR+0x1000)	//  base location for data memory

#define OTG_SYS_CTRL	(OTG_SYS_BASE+0x000)	//  base location for system
		
#define _reg_OTG_CORE_HWMODE		(*((volatile unsigned long *)(MX2_IO_ADDRESS(OTG_CORE_BASE+0x00))))  //  32bit core hardware mode reg
#define _reg_OTG_CORE_CINT_STAT		(*((volatile unsigned long *)(MX2_IO_ADDRESS(OTG_CORE_BASE+0x04))))  //  32bit core int status reg
#define _reg_OTG_CORE_CINT_STEN		(*((volatile unsigned long *)(MX2_IO_ADDRESS(OTG_CORE_BASE+0x08))))  //  32bit core int enable reg
#define _reg_OTG_CORE_CLK_CTRL		(*((volatile unsigned long *)(MX2_IO_ADDRESS(OTG_CORE_BASE+0x0C))))  //  32bit core clock control reg
#define _reg_OTG_CORE_RST_CTRL		(*((volatile unsigned long *)(MX2_IO_ADDRESS(OTG_CORE_BASE+0x10))))  //  32bit core reset control reg
#define _reg_OTG_CORE_FRM_INTVL		(*((volatile unsigned long *)(MX2_IO_ADDRESS(OTG_CORE_BASE+0x14))))  //  32bit core frame interval reg
#define _reg_OTG_CORE_FRM_REMAIN	(*((volatile unsigned long *)(MX2_IO_ADDRESS(OTG_CORE_BASE+0x18))))  //  32bit core frame remaining reg
#define _reg_OTG_CORE_HNP_CSTAT		(*((volatile unsigned long *)(MX2_IO_ADDRESS(OTG_CORE_BASE+0x1C))))  //  32bit core HNP current state reg
#define _reg_OTG_CORE_HNP_TIMER1	(*((volatile unsigned long *)(MX2_IO_ADDRESS(OTG_CORE_BASE+0x20))))  //  32bit core HNP timer 1 reg
#define _reg_OTG_CORE_HNP_TIMER2	(*((volatile unsigned long *)(MX2_IO_ADDRESS(OTG_CORE_BASE+0x24))))  //  32bit core HNP timer 2 reg
#define _reg_OTG_CORE_HNP_T3PCR		(*((volatile unsigned long *)(MX2_IO_ADDRESS(OTG_CORE_BASE+0x28))))  //  32bit core HNP timer 3 pulse ctrl
#define _reg_OTG_CORE_HINT_STAT		(*((volatile unsigned long *)(MX2_IO_ADDRESS(OTG_CORE_BASE+0x2C))))  //  32bit core HNP int status reg
#define _reg_OTG_CORE_HINT_STEN		(*((volatile unsigned long *)(MX2_IO_ADDRESS(OTG_CORE_BASE+0x30))))  //  32bit core HNP int enable reg
		
#define _reg_OTG_FUNC_CND_STAT		(*((volatile unsigned long *)(MX2_IO_ADDRESS(OTG_FUNC_BASE+0x00))))  //  32bit func command status reg
#define _reg_OTG_FUNC_DEV_ADDR		(*((volatile unsigned long *)(MX2_IO_ADDRESS(OTG_FUNC_BASE+0x04))))  //  32bit func device address reg
#define _reg_OTG_FUNC_SINT_STAT		(*((volatile unsigned long *)(MX2_IO_ADDRESS(OTG_FUNC_BASE+0x08))))  //  32bit func system int status reg
#define _reg_OTG_FUNC_SINT_STEN		(*((volatile unsigned long *)(MX2_IO_ADDRESS(OTG_FUNC_BASE+0x0C))))  //  32bit func system int enable reg
#define _reg_OTG_FUNC_XINT_STAT		(*((volatile unsigned long *)(MX2_IO_ADDRESS(OTG_FUNC_BASE+0x10))))  //  32bit func X buf int status reg
#define _reg_OTG_FUNC_YINT_STAT		(*((volatile unsigned long *)(MX2_IO_ADDRESS(OTG_FUNC_BASE+0x14))))  //  32bit func Y buf int status reg
#define _reg_OTG_FUNC_XYINT_STEN	(*((volatile unsigned long *)(MX2_IO_ADDRESS(OTG_FUNC_BASE+0x18))))  //  32bit func XY buf int enable reg
#define _reg_OTG_FUNC_XFILL_STAT	(*((volatile unsigned long *)(MX2_IO_ADDRESS(OTG_FUNC_BASE+0x1C))))  //  32bit func X filled status reg
#define _reg_OTG_FUNC_YFILL_STAT	(*((volatile unsigned long *)(MX2_IO_ADDRESS(OTG_FUNC_BASE+0x20))))  //  32bit func Y filled status reg
#define _reg_OTG_FUNC_EP_EN		(*((volatile unsigned long *)(MX2_IO_ADDRESS(OTG_FUNC_BASE+0x24))))  //  32bit func endpoints enable reg
#define _reg_OTG_FUNC_EP_RDY		(*((volatile unsigned long *)(MX2_IO_ADDRESS(OTG_FUNC_BASE+0x28))))  //  32bit func endpoints ready reg
#define _reg_OTG_FUNC_IINT		(*((volatile unsigned long *)(MX2_IO_ADDRESS(OTG_FUNC_BASE+0x2C))))  //  32bit func immediate interrupt reg
#define _reg_OTG_FUNC_EP_DSTAT		(*((volatile unsigned long *)(MX2_IO_ADDRESS(OTG_FUNC_BASE+0x30))))  //  32bit func endpoints done status
#define _reg_OTG_FUNC_EP_DEN		(*((volatile unsigned long *)(MX2_IO_ADDRESS(OTG_FUNC_BASE+0x34))))  //  32bit func endpoints done enable
#define _reg_OTG_FUNC_EP_TOGGLE		(*((volatile unsigned long *)(MX2_IO_ADDRESS(OTG_FUNC_BASE+0x38))))  //  32bit func endpoints toggle bits
#define _reg_OTG_FUNC_FRM_NUM		(*((volatile unsigned long *)(MX2_IO_ADDRESS(OTG_FUNC_BASE+0x3C))))  //  32bit func frame number reg
		
#define _reg_OTG_HOST_CTRL		(*((volatile unsigned long *)(MX2_IO_ADDRESS(OTG_HOST_BASE+0x00))))  //  32bit host controller config reg
#define _reg_OTG_HOST_SINT_STAT		(*((volatile unsigned long *)(MX2_IO_ADDRESS(OTG_HOST_BASE+0x08))))  //  32bit host system int status reg
#define _reg_OTG_HOST_SINT_STEN		(*((volatile unsigned long *)(MX2_IO_ADDRESS(OTG_HOST_BASE+0x0C))))  //  32bit host system int enable reg
#define _reg_OTG_HOST_XINT_STAT		(*((volatile unsigned long *)(MX2_IO_ADDRESS(OTG_HOST_BASE+0x18))))  //  32bit host X buf int status reg
#define _reg_OTG_HOST_YINT_STAT		(*((volatile unsigned long *)(MX2_IO_ADDRESS(OTG_HOST_BASE+0x1C))))  //  32bit host Y buf int status reg
#define _reg_OTG_HOST_XYINT_STEN	(*((volatile unsigned long *)(MX2_IO_ADDRESS(OTG_HOST_BASE+0x20))))  //  32bit host XY buf int enable reg
#define _reg_OTG_HOST_XFILL_STAT	(*((volatile unsigned long *)(MX2_IO_ADDRESS(OTG_HOST_BASE+0x28))))  //  32bit host X filled status reg
#define _reg_OTG_HOST_YFILL_STAT	(*((volatile unsigned long *)(MX2_IO_ADDRESS(OTG_HOST_BASE+0x2C))))  //  32bit host Y filled status reg
#define _reg_OTG_HOST_ETD_EN		(*((volatile unsigned long *)(MX2_IO_ADDRESS(OTG_HOST_BASE+0x40))))  //  32bit host ETD enables reg
#define _reg_OTG_HOST_DIR_ROUTE		(*((volatile unsigned long *)(MX2_IO_ADDRESS(OTG_HOST_BASE+0x48))))  //  32bit host direct routing reg
#define _reg_OTG_HOST_IINT		(*((volatile unsigned long *)(MX2_IO_ADDRESS(OTG_HOST_BASE+0x4C))))  //  32bit host immediate interrupt reg
#define _reg_OTG_HOST_EP_DSTAT		(*((volatile unsigned long *)(MX2_IO_ADDRESS(OTG_HOST_BASE+0x50))))  //  32bit host endpoints done status
#define _reg_OTG_HOST_ETD_DONE		(*((volatile unsigned long *)(MX2_IO_ADDRESS(OTG_HOST_BASE+0x54))))  //  32bit host ETD done reg
#define _reg_OTG_HOST_FRM_NUM		(*((volatile unsigned long *)(MX2_IO_ADDRESS(OTG_HOST_BASE+0x60))))  //  32bit host frame number reg
#define _reg_OTG_HOST_LSP_THRESH	(*((volatile unsigned long *)(MX2_IO_ADDRESS(OTG_HOST_BASE+0x64))))  //  32bit host low speed threshold reg
#define _reg_OTG_HOST_HUB_DESCA		(*((volatile unsigned long *)(MX2_IO_ADDRESS(OTG_HOST_BASE+0x68))))  //  32bit host root hub descriptor A
#define _reg_OTG_HOST_HUB_DESCB		(*((volatile unsigned long *)(MX2_IO_ADDRESS(OTG_HOST_BASE+0x6C))))  //  32bit host root hub descriptor B
#define _reg_OTG_HOST_HUB_STAT		(*((volatile unsigned long *)(MX2_IO_ADDRESS(OTG_HOST_BASE+0x70))))  //  32bit host root hub status reg
#define _reg_OTG_HOST_PORT1_STAT	(*((volatile unsigned long *)(MX2_IO_ADDRESS(OTG_HOST_BASE+0x74))))  //  32bit host port 1 status bits
#define _reg_OTG_HOST_PORT2_STAT	(*((volatile unsigned long *)(MX2_IO_ADDRESS(OTG_HOST_BASE+0x78))))  //  32bit host port 2 status bits
#define _reg_OTG_HOST_PORT3_STAT	(*((volatile unsigned long *)(MX2_IO_ADDRESS(OTG_HOST_BASE+0x7c))))  //  32bit host port 3 status bits
		
#define _reg_OTG_DMA_REV_NUM		(*((volatile unsigned long *)(MX2_IO_ADDRESS(OTG_DMA_BASE+0x000))))  //  32bit dma revision number reg
#define _reg_OTG_DMA_DINT_STAT		(*((volatile unsigned long *)(MX2_IO_ADDRESS(OTG_DMA_BASE+0x004))))  //  32bit dma int status reg
#define _reg_OTG_DMA_DINT_STEN		(*((volatile unsigned long *)(MX2_IO_ADDRESS(OTG_DMA_BASE+0x008))))  //  32bit dma int enable reg
#define _reg_OTG_DMA_ETD_ERR		(*((volatile unsigned long *)(MX2_IO_ADDRESS(OTG_DMA_BASE+0x00C))))  //  32bit dma ETD error status reg
#define _reg_OTG_DMA_EP_ERR		(*((volatile unsigned long *)(MX2_IO_ADDRESS(OTG_DMA_BASE+0x010))))  //  32bit dma EP error status reg
#define _reg_OTG_DMA_ETD_EN		(*((volatile unsigned long *)(MX2_IO_ADDRESS(OTG_DMA_BASE+0x020))))  //  32bit dma ETD DMA enable reg
#define _reg_OTG_DMA_EP_EN		(*((volatile unsigned long *)(MX2_IO_ADDRESS(OTG_DMA_BASE+0x024))))  //  32bit dma EP DMA enable reg
#define _reg_OTG_DMA_ETD_ENXREQ		(*((volatile unsigned long *)(MX2_IO_ADDRESS(OTG_DMA_BASE+0x028))))  //  32bit dma ETD DMA enable Xtrig req
#define _reg_OTG_DMA_EP_ENXREQ		(*((volatile unsigned long *)(MX2_IO_ADDRESS(OTG_DMA_BASE+0x02C))))  //  32bit dma EP DMA enable Ytrig req
#define _reg_OTG_DMA_ETD_ENXYREQ	(*((volatile unsigned long *)(MX2_IO_ADDRESS(OTG_DMA_BASE+0x030))))  //  32bit dma ETD DMA enble XYtrig req
#define _reg_OTG_DMA_EP_ENXYREQ		(*((volatile unsigned long *)(MX2_IO_ADDRESS(OTG_DMA_BASE+0x034))))  //  32bit dma EP DMA enable XYtrig req
#define _reg_OTG_DMA_ETD_BURST4		(*((volatile unsigned long *)(MX2_IO_ADDRESS(OTG_DMA_BASE+0x038))))  //  32bit dma ETD DMA enble burst4 reg
#define _reg_OTG_DMA_EP_BURST4		(*((volatile unsigned long *)(MX2_IO_ADDRESS(OTG_DMA_BASE+0x03C))))  //  32bit dma EP DMA enable burst4 reg
#define _reg_OTG_DMA_MISC_CTRL		(*((volatile unsigned long *)(MX2_IO_ADDRESS(OTG_DMA_BASE+0x040))))  //  32bit dma EP misc control reg
#define _reg_OTG_DMA_ETD_CH_CLR		(*((volatile unsigned long *)(MX2_IO_ADDRESS(OTG_DMA_BASE+0x048))))  //  32bit dma ETD clear channel reg
#define _reg_OTG_DMA_EP_CH_CLR		(*((volatile unsigned long *)(MX2_IO_ADDRESS(OTG_DMA_BASE+0x04C))))  //  32bit dma EP clear channel reg

#define _reg_OTG_DMA_ETD_MSA(x)	(*((volatile unsigned long *)(MX2_IO_ADDRESS(OTG_DMA_BASE+0x100+x*4))))  //  32bit dma ETD mem start addr reg

		#if 0		
		#define _reg_OTG_DMA_ETD0_MSA	(OTG_DMA_BASE+0x100)	//  32bit dma ETD0 mem start addr reg
		#define _reg_OTG_DMA_ETD1_MSA	(OTG_DMA_BASE+0x104)	//  32bit dma ETD1 mem start addr reg
		#define _reg_OTG_DMA_ETD2_MSA	(OTG_DMA_BASE+0x108)	//  32bit dma ETD2 mem start addr reg
		#define _reg_OTG_DMA_ETD3_MSA	(OTG_DMA_BASE+0x10C)	//  32bit dma ETD3 mem start addr reg
		#define _reg_OTG_DMA_ETD4_MSA	(OTG_DMA_BASE+0x110)	//  32bit dma ETD4 mem start addr reg
		#define _reg_OTG_DMA_ETD5_MSA	(OTG_DMA_BASE+0x114)	//  32bit dma ETD5 mem start addr reg
		#define _reg_OTG_DMA_ETD6_MSA	(OTG_DMA_BASE+0x118)	//  32bit dma ETD6 mem start addr reg
		#define _reg_OTG_DMA_ETD7_MSA	(OTG_DMA_BASE+0x11C)	//  32bit dma ETD7 mem start addr reg
		#define _reg_OTG_DMA_ETD8_MSA	(OTG_DMA_BASE+0x120)	//  32bit dma ETD8 mem start addr reg
		#define _reg_OTG_DMA_ETD9_MSA	(OTG_DMA_BASE+0x124)	//  32bit dma ETD9 mem start addr reg
		#define _reg_OTG_DMA_ETD10_MSA	(OTG_DMA_BASE+0x128)	//  32bit dma ETD10 mem start addr reg
		#define _reg_OTG_DMA_ETD11_MSA	(OTG_DMA_BASE+0x12C)	//  32bit dma ETD11 mem start addr reg
		#define _reg_OTG_DMA_ETD12_MSA	(OTG_DMA_BASE+0x130)	//  32bit dma ETD12 mem start addr reg
		#define _reg_OTG_DMA_ETD13_MSA	(OTG_DMA_BASE+0x134)	//  32bit dma ETD13 mem start addr reg
		#define _reg_OTG_DMA_ETD14_MSA	(OTG_DMA_BASE+0x138)	//  32bit dma ETD14 mem start addr reg
		#define _reg_OTG_DMA_ETD15_MSA	(OTG_DMA_BASE+0x13C)	//  32bit dma ETD15 mem start addr reg
		#define _reg_OTG_DMA_ETD16_MSA	(OTG_DMA_BASE+0x140)	//  32bit dma ETD16 mem start addr reg
		#define _reg_OTG_DMA_ETD17_MSA	(OTG_DMA_BASE+0x144)	//  32bit dma ETD17 mem start addr reg
		#define _reg_OTG_DMA_ETD18_MSA	(OTG_DMA_BASE+0x148)	//  32bit dma ETD18 mem start addr reg
		#define _reg_OTG_DMA_ETD19_MSA	(OTG_DMA_BASE+0x14C)	//  32bit dma ETD19 mem start addr reg
		#define _reg_OTG_DMA_ETD20_MSA	(OTG_DMA_BASE+0x150)	//  32bit dma ETD20 mem start addr reg
		#define _reg_OTG_DMA_ETD21_MSA	(OTG_DMA_BASE+0x154)	//  32bit dma ETD21 mem start addr reg
		#define _reg_OTG_DMA_ETD22_MSA	(OTG_DMA_BASE+0x158)	//  32bit dma ETD22 mem start addr reg
		#define _reg_OTG_DMA_ETD23_MSA	(OTG_DMA_BASE+0x15C)	//  32bit dma ETD23 mem start addr reg
		#define _reg_OTG_DMA_ETD24_MSA	(OTG_DMA_BASE+0x160)	//  32bit dma ETD24 mem start addr reg
		#define _reg_OTG_DMA_ETD25_MSA	(OTG_DMA_BASE+0x164)	//  32bit dma ETD25 mem start addr reg
		#define _reg_OTG_DMA_ETD26_MSA	(OTG_DMA_BASE+0x168)	//  32bit dma ETD26 mem start addr reg
		#define _reg_OTG_DMA_ETD27_MSA	(OTG_DMA_BASE+0x16C)	//  32bit dma ETD27 mem start addr reg
		#define _reg_OTG_DMA_ETD28_MSA	(OTG_DMA_BASE+0x170)	//  32bit dma ETD28 mem start addr reg
		#define _reg_OTG_DMA_ETD29_MSA	(OTG_DMA_BASE+0x174)	//  32bit dma ETD29 mem start addr reg
		#define _reg_OTG_DMA_ETD30_MSA	(OTG_DMA_BASE+0x178)	//  32bit dma ETD30 mem start addr reg
		#define _reg_OTG_DMA_ETD31_MSA	(OTG_DMA_BASE+0x17C)	//  32bit dma ETD31 mem start addr reg
		#endif 	
		
#define _reg_OTG_DMA_EP_O_MSA(x)	(*((volatile unsigned long *)(MX2_IO_ADDRESS(OTG_DMA_BASE+0x180+x*8))))  //  32bit dma EP0 o/p mem start addr		
#define _reg_OTG_DMA_EP_I_MSA(x)	(*((volatile unsigned long *)(MX2_IO_ADDRESS(OTG_DMA_BASE+0x184+x*8))))  //  32bit dma EP0 i/p mem start addr


		#if 0			
		#define _reg_OTG_DMA_EP0_O_MSA	(OTG_DMA_BASE+0x180)	//  32bit dma EP0 o/p mem start addr
		#define _reg_OTG_DMA_EP0_I_MSA	(OTG_DMA_BASE+0x184)	//  32bit dma EP0 i/p mem start addr
		#define _reg_OTG_DMA_EP1_O_MSA	(OTG_DMA_BASE+0x188)	//  32bit dma EP1 o/p mem start addr
		#define _reg_OTG_DMA_EP1_I_MSA	(OTG_DMA_BASE+0x18C)	//  32bit dma EP1 i/p mem start addr
		#define _reg_OTG_DMA_EP2_O_MSA	(OTG_DMA_BASE+0x190)	//  32bit dma EP2 o/p mem start addr
		#define _reg_OTG_DMA_EP2_I_MSA	(OTG_DMA_BASE+0x194)	//  32bit dma EP2 i/p mem start addr
		#define _reg_OTG_DMA_EP3_O_MSA	(OTG_DMA_BASE+0x198)	//  32bit dma EP3 o/p mem start addr
		#define _reg_OTG_DMA_EP3_I_MSA	(OTG_DMA_BASE+0x19C)	//  32bit dma EP3 i/p mem start addr
		#define _reg_OTG_DMA_EP4_O_MSA	(OTG_DMA_BASE+0x1A0)	//  32bit dma EP4 o/p mem start addr
		#define _reg_OTG_DMA_EP4_I_MSA	(OTG_DMA_BASE+0x1A4)	//  32bit dma EP4 i/p mem start addr
		#define _reg_OTG_DMA_EP5_O_MSA	(OTG_DMA_BASE+0x1A8)	//  32bit dma EP5 o/p mem start addr
		#define _reg_OTG_DMA_EP5_I_MSA	(OTG_DMA_BASE+0x1AC)	//  32bit dma EP5 i/p mem start addr
		#define _reg_OTG_DMA_EP6_O_MSA	(OTG_DMA_BASE+0x1B0)	//  32bit dma EP6 o/p mem start addr
		#define _reg_OTG_DMA_EP6_I_MSA	(OTG_DMA_BASE+0x1B4)	//  32bit dma EP6 i/p mem start addr
		#define _reg_OTG_DMA_EP7_O_MSA	(OTG_DMA_BASE+0x1B8)	//  32bit dma EP7 o/p mem start addr
		#define _reg_OTG_DMA_EP7_I_MSA	(OTG_DMA_BASE+0x1BC)	//  32bit dma EP7 i/p mem start addr
		#define _reg_OTG_DMA_EP8_O_MSA	(OTG_DMA_BASE+0x1C0)	//  32bit dma EP8 o/p mem start addr
		#define _reg_OTG_DMA_EP8_I_MSA	(OTG_DMA_BASE+0x1C4)	//  32bit dma EP8 i/p mem start addr
		#define _reg_OTG_DMA_EP9_O_MSA	(OTG_DMA_BASE+0x1C8)	//  32bit dma EP9 o/p mem start addr
		#define _reg_OTG_DMA_EP9_I_MSA	(OTG_DMA_BASE+0x1CC)	//  32bit dma EP9 i/p mem start addr
		#define _reg_OTG_DMA_EP10_O_MSA	(OTG_DMA_BASE+0x1D0)	//  32bit dma EP10 o/p mem start addr
		#define _reg_OTG_DMA_EP10_I_MSA	(OTG_DMA_BASE+0x1D4)	//  32bit dma EP10 i/p mem start addr
		#define _reg_OTG_DMA_EP11_O_MSA	(OTG_DMA_BASE+0x1D8)	//  32bit dma EP11 o/p mem start addr
		#define _reg_OTG_DMA_EP11_I_MSA	(OTG_DMA_BASE+0x1DC)	//  32bit dma EP11 i/p mem start addr
		#define _reg_OTG_DMA_EP12_O_MSA	(OTG_DMA_BASE+0x1E0)	//  32bit dma EP12 o/p mem start addr
		#define _reg_OTG_DMA_EP12_I_MSA	(OTG_DMA_BASE+0x1E4)	//  32bit dma EP12 i/p mem start addr
		#define _reg_OTG_DMA_EP13_O_MSA	(OTG_DMA_BASE+0x1E8)	//  32bit dma EP13 o/p mem start addr
		#define _reg_OTG_DMA_EP13_I_MSA	(OTG_DMA_BASE+0x1EC)	//  32bit dma EP13 i/p mem start addr
		#define _reg_OTG_DMA_EP14_O_MSA	(OTG_DMA_BASE+0x1F0)	//  32bit dma EP14 o/p mem start addr
		#define _reg_OTG_DMA_EP14_I_MSA	(OTG_DMA_BASE+0x1F4)	//  32bit dma EP14 i/p mem start addr
		#define _reg_OTG_DMA_EP15_O_MSA	(OTG_DMA_BASE+0x1F8)	//  32bit dma EP15 o/p mem start addr
		#define _reg_OTG_DMA_EP15_I_MSA	(OTG_DMA_BASE+0x1FC)	//  32bit dma EP15 i/p mem start addr
		#endif	

		
#define _reg_OTG_DMA_ETD_BPTR(x)	(*((volatile unsigned long *)(MX2_IO_ADDRESS(OTG_DMA_BASE+0x200+x*4))))  //  32bit dma ETD0 buf tx pointer reg


		#if 0			
		#define _reg_OTG_DMA_ETD0_BPTR	(OTG_DMA_BASE+0x200)	//  32bit dma ETD0 buf tx pointer reg
		#define _reg_OTG_DMA_ETD1_BPTR	(OTG_DMA_BASE+0x204)	//  32bit dma ETD1 buf tx pointer reg
		#define _reg_OTG_DMA_ETD2_BPTR	(OTG_DMA_BASE+0x208)	//  32bit dma ETD2 buf tx pointer reg
		#define _reg_OTG_DMA_ETD3_BPTR	(OTG_DMA_BASE+0x20C)	//  32bit dma ETD3 buf tx pointer reg
		#define _reg_OTG_DMA_ETD4_BPTR	(OTG_DMA_BASE+0x210)	//  32bit dma ETD4 buf tx pointer reg
		#define _reg_OTG_DMA_ETD5_BPTR	(OTG_DMA_BASE+0x214)	//  32bit dma ETD5 buf tx pointer reg
		#define _reg_OTG_DMA_ETD6_BPTR	(OTG_DMA_BASE+0x218)	//  32bit dma ETD6 buf tx pointer reg
		#define _reg_OTG_DMA_ETD7_BPTR	(OTG_DMA_BASE+0x21C)	//  32bit dma ETD7 buf tx pointer reg
		#define _reg_OTG_DMA_ETD8_BPTR	(OTG_DMA_BASE+0x220)	//  32bit dma ETD8 buf tx pointer reg
		#define _reg_OTG_DMA_ETD9_BPTR	(OTG_DMA_BASE+0x224)	//  32bit dma ETD9 buf tx pointer reg
		#define _reg_OTG_DMA_ETD10_BPTR	(OTG_DMA_BASE+0x228)	//  32bit dma ETD10 buf tx pointer reg
		#define _reg_OTG_DMA_ETD11_BPTR	(OTG_DMA_BASE+0x22C)	//  32bit dma ETD11 buf tx pointer reg
		#define _reg_OTG_DMA_ETD12_BPTR	(OTG_DMA_BASE+0x230)	//  32bit dma ETD12 buf tx pointer reg
		#define _reg_OTG_DMA_ETD13_BPTR	(OTG_DMA_BASE+0x234)	//  32bit dma ETD13 buf tx pointer reg
		#define _reg_OTG_DMA_ETD14_BPTR	(OTG_DMA_BASE+0x238)	//  32bit dma ETD14 buf tx pointer reg
		#define _reg_OTG_DMA_ETD15_BPTR	(OTG_DMA_BASE+0x23C)	//  32bit dma ETD15 buf tx pointer reg
		#define _reg_OTG_DMA_ETD16_BPTR	(OTG_DMA_BASE+0x240)	//  32bit dma ETD16 buf tx pointer reg
		#define _reg_OTG_DMA_ETD17_BPTR	(OTG_DMA_BASE+0x244)	//  32bit dma ETD17 buf tx pointer reg
		#define _reg_OTG_DMA_ETD18_BPTR	(OTG_DMA_BASE+0x248)	//  32bit dma ETD18 buf tx pointer reg
		#define _reg_OTG_DMA_ETD19_BPTR	(OTG_DMA_BASE+0x24C)	//  32bit dma ETD19 buf tx pointer reg
		#define _reg_OTG_DMA_ETD20_BPTR	(OTG_DMA_BASE+0x250)	//  32bit dma ETD20 buf tx pointer reg
		#define _reg_OTG_DMA_ETD21_BPTR	(OTG_DMA_BASE+0x254)	//  32bit dma ETD21 buf tx pointer reg
		#define _reg_OTG_DMA_ETD22_BPTR	(OTG_DMA_BASE+0x258)	//  32bit dma ETD22 buf tx pointer reg
		#define _reg_OTG_DMA_ETD23_BPTR	(OTG_DMA_BASE+0x25C)	//  32bit dma ETD23 buf tx pointer reg
		#define _reg_OTG_DMA_ETD24_BPTR	(OTG_DMA_BASE+0x260)	//  32bit dma ETD24 buf tx pointer reg
		#define _reg_OTG_DMA_ETD25_BPTR	(OTG_DMA_BASE+0x264)	//  32bit dma ETD25 buf tx pointer reg
		#define _reg_OTG_DMA_ETD26_BPTR	(OTG_DMA_BASE+0x268)	//  32bit dma ETD26 buf tx pointer reg
		#define _reg_OTG_DMA_ETD27_BPTR	(OTG_DMA_BASE+0x26C)	//  32bit dma ETD27 buf tx pointer reg
		#define _reg_OTG_DMA_ETD28_BPTR	(OTG_DMA_BASE+0x270)	//  32bit dma ETD28 buf tx pointer reg
		#define _reg_OTG_DMA_ETD29_BPTR	(OTG_DMA_BASE+0x274)	//  32bit dma ETD29 buf tx pointer reg
		#define _reg_OTG_DMA_ETD30_BPTR	(OTG_DMA_BASE+0x278)	//  32bit dma ETD30 buf tx pointer reg
		#define _reg_OTG_DMA_ETD31_BPTR	(OTG_DMA_BASE+0x27C)	//  32bit dma ETD31 buf tx pointer reg
		#endif
		
#define _reg_OTG_DMA_EP_O_BPTR(x)	(*((volatile unsigned long *)(MX2_IO_ADDRESS(OTG_DMA_BASE+0x280+8*x))))  //  32bit dma EP0 o/p buf tx pointer
#define _reg_OTG_DMA_EP_I_BPTR(x)	(*((volatile unsigned long *)(MX2_IO_ADDRESS(OTG_DMA_BASE+0x284+8*x))))  //  32bit dma EP0 i/p buf tx pointer

		#if 0		
		#define _reg_OTG_DMA_EP0_O_BPTR	(OTG_DMA_BASE+0x280)	//  32bit dma EP0 o/p buf tx pointer
		#define _reg_OTG_DMA_EP0_I_BPTR	(OTG_DMA_BASE+0x284)	//  32bit dma EP0 i/p buf tx pointer
		#define _reg_OTG_DMA_EP1_O_BPTR	(OTG_DMA_BASE+0x288)	//  32bit dma EP1 o/p buf tx pointer
		#define _reg_OTG_DMA_EP1_I_BPTR	(OTG_DMA_BASE+0x28C)	//  32bit dma EP1 i/p buf tx pointer
		#define _reg_OTG_DMA_EP2_O_BPTR	(OTG_DMA_BASE+0x290)	//  32bit dma EP2 o/p buf tx pointer
		#define _reg_OTG_DMA_EP2_I_BPTR	(OTG_DMA_BASE+0x294)	//  32bit dma EP2 i/p buf tx pointer
		#define _reg_OTG_DMA_EP3_O_BPTR	(OTG_DMA_BASE+0x298)	//  32bit dma EP3 o/p buf tx pointer
		#define _reg_OTG_DMA_EP3_I_BPTR	(OTG_DMA_BASE+0x29C)	//  32bit dma EP3 i/p buf tx pointer
		#define _reg_OTG_DMA_EP4_O_BPTR	(OTG_DMA_BASE+0x2A0)	//  32bit dma EP4 o/p buf tx pointer
		#define _reg_OTG_DMA_EP4_I_BPTR	(OTG_DMA_BASE+0x2A4)	//  32bit dma EP4 i/p buf tx pointer
		#define _reg_OTG_DMA_EP5_O_BPTR	(OTG_DMA_BASE+0x2A8)	//  32bit dma EP5 o/p buf tx pointer
		#define _reg_OTG_DMA_EP5_I_BPTR	(OTG_DMA_BASE+0x2AC)	//  32bit dma EP5 i/p buf tx pointer
		#define _reg_OTG_DMA_EP6_O_BPTR	(OTG_DMA_BASE+0x2B0)	//  32bit dma EP6 o/p buf tx pointer
		#define _reg_OTG_DMA_EP6_I_BPTR	(OTG_DMA_BASE+0x2B4)	//  32bit dma EP6 i/p buf tx pointer
		#define _reg_OTG_DMA_EP7_O_BPTR	(OTG_DMA_BASE+0x2B8)	//  32bit dma EP7 o/p buf tx pointer
		#define _reg_OTG_DMA_EP7_I_BPTR	(OTG_DMA_BASE+0x2BC)	//  32bit dma EP7 i/p buf tx pointer
		#define _reg_OTG_DMA_EP8_O_BPTR	(OTG_DMA_BASE+0x2C0)	//  32bit dma EP8 o/p buf tx pointer
		#define _reg_OTG_DMA_EP8_I_BPTR	(OTG_DMA_BASE+0x2C4)	//  32bit dma EP8 i/p buf tx pointer
		#define _reg_OTG_DMA_EP9_O_BPTR	(OTG_DMA_BASE+0x2C8)	//  32bit dma EP9 o/p buf tx pointer
		#define _reg_OTG_DMA_EP9_I_BPTR	(OTG_DMA_BASE+0x2CC)	//  32bit dma EP9 i/p buf tx pointer
		#define _reg_OTG_DMA_EP10_O_BPTR	(OTG_DMA_BASE+0x2D0)	//  32bit dma EP10 o/p buf tx pointer
		#define _reg_OTG_DMA_EP10_I_BPTR	(OTG_DMA_BASE+0x2D4)	//  32bit dma EP10 i/p buf tx pointer
		#define _reg_OTG_DMA_EP11_O_BPTR	(OTG_DMA_BASE+0x2D8)	//  32bit dma EP11 o/p buf tx pointer
		#define _reg_OTG_DMA_EP11_I_BPTR	(OTG_DMA_BASE+0x2DC)	//  32bit dma EP11 i/p buf tx pointer
		#define _reg_OTG_DMA_EP12_O_BPTR	(OTG_DMA_BASE+0x2E0)	//  32bit dma EP12 o/p buf tx pointer
		#define _reg_OTG_DMA_EP12_I_BPTR	(OTG_DMA_BASE+0x2E4)	//  32bit dma EP12 i/p buf tx pointer
		#define _reg_OTG_DMA_EP13_O_BPTR	(OTG_DMA_BASE+0x2E8)	//  32bit dma EP13 o/p buf tx pointer
		#define _reg_OTG_DMA_EP13_I_BPTR	(OTG_DMA_BASE+0x2EC)	//  32bit dma EP13 i/p buf tx pointer
		#define _reg_OTG_DMA_EP14_O_BPTR	(OTG_DMA_BASE+0x2F0)	//  32bit dma EP14 o/p buf tx pointer
		#define _reg_OTG_DMA_EP14_I_BPTR	(OTG_DMA_BASE+0x2F4)	//  32bit dma EP14 i/p buf tx pointer
		#define _reg_OTG_DMA_EP15_O_BPTR	(OTG_DMA_BASE+0x2F8)	//  32bit dma EP15 o/p buf tx pointer
		#define _reg_OTG_DMA_EP15_I_BPTR	(OTG_DMA_BASE+0x2FC)	//  32bit dma EP15 i/p buf tx pointer
		#endif

		
#define _reg_OTG_I2C_VENDOR_ID_REG0	(*((volatile unsigned long *)(MX2_IO_ADDRESS(OTG_I2C_BASE+0x00))))  //   8bit I2C reg
#define _reg_OTG_I2C_VENDOR_ID_REG1	(*((volatile unsigned long *)(MX2_IO_ADDRESS(OTG_I2C_BASE+0x01))))  //   8bit I2C reg
#define _reg_OTG_I2C_PRODUCT_ID_REG0	(*((volatile unsigned long *)(MX2_IO_ADDRESS(OTG_I2C_BASE+0x02))))  //   8bit I2C reg
#define _reg_OTG_I2C_PRODUCT_ID_REG1	(*((volatile unsigned long *)(MX2_IO_ADDRESS(OTG_I2C_BASE+0x03))))  //   8bit I2C reg
#define _reg_OTG_I2C_MODE_REG1_SET	(*((volatile unsigned long *)(MX2_IO_ADDRESS(OTG_I2C_BASE+0x04))))  //   8bit I2C reg
#define _reg_OTG_I2C_MODE_REG1_CLR	(*((volatile unsigned long *)(MX2_IO_ADDRESS(OTG_I2C_BASE+0x05))))  //   8bit I2C reg
#define _reg_OTG_I2C_OTG_CTRL_REG1_SET	(*((volatile unsigned long *)(MX2_IO_ADDRESS(OTG_I2C_BASE+0x06))))  //   8bit I2C reg
#define _reg_OTG_I2C_OTG_CTRL_REG1_CLR	(*((volatile unsigned long *)(MX2_IO_ADDRESS(OTG_I2C_BASE+0x07))))  //   8bit I2C reg
#define _reg_OTG_I2C_INT_SRC_REG	(*((volatile unsigned long *)(MX2_IO_ADDRESS(OTG_I2C_BASE+0x08))))  //   8bit I2C reg
#define _reg_OTG_I2C_INT_LAT_REG_SET	(*((volatile unsigned long *)(MX2_IO_ADDRESS(OTG_I2C_BASE+0x0A))))  //   8bit I2C reg
#define _reg_OTG_I2C_INT_LAT_REG_CLR	(*((volatile unsigned long *)(MX2_IO_ADDRESS(OTG_I2C_BASE+0x0B))))  //   8bit I2C reg
#define _reg_OTG_I2C_INT_FALSE_REG_SET	(*((volatile unsigned long *)(MX2_IO_ADDRESS(OTG_I2C_BASE+0x0C))))  //   8bit I2C reg
#define _reg_OTG_I2C_INT_FALSE_REG_CLR	(*((volatile unsigned long *)(MX2_IO_ADDRESS(OTG_I2C_BASE+0x0D))))  //   8bit I2C reg
#define _reg_OTG_I2C_INT_TRUE_REG_SET	(*((volatile unsigned long *)(MX2_IO_ADDRESS(OTG_I2C_BASE+0x0E))))  //   8bit I2C reg
#define _reg_OTG_I2C_INT_TRUE_REG_CLR	(*((volatile unsigned long *)(MX2_IO_ADDRESS(OTG_I2C_BASE+0x0F))))  //   8bit I2C reg
#define _reg_OTG_I2C_OTG_CTRL_REG2_SET	(*((volatile unsigned long *)(MX2_IO_ADDRESS(OTG_I2C_BASE+0x10))))  //   8bit I2C reg
#define _reg_OTG_I2C_OTG_CTRL_REG2_CLR	(*((volatile unsigned long *)(MX2_IO_ADDRESS(OTG_I2C_BASE+0x11))))  //   8bit I2C reg
#define _reg_OTG_I2C_MODE_REG2_SET	(*((volatile unsigned long *)(MX2_IO_ADDRESS(OTG_I2C_BASE+0x12))))  //   8bit I2C reg
#define _reg_OTG_I2C_MODE_REG2_CLR	(*((volatile unsigned long *)(MX2_IO_ADDRESS(OTG_I2C_BASE+0x13))))  //   8bit I2C reg
#define _reg_OTG_I2C_BCD_DEV_REG0	(*((volatile unsigned long *)(MX2_IO_ADDRESS(OTG_I2C_BASE+0x14))))  //   8bit I2C reg
#define _reg_OTG_I2C_BCD_DEV_REG1	(*((volatile unsigned long *)(MX2_IO_ADDRESS(OTG_I2C_BASE+0x15))))  //   8bit I2C reg
		
#define _reg_OTG_I2C_OTG_XCVR_DEVAD	(*((volatile unsigned long *)(MX2_IO_ADDRESS(OTG_I2C_BASE+0x18))))  //   8bit I2C reg
#define _reg_OTG_I2C_SEQ_OP_REG		(*((volatile unsigned long *)(MX2_IO_ADDRESS(OTG_I2C_BASE+0x19))))  //   8bit I2C reg
#define _reg_OTG_I2C_SEQ_RD_STARTAD	(*((volatile unsigned long *)(MX2_IO_ADDRESS(OTG_I2C_BASE+0x1A))))  //   8bit I2C reg
#define _reg_OTG_I2C_OP_CTRL_REG	(*((volatile unsigned long *)(MX2_IO_ADDRESS(OTG_I2C_BASE+0x1B))))  //   8bit I2C reg
#define _reg_OTG_I2C_SCLK_TO_SCL_HPER	(*((volatile unsigned long *)(MX2_IO_ADDRESS(OTG_I2C_BASE+0x1E))))  //   8bit I2C reg
#define _reg_OTG_I2C_MASTER_INT_REG	(*((volatile unsigned long *)(MX2_IO_ADDRESS(OTG_I2C_BASE+0x1F))))  //   8bit I2C reg
		
//#########################################		
//# EMMA                                  #		
//# $1002_6000 to $1002_6FFF              #		
//#########################################		
#define EMMA_BASE_ADDR	0x10026000	
#define EMMA_PP_BASE	(EMMA_BASE_ADDR+0x000)	//  base location for post processor
#define EMMA_PRP_BASE	(EMMA_BASE_ADDR+0x400)	//  base location for pre processor
#define EMMA_DEC_BASE	(EMMA_BASE_ADDR+0x800)	//  base location for decoder
#define EMMA_ENC_BASE	(EMMA_BASE_ADDR+0xC00)	//  base location for encoder
		
#define _reg_EMMA_PP_CNTL		(*((volatile unsigned long *)(MX2_IO_ADDRESS(EMMA_PP_BASE+0x00))))  //  32bit post processor control reg
#define _reg_EMMA_PP_INTRCTRL		(*((volatile unsigned long *)(MX2_IO_ADDRESS(EMMA_PP_BASE+0x04))))  //  32bit pp interrupt enable reg
#define _reg_EMMA_PP_INTRSTATUS		(*((volatile unsigned long *)(MX2_IO_ADDRESS(EMMA_PP_BASE+0x08))))  //  32bit pp interrupt status reg
#define _reg_EMMA_PP_SY_PTR		(*((volatile unsigned long *)(MX2_IO_ADDRESS(EMMA_PP_BASE+0x0C))))  //  32bit pp source Y data ptr reg
#define _reg_EMMA_PP_SCB_PTR		(*((volatile unsigned long *)(MX2_IO_ADDRESS(EMMA_PP_BASE+0x10))))  //  32bit pp source CB data ptr reg
#define _reg_EMMA_PP_SCR_PTR		(*((volatile unsigned long *)(MX2_IO_ADDRESS(EMMA_PP_BASE+0x14))))  //  32bit pp source CR data ptr reg
#define _reg_EMMA_PP_DRGB_PTR		(*((volatile unsigned long *)(MX2_IO_ADDRESS(EMMA_PP_BASE+0x18))))  //  32bit pp dest RGB data ptr reg
#define _reg_EMMA_PP_QUAN_PTR		(*((volatile unsigned long *)(MX2_IO_ADDRESS(EMMA_PP_BASE+0x1C))))  //  32bit pp quantizer data ptr reg
#define _reg_EMMA_PP_PROC_PARA		(*((volatile unsigned long *)(MX2_IO_ADDRESS(EMMA_PP_BASE+0x20))))  //  32bit pp process frame param reg
#define _reg_EMMA_PP_SFRM_WIDTH		(*((volatile unsigned long *)(MX2_IO_ADDRESS(EMMA_PP_BASE+0x24))))  //  32bit pp source frame width reg
#define _reg_EMMA_PP_DDIS_WIDTH		(*((volatile unsigned long *)(MX2_IO_ADDRESS(EMMA_PP_BASE+0x28))))  //  32bit pp destinatn display siz reg
#define _reg_EMMA_PP_DIMAGE_SIZE	(*((volatile unsigned long *)(MX2_IO_ADDRESS(EMMA_PP_BASE+0x2C))))  //  32bit pp destinatn image size reg
#define _reg_EMMA_PP_DPIX_FMT		(*((volatile unsigned long *)(MX2_IO_ADDRESS(EMMA_PP_BASE+0x30))))  //  32bit pp dest pixel format ctr reg
#define _reg_EMMA_PP_RSIZE_IDX		(*((volatile unsigned long *)(MX2_IO_ADDRESS(EMMA_PP_BASE+0x34))))  //  32bit pp resize table index reg
#define _reg_EMMA_PP_LOCK_BIT		(*((volatile unsigned long *)(MX2_IO_ADDRESS(EMMA_PP_BASE+0x38))))  //  32bit pp lock bit reg
#define _reg_EMMA_PP_RSIZE_COEF		(*((volatile unsigned long *)(MX2_IO_ADDRESS(EMMA_PP_BASE+0x100))))  //  32bit pp resize coef table reg
		
#define _reg_EMMA_PRP_CNTL		(*((volatile unsigned long *)(MX2_IO_ADDRESS(EMMA_PRP_BASE+0x00))))  //  32bit preprocessor control reg
#define _reg_EMMA_PRP_INTRCTRL		(*((volatile unsigned long *)(MX2_IO_ADDRESS(EMMA_PRP_BASE+0x04))))  //  32bit prp interrupt enable reg
#define _reg_EMMA_PRP_INTRSTATUS	(*((volatile unsigned long *)(MX2_IO_ADDRESS(EMMA_PRP_BASE+0x08))))  //  32bit prp interrupt status reg
#define _reg_EMMA_PRP_SY_PTR		(*((volatile unsigned long *)(MX2_IO_ADDRESS(EMMA_PRP_BASE+0x0C))))  //  32bit prp source Y data ptr reg
#define _reg_EMMA_PRP_SCB_PTR		(*((volatile unsigned long *)(MX2_IO_ADDRESS(EMMA_PRP_BASE+0x10))))  //  32bit prp source CB data ptr reg
#define _reg_EMMA_PRP_SCR_PTR		(*((volatile unsigned long *)(MX2_IO_ADDRESS(EMMA_PRP_BASE+0x14))))  //  32bit prp source CR data ptr reg
#define _reg_EMMA_PRP_DRGB1_PTR		(*((volatile unsigned long *)(MX2_IO_ADDRESS(EMMA_PRP_BASE+0x18))))  //  32bit prp dest RGB1 data ptr reg
#define _reg_EMMA_PRP_DRGB2_PTR		(*((volatile unsigned long *)(MX2_IO_ADDRESS(EMMA_PRP_BASE+0x1C))))  //  32bit prp dest RGB2 data ptr reg
#define _reg_EMMA_PRP_DY_PTR		(*((volatile unsigned long *)(MX2_IO_ADDRESS(EMMA_PRP_BASE+0x20))))  //  32bit prp dest Y data ptr reg
#define _reg_EMMA_PRP_DCB_PTR		(*((volatile unsigned long *)(MX2_IO_ADDRESS(EMMA_PRP_BASE+0x24))))  //  32bit prp dest CB data ptr reg
#define _reg_EMMA_PRP_DCR_PTR		(*((volatile unsigned long *)(MX2_IO_ADDRESS(EMMA_PRP_BASE+0x28))))  //  32bit prp dest CR data ptr reg
#define _reg_EMMA_PRP_SFRM_SIZE		(*((volatile unsigned long *)(MX2_IO_ADDRESS(EMMA_PRP_BASE+0x2C))))  //  32bit prp source frame size reg
#define _reg_EMMA_PRP_DLST_CH1		(*((volatile unsigned long *)(MX2_IO_ADDRESS(EMMA_PRP_BASE+0x30))))  //  32bit prp dest line stride reg
#define _reg_EMMA_PRP_SPIX_FMT		(*((volatile unsigned long *)(MX2_IO_ADDRESS(EMMA_PRP_BASE+0x34))))  //  32bit prp source pixel format ctrl
#define _reg_EMMA_PRP_DPIX_FMT		(*((volatile unsigned long *)(MX2_IO_ADDRESS(EMMA_PRP_BASE+0x38))))  //  32bit prp dest pixel format reg
#define _reg_EMMA_PRP_DISIZE_CH1	(*((volatile unsigned long *)(MX2_IO_ADDRESS(EMMA_PRP_BASE+0x3C))))  //  32bit prp dest ch1 o/p image size
#define _reg_EMMA_PRP_DISIZE_CH2	(*((volatile unsigned long *)(MX2_IO_ADDRESS(EMMA_PRP_BASE+0x40))))  //  32bit prp dest ch2 o/p image size
#define _reg_EMMA_PRP_DFRM_CNT		(*((volatile unsigned long *)(MX2_IO_ADDRESS(EMMA_PRP_BASE+0x44))))  //  32bit prp dest frame count reg
#define _reg_EMMA_PRP_SLIN_STRID	(*((volatile unsigned long *)(MX2_IO_ADDRESS(EMMA_PRP_BASE+0x48))))  //  32bit prp source line stride reg
#define _reg_EMMA_PRP_RSIZE_CTRL	(*((volatile unsigned long *)(MX2_IO_ADDRESS(EMMA_PRP_BASE+0x4C))))  //  32bit prp resize control reg
#define _reg_EMMA_PRP_LOCK_BIT		(*((volatile unsigned long *)(MX2_IO_ADDRESS(EMMA_PRP_BASE+0x50))))  //  32bit prp lock bit reg
		
#define _reg_EMMA_MPEG4DEC_DEREG	(*((volatile unsigned long *)(MX2_IO_ADDRESS(EMMA_DEC_BASE+0x00)	
#define _reg_EMMA_MPEG4DEC_CTRLBASE	(*((volatile unsigned long *)(MX2_IO_ADDRESS(EMMA_DEC_BASE+0x04)	
#define _reg_EMMA_MPEG4DEC_RLCBASE	(*((volatile unsigned long *)(MX2_IO_ADDRESS(EMMA_DEC_BASE+0x0C)	
#define _reg_EMMA_MPEG4DEC_MCDOBASE	(*((volatile unsigned long *)(MX2_IO_ADDRESS(EMMA_DEC_BASE+0x14)	
#define _reg_EMMA_MPEG4DEC_MCDIBASE	(*((volatile unsigned long *)(MX2_IO_ADDRESS(EMMA_DEC_BASE+0x18)	
#define _reg_EMMA_MPEG4DEC_IDREG	(*((volatile unsigned long *)(MX2_IO_ADDRESS(EMMA_DEC_BASE+0x1C)	
		
#define _reg_EMMA_MPEG4ENC_CONTREG0	(*((volatile unsigned long *)(MX2_IO_ADDRESS(EMMA_ENC_BASE+0x00)	
#define _reg_EMMA_MPEG4ENC_CONTREG1 	(*((volatile unsigned long *)(MX2_IO_ADDRESS(EMMA_ENC_BASE+0x04)	
#define _reg_EMMA_MPEG4ENC_CONTREG2	(*((volatile unsigned long *)(MX2_IO_ADDRESS(EMMA_ENC_BASE+0x08)	
#define _reg_EMMA_MPEG4ENC_CONTREG3	(*((volatile unsigned long *)(MX2_IO_ADDRESS(EMMA_ENC_BASE+0x0C)	
#define _reg_EMMA_MPEG4ENC_CONTREG4	(*((volatile unsigned long *)(MX2_IO_ADDRESS(EMMA_ENC_BASE+0x10)	
#define _reg_EMMA_MPEG4ENC_INTERRUPT	(*((volatile unsigned long *)(MX2_IO_ADDRESS(EMMA_ENC_BASE+0x14)	
#define _reg_EMMA_MPEG4ENC_NBLUMBASE	(*((volatile unsigned long *)(MX2_IO_ADDRESS(EMMA_ENC_BASE+0x18)	
#define _reg_EMMA_MPEG4ENC_IDREG	(*((volatile unsigned long *)(MX2_IO_ADDRESS(EMMA_ENC_BASE+0x1C)	
#define _reg_EMMA_MPEG4ENC_NBCHBASE	(*((volatile unsigned long *)(MX2_IO_ADDRESS(EMMA_ENC_BASE+0x20)	
#define _reg_EMMA_MPEG4ENC_MBTYPEBASE	(*((volatile unsigned long *)(MX2_IO_ADDRESS(EMMA_ENC_BASE+0x24)	
#define _reg_EMMA_MPEG4ENC_VLCCTRBASE	(*((volatile unsigned long *)(MX2_IO_ADDRESS(EMMA_ENC_BASE+0x28)	
#define _reg_EMMA_MPEG4ENC_VLCDABASE	(*((volatile unsigned long *)(MX2_IO_ADDRESS(EMMA_ENC_BASE+0x2C)	
#define _reg_EMMA_MPEG4ENC_SARLUMBASE	(*((volatile unsigned long *)(MX2_IO_ADDRESS(EMMA_ENC_BASE+0x30)	
#define _reg_EMMA_MPEG4ENC_SARCHBASE	(*((volatile unsigned long *)(MX2_IO_ADDRESS(EMMA_ENC_BASE+0x34)	
#define _reg_EMMA_MPEG4ENC_SAWLUMBASE	(*((volatile unsigned long *)(MX2_IO_ADDRESS(EMMA_ENC_BASE+0x38)	
#define _reg_EMMA_MPEG4ENC_SAWCHBASE	(*((volatile unsigned long *)(MX2_IO_ADDRESS(EMMA_ENC_BASE+0x3C)	
		
//#########################################		
//# Clock  Reset (CRM)                   #		
//# System control                        #		
//# $1002_7000 to $1002_7FFF              #		
//#########################################		
#define CRM_BASE_ADDR	0x10027000	
#define _reg_CRM_CSCR	(*((volatile unsigned long *)(MX2_IO_ADDRESS(CRM_BASE_ADDR+0x00))))  //  32bit Clock Source Control Reg
#define _reg_CRM_MPCTL0	(*((volatile unsigned long *)(MX2_IO_ADDRESS(CRM_BASE_ADDR+0x04))))  //  32bit MCU PLL Control Reg
#define _reg_CRM_MPCTL1	(*((volatile unsigned long *)(MX2_IO_ADDRESS(CRM_BASE_ADDR+0x08))))  //  32bit MCU PLL 
#define _reg_CRM_SPCTL0	(*((volatile unsigned long *)(MX2_IO_ADDRESS(CRM_BASE_ADDR+0x0C))))  //  32bit Serial Perpheral PLL Ctrl 0
#define _reg_CRM_SPCTL1	(*((volatile unsigned long *)(MX2_IO_ADDRESS(CRM_BASE_ADDR+0x10))))  //  32bit Serial Perpheral PLL Ctrl 1
#define _reg_CRM_OSC26MCTL	(*((volatile unsigned long *)(MX2_IO_ADDRESS(CRM_BASE_ADDR+0x14))))  //  32bit Osc 26M register
#define _reg_CRM_PCDR	(*((volatile unsigned long *)(MX2_IO_ADDRESS(CRM_BASE_ADDR+0x18))))  //  32bit Serial Perpheral Clk Div Reg
#define _reg_CRM_PCCR0	(*((volatile unsigned long *)(MX2_IO_ADDRESS(CRM_BASE_ADDR+0x1C))))  //  32bit Perpheral Clk Control Reg 0
#define _reg_CRM_PCCR1	(*((volatile unsigned long *)(MX2_IO_ADDRESS(CRM_BASE_ADDR+0x20))))  //  32bit Perpheral Clk Control Reg 1
		
#define _reg_CRM_RSR	(*((volatile unsigned long *)(MX2_IO_ADDRESS(CRM_BASE_ADDR+0x800))))  //  32bit Reset Source Reg
		
#define SYS_BASE_ADDR	0x10027800	
#define _reg_SYS_SIDR	(*((volatile unsigned long *)(MX2_IO_ADDRESS(SYS_BASE_ADDR+0x04))))  //  128bit Silicon ID Reg
#define _reg_SYS_SIDR1	(*((volatile unsigned long *)(MX2_IO_ADDRESS(SYS_BASE_ADDR+0x04))))  //  128bit Silicon ID Reg word 1
#define _reg_SYS_SIDR2	(*((volatile unsigned long *)(MX2_IO_ADDRESS(SYS_BASE_ADDR+0x08))))  //  128bit Silicon ID Reg word 2
#define _reg_SYS_SIDR3	(*((volatile unsigned long *)(MX2_IO_ADDRESS(SYS_BASE_ADDR+0x0C))))  //  128bit Silicon ID Reg word 3
#define _reg_SYS_SIDR4	(*((volatile unsigned long *)(MX2_IO_ADDRESS(SYS_BASE_ADDR+0x10))))  //  128bit Silicon ID Reg word 4
#define _reg_SYS_FMCR	(*((volatile unsigned long *)(MX2_IO_ADDRESS(SYS_BASE_ADDR+0x14))))  //  Functional Muxing Control Reg
#define _reg_SYS_GPCR	(*((volatile unsigned long *)(MX2_IO_ADDRESS(SYS_BASE_ADDR+0x18))))  //  Global Peripheral Control Reg
#define _reg_SYS_WBCR	(*((volatile unsigned long *)(MX2_IO_ADDRESS(SYS_BASE_ADDR+0x1C))))  //  Well Bias Control Reg
#define _reg_SYS_DSCR1	(*((volatile unsigned long *)(MX2_IO_ADDRESS(SYS_BASE_ADDR+0x20))))  //  Drive Strength Crtl Reg 1
#define _reg_SYS_DSCR2	(*((volatile unsigned long *)(MX2_IO_ADDRESS(SYS_BASE_ADDR+0x24))))  //  Drive Strength Crtl Reg 2
#define _reg_SYS_DSCR3	(*((volatile unsigned long *)(MX2_IO_ADDRESS(SYS_BASE_ADDR+0x28))))  //  Drive Strength Crtl Reg 3
#define _reg_SYS_DSCR4	(*((volatile unsigned long *)(MX2_IO_ADDRESS(SYS_BASE_ADDR+0x2C))))  //  Drive Strength Crtl Reg 4
#define _reg_SYS_DSCR5	(*((volatile unsigned long *)(MX2_IO_ADDRESS(SYS_BASE_ADDR+0x30))))  //  Drive Strength Crtl Reg 5
#define _reg_SYS_DSCR6	(*((volatile unsigned long *)(MX2_IO_ADDRESS(SYS_BASE_ADDR+0x34))))  //  Drive Strength Crtl Reg 6
#define _reg_SYS_DSCR7	(*((volatile unsigned long *)(MX2_IO_ADDRESS(SYS_BASE_ADDR+0x38))))  //  Drive Strength Crtl Reg 7
#define _reg_SYS_DSCR8	(*((volatile unsigned long *)(MX2_IO_ADDRESS(SYS_BASE_ADDR+0x3C))))  //  Drive Strength Crtl Reg 8
#define _reg_SYS_DSCR9	(*((volatile unsigned long *)(MX2_IO_ADDRESS(SYS_BASE_ADDR+0x40))))  //  Drive Strength Crtl Reg 9
#define _reg_SYS_DSCR10	(*((volatile unsigned long *)(MX2_IO_ADDRESS(SYS_BASE_ADDR+0x44))))  //  Drive Strength Crtl Reg 10
#define _reg_SYS_DSCR11	(*((volatile unsigned long *)(MX2_IO_ADDRESS(SYS_BASE_ADDR+0x48))))  //  Drive Strength Crtl Reg 11
#define _reg_SYS_DSCR12	(*((volatile unsigned long *)(MX2_IO_ADDRESS(SYS_BASE_ADDR+0x4C))))  //  Drive Strength Crtl Reg 12
#define _reg_SYS_PSCR	(*((volatile unsigned long *)(MX2_IO_ADDRESS(SYS_BASE_ADDR+0x50))))  //  Priority Control/select Reg
		
//#########################################		
//# FIRI                                  #		
//# $1002_8000 to $1002_8FFF              #		
//#########################################		
#define FIRI_BASE_ADDR	0x10028000	
#define _reg_FIRI_FIRITCR	(*((volatile unsigned long *)(MX2_IO_ADDRESS(FIRI_BASE_ADDR+0x00))))  //  32bit firi tx control reg 
#define _reg_FIRI_FIRITCTR	(*((volatile unsigned long *)(MX2_IO_ADDRESS(FIRI_BASE_ADDR+0x04))))  //  32bit firi tx count  reg
#define _reg_FIRI_FIRIRCR	(*((volatile unsigned long *)(MX2_IO_ADDRESS(FIRI_BASE_ADDR+0x08))))  //  32bit firi rx control reg
#define _reg_FIRI_FIRITSR	(*((volatile unsigned long *)(MX2_IO_ADDRESS(FIRI_BASE_ADDR+0x0C))))  //  32bit firi tx status reg
#define _reg_FIRI_FIRIRSR	(*((volatile unsigned long *)(MX2_IO_ADDRESS(FIRI_BASE_ADDR+0x10))))  //  32bit firi rx status reg
#define _reg_FIRI_TFIFO		(*((volatile unsigned long *)(MX2_IO_ADDRESS(FIRI_BASE_ADDR+0x14))))  //  32bit firi tx fifo reg
#define _reg_FIRI_RFIFO		(*((volatile unsigned long *)(MX2_IO_ADDRESS(FIRI_BASE_ADDR+0x18))))  //  32bit firi rx fifo reg
#define _reg_FIRI_FIRICR	(*((volatile unsigned long *)(MX2_IO_ADDRESS(FIRI_BASE_ADDR+0x1C))))  //  32bit firi control reg
		
		
		
		
//#########################################		
//# JAM                                   #		
//# $1003_E000 to $1003_EFFF              #		
//#########################################		
#define JAM_BASE_ADDR	0x1003E000	
#define _reg_JAM_ARM9P_GPR0	(*((volatile unsigned long *)(MX2_IO_ADDRESS(JAM_BASE_ADDR+0x00))))  //  32bit jam debug enable
#define _reg_JAM_ARM9P_GPR4	(*((volatile unsigned long *)(MX2_IO_ADDRESS(JAM_BASE_ADDR+0x10))))  //  32bit jam platform version
		
//#########################################		
//# MAX                                   #		
//# $1003_F000 to $1003_FFFF              #		
//#########################################		
#define MAX_BASE_ADDR	0x1003F000

#define MAX_SLV_BASE(x)	(*((volatile unsigned long *)(MX2_IO_ADDRESS(MAX_BASE_ADDR+0x100*x))))  //  base location for slave 0-3

		#if 0	
		#define MAX_SLV0_BASE	(MAX_BASE_ADDR+0x000)	//  base location for slave 0
		#define MAX_SLV1_BASE	(MAX_BASE_ADDR+0x100)	//  base location for slave 1
		#define MAX_SLV2_BASE	(MAX_BASE_ADDR+0x200)	//  base location for slave 2
		#define MAX_SLV3_BASE	(MAX_BASE_ADDR+0x300)	//  base location for slave 3
		#endif


//Karen modify,it should not use MAX_SLV_BASE again.
//#define _reg_MAX_SLV_MPR(x)		(*((volatile unsigned long *)(MX2_IO_ADDRESS(MAX_SLV_BASE(x)+0x00))))  //  32bit max slv master priority reg
//#define _reg_MAX_SLV_AMPR(x)		(*((volatile unsigned long *)(MX2_IO_ADDRESS(MAX_SLV_BASE(x)+0x04))))  //  32bit max slv0 alt priority reg
//#define _reg_MAX_SLV_SGPCR(x)		(*((volatile unsigned long *)(MX2_IO_ADDRESS(MAX_SLV_BASE(x)+0x10))))  //  32bit max slv0 general ctrl reg
//#define _reg_MAX_SLV_ASGPCR(x)		(*((volatile unsigned long *)(MX2_IO_ADDRESS(MAX_SLV_BASE(x)+0x14))))  //  32bit max slv0 alt generl ctrl reg
#define _reg_MAX_SLV_MPR(x)			(*((volatile unsigned long *)(MX2_IO_ADDRESS(MAX_BASE_ADDR+0x100*x+0x00))))  //  32bit max slv master priority reg
#define _reg_MAX_SLV_AMPR(x)		(*((volatile unsigned long *)(MX2_IO_ADDRESS(MAX_BASE_ADDR+0x100*x+0x04))))  //  32bit max slv0 alt priority reg
#define _reg_MAX_SLV_SGPCR(x)		(*((volatile unsigned long *)(MX2_IO_ADDRESS(MAX_BASE_ADDR+0x100*x+0x10))))  //  32bit max slv0 general ctrl reg
#define _reg_MAX_SLV_ASGPCR(x)		(*((volatile unsigned long *)(MX2_IO_ADDRESS(MAX_BASE_ADDR+0x100*x+0x14))))  //  32bit max slv0 alt generl ctrl reg

		#if 0		
		#define MAX_SLV0_MPR0	(MAX_SLV0_BASE+0x00)	//  32bit max slv0 master priority reg
		#define MAX_SLV0_AMPR0	(MAX_SLV0_BASE+0x04)	//  32bit max slv0 alt priority reg
		#define MAX_SLV0_SGPCR0	(MAX_SLV0_BASE+0x10)	//  32bit max slv0 general ctrl reg
		#define MAX_SLV0_ASGPCR0	(MAX_SLV0_BASE+0x14)	//  32bit max slv0 alt generl ctrl reg
				
		#define MAX_SLV1_MPR1	(MAX_SLV1_BASE+0x00)	//  32bit max slv1 master priority reg
		#define MAX_SLV1_AMPR1	(MAX_SLV1_BASE+0x04)	//  32bit max slv1 alt priority reg
		#define MAX_SLV1_SGPCR1	(MAX_SLV1_BASE+0x10)	//  32bit max slv1 general ctrl reg
		#define MAX_SLV1_ASGPCR1	(MAX_SLV1_BASE+0x14)	//  32bit max slv1 alt generl ctrl reg
				
		#define MAX_SLV2_MPR2	(MAX_SLV2_BASE+0x00)	//  32bit max slv2 master priority reg
		#define MAX_SLV2_AMPR2	(MAX_SLV2_BASE+0x04)	//  32bit max slv2 alt priority reg
		#define MAX_SLV2_SGPCR2	(MAX_SLV2_BASE+0x10)	//  32bit max slv2 general ctrl reg
		#define MAX_SLV2_ASGPCR2	(MAX_SLV2_BASE+0x14)	//  32bit max slv2 alt generl ctrl reg
				
		#define MAX_SLV3_MPR3	(MAX_SLV3_BASE+0x00)	//  32bit max slv3 master priority reg
		#define MAX_SLV3_AMPR3	(MAX_SLV3_BASE+0x04)	//  32bit max slv3 alt priority reg
		#define MAX_SLV3_SGPCR3	(MAX_SLV3_BASE+0x10)	//  32bit max slv3 general ctrl reg
		#define MAX_SLV3_ASGPCR3	(MAX_SLV3_BASE+0x14)	//  32bit max slv3 alt generl ctrl reg
		#endif

#define _reg_MAX_MST_MGPCR(x)	(*((volatile unsigned long *)(MX2_IO_ADDRESS(MAX_BASE_ADDR+0x800+0x100*x))))  //  32bit max mst0-5 general ctrl reg

		#if 0		
		#define MAX_MST0_MGPCR0	(MAX_BASE_ADDR+0x800)	//  32bit max mst0 general ctrl reg
		#define MAX_MST1_MGPCR1	(MAX_BASE_ADDR+0x900)	//  32bit max mst1 general ctrl reg
		#define MAX_MST2_MGPCR2	(MAX_BASE_ADDR+0xA00)	//  32bit max mst2 general ctrl reg
		#define MAX_MST3_MGPCR3	(MAX_BASE_ADDR+0xB00)	//  32bit max mst3 general ctrl reg
		#define MAX_MST4_MGPCR4	(MAX_BASE_ADDR+0xC00)	//  32bit max mst4 general ctrl reg
		#define MAX_MST5_MGPCR5	(MAX_BASE_ADDR+0xD00)	//  32bit max mst5 general ctrl reg
		#endif		
		
		
		
//#########################################		
//# AITC                                  #		
//# $1004_0000 to $1004_0FFF              #		
//#########################################		
#define AITC_BASE_ADDR	0x10040000	
#define _reg_AITC_INTCNTL	(*((volatile unsigned long *)(MX2_IO_ADDRESS(AITC_BASE_ADDR))))  //  32bit aitc int control reg
#define _reg_AITC_NIMASK	(*((volatile unsigned long *)(MX2_IO_ADDRESS(AITC_BASE_ADDR+0x04))))  //  32bit aitc int mask reg
#define _reg_AITC_INTENNUM	(*((volatile unsigned long *)(MX2_IO_ADDRESS(AITC_BASE_ADDR+0x08))))  //  32bit aitc int enable number reg
#define _reg_AITC_INTDISNUM	(*((volatile unsigned long *)(MX2_IO_ADDRESS(AITC_BASE_ADDR+0x0C))))  //  32bit aitc int disable number reg
#define _reg_AITC_INTENABLEH	(*((volatile unsigned long *)(MX2_IO_ADDRESS(AITC_BASE_ADDR+0x10))))  //  32bit aitc int enable reg high
#define _reg_AITC_INTENABLEL	(*((volatile unsigned long *)(MX2_IO_ADDRESS(AITC_BASE_ADDR+0x14))))  //  32bit aitc int enable reg low
#define _reg_AITC_INTTYPEH	(*((volatile unsigned long *)(MX2_IO_ADDRESS(AITC_BASE_ADDR+0x18))))  //  32bit aitc int type reg high
#define _reg_AITC_INTTYPEL	(*((volatile unsigned long *)(MX2_IO_ADDRESS(AITC_BASE_ADDR+0x1C))))  //  32bit aitc int type reg low

#define AITC_NIPRIORITY(x)	(*((volatile unsigned long *)(MX2_IO_ADDRESS(AITC_BASE_ADDR+0x20+(7-x)*4)	//  32bit aitc norm int priority lvl7

		#if 0
		#define AITC_NIPRIORITY7	(AITC_BASE_ADDR+0x20)	//  32bit aitc norm int priority lvl7
		#define AITC_NIPRIORITY6	(AITC_BASE_ADDR+0x24)	//  32bit aitc norm int priority lvl6
		#define AITC_NIPRIORITY5	(AITC_BASE_ADDR+0x28)	//  32bit aitc norm int priority lvl5
		#define AITC_NIPRIORITY4	(AITC_BASE_ADDR+0x2C)	//  32bit aitc norm int priority lvl4
		#define AITC_NIPRIORITY3	(AITC_BASE_ADDR+0x30)	//  32bit aitc norm int priority lvl3
		#define AITC_NIPRIORITY2	(AITC_BASE_ADDR+0x34)	//  32bit aitc norm int priority lvl2
		#define AITC_NIPRIORITY1	(AITC_BASE_ADDR+0x38)	//  32bit aitc norm int priority lvl1
		#define AITC_NIPRIORITY0	(AITC_BASE_ADDR+0x3C)	//  32bit aitc norm int priority lvl0
		#endif

#define _reg_AITC_NIVECSR	(*((volatile unsigned long *)(MX2_IO_ADDRESS(AITC_BASE_ADDR+0x40))))  //  32bit aitc norm int vector/status
#define _reg_AITC_FIVECSR	(*((volatile unsigned long *)(MX2_IO_ADDRESS(AITC_BASE_ADDR+0x44))))  //  32bit aitc fast int vector/status
#define _reg_AITC_INTSRCH	(*((volatile unsigned long *)(MX2_IO_ADDRESS(AITC_BASE_ADDR+0x48))))  //  32bit aitc int source reg high
#define _reg_AITC_INTSRCL	(*((volatile unsigned long *)(MX2_IO_ADDRESS(AITC_BASE_ADDR+0x4C))))  //  32bit aitc int source reg low
#define _reg_AITC_INTFRCH	(*((volatile unsigned long *)(MX2_IO_ADDRESS(AITC_BASE_ADDR+0x50))))  //  32bit aitc int force reg high
#define _reg_AITC_INTFRCL	(*((volatile unsigned long *)(MX2_IO_ADDRESS(AITC_BASE_ADDR+0x54))))  //  32bit aitc int force reg low
#define _reg_AITC_NIPNDH	(*((volatile unsigned long *)(MX2_IO_ADDRESS(AITC_BASE_ADDR+0x58))))  //  32bit aitc norm int pending high
#define _reg_AITC_NIPNDL	(*((volatile unsigned long *)(MX2_IO_ADDRESS(AITC_BASE_ADDR+0x5C))))  //  32bit aitc norm int pending low
#define _reg_AITC_FIPNDH	(*((volatile unsigned long *)(MX2_IO_ADDRESS(AITC_BASE_ADDR+0x60))))  //  32bit aitc fast int pending high
#define _reg_AITC_FIPNDL	(*((volatile unsigned long *)(MX2_IO_ADDRESS(AITC_BASE_ADDR+0x64))))  //  32bit aitc fast int pending low
		
//#########################################		
//# ROMPATCH                              #		
//# $1004_1000 to $1004_1FFF              #		
//#########################################		
#define ROMPATCH_BASE_ADDR	0x10041000	

#define _reg_ROMPATCH_D(x)	(*((volatile unsigned long *)(MX2_IO_ADDRESS(ROMPATCH_BASE_ADDR+0x0B4+(15-x)*4))))  //  32bit rompatch data reg 15

		#if 0
		#define ROMPATCH_D15	(ROMPATCH_BASE_ADDR+0x0B4)	//  32bit rompatch data reg 15
		#define ROMPATCH_D14	(ROMPATCH_BASE_ADDR+0x0B8)	//  32bit rompatch data reg 14
		#define ROMPATCH_D13	(ROMPATCH_BASE_ADDR+0x0BC)	//  32bit rompatch data reg 13
		#define ROMPATCH_D12	(ROMPATCH_BASE_ADDR+0x0C0)	//  32bit rompatch data reg 12
		#define ROMPATCH_D11	(ROMPATCH_BASE_ADDR+0x0C4)	//  32bit rompatch data reg 11
		#define ROMPATCH_D10	(ROMPATCH_BASE_ADDR+0x0C8)	//  32bit rompatch data reg 10
		#define ROMPATCH_D9	(ROMPATCH_BASE_ADDR+0x0CC)	//  32bit rompatch data reg 9
		#define ROMPATCH_D8	(ROMPATCH_BASE_ADDR+0x0D0)	//  32bit rompatch data reg 8
		#define ROMPATCH_D7	(ROMPATCH_BASE_ADDR+0x0D4)	//  32bit rompatch data reg 7
		#define ROMPATCH_D6	(ROMPATCH_BASE_ADDR+0x0D8)	//  32bit rompatch data reg 6
		#define ROMPATCH_D5	(ROMPATCH_BASE_ADDR+0x0DC)	//  32bit rompatch data reg 5
		#define ROMPATCH_D4	(ROMPATCH_BASE_ADDR+0x0E0)	//  32bit rompatch data reg 4
		#define ROMPATCH_D3	(ROMPATCH_BASE_ADDR+0x0E4)	//  32bit rompatch data reg 3
		#define ROMPATCH_D2	(ROMPATCH_BASE_ADDR+0x0E8)	//  32bit rompatch data reg 2
		#define ROMPATCH_D1	(ROMPATCH_BASE_ADDR+0x0EC)	//  32bit rompatch data reg 1
		#define ROMPATCH_D0	(ROMPATCH_BASE_ADDR+0x0F0)	//  32bit rompatch data reg 0
		#endif
		
#define _reg_ROMPATCH_CNTL	(*((volatile unsigned long *)(MX2_IO_ADDRESS(ROMPATCH_BASE_ADDR+0x0F4))))  //  32bit rompatch control reg
#define _reg_ROMPATCH_ENH	(*((volatile unsigned long *)(MX2_IO_ADDRESS(ROMPATCH_BASE_ADDR+0x0F8))))  //  32bit rompatch enable reg high
#define _reg_ROMPATCH_ENL	(*((volatile unsigned long *)(MX2_IO_ADDRESS(ROMPATCH_BASE_ADDR+0x0FC))))  //  32bit rompatch enable reg low


#define _reg_ROMPATCH_A(x)	(*((volatile unsigned long *)(MX2_IO_ADDRESS(ROMPATCH_BASE_ADDR+0x100+x*4))))  //  32bit rompatch data reg 15

		#if 0
		#define ROMPATCH_A0	(ROMPATCH_BASE_ADDR+0x100)	//  32bit rompatch addr reg 0
		#define ROMPATCH_A1	(ROMPATCH_BASE_ADDR+0x104)	//  32bit rompatch addr reg 1
		#define ROMPATCH_A2	(ROMPATCH_BASE_ADDR+0x108)	//  32bit rompatch addr reg 2
		#define ROMPATCH_A3	(ROMPATCH_BASE_ADDR+0x10C)	//  32bit rompatch addr reg 3
		#define ROMPATCH_A4	(ROMPATCH_BASE_ADDR+0x110)	//  32bit rompatch addr reg 4
		#define ROMPATCH_A5	(ROMPATCH_BASE_ADDR+0x114)	//  32bit rompatch addr reg 5
		#define ROMPATCH_A6	(ROMPATCH_BASE_ADDR+0x118)	//  32bit rompatch addr reg 6
		#define ROMPATCH_A7	(ROMPATCH_BASE_ADDR+0x11C)	//  32bit rompatch addr reg 7
		#define ROMPATCH_A8	(ROMPATCH_BASE_ADDR+0x120)	//  32bit rompatch addr reg 8
		#define ROMPATCH_A9	(ROMPATCH_BASE_ADDR+0x124)	//  32bit rompatch addr reg 9
		#define ROMPATCH_A10	(ROMPATCH_BASE_ADDR+0x128)	//  32bit rompatch addr reg 10
		#define ROMPATCH_A11	(ROMPATCH_BASE_ADDR+0x12C)	//  32bit rompatch addr reg 11
		#define ROMPATCH_A12	(ROMPATCH_BASE_ADDR+0x130)	//  32bit rompatch addr reg 12
		#define ROMPATCH_A13	(ROMPATCH_BASE_ADDR+0x134)	//  32bit rompatch addr reg 13
		#define ROMPATCH_A14	(ROMPATCH_BASE_ADDR+0x138)	//  32bit rompatch addr reg 14
		#define ROMPATCH_A15	(ROMPATCH_BASE_ADDR+0x13C)	//  32bit rompatch addr reg 15
		#define ROMPATCH_A16	(ROMPATCH_BASE_ADDR+0x140)	//  32bit rompatch addr reg 16
		#define ROMPATCH_A17	(ROMPATCH_BASE_ADDR+0x144)	//  32bit rompatch addr reg 17
		#define ROMPATCH_A18	(ROMPATCH_BASE_ADDR+0x148)	//  32bit rompatch addr reg 18
		#define ROMPATCH_A19	(ROMPATCH_BASE_ADDR+0x14C)	//  32bit rompatch addr reg 19
		#define ROMPATCH_A20	(ROMPATCH_BASE_ADDR+0x150)	//  32bit rompatch addr reg 20
		#define ROMPATCH_A21	(ROMPATCH_BASE_ADDR+0x154)	//  32bit rompatch addr reg 21
		#define ROMPATCH_A22	(ROMPATCH_BASE_ADDR+0x158)	//  32bit rompatch addr reg 22
		#define ROMPATCH_A23	(ROMPATCH_BASE_ADDR+0x15C)	//  32bit rompatch addr reg 23
		#define ROMPATCH_A24	(ROMPATCH_BASE_ADDR+0x160)	//  32bit rompatch addr reg 24
		#define ROMPATCH_A25	(ROMPATCH_BASE_ADDR+0x164)	//  32bit rompatch addr reg 25
		#define ROMPATCH_A26	(ROMPATCH_BASE_ADDR+0x168)	//  32bit rompatch addr reg 26
		#define ROMPATCH_A27	(ROMPATCH_BASE_ADDR+0x16C)	//  32bit rompatch addr reg 27
		#define ROMPATCH_A28	(ROMPATCH_BASE_ADDR+0x170)	//  32bit rompatch addr reg 28
		#define ROMPATCH_A29	(ROMPATCH_BASE_ADDR+0x174)	//  32bit rompatch addr reg 29
		#define ROMPATCH_A30	(ROMPATCH_BASE_ADDR+0x178)	//  32bit rompatch addr reg 30
		#define ROMPATCH_A31	(ROMPATCH_BASE_ADDR+0x17C)	//  32bit rompatch addr reg 31
		#endif


#define _reg_ROMPATCH_BRPT	(*((volatile unsigned long *)(MX2_IO_ADDRESS(ROMPATCH_BASE_ADDR+0x200))))  //  32bit rompatch 
#define _reg_ROMPATCH_BASE	(*((volatile unsigned long *)(MX2_IO_ADDRESS(ROMPATCH_BASE_ADDR+0x204))))  //  32bit rompatch base addr reg
#define _reg_ROMPATCH_SR	(*((volatile unsigned long *)(MX2_IO_ADDRESS(ROMPATCH_BASE_ADDR+0x208))))  //  32bit rompatch status reg
#define _reg_ROMPATCH_ABSR	(*((volatile unsigned long *)(MX2_IO_ADDRESS(ROMPATCH_BASE_ADDR+0x20C))))  //  32bit rompatch abort status reg
#define _reg_ROMPATCH_DADR	(*((volatile unsigned long *)(MX2_IO_ADDRESS(ROMPATCH_BASE_ADDR+0x210))))  //  32bit rompatch d-ahb addr abort
#define _reg_ROMPATCH_IADR	(*((volatile unsigned long *)(MX2_IO_ADDRESS(ROMPATCH_BASE_ADDR+0x214))))  //  32bit rompatch i-ahb addr abort
		
		
//#########################################		
//# SMN                                   #		
//# $1004_2000 to $1004_2FFF              #		
//#########################################		
#define SMN_BASE_ADDR	0x10042000	
#define _reg_SMN_STATUS		(*((volatile unsigned long *)(MX2_IO_ADDRESS(SMN_BASE_ADDR+0x00))))  //  32bit SMN status reg
#define _reg_SMN_CONTROL	(*((volatile unsigned long *)(MX2_IO_ADDRESS(SMN_BASE_ADDR+0x04))))  //  32bit SMN command reg
#define _reg_SMN_SEQ_START	(*((volatile unsigned long *)(MX2_IO_ADDRESS(SMN_BASE_ADDR+0x08))))  //  32bit SMN sequence start reg
#define _reg_SMN_SEQ_END	(*((volatile unsigned long *)(MX2_IO_ADDRESS(SMN_BASE_ADDR+0x0C))))  //  32bit SMN sequence end reg
#define _reg_SMN_SEQ_CHK	(*((volatile unsigned long *)(MX2_IO_ADDRESS(SMN_BASE_ADDR+0x10))))  //  32bit SMN sequence check reg
#define _reg_SMN_BIT_CNT	(*((volatile unsigned long *)(MX2_IO_ADDRESS(SMN_BASE_ADDR+0x14))))  //  32bit SMN bit count reg
#define _reg_SMN_INC_SIZE	(*((volatile unsigned long *)(MX2_IO_ADDRESS(SMN_BASE_ADDR+0x18))))  //  32bit SMN increment size reg
#define _reg_SMN_BB_DEC		(*((volatile unsigned long *)(MX2_IO_ADDRESS(SMN_BASE_ADDR+0x1C))))  //  32bit SMN bit bank decrement reg
#define _reg_SMN_COMP_SIZE	(*((volatile unsigned long *)(MX2_IO_ADDRESS(SMN_BASE_ADDR+0x20))))  //  32bit SMN compare size reg
#define _reg_SMN_PT_CHK		(*((volatile unsigned long *)(MX2_IO_ADDRESS(SMN_BASE_ADDR+0x24))))  //  32bit SMN plain text check reg
#define _reg_SMN_CT_CHK		(*((volatile unsigned long *)(MX2_IO_ADDRESS(SMN_BASE_ADDR+0x28))))  //  32bit SMN cipher text check reg
#define _reg_SMN_TIMER_IV	(*((volatile unsigned long *)(MX2_IO_ADDRESS(SMN_BASE_ADDR+0x2C))))  //  32bit SMN timer initial value reg
#define _reg_SMN_TIMER_CTL	(*((volatile unsigned long *)(MX2_IO_ADDRESS(SMN_BASE_ADDR+0x30))))  //  32bit SMN timer control reg
#define _reg_SMN_DD_STATUS	(*((volatile unsigned long *)(MX2_IO_ADDRESS(SMN_BASE_ADDR+0x34))))  //  32bit SMN debug detector reg
#define _reg_SMN_TIMER		(*((volatile unsigned long *)(MX2_IO_ADDRESS(SMN_BASE_ADDR+0x38))))  //  32bit SMN timer reg
		
		
//#########################################		
//# SCM                                   #		
//# $1004_3000 to $1004_3FFF              #		
//#########################################		
#define SCM_BASE_ADDR	0x10043000	
#define _reg_SCM_RED_START	(*((volatile unsigned long *)(MX2_IO_ADDRESS(SCM_BASE_ADDR+0x00))))  //  32bit SCM red memory start addr
#define _reg_SCM_BLACK_START	(*((volatile unsigned long *)(MX2_IO_ADDRESS(SCM_BASE_ADDR+0x004))))  //  32bit SCM black memory start addr
#define _reg_SCM_LENGTH		(*((volatile unsigned long *)(MX2_IO_ADDRESS(SCM_BASE_ADDR+0x008))))  //  32bit SCM num blks encrypted
#define _reg_SCM_CONTROL	(*((volatile unsigned long *)(MX2_IO_ADDRESS(SCM_BASE_ADDR+0x00C))))  //  32bit SCM control reg
#define _reg_SCM_STATUS		(*((volatile unsigned long *)(MX2_IO_ADDRESS(SCM_BASE_ADDR+0x010))))  //  32bit SCM status reg
#define _reg_SCM_ERROR		(*((volatile unsigned long *)(MX2_IO_ADDRESS(SCM_BASE_ADDR+0x014))))  //  32bit SCM error status reg
#define _reg_SCM_INT_MASK	(*((volatile unsigned long *)(MX2_IO_ADDRESS(SCM_BASE_ADDR+0x018))))  //  32bit SCM interrupt control reg
#define _reg_SCM_CONFIGURATION	(*((volatile unsigned long *)(MX2_IO_ADDRESS(SCM_BASE_ADDR+0x01C))))  //  32bit SCM configuration
#define _reg_SCM_INIT_VECTOR_0	(*((volatile unsigned long *)(MX2_IO_ADDRESS(SCM_BASE_ADDR+0x020))))  //  32bit SCM initialization vector high
#define _reg_SCM_INIT_VECTOR_1	(*((volatile unsigned long *)(MX2_IO_ADDRESS(SCM_BASE_ADDR+0x024))))  //  32bit SCM initialization vector low
#define _reg_SCM_RED_MEM_BASE	(*((volatile unsigned long *)(MX2_IO_ADDRESS(SCM_BASE_ADDR+0x400))))  //  32bit Red memory regs (400 - 7FF)
#define _reg_SCM_BLACK_MEM_BASE	(*((volatile unsigned long *)(MX2_IO_ADDRESS(SCM_BASE_ADDR+0x800))))  //  32bit Black memory regs (800 - BFF) 
		
		
		
//#########################################		
//# CSI                                   #		
//# $8000_0000 to $8000_0FFF              #		
//#########################################		
#define CSI_BASE_ADDR	0x80000000	
#define _reg_CSI_CSICR1			(*((volatile unsigned long *)(MX2_IO_ADDRESS(CSI_BASE_ADDR+0x00))))  //  32bit csi control 1 reg
#define _reg_CSI_CSICR2			(*((volatile unsigned long *)(MX2_IO_ADDRESS(CSI_BASE_ADDR+0x04))))  //  32bit csi control 2 reg
#define _reg_CSI_CSISR			(*((volatile unsigned long *)(MX2_IO_ADDRESS(CSI_BASE_ADDR+0x08))))  //  32bit csi status reg
#define _reg_CSI_CSISTATR		(*((volatile unsigned long *)(MX2_IO_ADDRESS(CSI_BASE_ADDR+0x0C))))  //  32bit csi fifo statistics reg
#define _reg_CSI_CSIRXR			(*((volatile unsigned long *)(MX2_IO_ADDRESS(CSI_BASE_ADDR+0x10))))  //  32bit csi receive image reg
#define _reg_CSI_CSIRXCNT		(*((volatile unsigned long *)(MX2_IO_ADDRESS(CSI_BASE_ADDR+0x14))))  //  32bit csi receive count reg
		
		
		
//#########################################		
//# BMI                                   #		
//# $A000_0000 to $A000_0FFF              #		
//#########################################		
#define BMI_BASE_ADDR	0xA0000000

#define _reg_BMI_BMICTRL(x)	(*((volatile unsigned long *)(MX2_IO_ADDRESS(BMI_BASE_ADDR+0x00+4*(x-1)))))  //  32bit bmi control 1-2 reg

		#if 0	
		#define _reg_BMI_BMICTRL1	(BMI_BASE_ADDR+0x00)	//  32bit bmi control 1 reg
		#define _reg_BMI_BMICTRL2	(BMI_BASE_ADDR+0x04)	//  32bit bmi control 2 reg
		#endif
		
#define _reg_BMI_BMISTR		(*((volatile unsigned long *)(MX2_IO_ADDRESS(BMI_BASE_ADDR+0x08))))  //  32bit bmi status reg
#define _reg_BMI_BMIRXD		(*((volatile unsigned long *)(MX2_IO_ADDRESS(BMI_BASE_ADDR+0x0C))))  //  32bit bmi Rx FIFO reg
#define _reg_BMI_BMITXD		(*((volatile unsigned long *)(MX2_IO_ADDRESS(BMI_BASE_ADDR+0x10))))  //  32bit bmi Tx FIFO reg
		
		
//#########################################		
//# External Memory CSD0 CSD1    #		
//# $C000_0000 to $C3FF_FFFF (CSD0)       #		
//# $C400_0000 to $C7FF_FFFF (CSD1)       #		
//#########################################
#define CSD_BASE_ADDR(x)		(0xC0000000+0x04000000*x)	//  CS0 (64Mb)
#define CSD_END_ADDR(x)			(CSD_BASE_ADDR(x)+0x3FFFFFF)

		#if 0		
		#define CSD0_BASE_ADDR	0xC0000000	//  CSD0 (64Mb)
		#define CSD0_END_ADDR	(CSD0_BASE_ADDR+0x3FFFFFF)	
		#define CSD1_BASE_ADDR	0xC4000000	//  CSD1 (64Mb)
		#define CSD1_END_ADDR	(CSD1_BASE_ADDR+0x3FFFFFF)	
		#endif
		
//#########################################		
//# External Memory CS0 - CS5             #		
//# $C800_0000 to $CBFF_FFFF (CS0)        #		
//# $CC00_0000 to $CFFF_FFFF (CS1)        #		
//# $D000_0000 to $D0FF_FFFF (CS2)        #		
//# $D100_0000 to $D1FF_FFFF (CS3)        #		
//# $D200_0000 to $D2FF_FFFF (CS4)        #		
//# $D300_0000 to $D3FF_FFFF (CS5)        #		
//#########################################		



#define CS_BASE_ADDR(x)		(0xC8000000+0x04000000*x)	//  CS0 (64Mb)
#define CS_END_ADDR(x)		(CS_BASE_ADDR(x)+0x3FFFFFF)

		#if 0
		#define CS0_BASE_ADDR	0xC8000000	//  CS0 (64Mb)
		#define CS0_END_ADDR	(CS0_BASE_ADDR+0x3FFFFFF)	
		#define CS1_BASE_ADDR	0xCC000000	//  CS1 (64Mb)
		#define CS1_END_ADDR	(CS1_BASE_ADDR+0x3FFFFFF)	
		#define CS2_BASE_ADDR	0xD0000000	//  CS2 (16Mb)
		#define CS2_END_ADDR	(CS2_BASE_ADDR+0x0FFFFFF)	
		#define CS3_BASE_ADDR	0xD1000000	//  CS3 (16Mb)
		#define CS3_END_ADDR	(CS3_BASE_ADDR+0x0FFFFFF)	
		#define CS4_BASE_ADDR	0xD2000000	//  CS4 (16Mb)
		#define CS4_END_ADDR	(CS4_BASE_ADDR+0x0FFFFFF)	
		#define CS5_BASE_ADDR	0xD3000000	//  CS5 (16Mb)
		#define CS5_END_ADDR	(CS5_BASE_ADDR+0x0FFFFFF)	
		#endif		
		
		
//#########################################		
//# External Memory PCMCIA/CF             #		
//# $D400_0000 to $D7FF_FFFF              #		
//#########################################		
#define PCMCF_BASE_ADDR	0xD4000000	//  pcmcia/cf (64Mb)
#define PCMCF_END_ADDR	(PCMCF_BASE_ADDR+0x7FFFFFF)	
		
		
		
//#########################################		
//# SDRAMC                                #		
//# $DF00_0000 to $DF00_0FFF              #		
//#########################################		
#define SDRAMC_BASE_ADDR	0xDF000000

//Modified by Bill Chen,Dec 26th,2003...start
//We should not use MX2_IO_ADDRESS to map SDRAM control registers' IO address
//#define _reg_SDRAMC_SDCTL(x)	(*((volatile unsigned long *)(MX2_IO_ADDRESS(SDRAMC_BASE_ADDR+0x00+x*4))))  //  32bit sdram 0 control reg
//
//		#if 0	
//		#define SDRAMC_SDCTL0	(SDRAMC_BASE_ADDR+0x00)	//  32bit sdram 0 control reg
//		#define SDRAMC_SDCTL1	(SDRAMC_BASE_ADDR+0x04)	//  32bit sdram 1 control reg
//		#endif

//#define _reg_SDRAMC_MISC	(*((volatile unsigned long *)(MX2_IO_ADDRESS(SDRAMC_BASE_ADDR+0x14))))  //  32bit sdram miscellaneous reg
//#define _reg_SDRAMC_SDRST	(*((volatile unsigned long *)(MX2_IO_ADDRESS(SDRAMC_BASE_ADDR+0x18))))  //  32bit sdram reset reg
#define _reg_SDRAMC_SDCTL(x)	(*((volatile unsigned long *)(MX2ADS_EMI_IOBASE+0x00+(x)*4)))  //  32bit sdram 0 control reg

		#if 0	
		#define SDRAMC_SDCTL0	(SDRAMC_BASE_ADDR+0x00)	//  32bit sdram 0 control reg
		#define SDRAMC_SDCTL1	(SDRAMC_BASE_ADDR+0x04)	//  32bit sdram 1 control reg
		#endif

#define _reg_SDRAMC_MISC	(*((volatile unsigned long *)(MX2ADS_EMI_IOBASE+0x14)))  //  32bit sdram miscellaneous reg
#define _reg_SDRAMC_SDRST	(*((volatile unsigned long *)(MX2ADS_EMI_IOBASE+0x18)))  //  32bit sdram reset reg
//Modified by Bill Chen,Dec 26th,2003...end

//#########################################		
//# WEIM                                  #		
//# $DF00_1000 to $DF00_1FFF              #		
//#########################################		
#define WEIM_CS0	0
#define WEIM_CS1	1
#define WEIM_CS2	2
#define WEIM_CS3	3
#define WEIM_CS4	4
#define WEIM_CS5	5

#define _reg_WEIM_CSU(x)	(*((volatile unsigned long *)(MX2ADS_EMI_IOBASE+0x1000+8*x)))  //  32bit eim chip sel 0 upper ctr reg
#define _reg_WEIM_CSL(x)	(*((volatile unsigned long *)(MX2ADS_EMI_IOBASE+0x1000+0x04+8*x)))  //  32bit eim chip sel 0 lower ctr reg

		#if 0	
		#define WEIM_CS0U	(WEIM_BASE_ADDR+0x00)	//  32bit eim chip sel 0 upper ctr reg
		#define WEIM_CS0L	(WEIM_BASE_ADDR+0x04)	//  32bit eim chip sel 0 lower ctr reg
		#define WEIM_CS1U	(WEIM_BASE_ADDR+0x08)	//  32bit eim chip sel 1 upper ctr reg
		#define WEIM_CS1L	(WEIM_BASE_ADDR+0x0C)	//  32bit eim chip sel 1 lower ctr reg
		#define WEIM_CS2U	(WEIM_BASE_ADDR+0x10)	//  32bit eim chip sel 2 upper ctr reg
		#define WEIM_CS2L	(WEIM_BASE_ADDR+0x14)	//  32bit eim chip sel 2 lower ctr reg
		#define WEIM_CS3U	(WEIM_BASE_ADDR+0x18)	//  32bit eim chip sel 3 upper ctr reg
		#define WEIM_CS3L	(WEIM_BASE_ADDR+0x1C)	//  32bit eim chip sel 3 lower ctr reg
		#define WEIM_CS4U	(WEIM_BASE_ADDR+0x20)	//  32bit eim chip sel 4 upper ctr reg
		#define WEIM_CS4L	(WEIM_BASE_ADDR+0x24)	//  32bit eim chip sel 4 lower ctr reg
		#define WEIM_CS5U	(WEIM_BASE_ADDR+0x28)	//  32bit eim chip sel 5 upper ctr reg
		#define WEIM_CS5L	(WEIM_BASE_ADDR+0x2C)	//  32bit eim chip sel 5 upper ctr reg
		#define WEIM_EIM	(WEIM_BASE_ADDR+0x30)	//  32bit eim configuration reg
		#endif		
//#########################################		
//# PCMCIA                                #		
//# $DF00_2000 to $DF00_2FFF              #		
//#########################################		
#define PCMCIA_BASE_ADDR	0xDF002000	




#define _reg_PCMCIA_PIPR	(*((volatile unsigned long *)(PCMCIA_IO_ADDRESS(PCMCIA_BASE_ADDR+0x00))))  //  32bit pcmcia input pins reg
#define _reg_PCMCIA_PSCR	(*((volatile unsigned long *)(PCMICA_IO_ADDRESS(PCMCIA_BASE_ADDR+0x04))))  //  32bit pcmcia status change reg
#define _reg_PCMCIA_PER		(*((volatile unsigned long *)(PCMICA_IO_ADDRESS(PCMCIA_BASE_ADDR+0x08))))  //  32bit pcmcia enable reg

#define _reg_PCMCIA_PBR(x)	(*((volatile unsigned long *)(PCMCIA_IO_ADDRESS(PCMCIA_BASE_ADDR+0x0C+4*x))))  //  32bit pcmcia base reg 0
#define _reg_PCMCIA_POR0(x)	(*((volatile unsigned long *)(PCMCIA_IO_ADDRESS(PCMCIA_BASE_ADDR+0x28+4*x))))  //  32bit pcmcia option reg 0
#define _reg_PCMCIA_POFR(x)	(*((volatile unsigned long *)(PCMCIA_IO_ADDRESS(PCMCIA_BASE_ADDR+0x44+4*x))))  //  32bit pcmcia offset reg 0


		#if 0
		#define PCMCIA_PBR0	(PCMCIA_BASE_ADDR+0x0C)	//  32bit pcmcia base reg 0
		#define PCMCIA_PBR1	(PCMCIA_BASE_ADDR+0x10)	//  32bit pcmcia base reg 1
		#define PCMCIA_PBR2	(PCMCIA_BASE_ADDR+0x14)	//  32bit pcmcia base reg 2
		#define PCMCIA_PBR3	(PCMCIA_BASE_ADDR+0x18)	//  32bit pcmcia base reg 3
		#define PCMCIA_PBR4	(PCMCIA_BASE_ADDR+0x1C)	//  32bit pcmcia base reg 4
		#define PCMCIA_PBR5	(PCMCIA_BASE_ADDR+0x20)	//  32bit pcmcia base reg 5
		#define PCMCIA_PBR6	(PCMCIA_BASE_ADDR+0x24)	//  32bit pcmcia base reg 6
		#define PCMCIA_POR0	(PCMCIA_BASE_ADDR+0x28)	//  32bit pcmcia option reg 0
		#define PCMCIA_POR1	(PCMCIA_BASE_ADDR+0x2C)	//  32bit pcmcia option reg 1
		#define PCMCIA_POR2	(PCMCIA_BASE_ADDR+0x30)	//  32bit pcmcia option reg 2
		#define PCMCIA_POR3	(PCMCIA_BASE_ADDR+0x34)	//  32bit pcmcia option reg 3
		#define PCMCIA_POR4	(PCMCIA_BASE_ADDR+0x38)	//  32bit pcmcia option reg 4
		#define PCMCIA_POR5	(PCMCIA_BASE_ADDR+0x3C)	//  32bit pcmcia option reg 5
		#define PCMCIA_POR6	(PCMCIA_BASE_ADDR+0x40)	//  32bit pcmcia option reg 6
		#define PCMCIA_POFR0	(PCMCIA_BASE_ADDR+0x44)	//  32bit pcmcia offset reg 0
		#define PCMCIA_POFR1	(PCMCIA_BASE_ADDR+0x48)	//  32bit pcmcia offset reg 1
		#define PCMCIA_POFR2	(PCMCIA_BASE_ADDR+0x4C)	//  32bit pcmcia offset reg 2
		#define PCMCIA_POFR3	(PCMCIA_BASE_ADDR+0x50)	//  32bit pcmcia offset reg 3
		#define PCMCIA_POFR4	(PCMCIA_BASE_ADDR+0x54)	//  32bit pcmcia offset reg 4
		#define PCMCIA_POFR5	(PCMCIA_BASE_ADDR+0x58)	//  32bit pcmcia offset reg 5
		#define PCMCIA_POFR6	(PCMCIA_BASE_ADDR+0x5C)	//  32bit pcmcia offset reg 6
		#endif

#define _reg_PCMCIA_PGCR	(*((volatile unsigned long *)(PCMCIA_IO_ADDRESS(PCMCIA_BASE_ADDR+0x60))))  //  32bit pcmcia general control reg
#define _reg_PCMCIA_PGSR	(*((volatile unsigned long *)(PCMCIA_IO_ADDRESS(PCMCIA_BASE_ADDR+0x64))))  //  32bit pcmcia general status reg
		
//#########################################		
//# NFC                                   #		
//# $DF00_3000 to $DF00_3FFF              #		
//#########################################		
#define NFC_BASE_ADDR	0xDF003000	

#define NFC_MAB_BASE(x)		(NFC_BASE_ADDR+0x000+0x200*x)	//  main area buffer0 (3000 - 31FE)
#define NFC_SAB_BASE(x)		(NFC_BASE_ADDR+0x800+0x10*x)	//  spare area buffer0 (3800 - 380E)
#define NFC_SAB_8BIT_BASE(x)	(NFC_BASE_ADDR+0x800+0x10*x)	//  spare area buffer0 for 8 bit(3800 - 380E)
#define NFC_SAB_16BIT_BASE(x)	(NFC_BASE_ADDR+0x800+0x10*x)	//  spare area buffer0 for 16 bit(3800 - 380E)

		#if 0		
		#define NFC_MAB0_BASE	(NFC_BASE_ADDR+0x000)	//  main area buffer0 (3000 - 31FE)
		#define NFC_MAB1_BASE	(NFC_BASE_ADDR+0x200)	//  main area buffer1 (3200 - 33FE)
		#define NFC_MAB2_BASE	(NFC_BASE_ADDR+0x400)	//  main area buffer2 (3400 - 35FE)
		#define NFC_MAB3_BASE	(NFC_BASE_ADDR+0x600)	//  main area buffer3 (3600 - 37FE)
				
		#define NFC_SAB0_BASE	(NFC_BASE_ADDR+0x800)	//  spare area buffer0 (3800 - 380E)
		#define NFC_SAB1_BASE	(NFC_BASE_ADDR+0x810)	//  spare area buffer1 (3810 - 381E)
		#define NFC_SAB2_BASE	(NFC_BASE_ADDR+0x820)	//  spare area buffer2 (3820 - 382E)
		#define NFC_SAB3_BASE	(NFC_BASE_ADDR+0x830)	//  spare area buffer3 (3830 - 383E)
				
		#define NFC_SAB0_8BIT_BASE	(NFC_BASE_ADDR+0x800)	//  spare area buffer0 for 8 bit(3800 - 380E)
		#define NFC_SAB1_8BIT_BASE	(NFC_BASE_ADDR+0x810)	//  spare area buffer1 for 8 bit(3810 - 381E)
		#define NFC_SAB2_8BIT_BASE	(NFC_BASE_ADDR+0x820)	//  spare area buffer2 for 8 bit(3820 - 382E)
		#define NFC_SAB3_8BIT_BASE	(NFC_BASE_ADDR+0x830)	//  spare area buffer3 for 8 bit(3830 - 383E)
				
		#define NFC_SAB0_16BIT_BASE	(NFC_BASE_ADDR+0x800)	//  spare area buffer0 for 16 bit(3800 - 380E)
		#define NFC_SAB1_16BIT_BASE	(NFC_BASE_ADDR+0x810)	//  spare area buffer1 for 16 bit(3810 - 381E)
		#define NFC_SAB2_16BIT_BASE	(NFC_BASE_ADDR+0x820)	//  spare area buffer2 for 16 bit(3820 - 382E)
		#define NFC_SAB3_16BIT_BASE	(NFC_BASE_ADDR+0x830)	//  spare area buffer3 for 16 bit(3830 - 383E)
		

		
        #define NFC_REG_BASE		(NFC_BASE_ADDR+0xE00)	//  register area (3E00 - 3E1C)
        #define NFC_BUFSIZE		    (NFC_REG_BASE+0x00)	//  16bit nfc internal sram size
        #define NFC_BLK_ADD_LOCK	(NFC_REG_BASE+0x02)	//  16bit nfc block addr for lock chk
        #define NFC_RAM_BUF_ADDR	(NFC_REG_BASE+0x04)	//  16bit nfc buffer number
        #define NFC_NAND_FLASH_ADDR	(NFC_REG_BASE+0x06)	//  16bit nfc nand flash address
        #define NFC_NAND_FLASH_CMD	(NFC_REG_BASE+0x08)	//  16bit nfc nand flash command
        #define NFC_CONFIGURATION	(NFC_REG_BASE+0x0A)	//  16bit nfc internal buf lock ctrl
        #define NFC_ECC_STAT_RES	(NFC_REG_BASE+0x0C)	//  16bit nfc controller status/result
        #define NFC_ECC_RSLT_MA		(NFC_REG_BASE+0x0E)	//  16bit nfc ecc err position in main
        #define NFC_ECC_RSLT_SA		(NFC_REG_BASE+0x10)	//  16bit nfc ecc err pos in spare
        #define NFC_NF_WR_PROT		(NFC_REG_BASE+0x12)	//  16bit nfc write protection
        #define NFC_ULOCK_START_BLK	(NFC_REG_BASE+0x14)	//  16bit nfc start unlock location
        #define NFC_ULOCK_END_BLK	(NFC_REG_BASE+0x16)	//  16bit nfc end unlock location
        #define NFC_NF_WR_PROT_STAT	(NFC_REG_BASE+0x18)	//  16bit nfc write protection status
        #define NFC_NF_CONFIG1		(NFC_REG_BASE+0x1A)	//  16bit nfc configuration 1
        #define NFC_NF_CONFIG2		(NFC_REG_BASE+0x1C)	//  16bit nfc configuration 2
        #endif              
        
#define NFC_REG_BASE                          (NFC_BASE_ADDR+0xE00)	//  register area (3E00 - 3E1C)   
#define _reg_NFC_BUFSIZE		      (*((volatile unsigned short *)(NFC_MX2_IO_ADDRESS(NFC_REG_BASE+0x00))))	//  16bit nfc internal sram size      
#define _reg_NFC_BLK_ADD_LOCK	              (*((volatile unsigned short *)(NFC_MX2_IO_ADDRESS(NFC_REG_BASE+0x02))))	//  16bit nfc block addr for lock chk 
#define _reg_NFC_RAM_BUF_ADDR	              (*((volatile unsigned short *)(NFC_MX2_IO_ADDRESS(NFC_REG_BASE+0x04))))	//  16bit nfc buffer number           
#define _reg_NFC_NAND_FLASH_ADDR              (*((volatile unsigned short *)(NFC_MX2_IO_ADDRESS(NFC_REG_BASE+0x06))))	//  16bit nfc nand flash address      
#define _reg_NFC_NAND_FLASH_CMD	              (*((volatile unsigned short *)(NFC_MX2_IO_ADDRESS(NFC_REG_BASE+0x08))))	//  16bit nfc nand flash command      
#define _reg_NFC_CONFIGURATION	              (*((volatile unsigned short *)(NFC_MX2_IO_ADDRESS(NFC_REG_BASE+0x0A))))	//  16bit nfc internal buf lock ctrl  
#define _reg_NFC_ECC_STAT_RES	              (*((volatile unsigned short *)(NFC_MX2_IO_ADDRESS(NFC_REG_BASE+0x0C))))	//  16bit nfc controller status/result
#define _reg_NFC_ECC_RSLT_MA		      (*((volatile unsigned short *)(NFC_MX2_IO_ADDRESS(NFC_REG_BASE+0x0E))))	//  16bit nfc ecc err position in main
#define _reg_NFC_ECC_RSLT_SA		      (*((volatile unsigned short *)(NFC_MX2_IO_ADDRESS(NFC_REG_BASE+0x10))))	//  16bit nfc ecc err pos in spare    
#define _reg_NFC_NF_WR_PROT		      (*((volatile unsigned short *)(NFC_MX2_IO_ADDRESS(NFC_REG_BASE+0x12))))	//  16bit nfc write protection        
#define _reg_NFC_ULOCK_START_BLK	      (*((volatile unsigned short *)(NFC_MX2_IO_ADDRESS(NFC_REG_BASE+0x14))))	//  16bit nfc start unlock location   
#define _reg_NFC_ULOCK_END_BLK	              (*((volatile unsigned short *)(NFC_MX2_IO_ADDRESS(NFC_REG_BASE+0x16))))	//  16bit nfc end unlock location     
#define _reg_NFC_NF_WR_PROT_STAT	      (*((volatile unsigned short *)(NFC_MX2_IO_ADDRESS(NFC_REG_BASE+0x18))))	//  16bit nfc write protection status 
#define _reg_NFC_NF_CONFIG1		      (*((volatile unsigned short *)(NFC_MX2_IO_ADDRESS(NFC_REG_BASE+0x1A))))	//  16bit nfc configuration 1         
#define _reg_NFC_NF_CONFIG2		      (*((volatile unsigned short *)(NFC_MX2_IO_ADDRESS(NFC_REG_BASE+0x1C))))	//  16bit nfc configuration 2         

#define NFC_MAB0_BASE	(NFC_MX2_IO_ADDRESS(NFC_BASE_ADDR+0x000))	//  main area buffer0 (3000 - 31FE)
#define NFC_MAB1_BASE	(NFC_MX2_IO_ADDRESS(NFC_BASE_ADDR+0x200))	//  main area buffer1 (3200 - 33FE)
#define NFC_MAB2_BASE	(NFC_MX2_IO_ADDRESS(NFC_BASE_ADDR+0x400))	//  main area buffer2 (3400 - 35FE)
#define NFC_MAB3_BASE	(NFC_MX2_IO_ADDRESS(NFC_BASE_ADDR+0x600))	//  main area buffer3 (3600 - 37FE)
		
#define NFC_SAB0_BASE	(NFC_MX2_IO_ADDRESS(NFC_BASE_ADDR+0x800))	//  spare area buffer0 (3800 - 380E)
#define NFC_SAB1_BASE	(NFC_MX2_IO_ADDRESS(NFC_BASE_ADDR+0x810))	//  spare area buffer1 (3810 - 381E)
#define NFC_SAB2_BASE	(NFC_MX2_IO_ADDRESS(NFC_BASE_ADDR+0x820))	//  spare area buffer2 (3820 - 382E)
#define NFC_SAB3_BASE	(NFC_MX2_IO_ADDRESS(NFC_BASE_ADDR+0x830))	//  spare area buffer3 (3830 - 383E)
	
#endif
