/*
 * SA11x0 + UCB1x00 Touch Screen Driver Version 0.2
 * Put together by Tak-Shing Chan <tchan.rd@idthk.com>
 *
 * 90% derived from the Touch screen driver for Tifon
 * Copyright 1999 Peter Danielsson
 *
 * Codec routines derived from Itsy's Touchscreen driver
 * Copyright (c) Compaq Computer Corporation, 1998, 1999
 * Author: Larry Brakmo.
 *
 * Sample filtering derived from Diagman (ucb1x00Touch.c)
 * Copyright (C) 1999 Intel Corp.
 *
 * IOCTL's and jitter elimination derived from ADS Touchscreen driver
 * Copyright (c) 2000 Century Software, Inc.
 *
 * iPAQ emulation derived from Driver for the h3600 Touch Screen
 * Copyright 2000 Compaq Computer Corporation.
 * Author: Charles Flynn.
 *
 * Add Some function to let it more flexible
 * Allen_cheng@xlinux.com
 * chester@linux.org.tw
 *
 * Add Freebird UCB1300 Touch-Panel/Button driver support
 * Eric Peng <ercipeng@coventive.com>
 * Tony Liu < tonyliu@coventive.com>
 *
 * Added support for ADCx (0-3) inputs on UCB1200
 * Brad Parker <brad@heeltoe.com>
 *
 * Added support for Flexanet machine.
 * On sa1100_ts_init() error, resouces are freed.
 * Jordi Colomer <jco@ict.es>
 *
 * Add proc fs for pen pressure threshold on SHARP SL5000D
 * 09-08-2001 SHARP
 *
 * Enable ring buffer on SHARP SL5000D
 * 09-20-2001 SHARP
 * 
 * Add noize reduction for SHARP SL5000D
 * 10-01-2001 SHARP
 *
 * Todo:
 * support other button driver controlled by UCB-1300
 *
 */

/*
 * INSTRUCTIONS
 * To use this driver, simply ``mknod /dev/ts c 11 0'' and
 * use it as if it were an iPAQ.  TODO: iPAQ-compatible IOCTL's.
 * 
 *
 */

#include <linux/config.h>
#include <linux/types.h>
#include <linux/module.h>
#include <linux/sched.h>
#include <linux/kernel.h>
#include <linux/proc_fs.h>
#include <linux/init.h>
#include <linux/fs.h>
#include <linux/delay.h>
#include <linux/fs.h>
#include <linux/poll.h>
#include <linux/string.h>

#include <asm/uaccess.h>
#include <asm/hardware.h>
#include <asm/irq.h>
#include <asm/ucb1200.h>
#include <asm/ts.h>
#if defined(CONFIG_SA1100_COLLIE)
#include <asm/arch/tc35143.h>
#endif

#include "ucb1200_ts.h"

#ifdef CONFIG_PM
static struct pm_dev* ucb1200_ts_pm_dev;
#endif

#ifdef CONFIG_PM
#include <linux/pm.h>
static struct pm_dev* ucb1200_ts_pm_dev;
#endif

/*
 * UCB1200 register 9: Touchscreen control register
 */
#define TSMX_POW (1 << 0)
#define TSPX_POW (1 << 1)
#define TSMY_POW (1 << 2)
#define TSPY_POW (1 << 3)
#define TSMX_GND (1 << 4)
#define TSPX_GND (1 << 5)
#define TSMY_GND (1 << 6)
#define TSPY_GND (1 << 7)
#define TSC_MODE_MASK (3 << 8)
#define TSC_MODE_INT (0 << 8)
#define TSC_MODE_PRESSURE (1 << 8)
#define TSC_MODE_POSITION (1 << 9)
#define TSC_BIAS_ENA (1 << 11)
#define TSPX_LOW (1 << 12)
#define TSMX_LOW (1 << 13)


#if defined(CONFIG_SA1100_COLLIE)
extern int idleCancel;
typedef struct {
	long x;
	long y;
  	long pressure;
	long long millisecs;
} TS_EVENT;
#define	PRESSURE_THRESHOLD	350
static int	threshold = PRESSURE_THRESHOLD;

static int	noFlDim = 1;
#define		flDim		(! noFlDim)
#else
/* From Compaq's Touch Screen Specification version 0.2 (draft) */
typedef struct {
    short pressure;
    short x;
    short y;
    short millisecs;
} TS_EVENT;
#endif

static int raw_max_x, raw_max_y, res_x, res_y, raw_min_x, raw_min_y, xyswap;

static int cal_ok, x_rev, y_rev;
static int smooth;

static char *dev_id = "ucb1200-ts";

static DECLARE_WAIT_QUEUE_HEAD(queue);
static struct timer_list timer;

/* state machine states for touch screen */
#if defined(CONFIG_SA1100_COLLIE) && !defined(CONFIG_COLLIE_TS) && !defined(CONFIG_COLLIE_TR0)
enum {
	PRESSED = 0,
	P_ADC_CHARGE,
	P_DONE,
	Y_ADC_CHARGE,
	Y_DONE,
	X_ADC_CHARGE,
	X_DONE,
	P_ADC_CHARGE2,
	P_DONE2,
	RELEASED
};
#else	/* (! CONFIG_SA1100_COLLIE) || CONFIG_COLLIE_TS || CONFIG_COLLIE_TR0 */
#define PRESSED  0
#define P_DONE   1
#define X_DONE   2
#define Y_DONE   3
#define RELEASED 4
#endif	/* end (! CONFIG_SA1100_COLLIE) || CONFIG_COLLIE_TS || CONFIG_COLLIE_TR0 */

/* state machine states for adc */
#define ADCX_IDLE   0
#define ADCX_SAMPLE 1

/* who owns the adc h/w (touch screen or adcx) */
#define ADC_OWNER_TS   1
#define ADC_OWNER_ADCX 2
static spinlock_t owner_lock = SPIN_LOCK_UNLOCKED;

#define BUFSIZE 128
#define XLIMIT 160
#define YLIMIT 160
static volatile int ts_state, adcx_state, adc_owner;
static int head, tail, sample;
#if CONFIG_SA1100_COLLIE
#define TS_SAMPLES	4
#define	TS_SAMPLES_MASK	(TS_SAMPLES - 1)
static TS_EVENT cur_data, samples[TS_SAMPLES], buf[BUFSIZE];
#define TS_OFFSET	0x10000
static int x_slope;
static int y_slope;
static int x_off;
static int y_off;
#else
static TS_EVENT cur_data, samples[3], buf[BUFSIZE];
#endif
static struct fasync_struct *fasync;
static unsigned long in_timehandle = 0;
#if defined(CONFIG_SA1100_COLLIE)
static volatile int	timehandling = 0;
#endif
static int adcx_channel, adcx_data[4];
static void ucb1200_ts_interrupt(int, void *, struct pt_regs *);
/* Allen Add */
static void ts_clear(void);
static void print_par(void);


extern void ucb1200_stop_adc(void);
extern void ucb1200_start_adc(u16 input);
extern u16 ucb1200_read_adc(void);


static inline void set_read_x_pos(void)
{
#if defined(CONFIG_SA1100_COLLIE)
	ucb1200_stop_adc();
	ucb1200_set_io(TC35143_GPIO_TBL_CHK, TC35143_IODAT_LOW);
	ucb1200_write_reg(UCB1200_REG_TS_CTL,
			  TSPX_POW | TSMX_GND |
			  TSC_MODE_POSITION | TSC_BIAS_ENA);
	ucb1200_start_adc(ADC_INPUT_TSPY);
#else
	/* See Philips' AN809 for an explanation of the pressure mode switch */
	ucb1200_write_reg(UCB1200_REG_TS_CTL,
			  TSPX_POW | TSMX_GND |
			  TSC_MODE_PRESSURE | TSC_BIAS_ENA);

	/* generate a SIB frame */
	ucb1200_stop_adc();

	ucb1200_write_reg(UCB1200_REG_TS_CTL,
			  TSPX_POW | TSMX_GND |
			  TSC_MODE_POSITION | TSC_BIAS_ENA);

	ucb1200_start_adc(ADC_INPUT_TSPY);
#endif
}

static inline void set_read_y_pos(void)
{
#if defined(CONFIG_SA1100_COLLIE)
	ucb1200_stop_adc();
	ucb1200_set_io(TC35143_GPIO_TBL_CHK, TC35143_IODAT_LOW);
	ucb1200_write_reg(UCB1200_REG_TS_CTL,
			  TSPY_POW | TSMY_GND |
			  TSC_MODE_POSITION | TSC_BIAS_ENA);
	ucb1200_start_adc(ADC_INPUT_TSPX);
#else
	ucb1200_write_reg(UCB1200_REG_TS_CTL,
			  TSPY_POW | TSMY_GND |
			  TSC_MODE_PRESSURE | TSC_BIAS_ENA);

	/* generate a SIB frame */
	ucb1200_stop_adc();

	ucb1200_write_reg(UCB1200_REG_TS_CTL,
			  TSPY_POW | TSMY_GND |
			  TSC_MODE_POSITION | TSC_BIAS_ENA);

	ucb1200_start_adc(ADC_INPUT_TSPX);
#endif
}

static inline void set_read_pressure(void)
{
#if defined(CONFIG_SA1100_COLLIE)
	ucb1200_set_io(TC35143_GPIO_TBL_CHK, TC35143_IODAT_HIGH);
	ucb1200_write_reg(UCB1200_REG_TS_CTL,
			  TSPX_POW | TSMX_POW |
			  TSC_MODE_POSITION | TSC_BIAS_ENA);
	ucb1200_start_adc(ADC_INPUT_AD2);
#else
	ucb1200_write_reg(UCB1200_REG_TS_CTL,
			  TSPX_POW | TSMX_POW | TSPY_GND | TSMY_GND |
			  TSC_MODE_PRESSURE | TSC_BIAS_ENA);

	ucb1200_start_adc(ADC_INPUT_TSPX);
#endif
}

static int ucb1200_ts_ioctl(struct inode *inode, struct file *filp, unsigned int cmd, unsigned long arg)
{
  /* int ret_data = 0; */
/* Microwindows style (should change to TS_CAL when the specification is ready) */
   switch (cmd)
   {
      case 3:
	 raw_max_x = arg;
	 break;
      case 4:
	 raw_max_y = arg;
	 break;
      case 5:
	 res_x = arg;
	 break;
      case 6:
	 res_y = arg;
	 break;
      case 10:
	 raw_min_x = arg;
	 break;
      case 11:
	 raw_min_y = arg;
	 break;
      case 12:
/* New attribute for portrait modes */
	 xyswap = arg;
/* Allen Add */
      case 13:	       /* 0 = Enable calibration ; 1 = Calibration OK */
	 cal_ok = arg;
      case 14:	       /* Clear all buffer data */
	 ts_clear();
	 break;
      case 15:	       /* X axis reversed setting */
	 x_rev = arg;
	 break;
      case 16:	       /* Y axis reversed setting */
	 y_rev = arg;
	 break;
      case 17:	       /* Clear all buffer data */
	 print_par();
	 break;
/* Allen */
      case TS_IOCGRESX:
	 return res_x;
      case TS_IOCGRESY:
	 return res_y;
      case TS_IOCSSMOOTH:
	 smooth = arg;
	 break;
      default:
	 return -EINVAL;
   }

   return 0;
}


static void ts_clear(void)
{
   int i;

   for (i=0; i < BUFSIZE; i++)
   {
       buf[i].pressure=0;
       buf[i].x=0;
       buf[i].y=0;
       buf[i].millisecs=0;
   }

   head = 0;
   tail = 0;

}

static void print_par(void)
{
   printk(" Kernel ==> cal_ok = %d\n",cal_ok);
   printk(" Kernel ==> raw_max_x = %d\n",raw_max_x);
   printk(" Kernel ==> raw_max_y = %d\n",raw_max_y);
   printk(" Kernel ==> res_x = %d\n",res_x);
   printk(" Kernel ==> res_y = %d\n",res_y);
   printk(" Kernel ==> raw_min_x = %d\n",raw_min_x);
   printk(" Kernel ==> raw_min_y = %d\n",raw_min_y);
   printk(" Kernel ==> xyswap = %d\n",xyswap);
   printk(" Kernel ==> x_rev = %d\n",x_rev);
   printk(" Kernel ==> y_rev = %d\n",y_rev);
   printk(" Kernel ==> smooth = %d\n",smooth);
}

/* Allen */

static inline int pen_up(void)
{
#if defined(CONFIG_SA1100_COLLIE)
	ucb1200_stop_adc();
	ucb1200_write_reg(UCB1200_REG_TS_CTL,
			  TSPX_POW | TSMX_POW | TSPY_GND | TSMY_GND);
#if 0	/* remove delay */
	udelay(1000);
#endif
	return !(ucb1200_read_reg(UCB1200_REG_TS_CTL) & TSPX_LOW);
#else
	ucb1200_stop_adc();
	ucb1200_write_reg(UCB1200_REG_TS_CTL,
			  TSPX_POW | TSMX_POW | TSPY_GND | TSMY_GND);
	return ucb1200_read_reg(UCB1200_REG_TS_CTL) & TSPX_LOW;
#endif
}

static void new_data(void)
{
   static TS_EVENT last_data = { 0, 0, 0, 0 };
   int diff0, diff1, diff2, diff3;

   if (!smooth) {
   } else
   if (cur_data.pressure)
   {
#ifdef CONFIG_SA1100_COLLIE
     samples[sample & TS_SAMPLES_MASK].x = cur_data.x;
     samples[sample & TS_SAMPLES_MASK].y = cur_data.y;
     sample++;
      if (sample <= TS_SAMPLES)
      {
	 return;
      }
      cur_data.x = samples[(sample-3) & TS_SAMPLES_MASK].x;
      cur_data.y = samples[(sample-3) & TS_SAMPLES_MASK].y;

#else	/* !CONFIG_SA1100_COLLIE */
      if (sample < 3)
      {
	 samples[sample].x = cur_data.x;
	 samples[sample++].y = cur_data.y;
	 return;
      }
      sample = 0;

/* Check the variance between X samples (discard if not similar), then choose the closest pair */
      diff0 = abs(samples[0].x - samples[1].x);
      diff1 = abs(samples[1].x - samples[2].x);
      diff2 = abs(samples[2].x - cur_data.x);
      diff3 = abs(cur_data.x - samples[1].x);

      if (diff0 > XLIMIT || diff1 > XLIMIT || diff2 > XLIMIT || diff3 > XLIMIT)
	 return;

      if (diff1 < diff2)
      {
	 if (diff1 < diff3)
	    cur_data.x = (samples[1].x + samples[2].x) / 2;
	 else
	    cur_data.x = (cur_data.x + samples[1].x) / 2;
      }
      else
      {
	 if (diff2 < diff3)
	    cur_data.x = (samples[2].x + cur_data.x) / 2;
	 else
	    cur_data.x = (cur_data.x + samples[1].x) / 2;
      }

/* Do the same for Y */
      diff0 = abs(samples[0].y - samples[1].y);
      diff1 = abs(samples[1].y - samples[2].y);
      diff2 = abs(samples[2].y - cur_data.y);
      diff3 = abs(cur_data.y - samples[1].y);

      if (diff0 > YLIMIT || diff1 > YLIMIT || diff2 > YLIMIT || diff3 > YLIMIT)
	 return;

      if (diff1 < diff2)
      {
	 if (diff1 < diff3)
	    cur_data.y = (samples[1].y + samples[2].y) / 2;
	 else
	    cur_data.y = (cur_data.y + samples[1].y) / 2;
      }
      else
      {
	 if (diff2 < diff3)
	    cur_data.y = (samples[2].y + cur_data.y) / 2;
	 else
	    cur_data.y = (cur_data.y + samples[1].y) / 2;
      }
#endif	/* end !CONFIG_SA1100_COLLIE */
   }
   else
   {
#ifdef CONFIG_SA1100_COLLIE
      if (sample <= TS_SAMPLES)
      {
	 return;
      }
      cur_data.x = samples[(sample-2) & TS_SAMPLES_MASK].x;
      cur_data.y = samples[(sample-2) & TS_SAMPLES_MASK].y;
#endif
/* Reset jitter detection on pen release */
      last_data.x = 0;
      last_data.y = 0;
   }

/* Jitter elimination */
/* Allen Mask */
//   if ((last_data.x || last_data.y) && abs(last_data.x - cur_data.x) <= 3 && abs(last_data.y - cur_data.y) <= 3)
//	return;
/* Allen */

   cur_data.millisecs = jiffies;
   last_data = cur_data;

   buf[head] = cur_data;

   if (++head >= BUFSIZE)
      head = 0;
   if (head == tail && ++tail >= BUFSIZE)
      tail = 0;

   if (fasync)
      kill_fasync(&fasync, SIGIO, POLL_IN);
//X   printk("new head = %d, tail = %d\n",head,tail);
   wake_up_interruptible(&queue);
}

static TS_EVENT get_data(void)
{
   int last = tail;

   if (++tail == BUFSIZE)
      tail = 0;
//X   printk("get head = %d, tail = %d\n",head,tail);
   return buf[last];
}

static void adcx_take_ownership(void);

static void wait_for_action(void)
{
	adc_owner = ADC_OWNER_TS;
	ts_state = PRESSED;
	sample = 0;

#if !defined(CONFIG_SA1100_COLLIE)
	ucb1200_disable_irq(IRQ_UCB1200_ADC);
#endif

#if defined(CONFIG_SA1100_COLLIE)
	ucb1200_set_irq_edge(TSPX_INT, GPIO_RISING_EDGE);
#else
	ucb1200_set_irq_edge(TSPX_INT, GPIO_FALLING_EDGE);
#endif
	ucb1200_enable_irq(IRQ_UCB1200_TSPX);

	ucb1200_stop_adc();
#ifdef CONFIG_SA1100_COLLIE
	ucb1200_unlock_adc(ucb1200_ts_interrupt);
#endif
	ucb1200_write_reg(UCB1200_REG_TS_CTL,
			  TSPX_POW | TSMX_POW | TSPY_GND | TSMY_GND |
			  TSC_MODE_INT);

	/* if adc is waiting, start it */
	if (adcx_state == ADCX_SAMPLE) {
		adcx_take_ownership();
	}
#ifdef CONFIG_SA1100_COLLIE
	idleCancel = 0;
#endif
}

static void
ts_take_ownership(void)
{
	/* put back in ts int mode */
	ucb1200_write_reg(UCB1200_REG_TS_CTL,
			  TSPX_POW | TSMX_POW | TSPY_GND | TSMY_GND |
			  TSC_MODE_INT);

	ucb1200_enable_irq(IRQ_UCB1200_ADC);
	set_read_pressure();
}

static void start_chain(void)
{
	unsigned long flags;

#ifdef CONFIG_SA1100_COLLIE
#if defined(CONFIG_COLLIE_TS) || defined(CONFIG_COLLIE_TR0)
	colliefl_temporary_suspend();	/* front light off */
#endif

	if (ucb1200_lock_adc(ucb1200_ts_interrupt) < 0) {
		ucb1200_set_irq_edge(TSPX_INT, GPIO_FALLING_EDGE);
		ucb1200_enable_irq(IRQ_UCB1200_TSPX);
		ucb1200_write_reg(UCB1200_REG_TS_CTL,
		  TSPX_POW | TSMX_POW | TSPY_GND | TSMY_GND | TSC_MODE_INT);
		ts_state = RELEASED;
		ucb1200_ts_starttimer();
		return;
	}

#if !defined(CONFIG_COLLIE_TS) && !defined(CONFIG_COLLIE_TR0)
	ts_state = P_ADC_CHARGE;
#else	/* CONFIG_COLLIE_TS || CONFIG_COLLIE_TR0 */
	ts_state = P_DONE;
#endif

#else	/* !CONFIG_SA1100_COLLIE */
	ts_state = P_DONE;
#endif

	/* if adcx is idle, grab adc, else wait for ts state check at end */
	spin_lock_irqsave(&owner_lock, flags);
	if (adcx_state == ADCX_IDLE || adc_owner == ADC_OWNER_TS) {
		ts_take_ownership();
	}
	spin_unlock_irqrestore(&owner_lock, flags);
}

static unsigned int ucb1200_ts_poll(struct file *filp, poll_table *wait)
{
	poll_wait(filp, &queue, wait);
	if (head != tail)
		return POLLIN | POLLRDNORM;
	return 0;
}

static ssize_t ucb1200_ts_read(struct file *filp, char *buf, size_t count, loff_t *l)
{
   DECLARE_WAITQUEUE(wait, current);
   int i;
   TS_EVENT t;
#if defined(CONFIG_SA1100_COLLIE)
   TS_EVENT out;
#else
   short out_buf[4];
#endif

   if (head == tail)
   {
      if (filp->f_flags & O_NONBLOCK)
	 return -EAGAIN;
      add_wait_queue(&queue, &wait);
      current->state = TASK_INTERRUPTIBLE;
      while ((head == tail) && !signal_pending(current))
      {
	 schedule();
	 current->state = TASK_INTERRUPTIBLE;
      }
      current->state = TASK_RUNNING;
      remove_wait_queue(&queue, &wait);
   }

#if defined(CONFIG_SA1100_COLLIE)
   for (i = count; i >= sizeof(out); i -= sizeof(out), buf += sizeof(out))
#else
   for (i = count; i >= sizeof(out_buf); i -= sizeof(out_buf), buf += sizeof(out_buf))
#endif
   {

      if (head == tail)
	 break;
      t = get_data();

#if defined(CONFIG_SA1100_COLLIE)
      out.pressure = t.pressure;
#else
      out_buf[0] = t.pressure;
#endif

/* Alen Add */
#if 0

#ifdef CONFIG_SA1100_ASSABET
      if (xyswap)
      {
	 out_buf[1] = (((raw_max_y - t.y)) * res_y) / (raw_max_y - raw_min_y);
	 out_buf[2] = (((t.x - raw_min_x)) * res_x) / (raw_max_x - raw_min_x);
      }
      else
      {
	 out_buf[1] = (((raw_max_x - t.x)) * res_x) / (raw_max_x - raw_min_x);
	 out_buf[2] = (((raw_max_y - t.y)) * res_y) / (raw_max_y - raw_min_y);
      }
#else
      if (xyswap)
      {
	 out_buf[1] = (((t.y - raw_min_y)) * res_y) / (raw_max_y - raw_min_y);
	 out_buf[2] = (((t.x - raw_min_x)) * res_x) / (raw_max_x - raw_min_x);
      }
      else
      {
	 out_buf[1] = (((t.x - raw_min_x)) * res_x) / (raw_max_x - raw_min_x);
	 out_buf[2] = (((t.y - raw_min_y)) * res_y) / (raw_max_y - raw_min_y);
      }
#endif

#else
#if defined(CONFIG_SA1100_COLLIE)
      if (cal_ok)
      {
	 	out.x = (x_rev) ? ((raw_max_x - t.x) * res_x) / (raw_max_x - raw_min_x) :
		((t.x - raw_min_x) * res_x) / (raw_max_x - raw_min_x);
	 	out.y = (y_rev) ? ((raw_max_y - t.y) * res_y) / (raw_max_y - raw_min_y) :
		((t.y - raw_min_y) * res_y) / (raw_max_y - raw_min_y);
      }
      else
      {
	 	out.x = t.x;
	 	out.y = t.y;
      }

      {
	/* convert x, y */
	long	save_y;
	out.x = ( ( (((1023 - out.x) * x_slope) + x_off) / TS_OFFSET ) * 1024 ) / 240;
	out.y = ( ( (((1023 - out.y) * y_slope) + y_off) / TS_OFFSET ) * 1024 ) / 320;
	save_y = out.y;
	out.y = out.x;
	out.x = save_y;
      }

#else
      if (cal_ok)
      {
	 	out_buf[1] = (x_rev) ? ((raw_max_x - t.x) * res_x) / (raw_max_x - raw_min_x) :
		((t.x - raw_min_x) * res_x) / (raw_max_x - raw_min_x);
	 	out_buf[2] = (y_rev) ? ((raw_max_y - t.y) * res_y) / (raw_max_y - raw_min_y) :
		((t.y - raw_min_y) * res_y) / (raw_max_y - raw_min_y);
      }
      else
      {
	 	out_buf[1] = t.x;
	 	out_buf[2] = t.y;
      }
#endif
#endif
/* Allen */

#if defined(CONFIG_SA1100_COLLIE)
      out.millisecs = t.millisecs;
#else
      out_buf[3] = t.millisecs;
#endif

#if defined(CONFIG_SA1100_COLLIE)
      copy_to_user(buf, &out, sizeof(out));
#else
      copy_to_user(buf, &out_buf, sizeof(out_buf));
#endif
   }

   return count - i;
}

/* Forward declaration */
static void ucb1200_ts_timer(unsigned long);

static int ucb1200_ts_starttimer(void)
{
#ifdef CONFIG_SA1100_COLLIE
#if defined(CONFIG_COLLIE_TS) || defined(CONFIG_COLLIE_TR0)
	colliefl_temporary_resume();	/* front light on */
#endif
	ucb1200_unlock_adc(ucb1200_ts_interrupt);
#endif
	in_timehandle++;
	init_timer(&timer);
	timer.function = ucb1200_ts_timer;
	timer.expires = jiffies + HZ / 100;
	add_timer(&timer);
	return 0;
}

static void ucb1200_ts_timer(unsigned long data)
{
#if defined(CONFIG_SA1100_COLLIE)
	timehandling = 1;
#endif
	in_timehandle--;
	if (pen_up()) {
#if defined(CONFIG_SA1100_COLLIE) && !defined(CONFIG_COLLIE_TS) && !defined(CONFIG_COLLIE_TR0)
		if (flDim) {
			colliefl_temporary_contrast_reset();
		}
#endif
	  	cur_data.pressure = 0;
		new_data();
		wait_for_action();
	}
	else {
	  	start_chain();
	}
#if defined(CONFIG_SA1100_COLLIE)
	timehandling = 0;
#endif
}

static int ucb1200_ts_fasync(int fd, struct file *filp, int on)
{
	int retval;

	retval = fasync_helper(fd, filp, on, &fasync);
	if (retval < 0)
		return retval;
	return 0;
}

static int ucb1200_ts_open(struct inode *inode, struct file *filp)
{
/* Allen Add */
	ts_clear();
/* Allen */

	MOD_INC_USE_COUNT;
	return 0;
}

static int ucb1200_ts_release(struct inode *inode, struct file *filp)
{
/* Allen Add */
	ts_clear();
/* Allen */

	ucb1200_ts_fasync(-1, filp, 0);
	MOD_DEC_USE_COUNT;
	return 0;
}

#if defined(CONFIG_SA1100_COLLIE)
#define	LCM_ALC_EN	0x8000
static inline void fl_enable(void)
{
	if (flDim) {
		if (sample) { /* ignore pen down position */
			LCM_ALS |= LCM_ALC_EN;
		}
	}
}
static inline void fl_disable(void)
{
	if (flDim) {
		if (sample) { /* ignore pen down position */
			LCM_ALS &= ~LCM_ALC_EN;
		}
	}
}
#endif

static void ucb1200_ts_interrupt(int irq, void *dev_id, struct pt_regs *regs)
{

#if defined(CONFIG_SA1100_COLLIE)
  long	pressure2;
  extern int autoPowerCancel;
  extern int autoLightCancel;
#endif


	if (irq == IRQ_UCB1200_TSPX) {
		if ((ts_state != PRESSED)&&(ts_state != RELEASED))
			return;
#if defined(CONFIG_SA1100_COLLIE)
	} else
	if (irq == IRQ_UCB1200_ADC) {
		if (ucb1200_get_adc_id() != ucb1200_ts_interrupt)
			return;
#endif
	}

	if (in_timehandle > 0)
		return;
#if defined(CONFIG_SA1100_COLLIE)
	if (timehandling && irq != IRQ_UCB1200_ADC) {
		return;
	}
#endif

#if defined(CONFIG_SA1100_COLLIE)
	autoPowerCancel = 0;
	autoLightCancel = 0;
#endif

#if defined(CONFIG_SA1100_COLLIE) && !defined(CONFIG_COLLIE_TS) && !defined(CONFIG_COLLIE_TR0)
	switch (ts_state) {
	case PRESSED:
		idleCancel = 1;
		if (flDim) {
			colliefl_temporary_contrast_set();
		}
		start_chain();
		break;
	case P_ADC_CHARGE:
		ucb1200_disable_irq(IRQ_UCB1200_TSPX);

		ts_state++;
		ucb1200_start_adc(ADC_INPUT_AD2);
		break;
	case P_DONE:
		cur_data.pressure = ucb1200_read_adc();
#if 0	/* redundant irq enable */
		ucb1200_enable_irq(IRQ_UCB1200_ADC);
#endif
		set_read_y_pos();
		ts_state++;
		fl_disable();
		break;
	case Y_ADC_CHARGE:
		ts_state++;
		ucb1200_start_adc(ADC_INPUT_TSPX);
		break;
	case Y_DONE:
		cur_data.y = ucb1200_read_adc();
#if 0	/* redundant irq enable */
		ucb1200_enable_irq(IRQ_UCB1200_ADC);
#endif
		set_read_x_pos();
		ts_state++;
		break;
	case X_ADC_CHARGE:
		ts_state++;
		ucb1200_start_adc(ADC_INPUT_TSPY);
		break;
	case X_DONE:
	        fl_enable();
		cur_data.x = ucb1200_read_adc();
#if 0	/* redundant irq enable */
		ucb1200_enable_irq(IRQ_UCB1200_ADC);
#endif
		set_read_pressure();
		ts_state++;
		break;
	case P_ADC_CHARGE2:
		ts_state++;
		ucb1200_start_adc(ADC_INPUT_AD2);
		break;
	case P_DONE2:
		pressure2 = ucb1200_read_adc();

		ucb1200_set_irq_edge(TSPX_INT, GPIO_FALLING_EDGE);
		ucb1200_enable_irq(IRQ_UCB1200_TSPX);
		ucb1200_stop_adc();
		ucb1200_write_reg(UCB1200_REG_TS_CTL,
		  TSPX_POW | TSMX_POW | TSPY_GND | TSMY_GND | TSC_MODE_INT);
		ts_state++;
		if (threshold < pressure2 && threshold < cur_data.pressure) {
		  new_data();
		}
		ucb1200_ts_starttimer();
		break;
	case RELEASED:
		if (flDim) {
			colliefl_temporary_contrast_reset();
		}
		cur_data.pressure = 0;
		new_data();
		wait_for_action();
	}
#else	/* !CONFIG_SA1100_COLLIE || CONFIG_COLLIE_TS || CONFIG_COLLIE_TR0 */
	switch (ts_state) {
	case PRESSED:
		start_chain();
		break;
	case P_DONE:
		ucb1200_disable_irq(IRQ_UCB1200_TSPX);

		cur_data.pressure = ucb1200_read_adc();
		ucb1200_enable_irq(IRQ_UCB1200_ADC);
		set_read_x_pos();
		ts_state++;
		break;
	case X_DONE:
		cur_data.x = ucb1200_read_adc();
		ucb1200_enable_irq(IRQ_UCB1200_ADC);
		set_read_y_pos();
		ts_state++;
		break;
	case Y_DONE:
		cur_data.y = ucb1200_read_adc();
#if defined(CONFIG_SA1100_COLLIE)
		ucb1200_set_irq_edge(TSPX_INT, GPIO_FALLING_EDGE);
		ucb1200_enable_irq(IRQ_UCB1200_TSPX);
#else
		ucb1200_set_irq_edge(TSPX_INT, GPIO_RISING_EDGE);
		ucb1200_enable_irq(IRQ_UCB1200_TSPX);
#endif
		ucb1200_stop_adc();
		ucb1200_write_reg(UCB1200_REG_TS_CTL,
		  TSPX_POW | TSMX_POW | TSPY_GND | TSMY_GND | TSC_MODE_INT);
		ts_state++;
		new_data();
		ucb1200_ts_starttimer();
		break;
	case RELEASED:
#ifdef CONFIG_SA1100_COLLIE
		colliefl_temporary_resume();	/* flont light on */
#endif
		cur_data.pressure = 0;
		new_data();
		wait_for_action();
	}
#endif	/* end !CONFIG_SA1100_COLLIE || CONFIG_COLLIE_TS || CONFIG_COLLIE_TR0 */
}

static void ucb1200_adcx_interrupt(int irq, void *dev_id, struct pt_regs *regs)
{
	adcx_data[adcx_channel] = ucb1200_read_adc();
	adcx_state = ADCX_IDLE;
	adc_owner = ADC_OWNER_TS;

	ucb1200_stop_adc();

	/* if ts is waiting, start it */
	if (ts_state == P_DONE) {
		ts_take_ownership();
	}
}

static void ucb1200_adc_interrupt(int irq, void *dev_id, struct pt_regs *regs)
{
	//printk("ucb1200_adc_interrupt() adc_owner %d\n", adc_owner);

	switch (adc_owner) {
	case ADC_OWNER_TS:
		ucb1200_ts_interrupt(irq, dev_id, regs);
		break;
	case ADC_OWNER_ADCX:
		ucb1200_adcx_interrupt(irq, dev_id, regs);
		break;
	}
}

static void
adcx_take_ownership(void)
{
	u16 inp = 0;

	adc_owner = ADC_OWNER_ADCX;

	/* take out of ts int mode */
	ucb1200_write_reg(UCB1200_REG_TS_CTL,
			  TSPX_POW | TSMX_POW | TSPY_GND | TSMY_GND);

	ucb1200_enable_irq(IRQ_UCB1200_ADC);

	switch (adcx_channel) {
	case 0: inp = ADC_INPUT_AD0; break;
	case 1: inp = ADC_INPUT_AD1; break;
	case 2: inp = ADC_INPUT_AD2; break;
	case 3: inp = ADC_INPUT_AD3; break;
	}

	ucb1200_start_adc(inp);
}

int ucb1200_adc_start(int channel)
{
	unsigned long flags;

#if 0
	printk("ucb1200_adc_start(channel=%d) adcx_state %d, adc_owner %d\n",
	       channel, adcx_state, adc_owner);
#endif

	if (adcx_state != ADCX_IDLE || adc_owner != ADC_OWNER_TS)
		return -EINVAL;

	adcx_state = ADCX_SAMPLE;
	adcx_channel = channel;

	/* if ts is idle, grab adc, else wait for adc state check at end */
	spin_lock_irqsave(&owner_lock, flags);
	if (ts_state == PRESSED) {
		adcx_take_ownership();
	}
	spin_unlock_irqrestore(&owner_lock, flags);

	return 0;
}

int ucb1200_adc_done(void)
{
	return adcx_state == ADCX_IDLE ? 1 : 0;
}

int ucb1200_adc_value(int channel)
{
	return adcx_data[channel];
}

static struct file_operations ucb1200_ts_fops = {
	read:	ucb1200_ts_read,
	poll:	ucb1200_ts_poll,
	ioctl:	ucb1200_ts_ioctl,
	fasync:	ucb1200_ts_fasync,
	open:	ucb1200_ts_open,
	release:ucb1200_ts_release,
};

int sa1100_ts_init(void)
{
	smooth = 1;
#ifdef CONFIG_SA1100_ASSABET
	raw_max_x = 944;
	raw_max_y = 944;
	raw_min_x = 70;
	raw_min_y = 70;
	res_x = 320;
	res_y = 240;
#elif defined(CONFIG_SA1100_COLLIE)
	//smooth = 0;
	raw_max_x = 944;
	raw_max_y = 944;
	raw_min_x = 70;
	raw_min_y = 70;
	res_x = 320;
	res_y = 240;
#elif defined(CONFIG_SA1100_CERF)
	raw_max_x = 944;
	raw_max_y = 944;
	raw_min_x = 70;
	raw_min_y = 70;
#if defined(CONFIG_CERF_LCD_38_A)
	res_x = 240;
	res_y = 320;
#elif defined(CONFIG_CERF_LCD_57_A)
	res_x = 320;
	res_y = 240;
#elif defined(CONFIG_CERF_LCD_72_A)
	res_x = 640;
	res_y = 480;
#else
#warning "Cannot enable the UCB1200 Touchscreen Driver without selecting a Cerfboard screen orientation first"
#error
#endif

#elif defined(CONFIG_SA1100_FREEBIRD)
	raw_max_x = 925;
	raw_max_y = 875;
	raw_min_x = 85;
	raw_min_y = 60;
	res_x = 240;
	res_y = 320;
#elif defined(CONFIG_SA1100_YOPY)
	raw_max_x = 964;
	raw_max_y = 958;
	raw_min_x = 45;
	raw_min_y = 53;
	res_x = 240;
	res_y = 320;
#elif defined(CONFIG_SA1100_PFS168)
	raw_max_x = 944;
	raw_max_y = 944;
	raw_min_x = 70;
	raw_min_y = 70;
	res_x = 320;
	res_y = 240;
#elif defined(CONFIG_SA1100_FLEXANET)
	switch (flexanet_GUI_type)
	{
	  /* set the touchscreen dimensions */
	  case FHH_GUI_TYPE_0:
	  
	    raw_max_x = 944;
	    raw_max_y = 944;
	    raw_min_x = 70;
	    raw_min_y = 70;
	    res_x = 320;
	    res_y = 240;
	    break;
	    
	  default:
	    return -ENODEV;
	}
#else
	raw_max_x = 885;
	raw_max_y = 885;
	raw_min_x = 70;
	raw_min_y = 70;
	res_x = 640;
	res_y = 480;
#endif

	xyswap = 0;
	head = 0;
	tail = 0;

	cal_ok = 0;
	x_rev = 1;
	y_rev = 1;

	init_waitqueue_head(&queue);

	/* Initialize the touchscreen controller */
	ucb1200_stop_adc();
	ucb1200_set_irq_edge(ADC_INT, GPIO_RISING_EDGE);
	wait_for_action();

	return 0;
}

#ifdef CONFIG_PM
static int ucb1200_ts_pm_callback(struct pm_dev *pm_dev,
				  pm_request_t req, void *data)
{
	switch (req) {
	case PM_SUSPEND:
		ucb1200_stop_adc();
		if (in_timehandle) {
			del_timer(&timer);
			in_timehandle = 0;
		}
	        ucb1200_disable_irq(IRQ_UCB1200_ADC);
		ucb1200_disable_irq(IRQ_UCB1200_TSPX);
		break;
	case PM_RESUME:
		head = 0;
		tail = 0;
		ucb1200_stop_adc();
		ucb1200_set_irq_edge(ADC_INT, GPIO_RISING_EDGE);
		wait_for_action();
                break;
	}
	return 0;
}
#endif

#ifdef CONFIG_SA1100_COLLIE
#ifdef CONFIG_PROC_FS
struct proc_dir_entry *proc_ts;

typedef struct ucb1200_ts_entry {
	int*		addr;
	int		def_value;
	char*		name;
	char*		description;
	unsigned short	low_ino;
} ucb1200_ts_entry_t;

static ucb1200_ts_entry_t ucb1200_ts_params[] = {
/*  { addr,	    def_value,		name,	    description }*/
  { &threshold,   PRESSURE_THRESHOLD,	"threshold","pen pressure threshold" },
  { &noFlDim,                      0,	  "noFlDim","frontlight dim prohibition" },
  { &ts_state,	                   0,	"ts_state","state of touch sampling"}
};
#define NUM_OF_UCB1200_TS_ENTRY	(sizeof(ucb1200_ts_params)/sizeof(ucb1200_ts_entry_t))

static ssize_t ucb1200_ts_read_params(struct file *file, char *buf,
				      size_t nbytes, loff_t *ppos)
{
	int i_ino = (file->f_dentry->d_inode)->i_ino;
	char outputbuf[15];
	int count;
	int i;
	ucb1200_ts_entry_t	*current_param = NULL;
	if (*ppos>0) /* Assume reading completed in previous read*/
		return 0;
	for (i=0; i<NUM_OF_UCB1200_TS_ENTRY; i++) {
		if (ucb1200_ts_params[i].low_ino==i_ino) {
			current_param = &ucb1200_ts_params[i];
			break;
		}
	}
	if (current_param==NULL) {
		return -EINVAL;
	}
	count = sprintf(outputbuf, "0x%08X\n",
			*((volatile Word *) current_param->addr));
	*ppos += count;
	if (count>nbytes)	/* Assume output can be read at one time */
		return -EINVAL;
	if (copy_to_user(buf, outputbuf, count))
		return -EFAULT;
	return count;
}

static ssize_t ucb1200_ts_write_params(struct file *file, const char *buf,
				       size_t nbytes, loff_t *ppos)
{
	int			i_ino = (file->f_dentry->d_inode)->i_ino;
	ucb1200_ts_entry_t	*current_param = NULL;
	int			i;
	unsigned long		param;
	char			*endp;

	for (i=0; i<NUM_OF_UCB1200_TS_ENTRY; i++) {
		if(ucb1200_ts_params[i].low_ino==i_ino) {
			current_param = &ucb1200_ts_params[i];
			break;
		}
	}
	if (current_param==NULL) {
		return -EINVAL;
	}

	param = simple_strtoul(buf,&endp,0);
	if (param == -1) {
		*current_param->addr = current_param->def_value;
	} else {
		*current_param->addr = param;
	}
	return nbytes+endp-buf;
}

static struct file_operations proc_params_operations = {
	read:	ucb1200_ts_read_params,
	write:	ucb1200_ts_write_params,
};
#endif
#endif


#ifdef CONFIG_SA1100_COLLIE
void collie_get_touch()
{
  if ( FLASH_DATA(FLASH_TOUCH_MAGIC_ADR) == FLASH_TOUCH_MAJIC ) {
   x_slope = FLASH_DATA(FLASH_TOUCH_XP_DATA_ADR);
   y_slope = FLASH_DATA(FLASH_TOUCH_YP_DATA_ADR);
   x_off   = FLASH_DATA(FLASH_TOUCH_XD_DATA_ADR);
   y_off   = FLASH_DATA(FLASH_TOUCH_YD_DATA_ADR);

   x_slope = float32_to_int32(float32_mul(x_slope,int32_to_float32(TS_OFFSET)));
   y_slope = float32_to_int32(float32_mul(y_slope,int32_to_float32(TS_OFFSET)));
   x_off   = x_off * TS_OFFSET;
   y_off   = y_off * TS_OFFSET;
  } else {
   x_slope = 18199;
   y_slope = 25264;
   x_off   = -23*TS_OFFSET;
   y_off   = -46*TS_OFFSET;
  }

  printk("touch adj= %d,%d,%d,%d\n",x_slope,y_slope,x_off,y_off);

}
#endif

int __init ucb1200_ts_init(void)
{
	int ret;

	register_chrdev(TS_MAJOR, TS_NAME, &ucb1200_ts_fops);

	if ((ret = ucb1200_request_irq(IRQ_UCB1200_ADC, ucb1200_adc_interrupt,
				       SA_SHIRQ, TS_NAME, dev_id)))
	{
		printk("ucb1200_ts_init: failed to register ADC IRQ\n");
		return ret;
	}
	if ((ret = ucb1200_request_irq(IRQ_UCB1200_TSPX, ucb1200_ts_interrupt,
				       SA_INTERRUPT, TS_NAME, dev_id)))
	{
		printk("ucb1200_ts_init: failed to register TSPX IRQ\n");
		ucb1200_free_irq(IRQ_UCB1200_ADC, dev_id);
		return ret;
	}

	if ((ret = sa1100_ts_init()) != 0)
	{
		ucb1200_free_irq(IRQ_UCB1200_TSPX, dev_id);
		ucb1200_free_irq(IRQ_UCB1200_ADC, dev_id);
		return ret;
	}

#ifdef CONFIG_PM
	ucb1200_ts_pm_dev = pm_register(PM_SYS_DEV, 0, ucb1200_ts_pm_callback);
#endif

#ifdef CONFIG_SA1100_COLLIE
	collie_get_touch();
#endif


#ifdef CONFIG_SA1100_COLLIE
#ifdef CONFIG_PROC_FS
	{
	int	i;
	struct proc_dir_entry *entry;

	proc_ts = proc_mkdir("driver/ts", NULL);
	if (proc_ts == NULL) {
		ucb1200_free_irq(IRQ_UCB1200_TSPX, dev_id);
		ucb1200_free_irq(IRQ_UCB1200_ADC, dev_id);
		printk(KERN_ERR "ts: can't create /proc/driver/ts\n");
		return -ENOMEM;
	}
	for (i=0; i<NUM_OF_UCB1200_TS_ENTRY; i++) {
		entry = create_proc_entry(ucb1200_ts_params[i].name,
					  S_IWUSR |S_IRUSR | S_IRGRP | S_IROTH,
					  proc_ts);
		if (entry) {
			ucb1200_ts_params[i].low_ino = entry->low_ino;
			entry->proc_fops = &proc_params_operations;
		} else {
			int	j;
			for (j=0; j<i; j++) {
				remove_proc_entry(ucb1200_ts_params[i].name,
						  proc_ts);
			}
			remove_proc_entry("driver/ts", &proc_root);
			proc_ts = 0;
			ucb1200_free_irq(IRQ_UCB1200_TSPX, dev_id);
			ucb1200_free_irq(IRQ_UCB1200_ADC, dev_id);
			printk(KERN_ERR "ts: can't create /proc/driver/ts/threshold\n");
			return -ENOMEM;
		}
	}
	}
#endif
#endif

	printk("ucb1200 touch screen driver initialized\n");

	return 0;
}

void __exit ucb1200_ts_cleanup(void)
{
	ucb1200_stop_adc();

	if (in_timehandle)
		del_timer(&timer);

	ucb1200_free_irq(IRQ_UCB1200_TSPX, dev_id);
	ucb1200_free_irq(IRQ_UCB1200_ADC, dev_id);

	unregister_chrdev(TS_MAJOR, TS_NAME);

#ifdef CONFIG_SA1100_COLLIE
#ifdef CONFIG_PROC_FS
	{
	int	i;
	for (i=0; i<NUM_OF_UCB1200_TS_ENTRY; i++) {
		remove_proc_entry(ucb1200_ts_params[i].name, proc_ts);
	}
	remove_proc_entry("driver/ts", NULL);
	proc_ts = 0;
	}
#endif
#endif

	printk("ucb1200 touch screen driver removed\n");
}

module_init(ucb1200_ts_init);
module_exit(ucb1200_ts_cleanup);
