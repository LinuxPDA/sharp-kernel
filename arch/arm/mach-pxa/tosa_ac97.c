/*
 * linux/asm/arch/mach-pxa/tosa_ac97.c
 *
 * AC97 interface for the Tosa chip
 *
 * (C) Copyright 2004 Lineo Solutions, Inc.
 *
 * Based on:
 * 	linux/drivers/sound/pxa-ac97.c -- AC97 interface for the Cotula chip
 * 	Author:     Nicolas Pitre
 * 	Created:    Aug 15, 2001
 * 	Copyright:  MontaVista Software Inc.
 *
 * 	linux/drivers/sound/ac97_codec.c -- Generic AC97 mixer/modem module
 * 	Derived from ac97 mixer in maestro and trident driver.
 * 	Copyright 2000 Silicon Integrated System Corporation
 *
 * ChangeLong:
 *      1-Nov-2003 Sharp Corporation   for Tosa
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/bitops.h>
#include <linux/slab.h>
#include <linux/pci.h>
#include <linux/completion.h>
#include <linux/delay.h>
#include <linux/poll.h>
#include <linux/sound.h>
#include <linux/soundcard.h>
#include <linux/ac97_codec.h>

#include <asm/hardware.h>
#include <asm/irq.h>
#include <asm/uaccess.h>
#include <asm/semaphore.h>
#include <asm/dma.h>

#include <asm/arch/tosa_wm9712.h>

#ifdef CONFIG_PM
#include <asm/sharp_apm.h>
#endif

#define CODEC_ID_BUFSZ 14

typedef struct {
  unsigned short ctl_val, pdown_val;
} power_mode_t;

static power_mode_t pwr_values[] = {
  { 0x6F00, 0x7FFF },   // Off
  { 0x4600, 0x73FF },	// Rec with no HP
  { 0x4100, 0x1F77 },	// Play with no HP
  { 0x4600, 0x73FF },	// Rec with Stereo HP
  { 0x0100, 0x1CEF },	// Play with Stereo HP
  { 0x4600, 0x73FF },	// Rec with Mic HP
  { 0x0100, 0x1EEF },	// Play with Mic HP
  { 0x4700, 0x7FFF },	// Tablet (waiting for pen-down)
  { 0x4700, 0x7FFF },	// Tablet (continuous conversion)
  { 0x4700, 0x7BFF },	// Enable MICBIAS for detecting Mic HP
  { 0x0000, 0x0000 },	// Full power
};
static const int num_of_pwr_values = sizeof(pwr_values)/sizeof(pwr_values[0]);

static power_mode_t power_mode_status[NUM_OF_WM9712_DEV];
static power_mode_t cur_power_status;

struct ac97_codec *codec;

static struct completion CAR_completion;
static DECLARE_MUTEX(CAR_mutex);

/************************************************************
 * Debug
 ************************************************************/
#define DBG_LEVEL       0
#define DBG_L1          1
#define DBG_L2          2
#define DBG_L3          3
#define DEBUG(level, x, args...) \
	if ( level <= DBG_LEVEL ) printk("%s: " x,__FUNCTION__ ,##args)

/************************************************************
 * AC97 Sequense
 ************************************************************/
typedef struct {
  u16		val;
  u8		seq;
} ac97_seq_t;
static volatile u32 *ac97_addr = NULL;
static ac97_seq_t ac97_seq;

#define AC97_SEQ_READ1		0x01
#define AC97_SEQ_READ2		0x02
#define AC97_SEQ_READ_DONE	0x03
#define AC97_SEQ_WRITE_DONE	0x04

/************************************************************
 * AC97 IDs
 ************************************************************/
typedef struct {
  u32			id;
  char			*name;
  struct ac97_ops	*ops;
  int			flags;
} ac97_ids_t;

static struct ac97_ops null_ops = { NULL, NULL, NULL };

static  ac97_ids_t ac97_ids[] = {
  { 0x574D4C12, "Wolfson WM9711/WM9712", &null_ops },
};

/************************************************************
 * Timer interrupt for AC97 lost interrupt
 ************************************************************/
static unsigned char ac97_timer_on = 0;
static struct timer_list ac97_timer;
static void ac97_timer_set(unsigned long);
static void ac97_timer_clear(void);

static void ac97_timer_func(unsigned long data)
{
  ac97_timer_on = 0;
  if ( (ac97_seq.seq == AC97_SEQ_READ_DONE) ||
       (ac97_seq.seq == AC97_SEQ_WRITE_DONE) ) {
    DEBUG(DBG_L2, "CAR_completion\n");
    complete(&CAR_completion);
    ac97_timer_set(data);
  } else {
    printk(KERN_WARNING  "AC97: lost interrupt(%08lx)\n", data);
    ac97_timer_set(data);
  }
}

static void ac97_timer_clear(void)
{
  if ( ac97_timer_on )
    del_timer(&ac97_timer);
  ac97_timer_on = 0;
}

static void ac97_timer_set(unsigned long val)
{
  ac97_timer_clear();
  init_timer(&ac97_timer);
  ac97_timer.data = val;
  ac97_timer.function = ac97_timer_func;
  ac97_timer.expires = jiffies + HZ;
  add_timer(&ac97_timer);
  ac97_timer_on = 1;
}

/************************************************************
 * AC97 functions
 ************************************************************/
#define AC97_TIMEOUT_VAL	0x10000
static u16 pxa_ac97_read(struct ac97_codec *codec, u8 reg)
{
  down(&CAR_mutex);
  lock_FCS(LOCK_FCS_AC97_SUB, 1);
  if ( !(CAR & CAR_CAIP) ) {
    ac97_addr = (volatile u32 *)&PAC_REG_BASE + (reg >> 1);
#ifdef USE_AC97_INTERRUPT
      ac97_seq.val = -1;
      ac97_seq.seq = AC97_SEQ_READ1;

      init_completion(&CAR_completion);

      ac97_timer_set(ac97_seq.seq);
    
      (void)*ac97_addr;
      wait_for_completion(&CAR_completion);
    
      ac97_timer_clear();
#else
      {
	u16 data=0;
	int timeout;
	// dummy
	GSR |= GSR_RDCS;
	GSR |= GSR_SDONE;
	data = *ac97_addr;
	timeout = 0;
	while((GSR & GSR_SDONE)==0 && timeout++<AC97_TIMEOUT_VAL);
	if ((GSR & GSR_RDCS)!=0 || timeout>=AC97_TIMEOUT_VAL) {
	  GSR |= GSR_RDCS;
	  printk(KERN_CRIT __FUNCTION__": AC97 is busy.\n");
	  lock_FCS(LOCK_FCS_AC97_SUB, 0);
	  up(&CAR_mutex);
	  return data;
	}

	// actual read
	GSR |= GSR_SDONE;
	data = *ac97_addr;
	timeout = 0;
	while((GSR & GSR_SDONE)==0 && timeout++<AC97_TIMEOUT_VAL);
	if ((GSR & GSR_RDCS)!=0 || timeout>=AC97_TIMEOUT_VAL) {
	  GSR |= GSR_RDCS;
	  printk(KERN_CRIT __FUNCTION__": AC97 is busy.\n");
	}
	lock_FCS(LOCK_FCS_AC97_SUB, 0);
	up(&CAR_mutex);
	return data;
      }
#endif // end USE_AC97_INTERRUPT
  } else {
    printk(KERN_CRIT __FUNCTION__": CAR_CAIP already set\n");
  }
  lock_FCS(LOCK_FCS_AC97_SUB, 0);
  up(&CAR_mutex);
  return ac97_seq.val;
}

static void pxa_ac97_write(struct ac97_codec *codec, u8 reg, u16 val)
{
  down(&CAR_mutex);
  lock_FCS(LOCK_FCS_AC97_SUB, 1);
  if ( !(CAR & CAR_CAIP) ) {
    ac97_addr = (volatile u32 *)&PAC_REG_BASE + (reg >> 1);
#ifdef USE_AC97_INTERRUPT
      ac97_seq.val = val;
      ac97_seq.seq = AC97_SEQ_WRITE_DONE;
      init_completion(&CAR_completion);

      ac97_timer_set(ac97_seq.seq);
      
      *ac97_addr = ac97_seq.val;
      wait_for_completion(&CAR_completion);

      ac97_timer_clear();
#else
      {
	int timeout=0;
	GSR |= GSR_CDONE;
	*ac97_addr = val;
	while((GSR & GSR_CDONE)==0 && timeout++<AC97_TIMEOUT_VAL);
	if (timeout>=AC97_TIMEOUT_VAL) {
	  printk(KERN_CRIT __FUNCTION__": AC97 is busy.\n");
	}
      }
#endif // end USE_AC97_INTERRUPT
  } else {
    printk(KERN_CRIT __FUNCTION__": CAR_CAIP already set\n");
  }
  lock_FCS(LOCK_FCS_AC97_SUB, 0);
  up(&CAR_mutex);
}

static void pxa_ac97_bit_clear(struct ac97_codec *codec, u8 reg, u16 val)
{
  unsigned short dat = codec->codec_read(codec, reg);
  dat &= ~val;
  codec->codec_write(codec, reg, dat);
}

static void pxa_ac97_bit_set(struct ac97_codec *codec, u8 reg, u16 val)
{
  unsigned short dat = codec->codec_read(codec, reg);
  dat |= val;
  codec->codec_write(codec, reg, dat);
}

static void pxa_ac97_irq(int irq, void *dev_id, struct pt_regs *regs)
{
  if (GSR & (GSR_SDONE|GSR_CDONE)) {
    GSR = GSR_SDONE|GSR_CDONE;

    ac97_timer_set(ac97_seq.seq);

    switch ( ac97_seq.seq ) {
    case AC97_SEQ_READ1:
      ac97_seq.seq = AC97_SEQ_READ2;
      (void)*ac97_addr;
      break;
    case AC97_SEQ_READ2:
      ac97_seq.seq = AC97_SEQ_READ_DONE;
      if ( GSR & GSR_RDCS ) {
	GSR |= GSR_RDCS;
	printk(KERN_CRIT __FUNCTION__": read codec register timeout.\n");
      }
      ac97_seq.val = *ac97_addr;
      break;
    case AC97_SEQ_READ_DONE:
      udelay(20);
      complete(&CAR_completion);
      break;
    case AC97_SEQ_WRITE_DONE:
      complete(&CAR_completion);
      break;
    }
  }
}

static struct ac97_codec pxa_ac97_codec = {
  codec_read:		pxa_ac97_read,
  codec_write:		pxa_ac97_write,
  codec_bit_clear:	pxa_ac97_bit_clear,
  codec_bit_set:	pxa_ac97_bit_set,
};

static DECLARE_MUTEX(pxa_ac97_mutex);
static int pxa_ac97_refcount = 0;

static char *codec_id(u16 id1, u16 id2, char *buf)
{
  if(id1&0x8080) {
    snprintf(buf, CODEC_ID_BUFSZ, "0x%04x:0x%04x", id1, id2);
  } else {
    buf[0] = (id1 >> 8);
    buf[1] = (id1 & 0xFF);
    buf[2] = (id2 >> 8);
    snprintf(buf+3, CODEC_ID_BUFSZ - 3, "%d", id2&0xFF);
  }
  return buf;
}

static int ac97_check_modem(struct ac97_codec *codec)
{
  /* Check for an AC97 1.0 soft modem (ID1) */
  if(codec->codec_read(codec, AC97_RESET) & 2)
    return 1;
  /* Check for an AC97 2.x soft modem */
  codec->codec_write(codec, AC97_EXTENDED_MODEM_ID, 0L);
  if(codec->codec_read(codec, AC97_EXTENDED_MODEM_ID) & 1)
    return 1;
  return 0;
}

static int ac97_probe(struct ac97_codec *codec)
{
  u16 id1, id2;
  u16 audio;
  int i;
  char cidbuf[CODEC_ID_BUFSZ];

  codec->codec_write(codec, AC97_RESET, 0L);

  /* also according to spec, we wait for codec-ready state */
  if ( codec->codec_wait ) codec->codec_wait(codec);
  else udelay(10);

  if ( (audio = codec->codec_read(codec, AC97_RESET)) & 0x8000 ) {
    printk(KERN_ERR "ac97_codec: %s ac97 codec not present\n",
	(codec->id & 0x2) ? (codec->id&1 ? "4th" : "Tertiary")
	: (codec->id&1 ? "Secondary":  "Primary"));
    return 0;
  }

  /* probe for Modem Codec */
  codec->modem = ac97_check_modem(codec);
  codec->name = NULL;
  codec->codec_ops = &null_ops;

  id1 = codec->codec_read(codec, AC97_VENDOR_ID1);
  id2 = codec->codec_read(codec, AC97_VENDOR_ID2);
  for (i = 0; i < ARRAY_SIZE(ac97_ids); i++) {
    if (ac97_ids[i].id == ((id1 << 16) | id2)) {
      codec->type = ac97_ids[i].id;
      codec->name = ac97_ids[i].name;
      codec->codec_ops = ac97_ids[i].ops;
      codec->flags = ac97_ids[i].flags;
      break;
    }
  }

  /* A device which thinks its a modem but isnt */
  if ( codec->flags & AC97_DELUDED_MODEM )
    codec->modem = 0;
  
  if ( codec->name == NULL )
    codec->name = "Unknown";
  printk(KERN_INFO "ac97_codec: AC97 %s codec, id: %s (%s)\n",
	codec->modem ? "Modem" : (audio ? "Audio" : ""),
	codec_id(id1, id2, cidbuf), codec->name);
  return 1;
}

int pxa_ac97_get(struct ac97_codec **codec, unsigned char *ac97_on)
{
  if ( *ac97_on != 0 ) return 0;
  
  *codec = NULL;
  down(&pxa_ac97_mutex);

  DEBUG(DBG_L1, "count(%d)\n", pxa_ac97_refcount);
  pxa_ac97_refcount++;
  up(&pxa_ac97_mutex);
  *codec = &pxa_ac97_codec;
  *ac97_on = 1;

  return 0;
}

void pxa_ac97_put(unsigned char *ac97_on)
{
  if ( *ac97_on == 0 ) return;
  down(&pxa_ac97_mutex);
  pxa_ac97_refcount--;
  DEBUG(DBG_L1, "count(%d)\n", pxa_ac97_refcount);
  up(&pxa_ac97_mutex);
  *ac97_on = 0;
}

unsigned int ac97_set_dac_rate(struct ac97_codec *codec, unsigned int rate)
{
  unsigned int new_rate = rate;
  u32 dacp;
  u32 mast_vol, phone_vol, mono_vol, pcm_vol;
  u32 mute_vol = 0x8000;  /* The mute volume? */
  
  if( rate != codec->codec_read(codec, AC97_PCM_FRONT_DAC_RATE) ) {
    /* Mute several registers */
    mast_vol = codec->codec_read(codec, AC97_MASTER_VOL_STEREO);
    mono_vol = codec->codec_read(codec, AC97_MASTER_VOL_MONO);
    phone_vol = codec->codec_read(codec, AC97_HEADPHONE_VOL);
    pcm_vol = codec->codec_read(codec, AC97_PCMOUT_VOL);
    codec->codec_write(codec, AC97_MASTER_VOL_STEREO, mute_vol);
    codec->codec_write(codec, AC97_MASTER_VOL_MONO, mute_vol);
    codec->codec_write(codec, AC97_HEADPHONE_VOL, mute_vol);
    codec->codec_write(codec, AC97_PCMOUT_VOL, mute_vol);
    
    /* Power down the DAC */
    dacp=codec->codec_read(codec, AC97_POWER_CONTROL);
    codec->codec_write(codec, AC97_POWER_CONTROL, dacp|0x0200);
    /* Load the rate and read the effective rate */
    codec->codec_write(codec, AC97_PCM_FRONT_DAC_RATE, rate);
    new_rate=codec->codec_read(codec, AC97_PCM_FRONT_DAC_RATE);
    /* Power it back up */
    codec->codec_write(codec, AC97_POWER_CONTROL, dacp);
    
    /* Restore volumes */
    codec->codec_write(codec, AC97_MASTER_VOL_STEREO, mast_vol);
    codec->codec_write(codec, AC97_MASTER_VOL_MONO, mono_vol);
    codec->codec_write(codec, AC97_HEADPHONE_VOL, phone_vol);
    codec->codec_write(codec, AC97_PCMOUT_VOL, pcm_vol);
  }
  return new_rate;
}

unsigned int ac97_set_adc_rate(struct ac97_codec *codec, unsigned int rate)
{
  unsigned int new_rate = rate;
  u32 dacp;
  if( rate != codec->codec_read(codec, AC97_PCM_LR_ADC_RATE) ) {
    /* Power "Up" the ADC */
    dacp=codec->codec_read(codec, AC97_POWER_CONTROL);
    dacp &= ~(0x1<<8);
    codec->codec_write(codec, AC97_POWER_CONTROL, dacp);
    
    codec->codec_write(codec, AC97_PCM_LR_ADC_RATE, rate);
    new_rate=codec->codec_read(codec, AC97_PCM_LR_ADC_RATE);

    /* Power it back up */
    codec->codec_write(codec, AC97_POWER_CONTROL, dacp);
  }
  return new_rate;
}

void wm9712_power_mode(int dev, int mode)
{
  int i;
  power_mode_t new_power_status;
  dev %= NUM_OF_WM9712_DEV;
  mode %= num_of_pwr_values;
  //  printk("wm9712_power_mode(%d,%d)\n",dev,mode);
  power_mode_status[dev] = pwr_values[mode];
  // create new state
  new_power_status = power_mode_status[0];
  for (i=1; i<NUM_OF_WM9712_DEV; i++) {
    new_power_status.ctl_val &= power_mode_status[i].ctl_val;
    new_power_status.pdown_val &= power_mode_status[i].pdown_val;
  }
  // compare with current state
  if (new_power_status.pdown_val != cur_power_status.pdown_val) {
    ac97_write(AC97_POWERDOWN, new_power_status.pdown_val);
    cur_power_status.pdown_val = new_power_status.pdown_val;
    //    printk("change status PWDOWN = %04X\n", cur_power_status.pdown_val);
  }
  if (new_power_status.ctl_val != cur_power_status.ctl_val) {
    ac97_write(AC97_POWER_CONTROL, new_power_status.ctl_val);
    cur_power_status.ctl_val = new_power_status.ctl_val;
    //    printk("change status PW_CTRL = %04X\n", cur_power_status.ctl_val);
  }

}

void tosa_ac97_init(void)
{
  int i;
  int ret,timeo;

  lock_FCS(LOCK_FCS_AC97, 1);
  codec = NULL;
  pxa_ac97_refcount = 0;
  set_GPIO_mode(GPIO31_SYNC_AC97_MD);
  set_GPIO_mode(GPIO30_SDATA_OUT_AC97_MD);
  set_GPIO_mode(GPIO28_BITCLK_AC97_MD);
  set_GPIO_mode(GPIO29_SDATA_IN_AC97_MD);
  set_GPIO_mode(GPIO20_DREQ0_MD);

  for (i=0; i<NUM_OF_WM9712_DEV; i++) {
    power_mode_status[i].ctl_val = pwr_values[WM9712_PWR_OFF].ctl_val;
    power_mode_status[i].pdown_val = pwr_values[WM9712_PWR_OFF].pdown_val;
  }
  memset(&cur_power_status,0,sizeof(cur_power_status));

#if 1 // move from ac97_get
    CKEN |= CKEN2_AC97;
    /* AC97 power on sequense */
    while ( 1 ) {
      GCR = 0;
      udelay(100);
      GCR |= GCR_COLD_RST;
      udelay(5);
      GCR |= GCR_WARM_RST;
      udelay(5);
      for ( timeo = 0x10000; timeo > 0; timeo-- ) {
        if ( GSR & GSR_PCR ) break;
        //X schedule();
	mdelay(5);
      }
      if( timeo > 0 ) break;
      printk(KERN_WARNING "AC97 power on retry!!\n");
    }
      
#ifdef USE_AC97_INTERRUPT
      if( (ret = request_irq(IRQ_AC97, pxa_ac97_irq, 0, "AC97", NULL)) ) {
	lock_FCS(LOCK_FCS_AC97, 0);
	return ret;
      }
    }
#endif

    if ( (ret = ac97_probe(&pxa_ac97_codec)) != 1 ) {
#ifdef USE_AC97_INTERRUPT
      free_irq(IRQ_AC97, NULL);
#endif
      GCR = GCR_ACLINK_OFF;
      CKEN &= ~CKEN2_AC97;
      lock_FCS(LOCK_FCS_AC97, 0);
      return ret;
    }
    pxa_ac97_write(&pxa_ac97_codec, AC97_EXTENDED_STATUS, 1);
    /*
     * Setting AC97 GPIO
     *	i/o     function
     *	GPIO1:   input   EAR_IN signal
     *	GPIO2:   output  IRQ signal
     *	GPIO3:   output  PENDOWN signal
     *	GPIO4:   input   MASK signal
     *	GPIO5:   input   DETECT MIC signal
     */
    pxa_ac97_bit_clear(&pxa_ac97_codec, AC97_GPIO_FUNC,
	    ((1<<2)|(1<<3)|(1<<4)|(1<<5)));
    pxa_ac97_bit_clear(&pxa_ac97_codec, AC97_GPIO_CONFIG,(1<<2)|(1<<3));
    pxa_ac97_bit_set(&pxa_ac97_codec, AC97_GPIO_CONFIG, (1<<1)|(1<<4)|(1<<5));
#endif
    codec = &pxa_ac97_codec;
    lock_FCS(LOCK_FCS_AC97, 0);
}

void tosa_ac97_exit(void)
{
#if 1 // move from ac97_put
    GCR = GCR_ACLINK_OFF;
    CKEN &= ~CKEN2_AC97;
#ifdef USE_AC97_INTERRUPT
    free_irq(IRQ_AC97, NULL);
#endif
    pxa_ac97_refcount = 0;
#endif
}

EXPORT_SYMBOL(ac97_set_dac_rate);
EXPORT_SYMBOL(ac97_set_adc_rate);
EXPORT_SYMBOL(pxa_ac97_get);
EXPORT_SYMBOL(pxa_ac97_put);
EXPORT_SYMBOL(wm9712_power_mode);
