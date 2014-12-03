/*
 * SA11x0 + UCB1x00 adc driver
 * really just a little layer on top of code in touch screen driver
 */

#include <linux/config.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/proc_fs.h>              /* all the /proc functions */
#include <linux/fs.h>
#include <linux/poll.h>

#include <asm/uaccess.h>
#include <asm/hardware.h>
#include <asm/ucb1200.h>

#if 0
#include <linux/config.h>
#include <linux/types.h>
#include <linux/module.h>
#include <linux/sched.h>
#include <linux/kernel.h>
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
#endif

extern struct proc_dir_entry *proc_ucb1200;

static struct fasync_struct *fasync;

/* in ucb1200_ts.c */
extern int ucb1200_adc_start(int channel);
extern int ucb1200_adc_done(void);
extern int ucb1200_adc_value(int channel);

static DECLARE_WAIT_QUEUE_HEAD(queue);

static unsigned int ucb1200_adc_poll(struct file *filp, poll_table *wait)
{
	poll_wait(filp, &queue, wait);
	if (ucb1200_adc_done())
		return POLLIN | POLLRDNORM;
	return 0;
}

static int ucb1200_adc_ioctl(struct inode *inode, struct file *filp, unsigned int cmd, unsigned long arg)
{
   switch (cmd)
   {
   case 0:
	 break;
   }

   return 0;
}

static int ucb1200_adc_fasync(int fd, struct file *filp, int on)
{
	int retval;

	retval = fasync_helper(fd, filp, on, &fasync);
	if (retval < 0)
		return retval;
	return 0;
}

static int ucb1200_adc_open(struct inode *inode, struct file *filp)
{
	MOD_INC_USE_COUNT;
	return 0;
}

static int ucb1200_adc_release(struct inode *inode, struct file *filp)
{
	ucb1200_adc_fasync(-1, filp, 0);
	MOD_DEC_USE_COUNT;
	return 0;
}


static ssize_t ucb1200_adc_read(struct file *filp, char *buf,
				size_t nbytes, loff_t *ppos)
{
   DECLARE_WAITQUEUE(wait, current);
   char outputbuf[15];
   int ret, count;

   //printk("ucb1200_adc_read() nbytes %d, pos %lu\n", nbytes, *ppos);

   if (*ppos > 0) /* Assume reading completed in previous read*/
	   return 0;

   ret = ucb1200_adc_start(0);

   if (ret != 0) {
	   return ret;
   }

   if (!ucb1200_adc_done())
   {
      if (filp->f_flags & O_NONBLOCK)
	 return -EAGAIN;
      add_wait_queue(&queue, &wait);
      current->state = TASK_INTERRUPTIBLE;
      while (!ucb1200_adc_done() && !signal_pending(current))
      {
	      schedule_timeout(1);
	      current->state = TASK_INTERRUPTIBLE;
      }
      current->state = TASK_RUNNING;
      remove_wait_queue(&queue, &wait);
   }

   count = sprintf(outputbuf, "%d\n", ucb1200_adc_value(0));

   *ppos += count;

   if (count > nbytes)  /* Assume output can be read at one time */
	   return -EINVAL;

   if (copy_to_user(buf, outputbuf, count))
	   return -EFAULT;

   return count;
}

static struct file_operations ucb1200_adc_fops = {
	read:	ucb1200_adc_read,
	poll:	ucb1200_adc_poll,
	ioctl:	ucb1200_adc_ioctl,
	fasync:	ucb1200_adc_fasync,
	open:	ucb1200_adc_open,
	release:ucb1200_adc_release,
};


int __init ucb1200_adc_init(void)
{
	struct proc_dir_entry *entry;

	if (!proc_ucb1200) {
		printk("ucb1200 not loaded\n");
		return -EINVAL;
	}

	entry = create_proc_entry("adc",
				  S_IWUSR | S_IRUSR | S_IRGRP | S_IROTH,
				  proc_ucb1200);

	if (entry) {
		entry->proc_fops = &ucb1200_adc_fops;
	} else {
		printk(KERN_ERR
		       "ucb1200_adc: can't create /proc/driver/ucb1200/adc\n");
		return -ENOMEM;
	}

	printk("ucb1200 adc driver initialized\n");
	return 0;
}

void __exit ucb1200_adc_cleanup(void)
{
	remove_proc_entry("adc", proc_ucb1200);

	printk("ucb1200 adc driver removed\n");
}

module_init(ucb1200_adc_init);
module_exit(ucb1200_adc_cleanup);

