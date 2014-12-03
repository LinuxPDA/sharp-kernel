From - Mon Jul 30 17:35:11 2001
X-UIDL: bd6a2955e72c74434e3a47eb6d7066a6
Return-Path: <jchen@brecis.com>
Received: from brecis.com ([65.198.34.10])
	by mail.lineo.ca (8.9.3/8.9.3/SuSE Linux 8.9.3-0.1) with ESMTP id UAA17606;
	Fri, 20 Apr 2001 20:01:24 -0400
Received: from solar.brecis.com (solar [126.0.27.8])
	by brecis.com (8.9.3/8.9.3) with ESMTP id RAA13903;
	Fri, 20 Apr 2001 17:00:29 -0700 (PDT)
Received: from hwc8701 (dhcp-27-151 [126.0.27.151])
	by solar.brecis.com (8.9.1b+Sun/8.9.3) with SMTP id RAA09120;
	Fri, 20 Apr 2001 17:01:23 -0700 (PDT)
From: "Jack Chen" <jchen@brecis.com>
To: "Michael Leslie" <mleslie@lineo.ca>, "Ted Ma" <mated@lineo.ca>
Cc: "Bob Bernstein" <bob.bernstein@brecis.com>,
        "Sekhar Kondepudi" <sekhar@brecis.com>
Subject: sample code
Date: Fri, 20 Apr 2001 16:59:31 -0700
Message-ID: <DBECLOCIOKAOPJGEIJGBMEIACAAA.jchen@brecis.com>
MIME-Version: 1.0
Content-Type: multipart/mixed;
	boundary="----=_NextPart_000_0010_01C0C9BB.44B761B0"
X-Priority: 3 (Normal)
X-MSMail-Priority: Normal
X-Mailer: Microsoft Outlook IMO, Build 9.0.2416 (9.0.2911.0)
Importance: Normal
X-MimeOLE: Produced By Microsoft MimeOLE V5.00.2919.6700
In-Reply-To: <Pine.LNX.4.21.0104191332330.20855-100000@trinity.lineo.ca>

This is a multi-part message in MIME format.

------=_NextPart_000_0010_01C0C9BB.44B761B0
Content-Type: text/plain;
	charset="us-ascii"
Content-Transfer-Encoding: 7bit

Hi guys,

Here is the sample code you need that we talked about in the meeting on
04/18/01.

1. allocate uncache memory:

#define TRIAD_MALLOC(x)  ((unsigned int)(malloc(x))| 0xa0000000 )

Usually, malloc(x)returns cacheable memory. By ORing with 0xa0000000. It
becomes uncacheable.

2. Interrupt vectors:

/* interrupt levels */
#define INT_LVL_TIMER           SR_IBIT8        /* RC32364 timer (fixed) */
#define INT_LVL_IORQ4           SR_IBIT7        /* IORQ 4 */
#define INT_LVL_IORQ3           SR_IBIT6        /* IORQ 3 */
#define INT_LVL_IORQ2           SR_IBIT5        /* IORQ 2 */
#define INT_LVL_IORQ1           SR_IBIT4        /* IORQ 1 */
#define INT_LVL_IORQ0           SR_IBIT3        /* IORQ 0 */
#define INT_LVL_SW1             SR_IBIT2        /* sw interrupt 1 (fixed) */
#define INT_LVL_SW0             SR_IBIT1        /* sw interrupt 0 (fixed) */
#define INT_LVL_MAC0            SR_IBIT3        /* MAC0					 */

/* interrupt indexes */
#define INT_INDX_TIMER          7       /* RC32364 timer (fixed) */
#define INT_INDX_IORQ4          6
#define INT_INDX_IORQ3          5       /* IORQ 3 */
#define INT_INDX_IORQ2          4       /* IORQ 2 */
#define INT_INDX_IORQ1          3       /* IORQ 1 */
#define INT_INDX_IORQ0          2       /* IORQ 0 */
#define INT_INDX_SW1            1       /* sw interrupt 1       */
#define INT_INDX_SW0            0       /* sw interrupt 0       */

/* interrupt vectors */
#define IV_TIMER_VEC            70      /* RC32364 timer (fixed)          */
#define IV_IORQ4_VEC            69      /* SLM         - IORQ 4 */
#define IV_IORQ3_VEC            68      /* FE          - IORQ 3 */
#define IV_IORQ2_VEC            67      /* FE                    - IORQ 2 */
#define IV_IORQ1_VEC            66      /* MAC1                   - IORQ 1
*/
#define IV_IORQ0_VEC            65      /* MAC0                      - IORQ
0 */

#define INT_VEC_IORQ0           IV_IORQ0_VEC
#define INT_VEC_IORQ1           IV_IORQ1_VEC
#define INT_VEC_IORQ2           IV_IORQ2_VEC
#define INT_VEC_IORQ3           IV_IORQ3_VEC
#define INT_VEC_IORQ4           IV_IORQ4_VEC
#define INT_VEC_IORQ5           IV_TIMER_VEC


The following attached files are are going to use on the MSP5000 FPGA/EVM
boards.

Best Regards,

Jack



------=_NextPart_000_0010_01C0C9BB.44B761B0
Content-Type: application/octet-stream;
	name="uart.h"
Content-Transfer-Encoding: quoted-printable
Content-Disposition: attachment;
	filename="uart.h"

/*=3D=3D=3D=3D=3D=3D=3D=3D=3D=3D=3D=3D=3D=3D=3D=3D=3D=3D=3D=3D=3D=3D=3D=3D=
=3D=3D=3D=3D=3D=3D=3D=3D=3D=3D=3D=3D=3D=3D=3D=3D=3D=3D=3D=3D=3D=3D=3D=3D=3D=
=3D=3D=3D=3D=3D=3D=3D=3D=3D=3D=3D=3D=3D=3D=3D=3D=3D=3D=3D=3D=3D=3D=3D=3D=3D=
+
|                                                                        =
   |
|   Title:   Uart.h                                                      =
   |
|                                                                        =
   |
|   Author:  Michael Ngo                                                 =
   |
|                                                                        =
   |
|   Date:    09/10/00.                                                   =
   |
|                                                                        =
   |
+=3D=3D=3D=3D=3D=3D=3D=3D=3D=3D=3D=3D=3D=3D=3D=3D=3D=3D=3D=3D=3D=3D=3D=3D=
=3D=3D=3D=3D=3D=3D=3D=3D=3D=3D=3D=3D=3D=3D=3D=3D=3D=3D=3D=3D=3D=3D=3D=3D=3D=
=3D=3D=3D=3D=3D=3D=3D=3D=3D=3D=3D=3D=3D=3D=3D=3D=3D=3D=3D=3D=3D=3D=3D=3D=3D=
=3D+
|   $Header:
+=3D=3D=3D=3D=3D=3D=3D=3D=3D=3D=3D=3D=3D=3D=3D=3D=3D=3D=3D=3D=3D=3D=3D=3D=
=3D=3D=3D=3D=3D=3D=3D=3D=3D=3D=3D=3D=3D=3D=3D=3D=3D=3D=3D=3D=3D=3D=3D=3D=3D=
=3D=3D=3D=3D=3D=3D=3D=3D=3D=3D=3D=3D=3D=3D=3D=3D=3D=3D=3D=3D=3D=3D=3D=3D=3D=
=3D+
|                                                                        =
   |
|   Description:                                                         =
   |
|     This file defines Triad architecture specific constants.           =
   |
|     									    |
|                                                                        =
   |
|   Special Notes:                                                       =
   |
|                                                                        =
   |
|                                                                        =
   |
+=3D=3D=3D=3D=3D=3D=3D=3D=3D=3D=3D=3D=3D=3D=3D=3D=3D=3D=3D=3D=3D=3D=3D=3D=
=3D=3D=3D=3D=3D=3D=3D=3D=3D=3D=3D=3D=3D=3D=3D=3D=3D=3D=3D=3D=3D=3D=3D=3D=3D=
=3D=3D=3D=3D=3D=3D=3D=3D=3D=3D=3D=3D=3D=3D=3D=3D=3D=3D=3D=3D=3D=3D=3D=3D=3D=
*/


/*-----------------------------------------------------------------------=
---+
| UART Registers.
|   Some of the UART registers are used for multiple purpose.  =
Therefore,
|   the below address map may define different names to the same address
|   base.  Not to be alarm...  In addition, all registers are byte
|   addressable.  Read/Write to these register must be perform using
|   load/store byte operations (respectively).
+------------------------------------------------------------------------=
--*/
#define UART_BASE	0xBC000100	/* Base address to UART            */
#define NSREG(x) ((x)*4)
#ifdef __ASSEMBLER__
#define DATA (NSREG(0))	/* data register (R/W)             */
#define IER  (NSREG(1))	/* interrupt enable (W)            */
#define IIR  (NSREG(2))	/* interrupt identification (R)    */
#define	FIFO (NSREG(2))	/* 16550 fifo control (W)          */
#define CFCR (NSREG(3))	/* line control register (R/W)     */
#define MCR  (NSREG(4))	/* modem control register (R/W)    */
#define LSR  (NSREG(5))	/* line status register (R/W)      */
#define MSR  (NSREG(6))	/* modem status register (R/W)     */
#define SCR  (NSREG(7))	/* scratch register (R/W)          */
#else
#define DATA  (volatile unsigned int *)(UART_BASE + (NSREG(0)))
#define IER   (volatile unsigned int *)(UART_BASE + (NSREG(1)))
#define IIR   (volatile unsigned int *)(UART_BASE + (NSREG(2)))
#define	FIFO  (volatile unsigned int *)(UART_BASE + (NSREG(2)))
#define CFCR  (volatile unsigned int *)(UART_BASE + (NSREG(3)))
#define MCR   (volatile unsigned int *)(UART_BASE + (NSREG(4)))
#define LSR   (volatile unsigned int *)(UART_BASE + (NSREG(5)))
#define MSR   (volatile unsigned int *)(UART_BASE + (NSREG(6)))
#define SCR   (volatile unsigned int *)(UART_BASE + (NSREG(7)))
#endif


/*-----------------------------------------------------------------------=
---+
| 16 bit baud rate divisor (lower byte in dca_data, upper in dca_ier)
+------------------------------------------------------------------------=
--*/
#define UART_FREQ	5000000
#define BAUD_RATE	19200=20
#define BRTC(x)         (UART_FREQ / (16*(x)))
#define NS16550_CHANA	UART_BASE

/*-----------------------------------------------------------------------=
---+
| interrupt enable register
+------------------------------------------------------------------------=
--*/
#define IER_ERXRDY      0x1     /* int on rx ready */
#define IER_ETXRDY      0x2     /* int on tx ready */
#define IER_ERLS        0x4     /* int on line status change */
#define IER_EMSC        0x8     /* int on modem status change */

/*-----------------------------------------------------------------------=
---+
| interrupt identification register
+------------------------------------------------------------------------=
--*/
#define IIR_IMASK       0xf     /* mask */
#define IIR_RXTOUT      0xc     /* receive timeout */
#define IIR_RLS         0x6     /* receive line status */
#define IIR_RXRDY       0x4     /* receive ready */
#define IIR_TXRDY       0x2     /* transmit ready */
#define IIR_NOPEND      0x1     /* nothing */
#define IIR_MLSC        0x0     /* modem status */
#define IIR_FIFO_MASK   0xc0    /* set if FIFOs are enabled */

/*-----------------------------------------------------------------------=
---+
| fifo control register
+------------------------------------------------------------------------=
--*/
#define FIFO_ENABLE     0x01    /* enable fifo */
#define FIFO_RCV_RST    0x02    /* reset receive fifo */
#define FIFO_XMT_RST    0x04    /* reset transmit fifo */
#define FIFO_DMA_MODE   0x08    /* enable dma mode */
#define FIFO_TRIGGER_1  0x00    /* trigger at 1 char */
#define FIFO_TRIGGER_4  0x40    /* trigger at 4 chars */
#define FIFO_TRIGGER_8  0x80    /* trigger at 8 chars */
#define FIFO_TRIGGER_14 0xc0    /* trigger at 14 chars */

/*-----------------------------------------------------------------------=
---+
| character format control register
+------------------------------------------------------------------------=
--*/
#define CFCR_DLAB       0x80    /* divisor latch */
#define CFCR_SBREAK     0x40    /* send break */
#define CFCR_PZERO      0x30    /* zero parity */
#define CFCR_PONE       0x20    /* one parity */
#define CFCR_PEVEN      0x10    /* even parity */
#define CFCR_PODD       0x00    /* odd parity */
#define CFCR_PENAB      0x08    /* parity enable */
#define CFCR_STOPB      0x04    /* 2 stop bits */
#define CFCR_8BITS      0x03    /* 8 data bits */
#define CFCR_7BITS      0x02    /* 7 data bits */
#define CFCR_6BITS      0x01    /* 6 data bits */
#define CFCR_5BITS      0x00    /* 5 data bits */

/*-----------------------------------------------------------------------=
---+
| modem control register
+------------------------------------------------------------------------=
--*/
#define MCR_LOOPBACK    0x10    /* loopback */
#define MCR_IENABLE     0x08    /* output 2 =3D int enable */
#define MCR_DRS         0x04    /* output 1 =3D xxx */
#define MCR_RTS         0x02    /* enable RTS */
#define MCR_DTR         0x01    /* enable DTR */

/*-----------------------------------------------------------------------=
---+
| line status register
+------------------------------------------------------------------------=
--*/
#define LSR_RCV_FIFO    0x80    /* error in receive fifo */
#define LSR_TSRE        0x40    /* transmitter empty */
#define LSR_TXRDY       0x20    /* transmitter ready */
#define LSR_BI          0x10    /* break detected */
#define LSR_FE          0x08    /* framing error */
#define LSR_PE          0x04    /* parity error */
#define LSR_OE          0x02    /* overrun error */
#define LSR_RXRDY       0x01    /* receiver ready */
#define LSR_RCV_MASK    0x1f

/*-----------------------------------------------------------------------=
---+
| modem status register
+------------------------------------------------------------------------=
--*/
#define MSR_DCD         0x80    /* DCD active */
#define MSR_RI          0x40    /* RI  active */
#define MSR_DSR         0x20    /* DSR active */
#define MSR_CTS         0x10    /* CTS active */
#define MSR_DDCD        0x08    /* DCD changed */
#define MSR_TERI        0x04    /* RI  changed */
#define MSR_DDSR        0x02    /* DSR changed */
#define MSR_DCTS        0x01    /* CTS changed */



------=_NextPart_000_0010_01C0C9BB.44B761B0
Content-Type: application/octet-stream;
	name="EMac.h"
Content-Transfer-Encoding: quoted-printable
Content-Disposition: attachment;
	filename="EMac.h"

/*=3D=3D=3D=3D=3D=3D=3D=3D=3D=3D=3D=3D=3D=3D=3D=3D=3D=3D=3D=3D=3D=3D=3D=3D=
=3D=3D=3D=3D=3D=3D=3D=3D=3D=3D=3D=3D=3D=3D=3D=3D=3D=3D=3D=3D=3D=3D=3D=3D=3D=
=3D=3D=3D=3D=3D=3D=3D=3D=3D=3D=3D=3D=3D=3D=3D=3D=3D=3D=3D=3D=3D=3D=3D=3D=3D=
+
|									   										|
|   Title:   EMac.h							 								|
|									   										|
|   Author:  Michael Ngo						    						|
|									   										|
|   Date:    09/10/00.						      							|
|									   										|
+=3D=3D=3D=3D=3D=3D=3D=3D=3D=3D=3D=3D=3D=3D=3D=3D=3D=3D=3D=3D=3D=3D=3D=3D=
=3D=3D=3D=3D=3D=3D=3D=3D=3D=3D=3D=3D=3D=3D=3D=3D=3D=3D=3D=3D=3D=3D=3D=3D=3D=
=3D=3D=3D=3D=3D=3D=3D=3D=3D=3D=3D=3D=3D=3D=3D=3D=3D=3D=3D=3D=3D=3D=3D=3D=3D=
=3D+
|   $Header:
+=3D=3D=3D=3D=3D=3D=3D=3D=3D=3D=3D=3D=3D=3D=3D=3D=3D=3D=3D=3D=3D=3D=3D=3D=
=3D=3D=3D=3D=3D=3D=3D=3D=3D=3D=3D=3D=3D=3D=3D=3D=3D=3D=3D=3D=3D=3D=3D=3D=3D=
=3D=3D=3D=3D=3D=3D=3D=3D=3D=3D=3D=3D=3D=3D=3D=3D=3D=3D=3D=3D=3D=3D=3D=3D=3D=
=3D+
|									   										|
|   Description:							    							|
|     This file defines Triad architecture specific constants.	      		|
|     									    								|
|									   										|
|   Special Notes:							  								|
|									   										|
|									   										|
+=3D=3D=3D=3D=3D=3D=3D=3D=3D=3D=3D=3D=3D=3D=3D=3D=3D=3D=3D=3D=3D=3D=3D=3D=
=3D=3D=3D=3D=3D=3D=3D=3D=3D=3D=3D=3D=3D=3D=3D=3D=3D=3D=3D=3D=3D=3D=3D=3D=3D=
=3D=3D=3D=3D=3D=3D=3D=3D=3D=3D=3D=3D=3D=3D=3D=3D=3D=3D=3D=3D=3D=3D=3D=3D=3D=
*/

/*-----------------------------------------------------------------------=
---+
|  EThernet MAC0/1
+------------------------------------------------------------------------=
--*/
#define	MAC0_BASE	0xB8600000	/* Base address to MAC0 Controller */
#define	MAC1_BASE	0xB8700000	/* Base address to MAC0 Controller */



/*-----------------------------------------------------------------------=
---+
| Ethernet MAC-0 and MAC-1 Registers Addresses
+------------------------------------------------------------------------=
--*/
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

			=09





------=_NextPart_000_0010_01C0C9BB.44B761B0--
