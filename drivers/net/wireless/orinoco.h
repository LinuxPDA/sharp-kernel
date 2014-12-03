/* orinoco.h
 * 
 * Common definitions to all pieces of the various orinoco
 * drivers
 */

#ifndef _ORINOCO_H
#define _ORINOCO_H

/* To enable debug messages */
//#define ORINOCO_DEBUG		3

#if (! defined (WIRELESS_EXT)) || (WIRELESS_EXT < 10)
#error "orinoco driver requires Wireless extensions v10 or later."
#endif /* (! defined (WIRELESS_EXT)) || (WIRELESS_EXT < 10) */
#define WIRELESS_SPY		// enable iwspy support

#define ORINOCO_MAX_KEY_SIZE	14
#define ORINOCO_MAX_KEYS	4

struct orinoco_key {
	u16 len;	/* always stored as little-endian */
	char data[ORINOCO_MAX_KEY_SIZE];
} __attribute__ ((packed));

struct orinoco_private {
	void *card;	/* Pointer to card dependant structure */
	void (*reset)(struct orinoco_private *);

	/* Synchronisation stuff */
	spinlock_t lock;
	int resetting;
	wait_queue_head_t reset_queue;
	struct tq_struct reset_task;

	/* Net device stuff */
	struct net_device *ndev;
	struct net_device_stats stats;
	struct iw_statistics wstats;

	/* Hardware control variables */
	hermes_t hw;
	u16 txfid;

	/* Capabilities of the hardware/firmware */
	int firmware_type;
#define FIRMWARE_TYPE_AGERE 1
#define FIRMWARE_TYPE_INTERSIL 2
#define FIRMWARE_TYPE_SYMBOL 3
	int has_ibss, has_port3, has_ibss_any, ibss_port;
	int has_wep, has_big_wep;
	int has_mwo;
	int has_pm;
	int has_preamble;
	int has_sensitivity;
	int nicbuf_size;
	int broken_cor_reset;
	u16 channel_mask;

	/* Configuration paramaters */
	u32 iw_mode;
	int prefer_port3;
	u16 wep_on, wep_restrict, tx_key;
	struct orinoco_key keys[ORINOCO_MAX_KEYS];
	int bitratemode;
 	char nick[IW_ESSID_MAX_SIZE+1];
	char desired_essid[IW_ESSID_MAX_SIZE+1];
	u16 frag_thresh, mwo_robust;
	u16 channel;
	u16 ap_density, rts_thresh;
	u16 pm_on, pm_mcast, pm_period, pm_timeout;
	u16 preamble;
#ifdef WIRELESS_SPY
	int			spy_number;
	u_char			spy_address[IW_MAX_SPY][ETH_ALEN];
	struct iw_quality	spy_stat[IW_MAX_SPY];
#endif

	/* Configuration dependent variables */
	int port_type, allow_ibss;
	int promiscuous, mc_count;

	/* /proc based debugging stuff */
	struct proc_dir_entry *dir_dev;
};

#ifdef ORINOCO_DEBUG
extern int orinoco_debug;
#define DEBUG(n, args...) do { if (orinoco_debug>(n)) printk(KERN_DEBUG args); } while(0)
#else
#define DEBUG(n, args...) do { } while (0)
#endif	/* ORINOCO_DEBUG */

#define TRACE_ENTER(devname) DEBUG(2, "%s: -> " __FUNCTION__ "()\n", devname);
#define TRACE_EXIT(devname)  DEBUG(2, "%s: <- " __FUNCTION__ "()\n", devname);

struct net_device *alloc_orinocodev(int sizeof_card, int (*open)(struct net_device *),
				    int (*stop)(struct net_device *),
				    void (*reset)(struct orinoco_private *));
int orinoco_commence_reset(struct orinoco_private *priv);
int __orinoco_startup(struct orinoco_private *priv);
extern void orinoco_shutdown(struct orinoco_private *dev);
extern int orinoco_proc_dev_init(struct net_device *dev);
extern void orinoco_proc_dev_cleanup(struct net_device *dev);
extern void orinoco_interrupt(int irq, void * dev_id, struct pt_regs *regs);

#endif /* _ORINOCO_H */
