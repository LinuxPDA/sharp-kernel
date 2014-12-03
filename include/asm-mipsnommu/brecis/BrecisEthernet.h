/*==========================================================================+
|									   										|
|   Title:   EMac.h							 								|
|									   										|
|   Author:  Michael Ngo						    						|
|									   										|
|   Date:    09/10/00.						      							|
|									   										|
+===========================================================================+
|   $Header:
+===========================================================================+
|									   										|
|   Description:							    							|
|     This file defines Triad architecture specific constants.	      		|
|     									    								|
|									   										|
|   Special Notes:							  								|
|									   										|
|									   										|
+==========================================================================*/

#ifndef _BRECIS_ETHERNET_H
#define _BRECIS_ETHERNET_H
/*--------------------------------------------------------------------------+
|  EThernet MAC0/1
+--------------------------------------------------------------------------*/
#define	MAC0_BASE	0xB8600000	/* Base address to MAC0 Controller */
#define	MAC1_BASE	0xB8700000	/* Base address to MAC0 Controller */



/*--------------------------------------------------------------------------+
| Ethernet MAC-0 and MAC-1 Registers Addresses
+--------------------------------------------------------------------------*/
#define DMA_CTL		0x00		/* DMA Control                     */
#define TXFRMPTR	0x04		/* Transmit Frame Pointer          */
#define TXTHRSH		0x08		/* Transmit Threshold              */
#define TXPOLLCTR	0x0c		/* Transmit Polling Counter        */
#define BLFRMPTR	0x10		/* Buffer List Frame Pointer       */
#define RXFRAGSIZE	0x14		/* Receive Fragment Size           */
#define INT_EN		0x18		/* Interrupt Enable                */
#define FDA_BAS		0x1c		/* Free Descriptor Area Base       */
#define FDA_LIM		0x20		/* Free Descriptor Area Limit      */
#define INT_SRC		0x24		/* Interrupt Source                */

#define RM_BBR		0x28		/* RMON Burst Base Register        */
#define RM_BCTL		0x2c		/* RMON Burst Control              */

/*
 * MAC Control and Status Register
 */
#define PAUSECNT	0x30		/* Pause Count                     */
#define REMPAUCNT	0x34		/* Remote Pause Count              */
#define TXCONFRMSTAT	0x38	/* Transmit Control Frame Status   */

#define MAC_CTL		0x40		/* MAC Control                     */
#define ARC_CTL		0x44		/* ARC Control                     */
#define TX_CTL		0x48		/* Transmit Control                */
#define TX_STAT		0x4c		/* Transmit Status                 */
#define RX_CTL		0x50		/* Receive Control                 */
#define RX_STAT		0x54		/* Receive Status                  */
#define MD_DATA		0x58		/* Station Management Data         */
#define MD_CA		0x5c		/* Station Management Control&Addr */
#define ARC_ADR		0x60		/* ARC Address                     */
#define ARC_DATA	0x64		/* ARC Data                        */
#define ARC_ENA		0x68		/* ARC Enable                      */
#define PROM_CTL	0x6c		/* PROM Control                    */
#define PROM_DATA	0x70		/* PROM data                       */
#define MISS_CNT	0x7c		/* Missed Error Count              */

#define CNTDATA		0x80		/* Count Data Register             */
#define CNTACC		0x84		/* Count Access Register           */
#define TXRMINTEN	0x88		/* Transmit RMON Interrupt Enable  */
#define RXRMINTEN	0x8c		/* Receive RMON Interrupt Enable   */
#define TXRMINTSTAT	0x90		/* Transmit RMON Interrupt Status  */
#define RXRMINTSTAT	0x94		/* Receive RMON Interrupt Status   */
#define MAC_ID_REG	0xd0		/* Version # */

				
#endif /* !(_BRECIS_ETHERNET_H) */
