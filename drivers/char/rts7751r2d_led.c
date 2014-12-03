/*
 * rts7751r2d_led.c -- driver for the LED Control.
 *
 * This driver will also support the Renesas Technology Sales RTS7751R2D.
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file "COPYING" in the main directory of this archive
 * for more details.
 *
 * Copylight (C) 2003 Atom Create Engineering Co., Ltd.
 *
 */

#include <linux/config.h>
#include <linux/module.h>
#include <linux/errno.h>
#include <linux/signal.h>
#include <linux/major.h>
#include <linux/poll.h>
#include <linux/init.h>
#include <linux/delay.h>
#include <linux/sched.h>
#include <linux/timer.h>

#include <asm/system.h>
#include <asm/io.h>
#include <asm/uaccess.h>
#include <asm/renesas_rts7751r2d.h>

#define LED_MAJOR		99	/* LED device major no. */
#define RTS7751R2D_LED_MINOR	1	/* LED device minor no. */

#define LED9_ON		1
#define LED9_OFF	2
#define LED10_ON	3
#define LED10_OFF	4
#define LED11_ON	5
#define LED11_OFF	6
#define LED12_ON	7
#define LED12_OFF	8
#define LED13_ON	9
#define LED13_OFF	10
#define LED14_ON	11
#define LED14_OFF	12
#define LED15_ON	13
#define LED15_OFF	14
#define LED16_ON	15
#define LED16_OFF	16

static int openCnt;
static short led_status;

/*
 * Functions prototypes
 */
static int rts7751r2d_led_ioctl(struct inode *, struct file *, unsigned int, unsigned long arg);
static int rts7751r2d_led_open(struct inode *, struct file *);
static int rts7751r2d_led_close(struct inode *, struct file *);
static int rts7751r2d_led_read(struct file *, char *, size_t, loff_t *);
static int rts7751r2d_led_write(struct file *, char *, size_t, loff_t *);

static int rts7751r2d_led_ioctl(struct inode *inode, struct file *file, unsigned int cmd, unsigned long arg)
{
	switch (cmd) {
		case LED9_ON:			/* LED9 ON */
			led_status |= 0x0001;
			break;
		case LED9_OFF:			/* LED9 OFF */
			led_status &= 0xfffe;
			break;
		case LED10_ON:			/* LED10 ON */
			led_status |= 0x0002;
			break;
		case LED10_OFF:			/* LED10 OFF */
			led_status &= 0xfffd;
			break;
		case LED11_ON:			/* LED11 ON */
			led_status |= 0x0004;
			break;
		case LED11_OFF:			/* LED11 OFF */
			led_status &= 0xfffb;
			break;
		case LED12_ON:			/* LED12 ON */
			led_status |= 0x0008;
			break;
		case LED12_OFF:			/* LED12 OFF */
			led_status &= 0xfff7;
			break;
		case LED13_ON:			/* LED13 ON */
			led_status |= 0x0010;
			break;
		case LED13_OFF:			/* LED13 OFF */
			led_status &= 0xffef;
			break;
		case LED14_ON:			/* LED14 ON */
			led_status |= 0x0020;
			break;
		case LED14_OFF:			/* LED14 OFF */
			led_status &= 0xffdf;
			break;
		case LED15_ON:			/* LED15 ON */
			led_status |= 0x0040;
			break;
		case LED15_OFF:			/* LED15 OFF */
			led_status &= 0xffbf;
			break;
		case LED16_ON:			/* LED16 ON */
			led_status |= 0x0080;
			break;
		case LED16_OFF:			/* LED16 OFF */
			led_status &= 0xff7f;
			break;
		default:
			return -EINVAL;
	}

	ctrl_outw(led_status, PA_OUTPORT);

	return 0;
}

static int rts7751r2d_led_open(struct inode *inode, struct file *file)
{
	int minor;

	minor = MINOR(inode->i_rdev);
	if (minor != RTS7751R2D_LED_MINOR)
		return -ENOENT;

	if (openCnt > 0)
		return -EALREADY;

	openCnt++;

	return 0;
}

static int rts7751r2d_led_close(struct inode *inode, struct file *file)
{

	openCnt--;

	return 0;
}

static int rts7751r2d_led_read(struct file *file, char *buff, size_t count, loff_t* ppos)
{
	int error ;

	if ((error = verify_area(VERIFY_WRITE, (void *)buff, count)))
		return error;

	return 0;
}

static int rts7751r2d_led_write(struct file *file, char *buff, size_t count, loff_t* ppos)
{
	int error ;

	if ((error = verify_area(VERIFY_WRITE, (void *)buff, count)))
		return error;

	return 0;
}

static struct file_operations led_fops = {
	read:		rts7751r2d_led_read,	/* read */
	ioctl:		rts7751r2d_led_ioctl,	/* ioctl */
	open:		rts7751r2d_led_open,	/* open */
	release:	rts7751r2d_led_close,	/* release */
};

static char banner[] __initdata =
	KERN_INFO "RTS7751R2D LED driver initialized\n";

int __init rts7751r2d_led_init(void)
{
	int error;

	printk("%s", banner);

	openCnt = 0;
	led_status = 0x0000;

	ctrl_outw(led_status, PA_OUTPORT);	/* LED Clear */

	if ((error=register_chrdev(LED_MAJOR, "rts7751r2dled", &led_fops))) {
		printk(KERN_ERR "RTS7751R2D LED driver:Couldn't register driver, error=%d\n", error);
		return 1;
	}

	return 0;
}

#ifdef MODULE
void __exit rts7751r2d_led_cleanup(void)
{
	unregister_chrdev(LED_MAJOR, "rts7751r2dled");
	printk("RTS7751R2D LED driver removed.\n");
}
#endif

module_init(rts7751r2d_led_init);
#ifdef MODULE
module_exit(rts7751r2d_led_cleanip);
#endif
