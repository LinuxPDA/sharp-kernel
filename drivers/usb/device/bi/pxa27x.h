/*
 * linux/drivers/usb/device/bi/pxa_27x.h
 *
 * Copyright (c) 2004 Lineo Solutions, Inc.
 *
 * Written by Kana Paku
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 * Based on
 *
 * linux/drivers/usb/device/bi/pxa.h
 *
 * Copyright (c) 2000, 2001, 2002 Lineo
 *
 * By: 
 *      Stuart Lynne <sl@lineo.com>, 
 *      Tom Rushworth <tbr@lineo.com>, 
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 */


#define EP0_PACKETSIZE  0x10

#define UDC_MAX_ENDPOINTS       6

#define UDC_NAME        "PXA"

#ifdef CONFIG_SABINAL_DISCOVERY
/* SHARP A3 specific GPIO assignments */
#define USBD_CONNECT_GPIO	50
#if defined(CONFIG_SABINAL_DISCOVERY_DVT) || defined(CONFIG_SABINAL_DISCOVERY_DVT2)
#define USBD_CONNECT_HIGH	 1   /* Pin high for connect (undef for pin low) */
#else
#undef USBD_CONNECT_HIGH
#endif

//#define CKEN11_USB	USB_CE		 /* USB Unit Clock Enable */
#endif

#if defined(CONFIG_ARCH_PXA_POODLE)
/* SHARP SL-5600 specific GPIO assignments */
#define USBD_CONNECT_GPIO	20
#define USBD_CONNECT_HIGH	1
#endif

#if defined(CONFIG_ARCH_PXA_CORGI)
/* SHARP SL-C700 specific GPIO assignments */
#define USBD_CONNECT_GPIO	45
#define USBD_CONNECT_HIGH	1
#endif

#define USB_CONNECT_GPIO		41
#define USBD_VBUS_GPIO_DEVICE	35
#define USBD_VBUS_GPIO_HOST		37

/*
 * DMA Notes
 *
 * DCSRX	DMA Channel Control/Status register	
 * DRCMRX	DMA Request to Channel Map register
 * DDADRX	DMA Descriptor Address register
 *
 * DSADRX	DMA Source Address register (RO)	
 * DTADRX	DMA Target Address register (RO)
 * DCMCDX	DMA Command register (RO)
 *
 *
 */

typedef enum {
	DMA_READY,
	DMA_ACTIVE, 
	DMA_DONE
} dma_status_t;

typedef struct {
	int offset;
	dma_status_t dma_status;

	unsigned char *dma_buf;
	dma_addr_t dma_buf_phys;

#ifndef CONFIG_SABINAL_DISCOVERY
	pxa_dma_desc *dma_desc;
	dma_addr_t dma_desc_phys;
#endif

} usb_buf_t;

typedef struct {
	char *name;
	u_long dcmd;
	volatile u32 *drcmr;
	u_long dev_addr;

	u_int dma_ch;
	usb_buf_t *buffers;

	unsigned char *dma_buf;
	dma_addr_t dma_buf_phys;

#ifndef CONFIG_SABINAL_DISCOVERY
	pxa_dma_desc *dma_desc;
	dma_addr_t dma_desc_phys;
#endif
	int num;
	int size;

        int ep;
        struct usb_endpoint_instance *endpoint;
} usb_stream_t;


/*
 * Reserved Register Definitions
 */
#define UDCRSVD0        __REG(0x40600004)  /* UDC Reserved Register 0 */
#define UDCRSVD1        __REG(0x40600008)  /* UDC Reserved Register 1 */
#define UDCRSVD2        __REG(0x4060000c)  /* UDC Reserved Register 2 */

#define UFNHR           __REG(0x40600060)  /* UDC Frame Number High Register */
#define UFNLR           __REG(0x40600064)  /* UDC Frame Number Low Register */



/*
 *      7654 3210
 * de   1101 1110
 *
 *      fedc ba98
 * 7b   0111 1011
 *
 *
 */

#define UDCRSVD0_DBL0   (1 << 0)                /* enable endpoint 0 double buffer (always zero)) */
#define UDCRSVD0_DBL1   (1 << 1)                /* enable endpoint 1 double buffer (read/write) */
#define UDCRSVD0_DBL2   (1 << 2)                /* enable endpoint 2 double buffer (read/write) */
#define UDCRSVD0_DBL3   (1 << 3)                /* enable endpoint 3 double buffer (read/write) */
#define UDCRSVD0_DBL4   (1 << 4)                /* enable endpoint 4 double buffer (read/write) */
#define UDCRSVD0_DBL5   (1 << 5)                /* enable endpoint 5 double buffer (always zero)) */
#define UDCRSVD0_DBL6   (1 << 6)                /* enable endpoint 6 double buffer (read/write) */
#define UDCRSVD0_DBL7   (1 << 7)                /* enable endpoint 7 double buffer (read/write) */

#define UDCRSVD0_DBL8   (1 << 0)                /* enable endpoint 8 double buffer (read/write) */
#define UDCRSVD0_DBL9   (1 << 1)                /* enable endpoint 9 double buffer (read/write) */
#define UDCRSVD0_DBL10  (1 << 2)                /* enable endpoint 10 double buffer (always zero)) */
#define UDCRSVD0_DBL11  (1 << 3)                /* enable endpoint 11 double buffer (read/write) */
#define UDCRSVD0_DBL12  (1 << 4)                /* enable endpoint 12 double buffer (read/write) */
#define UDCRSVD0_DBL13  (1 << 5)                /* enable endpoint 13 double buffer (read/write) */
#define UDCRSVD0_DBL14  (1 << 6)                /* enable endpoint 14 double buffer (read/write) */
#define UDCRSVD0_DBL15  (1 << 7)                /* enable endpoint 15 double buffer (always zero)) */

/* ep0 states */
#define WAIT_FOR_SETUP          0
#define DATA_STATE_XMIT         1
#define DATA_STATE_NEED_ZLP     2
#define WAIT_FOR_OUT_STATUS     3


// XXX Send to nico@cam.org to include in pxa-regs.h
//

/* UDC Control Register (UDCCR) */

#define UDCCR_UDE	(1 << 0)	/* UDC enable */
#define UDCCR_UDA	(1 << 1)	/* UDC active */
#define UDCCR_UDR	(1 << 2)	/* Device resume */
#define UDCCR_EMCE	(1 << 3)	/* Endpoint Memory Configuration Error */
#define UDCCR_SMAC	(1 << 4)	/* Switch Endpoint Memory to Active Configuration */
#define UDCCR_AAISN	(1 << 5)	/* Active UDC Alternate Interface Setting Number */
#define UDCCR_AIN	(1 << 8)	/* Active UDC Interface Number */
#define UDCCR_ACN	(1 << 11)	/* Active UDC Configuration Number */
#define UDCCR_DWRE	(1 << 16)	/* Device Remote Wake-up Feature */
#define UDCCR_BHNP	(1 << 28)	/* B-Device Host Negotiation Protocol Enable */
#define UDCCR_AHNP	(1 << 29)	/* A-Device Host Negotiation Protocol Support */
#define UDCCR_AALTHNP	(1 << 30)	/* A-Device Host Negotiation Protocol port Support */
#define UDCCR_OEN	(1 << 31)	/* On-The-Go Enable */


/* UDC Endpoint 0 Control Status Register (UDCCS0) */

#define UDCCSR0_OPC	(1 << 0)	/* OUT Packet Complete */
#define UDCCSR0_IPR	(1 << 1)	/* IN Packet Ready */
#define UDCCSR0_FTF	(1 << 2)	/* Flush Transmit FIFO */
#define UDCCSR0_DME	(1 << 3)	/* DME Enable */
#define UDCCSR0_SST	(1 << 4)	/* Sent Stall */
#define UDCCSR0_FST	(1 << 5)	/* Force Stall */
#define UDCCSR0_RNE	(1 << 6)	/* Receive FIFO Not Empty */
#define UDCCSR0_SA	(1 << 7)	/* Setup Active */
#define UDCCSR0_AREN	(1 << 8)	/* ACK Response Enable */
#define UDCCSR0_ACM	(1 << 9)	/* ACK Control Mode */


/* UDC Endpoint A-X Control Status Register */

#define UDCCS_FS		(1 << 0)	/* FIFO nees services */
#define UDCCS_PC		(1 << 1)	/* Packet Complete */
#define UDCCS_TRN		(1 << 2)	/* Tx/Rx NAK */
#define UDCCS_DME		(1 << 3)	/* DMA Enable */
#define UDCCS_SST		(1 << 4)	/* Sent Stall */
#define UDCCS_FST		(1 << 5)	/* Force Stall */
#define UDCCS_BNEBNF	(1 << 6)	/* Bufer Not Empty/Buffer Not Full */
#define UDCCS_SP		(1 << 7)	/* Short Packet Control/Status */
#define UDCCS_FEF		(1 << 8)	/* Flush Endpoint FIFO */
#define UDCCS_DPE		(1 << 9)	/* Data Packet Error */

/* UDC Interrupt Control Register 0 (UICR0) */

#define UDCICR0_IE0	(1 << 0)	/* Interrupt enable ep 0 */
#define UDCICR0_IEA	(1 << 2)	/* Interrupt enable ep A */
#define UDCICR0_IEB	(1 << 4)	/* Interrupt enable ep B */
#define UDCICR0_IEC	(1 << 6)	/* Interrupt enable ep C */
#define UDCICR0_IED	(1 << 8)	/* Interrupt enable ep D */
#define UDCICR0_IEE	(1 << 10)	/* Interrupt enable ep E */
#define UDCICR0_IEF	(1 << 12)	/* Interrupt enable ep F */
#define UDCICR0_IEG	(1 << 14)	/* Interrupt enable ep G */
#define UDCICR0_IEH	(1 << 16)	/* Interrupt enable ep H */
#define UDCICR0_IEI	(1 << 18)	/* Interrupt enable ep I */
#define UDCICR0_IEJ	(1 << 20)	/* Interrupt enable ep J */
#define UDCICR0_IEK	(1 << 22)	/* Interrupt enable ep K */
#define UDCICR0_IEL	(1 << 24)	/* Interrupt enable ep L */
#define UDCICR0_IEM	(1 << 26)	/* Interrupt enable ep M */
#define UDCICR0_IEN	(1 << 28)	/* Interrupt enable ep N */
#define UDCICR0_IEP	(1 << 30)	/* Interrupt enable ep P */


/* UDC Interrupt Control Register 0 (UICR1) */

#define UDCICR1_IEQ	(1 << 0)	/* Interrupt enable ep Q */
#define UDCICR1_IER	(1 << 2)	/* Interrupt enable ep R */
#define UDCICR1_IES	(1 << 4)	/* Interrupt enable ep S */
#define UDCICR1_IET	(1 << 6)	/* Interrupt enable ep T */
#define UDCICR1_IEU	(1 << 8)	/* Interrupt enable ep U */
#define UDCICR1_IEV	(1 << 10)	/* Interrupt enable ep V */
#define UDCICR1_IEW	(1 << 12)	/* Interrupt enable ep W */
#define UDCICR1_IEX	(1 << 14)	/* Interrupt enable ep X */
#define UDCICR1_RES1	(1 << 16)	/* Interrupt Reserved1 */
#define UDCICR1_IERS	(1 << 27)	/* interrupt enable - Reset */
#define UDCICR1_IESU	(1 << 28)	/* interrupt enable - Suspend */
#define UDCICR1_IERU	(1 << 29)	/* interrupt enable - Resume */
#define UDCICR1_IESOF	(1 << 30)	/* interrupt enable - Start of Frame */
#define UDCICR1_IECC	(1 << 31)	/* interrupt enable - Configuration Change */


/* UDC Status / Interrupt Register 0 (USIR0) */

#define UDCISR0_IR0	(1 << 0)	/* Interrupt request ep 0 */
#define UDCISR0_IRA	(1 << 2)	/* Interrupt request ep A */
#define UDCISR0_IRB	(1 << 4)	/* Interrupt request ep B */
#define UDCISR0_IRC	(1 << 6)	/* Interrupt request ep C */
#define UDCISR0_IRD	(1 << 8)	/* Interrupt request ep D */
#define UDCISR0_IRE	(1 << 10)	/* Interrupt request ep E */
#define UDCISR0_IRF	(1 << 12)	/* Interrupt request ep F */
#define UDCISR0_IRG	(1 << 14)	/* Interrupt request ep G */
#define UDCISR0_IRH	(1 << 16)	/* Interrupt request ep H */
#define UDCISR0_IRI	(1 << 18)	/* Interrupt request ep I */
#define UDCISR0_IRJ	(1 << 20)	/* Interrupt request ep J */
#define UDCISR0_IRK	(1 << 22)	/* Interrupt request ep K */
#define UDCISR0_IRL	(1 << 24)	/* Interrupt request ep L */
#define UDCISR0_IRM	(1 << 26)	/* Interrupt request ep M */
#define UDCISR0_IRN	(1 << 28)	/* Interrupt request ep N */
#define UDCISR0_IRP	(1 << 30)	/* Interrupt request ep P */


/* UDC Status / Interrupt Register 1 (USIR1) */

#define UDCISR1_IRQ	(1 << 0)	/* Interrupt request ep Q */
#define UDCISR1_IRR	(1 << 2)	/* Interrupt request ep R */
#define UDCISR1_IRS	(1 << 4)	/* Interrupt request ep S */
#define UDCISR1_IRT	(1 << 6)	/* Interrupt request ep T */
#define UDCISR1_IRU	(1 << 8)	/* Interrupt request ep U */
#define UDCISR1_IRV	(1 << 10)	/* Interrupt request ep V */
#define UDCISR1_IRW	(1 << 12)	/* Interrupt request ep W */
#define UDCISR1_IRX	(1 << 14)	/* Interrupt request ep X */
#define UDCISR1_RES1	(1 << 16)	/* Interrupt Reserved1 */
#define UDCISR1_IRRS	(1 << 27)	/* Interrupt request - Reset */
#define UDCISR1_IRSU	(1 << 28)	/* Interrupt request - Suspend */
#define UDCISR1_IRRU	(1 << 29)	/* Interrupt request - Resume */
#define UDCISR1_IRSOF	(1 << 30)	/* Interrupt request - Start of Frame */
#define UDCISR1_IRCC	(1 << 31)	/* Interrupt request - Configuration Change */







