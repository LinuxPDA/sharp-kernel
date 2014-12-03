/*
 * Copyright (c) 2003 Lineo uSolutions, Inc.
 *
 *  linux/drivers/char/serial_dbmx1.h
 *
 */

#define	URXDn(n)	(0x00 + (n) * 4)
#define	UTXnD(n)	(0x40 + (n) * 4)
#define	UCR1		0x80
#define	UCR2		0x84
#define	UCR3		0x88
#define	UCR4		0x8c
#define	UFCR		0x90
#define	USR1		0x94
#define	USR2		0x98
#define	UESC		0x9c
#define	UTIM		0xa0
#define	UBIR		0xa4
#define	UBMR		0xa8
#define	UBRC		0xac
#define	BIPR1		0xb0
#define	BIPR2		0xb4
#define	BIPR3		0xb8
#define	BIPR4		0xbc
#define	BMPR1		0xc0
#define	BMPR2		0xc4
#define	BMPR3		0xc8
#define	BMPR4		0xcc
#define	UTS		0xd0

/*
  URXDn -- UART Receiver Register n
*/

#define	URXDn_CHARRDY	(1 << 15)
#define	URXDn_ERR	(1 << 14)
#define	URXDn_OVRRUN	(1 << 13)
#define	URXDn_FRMERR	(1 << 12)
#define	URXDn_BRK	(1 << 11)
#define	URXDn_PRERR	(1 << 10)

/*
  UCR1 -- UART Control Register 1
*/

#define	UCR1_ADEN	(1 << 15)
#define	UCR1_ADBR	(1 << 14)
#define	UCR1_TRDYEN	(1 << 13)
#define	UCR1_IDEN	(1 << 12)
#define	UCR1_ICD	(3 << 10)
#define	UCR1_RRDYEN	(1 << 9)
#define	UCR1_RDMAEN	(1 << 8)
#define	UCR1_IREN	(1 << 7)
#define	UCR1_TXMPTYEN	(1 << 6)
#define	UCR1_RTSDEN	(1 << 5)
#define	UCR1_SNDBRK	(1 << 4)
#define	UCR1_TDMAEN	(1 << 3)
#define	UCR1_UARTCLKEN	(1 << 2)
#define	UCR1_DOZE	(1 << 1)
#define	UCR1_UARTEN	(1 << 0)

/*
  UCR2 -- UART Control Register 2
*/

#define	UCR2_ESCI	(1 << 15)
#define	UCR2_IRTS	(1 << 14)
#define	UCR2_CTSC	(1 << 13)
#define	UCR2_CTS	(1 << 12)
#define	UCR2_ESCEN	(1 << 11)
#define	UCR2_RTEC	(3 << 9)
#define	UCR2_PREN	(1 << 8)
#define	UCR2_PROE	(1 << 7)
#define	UCR2_STPB	(1 << 6)
#define	UCR2_WS		(1 << 5)
#define	UCR2_RTSEN	(1 << 4)
#define	UCR2_TXEN	(1 << 2)
#define	UCR2_RXEN	(1 << 1)
#define	UCR2_SRST	(1 << 0)

/*
  UCR3 -- UART Control Register 3
*/

#define	UCR3_DPEC	(3 << 14)
#define	UCR3_DTREN	(1 << 13)
#define	UCR3_PARERREN	(1 << 12)
#define	UCR3_FRAERREN	(1 << 11)
#define	UCR3_DSR	(1 << 10)
#define	UCR3_DCD	(1 << 9)
#define	UCR3_RI		(1 << 8)
#define	UCR3_RXDSEN	(1 << 6)
#define	UCR3_AIRINTEN	(1 << 5)
#define	UCR3_AWAKEN	(1 << 4)
#define	UCR3_REF25	(1 << 3)
#define	UCR3_REF30	(1 << 2)
#define	UCR3_INVT	(1 << 1)
#define	UCR3_BPEN	(1 << 0)

/*
  UCR4 -- UART Control Register 4
*/

#define	UCR4_CTSTL	(63 << 10)
#define	UCR4_INVR	(1 << 9)
#define	UCR4_ENIRI	(1 << 8)
#define	UCR4_WKEN	(1 << 7)
#define	UCR4_REF16	(1 << 6)
#define	UCR4_IRSC	(1 << 5)
#define	UCR4_TCEN	(1 << 3)
#define	UCR4_BKEN	(1 << 2)
#define	UCR4_OREN	(1 << 1)
#define	UCR4_DREN	(1 << 0)

/*
  USR1 -- UART Status Register 1
*/

#define	USR1_PARITYERR	(1 << 15)
#define	USR1_RTSS	(1 << 14)
#define	USR1_TRDY	(1 << 13)
#define	USR1_RTSD	(1 << 12)
#define	USR1_ESCF	(1 << 11)
#define	USR1_FRAMERR	(1 << 10)
#define	USR1_RRDY	(1 << 9)
#define	USR1_RXDS	(1 << 6)
#define	USR1_AIRINT	(1 << 5)
#define	USR1_AWAKE	(1 << 4)

/*
  USR2 -- UART Status Register 2
*/

#define	USR2_ADET	(1 << 15)
#define	USR2_TXFE	(1 << 14)
#define	USR2_DTRF	(1 << 13)
#define	USR2_IDLE	(1 << 12)
#define	USR2_IRINT	(1 << 8)
#define	USR2_WAKE	(1 << 7)
#define	USR2_RTSF	(1 << 4)
#define	USR2_TXDC	(1 << 3)
#define	USR2_BRCD	(1 << 2)
#define	USR2_ORE	(1 << 1)
#define	USR2_RDR	(1 << 0)

/*
  UTS -- UART Test Register
*/

#define	UTS_FRCPERR	(1 << 13)
#define	UTS_LOOP	(1 << 12)
#define	UTS_TXEMPTY	(1 << 6)
#define	UTS_RXEMPTY	(1 << 5)
#define	UTS_TXFULL	(1 << 4)
#define	UTS_RXFULL	(1 << 3)
#define	UTS_SOFTRST	(1 << 0)

struct dbmx1_async_struct {
	int			magic;
	unsigned long		port;
	int			hub6;
	int			flags;
	int			xmit_fifo_size;
	struct serial_state	*state;
	struct tty_struct 	*tty;
	int			read_status_mask;
	int			ignore_status_mask;
	int			timeout;
	int			quot;
	int			x_char;	/* xon/xoff character */
	int			close_delay;
	unsigned short		closing_wait;
	unsigned short		closing_wait2;
	int			ucr1;
	int			ucr2;
	int			ucr3;
	int			usr1;
	int			usr2;
	int			uts;
	int			LCR;
	int			ACR;
	unsigned long		event;
	unsigned long		last_active;
	int			line;
	int			blocked_open; /* # of blocked opens */
	long			session; /* Session of opening process */
	long			pgrp; /* pgrp of opening process */
 	struct circ_buf		xmit;
 	spinlock_t		xmit_lock;
	u8			*iomem_base;
	u16			iomem_reg_shift;
	int			io_type;
	struct tq_struct	tqueue;
#ifdef DECLARE_WAITQUEUE
	wait_queue_head_t	open_wait;
	wait_queue_head_t	close_wait;
	wait_queue_head_t	delta_msr_wait;
#else	
	struct wait_queue	*open_wait;
	struct wait_queue	*close_wait;
	struct wait_queue	*delta_msr_wait;
#endif	
	struct dbmx1_async_struct	*next_port; /* For the linked list */
	struct dbmx1_async_struct	*prev_port;
};
