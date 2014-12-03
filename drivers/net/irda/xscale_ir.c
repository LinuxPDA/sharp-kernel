/*
 * linux/drivers/net/irda/xscale_ir.c
 *
 * Infra-red driver for the Xscale embedded microprocessor.
 * Based on linux/drivers/net/irda/sa1100_ir.c
 *
 * (C) 2000 Russell King
 *
 * We currently only use HP-SIR mode.
 *
 * Change Log
 *	12-Nov-2001 Lineo Japan, Inc.
 *	16-Apr-2002 Steve Lin
 *	19-Apr-2002 Sharp
 */
#include <linux/config.h>
#include <linux/module.h>
#include <linux/types.h>
#include <linux/init.h>
#include <linux/errno.h>
#include <linux/netdevice.h>
#include <linux/slab.h>
#include <linux/rtnetlink.h>
#include <linux/interrupt.h>
#include <linux/pci.h>
#include <linux/pm.h>

#include <net/irda/irda.h>
#include <net/irda/irmod.h>
#include <net/irda/wrapper.h>
#include <net/irda/irda_device.h>
//steve TBD
#include <asm/arch/cotulla.h>
#include <asm/irq.h>
#include <asm/dma.h>
#include <asm/hardware.h>
#include <asm/mach-types.h>

//steve TBD
#include <linux/serial.h>
#include <linux/serialP.h>
#include <linux/serial_reg.h>
#include <asm/serial.h>

//MODULE_AUTHOR("Russell King <rmk@arm.linux.org.uk>");
//MODULE_DESCRIPTION("StrongARM SA1100 IrDA driver");
MODULE_AUTHOR("Steve Lin <stevelin168@hotmail.com");
MODULE_DESCRIPTION("XScale XPA210 IrDA driver");

/*
 * This should be in the SA1100 header files, rather
 * than scattering all the code with random #ifdefs
 */
#ifndef CONFIG_SA1100_ASSABET
#define BCR_clear(x)		do { } while (0)
#define BCR_set(x)		do { } while (0)
#endif

#ifndef CONFIG_SA1100_BITSY
#define clr_bitsy_egpio(x)	do { } while (0)
#define set_bitsy_egpio(x)	do { } while (0)
#endif

#ifndef GPIO_IRDA_FIR
#define GPIO_IRDA_FIR		(0)
#endif

#ifndef GPIO_IRDA_POWER
#define GPIO_IRDA_POWER		(0)
#endif

#ifndef BCR_IRDA_MD0
#define BCR_IRDA_MD0		(0)
#endif

#ifndef BCR_IRDA_MD1
#define BCR_IRDA_MD1		(0)
#endif

//steve TBD
#ifdef CONFIG_PM
#undef CONFIG_PM
#endif

/*
 * Our netdevice.  There is only ever one of these.
 */
static struct net_device *netdev;

struct pxa210_irda {
	volatile unsigned char	utcr3;
	volatile unsigned char	hscr0;
	unsigned char		utcr4;
	unsigned char		using_hssr;
	unsigned char		power;
	unsigned char		open;
	unsigned char		unused[2];

	int			speed;
	int			newspeed;

	struct sk_buff		*txskb;
	dma_addr_t		txbuf_dma;
	dma_addr_t		rxbuf_dma;
	int			txdma;
	int			rxdma;

	struct net_device_stats	stats;
	struct irlap_cb		*irlap;
	struct pm_dev		*pmdev;
	struct qos_info		qos;

	iobuff_t		tx_buff;
	iobuff_t		rx_buff;
};

static void pxa210_irda_dma_receive(struct pxa210_irda *si)
{
//steve TBD
/*	
	if ( si->hscr0 & HSCR0_TXE ) {
		printk("Got called, while HSCR0_TXE, aborting\n");
		return;
	}
	// First empty receive FIFO
	si->hscr0 &= ~HSCR0_TXE;
	Ser2HSCR0 = si->hscr0 & ~HSCR0_RXE;

	// Here one would enable DMA and stuff
	pxa210_dma_flush_all(si->rxdma);
	pxa210_dma_queue_buffer(si->rxdma, NULL, si->rxbuf_dma, si->rx_buff.truesize);
	si->hscr0 |= HSCR0_RXE;
	Ser2HSCR0 = si->hscr0;
*/	
}

static void pxa210_irda_dma_receive_stop(struct pxa210_irda *si)
{
//steve TBD
/*	
	pxa210_dma_stop(si->rxdma);
	pci_unmap_single(NULL, si->rxbuf_dma, si->rx_buff.truesize, PCI_DMA_FROMDEVICE);
	si->hscr0 &= ~(HSCR0_TXE | HSCR0_RXE);
	Ser2HSCR0 = si->hscr0;
*/
}

/*
 * Set the IrDA communications speed.
 */
static int pxa210_irda_set_speed(struct pxa210_irda *si, int speed)
{
	unsigned long flags;
	int brd, ret = -EINVAL;
	int t = si->using_hssr;
	u8 tmp;

	switch (speed) {
	case 9600:	case 19200:	case 38400:
	case 57600:	case 115200:
//steve TBD		brd = 3686400 / (16 * speed) - 1;
		brd = 14745600 / (16 * speed) ;

		save_flags(flags);
		cli();

//steve TBD
		*(COTULLA_UART3_BASE+0x04) = 0; //IER

		//set baudrate
		*(COTULLA_UART3_BASE+0x0c) |= 0x80; //LCR
		*(COTULLA_UART3_BASE+0x00) = brd; //L
		*(COTULLA_UART3_BASE+0x04) = brd >> 8; //H
		*(COTULLA_UART3_BASE+0x0c) &= 0x7f; //LCR

		*(COTULLA_UART3_BASE+0x04) = 0x41; //IER
		tmp = *(COTULLA_UART3_BASE+0x04);

/*
		if (si->using_hssr)
			pxa210_irda_dma_receive_stop(si);

		Ser2UTCR3 = 0;
		Ser2HSCR0 = si->hscr0 = HSCR0_UART;

		Ser2UTCR1 = brd >> 8;
		Ser2UTCR2 = brd;

		Ser2UTCR3 = si->utcr3 = UTCR3_RIE | UTCR3_RXE | UTCR3_TXE;
		Ser2HSCR2 = HSCR2_TrDataH | HSCR2_RcDataL;

		if (machine_is_assabet())
			BCR_clear(BCR_IRDA_FSEL);
		if (machine_is_bitsy())
			clr_bitsy_egpio(EGPIO_BITSY_IR_FSEL);
		if (machine_is_yopy())
			PPSR &= ~GPIO_IRDA_FIR;


*/

		si->speed = speed;
		si->using_hssr = 0;

		restore_flags(flags);
		ret = 0;
		break;

	case 4000000:
//steve TBD
/*
		save_flags(flags);
		cli();

		Ser2UTCR3 = si->utcr3 & ~(UTCR3_TXE | UTCR3_RXE);
		Ser2HSCR0 = si->hscr0 & ~(HSCR0_TXE | HSCR0_RXE);

		si->hscr0 = HSCR0_HSSP;
		si->utcr3 = 0;

		Ser2HSSR0 = 0xff;	// clear all error conditions
		Ser2HSCR0 = si->hscr0;
		Ser2UTCR3 = si->utcr3;
		Ser2HSCR2 = HSCR2_TrDataH | HSCR2_RcDataL;

		if (!si->using_hssr)
			si->rxbuf_dma = pci_map_single(NULL, si->rx_buff.head,
						si->rx_buff.truesize,
						PCI_DMA_FROMDEVICE);

		si->speed = speed;
		si->using_hssr = 1;

		if (machine_is_assabet())
			BCR_set(BCR_IRDA_FSEL);
		if (machine_is_bitsy())
			set_bitsy_egpio(EGPIO_BITSY_IR_FSEL);
		if (machine_is_yopy())
			PPSR |= GPIO_IRDA_FIR;

		restore_flags(flags);
		if ( t == 0 )
			pxa210_irda_dma_receive(si);
		ret = 0;
		break;
*/
	default:
		break;
	}
	return ret;
}

/*
 * This sets the IRDA power level on the Assabet.
 */
static inline int pxa210_irda_set_power_assabet(struct pxa210_irda *si, int state)
{
//steve TBD
/*	
	static int bcr_state[4] = { BCR_IRDA_MD0, BCR_IRDA_MD1|BCR_IRDA_MD0, BCR_IRDA_MD1, 0 };

	if (state < 4) {
		state = bcr_state[state];
		BCR_clear(state ^ (BCR_IRDA_MD1|BCR_IRDA_MD0));
		BCR_set(state);
	}
*/
	return 0;
}

static inline int pxa210_irda_set_power_collie(struct pxa210_irda *si, int state)
{
//steve TBD
#if 0	
	if (state) {
		/*
		 *  0 - off
		 *  1 - short range, lowest power
		 *  2 - medium range, medium power
		 *  3 - maximum range, high power
		 */
		ucb1200_set_io_direction(TC35143_GPIO_IR_ON,
					 TC35143_IODIR_OUTPUT);
		ucb1200_set_io(TC35143_GPIO_IR_ON, TC35143_IODAT_LOW);
		udelay(100);
	}
	else {
		/* OFF */
		ucb1200_set_io_direction(TC35143_GPIO_IR_ON,
					 TC35143_IODIR_OUTPUT);
		ucb1200_set_io(TC35143_GPIO_IR_ON, TC35143_IODAT_HIGH);
	}
#endif

	return 0;
}

/*
 * This turns the IRDA power on or off on the discovery
 */
static inline int xscale_irda_set_power_discovery(struct pxa210_irda *si, int state)
{
	if (state) { //on
		clr_discovery_egpio2(0x01);
	} else { //off
		set_discovery_egpio2(0x01);
	}	
	return 0;
}

/*
 * Control the power state of the IrDA transmitter.
 * State:
 *  0 - off
 *  1 - short range, lowest power
 *  2 - medium range, medium power
 *  3 - maximum range, high power
 *
 * Currently, only assabet is known to support this.
 */
static int __pxa210_irda_set_power(struct pxa210_irda *si, int state)
{
	int ret = 0;

	if (machine_is_cotulla())
		ret = xscale_irda_set_power_discovery(si, state);

	return ret;
}

static inline int pxa210_set_power(struct pxa210_irda *si, int state)
{
	int ret;

	ret = __pxa210_irda_set_power(si, state);
	if (ret == 0)
		si->power = state;

	return ret;
}

static int pxa210_irda_startup(struct pxa210_irda *si)
{
	int ret;

#if 0
	/*
	 * Ensure that the ports for this device are setup correctly.
	 */
	if (machine_is_yopy()) {
		PPDR |= GPIO_IRDA_POWER | GPIO_IRDA_FIR;
		PPSR |= GPIO_IRDA_POWER | GPIO_IRDA_FIR;
		PSDR |= GPIO_IRDA_POWER | GPIO_IRDA_FIR;
	}
	/*
	 * Enable HP-SIR modulation, and ensure that the port is disabled.
	 */
	Ser2UTCR3 = si->utcr3 = 0;
	Ser2HSCR0 = si->hscr0 = HSCR0_UART;
	Ser2UTCR4 = si->utcr4 = UTCR4_HPSIR | UTCR4_Z3_16Bit;
	Ser2UTCR0 = UTCR0_8BitData;
	/*
	 * Clear status register
	 */
	Ser2UTSR0 = UTSR0_REB | UTSR0_RBB | UTSR0_RID;
#endif

	if (machine_is_cotulla()) {
		printk("pxa210_irda_startup\n");
		//power on irda
		clr_discovery_egpio2(0x01);

		//init STUART
		*(COTULLA_UART3_BASE+0x04) = 0x40; //IER
		*(COTULLA_UART3_BASE+0x08) = 0x07; //IIR
		*(COTULLA_UART3_BASE+0x0c) = 0x03; //LCR
		*(COTULLA_UART3_BASE+0x10) = 0x03; //MCR
		*(COTULLA_UART3_BASE+0x1c) = 0x0; //SCR
		*(COTULLA_UART3_BASE+0x20) = 0x15; //irdasel
		*(COTULLA_UART3_BASE+0x0c) = 0x7f; //lcr
		
	}

	ret = pxa210_irda_set_speed(si, si->speed);
	if (ret)
		return ret;

	return 0;
}

static void pxa210_irda_shutdown(struct pxa210_irda *si)
{
	/* Disable the port. */
//	si->utcr3 = 0;
//	si->hscr0 = 0;
//	Ser2UTCR3 = 0;
//	Ser2HSCR0 = 0;

//steve TBD
	*(COTULLA_UART3_BASE+0x04) = 0x00; //IER
	*(COTULLA_UART3_BASE+0x20) = 0x00; //irdasel
	
}

#ifdef CONFIG_PM
/*
 * Suspend the IrDA interface.
 */
static int pxa210_irda_suspend(struct net_device *dev, int state)
{
	struct pxa210_irda *si = dev->priv;

	if (si && si->open) {
		/*
		 * Stop the transmit queue
		 */
		netif_device_detach(dev);
		disable_irq(dev->irq);
		__pxa210_irda_set_power(si, 0);
		pxa210_irda_shutdown(si);
	}

	return 0;
}

/*
 * Resume the IrDA interface.
 */
static int pxa210_irda_resume(struct net_device *dev)
{
	struct pxa210_irda *si = dev->priv;

	if (si && si->open) {
		/*
		 * If we missed a speed change, initialise at the new speed
		 * directly.  It is debatable whether this is actually
		 * required, but in the interests of continuing from where
		 * we left off it is desireable.  The converse argument is
		 * that we should re-negotiate at 9600 baud again.
		 */
		if (si->newspeed) {
			si->speed = si->newspeed;
			si->newspeed = 0;
		}

		pxa210_irda_startup(si);
		__pxa210_irda_set_power(si, si->power);
		enable_irq(dev->irq);

		/*
		 * This automatically wakes up the queue
		 */
		netif_device_attach(dev);
	}

	return 0;
}

static int pxa210_irda_pmproc(struct pm_dev *dev, pm_request_t rqst, void *data)
{
	int ret;

	if (!dev->data)
		return -EINVAL;

	switch (rqst) {
	case PM_SUSPEND:
		ret = pxa210_irda_suspend((struct net_device *)dev->data,
					  (int)data);
		break;

	case PM_RESUME:
		ret = pxa210_irda_resume((struct net_device *)dev->data);
		break;

	default:
		//ret = -EINVAL;
		ret = 0;
		break;
	}

	return ret;
}
#endif

/*
 * HP-SIR format interrupt service routines.
 */
static void pxa210_irda_hpsir_irq(struct net_device *dev)
{
	struct pxa210_irda *si = dev->priv;
	u8 status;

	//steve TBD
	status = *(COTULLA_UART3_BASE+0x8); //IIR
	//status = Ser2UTSR0;

	/*
	 * Deal with any receive errors first.  The bytes in error may be
	 * the only bytes in the receive FIFO, so we do this first.
	 */
	 
//steve TBD	 
//	while (status & UTSR0_EIF) { 
	while (status & 0x06) { //receive error
		
		int data;

		printk("receive fifo error!\n");
		//stat = Ser2UTSR1;
		data = *(COTULLA_UART3_BASE);
		/*
		if (stat & (UTSR1_FRE | UTSR1_ROR)) {
			si->stats.rx_errors++;
			if (stat & UTSR1_FRE)
				si->stats.rx_frame_errors++;
			if (stat & UTSR1_ROR)
				si->stats.rx_fifo_errors++;
		} else
			async_unwrap_char(dev, &si->stats, &si->rx_buff, data);
		*/
		status = *(COTULLA_UART3_BASE+0x8); //IIR
		//status = Ser2UTSR0;
	}

	/*
	 * We must clear certain bits.
	 */
	//steve TBD 
	//Ser2UTSR0 = status & (UTSR0_RID | UTSR0_RBB | UTSR0_REB);

//steve TBD
#if 0
	if (status & UTSR0_RFS) {
		/*
		 * There are at least 4 bytes in the FIFO.  Read 3 bytes
		 * and leave the rest to the block below.
		 */
		async_unwrap_char(dev, &si->stats, &si->rx_buff, Ser2UTDR);
		async_unwrap_char(dev, &si->stats, &si->rx_buff, Ser2UTDR);
		async_unwrap_char(dev, &si->stats, &si->rx_buff, Ser2UTDR);
	}

	if (status & (UTSR0_RFS | UTSR0_RID)) {
		/*
		 * Fifo contains more than 1 character.
		 */
		do {
			async_unwrap_char(dev, &si->stats, &si->rx_buff,
					  Ser2UTDR);
		} while (Ser2UTSR1 & UTSR1_RNE);

		dev->last_rx = jiffies;
	}

	if (status & UTSR0_TFS && si->tx_buff.len) {
		/*
		 * Transmitter FIFO is not full
		 */
		do {
			Ser2UTDR = *si->tx_buff.data++;
			si->tx_buff.len -= 1;
		} while (Ser2UTSR1 & UTSR1_TNF && si->tx_buff.len);

		if (si->tx_buff.len == 0) {
			
			si->stats.tx_packets++;
			si->stats.tx_bytes += si->tx_buff.data -
					      si->tx_buff.head;

			/*
			 * Still transmitting...
			 */
			si->utcr3 = UTCR3_TXE;
			Ser2UTCR3 = si->utcr3;

			/*
			 * We need to ensure that the transmitter has
			 * finished.  Reschedule ourselves for later.
			 * We may miss the other sides response, but
			 * they will retransmit.
			 */
			while (Ser2UTSR1 & UTSR1_TBY);

			/*
			 * Ok, we've finished transmitting.  Now enable
			 * the receiver.
			 */
			 
			si->utcr3 = UTCR3_RIE | UTCR3_RXE | UTCR3_TXE;
			Ser2UTCR3 = si->utcr3;

			if (si->newspeed) {
				pxa210_irda_set_speed(si, si->newspeed);
				si->newspeed = 0;
			}

			/* I'm hungry! */
			netif_wake_queue(dev);
		}
	}
#endif

/////////////////////////////////////////////
	if (status & 0x04) { //receive data available
		/*
		 * Fifo contains more than 1 character.
		 */
		do {
			async_unwrap_char(dev, &si->stats, &si->rx_buff,
					  *(COTULLA_UART3_BASE+0));
		} while ( *(COTULLA_UART3_BASE+0x14)& 0x01); //data ready

		dev->last_rx = jiffies;
	}
	
	if (status & 0x02 && si->tx_buff.len) {
		/*
		 * Transmitter FIFO is not full
		 */
		do {
			*(COTULLA_UART3_BASE+0) = *si->tx_buff.data++;
			si->tx_buff.len -= 1;
		} while ( ( *(COTULLA_UART3_BASE+0x14) & 0x20) && si->tx_buff.len);

		if (si->tx_buff.len == 0) {
			u8 tmp;
			
			si->stats.tx_packets++;
			si->stats.tx_bytes += si->tx_buff.data -
					      si->tx_buff.head;

			//si->utcr3 = UTCR3_TXE;

			while ( !(*(COTULLA_UART3_BASE+0x14) & 0x40) )
				;

			//steve TBD
			*(COTULLA_UART3_BASE+0x04) = 0x41; //IER
			tmp = *(COTULLA_UART3_BASE+0x04);
			//si->utcr3 = UTCR3_RIE | UTCR3_RXE | UTCR3_TXE;
			//Ser2UTCR3 = si->utcr3;

			if (si->newspeed) {
				pxa210_irda_set_speed(si, si->newspeed);
				si->newspeed = 0;
			}

			/* I'm hungry! */
			netif_wake_queue(dev);
		}
	}
	
}

/*
 * HSSP format interrupt service routine.
 */ 
static void pxa210_irda_hssp_irq(struct net_device *dev)
{
//steve TBD
#if 0	
	struct pxa210_irda *si = dev->priv;
	int status = Ser2HSSR0;

	printk("hssp interrupt arrived st: %x cr0: %x st1: %x\n",status,Ser2HSCR0,Ser2HSSR1);
	/*
	 * Deal with any receive errors first.  The any of the lowest 8 bytes
	 * in the FIFO may contain an error.
	 */
	if (status & HSSR0_EIF) {
		dma_addr_t dma_addr;
		int len;
		u8 *ptr;

		/*
		 * Stop DMA, and get the current data position.
		 */
		pxa210_dma_stop(si->rxdma);
		pxa210_dma_get_current(si->rxdma, NULL, &dma_addr);

		len = dma_addr - si->rxbuf_dma;
		if (len >= 2047) {// Frame is too big
			si->stats.rx_over_errors++;
			len = 0; // we will check for that later on.
		}
			
		/*
		 * Sync the DMA region.  After this call, we have
		 * control over the buffer.
		 */
		pci_dma_sync_single(NULL, si->rxbuf_dma, len,
				    PCI_DMA_FROMDEVICE);

		/*
		 * get the current pointer
		 */
		ptr = si->rx_buff.head + len;
		do {
			int stat, data;

			stat = Ser2HSSR1;
			data = Ser2HSDR;

			if (stat & (HSSR1_CRE | HSSR1_ROR)) {
				si->stats.rx_errors++;
				if (stat & HSSR1_CRE)
					si->stats.rx_crc_errors++;
				if (stat & HSSR1_ROR)
					si->stats.rx_frame_errors++;
				len = 0; // We won't submit this frame
			} else
				*ptr++ = data;

			/* end of frame? */
			if (stat & HSSR1_EOF && len != 0) {
				struct sk_buff *skb;

				len = ptr - si->rx_buff.head;

				skb = alloc_skb(len + 1, GFP_ATOMIC);
				if (skb == NULL) {
					si->stats.rx_dropped++;
					status = Ser2HSSR0;
					continue;
				}
				skb_reserve(skb, 1);
				skb_put(skb, len);
				memcpy(skb->data, si->rx_buff.head, len);
				skb->dev = dev;
				skb->mac.raw = skb->data;
				skb->protocol = htons(ETH_P_IRDA);
				netif_rx(skb);
				si->stats.rx_packets++;
				si->stats.rx_bytes += len;
				/*
				 * We do not expect anything in FIFO
				 * and if we still have something there,
				 * we'll drop that garbage later on.
				 */
			}

			status = Ser2HSSR0;
			printk("in loop st: %x cr0: %x st1: %x\n",status,Ser2HSCR0,Ser2HSSR1);
		} while (status & HSSR0_EIF);
	}

	/*
	 * Clear selected status bits now, so we
	 * don't miss them next time around.
	 */
	Ser2HSSR0 = status & (HSSR0_TUR|HSSR0_RAB|HSSR0_FRE);

	Ser2HSCR0 = si->hscr0 = si->hscr0 & ~HSCR0_RXE; // drop the garbage.

	/* Transmitter underrun */
	if (status & HSSR0_TUR)
		printk("pxa210_ir: error: transmitter underrun? %p\n",si->txskb);

	/* Receiver Abort - we lost synchronisation, start from scratch */
	// nothing that we can do about it.
        // if (status & HSSR0_RAB)

	/* Framing error */
	if (status & HSSR0_FRE)
		si->stats.rx_frame_errors++;

	pxa210_irda_dma_receive(si);

#endif	
}

static void pxa210_irda_irq(int irq, void *dev_id, struct pt_regs *regs)
{
	struct net_device *dev = dev_id;
	if (((struct pxa210_irda *)dev->priv)->using_hssr)
		pxa210_irda_hssp_irq(dev);
	else
		pxa210_irda_hpsir_irq(dev);
}

/*
 * TX dma completion handler.  Unmap the DMA buffer,
 * free the skb.  Ensure that the TX side is awake.
 */
#if 0
static void pxa210_irda_txdma_irq(void *id, int len)
{
	struct net_device *dev = id;
	struct pxa210_irda *si = dev->priv;
	struct sk_buff *skb = si->txskb;

	si->txskb = NULL;

	if (skb) {
		pci_unmap_single(NULL, si->txbuf_dma, len, PCI_DMA_TODEVICE);

		dev_kfree_skb_irq(skb);

		si->stats.tx_packets ++;
		si->stats.tx_bytes += len;

		/*
		 * Do we need to change speed?
		 */
		if (si->newspeed) {
			pxa210_irda_set_speed(si, si->newspeed);
			si->newspeed = 0;
		}

		// here we need to wait till transmission is actually completed, before we disable transmitter.
		while ( Ser2HSSR1 & HSSR1_TBY ) ;
		while ( !(Ser2HSSR0 & HSSR0_TUR) ) ;
		Ser2HSSR0 |= HSSR0_TUR;
		//printk("txirq interrupt finished st: %x cr0: %x st1: %x\n",Ser2HSSR0,Ser2HSCR0, Ser2HSSR1);
		si->hscr0 &= ~HSCR0_TXE;
		pxa210_irda_dma_receive(si);
		netif_wake_queue(dev);
	} else
		pxa210_irda_dma_receive(si);
}
#endif

/*
 * Note that we will never build up a backlog of frames; the protocol is a
 * half duplex protocol which basically means we transmit a frame, we
 * receive a frame, we transmit the next frame etc.
 */
static int pxa210_irda_hard_xmit(struct sk_buff *skb, struct net_device *dev)
{
	struct pxa210_irda *si = dev->priv;
	int speed = irda_get_next_speed(skb);
	int ret = 0;

	/*
	 * If the speed has changed, schedule an update just
	 * after we've sent this packet.
	 */
	if (speed != si->speed && speed != -1)
		si->newspeed = speed;

	/*
	 * If this is an empty frame
	 */
	if (skb->len == 0) {
		if (si->newspeed)
			pxa210_irda_set_speed(si, speed);
		si->newspeed = 0;
		dev_kfree_skb(skb);
		return 0;
	}

	if (si->using_hssr == 0) {
		unsigned long flags;

		si->tx_buff.data = si->tx_buff.head;
		si->tx_buff.len  = async_wrap_skb(skb, si->tx_buff.data,
						  si->tx_buff.truesize);
		/*
		 * Set the transmit interrupt enable.
		 * This will fire off an interrupt immediately.
		 */
		save_flags(flags);
		cli();

		//steve TBD
		*(COTULLA_UART3_BASE+0x04) = 0x42; //IER
		//si->utcr3 = UTCR3_TIE | UTCR3_TXE;
		//Ser2UTCR3 = si->utcr3;

		restore_flags(flags);

		kfree_skb(skb);
	} else {
//steve TBD: we only use SIR
/*
		if (si->txskb)
			BUG();

		if (si->hscr0 & HSCR0_TXE)
			printk("hardtx, sending with HSCR0_TXE!\n");
		si->txskb = skb;
		si->txbuf_dma = pci_map_single(NULL, skb->data,
					 skb->len, PCI_DMA_TODEVICE);

		pxa210_dma_queue_buffer(si->txdma, dev, si->txbuf_dma,
					skb->len);

		si->hscr0 = (si->hscr0 & ~HSCR0_RXE) | HSCR0_TXE;
		Ser2HSCR0 = si->hscr0;
		//printk("tx started st: %x cr0: %x st1: %x\n",Ser2HSSR0,Ser2HSCR0, Ser2HSSR1);
*/		
	}

	if (ret == 0)
		dev->trans_start = jiffies;

	return ret;
}

static int pxa210_irda_ioctl(struct net_device *dev, struct ifreq *ifreq,
			     int cmd)
{
	struct if_irda_req *rq = (struct if_irda_req *)ifreq;
	struct pxa210_irda *si = dev->priv;
	int ret = -EOPNOTSUPP;

	switch (cmd) {
	case SIOCSBANDWIDTH:
		if (capable(CAP_NET_ADMIN)) {
			/*
			 * We are unable to set the speed if the
			 * device is not running.
			 */
			if (si->open)
				ret = pxa210_irda_set_speed(si,
						rq->ifr_baudrate);
			else {
				printk("pxa210_irda_ioctl: SIOCSBANDWIDTH: !netif_running\n");
				ret = 0;
			}
		}
		break;

	case SIOCSMEDIABUSY:
		ret = -EPERM;
		if (capable(CAP_NET_ADMIN)) {
			irda_device_set_media_busy(dev, TRUE);
			ret = 0;
		}
		break;

	case SIOCGRECEIVING:
		rq->ifr_receiving = si->using_hssr
					? 0
					: si->rx_buff.state != OUTSIDE_FRAME;
		break;

	default:
		break;
	}
		
	return ret;
}

static struct net_device_stats *pxa210_irda_stats(struct net_device *dev)
{
	struct pxa210_irda *si = dev->priv;

	return &si->stats;
}

static int pxa210_irda_start(struct net_device *dev)
{
	struct pxa210_irda *si = dev->priv;
	int err;

	MOD_INC_USE_COUNT;

//steve TBD	si->speed = 9600;
	si->speed = 115200;

	err = request_irq(dev->irq, pxa210_irda_irq, 0, dev->name, dev);
	if (err)
		goto out;

	/*
	 * The interrupt must remain disabled for now.
	 */
	disable_irq(dev->irq);

	/*
	 * Setup the serial port for the specified speed.
	 */
	err = pxa210_irda_startup(si);
	if (err)
		goto out_irq;

	/*
	 * Open a new IrLAP layer instance.
	 */
	si->irlap = irlap_open(dev, &si->qos,"Discovery");
	err = -ENOMEM;
	if (!si->irlap)
		goto out_shutdown;
		
	//steve TBD
	//pxa210_dma_set_callback(si->txdma, pxa210_irda_txdma_irq);

	/*
	 * Now enable the interrupt and start the queue
	 */
	si->open = 1;
	pxa210_set_power(si, 3);
	enable_irq(dev->irq);
	netif_start_queue(dev);
	return 0;

out_shutdown:
	pxa210_irda_shutdown(si);
	si->open = 0;
out_irq:
	free_irq(dev->irq, dev);
out:
	MOD_DEC_USE_COUNT;
	return err;
}

static int pxa210_irda_stop(struct net_device *dev)
{
	struct pxa210_irda *si = dev->priv;

	disable_irq(dev->irq);

	/* if we were doing a DMA receive, make sure we close that down */
//steve TBD
/*
	if (si->using_hssr)
		pxa210_irda_dma_receive_stop(si);
*/
	/* Stop IrLAP */
	if (si->irlap) {
		irlap_close(si->irlap);
		si->irlap = NULL;
	}

	netif_stop_queue(dev);
	free_irq(dev->irq, dev);
	pxa210_set_power(si, 0);
	pxa210_irda_shutdown(si);
	si->open = 0;

	MOD_DEC_USE_COUNT;

	return 0;
}

static int pxa210_irda_init_iobuf(iobuff_t *io, int size)
{
	io->head = kmalloc(size, GFP_KERNEL | GFP_DMA);
	if (io->head != NULL) {
		io->truesize = size;
		io->in_frame = FALSE;
		io->state    = OUTSIDE_FRAME;
		io->data     = io->head;
	}
	return io->head ? 0 : -ENOMEM;
}

static int pxa210_irda_net_init(struct net_device *dev)
{
	struct pxa210_irda *si = dev->priv;
	unsigned int baudrate_mask;
	int err = -ENOMEM;
	unsigned long tmp;
	
	printk("pxa210_irda_net_init 1\n");

	si = kmalloc(sizeof(struct pxa210_irda), GFP_KERNEL);
	if (!si)
		goto out;

	memset(si, 0, sizeof(*si));

	err = pxa210_irda_init_iobuf(&si->rx_buff, 14384);
	if (err)
		goto out;
	err = pxa210_irda_init_iobuf(&si->tx_buff, 4000);
	if (err)
		goto out_free_rx;

	dev->priv = si;
	dev->hard_start_xmit	= pxa210_irda_hard_xmit;
	dev->open		= pxa210_irda_start;
	dev->stop		= pxa210_irda_stop;
	dev->do_ioctl		= pxa210_irda_ioctl;
	dev->get_stats		= pxa210_irda_stats;

	irda_device_setup(dev);
	irda_init_max_qos_capabilies(&si->qos);

	/*
	 * We support original IRDA up to 115k2. (we don't currently
	 * support 4Mbps).  Min Turn Time set to 1ms or greater.
	 */
	baudrate_mask = IR_9600|IR_19200|IR_38400|IR_57600|IR_115200;
	baudrate_mask |= (IR_4000000 << 8);
	si->qos.baud_rate.bits &= baudrate_mask;
	si->qos.min_turn_time.bits = 7;

	irda_qos_bits_to_value(&si->qos);

/*
	err = pxa210_request_dma(&si->rxdma, "IrDA receive", DMA_Ser2HSSPRd);
	if (err) {
		printk("pxa210_ir: unable to register rx dma\n");
		goto err_rx_dma;
	}
	err = pxa210_request_dma(&si->txdma, "IrDA transmit", DMA_Ser2HSSPWr);
	if (err) {
		printk("pxa210_ir: unable to register tx dma\n");
		goto err_tx_dma;
	}
*/	
#if 0
	pxa210_dma_set_device(si->rxdma, DMA_Ser2HSSPRd);
	pxa210_dma_set_device(si->txdma, DMA_Ser2HSSPWr);
#endif

	/*
	 * Initially enable HP-SIR modulation, and ensure that the port
	 * is disabled.
	 */

	//steve TBD
	//*(unsigned long *)0xf0e00000 &= ~(1<<14); //GPDR1 //gp46=0

	GPDR(1) = GPDR(1) & ~(1<<14);

//	*(unsigned long *)0xf0e00000 |= (1<<15); //GPDR1 //gp47=1
	GPDR(1) = GPDR(1) |= (1<<15);

//	*(unsigned long *)0xf0e0005c &= 0x0fffffff; //GAFR1_L //gp46=gp47=0
	GPAFR1_L = GPAFR1_L &= 0x0fffffff;

	GPAFR1_L = GPAFR1_L |= 0x60000000;
	
	clr_discovery_egpio2(0x01); //power on SIR, of extgpio2[0]

	//set baudrate to 115200
	*(COTULLA_UART3_BASE+0x0c) |= 0x80; //LCR
	*(COTULLA_UART3_BASE+0x00) = 0x08; //L
	*(COTULLA_UART3_BASE+0x04) = 0x0; //H
	*(COTULLA_UART3_BASE+0x0c) &= 0x7f; //LCR

	//
	*(COTULLA_UART3_BASE+0x04) = 0x40; //IER
	*(COTULLA_UART3_BASE+0x08) = 0x07; //IIR
	*(COTULLA_UART3_BASE+0x0c) = 0x03; //LCR
	*(COTULLA_UART3_BASE+0x10) = 0x03; //MCR
	*(COTULLA_UART3_BASE+0x1c) = 0x0; //SCR
	*(COTULLA_UART3_BASE+0x20) = 0x15; //irdasel
	*(COTULLA_UART3_BASE+0x0c) = 0x7f; //lcr

	printk("pxa210_irda_net_init done.\n");

/*	
	Ser2UTCR3 = si->utcr3 = 0;
	Ser2UTCR4 = si->utcr4 = UTCR4_HPSIR | UTCR4_Z3_16Bit;
	Ser2HSCR0 = si->hscr0 = HSCR0_UART;
*/

#ifdef CONFIG_PM
	/*
	 * Power-Management is optional.
	 */
	si->pmdev = pm_register(PM_SYS_DEV, PM_SYS_IRDA, pxa210_irda_pmproc);
	if (si->pmdev)
		si->pmdev->data = dev;
#endif

	return 0;

//steve TBD
/*
err_tx_dma:
	pxa210_free_dma(si->rxdma);
err_rx_dma:
	kfree(si->tx_buff.head);
*/
out_free_rx:
	kfree(si->rx_buff.head);

out:
	kfree(si);

	return err;
}

/*
 * Remove all traces of this driver module from the kernel, so we can't be
 * called.  Note that the device has already been stopped, so we don't have
 * to worry about that.
 */
static void pxa210_irda_net_uninit(struct net_device *dev)
{
	struct pxa210_irda *si = dev->priv;

	dev->hard_start_xmit	= NULL;
	dev->open		= NULL;
	dev->stop		= NULL;
	dev->do_ioctl		= NULL;
	dev->get_stats		= NULL;
	dev->priv		= NULL;

	pm_unregister(si->pmdev);
//steve TBD
/*
	pxa210_free_dma(si->txdma);
	pxa210_free_dma(si->rxdma);
*/
	kfree(si->tx_buff.head);
	kfree(si->rx_buff.head);
	kfree(si);
}

#ifdef MODULE
static
#endif
int __init pxa210_irda_init(void)
{
	struct net_device *dev;
	int err;

	printk("pxa210_irda_init.\n");
	rtnl_lock();
	dev = dev_alloc("irda%d", &err);
	if (dev) {
		dev->irq    = IRQ_STUART;
		dev->init   = pxa210_irda_net_init;
		dev->uninit = pxa210_irda_net_uninit;

		err = register_netdevice(dev);

		if (err)
			kfree(dev);
		else
			netdev = dev;
	}
	rtnl_unlock();

	return err;
}

static void __exit pxa210_irda_exit(void)
{
	struct net_device *dev = netdev;

	netdev = NULL;
	if (dev) {
		rtnl_lock();
		unregister_netdevice(dev);
		rtnl_unlock();
	}

	/*
	 * We now know that the netdevice is no longer in use, and all
	 * references to our driver have been removed.  The only structure
	 * which may still be present is the netdevice, which will get
	 * cleaned up by net/core/dev.c
	 */
}

#ifdef MODULE
module_init(pxa210_irda_init);
module_exit(pxa210_irda_exit);
#endif
