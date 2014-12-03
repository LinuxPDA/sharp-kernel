/*
 * linux/arch/arm/drivers/char/pfs168_spi.c
 *
 * SA-1110 Ser4SSP SPI driver for PFS-168.
 *
 * Changelog:
 */

#include <linux/config.h>

#include <linux/spinlock.h>
#include <linux/sched.h>
#include <linux/interrupt.h>
#include <linux/module.h>
#include <linux/tty.h>
#include <linux/mm.h>
#include <linux/signal.h>
#include <linux/init.h>
#include <linux/kbd_ll.h>
#include <linux/delay.h>
#include <linux/random.h>
#include <linux/poll.h>
#include <linux/miscdevice.h>
#include <linux/malloc.h>
#include <linux/kbd_kern.h>

#include <asm/bitops.h>
#include <asm/uaccess.h>
#include <asm/irq.h>
#include <asm/system.h>
#include <asm/hardware.h>
#include <asm/io.h>


#define SPI_BUF_SIZE	32


spinlock_t pfs168_spi_controller_lock = SPIN_LOCK_UNLOCKED;


struct pfs168_spi_queue {
	unsigned long head;
	unsigned long tail;
	wait_queue_head_t proc_list;
	struct fasync_struct *fasync;
	unsigned char buf[SPI_BUF_SIZE];
};                                                                                                                                            

static struct pfs168_spi_queue *queue;	 /* spi data buffer. */

static int pfs168_spi_count = 0;


static void send_SSP_msg(unchar *pBuf, int num)
{
	ushort	tmp;
	int		i;

	for (i = 0; i < num; i++) {
		while ((Ser4SSSR & SSSR_TNF) == 0)
			;

		tmp = pBuf[i];
		Ser4SSDR = (tmp << 8);
	}

	// Throw away Echo
	for (i = 0; i < num; i++) {
		while ((Ser4SSSR & SSSR_RNE) == 0)
			;

		tmp = Ser4SSDR;
	}
}


static unchar ReadSSPByte(void)
{
	if (Ser4SSSR & SSSR_ROR) {
		printk("%s() : Overrun\n", __FUNCTION__);
		return 0;
	}

	Ser4SSDR = 0x00;

	while ((Ser4SSSR & SSSR_RNE) == 0)
		;

	return ((unchar) Ser4SSDR);
}


static ulong read_SSP_response(int num)
{
	int		i;
	ulong	ret;
	
	while (ReadSSPByte() == 0)
		;

	ReadSSPByte();

	ret = 0;

	for (i = 0; i < num; i++) {
		ret <<= 8;
		ret |= ReadSSPByte();
	}

	return ret;
}


#if	0
typedef	struct	pfs168_spi_cmd {
	unchar	group, code; 
}	pfs168_spi_cmd;


static void pfs168_spi_interrupt(int irq, void *dev_id, struct pt_regs *regs)
{
	pfs168_spi_cmd	RD_INT_CMD = { 0x83, 0x01 };
	pfs168_spi_cmd	RD_SPI_CMD = { 0x83, 0x02 };
	unchar	code;

	/* Read AVR Interrupt Status to check which Interrupt occurred 
	 * and Clear AVR Interrupt */
	send_SSP_msg((unchar *) &RD_INT_CMD, 2);
	code = (unchar) (read_SSP_response(1) & 0xFF);

#if	0
	if (code  & (1 << 2)) {
		/* Read Scan code */
		send_SSP_msg((unchar *) &RD_SPI_CMD, 2);
		code = (unchar) (read_SSP_response(1) & 0xFF);
		handle_scancode( code, (code & 0x80) ? 0 : 1 );
	}
#endif
}
#endif


static inline void pfs168_spi_disable_irq(void)
{
	disable_irq(IRQ_GPIO14);
}


static inline void pfs168_spi_enable_irq(void)
{
	enable_irq(IRQ_GPIO14);
}

/* -------------------------------------------------------------------- */


static void pfs168_spi_interrupt(int irq, void *dev_id, struct pt_regs *regs)
{
	unsigned long flags;

	spin_lock_irqsave(&pfs168_spi_controller_lock, flags);

	if (pfs168_spi_count) {
		int head = queue->head;
		int tmp;

		/* This is *_awful_*, but for now... */

		while (Ser4SSSR & SSSR_RNE)
			tmp = Ser4SSDR;	/* Discard anything laying around */

		queue->buf[head] = ReadSSPByte();

		head = (head + 1) & (SPI_BUF_SIZE-1);

		if (head != queue->tail) {
			queue->head = head;

			if (queue->fasync)
				kill_fasync(&queue->fasync, SIGIO, POLL_IN);

			wake_up_interruptible(&queue->proc_list);
		}
	}

	/*tasklet_schedule(&pfs168_spi_tasklet);*/

	spin_unlock_irqrestore(&pfs168_spi_controller_lock, flags);
}


int pfs168_spi_startup_reset __initdata = 1;

/* for "spi-reset" cmdline param */
static int __init pfs168_spi_reset_setup(char *str)
{
	pfs168_spi_startup_reset = 1;

	return 1;
}

__setup("spi-reset", pfs168_spi_reset_setup);


/*
 * Send a byte to the spi.
 */

static void pfs168_spi_write_dev(char *p, int n)
{
	int i;
	unsigned int tmp;

	DECLARE_WAITQUEUE(wait, current);

	add_wait_queue(&queue->proc_list, &wait);

	printk("pfs168_spi_write_dev: %d Bytes to send\n", n);

	pfs168_spi_disable_irq();

xmit_wait:
	set_current_state(TASK_INTERRUPTIBLE);

	if (Ser4SSSR & SSSR_BSY) {
		printk("pfs168_spi_write_dev: SSSR_BSY loop 1\n", n);
		schedule();
		goto xmit_wait;
	}

	current->state = TASK_RUNNING;

	for (i = 0; i < n; i++) {
		tmp = *p++;
		Ser4SSDR = tmp << 8;

echo_wait:
		set_current_state(TASK_INTERRUPTIBLE);

		if (Ser4SSSR & SSSR_BSY) {
			printk("pfs168_spi_write_dev: SSSR_BSY loop 2\n", n);
			schedule();
			goto echo_wait;
		}

		current->state = TASK_RUNNING;

		// Throw away Echo
		tmp = Ser4SSDR;
	}

	remove_wait_queue(&queue->proc_list, &wait);

	pfs168_spi_enable_irq();
}


static unsigned char get_from_queue(void)
{
	unsigned char result;
	unsigned long flags;

	spin_lock_irqsave(&pfs168_spi_controller_lock, flags);
	result = queue->buf[queue->tail];
	queue->tail = (queue->tail + 1) & (SPI_BUF_SIZE-1);
	spin_unlock_irqrestore(&pfs168_spi_controller_lock, flags);

	return result;
}


static inline int queue_empty(void)
{
	return queue->head == queue->tail;
}


static int pfs168_spi_fasync(int fd, struct file *filp, int on)
{
	int retval;

	retval = fasync_helper(fd, filp, on, &queue->fasync);

	if (retval < 0)
		return retval;

	return 0;
}


static int pfs168_spi_release(struct inode * inode, struct file * file)
{
	pfs168_spi_fasync(-1, file, 0);

	if (!pfs168_spi_count)
		return 0;

	pfs168_spi_count -= 1;

	free_irq(IRQ_GPIO14, NULL);

	return 0;
}


/*
 * Install interrupt handler.
 */

static int pfs168_spi_open(struct inode * inode, struct file * file)
{
	if (pfs168_spi_count) {
		return -EBUSY;
	}

	pfs168_spi_count += 1;

	queue->head = queue->tail = 0;	   /* Flush input queue */

	if (request_irq (IRQ_GPIO14, pfs168_spi_interrupt, 0, "SA-1110 SSP", NULL) != 0) {
		printk("Could not allocate spi IRQ!\n");
		pfs168_spi_count--;
		return -EBUSY;
	}

	return 0;
}

/*
 * Put bytes from input queue to buffer.
 */

static ssize_t pfs168_spi_read(struct file * file, char * buffer, size_t count, loff_t *ppos)
{
	DECLARE_WAITQUEUE(wait, current);
	ssize_t i = count;
	unsigned char c;

	if (queue_empty()) {
		if (file->f_flags & O_NONBLOCK)
			return -EAGAIN;

		add_wait_queue(&queue->proc_list, &wait);

repeat:
		set_current_state(TASK_INTERRUPTIBLE);

		if (queue_empty() && !signal_pending(current)) {
			schedule();
			goto repeat;
		}

		current->state = TASK_RUNNING;
		remove_wait_queue(&queue->proc_list, &wait);
	}

	while (i > 0 && !queue_empty()) {
		c = get_from_queue();
		put_user(c, buffer++);
		i--;
	}

	if (count-i) {
		file->f_dentry->d_inode->i_atime = CURRENT_TIME;
		return count-i;
	}

	if (signal_pending(current))
		return -ERESTARTSYS;

	return 0;
}


/*
 * Write to the spi device.
 */

static ssize_t pfs168_spi_write(struct file * file, const char * buffer, size_t count, loff_t *ppos)
{
	ssize_t retval = 0;
	char kbuf[8];

	if (count) {
		if (count > 8)
			count = 8; /* Limit to 8 bytes. */

		copy_from_user(kbuf, buffer, count);

		pfs168_spi_write_dev(kbuf, count);

		retval = count;

		file->f_dentry->d_inode->i_mtime = CURRENT_TIME;
	}

	return retval;
}


static unsigned int pfs168_spi_poll(struct file *file, poll_table * wait)
{
	poll_wait(file, &queue->proc_list, wait);

	if (!queue_empty())
		return POLLIN | POLLRDNORM;

	return 0;
}


static inline void pfs168_spi_init_dev(void)
{
	GPDR |= (GPIO_GPIO10 | GPIO_GPIO12 | GPIO_GPIO13);
	GPDR &= ~GPIO_GPIO11;

	GAFR |= (GPIO_GPIO10 | GPIO_GPIO11 | GPIO_GPIO12 | GPIO_GPIO13);

	Ser4SSCR0 = SSCR0_SerClkDiv(0xa9) | SSCR0_Motorola | SSCR0_DataSize(8);
	Ser4SSSR = SSSR_ROR;
	Ser4SSCR1 = SSCR1_SP;
	Ser4SSCR0 |= SSCR0_SSE;

	/* Release AT89S53 reset line. */
	PFS168_SYSLCDDE &= ~0x4;
}


struct file_operations pfs168_spi_fops = {
	read:		pfs168_spi_read,
	write:		pfs168_spi_write,
	poll:		pfs168_spi_poll,
	open:		pfs168_spi_open,
	release:	pfs168_spi_release,
	fasync:		pfs168_spi_fasync,
};


static struct miscdevice pfs168_spi_dev = {
	MISC_DYNAMIC_MINOR, "spi", &pfs168_spi_fops
};


static void __init pfs168_spi_init(void)
{
	printk (KERN_INFO "PFS-168 SA-1110 SPI Driver v1.0\n");

	pfs168_spi_init_dev();

	misc_register(&pfs168_spi_dev);

	queue = (struct pfs168_spi_queue *) kmalloc(sizeof(*queue), GFP_KERNEL);
	memset(queue, 0, sizeof(*queue));
	queue->head = queue->tail = 0;
	init_waitqueue_head(&queue->proc_list);

	set_GPIO_IRQ_edge(GPIO_GPIO(14), GPIO_RISING_EDGE);
}

 
static void __exit pfs168_spi_exit(void)
{
	misc_deregister(&pfs168_spi_dev);
}

module_init(pfs168_spi_init);
module_exit(pfs168_spi_exit);

MODULE_AUTHOR("George G. Davis");
MODULE_DESCRIPTION("PFS-168 SPI Driver");

EXPORT_NO_SYMBOLS;
