/*
 *  * For the close wait times, 0 means wait forever for serial port to
 *   * flush its output.  65535 means don't wait at all.
 *    */
#define S_CLOSING_WAIT_INF	0
#define S_CLOSING_WAIT_NONE	65535

  

/*
 *  * Definitions for S_struct (and serial_struct) flags field
 *   */
#define S_HUP_NOTIFY 0x0001 	/* Notify getty on hangups and closes 
			       				   on the callout port */
#define S_FOURPORT  0x0002	/* Set OU1, OUT2 per AST Fourport settings */
#define S_SAK	0x0004		/* Secure Attention Key (Orange book) */
#define S_SPLIT_TERMIOS 0x0008 		/* Separate termios for dialin/callout
					 */

#define S_SPD_MASK	0x0030
#define S_SPD_HI	0x0010		/* Use 56000 instead of 38400 bps */

#define S_SPD_VHI	0x0020  	/* Use 115200 instead of 38400 bps */
#define S_SPD_CUST	0x0030  	/* Use user-specified divisor */

#define S_SKIP_TEST	0x0040 		/* Skip UART test during 
					   autoconfiguration */
#define S_AUTO_IRQ  0x0080 		/* Do automatic IRQ during 
					   autoconfiguration */
#define S_SESSION_LOCKOUT 0x0100 	/* Lock out cua opens based 
					   on session */
#define S_PGRP_LOCKOUT    0x0200 	/* Lock out cua opens based on pgrp */
#define S_CALLOUT_NOHUP   0x0400 	/* Don't do hangups for cua device */

#define S_FLAGS	0x0FFF			/* Possible legal S flags */
#define S_USR_MASK 0x0430		/* Legal flags that non-privileged
				 	* users can set or reset */


/*the address of the uart_1 registers*/
#define PORT1_URXD_ADD	IO_ADDRESS(0x00206000)

#define PORT1_UTXD_ADD	IO_ADDRESS(0x00206040)

#define	PORT1_UCR1_ADD	IO_ADDRESS(0x00206080)
#define PORT1_UCR2_ADD	IO_ADDRESS(0x00206084)
#define PORT1_UCR3_ADD	IO_ADDRESS(0x00206088)
#define PORT1_UCR4_ADD	IO_ADDRESS(0x0020608c)
#define PORT1_UFCR_ADD	IO_ADDRESS(0x00206090)

#define PORT1_UBIR_ADD	IO_ADDRESS(0x002060a4)
#define PORT1_UBMR_ADD	IO_ADDRESS(0x002060a8)

#define PORT1_USR1_ADD	IO_ADDRESS(0x00206094)
#define PORT1_USR2_ADD	IO_ADDRESS(0x00206098)

/*the address of the uart_2 registers*/
#define PORT2_URXD_ADD	IO_ADDRESS(0x00207000)

#define PORT2_UTXD_ADD	IO_ADDRESS(0x00207040)

#define	PORT2_UCR1_ADD	IO_ADDRESS(0x00207080)
#define PORT2_UCR2_ADD	IO_ADDRESS(0x00207084)
#define PORT2_UCR3_ADD	IO_ADDRESS(0x00207088)
#define PORT2_UCR4_ADD	IO_ADDRESS(0x0020708c)
#define PORT2_UFCR_ADD	IO_ADDRESS(0x00207090)

#define PORT2_UBIR_ADD	IO_ADDRESS(0x002070a4)
#define PORT2_UBMR_ADD	IO_ADDRESS(0x002070a8)

#define PORT2_USR1_ADD	IO_ADDRESS(0x00207094)
#define PORT2_USR2_ADD	IO_ADDRESS(0x00207098)

#ifdef 	UNIT_TEST
#define	INTENABLEH_ADD	IO_ADDRESS(0x00223010)	
#define	INTENABLEL_ADD	IO_ADDRESS(0x00223014)
#define	NIPNDL_ADD	IO_ADDRESS(0x0022305C)
#define INTSRCL_ADD	IO_ADDRESS(0x0022304C)
#endif
//Port C
//#define PORT_C_BASE_ADDRESS	IO_ADDRESS(0x0021C200)
//#define GIUS_C	(*( volatile unsigned int *) (PORT_C_BASE_ADDRESS + 0x20)) // GPIO IN USE Register
//#define GPR_C	(*( volatile unsigned int *) (PORT_C_BASE_ADDRESS + 0x38)) // GPIO GENERAL PURPOSE Register
#define	GIUS_C_ADD	IO_ADDRESS(0x0021C220)
#define	GPR_C_ADD	IO_ADDRESS(0x0021C238)

/*the io control command*/
#define TIOCSETDMA      0x54fe
#define TIOCSETISR      0x54ff

struct serial_struct
{
	int	type;
	int	line;
	int	port;
	int	irq;
	int	flags;
	int	xmit_fifo_size;
	int	custom_divisor;
	int	baud_base;
	unsigned short	close_delay;
	char	reserved_char[2];
	int	hub6;  /* FIXME: We don't have AT&T Hub6 boards! */
	unsigned short	closing_wait; /* time to wait before closing */
	unsigned short	closing_wait2; /* no longer used... */
	int	reserved[4];
};




struct dbmx_serial
{
	char soft_carrier;		/* Use soft carrier on this channel */
	char break_abort; 		/* Is serial console in, so process brk/abrt */
	char is_cons;		       /* Is this our console. */

					/* We need to know the current clock divisor to read 
					the bps rate the chip has currently loaded. */
	unsigned char clk_divisor;	 /* May be 1, 16, 32, or 64 */
	int baud;
	int magic;
	int baud_base;
	int port;
	int irq;
	int flags;	 		/* defined in tty.h */
	int type; 			/* UART type */
	struct tty_struct *tty;
	int read_status_mask;
	int ignore_status_mask;
	int timeout;
	int xmit_fifo_size;
	int custom_divisor;
	int x_char;			/* xon/xoff character */
	int close_delay;
	unsigned short closing_wait;
	unsigned short closing_wait2;
	unsigned long event;
	unsigned long last_active;
	int line;
	int count;			/* # of fd on device */
	int blocked_open;		/* # of blocked opens */
	long session;			/* Session of opening process */
	long pgrp;			/* pgrp of opening process */
	unsigned char *xmit_buf;
	int xmit_head;
	int xmit_tail;
	int xmit_cnt;
	struct tq_struct tqueue;
	struct tq_struct tqueue_hangup;
	struct termios normal_termios;
	struct termios callout_termios;
	wait_queue_head_t open_wait;
	wait_queue_head_t close_wait;
	int dma_enable;
	int dma_write;
	wait_queue_head_t	wq;
	
};


/* The size of the serial xmit buffer is 1 page, or 4096 bytes */
#define SERIAL_XMIT_SIZE 4096
  
/*the dma channel number*/
#define CHANNUMR		1
#define CHANNUMW		2

/*the interrupt number*/
#define UART_IRQ_NUM1		30	
#define UART_IRQ_NUM2		29	

#define UART_2_IRQ_NUM1         24      
#define UART_2_IRQ_NUM2         23

#define UART1_WAKEUP_INT_NUM	28
#define UART2_WAKEUP_INT_NUM    22 

#define SERIAL_MAGIC	0x5301


/*the bitwise defination of UCR1*/
#define SNDBRK		0x00000010
#define UARTCLKEN	0x00000004
#define UARTEN		0x00000001
#define RRDYEN		0x00000200
#define IDEN		0x00001000
#define IREN		0x00000080
#define TRDYEN		0x00002000
#define TXMPTYEN	0x00000040

/*the bitwise defination of UCR2*/
#define IRTS	0x00004000
#define CTSC	0x00002000
#define CTS	0x00001000
#define PREN	0x00000100
#define PROE	0x00000080
#define STPB	0x00000040
#define WS	0x00000020
#define TXEN	0x00000004
#define RXEN	0x00000002
#define SRST	0x0000fffe

/*the bitwise defination of UCR3*/
#define RXDSEN	0x00000040
#define INVT	0x00000002
#define AWAKEN	0x00000010

/*the bitwise defination of UCR4*/
#define INVR	0x00000200
#define DREN	0x00000001
#define REF16	0x00000040
#define IRSC	0x00000020

/*the bitwise defination of URXD*/
#define CHARRDY	0x00008000
#define OVRRUN	0x00002000
#define FRMERR	0x00001000
#define PRERR	0x00000400

/*the bitwise defination of USR1*/
#define RTSS		0x00004000
#define RRDY		0x00000200
#define TIMEOUT		0x00000080
#define AWAKE		0x00000010
#define TRDY		0x00002000

/*the bitwise defination of USR2*/
#define RDR	0x00000001
#define TXDC	0x00000008
#define IDLE    0x00001000
/*the interrupt mask*/
#define UCR1_TX_INT_MASK	0x00002040
#define UCR1_RX_INT_MASK	0x00000200

/*the interrupt enable bit*/
#define UCR1_TRDYEN		0x00002000
#define UCR1_TXMPTYEN		0x00000040
/* Internal flags used only by kernel/chr_drv/serial.c */
#define S_INITIALIZED	0x80000000 /* Serial port was initialized */
#define S_CALLOUT_ACTIVE	0x40000000 /* Call out device is active */
#define S_NORMAL_ACTIVE	0x20000000 /* Normal device is active */
#define S_BOOT_AUTOCONF	0x10000000 /* Autoconfigure port on bootup */
#define S_CLOSING		0x08000000 /* Serial port is closing */
#define S_CTS_FLOW		0x04000000 /* Do CTS flow control */
#define S_CHECK_CD		0x02000000 /* i.e., CLOCAL */


/* number of characters left in xmit buffer before we ask for more */
#define WAKEUP_CHARS 512


/*
 *  * Events are used to schedule things to happen at timer-interrupt
 *   * time, instead of at rs interrupt time.
 *    */
#define RS_EVENT_WRITE_WAKEUP	0

#ifndef	IRQ_FLG_STD
#define IRQ_FLG_STD	(0x8000)
#endif

#define IPG_CLOCK	2268554		/*???*/




extern void interruptible_sleep_on(wait_queue_head_t *q);
