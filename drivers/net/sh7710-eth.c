
/*
 * sh7710-eth.c -- network driver for SH7710 based on snull.c 
 *
 * Change Log
 *     26-Jan-2004 Lineo Solutions, Inc.  Create
 */


#include <linux/config.h>
#include <linux/module.h>
#include <linux/kernel.h> /* printk() */
#include <linux/sched.h>
#include <linux/errno.h>  /* error codes */
#include <linux/ioport.h>  /* error codes */
#include <linux/fs.h>
#include <linux/ip.h>
#include <linux/types.h>  /* size_t */
#include <linux/ptrace.h>  /* size_t */
#include <linux/string.h>  /* size_t */
#include <asm/system.h>
#include <asm/uaccess.h>
#include <asm/bitops.h>
#include <asm/io.h>
#include <asm/irq.h>
#include <linux/delay.h>  /* size_t */
#include <linux/fcntl.h>
#include <linux/in.h>
#include <linux/interrupt.h> /* mark_bh */
#include <linux/init.h>
#include <linux/crc32.h>
#include <linux/netdevice.h>   /* struct device, and other headers */
#include <linux/etherdevice.h> /* eth_type_trans */

//#define DEBUG
//
#include "sh7710-eth.h"

#if defined(DEBUG)
#define 	debug_printk( args... ) printk( args)
#else
#define 	debug_printk( args... ) 
#endif

#ifdef CONFIG_SH_MS7710SE
extern unsigned char cmdline_ifa[]; // "ifa=" kernel param (MAC addr)
#endif // CONFIG_SH_SOLUTION_ENGINE

#ifdef DEBUG
// for debug
#define status_print() \
	{ \
		vu32 *regwk; \
		vu32 *regwk2; \
		vu16 *regwk16; \
		regwk = (vu32 *)(ETHER_C_0_BASE + ECMR); \
		regwk2 = (vu32 *)(ETHER_C_1_BASE + ECMR); \
		printk("E-C ECMR  :%08x %08x\n", *regwk, *regwk2); \
        \
		regwk = (vu32 *)(ETHER_C_0_BASE + ECSR); \
		regwk2 = (vu32 *)(ETHER_C_1_BASE + ECSR); \
		printk("E-C ECSR  :%08x %08x\n", *regwk, *regwk2); \
        \
		regwk = (vu32 *)(ETHER_C_0_BASE + TROCR); \
		regwk2 = (vu32 *)(ETHER_C_1_BASE + TROCR); \
		printk("E-C TROCR :%08x %08x\n", *regwk, *regwk2); \
        \
		regwk = (vu32 *)(ETHER_C_0_BASE + CDCR); \
		regwk2 = (vu32 *)(ETHER_C_1_BASE + CDCR); \
		printk("E-C CDCR  :%08x %08x\n", *regwk, *regwk2); \
        \
		regwk = (vu32 *)(ETHER_C_0_BASE + LCCR); \
		regwk2 = (vu32 *)(ETHER_C_1_BASE + LCCR); \
		printk("E-C LCCR  :%08x %08x\n", *regwk, *regwk2); \
        \
		regwk = (vu32 *)(ETHER_C_0_BASE + CNDCR); \
		regwk2 = (vu32 *)(ETHER_C_1_BASE + CNDCR); \
		printk("E-C CNDCR :%08x %08x\n", *regwk, *regwk2); \
        \
		regwk = (vu32 *)(ETHER_C_0_BASE + CEFCR); \
		regwk2 = (vu32 *)(ETHER_C_1_BASE + CEFCR); \
		printk("E-C CEFCR :%08x %08x\n", *regwk, *regwk2); \
        \
		regwk = (vu32 *)(ETHER_C_0_BASE + FRECR); \
		regwk2 = (vu32 *)(ETHER_C_1_BASE + FRECR); \
		printk("E-C FRECR :%08x %08x\n", *regwk, *regwk2); \
        \
		regwk = (vu32 *)(ETHER_C_0_BASE + PSR); \
		regwk2 = (vu32 *)(ETHER_C_1_BASE + PSR); \
		printk("E-C PSR   :%08x %08x\n", *regwk, *regwk2); \
        \
		regwk = (vu32 *)(EDMAC0_BASE + EESR); \
		regwk2 = (vu32 *)(EDMAC1_BASE + EESR); \
		printk("ED  EESR  :%08x %08x\n", *regwk, *regwk2); \
        \
		regwk = (vu32 *)(EDMAC0_BASE + EDOCR); \
		regwk2 = (vu32 *)(EDMAC1_BASE + EDOCR); \
		printk("ED  EDOCR :%08x %08x\n", *regwk, *regwk2); \
		\
		regwk = (vu32 *)(EDMAC0_BASE + EESIPR); \
		regwk2 = (vu32 *)(EDMAC1_BASE + EESIPR); \
		printk("ED  EESIPR:%08x %08x\n", *regwk, *regwk2); \
		\
		regwk = (vu32 *)(EDMAC0_BASE + EDRRR); \
		regwk2 = (vu32 *)(EDMAC1_BASE + EDRRR); \
		printk("ED  EDRRR:%08x %08x\n", *regwk, *regwk2); \
		\
		regwk16 = (vu16 *)(0xa4050106); \
		printk("PFC PETCR :%08x\n", *regwk16); \
		\
		printk("\n"); \
	} 

#if defined (CONFIG_SH_MS7710SE)
#define PHY_print() \
	{\
		vu32 *reg0,*reg1;\
\
		reg0 = (vu32 *)(ETHER_C_0_BASE + PIR);\
		reg1 = (vu32 *)(ETHER_C_1_BASE + PIR);\
		printk("PHY:BMCR	:%04x  ",PHY_read(reg0, MII_BMCR_READ));\
		printk(":%04x\n",PHY_read(reg1, MII_BMCR_READ));\
\
		printk("PHY:BMSR	:%04x  ",PHY_read(reg0, MII_BMSR_READ));\
		printk(":%04x\n",PHY_read(reg1, MII_BMSR_READ));\
\
		printk("PHY:PHYID1	:%04x  ",PHY_read(reg0, MII_PHYID1_READ));\
		printk(":%04x\n",PHY_read(reg1, MII_PHYID1_READ));\
\
		printk("PHY:PHYID2	:%04x  ",PHY_read(reg0, MII_PHYID2_READ));\
		printk(":%04x\n",PHY_read(reg1, MII_PHYID2_READ));\
\
		printk("PHY:ANAR	:%04x  ",PHY_read(reg0, MII_ANAR_READ));\
		printk(":%04x\n",PHY_read(reg1, MII_ANAR_READ));\
\
		printk("PHY:ANLAPR	:%04x  ",PHY_read(reg0, MII_ANLAPR_READ));\
		printk(":%04x\n",PHY_read(reg1, MII_ANLAPR_READ));\
\
		printk("PHY:ANER	:%04x  ",PHY_read(reg0, MII_ANER_READ));\
		printk(":%04x\n",PHY_read(reg1, MII_ANER_READ));\
\
		printk("PHY:DSCR	:%04x  ",PHY_read(reg0, MII_DSCR_READ));\
		printk(":%04x\n",PHY_read(reg1, MII_DSCR_READ));\
\
		printk("PHY:DSCSR	:%04x  ",PHY_read(reg0, MII_DSCSR_READ));\
		printk(":%04x\n",PHY_read(reg1, MII_DSCSR_READ));\
\
		printk("PHY:10BTCSR	:%04x  ",PHY_read(reg0, MII_10BTCSR_READ));\
		printk(":%04x\n",PHY_read(reg1, MII_10BTCSR_READ));\
\
		printk("PHY:INTR	:%04x  ",PHY_read(reg0, MII_INTR_READ));\
		printk(":%04x\n",PHY_read(reg1, MII_INTR_READ));\
\
		printk("PHY:RECR	:%04x  ",PHY_read(reg0, MII_RECR_READ));\
		printk(":%04x\n",PHY_read(reg1, MII_RECR_READ));\
\
		printk("PHY:DISCR	:%04x  ",PHY_read(reg0, MII_DISCR_READ));\
		printk(":%04x\n",PHY_read(reg1, MII_DISCR_READ));\
\
		printk("PHY:RLSR	:%04x  ",PHY_read(reg0, MII_RLSR_READ));\
		printk(":%04x\n",PHY_read(reg1, MII_RLSR_READ));\
\
	}
#endif

#endif // DEBUG

/* device structer for CH0,1 */
struct net_device sh7710_devs[MAC_NUM] = {
	{ init: sh7710_init,},
	{ init: sh7710_init,}
};

/* for descripter MAC0,1 */
struct sh7710_txrxdesc *txrxdesc_org; // alloc address
struct sh7710_txrxdesc *txrxdesc_bound; // adjust address to 16byte bundary
unsigned char *desc_buf_org;	// allock address 
unsigned char *desc_buf_bound;	// adjust address to 16byte bundary
int	rx_desc_idx[MAC_NUM];	// rx index 
int	tx_desc_idx[MAC_NUM];	// tx index 

/* change mtu in MAC */
int sh7710_change_mtu(struct net_device *dev, int new_mtu)
{
	unsigned long flags;
	spinlock_t *lock = &((struct sh7710_priv *) dev->priv)->lock;

	debug_printk("sh7710_change_mtu\n");
	
	/* check range */
	if ((new_mtu < 68) || (new_mtu > 1500))
		return -EINVAL;

	spin_lock_irqsave(lock, flags);
	dev->mtu = new_mtu;	 /* chenge mtu value */
	spin_unlock_irqrestore(lock, flags);

	return 0;
}

/* get statistics */
struct net_device_stats *sh7710_stats(struct net_device *dev)
{
	struct sh7710_priv *priv = (struct sh7710_priv *)dev->priv;
	
	debug_printk("sh7710_stats\n");
	
	return &priv->stats;
}

/* ioctl function */
int sh7710_ioctl(struct net_device *dev, struct ifreq * rq, int cmd)
{
	debug_printk("sh7710_ioctl\n");
	
	return 0;
}

/* MAC timeout process */
void sh7710_tx_timeout(struct net_device *dev)
{
	struct sh7710_priv *priv = (struct sh7710_priv *)dev->priv;

	debug_printk("sh7710_tx_timeout\n");
	
	priv->status = SH7710_TX_INTR;
	sh7710_interrupt(0, dev, NULL);
	priv->stats.tx_errors++;
	netif_wake_queue(dev);
	
	return;
}

/* controller init */
static int sh7710_init_controller(struct net_device *dev)
{
	int	len;
	struct sh7710_txrxdesc *tmpdescptr; // temp desc-struct ptr
	vu32 *reg;
	u32 mac_base, mac_id;
	int	i;

	debug_printk("sh7710_init_controller\n");
	
	/* request interrupt */
	if(dev - sh7710_devs){ // MAC1
		dev->irq = EDMAC_EINT1_IRQ;
		tmpdescptr = txrxdesc_bound + 1;
		mac_base = EDMAC1_BASE;
		mac_id = 1;
	}else{
		dev->irq = EDMAC_EINT0_IRQ;
		tmpdescptr = txrxdesc_bound;
		mac_base = EDMAC0_BASE;
		mac_id = 0;
	}

	sh7710_PHY_init(mac_id); // PHY initialize

	if(init_sh7710_EtherC(dev)) /* initialize H/W */
		return -ENODEV;

	if(init_sh7710_EDMAC(mac_id)) /* initialize sh7710 E-DMAC */
		return -ENODEV;

	
	/* EDMAC receive start */
	for(i = 0; i < DESC_NUM; i++){ /* set a valid flag */
		tmpdescptr->macrxd[i].td0 = RX_DESC_VALID;
		tmpdescptr->macrxd[i].td1 = DESC_BUF_SIZE_SHIFT;
	}
	// set descripter last flag rx,tx
	tmpdescptr->macrxd[DESC_NUM - 1].td0 |= TX_DESC_LAST;

	// index initialize
	rx_desc_idx[mac_id] = 0;
	tx_desc_idx[mac_id] = 0;

	reg = (vu32 *)(mac_base + RDLAR); /* set desc buf addr to E-DMAC */
	*reg = (vu32)&(tmpdescptr->macrxd[0]);

	reg = (vu32 *)(mac_base + EDRRR); /* rx request to E-DMAC */
	*reg = EDRRR_RX_REQ;

	/* set tx desc ptr to EDMAC reg */
	tmpdescptr->mactxd[DESC_NUM - 1].td0 |= RX_DESC_LAST;
	reg = (vu32 *)(mac_base + TDLAR) ;
	*reg = (vu32)&(tmpdescptr->mactxd[0]);
	
	/* get MAC(H/W) address */
	len = get_sh7710_mac_address((int)(dev - sh7710_devs), (unsigned char*)dev->dev_addr);

	return 0;
}
/* device open (ifconfig up) */
int sh7710_open(struct net_device *dev)
{
	int	len;

	debug_printk("sh7710_open\n");
	
	MOD_INC_USE_COUNT;

	if(sh7710_init_controller(dev)){
		return -ENODEV;
	}
	
	if(dev - sh7710_devs){ // MAC1
		dev->irq = EDMAC_EINT1_IRQ;
	}else{
		dev->irq = EDMAC_EINT0_IRQ;
	}

	/* get MAC(H/W) address */
	len = get_sh7710_mac_address((int)(dev - sh7710_devs), (unsigned char*)dev->dev_addr);

	if(request_irq(dev->irq, &sh7710_interrupt, SA_SHIRQ, dev->name, dev)){
		MOD_DEC_USE_COUNT;
		return -EAGAIN;
	}

	netif_start_queue(dev);

	return 0;
}

/* device close (# ifconfig down) */
int sh7710_release(struct net_device *dev)
{
	struct sh7710_priv *priv = (struct sh7710_priv *)dev->priv;

	debug_printk("sh7710_release\n");
	
	netif_stop_queue(dev);

	spin_lock(&priv->lock);

	/* stop EDMA-C Ether-C (EDMAC0/1 software reset) */
	if(dev - sh7710_devs){ // MAC1
		EDMAC_soft_reset(1);
	}else{ // MAC0
		EDMAC_soft_reset(0);
	}

#ifdef DEBUG
//	ether_reg_dump2();
#endif
	//
	free_irq(dev->irq, dev);
	spin_unlock(&priv->lock);

	MOD_DEC_USE_COUNT;

	return 0;
}

/* configulation MAC */
int sh7710_config(struct net_device *dev, struct ifmap *map)
{
	debug_printk("sh7710_config\n");

	if(dev->flags & IFF_UP) // intarface is UP. can't configuration
		return -EBUSY;

	/* Don't change I/O address */
	if(map->base_addr != dev->base_addr){
		printk(KERN_WARNING "sh7710-eth: Can't change I/O address\n");
		return -EOPNOTSUPP;
	}

	/* change irq */
	if(map->irq != dev->irq){
		dev->irq = map->irq; // request irq is delayed to open-time
	}

	return 0;
}
/* interrupt proc */
void sh7710_interrupt(int irq, void *dev_id, struct pt_regs *regs)
{
	struct sh7710_priv *priv;
	int mac_id, buf_size, curdesc;
	vu32 *reg;
	u32 mac_base, reg_state;	// reg value
	u8 *buf;

	struct net_device *dev = (struct net_device *)dev_id;
	struct sh7710_txrxdesc *tmpdescptr; // temp desc-struct ptr

	priv = (struct sh7710_priv *)dev->priv;

	debug_printk("sh7710_interrupt\n");
	
	/* set interrupt */
	spin_lock(&priv->lock);

	/* whitch MAC ? */
	if(dev - sh7710_devs){ // MAC 1
		mac_base = EDMAC1_BASE;
		mac_id = 1;
		tmpdescptr = txrxdesc_bound + 1;
	}else{ // MAC 0
		mac_base = EDMAC0_BASE;
		mac_id = 0;
		tmpdescptr = txrxdesc_bound;
	}

	curdesc = rx_desc_idx[mac_id];

	reg = (vu32 *)(mac_base + EESR); /* get rx status */
	reg_state = *reg;

	if(reg_state & EESR_RX_DESC_NONE){ // rx desc none (retry)
		*reg = EESR_RX_DESC_NONE;
		debug_printk("%s :rx desc none\n", dev->name);
		reg_state &= ~EESR_RX_DESC_NONE;
		sh7710_retry_rx(dev); /* retry rx */
	}
	if(!(reg_state & EESR_RX_SUSPEND)){ /* no suspend receive */
		if(reg_state & (EESR_RX_DONE + EESR_RX_MULTI)){
			while(!(tmpdescptr->macrxd[curdesc].td0 & RX_DESC_VALID)){
				// get data size
				buf_size = 0;
				buf_size = tmpdescptr->macrxd[curdesc].td1 & RX_DESC_SIZE_MASK;
	
				if(buf_size){
					buf = (u8 *)P2SEGADDR(tmpdescptr->macrxd[curdesc].td2);
					sh7710_rx(dev, buf_size, buf);
				}
	
				// fix current desc setting
				// current desc invalid -> valid
				tmpdescptr->macrxd[curdesc].td0 &= RX_DESC_FRAME_INIT;
				tmpdescptr->macrxd[curdesc].td0 |= RX_DESC_VALID;
	
				// clear data size
				tmpdescptr->macrxd[curdesc].td1 &= RX_DESC_SIZE_INIT;
	
				// fix current index
				rx_desc_idx[mac_id] = (curdesc + 1) % DESC_NUM;
				curdesc = rx_desc_idx[mac_id]; // modify current index
			}
			/* clear interrupt flag */
			if(reg_state & EESR_RX_DONE){
				*reg = EESR_RX_DONE;
				reg_state &= ~EESR_RX_DONE;
			}

			if(reg_state & EESR_RX_MULTI){
				*reg = EESR_RX_MULTI;
				priv->stats.multicast++;
				reg_state &= ~EESR_RX_MULTI;
			}
		}
	}else{
		*reg = EESR_RX_SUSPEND;
		reg_state &= ~EESR_RX_SUSPEND;
	}

	/* check tx done */
	if(reg_state & EESR_TX_WB){
		priv->stats.tx_packets++;
		*reg = EESR_TX_WB;
		reg_state &= ~EESR_TX_WB;
	}
	if(reg_state & EESR_TX_DONE){
		priv->stats.tx_packets++;
		*reg = EESR_TX_DONE;
		reg_state &= ~EESR_TX_DONE;
	}
	if(reg_state & EESR_RX_FIFO_OFLW){ // rx over flow(over run)
		priv->stats.rx_over_errors++;
		*reg = EESR_RX_FIFO_OFLW;
		debug_printk("%s :rx over run\n",dev->name);
		reg_state &= ~EESR_RX_FIFO_OFLW;
	}
	if(reg_state & EESR_TX_FIFO_UFLW){ // tx under flow(over run)
		priv->stats.tx_fifo_errors++;
		*reg = EESR_TX_FIFO_UFLW;
		debug_printk("%s :tx over run\n",dev->name);
		reg_state &= ~EESR_TX_FIFO_UFLW;
	}
	if(reg_state & EESR_RX_CNT_OFLW){ // rx counter overflow 
		*reg = EESR_RX_CNT_OFLW;
		debug_printk("%s :rx count  over flow \n",dev->name);
		reg_state &= ~EESR_RX_CNT_OFLW;
	}
	if(reg_state){ // other INTR
		debug_printk("%s :irq : %08x\n",dev->name, reg_state);
		*reg = reg_state;
		reg_state &= ~reg_state;

#ifdef DEBUG
//		ether_reg_dump2();
#endif
	}

	spin_unlock(&priv->lock);
	
	return;
}

/* retry rx proc */
void sh7710_retry_rx(struct net_device *dev)
{
	u32	mac_base;
	vu32	*reg;

	sh7710_interrupt(0, dev, NULL);

	/* whitch MAC ? */
	if(dev - sh7710_devs){ // MAC 1
		mac_base = EDMAC1_BASE;
	}else{ // MAC 0
		mac_base = EDMAC0_BASE;
	}
	reg = (vu32 *)(mac_base + EDRRR); /* rx request to E-DMAC */
	*reg = EDRRR_RX_REQ;
}

/* receive packet (buf -> dev) */
void sh7710_rx(struct net_device *dev, int len, unsigned char *buf)
{
	struct sh7710_priv *priv = (struct sh7710_priv *)dev->priv;
	struct sk_buff *skb;

	debug_printk("sh7710_rx len:%d\n");
	
	
	/* alloc sk_buff area */
	skb = dev_alloc_skb(len + 2 + 3); // add boundary buff 2byte + 3byte
	if(skb == NULL){
		printk("sh7710-eth: not enogh memory - dorrped packet\n");
		priv->stats.rx_dropped++;
		return ;
	}

	skb_reserve(skb, (unsigned int)2); // align 16bit boundary 2byte add between hdr and data

	/* copy from buf to skb area */
	memcpy_sw((vu32 *)skb_put(skb, len), (vu32 *)buf, (len + 3) / 4 * 4); // org


	skb->dev = dev;
	skb->protocol = eth_type_trans(skb, dev);
	skb->ip_summed = CHECKSUM_UNNECESSARY; // no check 
	priv->stats.rx_packets++;
	priv->stats.rx_bytes += len;

	netif_rx(skb);

	return;
}

/* transmit packet */
int sh7710_tx(struct sk_buff *skb, struct net_device *dev)
{
	int len;
	char *data;
	struct sh7710_priv *priv = (struct sh7710_priv *)dev->priv;

	debug_printk("sh7710_tx\n");
	
	/* transmit busy */
	len = skb->len;
	data = skb->data;
	dev->trans_start = jiffies;

	/* H/W process (low level intarface) */
	sh7710_hw_tx(data, len, dev);
	priv->stats.tx_bytes += len;

	/* free skb buffer */
	dev_kfree_skb(skb);

	return 0;
}

/* H/W process (low level intarface) */
void sh7710_hw_tx(char *data, int len, struct net_device *dev)
{

	int mac_id, curdesc;
	vu32 mac_base;
	vu32 *reg;

	struct sh7710_txrxdesc *tmpdescptr; // temp desc-struct ptr
	unsigned char *tmpbuffptr; // temp buff of desc ptr

	debug_printk("sh7710_hw_tx len:%d\n", len);
	

	if(len < (sizeof(struct ethhdr) + sizeof(struct iphdr))){
		printk("sh7710-eth: Packet too short (%i octets)\n", len);
		return;
	}

	if(len > DESC_MAX_BUF_SIZE){
		printk("sh7710-eth: data too large (%i octets)\n", len);
		return;
	}

	/* which MAC ? */
	/* set reg base addr and desc-struct top ptr */
	if( dev - sh7710_devs ){ // MAC 1
		mac_id = 1;
		mac_base = EDMAC1_BASE;
		tmpdescptr = txrxdesc_bound + 1;
	}else{ // MAC 0
		mac_id = 0;
		mac_base = EDMAC0_BASE;
		tmpdescptr = txrxdesc_bound;
	}
	
	curdesc = tx_desc_idx[mac_id]; // current desc list number
	
	//
	/* wait to empty desc */
	/* last desc translate finish ? */
	while(tmpdescptr->mactxd[curdesc].td0 & TX_DESC_VALID);


	/* set discpriter control bit */
	/* top of desc-list : desc is valid + top of frame */
	tmpdescptr->mactxd[curdesc].td0 |= TX_DESC_VALID | TX_DESC_FRAME_ALL;
	tmpdescptr->mactxd[curdesc].td1 = len << 16;	// set data size
	tmpbuffptr = (unsigned char *)P2SEGADDR(tmpdescptr->mactxd[curdesc].td2); // descripter buff addr

	/* copy transrate data to desc-buff */
	memcpy_sw((vu32 *)tmpbuffptr, (vu32 *)data, (len + 3) / 4 * 4); // org

	/* transmit request to E-DMAC */
	reg = (vu32 *)(mac_base + EDTRR) ;
	*reg = EDTRR_TX_REQ;


	// fix current desc index (move to interrupt proc)
	tx_desc_idx[mac_id] = (tx_desc_idx[mac_id] + 1) % DESC_NUM;

#ifdef DEBUG
//	status_print();
//	PHY_print();
//	ether_reg_dump2();
//	desc_dump_all();
//	debug_printk("idx:%d\n", curdesc);
//	debug_printk("sh7710_hw_tx:end\n");
#endif

}

/* memory copy and byte swap to network byte order */
static void memcpy_sw(vu32 *to, vu32 *from, int size)
{
	int	i, last;
	vu32	*wkto, *wkfrom;
	vu8	*wkto8, *wkfrom8;

	wkto   = (vu32 *)to ;
	wkfrom = (vu32 *)from ;
	
	last = size / 4;

	for(i = 0; i < last ; i++){
			*wkto++ = htonl(*wkfrom++);
	}
	if( size % 4){
		wkto8   = (vu8 *)wkto;	
		wkfrom8 = (vu8 *)wkfrom;	

		switch(size % 4){
			case 3:
				*wkto8++ ;
				*wkto8++ = *wkfrom8 + 2;
				*wkto8++ = *wkfrom8 + 1;
				*wkto8   = *wkfrom8 + 0;
				break;
			case 2:
				*wkto8 += 2;
				*wkto8++ = *wkfrom8 + 1;
				*wkto8   = *wkfrom8 + 0;
				break;
			case 1:
				*wkto8 += 3;
				*wkto8   = *wkfrom8 + 0;
				break;
			default:
				break;
		}
	}

#ifdef DEBUG	/* print data */
#if 0
	{
	vu8	*wk;

	wk = (vu8 *)from;
	debug_printk("data from:\n");
	for(i = 0; i < (size + 3) / 4 * 4; i++){
		debug_printk("%02x%s",*wk, (i % 16) == 15 ? "\n" : " ");
		wk++;
	}
	debug_printk("\n\n");

	wk = (vu8 *)to;
	debug_printk("data to:\n");
	for(i = 0; i < (size + 3) / 4 * 4; i++){
		debug_printk("%02x%s",*wk, (i % 16) == 15 ? "\n" : " ");
		wk++;
	}
	debug_printk("\n\n");
	}
#endif
#endif // DEBUG
}
/* alloc desc area */
int alloc_desc_area(void)
{
	
	u32 addrwk;
	int i,j;
	vu32 buf;
	struct sh7710_txrxdesc *desc;

	/* allocate description area for MAC0(tx/rx) MAC1(tx/rx) */
	/* Note ::: max 128Kbyte : kmalloc limit */
	/* adress is P2 segment */
	txrxdesc_org = P2SEGADDR(
			kmalloc(sizeof(struct sh7710_txrxdesc) * MAC_NUM + DESC_MEM_BOUNDARY - 1, 
							GFP_ATOMIC | GFP_DMA));

	if (txrxdesc_org == NULL)
		return -ENODEV;

	// clear 0
	memset(txrxdesc_org, 0, 
			sizeof(struct sh7710_txrxdesc) * MAC_NUM + DESC_MEM_BOUNDARY - 1);

	/* adjust address 16byte boundary */
	addrwk = (u32)txrxdesc_org + DESC_MEM_BOUNDARY - 1;
	addrwk &= 0xfffffff0;
	txrxdesc_bound = (struct sh7710_txrxdesc *)addrwk;
	
	/* alloc data buff of description */
	/* buff size * desc num * 2(tx,rx) * MAC num(0,1) + boundary ajust */
	/* Note ::: max 128Kbyte : kmalloc limit */
	/* adress is P2 segment */
	desc_buf_org = P2SEGADDR(kmalloc(
					DESC_BUF_SIZE * DESC_NUM * 2 * MAC_NUM + DESC_MEM_BOUNDARY - 1, 
					GFP_ATOMIC | GFP_DMA));

	if (desc_buf_org == NULL){
		kfree(txrxdesc_org);
		return -ENODEV;
	}


	// clear 0
	memset(desc_buf_org, 0, 
		DESC_BUF_SIZE * DESC_NUM * 2 * MAC_NUM + DESC_MEM_BOUNDARY - 1);

	/* adjust address to 16byte boundary */
	addrwk = (u32)desc_buf_org + DESC_MEM_BOUNDARY - 1;
	addrwk &= 0xfffffff0;
	desc_buf_bound = (unsigned char *)addrwk;
	
	/* set buff address to desc list */
	desc = txrxdesc_bound;
	buf = (vu32)desc_buf_bound;
	for (i = 0 ; i < MAC_NUM; i++){
		for (j = 0; j < DESC_NUM; j++){
			desc->mactxd[j].td2 = 
					(vu32)virt_to_phys((void *)buf); // convert to physical addr

			buf += DESC_BUF_SIZE;
		}
		for (j = 0; j < DESC_NUM; j++){
			desc->macrxd[j].td2 = 
					(vu32)virt_to_phys((void *)buf); // convert to physical addr

			buf += DESC_BUF_SIZE;
		}
		desc++;

		// index initialize
		rx_desc_idx[i] = 0;
		tx_desc_idx[i] = 0;
	}
	return 0;
}

/* set multicast list */
static void  sh7710_set_multicast_list(struct net_device *dev)
{

	struct sh7710_priv *priv = (struct sh7710_priv *)dev->priv;
	unsigned long flags;
	int	mac_id;
	u32 	mac_base,ret;
	vu32	*reg, val;

	debug_printk("sh7710_set_multicast_list\n");

	spin_lock_irqsave(&priv->lock, flags);

	if(dev - sh7710_devs){ // MAC1
		mac_id = 1;
		mac_base = ETHER_C_1_BASE;
	}else{ // MAC0
		mac_id = 0;
		mac_base = ETHER_C_0_BASE;
	}

	/* read ECMR */
	reg = (vu32 *)(mac_base + ECMR);
	val = *reg;

	if(dev->flags & IFF_PROMISC){ // request promiscuous mode
		if(!(val & ECMR_PROMISCAS)){
			/* set promiscuous mode to ECMR */
			val |= ECMR_PROMISCAS;
		}

		/* print message */
		printk("%s: request to set promiscuous list\n", dev->name);
	}else{ // request promiscuous mode off
		if(val & ECMR_PROMISCAS){
			/* un-set promiscuous mode to ECMR */
			val &= ~ECMR_PROMISCAS;
		}
		/* print message */
		printk("%s: request to clear promiscuous list\n", dev->name);
	}
	if(val != *reg){ // differ setting 
		/* stop EDMA-C Ether-C (EDMAC0/1 software reset) */
		EDMAC_soft_reset(mac_id);

		ret = sh7710_init_controller(dev);
	}
	spin_unlock_irqrestore(&priv->lock, flags);
	
}

/* module initialize */
int sh7710_init(struct net_device *dev)
{

#ifdef DEBUG
	debug_printk("sh7710_init\n");
//	PHY_print();
//	status_print();
//	ether_reg_dump2();
#endif

	ether_setup(dev);	/* initialize dev struct (net_init.c) */

	/* setting dev function field */
	dev->open = 			sh7710_open;	/* ifconfig up */
	dev->stop = 			sh7710_release;	/* ifconfig down */
	dev->set_config = 		sh7710_config;
	dev->hard_start_xmit = 	sh7710_tx;		/* transmit */
	dev->do_ioctl = 		sh7710_ioctl;
	dev->get_stats = 		sh7710_stats;
	dev->change_mtu = 		sh7710_change_mtu;
	dev->set_multicast_list = 		sh7710_set_multicast_list;
	dev->base_addr = 		0xffe0;
#ifdef HAVE_TX_TIMEOUT
	dev->tx_timeout =		sh7710_tx_timeout;
//	dev->watchdog_timeo = 	timeout;
#endif

	SET_MODULE_OWNER(dev);

	/* private area allocate */
	dev->priv = kmalloc(sizeof(struct sh7710_priv), GFP_KERNEL);
	if (dev->priv == NULL)
		return -ENODEV;

	/* clear dev->priv */
	memset(dev->priv, 0, sizeof(struct sh7710_priv));

	spin_lock_init(&((struct sh7710_priv *)dev->priv)->lock);
	
	return 0;
}

/* initialize sh7710 E-DMAC */
int init_sh7710_EDMAC(u32 mac)
{
	unsigned int	mac_base;
	vu32			*reg;

	if (mac){
		mac_base = EDMAC1_BASE;
	}else{
		mac_base = EDMAC0_BASE;
	}

	debug_printk("init_sh7710_EDMAC:base %x :id %d\n", mac_base, mac_id);
	

	/* set E-DMAC mode */
	/*
	 * 31-6:reserve
	 *  5,4:00 desc size 16byte
	 *  3-1:reserve
	 *  0:0 no software reset
	 */
	reg = (vu32 *)(mac_base + EDMR);
	*reg = EDMR_DESC16; // 16byte

	/* trnsmit FIFO threshhold size */
	reg = (vu32 *)(mac_base + TFTR);
	*reg = TFTR_SIZE; 

	/* FIFO volume size */
	/*
	 * 31-11:reserve
	 * 10- 8:111 tramsmit FIFO 2048 byte
	 *  7- 3:reserve
	 *  2- 0:111 receive FIFO 2048 byte
	 */
	reg = (vu32 *)(mac_base + FDR);
	*reg = FDR_TX_VOL | FDR_RX_VOL;

	/* rx control
	 *
	 * 31- 1:reserve
	 *     0:1 continue
	 *     0:0 stop/frame
	 *
	 */
	reg = (vu32 *)(mac_base + RMCR);
	*reg = RMCR_CONT; // continue

	/* Ether-C/E-DMAC state interrupt setting
	 *
	 * about rx : all
	 *            0x4387009f
	 *       tx : tx stop & tx done
	 *            0x44a00000
	 */
	reg = (vu32 *)(mac_base + EESIPR);
	*reg = EESIPR_INIT; // all intr avail (0x47ff0f9f)

	/* transmit INT setting reg */
	reg = (vu32 *)(mac_base + TRIMD);
	*reg = TRIMD_TX_NOWB; // trnsmit done "NO" write-back to TWB-bit of EESR reg

	/* FCFTR rx FIFO overflow thresh hold size reg */
	reg = (vu32 *)(mac_base + FCFTR);
	*reg = FCFTR_FR_NUM_1 | FCFTR_SIZE_2048; // frame = 8 or size 1792 - 32 byte

	return 0;
}

/* initialize sh7710-eth Controler */
int init_sh7710_EtherC(struct net_device *dev)
{
	u32 	mac_base;
	vu32	*reg;	// reg work
	int		mac, i, len ;
	u32 	addr,val;
	u8		*c;

	if(dev - sh7710_devs){ // /dev/eth1?
		mac_base = ETHER_C_1_BASE;
		mac = 1;
	}else{
		mac_base = ETHER_C_0_BASE;
		mac = 0;
	}

	debug_printk("init_sh7710_EtherC:base %x :id %d\n", mac_base, mac_id);
	
	/* set mode */
	/*
	 * 13:0 not CAM entry table list frame
	 * 12:0 CRC err = err frame
	 * 11,10: reserve
	 *  9:0 not Magic Packet
	 *  8,7: reserve
	 *  6:1 rx leave
	 *  5:1 tx leave
	 *  4: reserve
	 *  3:0 not loopback inside
	 *  2:0 loopback outside lo
	 *  1:1 FULL-duplex mode 
	 *  0:0 normal mode
	 */
	reg = (vu32 *)(mac_base + ECMR);
	val = ECMR_INIT; // default (0x00000062)  mode

	if(dev->flags & IFF_PROMISC){ // request promiscuous mode
		val |= ECMR_PROMISCAS; //add promiscuous flag
	}
	*reg = val; 

	/* leave interrupt reg */
	/* default(reset) setting */
	/* 31-3:reserve
	 *  2:0 not leave LINK interrrupt
	 *  1:0 not leave Magic Packet intr
	 *  0:0 not leave wrong carryer
	 */
	reg = (vu32 *)(mac_base + ECSIPR);
	*reg = ECSIPR_INIT; // default is 0x0

	/* get MAC(H/W) address from EEPROM or kernel param */
	len = get_sh7710_mac_address(mac, (unsigned char*)dev->dev_addr);

	/* set MAC(H/W) address to Ether-C and dev-struct */
	c = dev->dev_addr;
	addr = 0;
	for(i = 0; i < 4 ;i++){
		addr = addr << 8;
		addr |= *c;
		c++;
	}
	reg = (vu32 *)(mac_base + MAHR); // set mac address upper 32bit
	*reg = addr;

	addr = 0;
	for(i = 0; i < 2 ;i++){
		addr = addr << 8;
		addr |= *c;
		c++;
	}
	reg = (vu32 *)(mac_base + MALR); // set mac address lower 16bit
	*reg = addr;

	/* set receive frame size */
	reg = (vu32 *)(mac_base + RFLR);
	*reg = RFLR_LEN2048;	/* 2048 byte */

	/* forward select control (TSU) */
	reg = (vu32 *)(ETH_TSU_BASE + FWSLC);
	*reg = FWSLC_INIT;	// 0x00000041 (reset val is 0x00000082)

	return 0;
}

/* get macaddress from EEPROM(target) or kernel param(Solution Engine) */
static int  get_sh7710_mac_address(int id, unsigned char *addr)
{

#if defined(CONFIG_SH_MS7710SE)
	/* 
	 * if use this process then valid function parse_ifa_cmdline() in 
	 * arch/sh/kernel/setup.c and add kernel param "ifa=xx:xx:xx:xx:xx:xx"
	 * mac2 H/W addr is make this function
	 * get address from cmdline_ifa[]
	 * cmdline_ifa[] = [mac1] 6byte 
	 *
	 */
	int i;
	unsigned char *c;

	for(i = 0; i < ETH_ALEN; i++){ // copy H/W addr from kernel param 
		addr[i] = cmdline_ifa[i];
		c++;
	}
	if( id == 1){ // if MAC1 addr
		addr[ETH_ALEN - 1] += 1; // MAC0 addr + 1
	}

#else // target bord
	/* get address from EEPROM ? */
#endif // CONFIG_SH_SOLUTION_ENGINE
	return ETH_ALEN;
}

/* PHY chip initialize */
/* 
 * for SE     : Davicom DM9161E
 * for target : Intel LXT
 */

static void sh7710_PHY_init(u32 mac)
{
	vu32	*reg;
	u16	mii_reg, mii_data;

	if(mac){ // MAC 1
		reg = (vu32 *)(ETHER_C_1_BASE + PIR);
	}else{ // MAC 0
		reg = (vu32 *)(ETHER_C_0_BASE + PIR);
	}

	/* PHY chip reset */
	mii_reg = MII_BMCR_WRITE;
	mii_data =	MII_BMCR_RESET;
	PHY_write(reg, mii_reg, mii_data);	// reset

#if defined (CONFIG_SH_MS7710SE)
	/* Interrupt reg setting */
	mii_reg = MII_INTR_WRITE;
	mii_data = MII_INTR_DEFAULT; // FDX/SPD/LINK intrrupt Mask
	PHY_write(reg, mii_reg, mii_data);	// auto-nego mode set
#endif // CONFIG_SH_MS7710SE

	/* PHY chip mode set (auto negotiation) */
	mii_reg = MII_ANAR_WRITE;
	mii_data =	MII_ANAR_AUTO | MII_ANAR_IEEE;	
	PHY_write(reg, mii_reg, mii_data);	// auto-nego mode set

	/* auto negotioation restart */
	mii_reg = MII_BMCR_WRITE;
	mii_data = MII_BMCR_AUTO_NEG_E | MII_BMCR_AUTO_NEG_RESTART; // restart chip
	PHY_write(reg, mii_reg, mii_data);	// auto-nego mode set
}



/* PHY chip reg write */
static void PHY_write(vu32 *reg, u16 phyreg, u16 data)
{
	int i;
	
	pre_operation(reg);

	for(i = 0; i < 16; i++){ /* write phy-reg data */
		if(phyreg & MII_BIT_CHECK){
			write_bit1(reg);	// write bit 1
		}else{
			write_bit0(reg);	// write bit 0
		}
		phyreg <<= 1;	// shift left 1 bit
	}

	for(i = 0; i < 16; i++){ /* write phy-control data */
		if(data & MII_BIT_CHECK){
			write_bit1(reg);	// write bit 1
		}else{
			write_bit0(reg);	// write bit 0
		}
		data <<= 1;	// shift left 1 bit
	}

	after_idle(reg);
}

/* after operation code */
static void after_idle(vu32 *reg)
{
	*reg = AFTER_IDLE_CODE; // after idle code
}

/* pre-operation code write */
static void pre_operation(vu32 *reg)
{
	int i;

	for(i = 0; i < 32 ; i++){
		write_bit1(reg); // send PHY 32 * 1 bit data
	}	
}

/* write MII data '0' */
static void write_bit0(vu32 *reg)
{
	u16	wdata;

	wdata = 0; // clear

	*reg = MII_MANAGE_W | MII_WRITE_0; // MII write 0
	udelay(MII_DELAY);	// deray 
	*reg = MII_MANAGE_W | MII_WRITE_0 | MII_MANAGE_CL; // MII write 0 + manage clock
	udelay(MII_DELAY);	// deray 
	*reg = MII_MANAGE_W | MII_WRITE_0; // MII write 0
	
}
/* write MII data '1' */
static void write_bit1(vu32 *reg)
{
	u16	wdata;

	wdata = 0; // clear

	*reg = MII_MANAGE_W | MII_WRITE_1; // MII write 1
	udelay(MII_DELAY);	// deray 
	*reg = MII_MANAGE_W | MII_WRITE_1 | MII_MANAGE_CL; // MII write 1
	udelay(MII_DELAY);	// deray 
	*reg = MII_MANAGE_W | MII_WRITE_1; // MII write 1
	
}


/* load module */
int sh7710_init_module(void)
{

	int i, ret, dev_cnt;
	dev_cnt = 0; /* init regist dev count */

	debug_printk("sh7710_init_module\n");
	
	/* set devide name for probe */
	strcpy(sh7710_devs[0].name, "eth%d");
	strcpy(sh7710_devs[1].name, "eth%d");

	/* allocate descripter (CH0,1 tx,rx)*/
	if(alloc_desc_area()){
		return -ENODEV;
	}

	EDMAC_soft_reset(0); 	/* E-DMAC0 soft-reset */
	EDMAC_soft_reset(1); 	/* E-DMAC0 soft-reset */
	etherC_soft_reset();	/* Ether-C soft-reset */

#ifdef DEBUG
//	PHY_print();
#endif

	printk(KERN_INFO "SH7710 Ethernet Driver 1.0\n");

	/* regist device */
	for(i = 0; i < 2 ; i++){
		if( (ret = register_netdev(sh7710_devs + i)) ){
			printk("sh7710-eth: err %i device name %s\n",
				ret, sh7710_devs[i].name);
		}else{
			dev_cnt++;
		}

	}

	/* print MAC info to console */
	for (i = 0; i < 2; i++){
		int j,len;
		unsigned char addr[32];

		printk(KERN_INFO "%s: %s at 0x%lx, ",
				sh7710_devs[i].name, "MAC in sh7710",
				(unsigned long)((i == 0) ? ETHER_C_0_BASE : ETHER_C_1_BASE));
		len = get_sh7710_mac_address(i, addr);
		for( j = 0; j < len - 1;j++){
				printk("%2.2x:", addr[j]);
		}
		printk("%2.2x, IRQ %d.\n", addr[j],
				(i == 0) ? EDMAC_EINT0_IRQ : EDMAC_EINT1_IRQ);
	}

	return dev_cnt ? 0 : -ENODEV;
}

/* initialize Ether-C & E-DMAC on module load */
static void EDMAC_soft_reset(int ch)
{
	vu32 *reg; // reg work

	if(ch){	// MAC1
		reg = (vu32 *)(EDMAC1_BASE + EDMR);
	}else{ // MAC0
		reg = (vu32 *)(EDMAC0_BASE + EDMR);
	}
	/* software reset E-DMAC */
	*reg = EDMR_SW_RST;
	mdelay(1); // delay 1 ms

}
/* initialize Ether-C & E-DMAC on module load */
static void etherC_soft_reset(void)
{
	vu32 *reg; // reg work

	/* software reset Ether-C */
	reg = (vu32 *)ETHER_C_ARSTR;
	*reg = ARSTR_SW_RST;
	mdelay(1); // delay 1 ms
}

/* unload module */
void sh7710_cleanup(void)
{
	int i,j;
	struct sh7710_txrxdesc *desc;
	
	debug_printk("sh7710_cleanup\n");
	
	/* free desc area */
	desc = txrxdesc_bound;

	for(i = 0; i < MAC_NUM; i++){ // loop max num
		for(j = 0; j < DESC_NUM; j++){ // loop descripter num
			if(desc->mactxd[j].td2 != NULL){
				kfree(phys_to_virt(desc->mactxd[j].td2));
				desc->mactxd[j].td2 = NULL;
			}
			if(desc->macrxd[j].td2 != NULL){
				kfree(phys_to_virt(desc->macrxd[j].td2));
				desc->macrxd[j].td2 = NULL;
			}
		}
		kfree(sh7710_devs[i].priv);
		unregister_netdev(sh7710_devs + i);
		desc++;
	}
	kfree(P1SEGADDR(txrxdesc_org)); // desc area
#if 0
	for (i = 0; i < 2; i++){
		kfree(sh7710_devs[i].priv);
		unregister_netdev(sh7710_devs + i);
	}
#endif
	
	return;
}

#ifdef DEBUG
/* reg dump (debug) */
static void ether_reg_dump(void)
{
	int i;
	vu32 *reg;
	debug_printk("addr    :data\n");
	for(i = 0xa7000000; i < 0xa7001000 ; i += 4){
		reg = (vu32 *)i;
		debug_printk("%08x:%08x\n",(u32)reg, *reg);
	}
}

/* reg dump (debug) */
static void ether_reg_dump2(void)
{
	int i,n_cnt;
	vu32 *reg;

	char	ethC_name[][7]={
			"EMCR  ", "ECSR  ", "ECSIPR", "PIR   ",
			"MAHR  ", "MALR  ", "RFLR  ", "PSR   ",
			"TROCR ", "CDCR  ", "LCCR  ", "CNDCR ",
			"      ", "CEFCR ", "FRECR ", "TSFRCR",
			"TLFRCR", "RFCR  ", "MAFCR ", "      ",
			"      ", "IPGR  "
	};
	char	DMAC_name[][7]={
			"EDMR  ", "EDTRR ", "EDRRR ", "TDLAR ",
			"RDLAR ", "EESR  ", "EESIPR", "TRSCER",
			"RMFCR ", "TFTR  ", "FDR   ", "RMCR  ",
			"EDOCR ", "FCFTR ", "RDADIR", "TRIMD ",
			"RBWAR ", "RDFAR ", "      ", "TBRAR ",
			"TDFAR "
	};

	debug_printk("\n");
	debug_printk("ETH-C ARSTR reg\n");
	reg = (vu32 *)0xa7000800;
	debug_printk("%08x:%08x\n",(u32)reg, *reg);

	debug_printk("ETH-C 0 reg          ETH-C 1 reg\n");
	n_cnt = 0;
	for(i = 0xa7000160; i < 0xa70001b8; i += 4){
		debug_printk("%s ",ethC_name[n_cnt]);
		reg = (vu32 *)i;
		debug_printk("%08x:%08x    ",(u32)reg, *reg);

		reg = (vu32 *)(i + 0x400);
		debug_printk("%08x:%08x\n",(u32)reg, *reg);
		n_cnt++;
	}
	debug_printk("\n");

	n_cnt = 0;
	debug_printk("EDMA-C 0 reg         EDMA-C 1 reg\n");
	for(i = 0xa7000000; i < 0xa7000054; i += 4){
		debug_printk("%s ",DMAC_name[n_cnt]);
		reg = (vu32 *)i;
		debug_printk("%08x:%08x    ",(u32)reg, *reg);

		reg = (vu32 *)(i + 0x400);
		debug_printk("%08x:%08x\n",(u32)reg, *reg);
		n_cnt++;
	}
	debug_printk("\n");

#if 1
	debug_printk("ETH-C (TSU) reg\n");
	for(i = 0xa7000804; i < 0xa7001000; i += 4){
		reg = (vu32 *)i;
		debug_printk("%08x:%08x  ",(u32)reg, *reg);
		debug_printk("%s",(i & 0xf ? "" : "\n"));
	}
	debug_printk("\n");
#endif

}

/* desc dump (debug) */
static void desc_dump_all(void)
{
	struct sh7710_txrxdesc *desc;
	struct sh7710_txrxdesc *desc1;
	int i,j;
	desc = txrxdesc_bound;
	desc1 = txrxdesc_bound + 1;
	debug_printk("num: eth : td0     : td2     : td3    : pad\n");
	for(i = 0; i < 1; i++){
		debug_printk("***** tx desc *****\n");
		debug_printk("desc addr: %08x     %08x\n", (u32)desc, (u32)desc1);
		debug_printk("tx list top  addr: %08x                      %08x\n", 
							(u32)(desc->mactxd), (u32)(desc1->mactxd));
		for(j = 0; j < DESC_NUM; j++){
			debug_printk("%02d:%d  :%08x:%08x:%08x:%08x   | ",j, i, 
							desc->mactxd[j].td0,
							desc->mactxd[j].td1,
							desc->mactxd[j].td2,
							desc->mactxd[j].pad
							);
			debug_printk("%02d:%d  :%08x:%08x:%08x:%08x\n",j, i + 1, 
							desc1->mactxd[j].td0,
							desc1->mactxd[j].td1,
							desc1->mactxd[j].td2,
							desc1->mactxd[j].pad
							);
		}
		debug_printk("\n");

		debug_printk("***** rx desc *****\n");
		debug_printk("rx list top  addr: %08x                       %08x\n", 
							(u32)(desc->macrxd), (u32)(desc1->macrxd));
		for(j = 0; j < DESC_NUM; j++){
			debug_printk("%02d:%d  :%08x:%08x:%08x:%08x   | ",j, i, 
							desc->macrxd[j].td0,
							desc->macrxd[j].td1,
							desc->macrxd[j].td2,
							desc->macrxd[j].pad
							);
			debug_printk("%02d:%d  :%08x:%08x:%08x:%08x\n",j, i + 1, 
							desc1->macrxd[j].td0,
							desc1->macrxd[j].td1,
							desc1->macrxd[j].td2,
							desc1->macrxd[j].pad
							);
		}
		debug_printk("\n");
//		desc++;
	}
}

/* release MII bus */
static void release_mii_bus(vu32 *reg)
{
	*reg = AFTER_IDLE_CODE;
	udelay(MII_DELAY);
	*reg = MII_MANAGE_CL;
	udelay(MII_DELAY);
	*reg = AFTER_IDLE_CODE;
}

/* PHY chip reg read */
static u16 PHY_read(vu32 *reg, u16 phyreg)
{
	int i;
	u32	rdata, rdatal;
	u16 rdatas;

	pre_operation(reg);
	
	for(i = 0; i < 14; i++){ // reg read operation 
		if(phyreg & MII_BIT_CHECK){
			write_bit1(reg);    // write bit 1
		}else{
			write_bit0(reg);    // write bit 0
		}
		phyreg <<= 1;
	}

	release_mii_bus(reg);

	rdata = 0;
	rdatal = 0;
	for(i = 0; i < 16; i++){ // read 16 bit reg data
		*reg = MII_MANAGE_R | MII_MANAGE_CL;
		udelay(MII_DELAY);
		rdata = *reg;
		rdata &= 0x00000008; // mask bit 4
		rdatal |= rdata;
		rdatal <<= 1;
		*reg = MII_MANAGE_R;
	}

	rdatal >>= 4;
	rdatas = (u16)rdatal; // convert u32 to u16
	release_mii_bus(reg);
	return(rdatas);
	
}
#endif

module_init(sh7710_init_module); // when load module
module_exit(sh7710_cleanup); // when unload module
MODULE_LICENSE("GPL");
MODULE_AUTHOR("Lineo Solutions, Inc. http://www.lineo.co.jp/");
MODULE_DESCRIPTION("SH7710 Ethernet driver");
