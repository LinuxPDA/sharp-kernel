/*
 * Copyright (c) 2003 Lineo uSolutions, Inc.
 *
 * linux/drivers/char/tpm102_ts.c
 *
 */

#include <linux/config.h>
#include <linux/init.h>
#include <linux/poll.h>
#include <linux/delay.h>
#include <linux/ioctl.h>

#include <asm/segment.h>
#include <asm/uaccess.h>
#include <asm/arch/hardware.h>
#include <asm/arch/irqs.h>
#include <asm/arch/tpm102_ts.h>

struct fasync_struct *fasync;
int ts_type = 0;

static struct timer_list tp_main_timer;

#define BUFSIZE 128

static int raw_max_x, raw_max_y, res_x, res_y, raw_min_x, raw_min_y, xyswap;
static int cal_ok, x_rev, y_rev;

typedef struct {
    short pressure;
    short x;
    short y;
    short millisecs;
    long long stamp;
} TS_EVENT;

static TS_EVENT tbuf[BUFSIZE], tc;
static DECLARE_WAIT_QUEUE_HEAD(queue);
static int head, tail, sample;
static char pendown = 0;

static spinlock_t ts_lock;

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
    unsigned long flags;

    if (tc.x > X_AXIS_MAX || tc.x < X_AXIS_MIN ||
	tc.y > Y_AXIS_MAX || tc.y < Y_AXIS_MIN) {
	return;
    }

    tc.millisecs = jiffies;

    spin_lock_irqsave(&ts_lock, flags);
    tbuf[head++] = tc;
    if (head >= BUFSIZE) { head = 0; }
    if (head == tail && ++tail >= BUFSIZE) { tail = 0; }
    if (fasync)
	kill_fasync(&fasync, SIGIO, POLL_IN);
    wake_up_interruptible(&queue);
    spin_unlock_irqrestore(&ts_lock, flags);
}

static void pendn_interrupt(void)
{
    int i;
    unsigned int x, y, xh, xl, yh, yl, tmp;

    del_timer(&tp_main_timer);

    DBMX1_SPI2_TXDATAREG2 = XDIFF_PENIRQ;
    DBMX1_SPI2_TXDATAREG2 = NULLPACK;
    DBMX1_SPI2_TXDATAREG2 = NULLPACK;
    DBMX1_SPI2_TXDATAREG2 = YDIFF_PENIRQ;
    DBMX1_SPI2_TXDATAREG2 = NULLPACK;
    DBMX1_SPI2_TXDATAREG2 = NULLPACK;
    DBMX1_SPI2_CONTROLREG2 |= 0x0100;

    for (i = 0; i < 10000; i++) {
	if (DBMX1_SPI2_INTREG2 & 0x0001)
	    break;
    }

    tmp = DBMX1_SPI2_RXDATAREG2;
    xh = DBMX1_SPI2_RXDATAREG2;
    xl = DBMX1_SPI2_RXDATAREG2;
    tmp = DBMX1_SPI2_RXDATAREG2;
    yh = DBMX1_SPI2_RXDATAREG2;
    yl = DBMX1_SPI2_RXDATAREG2;

    x = ((xh << 5) & 0x0FE0) | ((xl >> 3) & 0x1F);
    y = ((yh << 5) & 0x0FE0) | ((yl >> 3) & 0x1F);
    if (x != 0) {
	tc.x = x;
	tc.y = y;
	tc.pressure = 500;
	new_data();
    }
    mod_timer(&tp_main_timer, jiffies + HZ / 100);
}

static void penup_interrupt(void)
{
    del_timer(&tp_main_timer);
    tc.pressure = 0;
    new_data();
 }

static void main_ts_timer(unsigned long irq)
{
    pendn_interrupt();
}

static void ts_hardware_init(void)
{
    DBMX1_SPI2_RESETREG2 = 1;
    DBMX1_SPI2_RESETREG2 = 0;
    DBMX1_SPI2_CONTROLREG2 = 0x00002607;
//    DBMX1_SPI2_INTREG2 |= 0x0000fc00;
//    DBMX1_GPIO_DR_B &= ~0x00800000;	// LCD 3.3V on
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
    unsigned long flags;

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

    spin_lock_irqsave(&ts_lock, flags);
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
    spin_unlock_irqrestore(&ts_lock, flags);
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
    case 12:            /* New attribute for portrait modes */
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


struct file_operations ts_fops = {
	open:		ts_open,
	release:	ts_release,
	fasync:		ts_fasync,
	read:		ts_read,
	write:		ts_write,
	poll:		ts_poll,
	ioctl:		ts_ioctl,
};

int tpm102_ts_init(void)
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
    tp_main_timer.function = main_ts_timer;

    if (request_irq(IRQ_PEN_DN, pendn_interrupt, SA_INTERRUPT,
		    "pen_dn", NULL)) {
	return -EBUSY;
    }
    if (request_irq(IRQ_PEN_UP, penup_interrupt, SA_INTERRUPT,
		    "pen_up", NULL)) {
	free_irq(IRQ_PEN_DN, NULL);
	return -EBUSY;
    }

    printk("TPM102 Touch Screen driver initialized\n");

    return 0;
}

void tpm102_ts_cleanup(void)
{
    unregister_chrdev(TS_MAJOR, "ts");
    free_irq(IRQ_PEN_UP, NULL);
    free_irq(IRQ_PEN_DN, NULL);
}

module_init(tpm102_ts_init);
module_exit(tpm102_ts_cleanup);
