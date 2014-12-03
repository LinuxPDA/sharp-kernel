/************************************************************************************/
/*  Brecis Communications                                 			    */
/*  										    */
/*      file Name:         	triadMap.h					    */
/*  Creation Date:		6/20/00  					    */
/*  Initial Enginer:            Phil Le						    */
/************************************************************************************/		
#ifndef _BRECIS_TRIAD_MAP_H
#define _BRECIS_TRIAD_MAP_H

#if 0
#define NONE_CACHEABLE			0xA0000000		/* physical address*/
#define CACHEABLE			0x80000000		/* physical address*/
#endif										

#define SYSTEM_MEM_BASE			0x000000 
#define SYSTEM_MEM_SIZE		        0x10000000		/* 256MByte */
#define FLASH_SIZE			0x200000

#ifdef _ASMLANGUAGE
/***************************/
/* 2 MByte Flash           */
/***************************/
#define TRIAD_FLASH_BASE				0xBFC00000		
/*******************************************************************/
/* System Logic Module Address Space                               */
/*******************************************************************/

/*******************************************************************/
/* MEMORY CONTROLER DEFINE                                         */
/*******************************************************************/

/****************************************************************************/
/* base address for Memory controller                                       */
/* bit  0           =    Numbanks          0 = 2  banks(only for 16Mb)      */
/*                                         1 = 4  banks(64Mb or higher      */
/*                                                                          */
/* bits [2:1]       =    Buswidth         00 = 4  bits wide                 */
/*                                        01 = 8  bits wide                 */
/*                                        10 = 16 bits wide                 */
/*                                        11 = 32 bits wide                 */
/*                                                                          */
/* bits [5:3]       =    BurstLength     010 = 4                            */
/*                                       011 = 8                            */
/*                                       111 = page mode                    */
/*                                                                          */
/* bit  [6]         =    Burst type      0 Only support Seq                 */
/*                                                                          */
/* bits [9:7]       =    Read latency    010 CAS latency 2                  */
/*                                       011 CAS latency 3                  */
/*                                                                          */
/* bit  11          =    Burst aligned   Burst aligned accesse. If this bit */
/*                                       set 1, then all the accessed o the */
/*                                       memory are always burst aligned.   */
/*                                       Auto precharge will be used in this*/
/*                                       Mode                               */
/* bit 12           = bus redirection    0 = Bus redirection time = 1 cycle */
/*                                       1 = Bus redirection time = 2 cycle */
/* bit [14:13]      = sdram Size        00 = 64Mb or 16Mb Sdram             */
/*                                      01 = 128Mb                          */
/*                                      10 = 256Mb                          */
/* bit [31:15,10]                       Reserved                            */ 
/****************************************************************************/ 	
#define TRIAD_MEM_CNTL_BASE				0xB7F00000		

#define TRIAD_MEM_CONF1_REG				(TRIAD_MEM_CNTL_BASE + 0x00)
#define TRIAD_MEM_CONF2_REG				(TRIAD_MEM_CNTL_BASE + 0x04)


#define MEM_CNTL_2BANKS                                 0x0
#define MEM_CNTL_4BANKS                                 0x1

#define MEM_CNTL_BUSW_4B                                0x0
#define MEM_CNTL_BUSW_8B                                (0x1  << 1)
#define MEM_CNTL_BUSW_16B                               (0x2  << 1)
#define MEM_CNTL_BUSW_32B                               (0x11 << 1)
 
#define MEM_CNTL_BURST_LEN4                             (0x2 << 3)
#define MEM_CNTL_BURST_LEN8                             (0x3 << 3)
#define MEM_CNTL_BURST_LEN_PAGE                         (0x7 << 3)

#define MEM_CNTL_BURST_TYPE                             (0x0 << 6)

#define MEM_CNTL_READ_LATENCY2                          (0x2 << 7)
#define MEM_CNTL_READ_LATENCY3                          (0x3 << 7)

#define MEM_CNTL_BURST_ALIGNED                          (0x1 << 11)
#define MEM_CNTL_BURST_NO_ALIGNED                       (0x0 << 11)

#define MEM_CNTL_BUS_DIR_1CYLCE                         (0x0 << 12)
#define MEM_CNTL_BUS_DIR_2CYLCE                         (0x1 << 12)

#define MEM_CNTL_SDRAM_SIZE_64MB                        (0x0 << 13)
#define MEM_CNTL_SDRAM_SIZE_128MB                       (0x1 << 13)
#define MEM_CNTL_SDRAM_SIZE_256MB                       (0x2 << 13)   

/*********************************************************************************/
/* The Following are the sequence to detect COL bits of Memory			 */
/*                                                                               */
/*********************************************************************************/
#define M256Mb64M4_REG1		0x00005199
#define M256Mb64M4_REG2		0x00002000

#define M256Mb32M8_REG1		0x0000519B
#define M256Mb32M8_REG2		0x00002000

#define M256Mb16M16_REG1	0x0000519D
#define M256Mb16M16_REG2	0x00002000

#define M128Mb32M4_REG1		0x00003199
#define M128Mb32M4_REG2		0x00001000

#define M128Mb16M8_REG1		0x0000319B
#define M128Mb16M8_REG2		0x00001000

#define M128Mb8M16_REG1		0x0000319D
#define M128Mb8M16_REG2		0x00001000
#define M64Mb16M4_REG1		0x00001199
#define M64Mb16M4_REG2		0x00001000

#define M64Mb8M8_REG1		0x0000119b
#define M64Mb8M8_REG2		0x00001000

#define M64Mb4M16_REG1		0x0000119d
#define M64Mb4M16_REG2		0x00001000
 

#define M64Mb2M32_REG1		0x0000119E
#define M64Mb2M32_REG2		0x00001000


#define M16Mb4M4_REG1		0x00001198
#define M16Mb4M4_REG2		0x00000800

#define M16Mb2M8_REG1		0x0000119a
#define M16Mb2M8_REG2		0x00000800

#define M16Mb1M16_REG1		0x0000119c
#define M16Mb1M16_REG2		0x00000800





#define M8MEG		0x800000
#define M16MEG		0x1000000
#define M64MEG		0x4000000
#define M128MEG		0x8000000
#define M256MEG		0x10000000







/***********************************************************************/
/* SLM TRIAD BLOCK ADDRESS DEFINE                                      */
/***********************************************************************/
/*******************************************/
/* TRIAD UART DEFINE                       */
/*******************************************/
#define TRIAD_UART_BASE		 0xBC000100    /* UART Base Address */
#define UART_BASE		 TRIAD_UART_BASE
/***********************************************************************/
/* BLOCK COPY ENGINE REGISTER                                          */
/***********************************************************************/
#define TRIAD_BCOPY_BASE         0xBC000120
/***********************************************************************/
/* Bcopy engine status registes R/W 0x1C000120                         */
/* Bits Define                                                         */
/*                                                                     */
/* Bit 0 Bcopy Busy        0 - Bcopy engine is idle                    */
/*                         1 - Bcopy engine is busy                    */
/*                                                                     */
/* Bit 1 Bcopy complete    0 - Bcopy engine Transfer not complete      */
/*                         1 - Bcopy engine Transfer complete          */
/*                                                                     */
/* Bit 2 Bcopy Error       0 - Bcopy engine Transfer no Error          */
/*                         1 - Bcopy engine Transfer Error             */
/***********************************************************************/

#define TRIAD_BCOPY_STATUS_REG      (TRIAD_BCOPY_BASE + 0x00)
#define BCOPY_STATUS_BUSY_IDLE             0x6
#define BCOPY_STATUS_BUSY_BUSY             0x1
#define BCOPY_STATUS_COMPLETE_NO           0x5
#define BCOPY_STATUS_COMPLETE_YET          0x2
#define BCOPY_STATUS_TRANSFER_NO_ERROR     0x2
#define BCOPY_STATUS_TRANSFER_ERROR        0x4

      
/***********************************************************************/
/* Bcopy engine Mode Control  registes R/W 0x1C000124                  */
/* Bits Define                                                         */
/*                                                                     */
/* Bit 0 Bcopy Start       0 - Stop Bcopy engine                       */
/*                         1 - Start Bcopy engine                      */
/*                                                                     */
/* Bit 1 Bcopy Mode        0 - Copy Mode (Source to Destination)       */
/*                         1 - Fill Mode (Fill Register to Destination */
/*                                                                     */
/* Bit 2 Interrupt         0 - Not enable                              */
/*                         1 - Enable on completion of transfer        */
/*                                                                     */
/* Bit 3 Source Add Mode   0 - Fix Source Address                      */
/*                         1 - Increase source address                 */
/*                                                                     */
/* Bit 4 Dest. Add Mode    0 - Fix Destination  Address                */
/*                         1 - Increase Destination address            */
/*                                                                     */
/***********************************************************************/
#define TRIAD_BCOPY_MODDE_CNTL_REG       (TRIAD_BCOPY_BASE + 0x04)

#define BCOPY_MODE_CNTL_START            0x01
#define BCOPY_NODE_CNTL_STOP             0xfffffffe

#define BCOPY_MODE_CNTL_FILL_MODE        0x2
#define BCOPY_MODE_CNTL_COPY_MODE        0xfffffffd

#define BCOPY_INTERRUPT_ENABLE           0x4
#define BCOPY_INTERRUPT_DISABLE          0xfffffffB

#define BCOPY_SOURCE_INC_MODE            0x8
#define BCOPY_SOURCE_FIX_MODE            0xfffffff7

#define BCOPY_DEST_INC_MODE              0x10 
#define BCOPY_DEST_FIX_MODE              0xffffffef

#define TRIAD_BCOPY_SOURCE_ADD_REG       (TRIAD_BCOPY_BASE + 0x8)
#define TRIAD_BCOPY_DEST_ADD_REG         (TRIAD_BCOPY_BASE + 0xC)

#define TRIAD_BCOPY_BYTE_COUNT_REG      (TRIAD_BCOPY_BASE + 0x10)
#define TRIAD_BCOPY_FILL_REG		(TRIAD_BCOPY_BASE + 0x14)

/****************************************************************/
/* BCopy Defination configuration value				*/
/*								*/
/****************************************************************/
/******************************************************/
/* Fill Mode, Destination Incrment, Interrupt Disable */
/******************************************************/
#define BC_FILL_DINC_NINT_MODE	(((BCOPY_MODE_CNTL_START | BCOPY_MODE_CNTL_FILL_MOD | 
				    BCOPY_DEST_INC_MODE)) & BCOPY_INTERRUPT_DISABLE)

/******************************************************/
/* Fill Mode, Destination Incrment, Interrupt Enable  */
/******************************************************/
#define BC_FILL_DINC_INT_MODE	 (BCOPY_MODE_CNTL_START | BCOPY_MODE_CNTL_FILL_MOD | 
			       	  BCOPY_DEST_INC_MODE | BCOPY_INTERRUPT_ENABLE)

/******************************************************/
/* Fill Mode, Destination Fix, Interrupt Disable      */
/******************************************************/
#define BC_FILL_DFIX_NINT_MODE	(((BCOPY_MODE_CNTL_START | BCOPY_MODE_CNTL_FILL_MOD)) & 
			       	  (BCOPY_INTERRUPT_DISABLE & BCOPY_DEST_FIX_MODE))

/******************************************************/
/* Fill Mode, Destination Fix, Interrupt Disable      */
/******************************************************/
#define BC_FILL_DFIX_INT_MODE  	 ((BCOPY_MODE_CNTL_START | BCOPY_MODE_CNTL_FILL_MOD | 
 			       	   BCOPY_INTERRUPT_ENABLE) & BCOPY_DEST_FIX_MODE)

/*****************************************************************************/
/* Copy Mode, Destination Increment, Source Increment Interrupt Disable      */
/*****************************************************************************/
#define BC_CP_DINC_SINC_NINT_MODE  ((BCOPY_MODE_CNTL_START | BCOPY_SOURCE_INC_MODE |
				    BCOPY_DEST_INC_MODE) & (BCOPY_MODE_CNTL_COPY_MODE & 
							    BCOPY_INTERRUPT_DISABLE))     

/*****************************************************************************/
/* Copy Mode, Destination Increment, Source Increment, Interrupt Enable      */
/*****************************************************************************/
#define BC_CP_DINC_SINC_NINT_MODE  ((BCOPY_MODE_CNTL_START | BCOPY_SOURCE_INC_MODE |
				    BCOPY_DEST_INC_MODE | BCOPY_INTERRUPT_ENABLE) & 
				    BCOPY_MODE_CNTL_COPY_MODE )

/*****************************************************************************/
/* Copy Mode, Destination Increment, Source Fix, Interrupt Disable           */
/*****************************************************************************/
#define BC_CP_DINC_SFIX_NINT_MODE  ((BCOPY_MODE_CNTL_START | BCOPY_DEST_INC_MODE) & 
				    (BCOPY_MODE_CNTL_COPY_MODE & BCOPY_INTERRUPT_DISABLE & 
				     BCOPY_SOURCE_FIX_MODE))     

/*****************************************************************************/
/* Copy Mode, Destination Increment, Source Fix, Interrupt Enable            */
/*****************************************************************************/
#define BC_CP_DINC_SFIX_INT_MODE  ((BCOPY_MODE_CNTL_START | BCOPY_SOURCE_INC_MODE | 
				    BCOPY_INTERRUPT_ENABLE | BCOPY_DEST_INC_MODE) & 
				   (BCOPY_MODE_CNTL_COPY_MODE & BCOPY_SOURCE_FIX_MODE))  
/*****************************************************************************/
/* Copy Mode, Destination Fix, Source Increment , Interrupt Disable          */
/*****************************************************************************/
#define BC_CP_DFIX_SINC_NINT_MODE  ((BCOPY_MODE_CNTL_START | BCOPY_SOURCE_INC_MODE) & 
				    (BCOPY_MODE_CNTL_COPY_MODE & BCOPY_INTERRUPT_DISABLE & BCOPY_DEST_FIX_MODE))     

/*****************************************************************************/
/* Copy Mode, Destination Fix, Source Increment , Interrupt Disable          */
/*****************************************************************************/
#define BC_CP_DFIX_SINC_INT_MODE  ((BCOPY_MODE_CNTL_START | BCOPY_SOURCE_INC_MODE | 
				    BCOPY_INTERRUPT_ENABLE) & 
				   (BCOPY_MODE_CNTL_COPY_MODE & BCOPY_SOURCE_FIX_MODE))  


/*****************************************************************************/
/* Copy Mode, Destination Fix, Source Fix , Interrupt Disable                */
/*****************************************************************************/
#define BC_CP_DFIX_FIX_NINT_MODE  ((BCOPY_MODE_CNTL_START) | (BCOPY_SOURCE_FIX_MODE & 
				     BCOPY_MODE_CNTL_COPY_MODE & BCOPY_INTERRUPT_DISABLE 
				     & BCOPY_DEST_FIX_MODE))     

/*****************************************************************************/
/* Copy Mode, Destination Fix, Source Fix , Interrupt Disable          */
/*****************************************************************************/
#define BC_CP_DFIX_FIX_INT_MODE  ((BCOPY_MODE_CNTL_START | BCOPY_INTERRUPT_ENABLE) &  
				   (BCOPY_SOURCE_FIX_MODE & BCOPY_MODE_CNTL_COPY_MODE 
				    & BCOPY_SOURCE_FIX_MODE))  
         


/********************************************************************************/
/*   Chips Select Register							*/
/*   CS0 to CS7									*/
/*   CS Config Registers Defination						*/
/*   bit 0   Data width		0 = 16 bits					*/
/*				1 = 8 bits					*/
/*										*/
/*   bit 1   Wait_en		1 = Enable the use of a devices wait		*/
/*				    signal to extend the cycle			*/
/*				0 = Disable					*/
/*										*/
/*   bit 2   Bus mode		0 = Motorola bus mode				*/
/*				1 = Intel Bus mode				*/
/*										*/
/*   bit 3   Mux_lo		0 = No add/data Muxing of data [7:0]		*/
/*				1 = Mux data [7:0] to add [7:0]			*/
/*										*/
/*   bit 4   Mux_Hi		0 = No add/data Muxing of data [15:8]		*/
/*				1 = Mux data [15:8] to add [7:0]		*/
/*										*/
/*   NON_MUX Mode								*/
/*										*/
/*   Bit 5   Tadcs		1 = Add to CS delay, 0 + [5] Cycles		*/
/*				0 = Disable					*/
/*										*/
/*   Bit 6   Tcsds		1 = CS to DS/RD/WR delay, 0 +[6] Cycles		*/
/*				0 = Disable					*/
/*										*/
/*   Bit [8:7]   PWds		1 = Pulse Width of DS/RD/WR, 2+[9:8] Cycles	*/
/*				0 = Disable					*/
/*										*/
/*   MUX Mode									*/
/*										*/
/*   Bit 5   Tdsas              1 = DS/RD/WR to AS/ALE delay, 1+[5] cycle	*/
/*                                 Delay					*/
/*										*/
/*                              0 = Disable					*/
/*										*/
/*   Bit 6    Was               1 = Pulse Width of AS/ALE high, 1 + [6]		*/
/*										*/
/*				0 = Disable					*/
/*										*/
/*   Bit [7]  Tashs		1 = Pulse Width of DS/RD/WR, 2+[9:8] Cycles     */
/*				0 = Disable                                     */
/*   Bit [9:8] PWds		Pulse Width of DS high or RD/WR low, 2 + [8:90] */
/*				Cycle						*/
/********************************************************************************/
#define TRIAD_CSX_BASE          0xBC000080
#define TRIAD_REG(x)            ((x)*4)
#define TRIAD_CS0_CONFIG_REG	(TRIAD_CSX_BASE + (TRIAD_REG(0)))	/* 0xBC000080 */ 
#define TRIAD_CS0_BASEADD_REG	(TRIAD_CSX_BASE + (TRIAD_REG(1)))	/* 0xBC000084 */ 
#define TRIAD_CS0_MASK_REG	(TRIAD_CSX_BASE + (TRIAD_REG(2)))	/* 0xBC000088 */ 
#define TRIAD_CS0_SPARE_REG     (TRIAD_CSX_BASE + (TRIAD_REG(4)))	/* 0xBC00008C */ 

#define TRIAD_CS1_CONFIG_REG	(TRIAD_CSX_BASE + (TRIAD_REG(5)))	/* 0xBC000090 */ 
#define TRIAD_CS1_BASEADD_REG	(TRIAD_CSX_BASE + (TRIAD_REG(6))) 	/* 0xBC000094 */ 
#define TRIAD_CS1_MASK_REG	(TRIAD_CSX_BASE + (TRIAD_REG(7))) 	/* 0xBC000098 */ 
#define TRIAD_CS1_SPARE_REG     (TRIAD_CSX_BASE + (TRIAD_REG(8))) 	/* 0xBC00009C */ 
          
#define TRIAD_CS2_CONFIG_REG	(TRIAD_CSX_BASE + (TRIAD_REG(9)))	/* 0xBC0000A0 */  
#define TRIAD_CS2_BASEADD_REG	(TRIAD_CSX_BASE + (TRIAD_REG(10))) 	/* 0xBC0000A4 */  
#define TRIAD_CS2_MASK_REG	(TRIAD_CSX_BASE + (TRIAD_REG(11))) 	/* 0xBC0000A8 */  
#define TRIAD_CS2_SPARE_REG     (TRIAD_CSX_BASE + (TRIAD_REG(12)) 	/* 0xBC0000AC */  

#define TRIAD_CS3_CONFIG_REG	(TRIAD_CSX_BASE + (TRIAD_REG(13))) 	/* 0xBC0000B0 */  
#define TRIAD_CS3_BASEADD_REG	(TRIAD_CSX_BASE + (TRIAD_REG(14)))  	/* 0xBC0000B4 */  
#define TRIAD_CS3_MASK_REG	(TRIAD_CSX_BASE + (TRIAD_REG(15)))  	/* 0xBC0000B8 */  
#define TRIAD_CS3_SPARE_REG     (TRIAD_CSX_BASE + (TRIAD_REG(16))) 	/* 0xBC0000BC */  

#define TRIAD_CS4_CONFIG_REG	(TRIAD_CSX_BASE + (TRIAD_REG(17)))  	/* 0xBC0000C0 */  
#define TRIAD_CS4_BASEADD_REG	(TRIAD_CSX_BASE + (TRIAD_REG(18)))  	/* 0xBC0000C4 */  
#define TRIAD_CS4_MASK_REG	(TRIAD_CSX_BASE + (TRIAD_REG(19)))  	/* 0xBC0000C8 */  
#define TRIAD_CS4_SPARE_REG     (TRIAD_CSX_BASE + (TRIAD_REG(20)))  	/* 0xBC0000CC */  

#define TRIAD_CS5_CONFIG_REG	(TRIAD_CSX_BASE + (TRIAD_REG(21)))  	/* 0xBC0000D0 */  
#define TRIAD_CS5_BASEADD_REG	(TRIAD_CSX_BASE + (TRIAD_REG(22)))  	/* 0xBC0000D4 */  
#define TRIAD_CS5_MASK_REG	(TRIAD_CSX_BASE + (TRIAD_REG(23)))  	/* 0xBC0000D8 */  
#define TRIAD_CS5_SPARE_REG     (TRIAD_CSX_BASE + (TRIAD_REG(24))) 	/* 0xBC0000DC */  

#define TRIAD_CS6_CONFIG_REG	(TRIAD_CSX_BASE + (TRIAD_REG(25)))  	/* 0xBC0000E0 */  
#define TRIAD_CS6_BASEADD_REG	(TRIAD_CSX_BASE + (TRIAD_REG(26)))  	/* 0xBC0000E4 */  
#define TRIAD_CS6_MASK_REG	(TRIAD_CSX_BASE + (TRIAD_REG(27)))  	/* 0xBC0000E8 */  
#define TRIAD_CS6_SPARE_REG     (TRIAD_CSX_BASE + (TRIAD_REG(28)))  	/* 0xBC0000EC */  
 
#define TRIAD_CS7_CONFIG_REG	(TRIAD_CSX_BASE + (TRIAD_REG(29)))  	/* 0xBC0000F0 */  
#define TRIAD_CS7_BASEADD_REG	(TRIAD_CSX_BASE + (TRIAD_REG(30)))   	/* 0xBC0000F4 */  
#define TRIAD_CS7_MASK_REG	(TRIAD_CSX_BASE + (TRIAD_REG(31)))   	/* 0xBC0000F8 */  
#define TRIAD_CS7_SPARE_REG     (TRIAD_CSX_BASE + (TRIAD_REG(32)))  	/* 0xBC0000FC */   
 




/* base address for Ethernet controller-1 */
#define TRIAD_ETH_CNTL1_BASE			         0xB2200000
/* base address for Ethernet controller-0 */		
#define TRIAD_ETH_CNTL0_BASE			         0xB2100000		
/* base address for Encryption/Decryption */
#define TRIAD_DES_ENG_BASE				 0xB2000000
/* JTAG Base Address */
#define TRIAD_JTAG_BASE					0xB4002C00		
/* General Purpose I/O Controller Base Address */
#define TRIAD_GPIO_BASE					0xB4002400		
/* SLAC Interface Controller */
#define TRIAD_SINTF_BASE				0xB4002000
/* I2C Serial Interface Controller */		
#define TRIAD_I2C_BASE					0xB4001000
/* SCC Controller Port A and Port B */		
#define TRIAD_SCC_BASE					0xB4000400			
#else
#define TRIAD_FLASH_BASE				(volatile unsigned char *)0xBFC00000		
/******************************************/
/* System Control Sub-system Address Space*/
/******************************************/
/* base address for BIU controller        */
#define TRIAD_BIU_BASE					(volatile unsigned int *)0xBC201000 		
/* base address for Interrupt controller */
#define TRIAD_INTR_BASE					(volatile unsigned int *)0xBC200C00
/* base address for Triad Timer and counter */	
#define TRIAD_TIMER_BASE				(volatile unsigned int *)0xBC200800 		
/* base address for System Control Block */
#define TRIAD_SCB_BASE					(volatile unsigned int *)0xBC200400 
/* base address for Memory controller */ 	
#define TRIAD_MEM_CNTL_BASE				(volatile unsigned int *)0xBC200000		
#define TRIAD_MEM_CONF1_REG				(volatile unsigned int *)(TRIAD_MEM_CNTL_BASE + 0x00)
/** Bit definition here */
#define TRIAD_MEM_CONF2_REG				(volatile unsigned int *)(TRIAD_MEM_CNTL_BASE + 0x04)
/** Bit definition here */

/* base address for Ethernet controller-1 */
#define TRIAD_ETH_CNTL1_BASE			(volatile unsigned int *)0xB2200000
/* base address for Ethernet controller-0 */		
#define TRIAD_ETH_CNTL0_BASE			(volatile unsigned int *)0xB2100000		
/* base address for Encryption/Decryption */
#define TRIAD_DES_ENG_BASE				(volatile unsigned int *)0xB2000000	



#ifdef EVALBOARD
#define UART_1 							(volatile unsigned char *)SERIAL_CHAN_A_ADDR
#define UART_2 							(volatile unsigned char *)SERIAL_CHAN_B_ADDR
#else
#define TRIAD_UART_BASE					(volatile unsigned char *)0xB4000000		/* UART Base Address */
#define UART_1							TRIAD_UART_BASE
#endif

/* JTAG Base Address */
#define TRIAD_JTAG_BASE					(volatile unsigned char *)0x14002C00		
/* General Purpose I/O Controller Base Address */
#define TRIAD_GPIO_BASE					(volatile unsigned char *)0x14002400		
/* SLAC Interface Controller */
#define TRIAD_SINTF_BASE				(volatile unsigned char *)0x14002000
/* I2C Serial Interface Controller */		
#define TRIAD_I2C_BASE					(volatile unsigned char *)0x14001000
/* SCC Controller Port A and Port B */		
#define TRIAD_SCC_BASE					(volatile unsigned char *)0x14000400		


	
#endif





/* System Peripheral Sub-system Address Space */



/* base address for Framer Engine */
#define TRIAD_FRAMR_ENG_BASE			0x10200000		


/*********************************/
/* base address for Voice Engine */
/*********************************/

#ifdef _ASMLANGUAGE
/* base address for Voice Engine */
#define TRIAD_VOICE_ENG_BASE			0x10000000		
/* Voice Engine- Data Transmit Queue */
#define TRIAD_VOICE_DTXQ_BASE			0x1008FA00		
/* Voice Engine- Data Receive Queue */
#define TRIAD_VOICE_DRXQ_BASE			0x1008F800
/* Voice Engine- Control Transmit Queue */		
#define TRIAD_VOICE_CTXQ_BASE			0x1008F600
/* Voice Engine- Control Recieve Queue */		
#define TRIAD_VOICE_CRXQ_BASE			0x1008F400
/* Voice Engine- Voice Receive Queue */		
#define TRIAD_VOICE_VRXQ_BASE			0x1008F200
/* Voice Engine- Voice Transmit Queue */		
#define TRIAD_VOICE_VTXQ_BASE			0x1008F000
/* Voice Engine- TDM1 Bus Controller */		
#define TRIAD_VOICE_TDM1_BASE			0x1008EC00
/* Voice Engine- TDM0 Bus Controller */
#define TRIAD_VOICE_TDM0_BASE			0x1008EA00
/* VE- ADPCM Voice CODEC Co-processor */		
#define TRIAD_VOICE_ADPCM_BASE			0x1008E600
/* VE- Floating-Point Co-processor */		
#define TRIAD_VOICE_FP_BASE				0x1008E400
/* Voice Engine- DMA Controller */		
#define TRIAD_VOICE_DMA_BASE			0x1008E200
/* VE- ZSP External/Internal Data SRAM Address space */		
#define TRIAD_VOICE_DATA_SRAM_BASE		0x10040000
/* VE- ZSP External/Internal Instruction SRAM Address space */		
#define TRIAD_VOICE_INST_SRAM_BASE		0x10000000		
#else
/* base address for voice Engine */
#define TRIAD_VOICE_ENG_BASE			(volatile unsigned int *)0x10000000
/* Voice Engine- Data Transmit Queue */		
#define TRIAD_VOICE_DTXQ_BASE			(volatile unsigned int *)0x1008FA00
/* Voice Engine- Data Receive Queue */		
#define TRIAD_VOICE_DRXQ_BASE			(volatile unsigned int *)0x1008F800
/* Voice Engine- Control Transmit Queue */		
#define TRIAD_VOICE_CTXQ_BASE			(volatile unsigned int *)0x1008F600
/* Voice Engine- Control Recieve Queue */		
#define TRIAD_VOICE_CRXQ_BASE			(volatile unsigned int *)0x1008F400
/* Voice Engine- Voice Receive Queue */		
#define TRIAD_VOICE_VRXQ_BASE			(volatile unsigned int *)0x1008F200
/* Voice Engine- Voice Transmit Queue */	
#define TRIAD_VOICE_VTXQ_BASE			(volatile unsigned int *)0x1008F000
/* Voice Engine- TDM1 Bus Controller */		
#define TRIAD_VOICE_TDM1_BASE			(volatile unsigned int *)0x1008EC00
/* Voice Engine- TDM0 Bus Controller */
#define TRIAD_VOICE_TDM0_BASE			(volatile unsigned int *)0x1008EA00
/* VE- ADPCM Voice CODEC Co-processor */		
#define TRIAD_VOICE_ADPCM_BASE			(volatile unsigned int *)0x1008E600
/* VE- Floating-Point Co-processor */		
#define TRIAD_VOICE_FP_BASE				(volatile unsigned int *)0x1008E400	
/* Voice Engine- DMA Controller */	
#define TRIAD_VOICE_DMA_BASE			(volatile unsigned int *)0x1008E200
/* VE- ZSP External/Internal Data SRAM Address space */		
#define TRIAD_VOICE_DATA_SRAM_BASE		(volatile unsigned int *)0x10040000		
/* VE- ZSP External/Internal Instruction SRAM Address space */
#define TRIAD_VOICE_INST_SRAM_BASE		(volatile unsigned int *)0x10000000		
#endif


/* Framer Sub-system Address Space*/
#define TRIAD_FRAMR_DRXQ_BASE			0x1028FA00		/* Framer Engine- Data Transmit Queue */
#define TRIAD_FRAMR_DTXQ_BASE			0x1028F800		/* Framer Engine- Data Receive Queue */
#define TRIAD_FRAMR_CRXQ_BASE			0x1028F600		/* Framer Engine- Control Transmit Queue */
#define TRIAD_FRAMR_CTXQ_BASE			0x1028F400		/* Framer Engine- Control Recieve Queue */

#define TRIAD_FRAMR_VRXQ_BASE			0x1028F200		/* Framer Engine- Voice Receive Queue */
#define TRIAD_FRAMR_VTXQ_BASE			0x1028F000		/* Framer Engine- Voice Transmit Queue */

#define TRIAD_FRAMR_CRC_BASE			0x1028EA00		/* Framer Engine- CRC Controller */

#define TRIAD_FRAMR_TC_BASE				0x1028E800		/* FE- Transmission Convergence */
#define TRIAD_FRAMR_HDLC_BASE			0x1028E600		/* Framer Engine- HDLC Controller */
#define TRIAD_FRAMR_UTP2_BASE			0x1028E400		/* Framer Engine- Utopia2 Controller */

#define TRIAD_FRAMR_DMA_BASE			0x1028E200		/* Framer Engine- DMA Controller */

#define TRIAD_FRAMR_DATA_SRAM_BASE		0x10240000		/* FE- ZSP External/Internal Data SRAM Address space */
#define TRIAD_FRAMR_INST_SRAM_BASE		0x10200000		/* FE- ZSP External/Internal Instruction SRAM Address space */





/*EPROM and FLASH Address Space*/
#define TRIAD_MIPS_CODE_BASE			0x1F04000		/* MIPS System Software Base Address */
#define TRIAD_FRMR_FW_BASE				0x1F02000		/* Framer Firmware Base Address */
#define TRIAD_VTS_FW_BASE				0x1F00000		/* voice/Telephony Firmware Base Address */


#endif /* !(_BRECIS_TRIAD_MAP_H) */
