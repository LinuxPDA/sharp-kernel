/*
 * linux/drivers/char/ads7846_ts.c
 *
 * Copyright (c) 2001 LINEO Japan, inc.
 *
 * May be copied or modified under the terms of the GNU General Public
 * License.  See linux/COPYING for more information.
 *
 * Based on:
 *   cotulla_ts.c
 *
 * Change Log
 *  01-Jul-2002 SHARP Corporation
 */

/***************************************************************************/

#include <linux/config.h>
#include <linux/init.h>
#include <linux/poll.h>
#include <linux/delay.h>
#include <linux/ioctl.h>

#include <asm/segment.h>
#include <asm/uaccess.h>
#include <asm/arch/cotulla.h>
#include <asm/arch/irqs.h>
#include <asm/arch/cotulla_ts.h>

#ifdef CONFIG_PM
#include <linux/pm.h>
#endif


#ifdef CONFIG_PM
static struct pm_dev *touch_pm_dev;
static int cotulla_touch_pm_callback(struct pm_dev* pm_dev,
			       pm_request_t req, void* data);
#endif

struct fasync_struct *fasync;
int ts_type = 0;

static void ts_interrupt(int, void*, struct pt_regs*);
static void ts_interrupt_main(int irq, int isTimer);
static struct timer_list tp_main_timer;
/* for iPaq compatible */
#define IPAQ_COMPATIBLE
#ifdef IPAQ_COMPATIBLE
#define BUFSIZE 128
#define XLIMIT 256
#define YLIMIT 256
static int raw_max_x, raw_max_y, res_x, res_y, raw_min_x, raw_min_y, xyswap;
static int cal_ok, x_rev, y_rev;
 
typedef struct {
   short pressure;
   short x;
   short y;
   short millisecs;
} TS_EVENT;

static TS_EVENT tbuf[BUFSIZE], tc, samples[3];
static DECLARE_WAIT_QUEUE_HEAD(queue);
static int head, tail, sample;
static char pendown = 0;
unsigned long Pressure;

 #ifdef CONFIG_PM
  #define PMPD_MODE_ACTIVE  0
  #define PMPD_MODE_SUSPEND 1
  #define PMPD_MODE_RESUME  2
  static char PowerDownMode = PMPD_MODE_ACTIVE;
 #endif // CONFIG_PM
#endif
extern unsigned short touch_panel_intr;

unsigned long ads7846_rw_cli(ulong data)
{
        ulong flags,ret;

	save_flags(flags);
	cli();

	SSDR = data;
	while ((SSSR & SSSR_TNF_MSK) != SSSR_TNF_MSK);
	udelay(1);
	while ((SSSR & SSSR_RNE_MSK) != SSSR_RNE_MSK);
	ret = (SSDR);

	restore_flags(flags);
	return ret;
}

unsigned long ads7846_rw(ulong data)
{
	SSDR = data;
	while ((SSSR & SSSR_TNF_MSK) != SSSR_TNF_MSK);
	udelay(1);
	while ((SSSR & SSSR_RNE_MSK) != SSSR_RNE_MSK);
	return (SSDR);
}

static unsigned int read_xydata_sample(unsigned long cmd, int diff, int delay, int sn);

void read_xydata(ts_pos_t *tp)
{
#if 0 /* Change: Trace Sample */
	unsigned long Rx = 1000;
	unsigned long cmd;
	unsigned int dummy,z1,z2;

	/* X-axis */
	cmd = (1u << ADSCTRL_PD0_SH) | (1u << ADSCTRL_PD1_SH) |
			(5u << ADSCTRL_ADR_SH) | (1u << ADSCTRL_STS_SH);
	dummy = ads7846_rw(cmd);
	udelay(1);
	tp->xd = ads7846_rw(cmd);

	/* Y-axis */
	cmd = (1u << ADSCTRL_PD0_SH) | (1u << ADSCTRL_PD1_SH) |
			(1u << ADSCTRL_ADR_SH) | (1u << ADSCTRL_STS_SH);
	dummy = ads7846_rw(cmd);
	udelay(1);
	tp->yd = ads7846_rw(cmd);
#else

#define	REJECT_XDIFF	16 /*32*/
#define	REJECT_YDIFF	16 /*48*/
#define	SAMPLE_DELAY	0
#define	SAMPLE_COUNT	4
	unsigned long cmd;
	unsigned int dummy, t1, t2;

	/* X-axis */
	cmd =	(1u << ADSCTRL_PD0_SH) | (1u << ADSCTRL_PD1_SH) |
		(5u << ADSCTRL_ADR_SH) | (1u << ADSCTRL_STS_SH);
	t1 = read_xydata_sample(cmd, REJECT_XDIFF, SAMPLE_DELAY, SAMPLE_COUNT);
	if( !t1 ){
//DBG*/		printk("X\n");
		goto sampling_error;
	}
	/* Y-axis */
	cmd =	(1u << ADSCTRL_PD0_SH) | (1u << ADSCTRL_PD1_SH) |
		(1u << ADSCTRL_ADR_SH) | (1u << ADSCTRL_STS_SH);
	t2 = read_xydata_sample(cmd, REJECT_YDIFF, SAMPLE_DELAY, SAMPLE_COUNT);
	if( !t2 ){
//DBG*/		printk("Y\n");
sampling_error:
		t1 = t2 = 0;
	}

	tp->xd = t1;
	tp->yd = t2;
#endif

#if 0 /* Delete: Pressure Check */
	/* Z1 */
	cmd = (1u << ADSCTRL_PD0_SH) | (1u << ADSCTRL_PD1_SH) |
			(3u << ADSCTRL_ADR_SH) | (1u << ADSCTRL_STS_SH);
	dummy = ads7846_rw(cmd);
	udelay(1);
	z1 = ads7846_rw(cmd);

	/* Z2 */
	cmd = (1u << ADSCTRL_PD0_SH) | (1u << ADSCTRL_PD1_SH) |
			(4u << ADSCTRL_ADR_SH) | (1u << ADSCTRL_STS_SH);
	dummy = ads7846_rw(cmd);
	udelay(1);
	z2 = ads7846_rw(cmd);


	if (z1 != 0)
//		Pressure = Rx * (tp->xd/4096) * ((z2/z1) - 1);
		Pressure = Rx * (tp->xd) * ((10*z2/z1) - 1*10) >> 10;
	else 
		Pressure = 0;

//printk("TS:1=%d,2=%d,X=%d,P=%d\n",z1,z2,tp->xd,Pressure);

#endif
		
        cmd =	(1u << ADSCTRL_PD0_SH) | (1u << ADSCTRL_PD1_SH) |
		(4u << ADSCTRL_ADR_SH) | (1u << ADSCTRL_STS_SH);
	/* Power-Down Enable */
	cmd &= ~(ADSCTRL_PD0_MSK | ADSCTRL_PD1_MSK);
	dummy = ads7846_rw(cmd);
}

static unsigned int read_xydata_sample(unsigned long cmd, int diff, int delay, int sn)
{
	unsigned int dummy, t1, t2, st;
	int i;

	dummy = ads7846_rw(cmd);
	udelay(1);
	t1 = ads7846_rw(cmd);
	udelay(1);
	t2 = ads7846_rw(cmd);
	if( ((signed)(t1 - t2) > diff) ||
	    ((signed)(t2 - t1) > diff) ){
//DBG*/		printk("TS:%d,%d,%d(%d)",dummy,t1,t2,t1-t2);
		return 0;
	}
	udelay(1+delay);

	t1 = (unsigned)-1;
	t2 = 0;
	st = 0;
	if( !sn ) sn = 1;
	for(i=0; i<sn; i++){
		unsigned int sd = ads7846_rw(cmd);
		st += sd;
		if( t1 > sd ) t1 = sd;
		if( t2 < sd ) t2 = sd;
		udelay(1);
	}
	if( sn > 3 ){
//DBG*/		if( (t2 - t1) > diff )
//DBG*/			printk("TS:%d-%d(%d),%d(%d)\n",t1,t2,(t2-t1),st,st/sn);
		st -= t1 + t2;
		sn -= 2;
	}
	return st / sn;
}

#ifdef IPAQ_COMPATIBLE
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
}

void ts_clear(void)
{
    int i;
	
    for (i =0; i < BUFSIZE; i++) {
	 tbuf[i].pressure = 0;
	 tbuf[i].x = 0;
	 tbuf[i].y = 0;
	 tbuf[i].millisecs = 0;
    }
    head = tail = pendown = 0;
}

static TS_EVENT get_data(void)
{
   int last = tail;
   if (++tail == BUFSIZE) { tail = 0; }
   return tbuf[last];
}

static void new_data(void)
{
	int diff0, diff1, diff2, diff3;

#ifdef CONFIG_PM
	if( PowerDownMode != PMPD_MODE_ACTIVE )
		return;
#endif

	if (tc.x > X_AXIS_MAX || tc.x < X_AXIS_MIN ||
			tc.y > Y_AXIS_MAX || tc.y < Y_AXIS_MIN) {
		//printk("over range (%d,%d)\n",tc.x,tc.y);
		return;
	}
	if (tc.pressure) {
#if 0 /* delete: TouchPanel SpeedUp */
		if (sample < 3) {
			samples[sample].x = tc.x;
			samples[sample++].y = tc.y;
			return;
		}
		sample = 0;

		diff0 = abs(tc.x - samples[2].x);
		diff1 = abs(tc.y - samples[2].y);
		if (diff0 == 0 && diff1 == 0) {
			return;
		}
		/* Check the variance between X samples (discard if not
		   similar), then choose the closest pair */
		diff0 = abs(samples[0].x - samples[1].x);
		diff1 = abs(samples[1].x - samples[2].x);
		diff2 = abs(samples[2].x - tc.x);
		diff3 = abs(tc.x - samples[1].x);

		if (diff0 > XLIMIT || diff1 > XLIMIT || diff2 > XLIMIT
				|| diff3 > XLIMIT) {
			return;
		}
		if (diff1 < diff2) {
			if (diff1 < diff3)
				tc.x = (samples[1].x + samples[2].x) / 2;
			else
				tc.x = (tc.x + samples[1].x) / 2;
		} else {
			if (diff2 < diff3)
				tc.x = (samples[2].x + tc.x) / 2;
			else
				tc.x = (tc.x + samples[1].x) / 2;
		}

		/* Do the same for Y */
		diff0 = abs(samples[0].y - samples[1].y);
		diff1 = abs(samples[1].y - samples[2].y);
		diff2 = abs(samples[2].y - tc.y);
		diff3 = abs(tc.y - samples[1].y);

		if (diff0 > YLIMIT || diff1 > YLIMIT || diff2 > YLIMIT
					|| diff3 > YLIMIT) {
			return;
		}
		if (diff1 < diff2) {
			if (diff1 < diff3)
				tc.y = (samples[1].y + samples[2].y) / 2;
			else
				tc.y = (tc.y + samples[1].y) / 2;
		} else {
			if (diff2 < diff3)
				tc.y = (samples[2].y + tc.y) / 2;
			else
				tc.y = (tc.y + samples[1].y) / 2;
		}
#endif
	} else
		if (pendown == 0) {
			return;
		}

	tc.millisecs = jiffies;
	tbuf[head++] = tc;
	if (head >= BUFSIZE) { head = 0; }
	if (head == tail && ++tail >= BUFSIZE) { tail = 0; }
	if (fasync)
		kill_fasync(&fasync, SIGIO, POLL_IN);
	wake_up_interruptible(&queue);
}
#endif

static void cotulla_main_ts_timer(unsigned long irq)
{
//	ts_interrupt(irq, NULL, NULL);
	touch_panel_intr++;
	ts_interrupt_main(irq, 1);
}

static void ts_interrupt(int irq, void *dev_id, struct pt_regs *regs)
{
	touch_panel_intr++;
	ts_interrupt_main(irq, 0);
}

static void ts_interrupt_main(int irq, int isTimer)
{
	ts_pos_t pos_dt;
	static TS_EVENT before_data = { pressure: 0 };

	if ((GPLR(1) & GPIO_PENIRQ) != GPIO_PENIRQ) {
		/* critical section */
		cli();
		/* Disable Falling Edge */
		GFER(1) &= ~GPIO_PENIRQ;
		read_xydata(&pos_dt);
//		sti(); /* move: TouchPanel SpeedUp */
		GEDR(1) = GPIO_PENIRQ; /* Clear detect */
		sti();
//		if( GPLR(1) & GPIO_PENIRQ )
//			printk("!(%d,%d)",pos_dt.xd,pos_dt.yd);
		tc.x = pos_dt.xd;
		tc.y = pos_dt.yd;
		if( pos_dt.xd && pos_dt.yd ){
			tc.pressure = 1;
			before_data = tc;
			pendown = 1;
			new_data();
		}
		mod_timer(&tp_main_timer, jiffies + HZ / 100);
	} else {
		if( pendown == 1 ){
			mod_timer(&tp_main_timer, jiffies + HZ / 100);
			pendown ++;
			return;
		}
		if (pendown) {
			tc = before_data;
			tc.pressure = 0;
			new_data();
		}
		/* Clear detect */
		GEDR(1) = GPIO_PENIRQ;
		/* Enable Falling Edge */
		GFER(1) |= GPIO_PENIRQ;
		pendown = 0;
#ifdef CONFIG_PM
		PowerDownMode = PMPD_MODE_ACTIVE;
#endif
	}
}

static void ts_hardware_init(void)
{
	unsigned int dummy;
	
	/* Enable the Synchronous Serial Port clock */
	CKEN |= SSP_CKEN;

	SSCR0 = 0x1ab;
	SSCR1 = 0;

/*
	SSCR0 = (0x0b << SSCR0_DSS) | (2 << SSCR0_FRF) | (0 << SSCR0_ECS) |
		(1 << SSCR0_SSE) | (2 << SSCR0_SCR);

	SSCR1 = (0 << SSCR1_RIE) | (0 << SSCR1_TIE) | (0 << SSCR1_LBM) |
		(0 << SSCR1_SPO) | (0 << SSCR1_SPH) | (0 << SSCR1_MWDS) |
		(3 << SSCR1_TFT) | (3 << SSCR1_RFT);
*/


	/* Initiaize ADS7846 Difference Reference mode */
	dummy=ads7846_rw((1u << ADSCTRL_ADR_SH) | (1u << ADSCTRL_STS_SH));
	mdelay(5);
	dummy=ads7846_rw((3u << ADSCTRL_ADR_SH) | (1u << ADSCTRL_STS_SH));
	mdelay(5);
	dummy=ads7846_rw((4u << ADSCTRL_ADR_SH) | (1u << ADSCTRL_STS_SH));
	mdelay(5);
	dummy=ads7846_rw((5u << ADSCTRL_ADR_SH) | (1u << ADSCTRL_STS_SH));
	mdelay(5);

	/* Enable Falling Edge */
	GFER(1) |= GPIO_PENIRQ;
	/* Clear detect */
	GEDR(1) = GPIO_PENIRQ;
}


static int ts_open(struct inode *, struct file *);
static int ts_release(struct inode *, struct file *);
static int ts_fasync(int fd, struct file *filp, int on);
static ssize_t ts_read(struct file *, char *, size_t, loff_t *);
static ssize_t ts_write(struct file *, const char *, size_t, loff_t *);
static unsigned int ts_poll(struct file *, poll_table *);
static int ts_ioctl(struct inode *, struct file *, unsigned int, unsigned long);

struct file_operations ts_fops = {
	open:		ts_open,
	release:	ts_release,
	fasync:		ts_fasync,
	read:		ts_read,
	write:		ts_write,
	poll:		ts_poll,
	ioctl:		ts_ioctl,
};

int ts_cotulla_cleanup(void)
{
	unregister_chrdev(TS_MAJOR, "ts");
	free_irq(IRQ_GPIO51, NULL);
}

int ts_cotulla_init(void)
{
        if ( register_chrdev(TS_MAJOR,"ts",&ts_fops) )
                printk("unable to get major %d for touch screen\n", TS_MAJOR);

	ts_clear();
	raw_max_x = X_AXIS_MAX;
	raw_max_y = Y_AXIS_MAX;
	raw_min_x = X_AXIS_MIN;
	raw_min_y = Y_AXIS_MIN;
	res_x = 240;
	res_y = 320;

	cal_ok = 1;
	x_rev = 1;
	y_rev = 1;
	xyswap = 0;

	ts_hardware_init();
	init_timer(&tp_main_timer);
	tp_main_timer.data = IRQ_GPIO6;
	tp_main_timer.function = cotulla_main_ts_timer;

	if (request_irq(IRQ_GPIO51, ts_interrupt, 0, "ts", NULL)) {
		return -EBUSY;
	}

#ifdef CONFIG_PM
	touch_pm_dev = pm_register(PM_COTULLA_DEV, 0, cotulla_touch_pm_callback);
#endif

	printk("Cotulla Touch Screen driver initialized\n");

	return 0;
}

module_init(ts_cotulla_init);
module_exit(ts_cotulla_cleanup);


static int ts_open(struct inode *inode, struct file *file)
{
	kdev_t dev = inode->i_rdev;
	if( MINOR(dev) == 1 )
		ts_type = 0;
	else 	
		ts_type = 1;

	return 0;
}


static int ts_release(struct inode *inode, struct file *file)
{
	ts_fasync(-1, file, 0);
	return 0;
}

static int ts_fasync(int fd, struct file *filp, int on)
{
	int retval;

	retval = fasync_helper(fd, filp, on, &fasync);
	if (retval < 0)
		return retval;
	return 0;
}

static ssize_t ts_read(struct file *file, char *buffer, size_t count, loff_t *ppos)
{
	DECLARE_WAITQUEUE(wait, current);
	TS_EVENT t, r;
	int i;
	
	if (head == tail) {
		if (file->f_flags & O_NONBLOCK) {
			return -EAGAIN;
		}
		add_wait_queue(&queue, &wait);
		current->state = TASK_INTERRUPTIBLE;
		while ((head == tail)&& !signal_pending(current)) {
			schedule();
			current->state = TASK_INTERRUPTIBLE;
		}
		current->state = TASK_RUNNING;
		remove_wait_queue(&queue, &wait);
	}
	
	for (i = count ; i >= sizeof(TS_EVENT);
			i -= sizeof(TS_EVENT), buffer += sizeof(TS_EVENT)) {
		if (head == tail)
			break;
		t = get_data();

		if (xyswap) {
		    short tmp = t.x;
		    t.x = t.y;
		    t.y = tmp;
		}
		if (cal_ok) {
			r.x = (x_rev) ? ((raw_max_x - t.x) * res_x) / (raw_max_x - raw_min_x)
		 		: ((t.x - raw_min_x) * res_x) / (raw_max_x - raw_min_x);
			r.y = (y_rev) ? ((raw_max_y - t.y) * res_y) / (raw_max_y - raw_min_y)
				: ((t.y - raw_min_y) * res_y) / (raw_max_y - raw_min_y);
		} else {
			r.x = t.x;
			r.y = t.y;
		}

		r.pressure = t.pressure;
		r.millisecs = t.millisecs;
		//printk("ts:ts_read(x, y, p) = (%d, %d, %d)\n", (unsigned int)r.x, (unsigned int)r.y, (unsigned int)r.pressure);

		copy_to_user(buffer,&r,sizeof(TS_EVENT));
	}
	return count - i;
}

static ssize_t	ts_write(struct file *file, const char *buffer, size_t count, loff_t *ppos)
{
	return 0;
}

static unsigned int ts_poll(struct file *filp, poll_table *wait)
{
	poll_wait(filp, &queue, wait);
	if (head != tail)
		return POLLIN | POLLRDNORM;
	return 0;
}

static int ts_ioctl(struct inode *inode, struct file *file, unsigned int cmd, unsigned long arg)
{
	switch(cmd){
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
        case 13:         /* 0 = Enable calibration ; 1 = Calibration OK */
            cal_ok = arg;
        case 14:         /* Clear all buffer data */
	    ts_clear();
	    break;	
	case 15:         /* X axis reversed setting */
	    x_rev = arg;
	    break;
	case 16:         /* Y axis reversed setting */
            y_rev = arg;
	    break;
	case 17:         /* Clear all buffer data */
	    print_par();
	    break;
	default:
		return -EINVAL;
	}

	return 0;
}

#ifdef CONFIG_PM
void cotulla_touch_suspend() {
	unsigned long cmd;
	unsigned int dummy;

	GFER(1) &= ~GPIO_PENIRQ;
	if( pendown ){
		del_timer(&tp_main_timer);
		tc.pressure = 0;
		new_data();
		pendown = 0;
	}
	PowerDownMode = PMPD_MODE_SUSPEND;

	cmd = (1u << ADSCTRL_ADR_SH) | (1u << ADSCTRL_STS_SH);

	dummy = ads7846_rw(cmd);

	SSCR0 = 0x2b;

	CKEN &= ~SSP_CKEN;
	GEDR(1) = GPIO_PENIRQ;

}

void cotulla_touch_resume() {
	unsigned long cmd;
	unsigned int dummy;

	CKEN |= SSP_CKEN;
	SSCR0 = 0x1ab;
	SSCR1 = 0;
#if 0
	GEDR(1) = GPIO_PENIRQ;
	GFER(1) |= GPIO_PENIRQ;
//	GEDR(1) = GPIO_PENIRQ;		
#else
	PowerDownMode = PMPD_MODE_RESUME;
	mod_timer(&tp_main_timer, jiffies + HZ / 10);
#endif

	cmd =   (1u << ADSCTRL_PD0_SH) | (1u << ADSCTRL_PD1_SH) |
		(4u << ADSCTRL_ADR_SH) | (1u << ADSCTRL_STS_SH);
	cmd &= ~(ADSCTRL_PD0_MSK | ADSCTRL_PD1_MSK);
	dummy = ads7846_rw(cmd);
}

static int cotulla_touch_pm_callback(struct pm_dev* pm_dev,
			       pm_request_t req, void* data)
{
	switch (req) {
	case PM_SUSPEND:
		cotulla_touch_suspend();
		break;
		
	case PM_RESUME:
		cotulla_touch_resume();
		break;
		
	}

	return 0;
}
#endif
