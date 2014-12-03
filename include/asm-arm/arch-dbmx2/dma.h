/*
 * linux/include/asm-arm/arch-dbmx2/dma.h
 *
 * Copyright (C) 2003 Lineo Solutions, Inc.
 *
 * Based on:
 *  linux/include/asm-arm/arch-mx2ads/dma.h
 */

#ifndef MX2_DMA_INCLUDE
#define MX2_DMA_INCLUDE

#include <linux/config.h>
#include <asm/arch/mx2.h>

#define MAX_DMA_ADDRESS		0xffffffff


//#define MX2_DMA_CHANNELS 0			
#define MX2_DMA_CHANNELS 16			
#define MAX_DMA_CHANNELS MX2_DMA_CHANNELS
	
#define MX_DMA_CHANNELS		MX2_DMA_CHANNELS		 

#define DMA_MEM_SIZE_8	0x1
#define DMA_MEM_SIZE_16	0x2
#define DMA_MEM_SIZE_32	0x0

#define DMA_TYPE_LINEAR	0x0
#define DMA_TYPE_2D		0x01
#define DMA_TYPE_FIFO	0x2
#define DMA_TYPE_EBE	0x3

#define DMA_DONE		0x1000
#define DMA_BURST_TIMEOUT 	0x1
#define DMA_REQUEST_TIMEOUT 	0x2
#define DMA_TRANSFER_ERROR	0x4
#define DMA_BUFFER_OVERFLOW	0x8




typedef void (*dma_callback_t)(void * arg);	

typedef struct {	
	u32 dir;			/*	Trans direction		*/
	u32 burstLength;		/* 	Channel burst length	*/
	u32 repeat;			/*	Repeat flag		*/
	u32 sourceType;			/*	Source type		*/
	u32* sourceAddr;		/*	Source address		*/
	u32 sourcePort;			/*	Source port size		*/
	u32 destType; 			/*	Destination type		*/	
	u32* destAddr;			/*	Destination address	*/
	u32 destPort; 			/*	Destination port size	*/
	u32 count;				
	u32 W;				/*	2D Wide-size		*/
	u32 X; 				/*	2D X-size		*/
	u32 Y; 				/*	2D Y-size		*/
	u32 request;			/* 	request source		*/
	dma_callback_t callback;/*	call back function  */      
	void * arg;				
	
} dma_request_t;



typedef struct
{
	volatile u32    CEN:1;
	volatile u32 	FRC:1;
	volatile u32	RPT:1;
	volatile u32 	REN:1;
	volatile u32	SSIZ:2;
	volatile u32	DSIZ:2;
	volatile u32 	MSEL:1;
	volatile u32	MDIR:1;
	volatile u32	SMOD:2;
	volatile u32	DMOD:2;
	volatile u32	ACRPT:1;
	volatile u32	Reserver:17;	
	
}dma_regs_control;

#define DMA_CTL_CEN 0x1
#define DMA_CTL_FRC 0x2
#define DMA_CTL_RPT 0x4
#define DMA_CTL_REN 0x8

//#define DMA_CTL_SSIZ (0x10|0x20)
//#define DMA_CTL_DSIZ (0x40|0x80)

#define DMA_CTL_MSEL 0x100
#define DMA_CTL_MDIR 0x200
//#define DMA_CTL_SMOD (0x400|0x800)
//#define DMA_CTL_DMOD (0x1000|0x2000)
#define DMA_CTL_ACRPT 0x4000

#define DMA_CTL_GET_SSIZ(x) (((x)>>4)&0x3)
#define DMA_CTL_GET_DSIZ(x) (((x)>>6)&0x3)
#define DMA_CTL_GET_SMOD(x)  (((x)>>10)&0x3)
#define DMA_CTL_GET_DMOD(x)  (((x)>>12)&0x3)

#define DMA_CTL_SET_SSIZ(x,value) 	do{ \
									(x)&=~(0x3<<4); \
									(x)|=(value)<<4; 		\
								}while(0)

#define DMA_CTL_SET_DSIZ(x,value) 	do{ \
									(x)&=~(0x3<<6); \
									(x)|=(value)<<6; 		\
								}while(0)

#define DMA_CTL_SET_SMOD(x,value) 	do{ \
									(x)&=~(0x3<<10); \
									(x)|=(value)<<10; 		\
								}while(0)

#define DMA_CTL_SET_DMOD(x,value) 	do{ \
									(x)&=~(0x3<<12); \
									(x)|=(value)<<12; 		\
								}while(0)

typedef struct {
	volatile u32 	SourceAddr;
	volatile u32	DestAddr;
	volatile u32	Count;
	volatile u32 	Ctl;
	volatile u32	RequestSource;
	volatile u32	BurstLength;
	union{
		volatile u32	ReqTimeout;
		volatile u32	BusUtilt;
	};
} dma_regs_t;


typedef struct {
	unsigned int in_use;		/* Device is allocated */
	unsigned int active;		/* Device is active i.e. enabled) */
	unsigned int status;
	unsigned int mode;		
	void		* arg;
	const char *device_name;	/* Device name */
	dma_regs_t *regs;			/* points to appropriate DMA registers */
	int 		irpt;
	dma_callback_t callback; 	/* ... to call when buffers are done */

} mx2_dma_t;

int dbmx2_dma_start(dmach_t channel);
int dbmx2_request_dma(int *channel,const char * devicename);
int dbmx2_dma_set_config(dmach_t channel, dma_request_t *p);
int dbmx2_dma_get_config(dmach_t channel, dma_request_t *p);
void dbmx2_free_dma(dmach_t channel);
int dbmx2_dma_get_status(dmach_t channel,int * status);
int dbmx2_get_dma_residue(dmach_t channel);
int dbmx2_dump_dma_register(dmach_t channel);
void dbmx2_disable_dma(dmach_t channel);
void dbmx2_set_dma_mode(dmach_t channel, dmamode_t mode);
void dbmx2_set_dma_addr(dmach_t channel, unsigned long physaddr);
void dbmx2_set_dma_count(dmach_t channel, unsigned long count);
int dbmx2_dma_set_callback(dmach_t channel, dma_callback_t cb);
void dbmx2_enable_dma (dmach_t channel);
int dbmx2_get_dma_list(char *buf);

#define DMA_REQ_CSI_RX     31
#define DMA_REQ_CSI_STAT   30
#define DMA_REQ_BMI_RX     29
#define DMA_REQ_BMI_TX     28
#define DMA_REQ_UART1_TX   27
#define DMA_REQ_UART1_RX   26
#define DMA_REQ_UART2_TX   25
#define DMA_REQ_UART2_RX   24
#define DMA_REQ_UART3_TX   23
#define DMA_REQ_UART3_RX   22
#define DMA_REQ_UART4_TX   21
#define DMA_REQ_UART4_RX   20
#define DMA_REQ_CSPI1_TX   19
#define DMA_REQ_CSPI1_RX   18
#define DMA_REQ_CSPI2_TX   17
#define DMA_REQ_CSPI2_RX   16
#define DMA_REQ_SSI1_TX1   15
#define DMA_REQ_SSI1_RX1   14
#define DMA_REQ_SSI1_TX0   13
#define DMA_REQ_SSI1_RX0   12
#define DMA_REQ_SSI2_TX1   11
#define DMA_REQ_SSI2_RX1   10
#define DMA_REQ_SSI2_TX0   9
#define DMA_REQ_SSI2_RX0   8
#define DMA_REQ_SDHC1      7
#define DMA_REQ_SDHC2      6
#define DMA_FIRI_TX        5
#define DMA_FIRI_RX        4
#define DMA_EX             3
#endif
