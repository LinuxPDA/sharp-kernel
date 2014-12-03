/*
 * spectrum_cs.c
 *
 * Copyright (C) 2002 Pavel Roskin, AT&T Labs <proski@gnu.org>
 * Portions based on orinoco_cs.c, Copyright (C) David Gibson,
 *      Linuxcare Australia <hermes@gibson.dropbear.id.au>
 * Portions based on Spectrum24tDnld.c, Copyright (C)
 *       Symbol Technologies.
 *
 * This is a Linux driver for wireless cards that use Prism 2 chipset
 * and have no firmware in the non-volatile memory.  Such cards require
 * firmware download every time they are powered up.
 *
 * Currently supported cards are CompactFlash wireless cards by Symbol
 * (Spectrum24 High Rate) and Socket Communications (Low Power Wireless
 * LAN CF Card).
 *
 * The firmware downloader is a complete rewrite of the code from
 * Spectrum24/IEEE Software for Linux by Symbol Technologies, licensed
 * under GNU GPL.  The firmware also was taken from that project.
 * The project's homepage is http://sourceforge.net/projects/spectrum24/
 *
 * The bulk of the code was taken from Orinoco driver by David Gibson.
 * It implements PCMCIA interface and wireless extensions.  The homepage
 * of the orinoco driver is http://www.ozlabs.org/people/dgibson/dldwd/
 *
 * This driver compiles to a kernel module called "spectrum_cs".  It also
 * uses modules "hermes" and "orinoco" from the Orinoco driver for
 * low-level operations.  It is possible to compile this driver as part
 * of the kernel.
 *
 *
 * Theory of operation.
 *
 * There are two firmware images.  One is primary firmware, the other
 * is secondary firmware.  Both images have the same structure - they
 * begin with a text header followed by data blocks (see structure
 * dblock) and Plug Data references (see structure pdr).  In the case
 * of primary firmware, we are only interested in data blocks.  Each
 * block is separately written to the adapter memory at the address
 * specified in the header of the block.
 *
 * The main role of the primary firmware is to read the data from EEPROM
 * (non-volatile memory) of the adapter, which contains data specific to
 * this adapter, such as MAC address, MKK call sign, serial number and
 * country code.  This data is called Plug Data Area (PDA).  It consists
 * of Plug Data Items (PDI), each with an ID describing its meaning
 * (see structure pdi).
 *
 * The secondary firmware is what normally runs on the card.  But unlike
 * the primary firmware, it needs to be patched (plugged) by the data
 * from the PDA.  Each Plug Data Reference has an ID and an address in
 * the adapter memory where the data should be written.  The data from
 * each PDI is written to the address in the adapter pointed to by the
 * PDR with the same ID.
 *
 * In order to make it safe to patch the adapter memory, the card is put
 * to the idle state.  This is done using Host Configuration Register
 * (HCR), which is available in the PCMCIA attribute space.  HCR is
 * specific to HFA3842 MAC chip.  Cards using an older HFA3841 chip
 * always have non-volatile firmware and should be supported by
 * orinoco_cs.
 */

#include <linux/config.h>
#ifdef  __IN_PCMCIA_PACKAGE__
#include <pcmcia/k_compat.h>
#endif				/* __IN_PCMCIA_PACKAGE__ */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/sched.h>
#include <linux/ptrace.h>
#include <linux/slab.h>
#include <linux/string.h>
#include <linux/timer.h>
#include <linux/ioport.h>
#include <asm/uaccess.h>
#include <asm/io.h>
#include <asm/system.h>
#include <linux/proc_fs.h>
#include <linux/netdevice.h>
#include <linux/if_arp.h>
#include <linux/etherdevice.h>
#include <linux/wireless.h>

#include <pcmcia/version.h>
#include <pcmcia/cs_types.h>
#include <pcmcia/cs.h>
#include <pcmcia/cistpl.h>
#include <pcmcia/cisreg.h>
#include <pcmcia/ds.h>
#include <pcmcia/bus_ops.h>

#include "hermes.h"
#include "orinoco.h"
#include "spectrum_fw.h"

/***************************************
 *** Firmware downloader starts here ***
 ***************************************/

/* Position of PDA in the adapter memory */
#define EEPROM_ADDR        0x3000
#define EEPROM_LEN         0x200
#define PDA_OFFSET         0x100

#define PDA_ADDR           (EEPROM_ADDR + PDA_OFFSET)
#define PDA_WORDS          ((EEPROM_LEN - PDA_OFFSET) / 2)

/* Constants for the CISREG_CCSR register */
#define HCR_RUN            0x07	/* run firmware after reset */
#define HCR_IDLE           0x0E	/* don't run firmware after reset */
#define HCR_MEM16          0x10	/* memory width bit, should be preserved */

/* Hermes command run by primary firmware */
#define HERMES_CMD_READEE  0x0030	/* read serial EEPROM */

/*
 * AUX port access.  To unlock the AUX port write the access keys to the
 * PARAM0-2 registers, then write HERMES_AUX_ENABLE to the HERMES_CONTROL
 * register.  Then read it and make sure it's HERMES_AUX_ENABLED.
 */
#define HERMES_AUX_ENABLE  0x8000	/* Enable auxiliary port access */
#define HERMES_AUX_DISABLE 0x4000	/* Disable to auxiliary port access */
#define HERMES_AUX_ENABLED 0xC000	/* Auxiliary port is open */

#define HERMES_AUX_PW0     0xFE01
#define HERMES_AUX_PW1     0xDC23
#define HERMES_AUX_PW2     0xBA45

/* End markers */
#define PDI_END            0x00000000	/* End of PDA */
#define BLOCK_END          0xFFFFFFFF	/* Last image block */
#define TEXT_END           0x1A	/* End of text header */

/*
 * The following structures have little-endian fields denoted by
 * the leading underscore.  Don't access them directly - use inline
 * functions defined below.
 */

/*
 * The binary image to be downloaded consists of series of data blocks.
 * Each block has the following structure.
 */
struct dblock {
	u32 _addr;		/* adapter address where to write the block */
	u16 _len;		/* length of the data only, in bytes */
	char data[0];		/* data to be written */
} __attribute__ ((packed));

/*
 * Plug Data References are located in in the image after the last data
 * block.  They refer to areas in the adapter memory where the plug data
 * items with matching ID should be written.
 */
struct pdr {
	u32 _id;		/* record ID */
	u32 _addr;		/* adapter address where to write the data */
	u32 _len;		/* expected length of the data, in bytes */
	char next[0];		/* next PDR starts here */
} __attribute__ ((packed));


/*
 * Plug Data Items are located in the EEPROM read from the adapter by
 * primary firmware.  They refer to the device-specific data that should
 * be plugged into the secondary firmware.
 */
struct pdi {
	u16 _len;		/* length of ID and data, in words */
	u16 _id;		/* record ID */
	char data[0];		/* plug data */
} __attribute__ ((packed));;


/* Functions for access to little-endian data */
static inline u32
dblock_addr(const struct dblock *blk)
{
	return le32_to_cpu(blk->_addr);
}

static inline u32
dblock_len(const struct dblock *blk)
{
	return le16_to_cpu(blk->_len);
}

static inline u32
pdr_id(const struct pdr *pdr)
{
	return le32_to_cpu(pdr->_id);
}

static inline u32
pdr_addr(const struct pdr *pdr)
{
	return le32_to_cpu(pdr->_addr);
}

static inline u32
pdr_len(const struct pdr *pdr)
{
	return le32_to_cpu(pdr->_len);
}

static inline u32
pdi_id(const struct pdi *pdi)
{
	return le16_to_cpu(pdi->_id);
}

/* Return length of the data only, in bytes */
static inline u32
pdi_len(const struct pdi *pdi)
{
	return 2 * (le16_to_cpu(pdi->_len) - 1);
}


/* Set address of the auxiliary port */
static inline void
spectrum_aux_setaddr(hermes_t * hw, u32 addr)
{
	hermes_write_reg(hw, HERMES_AUXPAGE, (u16) (addr >> 7));
	hermes_write_reg(hw, HERMES_AUXOFFSET, (u16) (addr & 0x7F));
}


/* Open access to the auxiliary port */
static int
spectrum_aux_open(hermes_t * hw)
{
	int i;

	/* Already open? */
	if (hermes_read_reg(hw, HERMES_CONTROL) == HERMES_AUX_ENABLED)
		return 0;

	hermes_write_reg(hw, HERMES_PARAM0, HERMES_AUX_PW0);
	hermes_write_reg(hw, HERMES_PARAM1, HERMES_AUX_PW1);
	hermes_write_reg(hw, HERMES_PARAM2, HERMES_AUX_PW2);
	hermes_write_reg(hw, HERMES_CONTROL, HERMES_AUX_ENABLE);

	for (i = 0; i < 20; i++) {
		udelay(10);
		if (hermes_read_reg(hw, HERMES_CONTROL) ==
		    HERMES_AUX_ENABLED)
			return 0;
	}

	return -EBUSY;
}


#define CS_CHECK(fn, args...) \
while ((last_ret=CardServices(last_fn=(fn),args))!=0) goto cs_failed

#define CFG_CHECK(fn, args...) \
if (CardServices(fn, args) != 0) goto next_entry

static void
cs_error(client_handle_t handle, int func, int ret)
{
	error_info_t err = { func, ret };
	CardServices(ReportError, handle, &err);
}

/*
 * Reset the card using configuration registers COR and CCSR.
 * If IDLE is 1, stop the firmware, so that it can be safely rewritten.
 */
static int
spectrum_reset(dev_link_t * link, int idle)
{
	int last_ret, last_fn;
	conf_reg_t reg;
	u_int save_cor;

	/* Doing it if hardware is gone is guaranteed crash */
	if (!(link->state & DEV_CONFIG))
		return -ENODEV;

	/* Save original COR value */
	reg.Function = 0;
	reg.Action = CS_READ;
	reg.Offset = CISREG_COR;
	CS_CHECK(AccessConfigurationRegister, link->handle, &reg);
	save_cor = reg.Value;

	/* Soft-Reset card */
	reg.Action = CS_WRITE;
	reg.Offset = CISREG_COR;
	reg.Value = (save_cor | COR_SOFT_RESET);
	CS_CHECK(AccessConfigurationRegister, link->handle, &reg);
	udelay(1000);

	/* Read CCSR */
	reg.Action = CS_READ;
	reg.Offset = CISREG_CCSR;
	CS_CHECK(AccessConfigurationRegister, link->handle, &reg);

	/*
	 * Start or stop the firmware.  Memory width bit should be
	 * preserved from the value we've just read.
	 */
	reg.Action = CS_WRITE;
	reg.Offset = CISREG_CCSR;
	reg.Value = (idle ? HCR_IDLE : HCR_RUN) | (reg.Value & HCR_MEM16);
	CS_CHECK(AccessConfigurationRegister, link->handle, &reg);
	udelay(1000);

	/* Restore original COR configuration index */
	reg.Action = CS_WRITE;
	reg.Offset = CISREG_COR;
	reg.Value = (save_cor & ~COR_SOFT_RESET);
	CS_CHECK(AccessConfigurationRegister, link->handle, &reg);
	udelay(1000);
	return 0;

      cs_failed:
	cs_error(link->handle, last_fn, last_ret);
	return -ENODEV;
}


/*
 * Scan PDR for the record with the specified RECORD_ID.
 * If it's not found, return NULL.
 */
static struct pdr *
spectrum_find_pdr(struct pdr *first_pdr, u32 record_id)
{
	struct pdr *pdr = first_pdr;

	while (pdr_id(pdr) != PDI_END) {
		/*
		 * PDR area is currently not terminated by PDI_END.
		 * It's followed by CRC records, which have the type
		 * field where PDR has length.  The type can be 0 or 1.
		 */
		if (pdr_len(pdr) < 2)
			return NULL;

		/* If the record ID matches, we are done */
		if (pdr_id(pdr) == record_id)
			return pdr;

		pdr = (struct pdr *) pdr->next;
	}
	return NULL;
}


/* Process one Plug Data Item - find corresponding PDR and plug it */
static int
spectrum_plug_pdi(hermes_t * hw, struct pdr *first_pdr, struct pdi *pdi)
{
	struct pdr *pdr;

	/* Find the PDI corresponding to this PDR */
	pdr = spectrum_find_pdr(first_pdr, pdi_id(pdi));

	/* No match is found, safe to ignore */
	if (!pdr)
		return 0;

	/* Lengths of the data in PDI and PDR must match */
	if (pdi_len(pdi) != pdr_len(pdr))
		return -EINVAL;

	/* do the actual plugging */
	spectrum_aux_setaddr(hw, pdr_addr(pdr));
	hermes_write_words(hw, HERMES_AUXDATA, pdi->data,
			   pdi_len(pdi) / 2);

	return 0;
}


/* Read PDA from the adapter */
static int
spectrum_read_pda(hermes_t * hw, u16 * pda, int pda_len)
{
	int ret;
	int pda_size;

	/* Issue command to read EEPROM */
	ret = hermes_docmd_wait(hw, HERMES_CMD_READEE, 0, NULL);
	if (ret)
		return ret;

	/* Open auxiliary port */
	ret = spectrum_aux_open(hw);
	if (ret)
		return ret;

	/* read PDA from EEPROM */
	spectrum_aux_setaddr(hw, PDA_ADDR);
	hermes_read_words(hw, HERMES_AUXDATA, pda, pda_len / 2);

	/* Check PDA length */
	pda_size = le16_to_cpu(pda[0]);
	if (pda_size > pda_len)
		return -EINVAL;

	return 0;
}


/* Parse PDA and write the records into the adapter */
static int
spectrum_apply_pda(hermes_t * hw, const struct dblock *first_block,
		   u16 * pda)
{
	int ret;
	struct pdi *pdi;
	struct pdr *first_pdr;
	const struct dblock *blk = first_block;

	/* Skip all blocks to locate Plug Data References */
	while (dblock_addr(blk) != BLOCK_END)
		blk = (struct dblock *) &blk->data[dblock_len(blk)];

	first_pdr = (struct pdr *) blk;

	/* Go through every PDI and plug them into the adapter */
	pdi = (struct pdi *) (pda + 2);
	while (pdi_id(pdi) != PDI_END) {
		ret = spectrum_plug_pdi(hw, first_pdr, pdi);
		if (ret)
			return ret;

		/* Increment to the next PDI */
		pdi = (struct pdi *) &pdi->data[pdi_len(pdi)];
	}
	return 0;
}


/* Load firmware blocks into the adapter */
static int
spectrum_load_blocks(hermes_t * hw, const struct dblock *first_block)
{
	const struct dblock *blk;
	u32 blkaddr;
	u32 blklen;

	blk = first_block;
	blkaddr = dblock_addr(blk);
	blklen = dblock_len(blk);

	while (dblock_addr(blk) != BLOCK_END) {
		spectrum_aux_setaddr(hw, blkaddr);
		hermes_write_words(hw, HERMES_AUXDATA, blk->data,
				   blklen / 2);

		blk = (struct dblock *) &blk->data[blklen];
		blkaddr = dblock_addr(blk);
		blklen = dblock_len(blk);
	}
	return 0;
}


/*
 * Process a firmware image - stop the card, load the firmware, reset
 * the card and make sure it responds.  For the secondary firmware take
 * care of the PDA - read it and then write it on top of the firmware.
 */
static int
spectrum_dl_image(hermes_t * hw, dev_link_t * link,
		  const unsigned char *image)
{
	int ret;
	const unsigned char *ptr;
	const struct dblock *first_block;

	/* Plug Data Area (PDA) */
	u16 pda[PDA_WORDS];

	/* Binary block begins after the 0x1A marker */
	ptr = image;
	while (*ptr++ != TEXT_END);
	first_block = (const struct dblock *) ptr;

	/* Read the PDA */
	if (image != primsym) {
		ret = spectrum_read_pda(hw, pda, sizeof(pda));
		if (ret)
			return ret;
	}

	/* Stop the firmware, so that it can be safely rewritten */
	ret = spectrum_reset(link, 1);
	if (ret)
		return ret;

	/* Program the adapter with new firmware */
	ret = spectrum_load_blocks(hw, first_block);
	if (ret)
		return ret;

	/* Write the PDA to the adapter */
	if (image != primsym) {
		ret = spectrum_apply_pda(hw, first_block, pda);
		if (ret)
			return ret;
	}

	/* Run the firmware */
	ret = spectrum_reset(link, 0);
	if (ret)
		return ret;

	/* Reset hermes chip and make sure it responds */
	ret = hermes_init(hw);

	/* hermes_init() should return 0 with the secondary firmware */
	if (image != primsym && ret != 0)
		return -ENODEV;

	/* And this should work with any firmware */
	if (!hermes_present(hw))
		return -ENODEV;

	return 0;
}


/*
 * Download the firmware into the card, this also does a PCMCIA soft
 * reset on the card, to make sure it's in a sane state.
 */
static int
spectrum_dl_firmware(hermes_t * hw, dev_link_t * link)
{
	int ret;

	/* load primary firmware */
	ret = spectrum_dl_image(hw, link, primsym);
	if (ret)
		return ret;

	/* load secondary firmware */
	ret = spectrum_dl_image(hw, link, secsym);

	return ret;
}

/*************************************
 *** Firmware downloader ends here ***
 *************************************/

/*====================================================================*/

static char version[] __initdata = "spectrum_cs.c 0.3.4";

MODULE_AUTHOR("Pavel Roskin <proski@gnu.org>");
MODULE_DESCRIPTION
    ("Driver for Symbol Spectrum24 cards with firmware downloader");
#ifdef MODULE_LICENSE
MODULE_LICENSE("Dual MPL/GPL");
#endif

/* Parameters that can be set with 'insmod' */

/* The old way: bit map of interrupts to choose from */
/* This means pick from 15, 14, 12, 11, 10, 9, 7, 5, 4, and 3 */
static uint irq_mask = 0xdeb8;
/* Newer, simpler way of listing specific interrupts */
static int irq_list[4] = { -1 };

MODULE_PARM(irq_mask, "i");
MODULE_PARM(irq_list, "1-4i");

/* Pcmcia specific structure */
struct orinoco_pccard {
	dev_link_t link;
	dev_node_t node;
};

/*
 * Function prototypes
 */

/* struct net_device methods */
static int spectrum_cs_open(struct net_device *dev);
static int spectrum_cs_stop(struct net_device *dev);

/* PCMCIA gumpf */
static void spectrum_cs_config(dev_link_t * link);
static void spectrum_cs_release(u_long arg);
static int spectrum_cs_event(event_t event, int priority,
			     event_callback_args_t * args);

static dev_link_t *spectrum_cs_attach(void);
static void spectrum_cs_detach(dev_link_t *);

/*
   The dev_info variable is the "key" that is used to match up this
   device driver with appropriate cards, through the card configuration
   database.
*/
static dev_info_t dev_info = "spectrum_cs";

/*
   A linked list of "instances" of the dummy device.  Each actual
   PCMCIA card corresponds to one device instance, and is described
   by one dev_link_t structure (defined in ds.h).

   You may not want to use a linked list for this -- for example, the
   memory card driver uses an array of dev_link_t pointers, where minor
   device numbers are used to derive the corresponding array index.
*/

static dev_link_t *dev_list;	/* = NULL */

/*====================================================================*/

static int
spectrum_cs_open(struct net_device *dev)
{
	struct orinoco_private *priv =
	    (struct orinoco_private *) dev->priv;
	struct orinoco_pccard *card = (struct orinoco_pccard *) priv->card;
	dev_link_t *link = &card->link;
	int err;

	/* hard reset here */
	link->open++;
	netif_device_attach(dev);

	err = __orinoco_startup(priv);
	if (err)
		spectrum_cs_stop(dev);
	else
		netif_start_queue(dev);

	return err;
}

static int
spectrum_cs_stop(struct net_device *dev)
{
	struct orinoco_private *priv =
	    (struct orinoco_private *) dev->priv;
	struct orinoco_pccard *card = (struct orinoco_pccard *) priv->card;
	dev_link_t *link = &card->link;

	netif_stop_queue(dev);

	if (link->state & DEV_PRESENT)
		orinoco_shutdown(priv);

	link->open--;

	if (link->state & DEV_STALE_CONFIG) {
		del_timer(&link->release);
		spectrum_cs_release((u_long) link);
	}

	return 0;
}

static void
spectrum_cs_reset(struct orinoco_private *priv)
{
	struct net_device *dev = priv->ndev;
	struct orinoco_pccard *card = (struct orinoco_pccard *) priv->card;
	dev_link_t *link = &card->link;
	int err;

	err = CardServices(ResetCard, link->handle, NULL);
	if (err) {
		printk(KERN_WARNING
		       "%s: ResetCard returned %d.  Not sure what to do.\n",
		       dev->name, err);
	}

	/* Does Reset card wait for the reset to fully complete? */
	return;
}

/* Remove zombie instances (card removed, detach pending) */
static void
flush_stale_links(void)
{
	dev_link_t *link, *next;

	for (link = dev_list; link; link = next) {
		next = link->next;
		if (link->state & DEV_STALE_LINK)
			spectrum_cs_detach(link);
	}
}

/*======================================================================
  spectrum_cs_attach() creates an "instance" of the driver, allocating
  local data structures for one device.  The device is registered
  with Card Services.
  
  The dev_link structure is initialized, but we don't actually
  configure the card at this point -- we wait until we receive a
  card insertion event.
  ======================================================================*/

static dev_link_t *
spectrum_cs_attach(void)
{
	struct net_device *dev;
	struct orinoco_private *priv;
	struct orinoco_pccard *card;
	dev_link_t *link;
	client_reg_t client_reg;
	int ret, i;

	/* A bit of cleanup */
	flush_stale_links();

	dev =
	    alloc_orinocodev(sizeof(*card), spectrum_cs_open,
			     spectrum_cs_stop, spectrum_cs_reset);
	if (!dev)
		return NULL;
	priv = dev->priv;
	card = priv->card;

	/* Link both structures together */
	link = &card->link;
	link->priv = priv;

	/* Initialize the dev_link_t structure */
	link->release.function = &spectrum_cs_release;
	link->release.data = (u_long) link;

	/* Interrupt setup */
	link->irq.Attributes = IRQ_TYPE_EXCLUSIVE;
	link->irq.IRQInfo1 = IRQ_INFO2_VALID | IRQ_LEVEL_ID;
	if (irq_list[0] == -1)
		link->irq.IRQInfo2 = irq_mask;
	else
		for (i = 0; i < 4; i++)
			link->irq.IRQInfo2 |= 1 << irq_list[i];
	link->irq.Handler = NULL;

	/* General socket configuration defaults can go here.  In this
	 * client, we assume very little, and rely on the CIS for
	 * almost everything.  In most clients, many details (i.e.,
	 * number, sizes, and attributes of IO windows) are fixed by
	 * the nature of the device, and can be hard-wired here. */
	link->conf.Attributes = 0;
	link->conf.IntType = INT_MEMORY_AND_IO;

	/* Register with Card Services */
	link->next = dev_list;
	dev_list = link;
	client_reg.dev_info = &dev_info;
	client_reg.Attributes = INFO_IO_CLIENT | INFO_CARD_SHARE;
	client_reg.EventMask =
	    CS_EVENT_CARD_INSERTION | CS_EVENT_CARD_REMOVAL |
	    CS_EVENT_RESET_PHYSICAL | CS_EVENT_CARD_RESET |
	    CS_EVENT_PM_SUSPEND | CS_EVENT_PM_RESUME;
	client_reg.event_handler = &spectrum_cs_event;
	client_reg.Version = 0x0210;
	client_reg.event_callback_args.client_data = link;
	ret = CardServices(RegisterClient, &link->handle, &client_reg);
	if (ret != CS_SUCCESS) {
		cs_error(link->handle, RegisterClient, ret);
		spectrum_cs_detach(link);
		return NULL;
	}

	return link;
}				/* spectrum_cs_attach */

/*
 * This deletes a driver "instance".  The device is de-registered with
 * Card Services.  If it has been released, all local data structures
 * are freed.  Otherwise, the structures will be freed when the device
 * is released.
 */
static void
spectrum_cs_detach(dev_link_t * link)
{
	dev_link_t **linkp;
	struct orinoco_private *priv = link->priv;
	struct net_device *dev = priv->ndev;

	/* Locate device structure */
	for (linkp = &dev_list; *linkp; linkp = &(*linkp)->next)
		if (*linkp == link)
			break;
	if (*linkp == NULL)
		return;

	/*
	   If the device is currently configured and active, we won't
	   actually delete it yet.  Instead, it is marked so that when
	   the release() function is called, that will trigger a proper
	   detach().
	 */
	if (link->state & DEV_CONFIG) {
		link->state |= DEV_STALE_LINK;
		return;
	}

	/* Break the link with Card Services */
	if (link->handle)
		CardServices(DeregisterClient, link->handle);

	/* Unlink device structure, and free it */
	*linkp = link->next;
	if (link->dev) {
		unregister_netdev(dev);
	}

	kfree(dev);
}				/* spectrum_cs_detach */

/*======================================================================
  spectrum_cs_config() is scheduled to run after a CARD_INSERTION event
  is received, to configure the PCMCIA socket, and to make the
  device available to the system.
  ======================================================================*/

static void
spectrum_cs_config(dev_link_t * link)
{
	client_handle_t handle = link->handle;
	struct orinoco_private *priv = link->priv;
	struct orinoco_pccard *card = (struct orinoco_pccard *) priv->card;
	hermes_t *hw = &priv->hw;
	struct net_device *ndev = priv->ndev;
	tuple_t tuple;
	cisparse_t parse;
	int last_fn, last_ret;
	u_char buf[64];
	config_info_t conf;
	cistpl_cftable_entry_t dflt = { 0 };
	cisinfo_t info;

	TRACE_ENTER("orinoco");

	CS_CHECK(ValidateCIS, handle, &info);

	/*
	   This reads the card's CONFIG tuple to find its configuration
	   registers.
	 */
	tuple.DesiredTuple = CISTPL_CONFIG;
	tuple.Attributes = 0;
	tuple.TupleData = buf;
	tuple.TupleDataMax = sizeof(buf);
	tuple.TupleOffset = 0;
	CS_CHECK(GetFirstTuple, handle, &tuple);
	CS_CHECK(GetTupleData, handle, &tuple);
	CS_CHECK(ParseTuple, handle, &tuple, &parse);
	link->conf.ConfigBase = parse.config.base;
	link->conf.Present = parse.config.rmask[0];

	/* Configure card */
	link->state |= DEV_CONFIG;

	/* Look up the current Vcc */
	CS_CHECK(GetConfigurationInfo, handle, &conf);
	link->conf.Vcc = conf.Vcc;

	/*
	   In this loop, we scan the CIS for configuration table entries,
	   each of which describes a valid card configuration, including
	   voltage, IO window, memory window, and interrupt settings.

	   We make no assumptions about the card to be configured: we use
	   just the information available in the CIS.  In an ideal world,
	   this would work for any PCMCIA card, but it requires a complete
	   and accurate CIS.  In practice, a driver usually "knows" most of
	   these things without consulting the CIS, and most client drivers
	   will only use the CIS to fill in implementation-defined details.
	 */
	tuple.DesiredTuple = CISTPL_CFTABLE_ENTRY;
	CS_CHECK(GetFirstTuple, handle, &tuple);
	while (1) {
		cistpl_cftable_entry_t *cfg = &(parse.cftable_entry);
		CFG_CHECK(GetTupleData, handle, &tuple);
		CFG_CHECK(ParseTuple, handle, &tuple, &parse);

		if (cfg->flags & CISTPL_CFTABLE_DEFAULT)
			dflt = *cfg;
		if (cfg->index == 0)
			goto next_entry;
		link->conf.ConfigIndex = cfg->index;

		/* Does this card need audio output? */
		if (cfg->flags & CISTPL_CFTABLE_AUDIO) {
			link->conf.Attributes |= CONF_ENABLE_SPKR;
			link->conf.Status = CCSR_AUDIO_ENA;
		}

		/* Use power settings for Vcc and Vpp if present */
		/*  Note that the CIS values need to be rescaled */
		if (cfg->vcc.present & (1 << CISTPL_POWER_VNOM)) {
			if (conf.Vcc !=
			    cfg->vcc.param[CISTPL_POWER_VNOM] / 10000) {
				DEBUG(2,
				      "spectrum_cs_config: Vcc mismatch (conf.Vcc = %d, CIS = %d)\n",
				      conf.Vcc,
				      cfg->vcc.param[CISTPL_POWER_VNOM] /
				      10000);
			}
		} else if (dflt.vcc.present & (1 << CISTPL_POWER_VNOM)) {
			if (conf.Vcc !=
			    dflt.vcc.param[CISTPL_POWER_VNOM] / 10000) {
				DEBUG(2,
				      "spectrum_cs_config: Vcc mismatch (conf.Vcc = %d, CIS = %d)\n",
				      conf.Vcc,
				      dflt.vcc.param[CISTPL_POWER_VNOM] /
				      10000);
			}
		}

		if (cfg->vpp1.present & (1 << CISTPL_POWER_VNOM))
			link->conf.Vpp1 = link->conf.Vpp2 =
			    cfg->vpp1.param[CISTPL_POWER_VNOM] / 10000;
		else if (dflt.vpp1.present & (1 << CISTPL_POWER_VNOM))
			link->conf.Vpp1 = link->conf.Vpp2 =
			    dflt.vpp1.param[CISTPL_POWER_VNOM] / 10000;

		/* Do we need to allocate an interrupt? */
		if (cfg->irq.IRQInfo1 || dflt.irq.IRQInfo1)
			link->conf.Attributes |= CONF_ENABLE_IRQ;

		/* IO window settings */
		link->io.NumPorts1 = link->io.NumPorts2 = 0;
		if ((cfg->io.nwin > 0) || (dflt.io.nwin > 0)) {
			cistpl_io_t *io =
			    (cfg->io.nwin) ? &cfg->io : &dflt.io;
			link->io.Attributes1 = IO_DATA_PATH_WIDTH_AUTO;
			if (!(io->flags & CISTPL_IO_8BIT))
				link->io.Attributes1 =
				    IO_DATA_PATH_WIDTH_16;
			if (!(io->flags & CISTPL_IO_16BIT))
				link->io.Attributes1 =
				    IO_DATA_PATH_WIDTH_8;
			link->io.IOAddrLines =
			    io->flags & CISTPL_IO_LINES_MASK;
			link->io.BasePort1 = io->win[0].base;
			link->io.NumPorts1 = io->win[0].len;
			if (io->nwin > 1) {
				link->io.Attributes2 =
				    link->io.Attributes1;
				link->io.BasePort2 = io->win[1].base;
				link->io.NumPorts2 = io->win[1].len;
			}

			/* This reserves IO space but doesn't actually enable it */
			CFG_CHECK(RequestIO, link->handle, &link->io);
		}


		/* If we got this far, we're cool! */

		break;

	      next_entry:
		if (link->io.NumPorts1)
			CardServices(ReleaseIO, link->handle, &link->io);
		last_ret = CardServices(GetNextTuple, handle, &tuple);
		if (last_ret == CS_NO_MORE_ITEMS) {
			printk(KERN_ERR
			       "GetNextTuple().  No matching CIS configuration\n");
			goto cs_failed;
		}
	}

	/*
	   Allocate an interrupt line.  Note that this does not assign a
	   handler to the interrupt, unless the 'Handler' member of the
	   irq structure is initialized.
	 */
	if (link->conf.Attributes & CONF_ENABLE_IRQ) {
		int i;

		link->irq.Attributes =
		    IRQ_TYPE_EXCLUSIVE | IRQ_HANDLE_PRESENT;
		link->irq.IRQInfo1 = IRQ_INFO2_VALID | IRQ_LEVEL_ID;
		if (irq_list[0] == -1)
			link->irq.IRQInfo2 = irq_mask;
		else
			for (i = 0; i < 4; i++)
				link->irq.IRQInfo2 |= 1 << irq_list[i];

		link->irq.Handler = orinoco_interrupt;
		link->irq.Instance = priv;

		CS_CHECK(RequestIRQ, link->handle, &link->irq);
	}

	/* We initialize the hermes structure before completing PCMCIA
	   configuration just in case the interrupt handler gets
	   called. */
	hermes_struct_init(hw, link->io.BasePort1, HERMES_IO,
			   HERMES_16BIT_REGSPACING);

	/*
	   This actually configures the PCMCIA socket -- setting up
	   the I/O windows and the interrupt mapping, and putting the
	   card and host interface into "Memory and IO" mode.
	 */
	CS_CHECK(RequestConfiguration, link->handle, &link->conf);

	ndev->base_addr = link->io.BasePort1;
	ndev->irq = link->irq.AssignedIRQ;

	/*
	   Sanity check to avoid downloading firmware into a wrong card.
	   HFA3842 should have CCSR and 7 address lines.
	 */
	if (link->io.IOAddrLines < 7) {
		printk(KERN_ERR "spectrum_cs: expected at least 7 address "
		       "lines, found just %d\n", link->io.IOAddrLines);
		goto failed;
	}
	if (!(link->conf.Present | PRESENT_STATUS)) {
		printk(KERN_ERR
		       "spectrum_cs: Status register not found\n");
		goto failed;
	}

	/* Reset card and download firmware */
	if (spectrum_dl_firmware(hw, link) != 0) {
		goto failed;
	}

	/* register_netdev will give us an ethX name */
	ndev->name[0] = '\0';
	/* Tell the stack we exist */
	if (register_netdev(ndev) != 0) {
		printk(KERN_ERR "spectrum_cs: register_netdev() failed\n");
		goto failed;
	}
	strcpy(card->node.dev_name, ndev->name);

	/* Finally, report what we've done */
	printk(KERN_DEBUG "%s: index 0x%02x: Vcc %d.%d", ndev->name,
	       link->conf.ConfigIndex, link->conf.Vcc / 10,
	       link->conf.Vcc % 10);
	if (link->conf.Vpp1)
		printk(", Vpp %d.%d", link->conf.Vpp1 / 10,
		       link->conf.Vpp1 % 10);
	if (link->conf.Attributes & CONF_ENABLE_IRQ)
		printk(", irq %d", link->irq.AssignedIRQ);
	if (link->io.NumPorts1)
		printk(", io 0x%04x-0x%04x", link->io.BasePort1,
		       link->io.BasePort1 + link->io.NumPorts1 - 1);
	if (link->io.NumPorts2)
		printk(" & 0x%04x-0x%04x", link->io.BasePort2,
		       link->io.BasePort2 + link->io.NumPorts2 - 1);
	printk("\n");

	/* And give us the proc nodes for debugging */
	if (orinoco_proc_dev_init(priv->ndev) != 0) {
		printk(KERN_ERR
		       "spectrum_cs: Failed to create /proc node for %s\n",
		       ndev->name);
		goto failed;
	}

	/* Note to myself : this replace MOD_INC_USE_COUNT/MOD_DEC_USE_COUNT */
	SET_MODULE_OWNER(ndev);

	/*
	   At this point, the dev_node_t structure(s) need to be
	   initialized and arranged in a linked list at link->dev.
	 */
	card->node.major = card->node.minor = 0;
	link->dev = &card->node;
	link->state &= ~DEV_CONFIG_PENDING;

	TRACE_EXIT("orinoco");

	return;

      cs_failed:
	cs_error(link->handle, last_fn, last_ret);
      failed:
	spectrum_cs_release((u_long) link);

	TRACE_EXIT("orinoco");
}				/* spectrum_cs_config */

/*
 * After a card is removed, spectrum_cs_release() will unregister the
 * device, and release the PCMCIA configuration.  If the device is
 * still open, this will be postponed until it is closed.
 */
static void
spectrum_cs_release(u_long arg)
{
	dev_link_t *link = (dev_link_t *) arg;
	struct orinoco_private *priv = link->priv;

	/*
	   If the device is currently in use, we won't release until it
	   is actually closed, because until then, we can't be sure that
	   no one will try to access the device or its data structures.
	 */
	if (link->open) {
		link->state |= DEV_STALE_CONFIG;
		return;
	}

	/* Unregister proc entry */
	orinoco_proc_dev_cleanup(priv->ndev);

	/* Don't bother checking to see if these succeed or not */
	CardServices(ReleaseConfiguration, link->handle);
	if (link->io.NumPorts1)
		CardServices(ReleaseIO, link->handle, &link->io);
	if (link->irq.AssignedIRQ)
		CardServices(ReleaseIRQ, link->handle, &link->irq);
	link->state &= ~DEV_CONFIG;
}				/* spectrum_cs_release */

/*
 * The card status event handler.  Mostly, this schedules other stuff
 * to run after an event is received.
 */
static int
spectrum_cs_event(event_t event, int priority,
		  event_callback_args_t * args)
{
	dev_link_t *link = args->client_data;
	struct orinoco_private *priv =
	    (struct orinoco_private *) link->priv;
	struct net_device *dev = priv->ndev;
	int err;

	switch (event) {
	case CS_EVENT_CARD_REMOVAL:
		link->state &= ~DEV_PRESENT;
		if (link->state & DEV_CONFIG) {
			netif_stop_queue(dev);
			netif_device_detach(dev);
			mod_timer(&link->release, jiffies + HZ / 20);
		}
		break;
	case CS_EVENT_CARD_INSERTION:
		link->state |= DEV_PRESENT | DEV_CONFIG_PENDING;
		spectrum_cs_config(link);
		break;
	case CS_EVENT_PM_SUSPEND:

		link->state |= DEV_SUSPEND;
		/* Fall through... */
	case CS_EVENT_RESET_PHYSICAL:
		orinoco_shutdown(priv);
		/* Mark the device as stopped, to block IO until later */

		if (link->state & DEV_CONFIG) {
			if (link->open) {
				err = orinoco_commence_reset(priv);
				if (err) {
					printk
					    ("%s: signalled during reset?\n",
					     dev->name);
					return err;
				}
				netif_device_detach(dev);
			}
			CardServices(ReleaseConfiguration, link->handle);
		}
		break;
	case CS_EVENT_PM_RESUME:
		link->state &= ~DEV_SUSPEND;
		/* Fall through... */
	case CS_EVENT_CARD_RESET:
		if (link->state & DEV_CONFIG) {
			CardServices(RequestConfiguration, link->handle,
				     &link->conf);

			/* The firmware needs to be reloaded */
			if (spectrum_dl_firmware(&priv->hw, link) != 0) {
				printk(KERN_ERR
				       "%s: firmware download failed\n",
				       dev->name);
			}

			if (link->open) {
				netif_device_attach(dev);
				err = __orinoco_startup(priv);
				if (err) {
					printk(KERN_ERR
					       "%s: Error resetting device on PCMCIA event\n",
					       dev->name);
					spectrum_cs_stop(dev);
					return err;
				} else {
					netif_start_queue(dev);
				}
			}
		}
		/*
		   In a normal driver, additional code may go here to restore
		   the device state and restart IO. 
		 */
		break;
	}

	return 0;
}				/* spectrum_cs_event */

static int __init
init_spectrum_cs(void)
{
	servinfo_t serv;

	printk(KERN_DEBUG "%s\n", version);

	CardServices(GetCardServicesInfo, &serv);
	if (serv.Revision != CS_RELEASE_CODE) {
		printk(KERN_NOTICE "spectrum_cs: Card Services release "
		       "does not match!\n");
		return -1;
	}

	register_pccard_driver(&dev_info, &spectrum_cs_attach,
			       &spectrum_cs_detach);

	return 0;
}

static void __exit
exit_spectrum_cs(void)
{
	unregister_pccard_driver(&dev_info);

	while (dev_list != NULL) {
		del_timer(&dev_list->release);
		if (dev_list->state & DEV_CONFIG)
			spectrum_cs_release((u_long) dev_list);
		spectrum_cs_detach(dev_list);
	}
}

module_init(init_spectrum_cs);
module_exit(exit_spectrum_cs);
