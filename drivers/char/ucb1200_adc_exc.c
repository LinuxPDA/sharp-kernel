/*
 *  linux/drivers/char/ucb1200_adc.c
 *
 * (C) Copyright 2001 Lineo Japan, Inc.
 *
 * May be copied or modified under the terms of the GNU General Public
 * License.  See linux/COPYING for more information.
 *
 *
 * ChangeLog:
 *	14-Nov-2001 SHARP Corporation
 */
#include <linux/config.h>
#include <linux/module.h>
#include <linux/sched.h>
#include <linux/malloc.h>
#include <linux/interrupt.h>
#include <linux/random.h>
#include <linux/proc_fs.h>
#include <linux/delay.h>
#include <linux/init.h>

#include <asm/hardware.h>
#include <asm/ucb1200.h>


//#define DEBUG 1
#ifdef DEBUG
#define DPRINTK(x, args...)  printk(__FUNCTION__ ": " x,##args)
#define DPRINT_ADC() \
{\
	DPRINTK("adc_busy=%d\n", adc_busy);\
	DPRINTK("adc_id=%08x\n", adc_id);\
\
	DPRINTK("request_adc_id=%08x\n", request_adc_id);\
	DPRINTK("complete_adc=%d\n", complete_adc);\
	DPRINTK("adc_value=%d\n", adc_value);\
}
#else
#define DPRINTK(x, args...)
#define DPRINT_ADC()
#endif


static volatile long adc_busy;
static u32 adc_id = 0;
static spinlock_t adc_lock = SPIN_LOCK_UNLOCKED;
static DECLARE_WAIT_QUEUE_HEAD(adc_wait);


static u32 request_adc_id = 0xffffffff;
static volatile int adc_value;
static volatile long complete_adc;
static spinlock_t complete_adc_lock = SPIN_LOCK_UNLOCKED;
static DECLARE_WAIT_QUEUE_HEAD(complete_adc_wait);



int ucb1200_lock_adc(u32 id)
{
	unsigned long flags;
	int busy = 1;

	spin_lock_irqsave(&adc_lock, flags);
	if ( !adc_busy ) {
		adc_busy = 1;
		adc_id = id;
		busy = 0;
	}
	spin_unlock_irqrestore(&adc_lock, flags);
	if ( busy ) {
		if ( in_interrupt() ) {
			return -1;
		} else {
			DECLARE_WAITQUEUE(wait, current);
			add_wait_queue(&adc_wait, &wait);
			while (1) {
				set_current_state(TASK_INTERRUPTIBLE);
				spin_lock_irqsave(&adc_lock, flags);
                                if ( !adc_busy ) {
                                        adc_busy = 1;
					adc_id = id;
                                        spin_unlock_irqrestore(&adc_lock, flags);
                                        break;
                                }
				spin_unlock_irqrestore(&adc_lock, flags);

				DPRINTK("adc sleep %08x\n",id);
				DPRINT_ADC();

				schedule();

				DPRINTK("adc wakeup %08x\n",id);

				if ( signal_pending(current) ) {
					set_current_state(TASK_RUNNING);
					remove_wait_queue(&adc_wait, &wait);
					return -EINTR;
				}
			}
			set_current_state(TASK_RUNNING);
			remove_wait_queue(&adc_wait, &wait);
		}
	}
	return 0;
}

u32 ucb1200_get_adc_id(void)
{
	unsigned long flags;
	unsigned long id;

	spin_lock_irqsave(&adc_lock, flags);
	id = adc_id;
	spin_unlock_irqrestore(&adc_lock, flags);

	return id;
}

void ucb1200_unlock_adc(u32 id)
{
	unsigned long flags;

	spin_lock_irqsave(&adc_lock, flags);
	if (id != adc_id) {
		spin_unlock_irqrestore(&adc_lock, flags);
		return;
	}
	adc_busy = 0;
	adc_id = 0;
	spin_unlock_irqrestore(&adc_lock, flags);
	wake_up_interruptible(&adc_wait);
}




static void ucb1200_get_adc_value_interrupt(int irq, void *dummy, struct pt_regs *fp)
{
	unsigned long flags;

	if (ucb1200_get_adc_id() != request_adc_id)
		return;

	adc_value = ucb1200_read_adc();
	ucb1200_stop_adc();
	//ucb1200_disable_irq(IRQ_UCB1200_ADC);
	ucb1200_unlock_adc(request_adc_id);

	spin_lock_irqsave(&complete_adc_lock, flags);
	complete_adc = 1;
	request_adc_id = 0xffffffff;
	spin_unlock_irqrestore(&complete_adc_lock, flags);
	wake_up_interruptible(&complete_adc_wait);

}

int ucb1200_get_adc_value(u32 id, u16 input)
{
	unsigned long flags;
	long complete;

	if (ucb1200_lock_adc(id) < 0)
		return -1;
	request_adc_id = id;
	adc_value = -1;
	ucb1200_enable_irq(IRQ_UCB1200_ADC);
	spin_lock_irqsave(&complete_adc_lock, flags);
	complete_adc = 0;
	spin_unlock_irqrestore(&complete_adc_lock, flags);
	ucb1200_start_adc(input);

	spin_lock_irqsave(&complete_adc_lock, flags);
	complete = complete_adc;
	spin_unlock_irqrestore(&complete_adc_lock, flags);
	
	if ( !complete ) {
		DECLARE_WAITQUEUE(wait, current);
		add_wait_queue(&complete_adc_wait, &wait);
		while (1) {
			set_current_state(TASK_INTERRUPTIBLE);
			spin_lock_irqsave(&complete_adc_lock, flags);
			if ( complete_adc ) {
				spin_unlock_irqrestore(&complete_adc_lock, flags);
				break;
			}
			spin_unlock_irqrestore(&complete_adc_lock, flags);
			schedule();
			if ( signal_pending(current) ) {
				ucb1200_stop_adc();
				adc_value = -EINTR;
				break;
			}
		}
		set_current_state(TASK_RUNNING);
		remove_wait_queue(&complete_adc_wait, &wait);
	}
	return adc_value;
}

void ucb1200_cancel_get_adc_value(u32 id)
{
	DPRINTK("start ----\n");
	DPRINT_ADC();

	ucb1200_stop_adc();
	//ucb1200_disable_irq(IRQ_UCB1200_ADC);
	ucb1200_unlock_adc(id);

	complete_adc = 1;
	request_adc_id = 0xffffffff;
	wake_up_interruptible(&complete_adc_wait);

	DPRINTK("end ----\n");
	DPRINT_ADC();
}

int __init ucb1200_adc_exc_init(void) 
{
        static int ucb1200_adc_initialized = 0;


	if (ucb1200_adc_initialized)
	  return 0;

	if (ucb1200_request_irq(IRQ_UCB1200_ADC, ucb1200_get_adc_value_interrupt,
			SA_SHIRQ, "batterynb adc", ucb1200_get_adc_value_interrupt)) {
		printk("Could not get irq %d.\n", IRQ_UCB1200_ADC);
		return -1;
	}
	init_waitqueue_head(&complete_adc_wait);
	init_waitqueue_head(&adc_wait);

	ucb1200_adc_initialized = 1;

	return 0;
}

void ucb1200_adc_exc_cleanup()
{
	ucb1200_free_irq(IRQ_UCB1200_ADC, ucb1200_get_adc_value_interrupt);
}




int suspend_read_adc(int id, int num)
{
  int temp;

  request_adc_id = id;

  ucb1200_disable_irq(IRQ_UCB1200_ADC);

  udelay(700);
  ucb1200_start_adc(num);
  temp = ucb1200_read_adc();
  ucb1200_stop_adc();

  ucb1200_unlock_adc(id);
  ucb1200_cancel_get_adc_value(id);

  ucb1200_enable_irq(IRQ_UCB1200_ADC);

  return temp;
}


module_init(ucb1200_adc_exc_init);
module_exit(ucb1200_adc_exc_cleanup);
