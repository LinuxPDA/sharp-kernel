/************************************************************************/
/*                                                                      */
/*  mcf_qspi.c - QSPI driver for MCF5272                                */
/*                                                                      */
/*  (C) Copyright 2001, Wayne Roberts (wroberts1@home.com)              */
/*                                                                      */
/*  Driver has an 8bit mode, and a 16bit mode.                          */
/*  Transfer size QMR[BITS] is set thru QSPIIOCS_BITS.                  */
/*  When size is 8, driver works normally:                              */
/*        a char is sent for every transfer                             */
/*  When size is 9 to 16bits, driver reads & writes the QDRs with       */
/*  the buffer cast to unsigned shorts.  The QTR & QRR registers can    */
/*  be filled with up to 16bits.  The length passed to read/write must  */
/*  be of the number of chars (2x number of shorts). This has been      */
/*  tested with 10bit a/d and d/a converters.                           */
/*                                                                      */
/*  * QSPIIOCS_READDATA:                                                */
/*    data to send out during read                                      */
/*  * all other ioctls are global                                       */
/* -------------------------------------------------------------------- */
/*  Ported to linux-2.4.x by Ron Fial (ron@fial.com) August 26,2002     */
/*                                                                      */
/*   Added new include files                                            */
/*   Added module_init(),exit(),                                        */
/*   qspi_read(),qspi_write():  Revised qspi_read & write argument      */
/*     processing to handle new *filep argument. Changed i_rdev access  */
/*     to use filep->f_dentry->d_inode->i_rdev  Changed memcpy_fromfs() */
/*     to memcpy().                                                     */
/*   Added '__init' to compiled-in init routine for memory recovery     */
/*   Added '__exit' for loadable-driver module cleanup routine          */ 
/*   changed  register_chrdev to  devfs_register_chrdev                 */
/*   changed  unregister_chrdev to  devfs_unregister_chrdev             */
/*   Changed various declarations from int to ssize_t or loff_t         */
/* -------------------------------------------------------------------- */
/*   Changed interruptible_sleep_on to sleep_on so the driver has       */
/*           chance to finish the current transfer before application   */
/*           quits when typing '^C'. Otherwise a write collision will   */
/*           most likely occur.                                         */
/*   Added   safe_flags(); cli; and restore_flags() according to        */
/*           gerg@snapgear.com. Otherwise in some cases (higher clock   */
/*           rates) the transfer is finished before the current process */
/*           is put to sleep and therefore never wakes up again.        */
/*           09/12/2002 richard@opentcp.org                             */
/************************************************************************/

/* **********************************************************************
Chapter 14. (excerpt) Queued Serial Peripheral Interface (QSPI) Module
   From:  http://e-www.motorola.com/brdata/PDFDB/docs/MCF5272UM.pdf 

The following steps are necessary to set up the QSPI 12-bit data transfers 
and a QSPI_CLK of 4.125 MHz. The QSPI RAM is set up for a queue of 16 
transfers. All four QSPI_CS signals are used in this example.

1. Enable all QSPI_CS pins on the MCF5272. Write PACNT with 0x0080_4000 to
enable QSPI_CS1 and QSPI_CS3.Write PDCNT with 0x0000_0030 to enable QSPI_CS2.

2. Write the QMR with 0xB308 to set up 12-bit data words with the data 
shifted on the falling clock edge, and a clock frequency of 4.125 MHz 
(assuming a 66-MHz CLKIN).

3. Write QDLYR with the desired delays.

4. Write QIR with 0xD00F to enable write collision, abort bus errors, and 
clear any interrupts.

5. Write QAR with 0x0020 to select the first command RAM entry.

6. Write QDR with 0x7E00, 0x7E00, 0x7E00, 0x7E00, 0x7D00, 0x7D00, 0x7D00,
0x7D00, 0x7B00, 0x7B00, 0x7B00, 0x7B00, 0x7700, 0x7700, 0x7700, and 0x7700
to set up four transfers for each chip select. The chip selects are active 
low in this example.  NOTE: QDR value auto-increments after each write.

7. Write QAR with 0x0000 to select the first transmit RAM entry.

8. Write QDR with sixteen 12-bit words of data.

9. Write QWR with 0x0F00 to set up a queue beginning at entry 0 and ending 
at entry 15.

10. Set QDLYR[SPE] to enable the transfers.

11.Wait until the transfers are complete. QIR[SPIF] is set when the 
transfers are complete.

12. Write QAR with 0x0010 to select the first receive RAM entry.

13. Read QDR to get the received data for each transfer.  NOTE: QDR 
auto-increments.

14. Repeat steps 5 through 13 to do another transfer.

************************************************************************* */


#include <linux/module.h>
#include <linux/version.h>
#include <linux/types.h>
#include <linux/errno.h>

#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/fcntl.h>
#include <linux/fs.h>


#if CONFIG_MODVERSIONS==1
#define MODVERSIONS
#include <linux/modversions.h>
#endif        


#include <linux/devfs_fs_kernel.h>


/*   #if LINUX_VERSION_CODE > KERNEL_VERSION(2,2,0)  */
#include <asm/uaccess.h> /* for put_user */
/*    #endif   */


#include <linux/poll.h>
#include <asm/uaccess.h>
#include <asm/system.h>
#include <linux/init.h>   /* added by Ron so driver builds directly into kernel */


#include <asm/io.h>
#include <asm/irq.h>

#include <linux/signal.h>
#include <linux/sched.h>
#include <asm/coldfire.h>   /* gets us MCF_MBAR value */
#include <asm/m5272sim.h>   /* gets us MCFSIM_ICR4, MCFSIM_PACNT */

#include "mcf_qspi.h"

#define DEVICE_NAME "qspi"

int __init qspi_init(void);
int init(void);
int init_module(void);
void __exit cleanup_module(void);


/* struct wait_queue *wqueue;   */
static DECLARE_WAIT_QUEUE_HEAD(wqueue);   /* use ver 2.4 static declaration - ron */
  /* or should we use   wait_queue_heat_t *wqueue   ?? see page 141  */

static unsigned char dbuf[1024];

/*  static struct semaphore sem = MUTEX;   */


#if LINUX_VERSION_CODE < 0x020100
static struct semaphore sem = MUTEX;
#else
static DECLARE_MUTEX(sem);
#endif

void qspi_interrupt(int irq, void *dev_id, struct pt_regs *regs)
{
	unsigned short qir = QIR;
	if (qir & QIR_WCEF)
		printk("WCEF ");	/* write collison */
	if (qir & QIR_ABRT)
		printk("ABRT ");	/* aborted by clearing of QDLYR[SPE] */
	if (qir & QIR_SPIF)
		wake_up(&wqueue);	/* transfer finished */

	QIR |= QIR_WCEF | QIR_ABRT | QIR_SPIF;	/* clear all bits */
}

static int qspi_ioctl(struct inode *inode, struct file *file,
				unsigned int cmd, unsigned long arg)
{
	int error, n, rc = 0;
	qspi_dev *dev = file->private_data;
	qspi_read_data *user_read_data;

	switch (cmd) {
		case QSPIIOCS_DOUT_HIZ:
			n = QMR & ~QMR_DOHIE;
			QMR = n | ((arg << 14) & QMR_DOHIE);
			break;
		case QSPIIOCS_BITS:	/* set QMR[BITS] */
			n = QMR & ~QMR_BITS;
			QMR = n | ((arg << 10) & QMR_BITS);
			break;
		case QSPIIOCG_BITS:	/* get QMR[BITS] */
			*((int *)arg) = (QMR >> 10) & 0xf;
			break;
		case QSPIIOCS_CPOL:	/* set SCK inactive state */
			n = QMR & ~QMR_CPOL;
			QMR = n | ((arg << 9) & QMR_CPOL);
			break;
		case QSPIIOCS_CPHA:	/* set SCK phase, 1=rising edge */
			n = QMR & ~QMR_CPHA;
			QMR = n | ((arg << 8) & QMR_CPHA);
			break;
		case QSPIIOCS_BAUD:	/* set SCK baud rate divider */
			n = QMR & ~QMR_BAUD;
			QMR = n | (arg & QMR_BAUD);
			break;
		case QSPIIOCS_QCD:	/* set start delay */
			n = QDLYR & ~QDLYR_QCD;
			QDLYR = n | ((arg << 8) & QDLYR_QCD);
			break;
		case QSPIIOCS_DTL:	/* set after delay */
			n = QDLYR & ~QDLYR_DTL;
			QDLYR = n | (arg & QDLYR_DTL);
			break;
		case QSPIIOCS_CONT:	/* continuous CS asserted during transfer */
			dev->qcr_cont = arg ? 1 : 0;
			break;
		case QSPIIOCS_READDATA:	/* set data to be sent during reads */
			user_read_data = (qspi_read_data *)arg;
			error = verify_area(VERIFY_READ, user_read_data,
							sizeof(qspi_read_data));
			if (error)
				return error;

			if (user_read_data->length > sizeof(user_read_data->buf))
				 return -EINVAL;

			dev = file->private_data;
                                  /* next line was memcpy_fromfs()  */
			memcpy(&dev->read_data, user_read_data,
							sizeof(qspi_read_data));
			break;
		default:
			rc = -EINVAL;
			break;
			
	}

	return rc;
}

static int qspi_open(struct inode *inode, struct file *file)
{
	qspi_dev *dev;

/*    #if LINUX_VERSION_CODE < KERNEL_VERSION(2,4,0)
		MOD_INC_USE_COUNT;
   #endif   */

	if ((dev = kmalloc(sizeof(qspi_dev), GFP_KERNEL)) == NULL) {
  /*  #if LINUX_VERSION_CODE < KERNEL_VERSION(2,4,0)
		MOD_DEC_USE_COUNT;
	#endif  */
		return -ENOMEM;
	}

	dev->read_data.length = 0;
	dev->read_data.buf[0] = 0;
	dev->read_data.buf[1] = 0;
	dev->read_data.loop = 0;
	file->private_data = dev;


	return 0;
}

static int qspi_release(struct inode *inode, struct file *file)


{
	kfree(file->private_data);

/*  #if LINUX_VERSION_CODE < KERNEL_VERSION(2,4,0)
	MOD_DEC_USE_COUNT;
	#endif    */
	
	return 0;
}


//   basic 2.4 kernel function format
// ssize_t qspi_read(struct file* w12f, char * w12c, size_t w12d, loff_t * w12e) { ; }


static ssize_t qspi_read(
	struct file * filep, 
	char * buffer,
	size_t length, 
	loff_t *off)


/******** older 2.0 kernel format **********
static int qspi_read(
        struct inode *inode,
        struct file *filep,
        char *buffer,
        int length)
********************************************/

{
        int qcr_cs, total = 0, i = 0;
        unsigned char bits, word = 0;
        qspi_dev *dev;
        int rdi = 0;

        down(&sem);

        qcr_cs = (~MINOR(filep->f_dentry->d_inode->i_rdev) << 8) & 0xf00;  /* CS for QCR */
//	qcr_cs = 0;  // temporarilly force CS0 only, until can check minor device number processing!!
        bits = (QMR >> 10) % 0x10;
        if (bits == 0 || bits > 0x08)
                word = 1;       /* 9 to 16bit transfers */

        dev = filep->private_data;

//		printk("\n READ driver -- ioctl xmit data fm dev->read_data.buf array  %x %x %x %x \n",dev->read_data.buf[0],dev->read_data.buf[1],dev->read_data.buf[2],dev->read_data.buf[3]);

        while (i < length) {
                unsigned short *sp = (unsigned short *)&buffer[i];
                unsigned char *cp = &buffer[i];
                unsigned short *rd_sp = (unsigned short *)dev->read_data.buf;
                int x, n;

                QAR = TX_RAM_START;             /* address first QTR */
                for (n=0; n<16; n++) {
                        if (rdi != -1) {
                                if (word) {
                                        QDR = rd_sp[rdi++];
                                        if (rdi == dev->read_data.length>>1)
                                                rdi = dev->read_data.loop ? 0 : -1;
                                } else {
                                        QDR = dev->read_data.buf[rdi++];
                                        if (rdi == dev->read_data.length)
                                                rdi = dev->read_data.loop ? 0 : -1;
                                }
                        } else
                                QDR = 0;

                        i++;
                        if (word)
                                i++;
                        if (i > length) break;
                }
                QAR = COMMAND_RAM_START;        /* address first QCR */
                for (x=0; x<n; x++) {
                        /* QCR write */
                        if (dev->qcr_cont) {
                                if (x == n-1 && i == length)
                                        QDR = QCR_SETUP | qcr_cs;       /* last transfer */
                                else
                                        QDR = QCR_CONT | QCR_SETUP | qcr_cs;
                        } else
                                QDR = QCR_SETUP | qcr_cs;
                }

				QWR = QWR_CSIV | ((n-1) << 8);
				QIR = QIR_SETUP;
				QDLYR |= QDLYR_SPE;

//				interruptible_sleep_on(&wqueue);
                sleep_on(&wqueue);					// changed richard@opentcp.org

                QAR = RX_RAM_START;     /* address: first QRR */
                if (word) {
                        /* 9 to 16bit transfers */
                        for (x=0; x<n; x++) {
                               put_user(*(volatile unsigned short *)(MCF_MBAR + MCFSIM_QDR),
                                                        sp++);
                        }
                } else {
                        /* 8bit transfers */
                        for (x=0; x<n; x++)
                                put_user(*(volatile unsigned short *)(MCF_MBAR + MCFSIM_QDR),
                                                                cp++);
                }

                if (word)
                        n <<= 1;

                total += n;
        }

        up(&sem);

        return total;
} 



static int qspi_write(
        struct file * filep,
        const char * buffer,
        size_t length,
        loff_t *off)
{
	int				qcr_cs, i = 0, total = 0;
	unsigned char	bits, word = 0;
	unsigned long	flag;
	qspi_dev *dev;

	down(&sem);

	bits = (QMR >> 10) % 0x10;
	if (bits == 0 || bits > 0x08)
		word = 1;	/* 9 to 16bit transfers */

	qcr_cs = (~MINOR(filep->f_dentry->d_inode->i_rdev) << 8) & 0xf00;	/* CS for QCR */
                                  /* next line was memcpy_fromfs()  */
//	qcr_cs = 0; // for testing, to force CS0 always 
	memcpy(dbuf, buffer, length);

	dev = filep->private_data;

//	printk("data to write is %x  %x  %x  %X  \n",dbuf[0],dbuf[1],dbuf[2],dbuf[3]);


	while (i < length) {
		int x, n;
		QAR = TX_RAM_START;		/* address: first QTR */
		if (word) {
			for (n=0; n<16; ) {
				QDR = (dbuf[i] << 8) + dbuf[i+1];	/* tx data: QTR write */
				n++;
				i += 2;
				if (i >= length) break;
			}
		} else {
			/* 8bit transfers */
			for (n=0; n<16; ) {
				QDR = dbuf[i];	/* tx data: QTR write */
				n++;
				i++;
				if (i == length) break;
			}
		}
		QAR = COMMAND_RAM_START;	/* address: first QCR */
		for (x=0; x<n; x++) {
			/* QCR write */
			if (dev->qcr_cont) {
				if (x == n-1 && i == length)
					QDR = QCR_SETUP | qcr_cs;	/* last transfer */
				else
					QDR = QCR_CONT | QCR_SETUP | qcr_cs;
			} else
				QDR = QCR_SETUP | qcr_cs;
		}

		QWR = QWR_CSIV | ((n-1) << 8);	/* QWR[ENDQP] = n << 8 */
		QIR = QIR_SETUP;
		save_flags(flag); cli();		// added according to gerg@snapgear.com
		QDLYR |= QDLYR_SPE;

//		interruptible_sleep_on(&wqueue);
		sleep_on(&wqueue);				// changed richard@opentcp.org
		
		restore_flags(flag);			// added according to gerg@snapgear.com

		if (word)
			n <<= 1;

		total += n;
	}

	up(&sem);

	return total;
}
                 /*  fixed for 2.4 kernel, owner was ifdef'ed out for 2.0 kernel */
struct file_operations Fops = {
	owner:		THIS_MODULE,
	read:		qspi_read, 
	write:		qspi_write,
	ioctl:		qspi_ioctl,
	open:		qspi_open,
	release:	qspi_release  /* a.k.a. close */
};


int
init()
{
	volatile unsigned long *lp;

	/* common init: driver or module: */

	if (request_irq(MCFQSPI_IRQ_VECTOR, qspi_interrupt, SA_INTERRUPT, "ColdFire QSPI", NULL)) {
		printk("QSPI: Unable to attach ColdFire QSPI interrupt "
			"vector=%d\n", MCFQSPI_IRQ_VECTOR);
		return -EINVAL;
	}

	/* set our IPL */
    lp = (volatile unsigned long *)(MCF_MBAR + MCFSIM_ICR4);
    *lp = (*lp & 0x07777777) | 0xd0000000;

	/* 1) CS pin setup 17.2.x
	 *	Dout, clk, cs0 always enabled. Din, cs[3:1] must be enabled.
	 *	CS1: PACNT[23:22] = 10
	 *	CS1: PBCNT[23:22] = 10 ?
	 *	CS2: PDCNT[05:04] = 11
	 *	CS3: PACNT[15:14] = 01
	 */
    lp = (volatile unsigned long *)(MCF_MBAR + MCFSIM_PACNT);
    *lp = (*lp & 0xFF3F3FFF) | 0x00804000;  /* 17.2.1 QSPI CS1 & CS3 */
    lp = (volatile unsigned long *)(MCF_MBAR + MCFSIM_PDCNT);
    *lp = (*lp & 0xFFFFFFCF) | 0x00000030;  /* QSPI_CS2 */

/*
 *  These values have to be setup according to the applications
 *  using the qspi driver. Maybe some #defines at the beginning
 *  would be more appropriate. Especially the transfer size
 *  and speed settings
 */

	QMR = 0xA1A2;// default mode setup: 8 bits, baud, 160kHz clk.
//	QMR = 0x81A2;// default mode setup: 16 bits, baud, 160kHz clk.
	QDLYR = 0x0202;	// default start & end delays

	init_waitqueue_head(&wqueue);    /* was init_waitqueue() --Ron */

	printk("MCF5272 QSPI driver ok\n");

	return 0;
}

/* init for compiled-in driver: call from mem.c */
int __init qspi_init()                                  /* the __init added by ron  */
{
	int ret;

	if ((ret = devfs_register_chrdev(QSPI_MAJOR, DEVICE_NAME, &Fops) < 0)) {
		printk ("%s device failed with %d\n",
			"Sorry, registering the character", ret);
		return ret;
	}
        printk ("QSPI device driver installed OK\n");
	return init();
}

/* init for module driver */
int
init_module()
{
	int ret;

	if ((ret = devfs_register_chrdev(QSPI_MAJOR, DEVICE_NAME, &Fops) < 0)) {
		printk ("%s device failed with %d\n",
			"Sorry, registering the character", ret);
		return ret;
	}

	return init();
}

/* Cleanup - undid whatever init_module did */
void __exit cleanup_module()      /* the __exit added by ron  */
{
	int ret;

	free_irq(MCFQSPI_IRQ_VECTOR, NULL);

	/* zero our IPL */
	*((volatile unsigned long *)(MCF_MBAR + MCFSIM_ICR4)) = 0x80000000;

	/* Unregister the device */
	if ((ret = devfs_unregister_chrdev(QSPI_MAJOR, DEVICE_NAME)) < 0)
		printk("Error in unregister_chrdev: %d\n", ret);
}

module_init(qspi_init);      /*   added by ron so driver can compile directly into kernel */
module_exit(cleanup_module);  /*   added by ron so driver can compile directly into kernel */

