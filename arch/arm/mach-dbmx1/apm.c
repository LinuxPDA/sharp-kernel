/*
 * linux/arch/arm/mach-dbmx1/apm.c
 *
 * bios-less APM driver for ARM Linux
 *
 * (C) Copyright 2003 Lineo uSolutions, Inc.
 *
 * May be copied or modified under the terms of the GNU General Public
 * License.  See linux/COPYING for more information.
 *
 * Based on:
 *   arch/arm/mach-sa1100/apm.c
 *   arch/arm/mach-pxa/lubbock_apm.c
 *
 *  Jamey Hicks <jamey@crl.dec.com>
 *  adapted from the APM BIOS driver for Linux by Stephen Rothwell
 *  (sfr@linuxcare.com)
 *
 * APM 1.2 Reference:
 *   Intel Corporation, Microsoft Corporation. Advanced Power Management
 *   (APM) BIOS Interface Specification, Revision 1.2, February 1996.
 *
 * [This document is available from Microsoft at:
 *    http://www.microsoft.com/hwdev/busbios/amp_12.htm]
 *
 */

#include <linux/config.h>
#include <linux/module.h>

#include <linux/poll.h>
#include <linux/types.h>
#include <linux/stddef.h>
#include <linux/timer.h>
#include <linux/fcntl.h>
#include <linux/slab.h>
#include <linux/stat.h>
#include <linux/proc_fs.h>
#include <linux/miscdevice.h>
#include <linux/apm_bios.h>
#include <linux/init.h>
#include <linux/sched.h>
#include <linux/pm.h>
#include <linux/kernel.h>
#include <linux/smp_lock.h>

#include <asm/system.h>
#include <asm/hardware.h>
#include <asm/irq.h>
#include <asm/arch/irqs.h>
#include <linux/ioctl.h>

#define DEBUG	1
#ifdef DEBUG
#define DPRINTK(x, args...)  printk(__FUNCTION__ ": " x,##args)
#else
#define DPRINTK(x, args...)
#endif

static int dbmx1_suspend_request;	/* 1 : do_suspend / 0:  */

#ifdef CONFIG_MAGIC_SYSRQ
extern void (*sysrq_power_off)(void);
#endif

struct apm_bios_info apm_bios_info = {
        /* this driver simulates APM version 1.2 */
        version: 0x102,
        flags: APM_32_BIT_SUPPORT
};
struct apm_info apm_info;

/*
 * The apm_bios device is one of the misc char devices.
 * This is its minor number.
 */
#define	APM_MINOR_DEV	134

/*
 * See Documentation/Config.help for the configuration options.
 *
 * Various options can be changed at boot time as follows:
 * (We allow underscores for compatibility with the modules code)
 *	apm=on/off			enable/disable APM
 *	    [no-]debug			log some debugging messages
 *	    [no-]power[-_]off		power off on shutdown
 */


/*
 * Need to poll the APM BIOS every second
 */
#define APM_CHECK_TIMEOUT	(HZ)

/*
 * Ignore suspend events for this amount of time after a resume
 */
#define DEFAULT_BOUNCE_INTERVAL		(3 * HZ)

/*
 * Maximum number of events stored
 */
#define APM_MAX_EVENTS		20

/*
 * The per-file APM data
 */
struct apm_user {
	int		magic;
	struct apm_user *	next;
	int		suser: 1;
	int		suspend_wait: 1;
	int		suspend_result;
	int		suspends_pending;
	int		standbys_pending;
	int		suspends_read;
	int		standbys_read;
	int		event_head;
	int		event_tail;
	apm_event_t	events[APM_MAX_EVENTS];
};

static int		resume_notice = 0;
static int		is_goto_suspend = 0;

/*
 * The magic number in apm_user
 */
#define APM_BIOS_MAGIC		0x4101


/*
 * Local variables
 */
static int			suspends_pending;
static int			standbys_pending;
static int			waiting_for_resume;
static int			ignore_normal_resume;
static int			bounce_interval = DEFAULT_BOUNCE_INTERVAL;

#ifdef CONFIG_APM_RTC_IS_GMT
#	define	clock_cmos_diff	0
#else
static long			clock_cmos_diff;
#endif
static int			debug;
static int			apm_disabled;
static int			power_off = 1;
static int			exit_kapmd;
static int			kapmd_running;

static DECLARE_WAIT_QUEUE_HEAD(apm_waitqueue);
static DECLARE_WAIT_QUEUE_HEAD(apm_suspend_waitqueue);
static struct apm_user *	user_list;

static char	driver_version[] = "1.14";	/* no spaces */

static char *	apm_event_name[] = {
	"system standby",
	"system suspend",
	"normal resume",
	"critical resume",
	"low battery",
	"power status change",
	"update time",
	"critical suspend",
	"user standby",
	"user suspend",
	"system standby resume",
	"capabilities change"
};
#define NR_APM_EVENT_NAME	\
		(sizeof(apm_event_name) / sizeof(apm_event_name[0]))

typedef struct lookup_t {
	int	key;
	char *	msg;
} lookup_t;

static const lookup_t error_table[] = {
/* N/A	{ APM_SUCCESS,		"Operation succeeded" }, */
	{ APM_DISABLED,		"Power management disabled" },
	{ APM_CONNECTED,	"Real mode interface already connected" },
	{ APM_NOT_CONNECTED,	"Interface not connected" },
	{ APM_16_CONNECTED,	"16 bit interface already connected" },
/* N/A	{ APM_16_UNSUPPORTED,	"16 bit interface not supported" }, */
	{ APM_32_CONNECTED,	"32 bit interface already connected" },
	{ APM_32_UNSUPPORTED,	"32 bit interface not supported" },
	{ APM_BAD_DEVICE,	"Unrecognized device ID" },
	{ APM_BAD_PARAM,	"Parameter out of range" },
	{ APM_NOT_ENGAGED,	"Interface not engaged" },
	{ APM_BAD_FUNCTION,     "Function not supported" },
	{ APM_RESUME_DISABLED,	"Resume timer disabled" },
	{ APM_BAD_STATE,	"Unable to enter requested state" },
/* N/A	{ APM_NO_EVENTS,	"No events pending" }, */
	{ APM_NO_ERROR,		"BIOS did not set a return code" },
	{ APM_NOT_PRESENT,	"No APM present" }
};
#define ERROR_COUNT	(sizeof(error_table)/sizeof(lookup_t))

/*
 * Function 
 */
static int send_event(apm_event_t event);
static int suspend(void);
static int set_power_state(u_short what, u_short state);

static int __init apm_driver_version(u_short *val)
{
	*val = apm_bios_info.version;
	return APM_SUCCESS;
}

static void powerirq_handler(int irq, void *dev_id, struct pt_regs *regs)
{
	if (!dbmx1_suspend_request) {
		dbmx1_suspend_request = 1;
	}
}

static int apm_get_event(apm_event_t *event, apm_eventinfo_t *info)
{
	/* assigns *event and *info */
	*event = 0;
	*info = 0;

	/* Resume Notice */
	if (resume_notice) {
		DPRINTK("resume notice\n");
		resume_notice = 0;
		*event = APM_NORMAL_RESUME;
		return APM_SUCCESS;
	}
	if (is_goto_suspend)
		return APM_SUCCESS;

	/* Power Off (Suspend) Button */
	if (dbmx1_suspend_request) {
		/* power on/off button is depressed */
		DPRINTK("power button\n");
		dbmx1_suspend_request = 0;
		*event = APM_USER_SUSPEND;
		return APM_SUCCESS;
	} else {
		dbmx1_suspend_request = 0;
	}

	return APM_NO_EVENTS;
}

static int is_kernel_thread(struct task_struct* task)
{
	int ret;

	task_lock(task);
	ret = (task->mm == NULL);
	task_unlock(task);
	return ret;
}

static void send_sig_to_all_procs(int signo) /* SIGSTOP or SIGCONT */
{
	//static spinlock_t lock = SPIN_LOCK_UNLOCKED;
	struct task_struct* p = NULL;
	struct task_struct* tsk = current;

	if (! spin_trylock(&lock))
		return;
 
	/* send signal to all procs except for kernel-threads */
	read_lock(&tasklist_lock);
	for_each_task(p) {
		struct siginfo si;

		if (p->pid == 1 || p->pid == tsk->pid || is_kernel_thread(p))
			continue;

		si.si_signo = signo;
		si.si_errno = 0;
		si.si_code = SI_KERNEL;
		si.si_pid = tsk->pid;
		si.si_uid = tsk->uid;
		send_sig_info(signo, &si, p);
	}
	read_unlock(&tasklist_lock);

	if (signo == SIGSTOP) {
		int state;

	  retry:
		state = tsk->state;
		set_current_state(TASK_RUNNING);
		schedule();
		set_current_state(state);

		read_lock(&tasklist_lock);
		for_each_task(p) {
			if (p->pid == 1 || p->pid == tsk->pid || is_kernel_thread(p))
				continue;

			if (p->state != TASK_STOPPED) {
				read_unlock(&tasklist_lock);
				goto retry;
			}
		}
		read_unlock(&tasklist_lock);
	}

	spin_unlock(&lock);
}

static int set_power_state(u_short what, u_short state)
{
	DPRINTK("what=%d state=%d\n",what,state);

	switch(what) {
	case APM_DEVICE_ALL:
		switch(state) {
		case APM_STATE_SUSPEND:
			DPRINTK("*** go into suspend\n");
			extern int pm_do_suspend(void);
			pm_do_suspend();
			resume_notice = 1;
			break;
		case APM_STATE_OFF:
		case APM_STATE_STANDBY:
			DPRINTK("*** go into standby\n");
//			send_event(APM_STATE_STANDBY);
			extern int pm_do_standby(void);
			pm_do_standby();
			break;
		case APM_STATE_BUSY:
		case APM_STATE_REJECT:
			break;
		}
		break;
	case APM_DEVICE_DISPLAY:	// what & APM_DEVICE_CLASS:
		switch(state) {
		case APM_STATE_READY:
			send_event(APM_STATE_READY);
			break;
		case APM_STATE_STANDBY:
			send_event(APM_STATE_STANDBY);
			break;
		}
		break;
	}
	return APM_SUCCESS;
}

static int apm_set_power_state(u_short state)
{
	return set_power_state(APM_DEVICE_ALL, state);
}

#ifdef CONFIG_APM_CPU_IDLE
static int apm_do_idle(void)
{
	return 1;
}

static void apm_do_busy(void)
{
}
#endif

static void apm_power_off(void)
{
	apm_set_power_state(APM_STATE_OFF);
}

static int apm_engage_power_management(u_short device, int enable)
{
	DPRINTK("device=%d enable=%d\n", device, enable);

	if ((enable == 0) && (device == APM_DEVICE_ALL)
	    && (apm_info.bios.flags & APM_BIOS_DISABLED))
		return APM_DISABLED;

	if (device == APM_DEVICE_ALL) {
		if (enable)
			apm_info.bios.flags &= ~APM_BIOS_DISENGAGED;
		else
			apm_info.bios.flags |= APM_BIOS_DISENGAGED;
	}
	return APM_SUCCESS;
}

static void apm_error(char *str, int err)
{
	int	i;

	for (i = 0; i < ERROR_COUNT; i++)
		if (error_table[i].key == err) break;
	if (i < ERROR_COUNT)
		printk(KERN_NOTICE "apm: %s: %s\n", str, error_table[i].msg);
	else
		printk(KERN_NOTICE "apm: %s: unknown error code %#2.2x\n",
			str, err);
}

static int queue_empty(struct apm_user *as)
{
	return as->event_head == as->event_tail;
}

static apm_event_t get_queued_event(struct apm_user *as)
{
	as->event_tail = (as->event_tail + 1) % APM_MAX_EVENTS;

	DPRINTK("event=%d\n",as->events[as->event_tail]);

	return as->events[as->event_tail];
}

static void queue_event(apm_event_t event, struct apm_user *sender)
{
	struct apm_user *	as;

	DPRINTK("event=%d\n",event);

	if (user_list == NULL)
		return;
	for (as = user_list; as != NULL; as = as->next) {
		if (as == sender)
			continue;
		as->event_head = (as->event_head + 1) % APM_MAX_EVENTS;
		if (as->event_head == as->event_tail) {
		  static int notified;
		  if (notified++ == 0)
			    printk(KERN_ERR "apm: an event queue overflowed\n");
			as->event_tail = (as->event_tail + 1) % APM_MAX_EVENTS;
		}
		as->events[as->event_head] = event;
		if (!as->suser)
			continue;
		switch (event) {
		case APM_SYS_SUSPEND:
		case APM_USER_SUSPEND:
			as->suspends_pending++;
			suspends_pending++;
			break;

		case APM_SYS_STANDBY:
		case APM_USER_STANDBY:
			as->standbys_pending++;
			standbys_pending++;
			break;
		}
	}
	wake_up_interruptible(&apm_waitqueue);
}

static unsigned long get_cmos_time(void)
{
	return (unsigned long)(0);
}

static void set_time(void)
{
	unsigned long	flags;

	save_flags(flags);
	cli();
	CURRENT_TIME = get_cmos_time() + clock_cmos_diff;
	restore_flags(flags);
}

static void get_time_diff(void)
{
#ifndef CONFIG_APM_RTC_IS_GMT
	unsigned long	flags;

	/*
	 * Estimate time zone so that set_time can update the clock
	 */
	save_flags(flags);
	clock_cmos_diff = -get_cmos_time();
	cli();
	clock_cmos_diff += CURRENT_TIME;
	restore_flags(flags);
#endif
}

static int resume_handling = 0;

static int send_event(apm_event_t event)
{
	DPRINTK("event=%d\n",event);

	switch (event) {
	case APM_SYS_SUSPEND:
	case APM_CRITICAL_SUSPEND:
	case APM_USER_SUSPEND:
		is_goto_suspend = 1;
		send_sig_to_all_procs(SIGSTOP);
		/* map all suspends to ACPI D3 */
		if (pm_send_all(PM_SUSPEND, (void *)3)) {
			if (event == APM_CRITICAL_SUSPEND) {
				printk(KERN_CRIT "apm: Critical suspend was vetoed, expect armageddon\n" );
				return 0;
			}
			if (apm_info.connection_version > 0x100)
				apm_set_power_state(APM_STATE_REJECT);
			return 0;
		}
		break;
	case APM_NORMAL_RESUME:
	case APM_CRITICAL_RESUME:
		/* map all resumes to ACPI D0 */
		if (resume_handling)
			return 1;
		resume_handling = 1;
		(void) pm_send_all(PM_RESUME, (void *)0);
		is_goto_suspend = 0;
		send_sig_to_all_procs(SIGCONT);
		resume_handling = 0;
		break;
	case APM_STATE_STANDBY:
	case APM_STATE_READY:
		break;
	}

	return 1;
}

static int suspend(void)
{
	int		err;
	struct apm_user	*as;

	DPRINTK("\n");

	get_time_diff();
	cli();
	err = apm_set_power_state(APM_STATE_SUSPEND);
	set_time();
	if (err == APM_NO_ERROR)
		err = APM_SUCCESS;
	if (err != APM_SUCCESS)
		apm_error("suspend", err);
	send_event(APM_NORMAL_RESUME);
	sti();
	queue_event(APM_NORMAL_RESUME, NULL);
	for (as = user_list; as != NULL; as = as->next) {
		as->suspend_wait = 0;
		as->suspend_result = ((err == APM_SUCCESS) ? 0 : -EIO);
	}
	ignore_normal_resume = 1;
	wake_up_interruptible(&apm_suspend_waitqueue);
	return err;
}

static void standby(void)
{
	int	err;

	DPRINTK("\n");

	get_time_diff();
	err = apm_set_power_state(APM_STATE_STANDBY);
	if ((err != APM_SUCCESS) && (err != APM_NO_ERROR))
		apm_error("standby", err);
}

static apm_event_t get_event(void)
{
	int		error;
	apm_event_t	event;
	apm_eventinfo_t	info;

	static int notified;

	/* we don't use the eventinfo */
	error = apm_get_event(&event, &info);
	if (error == APM_SUCCESS)
		return event;

	if ((error != APM_NO_EVENTS) && (notified++ == 0))
		apm_error("get_event", error);

	return 0;
}

static void check_events(void)
{
	apm_event_t		event;
	static unsigned long	last_resume;
	static int		ignore_bounce;

	while ((event = get_event()) != 0) {
		if (debug) {
			if (event <= NR_APM_EVENT_NAME)
				printk(KERN_DEBUG "apm: received %s notify\n",
				       apm_event_name[event - 1]);
			else
				printk(KERN_DEBUG "apm: received unknown "
				       "event 0x%02x\n", event);
		}
		if (ignore_bounce
		    && ((jiffies - last_resume) > bounce_interval))
			ignore_bounce = 0;
		if (ignore_normal_resume && (event != APM_NORMAL_RESUME))
			ignore_normal_resume = 0;

		switch (event) {
		case APM_SYS_STANDBY:
		case APM_USER_STANDBY:
			if (send_event(event)) {
				queue_event(event, NULL);
				if (standbys_pending <= 0)
					standby();
			}
			break;

		case APM_USER_SUSPEND:
#ifdef CONFIG_APM_IGNORE_USER_SUSPEND
			if (apm_info.connection_version > 0x100)
				apm_set_power_state(APM_STATE_REJECT);
			break;
#endif
		case APM_SYS_SUSPEND:
			if (ignore_bounce) {
				if (apm_info.connection_version > 0x100)
					apm_set_power_state(APM_STATE_REJECT);
				break;
			}
			/*
			 * If we are already processing a SUSPEND,
			 * then further SUSPEND events from the BIOS
			 * will be ignored.  We also return here to
			 * cope with the fact that the Thinkpads keep
			 * sending a SUSPEND event until something else
			 * happens!
			 */
			if (waiting_for_resume)
				return;
			if (send_event(event)) {
				queue_event(event, NULL);
				waiting_for_resume = 1;
				if (suspends_pending <= 0)
					(void) suspend();
			}
			break;

		case APM_NORMAL_RESUME:
		case APM_CRITICAL_RESUME:
		case APM_STANDBY_RESUME:
			waiting_for_resume = 0;
			last_resume = jiffies;
			ignore_bounce = 1;
			if ((event != APM_NORMAL_RESUME)
			    || (ignore_normal_resume == 0)) {
				set_time();
				send_event(event);
				queue_event(event, NULL);
			}
			break;

		case APM_CAPABILITY_CHANGE:
		case APM_LOW_BATTERY:
		case APM_POWER_STATUS_CHANGE:
			send_event(event);
			queue_event(event, NULL);
			break;

		case APM_UPDATE_TIME:
			set_time();
			break;

		case APM_CRITICAL_SUSPEND:
			send_event(event);
			/*
			 * We can only hope it worked - we are not allowed
			 * to reject a critical suspend.
			 */
			(void) suspend();
			break;
		}
	}
}

static void apm_event_handler(void)
{
	static int	pending_count = 4;
	int		err;

	if ((standbys_pending > 0) || (suspends_pending > 0)) {
		if ((apm_info.connection_version > 0x100) && (pending_count-- <= 0)) {
			pending_count = 4;
			if (debug)
				printk(KERN_DEBUG "apm: setting state busy\n");
			err = apm_set_power_state(APM_STATE_BUSY);
			if (err)
				apm_error("busy", err);
		}
	} else
		pending_count = 4;
	check_events();
}

/*
 * This is the APM thread main loop.
 *
 * Check whether we're the only running process to
 * decide if we should just power down.
 *
 */
static void apm_mainloop(void)
{
	int timeout = HZ;
	DECLARE_WAITQUEUE(wait, current);

	add_wait_queue(&apm_waitqueue, &wait);
	set_current_state(TASK_INTERRUPTIBLE);
	for (;;) {
		/* Nothing to do, just sleep for the timeout */
		timeout = 2*timeout;
		if (timeout > APM_CHECK_TIMEOUT)
			timeout = APM_CHECK_TIMEOUT;
		schedule_timeout(timeout);
		if (exit_kapmd)
			break;

		/*
		 * Ok, check all events, check for idle (and mark us sleeping
		 * so as not to count towards the load average)..
		 */
		set_current_state(TASK_INTERRUPTIBLE);
		apm_event_handler();
#ifdef CONFIG_APM_CPU_IDLE
		if (!(nr_running == 1))
			continue;

		if (apm_do_idle()) {
			unsigned long start = jiffies;
			while ((!exit_kapmd) && (nr_running == 1)) {
				apm_do_idle();
				if ((jiffies - start) > APM_CHECK_TIMEOUT) {
					apm_event_handler();
					start = jiffies;
				}
			}
			apm_do_busy();
			apm_event_handler();
			timeout = 1;
		}
#endif
	}
	remove_wait_queue(&apm_waitqueue, &wait);
}

static int check_apm_user(struct apm_user *as, const char *func)
{
	if ((as == NULL) || (as->magic != APM_BIOS_MAGIC)) {
		printk(KERN_ERR "apm: %s passed bad filp\n", func);
		return 1;
	}
	return 0;
}

static ssize_t do_read(struct file *fp, char *buf, size_t count, loff_t *ppos)
{
	struct apm_user *	as;
	int			i;
	apm_event_t		event;
	DECLARE_WAITQUEUE(wait, current);

	as = fp->private_data;
	if (check_apm_user(as, "read"))
		return -EIO;
	if (count < sizeof(apm_event_t))
		return -EINVAL;
	if (queue_empty(as)) {
		if (fp->f_flags & O_NONBLOCK)
			return -EAGAIN;
		add_wait_queue(&apm_waitqueue, &wait);
		DPRINTK("do_read: waiting\n");
repeat:
		set_current_state(TASK_INTERRUPTIBLE);
		if (queue_empty(as) && !signal_pending(current)) {
			schedule();
			goto repeat;
		}
		set_current_state(TASK_RUNNING);
		remove_wait_queue(&apm_waitqueue, &wait);
	}
	i = count;
	while ((i >= sizeof(event)) && !queue_empty(as)) {
		event = get_queued_event(as);
		if (copy_to_user(buf, &event, sizeof(event))) {
			if (i < count)
				break;
			return -EFAULT;
		}
		switch (event) {
		case APM_SYS_SUSPEND:
		case APM_USER_SUSPEND:
			as->suspends_read++;
			break;

		case APM_SYS_STANDBY:
		case APM_USER_STANDBY:
			as->standbys_read++;
			break;
		}
		buf += sizeof(event);
		i -= sizeof(event);
	}
	if (i < count)
		return count - i;
	if (signal_pending(current))
		return -ERESTARTSYS;
	return 0;
}

static unsigned int do_poll(struct file *fp, poll_table * wait)
{
	struct apm_user * as;

	as = fp->private_data;
	if (check_apm_user(as, "poll"))
		return 0;
	poll_wait(fp, &apm_waitqueue, wait);
	if (!queue_empty(as))
		return POLLIN | POLLRDNORM;
	return 0;
}

static int do_ioctl(struct inode * inode, struct file *filp,
		    u_int cmd, u_long arg)
{
	struct apm_user *	as;
	DECLARE_WAITQUEUE(wait, current);

	as = filp->private_data;
	if (check_apm_user(as, "ioctl"))
		return -EIO;
	if (!as->suser)
		return -EPERM;
	switch (cmd) {
	case APM_IOC_STANDBY:
		if (as->standbys_read > 0) {
			as->standbys_read--;
			as->standbys_pending--;
			standbys_pending--;
		} else if (!send_event(APM_USER_STANDBY))
			return -EAGAIN;
		else
			queue_event(APM_USER_STANDBY, as);
		if (standbys_pending <= 0)
			standby();
		break;
	case APM_IOC_SUSPEND:
		if (as->suspends_read > 0) {
			as->suspends_read--;
			as->suspends_pending--;
			suspends_pending--;
		} else if (!send_event(APM_USER_SUSPEND))
			return -EAGAIN;
		else
			queue_event(APM_USER_SUSPEND, as);
		if (suspends_pending <= 0) {
			if (suspend() != APM_SUCCESS)
				return -EIO;
		} else {
			as->suspend_wait = 1;
			add_wait_queue(&apm_suspend_waitqueue, &wait);
			while (1) {
				set_current_state(TASK_INTERRUPTIBLE);
				if ((as->suspend_wait == 0)
				    || signal_pending(current))
					break;
				schedule();
			}
			set_current_state(TASK_RUNNING);
			remove_wait_queue(&apm_suspend_waitqueue, &wait);
			return as->suspend_result;
		}
		break;
	default:
		return -EINVAL;
	}
	return 0;
}

static int do_release(struct inode * inode, struct file * filp)
{
	struct apm_user *	as;

	as = filp->private_data;
	if (check_apm_user(as, "release"))
		return 0;
	filp->private_data = NULL;
	lock_kernel();
	if (as->standbys_pending > 0) {
		standbys_pending -= as->standbys_pending;
		if (standbys_pending <= 0)
			standby();
	}
	if (as->suspends_pending > 0) {
		suspends_pending -= as->suspends_pending;
		if (suspends_pending <= 0)
			(void) suspend();
	}
	if (user_list == as)
		user_list = as->next;
	else {
		struct apm_user *	as1;

		for (as1 = user_list;
		     (as1 != NULL) && (as1->next != as);
		     as1 = as1->next)
			;
		if (as1 == NULL)
			printk(KERN_ERR "apm: filp not in user list\n");
		else
			as1->next = as->next;
	}
	unlock_kernel();
	kfree(as);
	return 0;
}

static int do_open(struct inode * inode, struct file * filp)
{
	struct apm_user *	as;

	as = (struct apm_user *)kmalloc(sizeof(*as), GFP_KERNEL);
	if (as == NULL) {
		printk(KERN_ERR "apm: cannot allocate struct of size %d bytes\n",
		       sizeof(*as));
		return -ENOMEM;
	}
	as->magic = APM_BIOS_MAGIC;
	as->event_tail = as->event_head = 0;
	as->suspends_pending = as->standbys_pending = 0;
	as->suspends_read = as->standbys_read = 0;
	/*
	 * XXX - this is a tiny bit broken, when we consider BSD
         * process accounting. If the device is opened by root, we
	 * instantly flag that we used superuser privs. Who knows,
	 * we might close the device immediately without doing a
	 * privileged operation -- cevans
	 */
	as->suser = capable(CAP_SYS_ADMIN);
	as->next = user_list;
	user_list = as;
	filp->private_data = as;
	return 0;
}

static int apm_get_info(char *buf, char **start, off_t fpos, int length)
{
	char *p = buf;

	/* Arguments, with symbols from linux/apm_bios.h.  Information is
	   from the Get Power Status (0x0a) call unless otherwise noted.

	   0) Linux driver version (this will change if format changes)
	   1) APM BIOS Version.  Usually 1.0, 1.1 or 1.2.
	   2) APM flags from APM Installation Check (0x00):
	      bit 0: APM_16_BIT_SUPPORT
	      bit 1: APM_32_BIT_SUPPORT
	      bit 2: APM_IDLE_SLOWS_CLOCK
	      bit 3: APM_BIOS_DISABLED
	      bit 4: APM_BIOS_DISENGAGED */

	p += sprintf(p, "%s %d.%d 0x%02x\n",
		     driver_version,
		     (apm_info.bios.version >> 8) & 0xff,
		     apm_info.bios.version & 0xff,
		     apm_info.bios.flags);

	return p - buf;
}

static int apm(void *unused)
{
	int		error;

	kapmd_running = 1;

	exit_files(current);	/* daemonize doesn't do exit_files */
	current->files = init_task.files;
	atomic_inc(&current->files->count);
	daemonize();

	strcpy(current->comm, "kapm-idled");
	sigfillset(&current->blocked);
	current->tty = NULL;	/* get rid of controlling tty */

	if (apm_info.connection_version == 0) {
		apm_info.connection_version = apm_info.bios.version;
		if (apm_info.connection_version > 0x100) {
			/*
			 * We only support BIOSs up to version 1.2
			 */
			if (apm_info.connection_version > 0x0102)
				apm_info.connection_version = 0x0102;
			error = apm_driver_version(&apm_info.connection_version);
			if (error != APM_SUCCESS) {
				apm_error("driver version", error);
				/* Fall back to an APM 1.0 connection. */
				apm_info.connection_version = 0x100;
			}
		}
	}

	if (debug)
		printk(KERN_INFO "apm: Connection version %d.%d\n",
			(apm_info.connection_version >> 8) & 0xff,
			apm_info.connection_version & 0xff);

	if ((apm_info.bios.flags & APM_BIOS_DISENGAGED)
	    && (apm_info.connection_version > 0x0100)) {
		error = apm_engage_power_management(APM_DEVICE_ALL, 1);
		if (error) {
			apm_error("engage power management", error);
			return -1;
		}
	}

	/* Install our power off handler.. */
	if (power_off)
		pm_power_off = apm_power_off;
#ifdef CONFIG_MAGIC_SYSRQ
	sysrq_power_off = apm_power_off;
#endif
	if (smp_num_cpus == 1)
		apm_mainloop();

	kapmd_running = 0;

	return 0;
}

#ifndef MODULE
static int __init apm_setup(char *str)
{
	int	invert;

	while ((str != NULL) && (*str != '\0')) {
		if (strncmp(str, "off", 3) == 0)
			apm_disabled = 1;
		if (strncmp(str, "on", 2) == 0)
			apm_disabled = 0;
		invert = (strncmp(str, "no-", 3) == 0);
		if (invert)
			str += 3;
		if (strncmp(str, "debug", 5) == 0)
			debug = !invert;
		if ((strncmp(str, "power-off", 9) == 0) ||
		    (strncmp(str, "power_off", 9) == 0))
			power_off = !invert;
		if ((strncmp(str, "bounce-interval=", 16) == 0) ||
		    (strncmp(str, "bounce_interval=", 16) == 0))
			bounce_interval = simple_strtol(str + 16, NULL, 0);
		str = strchr(str, ',');
		if (str != NULL)
			str += strspn(str, ", \t");
	}
	return 1;
}

__setup("apm=", apm_setup);
#endif

static struct file_operations apm_bios_fops = {
	owner:		THIS_MODULE,
	read:		do_read,
	poll:		do_poll,
	ioctl:		do_ioctl,
	open:		do_open,
	release:	do_release,
};

static struct miscdevice apm_device = {
	APM_MINOR_DEV,
	"apm_bios",
	&apm_bios_fops
};

/*
 * Just start the APM thread. We do NOT want to do APM BIOS
 * calls from anything but the APM thread, if for no other reason
 * than the fact that we don't trust the APM BIOS. This way,
 * most common APM BIOS problems that lead to protection errors
 * etc will have at least some level of being contained...
 *
 * In short, if something bad happens, at least we have a choice
 * of just killing the apm thread..
 */
static int __init apm_init(void)
{
	struct proc_dir_entry *apm_proc;

	apm_info.bios = apm_bios_info;
	if (apm_info.bios.version == 0) {
		printk(KERN_INFO "apm: BIOS not found.\n");
		return -ENODEV;
	}
	printk(KERN_INFO
		"apm: BIOS version %d.%d Flags 0x%02x (Driver version %s)\n",
		((apm_info.bios.version >> 8) & 0xff),
		(apm_info.bios.version & 0xff),
		apm_info.bios.flags,
		driver_version);
	if ((apm_info.bios.flags & APM_32_BIT_SUPPORT) == 0) {
		printk(KERN_INFO "apm: no 32 bit BIOS support\n");
		return -ENODEV;
	}

	/*
	 * Fix for the Compaq Contura 3/25c which reports BIOS version 0.1
	 * but is reportedly a 1.0 BIOS.
	 */
	if (apm_info.bios.version == 0x001)
		apm_info.bios.version = 0x100;

	if (apm_disabled) {
		printk(KERN_NOTICE "apm: disabled on user request.\n");
		return -ENODEV;
	}
	if ((smp_num_cpus > 1) && !power_off) {
		printk(KERN_NOTICE "apm: disabled - APM is not SMP safe.\n");
		return -ENODEV;
	}
	if (PM_IS_ACTIVE()) {
		printk(KERN_NOTICE "apm: overridden by ACPI.\n");
		return -ENODEV;
	}
	pm_active = 1;

	standbys_pending = 0;
	suspends_pending = 0;
	dbmx1_suspend_request = 0;

/*
	set_GPIO_IRQ_edge(IRQ_TO_GPIO(IRQ_GPIO1), GPIO_RISING_EDGE);
	if (request_irq(IRQ_GPIO1,
			powerirq_handler, SA_INTERRUPT, "apm", powerirq_handler)) {
		printk(KERN_NOTICE "apm: Couldn't get irq %d.\n", IRQ_GPIO1);
		return -ENODEV;
	}
*/

	apm_proc = create_proc_info_entry("apm", 0, NULL, apm_get_info);
	if (apm_proc)
		SET_MODULE_OWNER(apm_proc);

	kernel_thread(apm, NULL, CLONE_FS | CLONE_FILES | CLONE_SIGHAND | SIGCHLD);

	if (smp_num_cpus > 1) {
		printk(KERN_NOTICE
		   "apm: disabled - APM is not SMP safe (power off active).\n");
		return 0;
	}

	misc_register(&apm_device);

	return 0;
}

static void __exit apm_exit(void)
{
	int	error;

	if (((apm_info.bios.flags & APM_BIOS_DISENGAGED) == 0)
	    && (apm_info.connection_version > 0x0100)) {
		error = apm_engage_power_management(APM_DEVICE_ALL, 0);
		if (error)
			apm_error("disengage power management", error);
	}
	misc_deregister(&apm_device);
	remove_proc_entry("apm", NULL);

#ifdef CONFIG_MAGIC_SYSRQ
	sysrq_power_off = NULL;
#endif
	if (power_off)
		pm_power_off = NULL;
	exit_kapmd = 1;
	while (kapmd_running)
		schedule();
	pm_active = 0;
}


module_init(apm_init);
module_exit(apm_exit);

MODULE_AUTHOR("Stephen Rothwell");
MODULE_DESCRIPTION("Advanced Power Management");
MODULE_PARM(debug, "i");
MODULE_PARM_DESC(debug, "Enable debug mode");
MODULE_PARM(power_off, "i");
MODULE_PARM_DESC(power_off, "Enable power off");
MODULE_PARM(bounce_interval, "i");
MODULE_PARM_DESC(bounce_interval, "Set the number of ticks to ignore suspend bounces");
