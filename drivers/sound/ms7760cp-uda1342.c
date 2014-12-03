/*
 * drivers/sound/ms7760cp-uda1342.c
 *
 * Philips UDA1342TS Audio CODEC low level driver for Renesas SH-4
 *   Suport Platform: MS7760 (Hitachi-UL Solution Engine2)
 *
 * Copyright (C) 2003	Takeo Takahashi <takahashi.takeo@renesas.com>
 *
 * This driver is based on the AD1848/CS4284 driver:
 *	  Copyright (C) by Hannu Savolainen 1993-1997
 *
 * This program is free software; you can redistribute  it and/or modify it
 * under  the terms of  the GNU General  Public License as published by the
 * Free Software Foundation;  either version 2 of the  License, or (at your
 * option) any later version.
 *
 */
#define VERSION		"0.4a"

#include <linux/config.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/stddef.h>
#include <asm/irq.h>
#include <asm/dma.h>
#include <asm/delay.h>
#include <asm/ms7760cp.h>

#include "sound_config.h"

#define USE_FIFO_WRITE		1
#define USE_SCL_400		1
#define USE_TIMEOUT_WAIT	0
#define DMA_STUFF_WORKAROUND	1	/* modified dmabuf.c to avoid it */
#define DMA_STOP_WORKAROUND	0
#define SSI_RESTART		0

#define DBG(x)
#define DBG_DUMP_REG		0

/******************************************************************
 * DMA module
 ******************************************************************/
#define DMARSRA_CH0	0x14000000 // CH0 req from DMABRG

/******************************************************************
 * SSI module
 ******************************************************************/
#define SSICR0		0xfe680000 	// Channel 0 -- Input
#define SSISR0		0xfe680004
#define SSITDR0		0xfe680008
#define SSIRDR0		0xfe68000c
#define SSICR1		0xfe690000 	// Channel 1 -- Output
#define SSISR1		0xfe690004
#define SSITDR1		0xfe690008
#define SSIRDR1		0xfe69000c

/* SSICR */
#define SSICR_DMEN	0x10000000	// DMA enable
#define SSICR_UIEN	0x08000000	// underflow interrupt enable
#define SSICR_OIEN	0x04000000	// overflow interrupt enable
#define SSICR_IIEN	0x02000000	// idle mode interrupt enable
#define SSICR_DIEN	0x01000000	// data interrupt enable
#define SSICR_CHNL_1	0x00000000	// 1 channel
#define SSICR_CHNL_2	0x00400000	// 2 channel
#define SSICR_CHNL_3	0x00800000	// 3 channel
#define SSICR_CHNL_4	0x00c00000	// 4 channel
#define SSICR_DWL_8	0x00000000	// 8 bits
#define SSICR_DWL_16	0x00080000	// 16 bits
#define SSICR_DWL_18	0x00100000	// 18 bits
#define SSICR_DWL_20	0x00180000	// 20 bits
#define SSICR_DWL_22	0x00200000	// 22 bits
#define SSICR_DWL_24	0x00280000	// 24 bits
#define SSICR_DWL_32	0x00300000	// 32 bits
#define SSICR_SWL_8	0x00000000	// 8 bits
#define SSICR_SWL_16	0x00010000	// 16 bits
#define SSICR_SWL_24	0x00020000	// 24 bits
#define SSICR_SWL_32	0x00030000	// 32 bits
#define SSICR_SWL_48	0x00040000	// 48 bits
#define SSICR_SWL_64	0x00050000	// 64 bits
#define SSICR_SWL_128	0x00060000	// 128 bits
#define SSICR_SWL_256	0x00070000	// 256 bits
#define SSICR_SCKD	0x00008000	// serial bit-clock output, master
#define SSICR_SWSD	0x00004000	// serial WS output, master
#define SSICR_SCKP	0x00002000	// serial bit clock polarity
#define SSICR_SWSP	0x00001000	// serial WS polarity
#define SSICR_SPDP	0x00000800	// serial padding polarity
#define SSICR_SDTA	0x00000400	// serial data alignment
#define SSICR_PDTA	0x00000200	// parallel data alignment
#define SSICR_DEL	0x00000100	// serial data delay
#define SSICR_BREN	0x00000080	// burst mode
#define SSICR_CKDV_1	0x00000000	// serial=sample/1
#define SSICR_CKDV_2	0x00000010	// serial=sample/2
#define SSICR_CKDV_4	0x00000020	// serial=sample/4
#define SSICR_CKDV_8	0x00000030	// serial=sample/8
#define SSICR_CKDV_16	0x00000040	// serial=sample/16
#define SSICR_MUEN	0x00000008	// mute enable
#define SSICR_CPEN	0x00000004	// compression mode enable
#define SSICR_TRMD	0x00000002	// transfer mode(0:receive, 1:send)
#define SSICR_EN	0x00000001	// SSI module enable

/* SSISR */
#define SSISR_DMRQ	0x10000000	// DMA request
#define SSISR_UIRQ	0x08000000	// underflow error interrupt
#define SSISR_OIRQ	0x04000000	// overflow error interrupt
#define SSISR_IIRQ	0x02000000	// idle mode interrupt
#define SSISR_DIRQ	0x01000000	// data interrupt
#define SSISR_CHNO	0x0000000c	// channel number
#define ssisr_chno(x)	((x) & SSISR_CHNO) >> 2)	// channel number
#define SSISR_SWNO	0x00000002	// serial word number
#define SSISR_IDST	0x00000001	// idle mode status

#define SSICR0_DEFAULT (SSICR_CHNL_1 | SSICR_DWL_16 | SSICR_SWL_16 | \
			SSICR_SCKD   | SSICR_SWSD   | SSICR_CKDV_16)
#define SSICR1_DEFAULT (SSICR_CHNL_1 | SSICR_DWL_16 | SSICR_SWL_16 | \
			SSICR_TRMD)

/******************************************************************
 * I2C module
 ******************************************************************/
#define ICSCR0		0xfe140000 	// channel 0
#define ICMCR0		0xfe140004
#define ICSSR0		0xfe140008
#define ICMSR0		0xfe14000c
#define ICSIER0		0xfe140010
#define ICMIER0		0xfe140014
#define ICCCR0		0xfe140018
#define ICSAR0		0xfe14001c
#define ICMAR0		0xfe140020
#define ICRXD0		0xfe140024
#define ICTXD0		0xfe140024
#define ICFCR0		0xfe140028
#define ICFSR0		0xfe14002c
#define ICFIER0		0xfe140030
#define ICRFDR0		0xfe140034
#define ICTFDR0		0xfe140038
#define ICSCR1		0xfe150000 	// channel 1
#define ICMCR1		0xfe150004
#define ICSSR1		0xfe150008
#define ICMSR1		0xfe15000c
#define ICSIER1		0xfe150010
#define ICMIER1		0xfe150014
#define ICCCR1		0xfe150018
#define ICSAR1		0xfe15001c
#define ICMAR1		0xfe150020
#define ICRXD1		0xfe150024
#define ICTXD1		0xfe150024
#define ICFCR1		0xfe150028
#define ICFSR1		0xfe15002c
#define ICFIER1		0xfe150030
#define ICRFDR1		0xfe150034
#define ICTFDR1		0xfe150038

/******************************************************************
 * DMA
 ******************************************************************/
#define DMAOR_DEFAULT           (DMAOR_DMS_DMABRG | DMAOR_PRI_ROUND)
#define DMAACR0_DEFAULT         (DMAACR_RAM_NOALIGN | DMAACR_TAM_NOALIGN)
#define DMAACR1_DEFAULT         (DMAACR_RAM_NOALIGN | DMAACR_TAM_NOALIGN)
#define DMABRGCR_DEFAULT_INT    (DMABRGCR_A1TXEE | DMABRGCR_A1TXHE)
#define DMABRGCR_DEFAULT_FLAG   (DMABRGCR_A1TXEF | DMABRGCR_A1TXHF)
#define DMABRGCR_DEFAULT_INT_REC  (DMABRGCR_A0RXEE | DMABRGCR_A0RXHE)
#define DMABRGCR_DEFAULT_FLAG_REC (DMABRGCR_A0RXEF | DMABRGCR_A0RXHF)

/******************************************************************
 * Philips UDA1342
 ******************************************************************/
#define UDA1342_CH_1	0x1a	/* input left 1 and right 1 */
#define UDA1342_CH_2	0x1b	/* input left 2 and right 2 */

/******************************************************************
 * Low-level driver
 ******************************************************************/
typedef struct {
        int base;          /* set in attach */
	int irq;
	int dma_playback;
        int dma_capture;
  
        int speed;         /* open */
	int channels;
	int audio_format;
	unsigned char format_bits;
        int audio_mode; 
	int opened;
  
        int recmask;        /* setup */
	int supported_devices;
	int supported_rec_devices;
	unsigned short levels[SOUND_MIXER_NRDEVICES];
        int dev_no;   /* this is the # in audio_devs and NOT 
				    in uda1342_t */
	int irq_ok;
	int *osp;
  
} uda1342_t;

static int nr_uda1342_devs;
static int ad_format_mask = (AFMT_U8 | AFMT_S16_LE | AFMT_S16_BE | AFMT_MU_LAW | AFMT_A_LAW);
static uda1342_t dev_info[MAX_AUDIO_DEV];
static int verbose = 0;
static unsigned int master_volume = 0;
static unsigned int bass = 0;
static unsigned int treble = 0;
#define MIXER_MUTE_ON	1
#define MIXER_MUTE_OFF	0
static unsigned int master_mute = MIXER_MUTE_OFF;	/* never changed */
static unsigned int mta_mute = MIXER_MUTE_OFF;
static unsigned int mtb_mute = MIXER_MUTE_OFF;
static unsigned int pda_porarity = 0;	/* 0:non-inverting, others:interting */ 
static unsigned int mix_reset = 1;

static void print_uda1342(void);
static void init_codec(int reset);
static void uda1342_mixer_dac_mute(int on);
static void uda1342_mixer_adc_mute(int on);

static int open_mode = 0;

/******************************************************************
 * Utility functions for debugging
 ******************************************************************/
#if DBG_DUMP_REG
static void check_registers(void);
static void check_registers(void)
{
	printk("DMABRGCR=0x%08x\n", ctrl_inl(DMABRGCR));
	printk("DMAOR=0x%08x\n", ctrl_inl(DMAOR));
//	printk("DMATCR0=0x%08x\n", ctrl_inl(DMATCR[0]));
//	printk("CHCR0=0x%08x\n", ctrl_inl(CHCR[0]));
	printk("DMARCR=0x%08x\n", ctrl_inl(DMARCR));
	printk("DMARSRA=0x%08x\n", ctrl_inl(DMARSRA));
	printk("SSICR0=0x%08x\n", ctrl_inl(SSICR0));
	printk("SSISR0=0x%08x\n", ctrl_inl(SSISR0));
	printk("SSICR1=0x%08x\n", ctrl_inl(SSICR1));
	printk("SSISR1=0x%08x\n", ctrl_inl(SSISR1));
	printk("DMAATXSAR1=0x%08x\n", ctrl_inl(DMAATXSAR1));
	printk("DMAATXTCR1=0x%08x\n", ctrl_inl(DMAATXTCR1));
	printk("IPSEL=0x%04x\n", ctrl_inl(0xfe400034));
	printk("DMAACR=0x%08x\n", ctrl_inl(DMAACR1));
	printk("DMAATXTCNT=0x%08x\n", ctrl_inl(0xfe3c0074));// DMAATXTCNT1
	printk("INTC_INTPRI00=0x%08x\n", ctrl_inl(INTC_INTPRI00));
	printk("INTC_INTPRI04=0x%08x\n", ctrl_inl(INTC_INTPRI04));
	printk("INTC_INTPRI08=0x%08x\n", ctrl_inl(INTC_INTPRI08));
	printk("INTC_INTREQ00=0x%08x\n", ctrl_inl(INTC_INTREQ00));
	printk("INTC_INTREQ04=0x%08x\n", ctrl_inl(INTC_INTREQ04));
	printk("INTC_INTMSK00=0x%08x\n", ctrl_inl(INTC_INTMSK00));
	printk("INTC_INTMSK04=0x%08x\n", ctrl_inl(INTC_INTMSK04));
	printk("CLKSTP00=0x%08x\n", ctrl_inl(CLKSTP00));
	if (ctrl_inl(DMAOR) & 0x4) {
		printk(KERN_ERR "%s: *** DMA address error !!!\n", __func__);
	}
	if (ctrl_inl(DMAOR) & 0x2) {
		printk(KERN_ERR "%s: *** NMIF !!!\n", __func__);
	}
}
#endif	/* DBG_UTIL */

/******************************************************************
 * I2C functions
 ******************************************************************/
#define I2C_TIMEOUT	0x100000
#if USE_TIMEOUT_WAIT
#define WAIT_FOR_STATUS(x)	snd_sh_i2c_wait(x)
#define WAIT_FOR_FSTATUS(x)	snd_sh_i2c_fifo_wait(x)
#else
#define WAIT_FOR_STATUS(x)	while ((ctrl_inl(ICMSR0) & (x)) != (x))
#define WAIT_FOR_FSTATUS(x)	while ((ctrl_inl(ICFSR0) & (x)) != (x))
#endif

static void snd_sh_i2c_init(void)
{
	DBG(printk(KERN_INFO "%s: \n", __func__));
#if USE_SCL_400
	ctrl_outl(0x00000006, ICCCR0);	/* SCL 400kHz */
#else
	ctrl_outl(0x00000023, ICCCR0);	/* SCL 100kHz */
#endif
	ctrl_outl(0x00000000, ICSCR0);
	ctrl_outl(0x00000000, ICSSR0);
	ctrl_outl(0x00000000, ICSIER0);
	ctrl_outl(0x00000000, ICSAR0);
	ctrl_outl(0x00000000, ICMCR0);
	ctrl_outl(0x00000000, ICMSR0);
	ctrl_outl(0x00000000, ICMIER0);
	ctrl_outl(0x00000000, ICMAR0);
#if USE_FIFO_WRITE
	ctrl_outl(0x000000ff, ICFCR0);	// RTRG=16, TTRG=16, RFRST=1, TFRST=1
#else
	ctrl_outl(0x00000001, ICFCR0);
#endif
	ctrl_outl(0x00000000, ICFSR0);
	ctrl_outl(0x00000000, ICFIER0);
#if USE_FIFO_WRITE
	ctrl_outl(0x000000fc, ICFCR0);	// RTRG=16, TTRG=16
#else
	ctrl_outl(0x00000000, ICFCR0);
#endif
}

#if USE_TIMEOUT_WAIT
static void snd_sh_i2c_wait(unsigned long wait_status)
{
	int i;
	int tmout = I2C_TIMEOUT;
	unsigned long status;

	// already saved flags and cli
	for (i = 0; i < tmout; i++) {
		status = ctrl_inl(ICMSR0);
		if ((status & wait_status) == wait_status)
			break;
	}
	if (i >= tmout)
		printk(KERN_INFO "i2c_wait time-out (0x%08lx).\n", status);
}

static void snd_sh_i2c_fifo_wait(unsigned long wait_status)
{
	int i;
	int tmout = I2C_TIMEOUT;
	unsigned long status;

	// already saved flags and cli
	for (i = 0; i < tmout; i++) {
		status = ctrl_inl(ICFSR0);
		if ((status & wait_status) == wait_status)
			break;
	}
	if (i >= tmout)
		printk(KERN_INFO "i2c_fifo_wait time-out (0x%08lx).\n", status);
}
#endif

static unsigned long snd_sh_i2c_read(unsigned long dev, unsigned long reg)
{
	unsigned long val1, val2;
	unsigned long flags;

	DBG(printk("%s: dev=%d, reg=0x%lx\n", __func__, (unsigned int)dev, reg));

	save_and_cli(flags);
	ctrl_outl(dev << 1, ICMAR0);		/* slave address, R/W=0(W) */
	ctrl_outl(reg, ICTXD0);			/* regsiter address */
	ctrl_outl(0x00000089, ICMCR0);		/* single-buffer,enable,start */
	WAIT_FOR_STATUS(0x00000009); 		/* MDE=1, MAT=1 */
	ctrl_outl(ctrl_inl(ICMSR0) & 0xfffffff6, ICMSR0); /* clear MDE, MAT */

	ctrl_outl((dev << 1)| 1, ICMAR0);	/* slave address, R/W=1(R) */
	WAIT_FOR_STATUS(0x00000003); 		/* MAT=1, MDR=1 */
	ctrl_outl(0x00000088, ICMCR0);
	ctrl_outl(ctrl_inl(ICMSR0) & 0xfffffffc, ICMSR0); /* clear MDR, MAT */

	WAIT_FOR_STATUS(0x00000002); 		/* MDR=1 */
	val1 = ctrl_inl(ICRXD0);		/* upper data */
	ctrl_outl(0x0000008a, ICMCR0);
	ctrl_outl(ctrl_inl(ICMSR0) & 0xfffffffd, ICMSR0); /* clear MDR */

	WAIT_FOR_STATUS(0x00000002); 		/* MDR=1 */
	val2 = ctrl_inl(ICRXD0);		/* lower data */
	ctrl_outl(ctrl_inl(ICMSR0) & 0xfffffffd, ICMSR0); /* clear MDR */

	WAIT_FOR_STATUS(0x00000010); 		/* MST=1 */
	ctrl_outl(0x00000000, ICMSR0);

	restore_flags (flags);
	DBG(printk(KERN_INFO "%s: val=0x%04x\n", __func__, (unsigned int)(((val1&0xff)<<8)|(val2&0xff))));
	return (((val1 & 0xff) << 8) | (val2 & 0xff));
}

#if USE_FIFO_WRITE
static void snd_sh_i2c_write(unsigned long dev, unsigned long reg, unsigned long val)
{
	unsigned long flags;
	unsigned long val1, val2;

	DBG(printk("%s: dev=%d, ** reg=0x%lx **, val=0x%lx\n", __func__, (unsigned int)dev, reg, val));

	val1 = (val >> 8) & 0xff;
	val2 = (val & 0xff);

	save_and_cli(flags);
	ctrl_outl(0, ICFSR0);		// clear TEND, RDFE, TRFE
	ctrl_outl(dev << 1, ICMAR0);	// slave address & R/W=0(write)
	ctrl_outl(reg, ICTXD0);		// register address
	ctrl_outl(ctrl_inl(ICFSR0) & 0xfffffffe, ICFSR0);  // clear TDFE
	ctrl_outl(0x00000009, ICMCR0);	// MIE, ESG
	WAIT_FOR_STATUS(0x00000001);	// MAT=1
	ctrl_outl(0x00000008, ICMCR0);	// clear ESG
	ctrl_outl(ctrl_inl(ICMSR0) & 0xfffffff6, ICMSR0);  /* clear MAT,MDE */
	WAIT_FOR_FSTATUS(0x00000001);	// TDFE=1
	ctrl_outl(0, ICFSR0);		// clear ICFSR
	ctrl_outl(val1, ICTXD0);	/* write upper data */
	WAIT_FOR_FSTATUS(0x00000001);	// TDFE=1
	ctrl_outl(0, ICFSR0);		// clear ICFSR
	ctrl_outl(val2, ICTXD0);	/* write lower data */
	WAIT_FOR_FSTATUS(0x00000001);	// TDFE=1
	ctrl_outl(0, ICFSR0);		// clear ICFSR
	WAIT_FOR_FSTATUS(0x00000004);	// TEND=1
	ctrl_outl(0xa, ICMCR0);		// FSB
	ctrl_outl(ctrl_inl(ICFSR0) & 0xfffffffb, ICFSR0);	// clear TEND
	WAIT_FOR_STATUS(0x00000010);	// MST=1
	ctrl_outl(0, ICMSR0);
	ctrl_outl(0, ICFSR0);
	restore_flags (flags);
}
#else	/* ! USE_FIFO_WRITE */
/*
 * This function does not work fine.
 * DON'T USE IT.
 */
static void snd_sh_i2c_write(unsigned long dev, unsigned long reg, unsigned long val)
{
	unsigned long flags;
	unsigned long val1, val2;

	DBG(printk("%s: dev=%d, ** reg=0x%lx **, val=0x%lx\n", __func__, (unsigned int)dev, reg, val));

	val1 = (val >> 8) & 0xff;
	val2 = (val & 0xff);

	save_and_cli(flags);
	ctrl_outl(dev << 1, ICMAR0);	/* slave address, R/W=0(write) */
	ctrl_outl(reg, ICTXD0);
	ctrl_outl(0x00000089, ICMCR0);
	WAIT_FOR_STATUS(0x00000009);
	ctrl_outl(0x00000088, ICMCR0);
	ctrl_outl(ctrl_inl(ICMSR0) & 0xfffffff6, ICMSR0);  /* clear MAT,MDE */

	ctrl_outl(val1, ICTXD0);	/* write upper data */
	WAIT_FOR_STATUS(0x00000004);		// wait for MDT
	ctrl_outl(ctrl_inl(ICMSR0) & 0xfffffffb, ICMSR0);  // clear MDT
	WAIT_FOR_STATUS(0x00000004);		// wait for MDT
	WAIT_FOR_STATUS(0x00000008);		// make sure MDE=1
	ctrl_outl(val2, ICTXD0);	/* write lower data */
	ctrl_outl(ctrl_inl(ICMSR0) & 0xfffffff3, ICMSR0);  // clear MDE, MDT
	ctrl_outl(0x0000008a, ICMCR0);		// set FSB
	WAIT_FOR_STATUS(0x00000010);		// wait for MST
	DBG(printk("**** ICMSR0 = 0x%08x\n", ctrl_inl(ICMSR0)));
	ctrl_outl(0x00000000, ICMSR0);
	restore_flags (flags);
}
#endif	/* ! USE_FIFO_WRITE */


/******************************************************************
 * Audio Driver Interface functions
 ******************************************************************/
#if 0
static void uda1342_copy_from_user(
	int dev,
	char *dma_buf,
	int dma_buf_offs,
	const char *userbuf,
	int useroffs,
	int request_cnt,
	int buffsize,
	int *used,
	int *returned,
	int len)
{
	int fragment;

	if (0) printk("%s: len=%d\n", __func__, len);
	fragment = audio_devs[dev]->dmap_out->fragment_size;
	copy_from_user (dma_buf + dma_buf_offs, userbuf + useroffs, len);
	if (len < fragment) {
		memset(dma_buf + dma_buf_offs + len, 
			audio_devs[dev]->dmap_out->neutral_byte,
			fragment - len);
	}
	*used = len;
	*returned = len;
}
#endif

static void uda1342_halt_input(int dev)
{
	uda1342_t *devc = (uda1342_t *)audio_devs[dev]->devc;
	unsigned long flags;
	unsigned long dmaor;
	unsigned long dmabrgcr;
	int tmout = 3000;

	DBG(printk("%s: dev=%d\n", __func__, dev));
	
	save_and_cli(flags);
	if (1) uda1342_mixer_dac_mute(MIXER_MUTE_ON);
	
	/* disable DMA interrupts before stop SSI */
	dmabrgcr = ctrl_inl(DMABRGCR);
	dmabrgcr &= ~DMABRGCR_DEFAULT_INT_REC;
	ctrl_outl(dmabrgcr, DMABRGCR);
	ctrl_inl(DMABRGCR);

	/* SSI must be disabled before stop DMA */
	//ctrl_outl(audio_devs[dev]->dmap_out->neutral_byte, SSITDR1);
	ctrl_outl(SSICR0_DEFAULT, SSICR0); 
	//ctrl_outl(0, SSISR0);			// clear status
	ctrl_inl(SSICR0); 

	/* stop DMA */
	ctrl_outl(DMAACR0_DEFAULT | DMAACR_RDS, DMAACR0);	// disable DMA
	for (; tmout > 0; --tmout) {
		if ((ctrl_inl(DMAACR0) & DMAACR_RDS) == 0) break;
	}
	if (tmout <= 0) {
		printk("uda1342: unable to stop DMA\n");
	}
	//ctrl_outl(DMAACR0_DEFAULT, DMAACR0);			// disable DMA

	/* check error and clear error bit */
	dmaor = ctrl_inl(DMAOR);
	if (dmaor & DMAOR_AE) {
		printk(KERN_ERR "uda1342: DMA address error.\n");
		dmaor &= ~DMAOR_AE;			// clear error bit AE=0
		ctrl_outl(dmaor, DMAOR);
	}
	devc->audio_mode &= ~PCM_ENABLE_INPUT;
	restore_flags (flags);
}

static void uda1342_halt_output(int dev)
{
	uda1342_t *devc = (uda1342_t *)audio_devs[dev]->devc;
	unsigned long  flags;
	unsigned long dmaor;
	unsigned long dmabrgcr;
	int tmout = 3000;
	
	if (0) printk("%s: dev=%d\n", __func__, dev);

	save_and_cli(flags);
	if (1) uda1342_mixer_dac_mute(MIXER_MUTE_ON);
	
	/* disable DMA interrupts before stop SSI */
	dmabrgcr = ctrl_inl(DMABRGCR);
	dmabrgcr &= ~DMABRGCR_DEFAULT_INT;
	ctrl_outl(dmabrgcr, DMABRGCR);
	ctrl_inl(DMABRGCR);

	/* SSI must be disabled before stop DMA */
	//ctrl_outl(audio_devs[dev]->dmap_out->neutral_byte, SSITDR1);
	ctrl_outl(SSICR1_DEFAULT, SSICR1); 
	//ctrl_outl(0, SSISR1);			// clear status
	ctrl_inl(SSICR1); 

	/* stop DMA */
	ctrl_outl(DMAACR1_DEFAULT | DMAACR_TDS, DMAACR1);	// disable DMA
	for (; tmout > 0; --tmout) {
		if ((ctrl_inl(DMAACR1) & DMAACR_TDS) == 0) break;
	}
	if (tmout <= 0) {
		printk("uda1342: unable to stop DMA\n");
	}
	//ctrl_outl(DMAACR1_DEFAULT, DMAACR1);			// disable DMA

	/* check error and clear error bit */
	dmaor = ctrl_inl(DMAOR);
	if (dmaor & DMAOR_AE) {
		printk(KERN_ERR "uda1342: DMA address error.\n");
		dmaor &= ~DMAOR_AE;			// clear error bit AE=0
		ctrl_outl(dmaor, DMAOR);
	}
	devc->audio_mode &= ~PCM_ENABLE_OUTPUT;

	restore_flags(flags);
}

static void uda1342_output_block(int dev, unsigned long buf, int count, int intrflag)
{
	uda1342_t *devc = (uda1342_t *)audio_devs[dev]->devc;
	unsigned long flags;
	
	if (0) printk("%s: dev=%d, buf=0x%lx, count=%d, intrflag=%d\n", __func__, dev, buf, count, intrflag);
  
	save_and_cli(flags);
	if (0) memset(phys_to_virt(buf), 0, count);	/* for silence test */
#if DBG_DUMP_REG
	check_registers();
#endif
	devc->audio_mode |= PCM_ENABLE_OUTPUT; 
	restore_flags (flags);
}


static void uda1342_start_input(int dev, unsigned long buf, int count, int intrflag)
{
	uda1342_t *devc = (uda1342_t *)audio_devs[dev]->devc;
	unsigned long flags;
	
	DBG(printk("%s: dev=%d, buf=0x%lx, count=%d, intrflag=%d\n", __func__, dev, buf, count, intrflag));

	save_and_cli(flags);
	/*
 	 * TODO
	 */
	devc->audio_mode |= PCM_ENABLE_INPUT;
	restore_flags (flags);
}

static int uda1342_prepare_for_input (int dev, int bsize, int bcount)
{
	//uda1342_t *devc = (uda1342_t *)audio_devs[dev]->devc;
	unsigned long flags;
	unsigned long dmaor;
	unsigned long dmarcr;
	unsigned long dmarsra;
	unsigned long buf;
	int buffsize; 
	
	DBG(printk("%s: dev=%d, bsize=%d, bcount=%d\n", __func__, dev, bsize, bcount));
	save_and_cli(flags);

	init_codec_rec(1);

	uda1342_mixer_dac_mute(MIXER_MUTE_OFF);

	audio_devs[dev]->dmap_in->flags |= DMA_NODMA;

	ctrl_outl(SSICR0_DEFAULT | SSICR_DMEN | SSICR_EN, SSICR0); 

	ctrl_outl(DMABRGCR_DEFAULT_INT_REC, DMABRGCR);

	// check DMAOR condition and enable it
	dmaor = ctrl_inl(DMAOR);
	if ((dmaor & DMAOR_DEFAULT) != DMAOR_DEFAULT) {
		printk("uda1342: resetting DMAOR 0x%08lx => 0x%08x\n",
				dmaor, DMAOR_DEFAULT);
		dmaor |= DMAOR_DEFAULT;
		ctrl_outl(dmaor, DMAOR);
	}
	if ((dmaor & DMAOR_MASTER_EN) != DMAOR_MASTER_EN) {
		printk("uda1342: enable DMAOR EN 0x%08lx => 0x%08x\n",
				dmaor, DMAOR_MASTER_EN);
		dmaor |= DMAOR_MASTER_EN;
		ctrl_outl(dmaor, DMAOR);
	}

	// check DMARCR condition and set the priority mode
	dmarcr = ctrl_inl(DMARCR);
	if ((dmarcr & DMARCR_RPR_MODE01) != DMARCR_RPR_MODE01) {
		printk("uda1342: resetting DMARCR 0x%08lx => 0x%08x\n",
				dmarcr, DMARCR_RPR_MODE01);
		dmarcr |= DMARCR_RPR_MODE01;
		ctrl_outl(dmarcr, DMARCR);
	}
	dmarsra = ctrl_inl(DMARSRA);
	if ((dmarsra & DMARSRA_CH0) != DMARSRA_CH0) {
		printk("uda1342: select CH0 in DMARSRA 0x%08lx => 0x%08x\n",
				dmarsra, (DMARSRA_CH0 | DMARSRA_CH0_WEN));
		dmarsra |= (DMARSRA_CH0 | DMARSRA_CH0_WEN);
		ctrl_outl(dmarsra, DMARSRA);
	}
	buf = audio_devs[dev]->dmap_in->raw_buf_phys;
	buffsize = audio_devs[dev]->dmap_in->bytes_in_use;

	if (0) printk("raw_buf_phys = %08x, buffsize = %d\n",
		      (unsigned int)buf, buffsize);
	ctrl_outl(buf, DMAARXDAR0);		// source address
	ctrl_outl(buffsize, DMAARXTCR0);	// transfer count
	ctrl_outl(DMAACR0_DEFAULT | DMAACR_RAR | DMAACR_RDE, DMAACR0);
	restore_flags (flags);

	return 0;
}

static int uda1342_prepare_for_output (int dev, int bsize, int bcount)
{
	//uda1342_t *devc = (uda1342_t *)audio_devs[dev]->devc;
	unsigned long flags;
	unsigned long dmaor;
	unsigned long dmarcr;
	unsigned long dmarsra;
	unsigned long buf;
	int buffsize; 

	if (0) printk("%s: dev=%d, fragments=%d, count=%d\n", __func__, dev, bsize, bcount);

	/* check fragment size (should be 4 byte alignment) */
	if (bsize & 3) {
		printk(KERN_ERR "uda1342: invalid fragment size %d\n", bsize);
		return -1;
	}

	save_and_cli(flags);

	init_codec2(1);

	uda1342_mixer_dac_mute(MIXER_MUTE_OFF);

	audio_devs[dev]->dmap_out->flags |= DMA_NODMA;

	ctrl_outl(SSICR0_DEFAULT | SSICR_DMEN | SSICR_EN, SSICR0);
	ctrl_outl(SSICR1_DEFAULT | SSICR_DMEN | SSICR_EN, SSICR1); 

	ctrl_outl(DMABRGCR_DEFAULT_INT, DMABRGCR);

	// check DMAOR condition and enable it
	dmaor = ctrl_inl(DMAOR);
	if ((dmaor & DMAOR_DEFAULT) != DMAOR_DEFAULT) {
		printk("uda1342: resetting DMAOR 0x%08lx => 0x%08x\n",
				dmaor, DMAOR_DEFAULT);
		dmaor |= DMAOR_DEFAULT;
		ctrl_outl(dmaor, DMAOR);
	}
	if ((dmaor & DMAOR_MASTER_EN) != DMAOR_MASTER_EN) {
		printk("uda1342: enable DMAOR EN 0x%08lx => 0x%08x\n",
				dmaor, DMAOR_MASTER_EN);
		dmaor |= DMAOR_MASTER_EN;
		ctrl_outl(dmaor, DMAOR);
	}

	// check DMARCR condition and set the priority mode
	dmarcr = ctrl_inl(DMARCR);
	if ((dmarcr & DMARCR_RPR_MODE01) != DMARCR_RPR_MODE01) {
		printk("uda1342: resetting DMARCR 0x%08lx => 0x%08x\n",
				dmarcr, DMARCR_RPR_MODE01);
		dmarcr |= DMARCR_RPR_MODE01;
		ctrl_outl(dmarcr, DMARCR);
	}
	dmarsra = ctrl_inl(DMARSRA);
	if ((dmarsra & DMARSRA_CH0) != DMARSRA_CH0) {
		printk("uda1342: select CH0 in DMARSRA 0x%08lx => 0x%08x\n",
				dmarsra, (DMARSRA_CH0 | DMARSRA_CH0_WEN));
		dmarsra |= (DMARSRA_CH0 | DMARSRA_CH0_WEN);
		ctrl_outl(dmarsra, DMARSRA);
	}
	buf = audio_devs[dev]->dmap_out->raw_buf_phys;
	buffsize = audio_devs[dev]->dmap_out->bytes_in_use;

	if (0) printk("raw_buf_phys = %08x, buffsize = %d\n",
		      (unsigned int)buf, buffsize);
	ctrl_outl(buf, DMAATXSAR1);		// source address
	ctrl_outl(buffsize, DMAATXTCR1);	// transfer count
	ctrl_outl(DMAACR1_DEFAULT | DMAACR_TAR | DMAACR_TDE, DMAACR1);
	restore_flags (flags);

	return 0;
}

static void uda1342_trigger (int dev, int state) 
{
	uda1342_t *devc = (uda1342_t *)audio_devs[dev]->devc;
	unsigned long flags;

	if (0) printk("%s: dev=%d, state=0x%08x\n", __func__, dev, state);

	save_and_cli(flags);

	state &= devc->audio_mode; 
				
	if (state & PCM_ENABLE_INPUT) { 	/* enable capture */
		DBG(printk("%s: disable capture\n", __func__));
	} else { 				/* disable capture */
		DBG(printk("%s: disable capture\n", __func__));
	}

	if (state & PCM_ENABLE_OUTPUT) { 	/* enable playback */
		DBG(printk("%s: enable playback\n", __func__));
#if DMA_STOP_WORKAROUND
		if (audio_devs[dev]->dmap_out->underrun_count == 0) {
			if (0) uda1342_mixer_dac_mute(MIXER_MUTE_OFF);
			if (0) ctrl_outl(DMAACR1_DEFAULT | DMAACR_TDE, DMAACR1);
		} else {
			if (0) uda1342_mixer_dac_mute(MIXER_MUTE_ON);
			if (0) printk(KERN_ERR "uda1342: underrun happens\n");
		}
#endif
#if DBG_DUMP_REG
		check_registers();
#endif
	} else {
		DBG(printk("%s: disable playback\n", __func__));
		if (0) uda1342_mixer_dac_mute(MIXER_MUTE_ON);
	}
	restore_flags(flags);
}

static void uda1342_halt_io(int dev)
{
	if (open_mode & 0x1)
		uda1342_halt_input(dev);
	if (open_mode & 0x2)
		uda1342_halt_output(dev);
}

static int uda1342_set_speed(int dev, int arg)
{
	uda1342_t *devc = (uda1342_t *)audio_devs[dev]->devc;
	
	DBG(printk("%s: speed = %d\n", __func__, arg));

	if (arg == 0) return devc->speed;
	if (arg != 44100) arg = 44100; 	/* support only 44.1kHz */
	devc->speed = arg;
	DBG(printk("%s: speed = %d\n", __func__, devc->speed));
	return devc->speed;
}

static unsigned int uda1342_set_bits(int dev, unsigned int arg)
{
	uda1342_t *devc = (uda1342_t *)audio_devs[dev]->devc;

	if (0) printk("%s: dev=%d, audio_format=0x%08x\n", __func__, (unsigned int)dev, (unsigned int)arg);

#if 1
	static struct format_tbl {
		int             format;
		unsigned char   bits;
	} format2bits[] = {
		{ 0, 0 },
		{ AFMT_MU_LAW, 1 },
		{ AFMT_A_LAW, 3 },
		{ AFMT_IMA_ADPCM, 0 },
		{ AFMT_U8, 0 },
		{ AFMT_S16_LE, 2 },
		{ AFMT_S16_BE, 6 },
		{ AFMT_S8, 0 },
		{ AFMT_U16_LE, 0 },
		{ AFMT_U16_BE, 0 }
  	};
	int  i, n = sizeof (format2bits) / sizeof (struct format_tbl);

	if (arg == 0) return devc->audio_format;
	devc->audio_format = arg;
	/* search matching format bits */
	for (i = 0; i < n; i++) {
		if (format2bits[i].format == arg) {
			devc->format_bits = format2bits[i].bits;
			devc->audio_format = arg;
			return arg;
		}
	}

	/* Still hanging here. Something must be terribly wrong */
	devc->format_bits = 0;
#endif
	devc->audio_format = AFMT_U16_LE;
	return devc->audio_format;
}

static short uda1342_set_channels(int dev, short arg)
{
	uda1342_t *devc = (uda1342_t *)audio_devs[dev]->devc;

	DBG(printk("%s: dev=%d, arg=%d\n", __func__, dev, arg));

	if (arg != 1 && arg != 2)
		return devc->channels;

	devc->channels = arg;
	return arg;
}

static int uda1342_open(int dev, int mode) 
{
	uda1342_t *devc;
	unsigned long flags;

	DBG(printk("%s: dev=%d, mode=0x%08x\n", __func__, dev, mode));

	/* check device id */
	if (dev < 0 || dev >= num_audiodevs) return -ENXIO;

	devc = (uda1342_t *)audio_devs[dev]->devc; 

	save_and_cli(flags);
	if (devc->opened) {
		restore_flags (flags);
		return -EBUSY;
	}
	/* initialize status */
	devc->opened = 1; 
	devc->audio_mode = 0;
	devc->speed = 44100;		// 44.1kHz
	devc->audio_format = AFMT_U16_LE;
	devc->channels=1;
	devc->irq_ok = 0;

	open_mode = mode;

	restore_flags (flags);
	return 0;
}

static void uda1342_close(int dev)
{
	uda1342_t *devc = (uda1342_t *)audio_devs[dev]->devc;
	unsigned long flags;

	DBG(printk("%s: dev=%d\n", __func__, dev));

	save_and_cli(flags);
	//uda1342_halt_io(devc->dev_no);
	devc->opened = 0;
	devc->audio_mode = 0;
	devc->speed = 44100;
	devc->audio_format = AFMT_U16_LE;
	devc->format_bits = 0;

	open_mode = 0;

	restore_flags (flags);
}

static struct audio_driver uda1342_audio_driver = {
	owner:			THIS_MODULE,
	open:			uda1342_open,
	close:			uda1342_close,
	output_block:		uda1342_output_block,
	start_input:		uda1342_start_input,
	prepare_for_input:	uda1342_prepare_for_input,
	prepare_for_output:	uda1342_prepare_for_output,
	halt_io:		uda1342_halt_io,
	halt_input:		uda1342_halt_input,
	halt_output:		uda1342_halt_output,
/*
	copy_user:		uda1342_copy_from_user,
*/
	trigger:		uda1342_trigger,
	set_speed:		uda1342_set_speed,
	set_bits:		uda1342_set_bits,
	set_channels:		uda1342_set_channels,
};

/*
 * Interrupt handler is called by DMABRGI1 and DMABRGI2.
 */
static void uda1342_interrupt(int irq, void *dev_id, struct pt_regs *dummy)
{
	uda1342_t *devc;
	unsigned long status;
	int dev = (int)dev_id;

	if (0) printk("%s: irq=%d\n", __func__, irq);

	devc = (uda1342_t *)audio_devs[dev]->devc;
	
	status = ctrl_inl(DMABRGCR);
	DBG(printk("%s: interrupt status 0x%08lx\n", __func__, status));
#if DBG_DUMP_REG
	check_registers();
#endif

	if (status & DMABRGCR_DEFAULT_FLAG) {	// check end of DMA transfer
		devc->irq_ok=1;
		status &= ~DMABRGCR_DEFAULT_FLAG;	// clear flag
		ctrl_outl(status, DMABRGCR);
#if DBG_DUMP_REG
		check_registers();
#endif
		if (devc->opened && (devc->audio_mode & PCM_ENABLE_OUTPUT))
			DMAbuf_outputintr (dev, 1);
		devc->irq_ok=0;
	}
	else if (status & DMABRGCR_DEFAULT_FLAG_REC) {	// check end of DMA transfer
		devc->irq_ok=1;
		status &= ~DMABRGCR_DEFAULT_FLAG_REC;	// clear flag
		ctrl_outl(status, DMABRGCR);
#if DBG_DUMP_REG
		check_registers();
#endif
		if (devc->opened && (devc->audio_mode & PCM_ENABLE_INPUT))
			DMAbuf_inputintr(dev);
		devc->irq_ok=0;
	}
}

/******************************************************************
 * Mixer device Interface functions
 *
 * Supported features is as follows:
 *
 * -------------------------------------------------------
 * Functions	UDA1342 functionality
 * -------------------------------------------------------
 * VOLUME:	master volume left and right
 * BASS:	DAC feature BB3-BB0
 * TREBLE:	DAC feature TR1-TR0
 * SYNTH:	-
 * PCM:		- 
 * SPEAKER:	- (Electrical volume supproted by H8 power control)
 * LINE:	ADC input channel 2
 * MIC:		ADC input channel 1
 * CD:		-
 * IMIX:	-
 * ALTPCM:	-
 * RECLEV:	-	
 * IGAIN:	-
 * LINE1:	-
 * LINE2:	-
 * LINE3:	-
 ******************************************************************/
#if 0
static char mix_cvt[101] = {
	 0, 0, 3, 7,10,13,16,19,21,23,26,28,30,32,34,35,37,39,40,42,
	43,45,46,47,49,50,51,52,53,55,56,57,58,59,60,61,62,63,64,65,
	65,66,67,68,69,70,70,71,72,73,73,74,75,75,76,77,77,78,79,79,
	80,81,81,82,82,83,84,84,85,85,86,86,87,87,88,88,89,89,90,90,
	91,91,92,92,93,93,94,94,95,95,96,96,96,97,97,98,98,98,99,99,
	100
};
#endif

typedef struct {
	unsigned int regno: 7;
	unsigned int polarity:1;	/* 0=normal, 1=reversed */
	unsigned int bitpos:4;
	unsigned int nbits:4;
} mixer_ent;

#define LEFT_CHN	0
#define RIGHT_CHN	1

#define MIXENT(name, reg_l, pola_l, pos_l, len_l, reg_r, pola_r, pos_r, len_r)\
  {{reg_l, pola_l, pos_l, len_l}, {reg_r, pola_r, pos_r, len_r}}

mixer_ent mix_devices[SOUND_MIXER_NRDEVICES][2] = {
// device			LEFT			RIGHT
MIXENT(SOUND_MIXER_VOLUME,	0x11,  1,  8,  8,	0x11,  1,  0,  8),
MIXENT(SOUND_MIXER_BASS,	0x10,  0, 10,  4,	0x10,  0, 10,  0),
MIXENT(SOUND_MIXER_TREBLE,	0x10,  0,  8,  2,	0x10,  0,  8,  0),
MIXENT(SOUND_MIXER_SYNTH,	   0,  0,  0,  0,	   0,  0,  0,  0),
MIXENT(SOUND_MIXER_PCM,	   	   0,  0,  0,  0,	   0,  0,  0,  0),
MIXENT(SOUND_MIXER_SPEAKER,	   0,  0,  0,  0,	   0,  0,  0,  0),
MIXENT(SOUND_MIXER_LINE,	0x21,  0,  8,  4,	0x21,  0,  8,  0),
MIXENT(SOUND_MIXER_MIC,		0x20,  0,  8,  4,	0x20,  0,  8,  0),
MIXENT(SOUND_MIXER_CD,	 	   0,  0,  0,  0,	   0,  0,  0,  0),
MIXENT(SOUND_MIXER_IMIX,	   0,  0,  0,  0,	   0,  0,  0,  0),
MIXENT(SOUND_MIXER_ALTPCM,	   0,  0,  0,  0,	   0,  0,  0,  0),
MIXENT(SOUND_MIXER_RECLEV,	   0,  0,  0,  0,	   0,  0,  0,  0),
MIXENT(SOUND_MIXER_IGAIN,	   0,  0,  0,  0,	   0,  0,  0,  0),
MIXENT(SOUND_MIXER_OGAIN,	   0,  0,  0,  0,	   0,  0,  0,  0),
MIXENT(SOUND_MIXER_LINE1, 	   0,  0,  0,  0,	   0,  0,  0,  0),
MIXENT(SOUND_MIXER_LINE2,	   0,  0,  0,  0,	   0,  0,  0,  0),
MIXENT(SOUND_MIXER_LINE3,	   0,  0,  0,  0,	   0,  0,  0,  0),
};

static unsigned short default_mixer_levels[SOUND_MIXER_NRDEVICES] =
{
	/* L R */
	0xffff,		/* Master Volume */	/* Support */
	0x0000,		/* Bass */		/* Support */
	0x0000,		/* Treble */		/* Support */
	0x0000,		/* FM */
	0x0000,		/* PCM */
	0x0000,		/* PC Speaker */
	0x0000,		/* Ext Line */		/* Support? */
	0x0000,		/* Mic */		/* Support? */
	0x0000,		/* CD */
	0x0000,		/* Recording monitor */
	0x0000,		/* SB PCM */
	0x0000,		/* Recording level */
	0x0000,		/* Input gain */
	0x0000,		/* Output gain */
	0x0000,		/* Line1 */
	0x0000,		/* Line2 */
	0x0000		/* Line3 (usually line in)*/
};

#define MIXER_DEVICES  (SOUND_MASK_VOLUME | \
			SOUND_MASK_BASS | SOUND_MASK_TREBLE | \
			SOUND_MASK_LINE | SOUND_MASK_MIC)
#define REC_DEVICES    (SOUND_MASK_LINE | SOUND_MASK_MIC)

static int uda1342_set_recmask(uda1342_t *devc, int mask)
{
	unsigned char   recdev;
	unsigned long	val;
	int             i, n;
	
	DBG(printk("%s: devc=%p, mask=0x%08x\n", __func__, devc, mask));

	mask &= devc->supported_rec_devices;
	
	n = 0;
	/* Count selected device bits */
	for (i = 0; i < 32; i++)
		if (mask & (1 << i))
			n++;
	if (n == 0)
		mask = SOUND_MASK_MIC;
	else if (n != 1) { /* Too many devices selected */
		/* Filter out active settings */
		mask &= ~devc->recmask;	
		
		n = 0;
		/* Count selected device bits */
		for (i = 0; i < 32; i++) 
			if (mask & (1 << i))
				n++;
		
		if (n != 1)
			mask = SOUND_MASK_MIC;
	}
	
	switch (mask) {
	case SOUND_MASK_MIC:
		recdev = 0x20;
		break;
		
	case SOUND_MASK_LINE:
		recdev = 0x21;
		break;
	default:
		mask = SOUND_MASK_MIC;
		recdev = 0x20;
	}
	val = snd_sh_i2c_read(UDA1342_CH_1, recdev);
	val &= 0xf0ff;	/* clear IA0-4 or IB0-4 */
	snd_sh_i2c_write(UDA1342_CH_1, recdev, val);
	devc->recmask = mask;
	DBG(printk("%s: devc->recmask=0x%08x\n", __func__, mask));
	return mask;
}

static void change_bits(unsigned long *regval, int dev, int chn, unsigned long newval)
{
	unsigned char   mask;
	int             shift;
  
	DBG(printk("%s: dev=%d, chn=%d, newval=%d\n",__func__,dev,chn,newval));

	if (mix_devices[dev][chn].polarity == 1) 
		newval = 100 - newval;

	mask = (1 << mix_devices[dev][chn].nbits) - 1;
	shift = mix_devices[dev][chn].bitpos;
	/* Scale it */
	newval = (int)((newval * mask) / 100);
	/* Clear bits */
	*regval &= ~(mask << shift);	
	/* Set new value */
	*regval |= (newval & mask) << shift;	
	DBG(printk("%s: *regval = 0x%08x\n", __func__, (unsigned int)*regval));
}

static int uda1342_mixer_get(uda1342_t *devc, int dev)
{
	DBG(printk("%s: dev=%d\n", __func__, dev));
	
	/* range check + supported mixer check */
	if (dev < 0 || dev >= SOUND_MIXER_NRDEVICES )
	        return (-(EINVAL));
	if (!((1 << dev) & devc->supported_devices))
		return -(EINVAL);
	DBG(printk("%s: levels[%d] = %d\n", __func__, dev, devc->levels[dev]));
	return devc->levels[dev];
}

static int uda1342_mixer_set(uda1342_t *devc, int dev, int value)
{
	unsigned long left = (unsigned long)((value & 0x0000ff00) >> 8);
	unsigned long right = (unsigned long)(value & 0x000000ff);
	unsigned long regno;
	unsigned long val1, val2;
	unsigned int retval;

	DBG(printk("%s: dev=%d, value=%x\n", __func__, dev, value));

	if (dev < 0 || dev >= SOUND_MIXER_NRDEVICES )
		return -(EINVAL);

	if (left > 100) left = 100;
	if (right > 100) right = 100;
	
	/* Mono control */
	if (mix_devices[dev][RIGHT_CHN].nbits == 0) 
		right = left;

	retval = ((left << 8) | right);

#if 0
	left = mix_cvt[left];
	right = mix_cvt[right];
#endif

	/* reject all mixers that are not supported */
	if (!(devc->supported_devices & (1 << dev)))
		return -EINVAL;
	
	/* sanity check */
	if (mix_devices[dev][LEFT_CHN].nbits == 0)
		return -EINVAL;

	/* keep precise volume internal */
	devc->levels[dev] = retval;

	/* Set the left channel */
	regno = mix_devices[dev][LEFT_CHN].regno;
	val1 = snd_sh_i2c_read(UDA1342_CH_1, regno);
	change_bits (&val1, dev, LEFT_CHN, left);
#if 0	/* FIXME: can not write correctly, so we use init_codec() */
	snd_sh_i2c_write(UDA1342_CH_1, regno, val1);
#endif

	/* Was just a mono channel */
	if (mix_devices[dev][RIGHT_CHN].nbits == 0) {
		if (dev == 1) bass = ((val1 >> 10) & 0xf);
		if (dev == 2) treble = ((val1 >> 8) & 3);
		init_codec(0);
		return (int)retval;		
	}

	/*  Set the right channel */
	regno = mix_devices[dev][RIGHT_CHN].regno;
	val2 = snd_sh_i2c_read(UDA1342_CH_1, regno);
	change_bits (&val2, dev, RIGHT_CHN, right);
#if 0	/* FIXME: can not write correctly, so we use init_codec() */
	snd_sh_i2c_write(UDA1342_CH_1, regno, val2);
#endif
	/* master_volume in this case */
	if (dev == 0) master_volume = ((val1 & 0xff00) | (val2 & 0xff));
	init_codec(0);
       	return (int)retval;
}

static void uda1342_mixer_update(uda1342_t *devc)
{
	int  i;

	for (i = 0; i < SOUND_MIXER_NRDEVICES; i++)
		if (devc->supported_devices & (1 << i))
			uda1342_mixer_set (devc, i, default_mixer_levels[i]);
}

static void uda1342_mixer_reset(uda1342_t *devc)
{
	int  i;

	DBG(printk("%s: \n", __func__));

	devc->supported_devices = MIXER_DEVICES;
	devc->supported_rec_devices = REC_DEVICES;
	for (i = 0; i < SOUND_MIXER_NRDEVICES; i++)
		if (devc->supported_devices & (1 << i))
			uda1342_mixer_set (devc, i, default_mixer_levels[i]);
	uda1342_set_recmask (devc, SOUND_MASK_MIC);
}

static void uda1342_mixer_dac_mute(int on_off)
{
	if (mta_mute == on_off) return;
	mta_mute = on_off;
	init_codec(0);
}

static void uda1342_mixer_adc_mute(int on_off)
{
	if (mta_mute == on_off) return;
	mtb_mute = on_off;
	init_codec(0);
}

static int uda1342_mixer_ioctl(int dev, unsigned int cmd, caddr_t arg)
{
	uda1342_t *devc = mixer_devs[dev]->devc;
	int val;
  
	// check request for mixer or not
	if (((cmd >> 8) & 0xff) != 'M')
		return -(EINVAL);	/* not request for mixer */
		
	// check direction
	if (_SIOC_DIR (cmd) & _SIOC_WRITE) { 
		DBG(printk("%s: _SIOC_WRITE\n",__func__));
		switch (cmd & 0xff) {
		case SOUND_MIXER_RECSRC:
			DBG(printk("%s: SOUND_MIXER_RECSRC\n", __func__));
			if (get_user(val, (int *)arg))
				return -EFAULT;
			val = uda1342_set_recmask(devc, val);
			return put_user(val, (int *)arg);
			break;
		default:
			DBG(printk("%s: default\n", __func__));
			if (get_user(val, (int *)arg))
				return -EFAULT;
			val = uda1342_mixer_set(devc, cmd & 0xff, val);
			if (verbose) print_uda1342();
			if (val < 0)
			        return val;
			else
			        return put_user(val, (int *)arg);
		}
	} else { 
		DBG(printk("%s: _SIOC_READ\n", __func__));
		/* read ioctl */
		switch (cmd & 0xff) {
		case SOUND_MIXER_RECSRC:
			DBG(printk("%s: SOUND_MIXER_RECSRC\n", __func__));
			val = devc->recmask;
			return put_user(val, (int *)arg);
			break;
		case SOUND_MIXER_DEVMASK:
			DBG(printk("%s: SOUND_MIXER_DEVMASK\n", __func__));
			val = devc->supported_devices;
			return put_user(val, (int *)arg);
			break;
		case SOUND_MIXER_RECMASK:
			DBG(printk("%s: SOUND_MIXER_RECMASK\n", __func__));
			val = devc->supported_rec_devices;
			return put_user(val, (int *)arg);
			break;
		case SOUND_MIXER_CAPS:
			DBG(printk("%s: SOUND_MIXER_CAPS\n", __func__));
			val = SOUND_CAP_EXCL_INPUT;
			return put_user(val, (int *)arg);
			break;
		case SOUND_MIXER_STEREODEVS:
			DBG(printk("%s: SOUND_MIXER_STEREODEVS\n", __func__));
			val = devc->supported_devices &
				~(SOUND_MASK_SPEAKER | SOUND_MASK_IMIX);
			return put_user(val, (int *)arg);
			break;
		case SOUND_MIXER_OUTSRC:
			DBG(printk("%s: SOUND_MIXER_OUTSRC\n", __func__));
			val = devc->supported_devices &
				~(SOUND_MASK_SPEAKER | SOUND_MASK_IMIX);
			return put_user(val, (int *)arg);
			break;
		default:
			DBG(printk("%s: default\n", __func__));
		        if ((val=uda1342_mixer_get(devc, cmd & 0xff))<0)
			        return val;
			else
			        return put_user(val, (int *)arg);
		}
	}
	/* NOT REACHED */
}

static struct mixer_operations uda1342_mixer_operations = {
	owner:	THIS_MODULE,
	id:	"UDA1342",
	name:	"UDA1342 Mixer",
	ioctl:	uda1342_mixer_ioctl
};

static int __init probe_uda1342(struct address_info *card)
{
	uda1342_t *devc;
	int io_base;
	int *osp;

	printk(KERN_INFO "uda1342: UDA1342 Sound Driver %s (%s %s)\n",
				VERSION, __DATE__,  __TIME__ );

	devc = &dev_info[nr_uda1342_devs];
	io_base = card->io_base;
	osp = card->osp;
	if (nr_uda1342_devs >= MAX_AUDIO_DEV) {
		printk(KERN_WARNING "uda1342: detect error\n");
		goto out_release_region;
	}
	devc->base = io_base;
	devc->irq_ok = 0;
	devc->irq = 0;
	devc->opened = 0;
	devc->osp = osp;
	printk("uda1342: detected\n");
	return 1; 

out_release_region:
	return 0;
}

static void print_uda1342(void)
{
	unsigned long reg, val;

	if (! verbose) return;
	reg = 0x00;	// system register
	val = snd_sh_i2c_read(UDA1342_CH_1, reg);
	printk("uda1342: system register = 0x%04lx\n", val);
	reg = 0x01;
	val = snd_sh_i2c_read(UDA1342_CH_1, reg);
	printk("uda1342: subsystem register = 0x%04lx\n", val);
	reg = 0x10;
	val = snd_sh_i2c_read(UDA1342_CH_1, reg);
	printk("uda1342: DAC features = 0x%04lx\n", val);
	reg = 0x11;
	val = snd_sh_i2c_read(UDA1342_CH_1, reg);
	printk("uda1342: DAC master volume = 0x%04lx\n", val);
	reg = 0x12;
	val = snd_sh_i2c_read(UDA1342_CH_1, reg);
	printk("uda1342: DAC mixer volume = 0x%04lx\n", val);
	reg = 0x20;
	val = snd_sh_i2c_read(UDA1342_CH_1, reg);
	printk("uda1342: ADC input and mixer gain channel 1 = 0x%04lx\n", val);
	reg = 0x21;
	val = snd_sh_i2c_read(UDA1342_CH_1, reg);
	printk("uda1342: ADC input and mixer gain channel 2 = 0x%04lx\n", val);
#if 0
	reg = 0x30;
	val = snd_sh_i2c_read(UDA1342_CH_1, reg);
	printk("uda1342: evaluation register = 0x%04lx\n", val);
#endif
}

static void init_codec(int reset)
{
	return;
}

static void init_codec2(int reset)
{
	/*
 	 * UDA1342
	 * 	setup sound system mode
	 *	RST=0, QS=0, MDC=0, DC=1, AM=000, PAD=0,
	 *	0, SC01=10, IF=000, DP=1, PDA=0
	 *	=00010000 01000010
	 */
	snd_sh_i2c_write(UDA1342_CH_1, 0x00, 0x0000 | reset << 15);
	snd_sh_i2c_write(UDA1342_CH_1, 0x00, 0x1042);	// disable ADC

	/* subsystem
	 * OS=00, MPS=0, MIX=0, SD=00, MP=00
	 */
	snd_sh_i2c_write(UDA1342_CH_1, 0x01, 0x0000);

	/* DAC features
	 * M=00, BB=0000, TR=00
	 * SDS=1, MTB=0, MTA=0, MT=0, QM=1, DE=000
	 */
	snd_sh_i2c_write(UDA1342_CH_1, 0x10,
		0x0088 | ((bass & 0xf) << 10) | ((treble & 3) << 8) |
		((mtb_mute & 1) << 6) | ((mta_mute & 1) << 5) |
		((master_mute & 1) << 4));

	/* DAC master volume */
	snd_sh_i2c_write(UDA1342_CH_1, 0x11, master_volume);
	snd_sh_i2c_write(UDA1342_CH_1, 0x12, 0x0000);
	snd_sh_i2c_write(UDA1342_CH_1, 0x20, 0x0080);
	snd_sh_i2c_write(UDA1342_CH_1, 0x21, 0x0080);
}

static void init_codec_rec(int reset)
{
	snd_sh_i2c_write(UDA1342_CH_1, 0x00, 0x0000 | reset << 15);
	snd_sh_i2c_write(UDA1342_CH_1, 0x00, 0x0240);
	snd_sh_i2c_write(UDA1342_CH_1, 0x01, 0x0000);
	snd_sh_i2c_write(UDA1342_CH_1, 0x20, 0x0000);
	snd_sh_i2c_write(UDA1342_CH_1, 0x21, 0x0000);
}

static void __init init_hw(void)
{
	unsigned char val;

	/*
	 * I2C & UDA1342
	 */
	snd_sh_i2c_init(); 	// initialize I2C hardware resource
	init_codec(1);		// inizialize UDA1342

	/*
	 * SSI (Serial Sound Interface)
	 */
	ctrl_outl(SSICR0_DEFAULT | SSICR_DMEN | SSICR_EN, SSICR0);
	if (0) ctrl_outl(SSICR1_DEFAULT | SSICR_DMEN | SSICR_EN, SSICR1); 

	/*
	 * Speaker volume (H8)
	 */
	val = 100;	/* max value */
	h8_out(&val, 1, SH7760SE_VOL_EVRDR);	// right speaker volume
	h8_out(&val, 1, SH7760SE_VOL_EVLDR);	// left speaker volume
}


static void __init attach_uda1342(struct address_info *card)
{
	int devid;
	char dev_name[100];
	uda1342_t *devc = &dev_info[nr_uda1342_devs];

	DBG(printk("%s:\n", __func__));

	devc->base = card->io_base;	

	/* all data transfer interrupt (DMABRGCI1) */
	if (request_irq(card->irq, uda1342_interrupt, 0,
			"SoundPort", card->osp) < 0) {
	        printk(KERN_WARNING "uda1342: IRQ in use\n");
		goto out_release_region;
	}
	/* halt data transfer interrupt (DMABRGCI1) */
	if (request_irq(card->irq + 1, uda1342_interrupt, 0,
			"SoundPort", card->osp) < 0) {
	        printk(KERN_WARNING "uda1342: IRQ in use\n");
		goto out_release_region;
	}
	devc->irq = card->irq;

	/* DMA stuff */
#if DMA_STUFF_WORKAROUND
	if (sound_alloc_dma (card->dma, "Sound System")) {
		printk(KERN_WARNING "uda1342: Can't allocate DMA%d\n",
				    card->dma);
		goto out_free_irq;
	}
#endif
	devc->dma_playback = card->dma;

	sprintf (dev_name,"UDA1342 audio driver");
  
	conf_printf2 (dev_name,
		      devc->base, devc->irq, card->dma, card->dma2);

	devid = sound_install_audiodrv (
			AUDIO_DRIVER_VERSION,
			dev_name,
		    	&uda1342_audio_driver,
			sizeof (struct audio_driver),
		      	devc->audio_mode,
		      	ad_format_mask,
		      	devc,
		      	card->dma, 
		      	card->dma2);
	if (devid < 0) {
		printk("uda1342: Can't install sound driver\n");
		goto out_free_dma_2;
	}

	printk("uda1342: installed audio driver as devid = %d\n", devid);
	devc->dev_no = devid;
	devc->opened = 0;
	devc->irq_ok = 0;
	devc->osp = card->osp;  
	nr_uda1342_devs++;

	uda1342_mixer_reset (devc); 	/* reset mixer device */
	audio_devs[devid]->mixer_dev = sound_install_mixer(
				       	MIXER_DRIVER_VERSION,
				       	dev_name,
				       	&uda1342_mixer_operations,
				       	sizeof (struct mixer_operations),
				       	devc);
	if (audio_devs[devid]->mixer_dev >= 0)
		audio_devs[devid]->min_fragment = 0;

	print_uda1342();

out:	return;

out_free_dma_2:
	if (devc->dma_capture >= 0)
	        sound_free_dma(card->dma2);
#if DMA_STUFF_WORKAROUND
out_free_dma:
#endif
	if (card->dma >= 0)
		sound_free_dma(card->dma);
#if DMA_STUFF_WORKAROUND
out_free_irq:
#endif
	free_irq(card->irq, card->osp);
	free_irq(card->irq + 1, card->osp);
out_release_region:
	//release_region(card->io_base, 16);
	goto out;
}

static void __exit unload_card(uda1342_t *devc)
{
	int  mixer, dev = 0;
	
	DBG(printk("%s: \n", __func__));

	if (devc != NULL) {
		printk("uda1342: Unloading card\n");
		dev = devc->dev_no;
		mixer = audio_devs[dev]->mixer_dev;
		if(mixer >= 0)
			sound_unload_mixerdev(mixer);
		sound_unload_audiodev(dev);
		
		if (devc->dma_capture >= 0)
			sound_free_dma(devc->dma_capture);

		/* card wont get added if resources could not be allocated
		   thus we need not ckeck if allocation was successful */
		if (devc->dma_playback >=0)
			sound_free_dma (devc->dma_playback);
		free_irq(devc->irq, devc->osp);
		free_irq(devc->irq + 1, devc->osp);
		printk("uda1342: Unloading card was successful\n");
	} else
		printk(KERN_WARNING "uda1342: no device/card specified\n");
}

static struct address_info uda1342_card;

static int __initdata irq = DMABRGI1;	// DMABRG interrupt
#if DMA_STUFF_WORKAROUND
static int __initdata dma = 0;
#else
static int __initdata dma = -1;
#endif
static int __initdata dma2 = -1;

MODULE_PARM(verbose,"i");
MODULE_PARM(irq,"i");
MODULE_PARM(dma,"i");
MODULE_PARM(dma2,"i");
MODULE_LICENSE("GPL");

static int __init init_uda1342(void)
{
	DBG(printk("%s: \n", __func__));

	uda1342_card.io_base	= SSICR0;
	uda1342_card.irq	= irq;
	uda1342_card.dma	= dma;
	uda1342_card.dma2	= dma2;
	if (probe_uda1342(&uda1342_card) == 0)
		return -ENODEV;
	init_hw();
	attach_uda1342(&uda1342_card);
	return 0;
}

static void __exit cleanup_uda1342(void)
{
	int          i;
	uda1342_t  *devc = NULL;
  
	/* remove any soundcard */
	for (i = 0;  i < nr_uda1342_devs; i++) {
		devc = &dev_info[i];
		unload_card(devc);
	}     
	nr_uda1342_devs=0;
}

module_init(init_uda1342);
module_exit(cleanup_uda1342);
