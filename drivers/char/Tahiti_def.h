/******************************************************************************

 C   H E A D E R   F I L E

 (c) Copyright Motorola Semiconductors Hong Kong Limited 2002-2003
 ALL RIGHTS RESERVED

*******************************************************************************

 Project Name : Tahiti Bootstrap
 Project No.  : 
 Title        : 
 Template Ver : 0.3
 File Name    : Tahiti_def.h  
 Last Modified: Feb 21, 2003

 Description  : Definition header for DBMX2 silicon. 

 Assumptions  : NA
 
 Dependency Comments : NA

 Project Specific Data : NA

******************************************************************************/

#ifndef MX1_DEF_INC
#define MX1_DEF_INC


/*************************** Header File Includes ****************************/

//;#########################################			
//;# AIPI1                                 #			
//;# $1000_0000 to $1000_0FFF              #			
//;#########################################			
#define AIPI1_BASE_ADDR	      	0x10000000	
#define AIPI1_PSR0	      	(AIPI1_BASE_ADDR+0x00)	//  32bit Peripheral Size Reg 0
#define AIPI1_PSR1	     	(AIPI1_BASE_ADDR+0x04)	//  32bit Peripheral Size Reg 1
#define AIPI1_PAR	      	(AIPI1_BASE_ADDR+0x08)	//  32bit Peripheral Access Reg

#define BIR_115200	1151
#define BIR_57600	575
     
//
// ;---------------------------------------;
// ; UART1                                 ;
// ; $1000A000 to $1000_A0FF             ;
// ;---------------------------------------;
#define UART1_BASE_ADDR         0x1000A000                
#define UART1_RXDATA            UART1_BASE_ADDR                
#define UART1_TXDATA            (UART1_BASE_ADDR+0x40)                
#define UART1_CR1               (UART1_BASE_ADDR+0x80)                
#define UART1_CR2               (UART1_BASE_ADDR+0x84)                
#define UART1_CR3               (UART1_BASE_ADDR+0x88)                
#define UART1_CR4               (UART1_BASE_ADDR+0x8C)                
#define UART1_FCR               (UART1_BASE_ADDR+0x90)                
#define UART1_SR1               (UART1_BASE_ADDR+0x94)                
#define UART1_SR2               (UART1_BASE_ADDR+0x98)                
#define UART1_ESC               (UART1_BASE_ADDR+0x9C)                
#define UART1_TIM               (UART1_BASE_ADDR+0xA0)                
#define UART1_BIR               (UART1_BASE_ADDR+0xA4)                
#define UART1_BMR               (UART1_BASE_ADDR+0xA8)                
#define UART1_BRC               (UART1_BASE_ADDR+0xAC)                
#define UART1_ONEMS             (UART1_BASE_ADDR+0xB0)                
#define UART1_TS	            (UART1_BASE_ADDR+0xB4) 





#define UART2_BASE_ADDR         0x1000b000                
#define UART2_RXDATA            UART2_BASE_ADDR                
#define UART2_TXDATA            (UART2_BASE_ADDR+0x40)                
#define UART2_CR1               (UART2_BASE_ADDR+0x80)                
#define UART2_CR2               (UART2_BASE_ADDR+0x84)                
#define UART2_CR3               (UART2_BASE_ADDR+0x88)                
#define UART2_CR4               (UART2_BASE_ADDR+0x8C)                
#define UART2_FCR               (UART2_BASE_ADDR+0x90)                
#define UART2_SR1               (UART2_BASE_ADDR+0x94)                
#define UART2_SR2               (UART2_BASE_ADDR+0x98)                
#define UART2_ESC               (UART2_BASE_ADDR+0x9C)                
#define UART2_TIM               (UART2_BASE_ADDR+0xA0)                
#define UART2_BIR               (UART2_BASE_ADDR+0xA4)                
#define UART2_BMR               (UART2_BASE_ADDR+0xA8)                
#define UART2_BRC               (UART2_BASE_ADDR+0xAC)                
#define UART2_ONEMS             (UART2_BASE_ADDR+0xB0)                
#define UART2_TS	            (UART2_BASE_ADDR+0xB4) 



#define UART3_BASE_ADDR         0x1000c000                
#define UART3_RXDATA            UART3_BASE_ADDR                
#define UART3_TXDATA            (UART3_BASE_ADDR+0x40)                
#define UART3_CR1               (UART3_BASE_ADDR+0x80)                
#define UART3_CR2               (UART3_BASE_ADDR+0x84)                
#define UART3_CR3               (UART3_BASE_ADDR+0x88)                
#define UART3_CR4               (UART3_BASE_ADDR+0x8C)                
#define UART3_FCR               (UART3_BASE_ADDR+0x90)                
#define UART3_SR1               (UART3_BASE_ADDR+0x94)                
#define UART3_SR2               (UART3_BASE_ADDR+0x98)                
#define UART3_ESC               (UART3_BASE_ADDR+0x9C)                
#define UART3_TIM               (UART3_BASE_ADDR+0xA0)                
#define UART3_BIR               (UART3_BASE_ADDR+0xA4)                
#define UART3_BMR               (UART3_BASE_ADDR+0xA8)                
#define UART3_BRC               (UART3_BASE_ADDR+0xAC)                
#define UART3_ONEMS             (UART3_BASE_ADDR+0xB0)                
#define UART3_TS	        (UART3_BASE_ADDR+0xB4) 



#define UART4_BASE_ADDR         0x1000d000                
#define UART4_RXDATA            UART4_BASE_ADDR                
#define UART4_TXDATA            (UART4_BASE_ADDR+0x40)                
#define UART4_CR1               (UART4_BASE_ADDR+0x80)                
#define UART4_CR2               (UART4_BASE_ADDR+0x84)                
#define UART4_CR3               (UART4_BASE_ADDR+0x88)                
#define UART4_CR4               (UART4_BASE_ADDR+0x8C)                
#define UART4_FCR               (UART4_BASE_ADDR+0x90)                
#define UART4_SR1               (UART4_BASE_ADDR+0x94)                
#define UART4_SR2               (UART4_BASE_ADDR+0x98)                
#define UART4_ESC               (UART4_BASE_ADDR+0x9C)                
#define UART4_TIM               (UART4_BASE_ADDR+0xA0)                
#define UART4_BIR               (UART4_BASE_ADDR+0xA4)                
#define UART4_BMR               (UART4_BASE_ADDR+0xA8)                
#define UART4_BRC               (UART4_BASE_ADDR+0xAC)                
#define UART4_ONEMS             (UART4_BASE_ADDR+0xB0)                
#define UART4_TS	        (UART4_BASE_ADDR+0xB4) 













//------------------------------------------
//System Control
//------------------------------------------

#define SYS_BASE_ADDR	      	0x10027800	
#define SYS_SIDR	      	(SYS_BASE_ADDR+0x10)	//;  128bit Silicon ID Reg
#define SYS_FMCR	      	(SYS_BASE_ADDR+0x14)	//;  Functional Muxing Control Reg
#define SYS_GPCR	      	(SYS_BASE_ADDR+0x18)	//;  Global Peripheral Control Reg
#define SYS_WBCR	      	(SYS_BASE_ADDR+0x1C)	//;  Well Bias Control Reg
#define SYS_DSCR1	      	(SYS_BASE_ADDR+0x20)	//;  Drive Strength Crtl Reg 1
#define SYS_DSCR2	      	(SYS_BASE_ADDR+0x24)	//;  Drive Strength Crtl Reg 2
#define SYS_DSCR3	      	(SYS_BASE_ADDR+0x28)	//;  Drive Strength Crtl Reg 3
#define SYS_DSCR4	      	(SYS_BASE_ADDR+0x2C)	//;  Drive Strength Crtl Reg 4
#define SYS_DSCR5	      	(SYS_BASE_ADDR+0x30)	//;  Drive Strength Crtl Reg 5
#define SYS_DSCR6	      	(SYS_BASE_ADDR+0x34)	//;  Drive Strength Crtl Reg 6
#define SYS_DSCR7	      	(SYS_BASE_ADDR+0x38)	//;  Drive Strength Crtl Reg 7
#define SYS_DSCR8	      	(SYS_BASE_ADDR+0x3C)	//;  Drive Strength Crtl Reg 8
#define SYS_DSCR9	      	(SYS_BASE_ADDR+0x40)	//;  Drive Strength Crtl Reg 9
#define SYS_DSCR10	      	(SYS_BASE_ADDR+0x44)	//;  Drive Strength Crtl Reg 10
#define SYS_DSCR11	      	(SYS_BASE_ADDR+0x48)	//;  Drive Strength Crtl Reg 11
#define SYS_DSCR12	      	(SYS_BASE_ADDR+0x4C)	//;  Drive Strength Crtl Reg 12
#define SYS_PSCR	      	(SYS_BASE_ADDR+0x50)	//;  Priority Control/select Reg



//;#########################################			
//;# #define GPIO                          #			
//;# $1001_5000 to $1001_5FFF              #			
//;#########################################			
#define GPIOA_BASE_ADDR	       	0x10015000	
#define GPIOA_DDIR	      (GPIOA_BASE_ADDR+0x00)   //  32bit gpio pta data direction reg
#define GPIOA_OCR1	      (GPIOA_BASE_ADDR+0x04)   //  32bit gpio pta output config 1 reg
#define GPIOA_OCR2	      (GPIOA_BASE_ADDR+0x08)   //  32bit gpio pta output config 2 reg
#define GPIOA_ICONFA1	      (GPIOA_BASE_ADDR+0x0C)   //  32bit gpio pta input config A1 reg
#define GPIOA_ICONFA2	      (GPIOA_BASE_ADDR+0x10)   //  32bit gpio pta input config A2 reg
#define GPIOA_ICONFB1	      (GPIOA_BASE_ADDR+0x14)   //  32bit gpio pta input config B1 reg
#define GPIOA_ICONFB2	      (GPIOA_BASE_ADDR+0x18)   //  32bit gpio pta input config B2 reg
#define GPIOA_DR	      (GPIOA_BASE_ADDR+0x1C)   //  32bit gpio pta data reg
#define GPIOA_GIUS	      (GPIOA_BASE_ADDR+0x20)   //  32bit gpio pta in use reg
#define GPIOA_SSR	      (GPIOA_BASE_ADDR+0x24)   //  32bit gpio pta sample status reg
#define GPIOA_ICR1	      (GPIOA_BASE_ADDR+0x28)   //  32bit gpio pta interrupt ctrl 1 reg
#define GPIOA_ICR2	      (GPIOA_BASE_ADDR+0x2C)   //  32bit gpio pta interrupt ctrl 2 reg
#define GPIOA_IMR	      (GPIOA_BASE_ADDR+0x30)   //  32bit gpio pta interrupt mask reg
#define GPIOA_ISR	      (GPIOA_BASE_ADDR+0x34)   //  32bit gpio pta interrupt status reg
#define GPIOA_GPR	      (GPIOA_BASE_ADDR+0x38)   //  32bit gpio pta general purpose reg
#define GPIOA_SWR	      (GPIOA_BASE_ADDR+0x3C)   //  32bit gpio pta software reset reg
#define GPIOA_PUEN	      (GPIOA_BASE_ADDR+0x40)   //  32bit gpio pta pull up enable reg
			
#define GPIOB_BASE_ADDR	       	0x10015100	
#define GPIOB_DDIR	      (GPIOB_BASE_ADDR+0x00)   //  32bit gpio ptb data direction reg
#define GPIOB_OCR1	      (GPIOB_BASE_ADDR+0x04)   //  32bit gpio ptb output config 1 reg
#define GPIOB_OCR2	      (GPIOB_BASE_ADDR+0x08)   //  32bit gpio ptb output config 2 reg
#define GPIOB_ICONFA1	      (GPIOB_BASE_ADDR+0x0C)   //  32bit gpio ptb input config A1 reg
#define GPIOB_ICONFA2	      (GPIOB_BASE_ADDR+0x10)   //  32bit gpio ptb input config A2 reg
#define GPIOB_ICONFB1	      (GPIOB_BASE_ADDR+0x14)   //  32bit gpio ptb input config B1 reg
#define GPIOB_ICONFB2	      (GPIOB_BASE_ADDR+0x18)   //  32bit gpio ptb input config B2 reg
#define GPIOB_DR	      (GPIOB_BASE_ADDR+0x1C)   //  32bit gpio ptb data reg
#define GPIOB_GIUS	      (GPIOB_BASE_ADDR+0x20)   //  32bit gpio ptb in use reg
#define GPIOB_SSR	      (GPIOB_BASE_ADDR+0x24)   //  32bit gpio ptb sample status reg
#define GPIOB_ICR1	      (GPIOB_BASE_ADDR+0x28)   //  32bit gpio ptb interrupt ctrl 1 reg
#define GPIOB_ICR2	      (GPIOB_BASE_ADDR+0x2C)   //  32bit gpio ptb interrupt ctrl 2 reg
#define GPIOB_IMR	      (GPIOB_BASE_ADDR+0x30)   //  32bit gpio ptb interrupt mask reg
#define GPIOB_ISR	      (GPIOB_BASE_ADDR+0x34)   //  32bit gpio ptb interrupt status reg
#define GPIOB_GPR	      (GPIOB_BASE_ADDR+0x38)   //  32bit gpio ptb general purpose reg
#define GPIOB_SWR	      (GPIOB_BASE_ADDR+0x3C)   //  32bit gpio ptb software reset reg 
#define GPIOB_PUEN	      (GPIOB_BASE_ADDR+0x40)   //  32bit gpio ptb pull up enable reg
			
#define GPIOC_BASE_ADDR	       	0x10015200	
#define GPIOC_DDIR	      (GPIOC_BASE_ADDR+0x00)   //  32bit gpio ptc data direction reg
#define GPIOC_OCR1	      (GPIOC_BASE_ADDR+0x04)   //  32bit gpio ptc output config 1 reg
#define GPIOC_OCR2	      (GPIOC_BASE_ADDR+0x08)   //  32bit gpio ptc output config 2 reg
#define GPIOC_ICONFA1	      (GPIOC_BASE_ADDR+0x0C)   //  32bit gpio ptc input config A1 reg
#define GPIOC_ICONFA2	      (GPIOC_BASE_ADDR+0x10)   //  32bit gpio ptc input config A2 reg
#define GPIOC_ICONFB1	      (GPIOC_BASE_ADDR+0x14)   //  32bit gpio ptc input config B1 reg
#define GPIOC_ICONFB2	      (GPIOC_BASE_ADDR+0x18)   //  32bit gpio ptc input config B2 reg
#define GPIOC_DR	      (GPIOC_BASE_ADDR+0x1C)   //  32bit gpio ptc data reg
#define GPIOC_GIUS	      (GPIOC_BASE_ADDR+0x20)   //  32bit gpio ptc in use reg
#define GPIOC_SSR	      (GPIOC_BASE_ADDR+0x24)   //  32bit gpio ptc sample status reg
#define GPIOC_ICR1	      (GPIOC_BASE_ADDR+0x28)   //  32bit gpio ptc interrupt ctrl 1 reg
#define GPIOC_ICR2	      (GPIOC_BASE_ADDR+0x2C)   //  32bit gpio ptc interrupt ctrl 2 reg
#define GPIOC_IMR	      (GPIOC_BASE_ADDR+0x30)   //  32bit gpio ptc interrupt mask reg
#define GPIOC_ISR	      (GPIOC_BASE_ADDR+0x34)   //  32bit gpio ptc interrupt status reg
#define GPIOC_GPR	      (GPIOC_BASE_ADDR+0x38)   //  32bit gpio ptc general purpose reg
#define GPIOC_SWR	      (GPIOC_BASE_ADDR+0x3C)   //  32bit gpio ptc software reset reg 
#define GPIOC_PUEN	      (GPIOC_BASE_ADDR+0x40)   //  32bit gpio ptc pull up enable reg
			
#define GPIOD_BASE_ADDR	       	0x10015300	
#define GPIOD_DDIR	      (GPIOD_BASE_ADDR+0x00)   //  32bit gpio ptd data direction reg
#define GPIOD_OCR1	      (GPIOD_BASE_ADDR+0x04)   //  32bit gpio ptd output config 1 reg
#define GPIOD_OCR2	      (GPIOD_BASE_ADDR+0x08)   //  32bit gpio ptd output config 2 reg
#define GPIOD_ICONFA1	      (GPIOD_BASE_ADDR+0x0C)   //  32bit gpio ptd input config A1 reg
#define GPIOD_ICONFA2	      (GPIOD_BASE_ADDR+0x10)   //  32bit gpio ptd input config A2 reg
#define GPIOD_ICONFB1	      (GPIOD_BASE_ADDR+0x14)   //  32bit gpio ptd input config B1 reg
#define GPIOD_ICONFB2	      (GPIOD_BASE_ADDR+0x18)   //  32bit gpio ptd input config B2 reg
#define GPIOD_DR	      (GPIOD_BASE_ADDR+0x1C)   //  32bit gpio ptd data reg
#define GPIOD_GIUS	      (GPIOD_BASE_ADDR+0x20)   //  32bit gpio ptd in use reg
#define GPIOD_SSR	      (GPIOD_BASE_ADDR+0x24)   //  32bit gpio ptd sample status reg
#define GPIOD_ICR1	      (GPIOD_BASE_ADDR+0x28)   //  32bit gpio ptd interrupt ctrl 1 reg
#define GPIOD_ICR2	      (GPIOD_BASE_ADDR+0x2C)   //  32bit gpio ptd interrupt ctrl 2 reg
#define GPIOD_IMR	      (GPIOD_BASE_ADDR+0x30)   //  32bit gpio ptd interrupt mask reg
#define GPIOD_ISR	      (GPIOD_BASE_ADDR+0x34)   //  32bit gpio ptd interrupt status reg
#define GPIOD_GPR	      (GPIOD_BASE_ADDR+0x38)   //  32bit gpio ptd general purpose reg
#define GPIOD_SWR	      (GPIOD_BASE_ADDR+0x3C)   //  32bit gpio ptd software reset reg 
#define GPIOD_PUEN	      (GPIOD_BASE_ADDR+0x40)   //  32bit gpio ptd pull up enable reg
			
#define GPIOE_BASE_ADDR	       	0x10015400	
#define GPIOE_DDIR	      (GPIOE_BASE_ADDR+0x00)   //  32bit gpio pte data direction reg
#define GPIOE_OCR1	      (GPIOE_BASE_ADDR+0x04)   //  32bit gpio pte output config 1 reg
#define GPIOE_OCR2	      (GPIOE_BASE_ADDR+0x08)   //  32bit gpio pte output config 2 reg
#define GPIOE_ICONFA1	      (GPIOE_BASE_ADDR+0x0C)   //  32bit gpio pte input config A1 reg
#define GPIOE_ICONFA2	      (GPIOE_BASE_ADDR+0x10)   //  32bit gpio pte input config A2 reg
#define GPIOE_ICONFB1	      (GPIOE_BASE_ADDR+0x14)   //  32bit gpio pte input config B1 reg
#define GPIOE_ICONFB2	      (GPIOE_BASE_ADDR+0x18)   //  32bit gpio pte input config B2 reg
#define GPIOE_DR	      (GPIOE_BASE_ADDR+0x1C)   //  32bit gpio pte data reg
#define GPIOE_GIUS	      (GPIOE_BASE_ADDR+0x20)   //  32bit gpio pte in use reg
#define GPIOE_SSR	      (GPIOE_BASE_ADDR+0x24)   //  32bit gpio pte sample status reg
#define GPIOE_ICR1	      (GPIOE_BASE_ADDR+0x28)   //  32bit gpio pte interrupt ctrl 1 reg
#define GPIOE_ICR2	      (GPIOE_BASE_ADDR+0x2C)   //  32bit gpio pte interrupt ctrl 2 reg
#define GPIOE_IMR	      (GPIOE_BASE_ADDR+0x30)   //  32bit gpio pte interrupt mask reg
#define GPIOE_ISR	      (GPIOE_BASE_ADDR+0x34)   //  32bit gpio pte interrupt status reg
#define GPIOE_GPR	      (GPIOE_BASE_ADDR+0x38)   //  32bit gpio pte general purpose reg
#define GPIOE_SWR	      (GPIOE_BASE_ADDR+0x3C)   //  32bit gpio pte software reset reg 
#define GPIOE_PUEN	      (GPIOE_BASE_ADDR+0x40)   //  32bit gpio pte pull up enable reg
			
#define GPIOF_BASE_ADDR	       	0x10015500	
#define GPIOF_DDIR	      (GPIOF_BASE_ADDR+0x00)   //  32bit gpio ptf data direction reg
#define GPIOF_OCR1	      (GPIOF_BASE_ADDR+0x04)   //  32bit gpio ptf output config 1 reg
#define GPIOF_OCR2	      (GPIOF_BASE_ADDR+0x08)   //  32bit gpio ptf output config 2 reg
#define GPIOF_ICONFA1	      (GPIOF_BASE_ADDR+0x0C)   //  32bit gpio ptf input config A1 reg
#define GPIOF_ICONFA2	      (GPIOF_BASE_ADDR+0x10)   //  32bit gpio ptf input config A2 reg
#define GPIOF_ICONFB1	      (GPIOF_BASE_ADDR+0x14)   //  32bit gpio ptf input config B1 reg
#define GPIOF_ICONFB2	      (GPIOF_BASE_ADDR+0x18)   //  32bit gpio ptf input config B2 reg
#define GPIOF_DR	      (GPIOF_BASE_ADDR+0x1C)   //  32bit gpio ptf data reg
#define GPIOF_GIUS	      (GPIOF_BASE_ADDR+0x20)   //  32bit gpio ptf in use reg
#define GPIOF_SSR	      (GPIOF_BASE_ADDR+0x24)   //  32bit gpio ptf sample status reg
#define GPIOF_ICR1	      (GPIOF_BASE_ADDR+0x28)   //  32bit gpio ptf interrupt ctrl 1 reg
#define GPIOF_ICR2	      (GPIOF_BASE_ADDR+0x2C)   //  32bit gpio ptf interrupt ctrl 2 reg
#define GPIOF_IMR	      (GPIOF_BASE_ADDR+0x30)   //  32bit gpio ptf interrupt mask reg
#define GPIOF_ISR	      (GPIOF_BASE_ADDR+0x34)   //  32bit gpio ptf interrupt status reg
#define GPIOF_GPR	      (GPIOF_BASE_ADDR+0x38)   //  32bit gpio ptf general purpose reg
#define GPIOF_SWR	      (GPIOF_BASE_ADDR+0x3C)   //  32bit gpio ptf software reset reg 
#define GPIOF_PUEN	      (GPIOF_BASE_ADDR+0x40)   //  32bit gpio ptf pull up enable reg
			
#define GPIO_REG_BASE	       	0x10015600	
#define GPIO_PMASK	      (GPIO_REG_BASE+0x00)   //  32bit gpio interrupt mask reg


//;#########################################			
//;# AIPI2                                 #			
//;# $1002_0000 to $1002_0FFF              #			
//;#########################################			
#define AIPI2_BASE_ADDR	      	0x10020000	
#define AIPI2_PSR0	      	(AIPI2_BASE_ADDR+0x00)	//  32bit Peripheral Size Reg 0
#define AIPI2_PSR1	      	(AIPI2_BASE_ADDR+0x04)	//  32bit Peripheral Size Reg 1
#define AIPI2_PAR	      	(AIPI2_BASE_ADDR+0x08)	//  32bit Peripheral Access Reg


// ;---------------------------------------;
// ; Clock & Reset (CRM)                   ;
// ; &1002_7000 to $1002_7020              ;
// ;---------------------------------------;
#define CRM_BASE_ADDR           0x10027000                
#define CRM_CSCR                CRM_BASE_ADDR           // ; Clock Source Control Reg
#define CRM_MPCTL0              (CRM_BASE_ADDR+0x04)    // ; MCU PLL Control Reg      
#define CRM_MPCTL1              (CRM_BASE_ADDR+0x08)  // ; MCU PLL & System Clk Ctl Reg
#define CRM_SPCTL0              (CRM_BASE_ADDR+0x0C)    // ; USB PLL Control Reg 0
#define CRM_SPCTL1              (CRM_BASE_ADDR+0x10)    // ; USB PLL Control Reg 1
#define CRM_OSC26MCTL			(CRM_BASE_ADDR+0x14)    // ; Oscillator 26M Register
#define CRM_PCDR                (CRM_BASE_ADDR+0x18)  	// ; Perpheral Clock Divider Reg
#define CRM_PCCR0				(CRM_BASE_ADDR+0x1C)    // ; Perpheral Clock Control Reg0
#define CRM_PCCR1 				(CRM_BASE_ADDR+0x20)    // ; Perpheral Clock Control Reg1





//;#########################################			                                                                                                                                                                                                                                                                                    
//;# MAX                                   #			                                                                                                                                                                                                                                                                                    
//;# $1003_F000 to $1003_FFFF              #			                                                                                                                                                                                                                                                                                    
//;#########################################			                                                                                                                                                                                                                                                                                    
#define MAX_BASE_ADDR	    0x1003F000	                                                                                                                                                                                                                                                                                                    
#define MAX_SLV0_BASE	    	 	(MAX_BASE_ADDR+0x000)	//  base location for slave 0                                                                                                                                                                                                                                                                
#define MAX_SLV1_BASE	    		(MAX_BASE_ADDR+0x100)	//  base location for slave 1                                                                                                                                                                                                                                                                
#define MAX_SLV2_BASE	    		(MAX_BASE_ADDR+0x200)	//  base location for slave 2                                                                                                                                                                                                                                                                
#define MAX_SLV3_BASE	    		(MAX_BASE_ADDR+0x300)	// base location for slave 3                                                                                                                                                                                                                                                                
			    		                                                                                                                                                                                                                                                                                                    
#define MAX_SLV0_MPR0	    		(MAX_SLV0_BASE+0x00)	// 32bit max slv0 master priority reg                                                                                                                                                                                                                                                       
#define MAX_SLV0_AMPR0	    		(MAX_SLV0_BASE+0x04)	// 32bit max slv0 alt priority reg                                                                                                                                                                                                                                                          
#define MAX_SLV0_SGPCR0	    		(MAX_SLV0_BASE+0x10)	// 32bit max slv0 general ctrl reg                                                                                                                                                                                                                                                          
#define MAX_SLV0_ASGPCR0	    	(MAX_SLV0_BASE+0x14)	// 32bit max slv0 alt generl ctrl reg                                                                                                                                                                                                                                               
			                                                                                                                                                                                                                                                                                                                            
#define MAX_SLV1_MPR1	    		(MAX_SLV1_BASE+0x00)	// 32bit max slv1 master priority reg                                                                                                                                                                                                                                                       
#define MAX_SLV1_AMPR1	    		(MAX_SLV1_BASE+0x04)	// 32bit max slv1 alt priority reg                                                                                                                                                                                                                                                          
#define MAX_SLV1_SGPCR1	    		(MAX_SLV1_BASE+0x10)	// 32bit max slv1 general ctrl reg                                                                                                                                                                                                                                                          
#define MAX_SLV1_ASGPCR1	    	(MAX_SLV1_BASE+0x14)	// 32bit max slv1 alt generl ctrl reg                                                                                                                                                                                                                                               
			                                                                                                                                                                                                                                                                                                                            
#define MAX_SLV2_MPR2	    		(MAX_SLV2_BASE+0x00)	// 32bit max slv2 master priority reg                                                                                                                                                                                                                                                       
#define MAX_SLV2_AMPR2	    		(MAX_SLV2_BASE+0x04)	// 32bit max slv2 alt priority reg                                                                                                                                                                                                                                                          
#define MAX_SLV2_SGPCR2	    		(MAX_SLV2_BASE+0x10)	// 32bit max slv2 general ctrl reg                                                                                                                                                                                                                                                          
#define MAX_SLV2_ASGPCR2	    	(MAX_SLV2_BASE+0x14)	// 32bit max slv2 alt generl ctrl reg                                                                                                                                                                                                                                               
			                                                                                                                                                                                                                                                                                                                            
#define MAX_SLV3_MPR3	    		(MAX_SLV3_BASE+0x00)	// 32bit max slv3 master priority reg                                                                                                                                                                                                                                                       
#define MAX_SLV3_AMPR3	    		(MAX_SLV3_BASE+0x04)	// 32bit max slv3 alt priority reg                                                                                                                                                                                                                                                          
#define MAX_SLV3_SGPCR3	    		(MAX_SLV3_BASE+0x10)	// 32bit max slv3 general ctrl reg                                                                                                                                                                                                                                                          
#define MAX_SLV3_ASGPCR3	    	(MAX_SLV3_BASE+0x14)	// 32bit max slv3 alt generl ctrl reg                                                                                                                                                                                                                                               
			                                                                                                                                                                                                                                                                                                                            
#define MAX_MST0_MGPCR0	    		(MAX_BASE_ADDR+0x800)	// 32bit max mst0 general ctrl reg                                                                                                                                                                                                                                                          
#define MAX_MST1_MGPCR1	    		(MAX_BASE_ADDR+0x900)	// 32bit max mst1 general ctrl reg                                                                                                                                                                                                                                                          
#define MAX_MST2_MGPCR2	    		(MAX_BASE_ADDR+0xA00)	// 32bit max mst2 general ctrl reg                                                                                                                                                                                                                                                          
#define MAX_MST3_MGPCR3	    		(MAX_BASE_ADDR+0xB00)	// 32bit max mst3 general ctrl reg                                                                                                                                                                                                                                                          
#define MAX_MST4_MGPCR4	    		(MAX_BASE_ADDR+0xC00)	// 32bit max mst4 general ctrl reg                                                                                                                                                                                                                                                          
#define MAX_MST5_MGPCR5	    		(MAX_BASE_ADDR+0xD00)	// 32bit max mst5 general ctrl reg                                                                                                                                                                                                                                                          
			                                                                                                                                                                                                                                                                                                                            


//
// ;---------------------------------------;
// ; WEIM                                  ;
// ; $DF00_1000 to $DF00_1030              ;
// ;---------------------------------------;
#define EIM_BASE_ADDR           0xDF001000                
#define EIM_CS0H                EIM_BASE_ADDR                
#define EIM_CS0L                (EIM_BASE_ADDR+0x04)                
#define EIM_CS1H                (EIM_BASE_ADDR+0x08)                
#define EIM_CS1L                (EIM_BASE_ADDR+0x0C)                
#define EIM_CS2H                (EIM_BASE_ADDR+0x10)                
#define EIM_CS2L                (EIM_BASE_ADDR+0x14)                
#define EIM_CS3H                (EIM_BASE_ADDR+0x18)                
#define EIM_CS3L                (EIM_BASE_ADDR+0x1C)                
#define EIM_CS4H                (EIM_BASE_ADDR+0x20)                
#define EIM_CS4L                (EIM_BASE_ADDR+0x24)                
#define EIM_CS5H                (EIM_BASE_ADDR+0x28)                
#define EIM_CS5L                (EIM_BASE_ADDR+0x2C)                
#define EIM                     (EIM_BASE_ADDR+0x30)


#ifdef FPGA

#define EUART_BASE_ADDR		0x17000000
#define EUART_DR			(EUART_BASE_ADDR+0x0)           
#define EUART_RSR			(EUART_BASE_ADDR+0x4)           
#define EUART_ECR			(EUART_BASE_ADDR+0x4)           
#define EUART_LCRH			(EUART_BASE_ADDR+0x8)           
#define EUART_LCRM			(EUART_BASE_ADDR+0xC)           
#define EUART_LCRL			(EUART_BASE_ADDR+0x10)    
#define EUART_CR			(EUART_BASE_ADDR+0x14)           
#define EUART_FR			(EUART_BASE_ADDR+0x18)  
#define EUART_IIR			(EUART_BASE_ADDR+0x1C)           
#define EUARTI_ICR			(EUART_BASE_ADDR+0x1C)   

#define LM_AIPI  0xC0000000
#define LM_SW	 0xC0001014 	
//#define LM_SW	 0xC0000014 	


#define LED_BASE		0x1A000000
#define	LED_ALPHA		(LED_BASE+0x0)
#define LED_LIGHTS		(LED_BASE+0x4)
#define LED_SWITCHES	(LED_BASE+0x8)

#endif

#endif
