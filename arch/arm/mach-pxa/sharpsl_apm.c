/*
 * linux/arch/arm/mach-pxa/sharpsl_apm.c
 *
 * bios-less APM driver for ARM Linux
 *
 * (C) Copyright 2001 Lineo Japan, Inc.
 *
 * May be copied or modified under the terms of the GNU General Public
 * License.  See linux/COPYING for more information.
 *
 * Based on:
 *   arch/arm/mach-sa1100/apm.c
 *   arch/arm/mach-sa1100/collie_apm.c
 *   arch/arm/mach-cotulla/discovery_apm.c
 *
 * Copyright (C) 2002 Steve Lin (stevelin168@hotmail.com)
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
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
 *
 * ChangeLog:
 *	25-APR-2002 SHARP Corporation for Discovery
 *	27-AUG-2002 Steve Lin for Discovery
 *	31-Jul-2002 Lineo Japan, Inc.  for ARCH_PXA
 *	07-Aug-2002 Lineo Japan, Inc.  for Poodle
 *	12-Dec-2002 Sharp Corporation for Poodle and Corgi
 *	16-Jan-2003 SHARP sleep_on -> interruptible_sleep_on
 *	13-Mar-2003 SHARP for PXA255
 *      29-Jan-2004 Sharp Corporation for Tosa
 *      26-Feb-2004 Lineo Solutions, Inc.  for Tosa
 *	18-Oct-2004 Lineo Solutions, Inc.  for Spitz
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

#ifdef CONFIG_ARCH_SHARP_SL
#include <asm/irq.h>
#include <asm/arch/irqs.h>
#ifdef CONFIG_SABINAL_DISCOVERY
#include <asm/arch/discovery_asic3.h>
#endif
#include <linux/ioctl.h>
#include <asm/sharp_apm.h>
#include <asm/sharp_char.h>
#include <linux/delay.h>
#include <asm/arch/ads7846_ts.h>
#include <linux/apm_bios.h>
#include <linux/time.h>
#include <asm/arch/keyboard.h>
#include <asm/sharp_keycode.h>
#define  KBUP    (0x80)
#define  KBDOWN  (0)

#ifdef CONFIG_APM_CPU_IDLE
#if defined(CONFIG_ARCH_PXA_POODLE) || defined(CONFIG_ARCH_PXA_CORGI) || defined(CONFIG_ARCH_PXA_TOSA)
#define SHARPSL_NEW_IDLE
#endif
#endif

#if defined(CONFIG_SABINAL_DISCOVERY)
extern int discovery_get_main_battery(void);
#define	get_main_battery discovery_get_main_battery
#elif defined(CONFIG_ARCH_PXA_POODLE) || defined(CONFIG_ARCH_PXA_CORGI) || defined(CONFIG_ARCH_PXA_TOSA)

extern int sharpsl_get_main_battery(void);
#define	get_main_battery sharpsl_get_main_battery

#if defined(CONFIG_ARCH_PXA_TOSA)
extern int sharpsl_jacket_battery;
extern int sharpsl_jacket_exist;
extern int sharpsl_get_cardslot_error(void);
#endif

#ifdef SHARPSL_NEW_IDLE
static int chg_freq_mode = 0;
#endif
unsigned long cccr_reg = 0;
extern unsigned int sharpsl_chg_freq;
extern int sharpsl_off_mode;
#endif
static int sharpsl_suspend_request;	/* 1 : do_suspend / 0:  */
extern int charge_status;		/* charge status  1 : charge  0: not charge */
extern unsigned int APOCnt;
extern unsigned int LPOCnt;
#if defined(CONFIG_SABINAL_DISCOVERY)
extern unsigned long	Init_Voltage;
extern int discovery_read_MBattery(void);
extern int		HWR_flag;
#endif

#if defined(CONFIG_SABINAL_DISCOVERY)
#define SHARPSL_AC_LINE_STATUS (( ASIC3_GPIO_PSTS_D & AC_IN )? APM_AC_OFFLINE : APM_AC_ONLINE)
#define BACKPACK_IN_DETECT()	( ASIC3_GPIO_PSTS_D & BACKPACK_DETECT ) /* 0: exist , 1: not in */
#else
#define SHARPSL_BATTERY_OK	(( GPLR(GPIO_MAIN_BAT_LOW) & GPIO_bit(GPIO_MAIN_BAT_LOW) ) ? 1 : 0)	/* 1: OK / 0: FATAL */
#define	SHARPSL_AC_LINE_STATUS	(AC_IN_STATUS ? APM_AC_ONLINE : APM_AC_OFFLINE)
#endif


/// ioctl 
#if defined(CONFIG_ARCH_PXA_POODLE) || defined(CONFIG_ARCH_PXA_CORGI) || defined(CONFIG_ARCH_PXA_TOSA)
static u32 apm_event_mask = (APM_EVT_POWER_BUTTON);
#else
static u32 apm_event_mask = (APM_EVT_POWER_BUTTON | APM_EVT_BATTERY_STATUS);
#endif

//Global
int autoPowerCancel = 0;
int autoLightCancel = 0;
int daemon_mode     = 0;
unsigned int sharpsl_get_on_mode;
int idleCancel	    = 0;

static int sleeping = 0;
#if defined(CONFIG_ARCH_PXA_CORGI) && !defined(CONFIG_ARCH_PXA_SPITZ)
#define HINGE_STABLE_COUNT 10
int sharpsl_hinge_state = 0;
static int hinge_count = HINGE_STABLE_COUNT;
#endif
#if defined(CONFIG_ARCH_PXA_SPITZ)
//Xint sharpsl_hinge_state = 0;
#include <asm/arch/sharpsl_wakeup.h>
#endif
#endif

#ifdef DEBUG
#define DPRINTK(x, args...)  printk(__FUNCTION__ ": " x,##args)
#else
#define DPRINTK(x, args...)
#endif

#ifdef CONFIG_MAGIC_SYSRQ
extern void (*sysrq_power_off)(void);
#endif
#if defined(CONFIG_APM_DISPLAY_BLANK) && defined(CONFIG_VT)
extern int (*console_blank_hook)(int);
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
 * Define to always call the APM BIOS busy routine even if the clock was
 * not slowed by the idle routine.
 */
#define ALWAYS_CALL_BUSY

/*
 * Define to re-initialize the interrupt 0 timer to 100 Hz after a suspend.
 * This patched by Chad Miller <cmiller@surfsouth.com>, original code by
 * David Chen <chen@ctpa04.mit.edu>
 */
#undef INIT_TIMER_AFTER_SUSPEND

#ifdef INIT_TIMER_AFTER_SUSPEND
#include <linux/timex.h>
#include <asm/io.h>
#include <linux/delay.h>
#endif

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
static int		blank_request = 0;
static u_short		blank_state;

/*
 * The magic number in apm_user
 */
#define APM_BIOS_MAGIC		0x4101


/*
 * Local variables
 */
#ifdef CONFIG_APM_CPU_IDLE
static int			clock_slowed;
#endif
static int			suspends_pending;
static int			standbys_pending;
static int			waiting_for_resume,btf_waiting_for_resume;
static int			ignore_normal_resume;
static int			bounce_interval = DEFAULT_BOUNCE_INTERVAL;


#ifdef CONFIG_APM_RTC_IS_GMT
#	define	clock_cmos_diff	0
#	define	got_clock_diff	1
#else
static long			clock_cmos_diff;
static int			got_clock_diff;
#endif
static int			debug;
static int			apm_disabled;
#ifdef CONFIG_SMP
static int			power_off;
#else
static int			power_off = 1;
#endif
static int			exit_kapmd;
static int			kapmd_running;

static DECLARE_WAIT_QUEUE_HEAD(apm_waitqueue);
static DECLARE_WAIT_QUEUE_HEAD(apm_suspend_waitqueue);
static DECLARE_WAIT_QUEUE_HEAD(lock_fcs_waitqueue);

//sample code from Yamade-san
#define KEY_TICK         		( 100 / 10 )  // 100msec
#define FLONT_LIGHT_TOGGLE_TIME         ( 2000 / 20 ) // 2sec
static DECLARE_WAIT_QUEUE_HEAD(fl_key);
static DECLARE_WAIT_QUEUE_HEAD(queue);
static __u32  on_press_time=0;

static struct apm_user *	user_list;

static char			driver_version[] = "1.14";	/* no spaces */

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
#ifndef CONFIG_SABINAL_DISCOVERY
extern int sharpsl_main_battery;
#ifdef CONFIG_ARCH_PXA_TOSA
extern int sharpsl_bu_battery;
#endif
#endif
static int apm_get_power_status(u_char *ac_line_status,
                                u_char *battery_status,
                                u_char *battery_flag,
                                u_char *battery_percentage,
                                u_short *battery_life);

static int __init apm_driver_version(u_short *val)
{
	*val = apm_bios_info.version;
	return APM_SUCCESS;
}

#ifdef CONFIG_SABINAL_DISCOVERY

static void powerirq_handler(int irq, void *dev_id, struct pt_regs *regs)
{

	if (irq == IRQ_ASIC3_PWR_ON) {	
						
		if ((ASIC3_GPIO_PSTS_A & PWR_ON_KEY) != 0) {	//key up 

			if ( ( jiffies - on_press_time ) < 0 ) {

				if ( ( jiffies + (0xffffffff - on_press_time) ) < FLONT_LIGHT_TOGGLE_TIME ) {

					/* suspend */
					ASIC3_GPIO_ETSEL_A &= ~(PWR_ON_KEY);  //falling edge trigger
					ASIC3_GPIO_MASK_A &= ~(PWR_ON_KEY);
					ICMR |= (1 << 8);

					if (sleeping || btf_waiting_for_resume) return;
			
					if ( apm_event_mask & APM_EVT_POWER_BUTTON ) {
						sharpsl_suspend_request = 1;
					} else {
						handle_scancode(SLKEY_OFF|KBDOWN , 1);
						handle_scancode(SLKEY_OFF|KBUP   , 0);
					}
					sleeping = 1;
				}
			} else {
				if ( ( jiffies - on_press_time ) < FLONT_LIGHT_TOGGLE_TIME ) {

					/* suspend */
					ASIC3_GPIO_ETSEL_A &= ~(PWR_ON_KEY);  //falling edge trigger
					ASIC3_GPIO_MASK_A &= ~(PWR_ON_KEY);
					ICMR |= (1 << 8);

					if (sleeping || btf_waiting_for_resume) return;
			
					if ( apm_event_mask & APM_EVT_POWER_BUTTON ) {
						sharpsl_suspend_request = 1;
					} else {
						handle_scancode(SLKEY_OFF|KBDOWN , 1);
						handle_scancode(SLKEY_OFF|KBUP   , 0);
					}
					sleeping = 1;
			}

			on_press_time = 0;

			ASIC3_GPIO_ETSEL_A &= ~(PWR_ON_KEY); //falling edge trigger

			}

		} else { //key down
			
			mdelay(1); //wait for stable 

			if ((ASIC3_GPIO_PSTS_A & PWR_ON_KEY) == 0) {	//key down
				on_press_time = jiffies;
				wake_up(&fl_key);

				ASIC3_GPIO_ETSEL_A |= (PWR_ON_KEY);  //raising edge trigger
			} else {
				on_press_time = 0;
			}
			
		}
						
	}
	
}

#else	// CONFIG_SABINAL_DISCOVERY

static void powerirq_handler(int irq, void *dev_id, struct pt_regs *regs)
{
	static __u32 count;

#if defined(CONFIG_ARCH_PXA_POODLE)
	if (irq == IRQ_GPIO_ON_KEY) {	/* suspend */
		//DPRINTK("irq=%d count=%d sharpsl_suspend_request%d\n",irq, count, sharpsl_suspend_request);
		if ( GPLR(GPIO_ON_KEY) & GPIO_bit(GPIO_ON_KEY) ) {
			/* release */
			count = 0;
		} else {
			/* Push */
			if (count++  > 150 && !sharpsl_suspend_request) {
				count = 0;
				sharpsl_suspend_request = 1;
			}
		}
	}
#endif
}

#endif

static int apm_get_event(apm_event_t *event, apm_eventinfo_t *info)
{
	/* assigns *event and *info */
	*event = 0;
	*info = 0;

	/* console blank */
	if ( blank_request ) {
		/* Blank the first display device */
		set_power_state(APM_DEVICE_DISPLAY, blank_state);
		blank_request = 0;
	}
	/* Resume Notice */
	if (resume_notice) {
		DPRINTK("resume notice\n");
		resume_notice = 0;
		*event = APM_NORMAL_RESUME;
		return APM_SUCCESS;
	}
	if (is_goto_suspend)
		return APM_SUCCESS;

#ifdef CONFIG_ARCH_SHARP_SL
	/* Power Off (Suspend) Button */
	if ( (apm_event_mask & APM_EVT_POWER_BUTTON ) && sharpsl_suspend_request) {
		/* power on/off button is depressed */
		DPRINTK("power button\n");
		sharpsl_suspend_request = 0;
		*event = APM_USER_SUSPEND;
		return APM_SUCCESS;
	} else {
		sharpsl_suspend_request = 0;
	}
#ifndef CONFIG_SABINAL_DISCOVERY
	/* Battery NG */
	if ( (apm_event_mask & APM_EVT_BATTERY_FAULT)
		&& !SHARPSL_BATTERY_OK && SHARPSL_AC_LINE_STATUS == APM_AC_OFFLINE) {
		DPRINTK("battery fatal\n");
		*event = APM_CRITICAL_SUSPEND;
		return APM_SUCCESS;
	}
#endif
	/* Batery Status */
	if ( apm_event_mask & APM_EVT_BATTERY_STATUS ) {
		static int is_first = 1;
		static int battery_is_first = 1;
		static int prev_status;
		static int prev_ac_line_status;
		int status;

		status = get_main_battery();
		if (status >= 0) {

#ifdef CONFIG_SABINAL_DISCOVERY
		  if ( status == APM_BATTERY_FAULT ) {
		  	if (battery_is_first) {
		    	*event = APM_BATTERY_FAULT;
    			battery_is_first = 0;
			    return APM_SUCCESS;
		    } else {
		    	*event = APM_NO_EVENTS;
    			battery_is_first = 1;
				return APM_NO_EVENTS;
			}
		  } else if (status == APM_LOW_BATTERY) {
		  	if (battery_is_first) {
		    	*event = APM_LOW_BATTERY;
    			battery_is_first = 0;
			    return APM_SUCCESS;
		    } else {
		    	*event = APM_NO_EVENTS;
    			battery_is_first = 1;
				return APM_NO_EVENTS;
			}
		  }

		  if (status == APM_BATTERY_STATUS_UNKNOWN) {
		    return APM_NO_EVENTS;
		  }
		  
		  if (prev_status != status) {
				prev_status = status;
				if (!is_first) {
					*event = APM_POWER_STATUS_CHANGE;
					return APM_SUCCESS;
				}
			}
			is_first = 0;
#else
		  if ( ( SHARPSL_AC_LINE_STATUS == APM_AC_OFFLINE ) && ( status == APM_BATTERY_STATUS_CRITICAL ) ) {
		    if (battery_is_first) {
		      *event = APM_LOW_BATTERY;
    		      battery_is_first = 0;
		      return APM_SUCCESS;
		    }
		  } else {
    		    battery_is_first = 1;
		  }
		  if ( ( prev_ac_line_status != SHARPSL_AC_LINE_STATUS ) || ( prev_status != status ) ) {
				DPRINTK("power status  change AC=%d V=%d->%d\n",
					(prev_ac_line_status != SHARPSL_AC_LINE_STATUS),prev_status, status);
				prev_ac_line_status = SHARPSL_AC_LINE_STATUS;
				prev_status = status;
				if (!is_first) {
					*event = APM_POWER_STATUS_CHANGE;
					return APM_SUCCESS;
				}
			}
			is_first = 0;
#endif

		}
	}

	/* ac Status */
#ifndef CONFIG_SABINAL_DISCOVERY
	if ( apm_event_mask & APM_EVT_LIGHT_CHANGE ) {
		static int prev_ac_line_status;
		static int prev_chg_status;

#if defined(CONFIG_APM_DISPLAY_BLANK) && defined(CONFIG_VT)
		if (prev_ac_line_status != SHARPSL_AC_LINE_STATUS) {
			// Flont Light contrast
			struct pm_dev *dev;
			dev = pm_find(PM_ILLUMINATION_DEV, NULL);
			if (dev && blank_state != APM_STATE_STANDBY) {
				DPRINTK("FL contrust :callback=%08x\n",	dev->callback);
				pm_send(dev, PM_BLANK, NULL);
				pm_send(dev, PM_UNBLANK, NULL);
			}
		        prev_ac_line_status = SHARPSL_AC_LINE_STATUS;

			*event = APM_POWER_STATUS_CHANGE;
			return APM_SUCCESS;
		}
#endif
		if (prev_chg_status != charge_status) {
		        prev_chg_status = charge_status;
			*event = APM_POWER_STATUS_CHANGE;
			return APM_SUCCESS;
		}

	}
#endif
	return APM_NO_EVENTS;

#endif	// CONFIG_ARCH_SHARP_SL
	return APM_SUCCESS;
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
	static spinlock_t lock = SPIN_LOCK_UNLOCKED;
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
		case APM_STATE_OFF:
			break;
		case APM_STATE_SUSPEND:
			DPRINTK("*** go into suspend\n");
			pxa_suspend();
			resume_notice = 1;
			break;
		case APM_STATE_STANDBY:
			DPRINTK("*** go into standby\n");
			send_event(APM_STATE_STANDBY);
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

static spinlock_t locklockFCS = SPIN_LOCK_UNLOCKED;
static unsigned long lockFCS = 0;
static int change_lockFCS = 0;
static spinlock_t lock_power_mode = SPIN_LOCK_UNLOCKED;
static unsigned long power_mode = 0;
static unsigned long unavailable_module = 0;

//#define POWER_MODE_LOG

int change_power_mode(unsigned long module, int set)
{
	unsigned long flag;
	unsigned short	dx;
	int		error;
	unsigned char  ac_line_status = 0xff;
	unsigned char  battery_status = 0xff;
	unsigned char  battery_flag   = 0xff;
	unsigned char   percentage     = 0xff;
	int retval = 1;
#if defined(POWER_MODE_LOG)
	static const char module_name[33][8] = {
			"MMC", "FFUART", "STUART", "BTUART", "IRDA", "SSP", "UDC", "AC97",
			"PCMCIA", "SOUND", "?", "?", "?", "?", "?", "?",
			"?", "?", "?", "?", "?", "?", "?", "?",
			"?", "?", "?", "?", "?", "?", "?", "?",
			"NO"
	};
	int i;
	unsigned long bit;
#endif
			
	spin_lock_irqsave(&lock_power_mode, flag);
	if (set) {
#if defined(POWER_MODE_LOG)
		bit = 1;
		for (i = 0; i < 32; i++) {
			if (module & (bit << i)) break;
		}
		printk("set power_mode by %s\n", module_name[i]);
#endif
		unavailable_module &= ~module;
		power_mode |= module;
	}
	else {
#if defined(POWER_MODE_LOG)
		bit = 1;
		for (i = 0; i < 32; i++) {
			if (module & (bit << i)) break;
		}
		printk("reset power_mode by %s\n", module_name[i]);
#endif
		power_mode &= ~module;
	}
	spin_unlock_irqrestore(&lock_power_mode, flag);
	if (module & LOCK_FCS_POWER_MODE_CORRESPOND_MASK) {
		lock_FCS(module, set);
	}
	return retval;
}

int sharpsl_is_power_mode_sensitive_battery(void)
{
	return (power_mode & POWER_MODE_CAUTION_MASK);
}

static ssize_t power_mode_read_params(struct file *file, char *buf,
									size_t nbytes, loff_t *ppos)
{
	char outputbuf[256];
	int count;
	static const char module_name[33][10] = {
			" MMC", " FFUART", " STUART", " BTUART", " IRDA", " SSP", " UDC", " AC97",
			" PCMCIA", " SOUND", " ?", " ?", " ?", " ?", " ?", " ?",
			" ?", " ?", " ?", " ?", " ?", " ?", " ?", " ?",
			" ?", " ?", " ?", " ?", " ?", " ?", " ?", " ?",
			" NO"
	};
	int i;
	unsigned long bit;

	if (*ppos>0) /* Assume reading completed in previous read*/
		return 0;
	if (unavailable_module) {
		count = sprintf(outputbuf, "0x%08X unavailable", (unsigned int)power_mode);
		bit = 1;
		for (i = 0; i < 32; i++) {
			if (unavailable_module & (bit << i)) {
				strcat(outputbuf, module_name[i]);
				count += strlen(module_name[i]);
			}
		}
		strcat(outputbuf, "\n");
		count += strlen("\n");
	}
	else {
		count = sprintf(outputbuf, "0x%08X\n", (unsigned int)power_mode);
	}
	count++;
	*ppos += count;
	if (count>nbytes)	/* Assume output can be read at one time */
		return -EINVAL;
	if (copy_to_user(buf, outputbuf, count+1))
		return -EFAULT;
	return count;
}

static struct file_operations proc_power_mode_params_operations = {
	read:	power_mode_read_params,
};

EXPORT_SYMBOL(change_power_mode);

//#define LOCK_FCS_LOG

int lock_FCS(unsigned long module, int lock)
{
	unsigned long flag;
#if defined(LOCK_FCS_LOG)
	static const char module_name[33][8] = {
			"MMC", "FFUART", "STUART", "BTUART", "IRDA", "SSP", "UDC", "AC97",
			"PCMCIA", "SOUND", "?", "?", "?", "?", "?", "?",
			"?", "?", "?", "?", "?", "?", "?", "?",
			"USR0", "USR1", "USR2", "USR3", "USR4", "USR5", "USR6", "USR7",
			"NO"
	};
	int i;
	unsigned long bit;
#endif

	spin_lock_irqsave(&locklockFCS, flag);
	if (lock) {
#if defined(LOCK_FCS_LOG)
		bit = 1;
		for (i = 0; i < 32; i++) {
			if (module & (bit << i)) break;
		}
		if (i != 0) {
			printk("lock_FCS() by %s\n", module_name[i]);
		}
#endif
		if (((lockFCS         ) & LOCK_FCS_SYNC_MSK) !=
			((lockFCS | module) & LOCK_FCS_SYNC_MSK)) {
			change_lockFCS = 1;
			wake_up_interruptible(&lock_fcs_waitqueue);
		}
		lockFCS |= module;
	}
	else {
#if defined(LOCK_FCS_LOG)
		bit = 1;
		for (i = 0; i < 32; i++) {
			if (module & (bit << i)) break;
		}
		if (i != 0) {
			printk("unlock_FCS() by %s\n", module_name[i]);
		}
#endif
		
	    	//	if (((lockFCS          ) & LOCK_FCS_SYNC_MSK) !=
		//	((lockFCS & ~module) & LOCK_FCS_SYNC_MSK)) {
		if (module & LOCK_FCS_SYNC_MSK) {
			change_lockFCS = 1;
			wake_up_interruptible(&lock_fcs_waitqueue);
		}
		lockFCS &= ~module;
	}
	spin_unlock_irqrestore(&locklockFCS, flag);
	return 1;
}

static ssize_t lock_fcs_read_params(struct file *file, char *buf,
									size_t nbytes, loff_t *ppos)
{
	char outputbuf[32];
	int count;

	if (*ppos>0) /* Assume reading completed in previous read*/
		return 0;
	if ((lockFCS & LOCK_FCS_UDC) && (lockFCS & LOCK_FCS_FFUART)) {
		count = sprintf(outputbuf, "0x%08X USB serial\n", (unsigned int)lockFCS);
	}
	else if (lockFCS & LOCK_FCS_UDC) {
		count = sprintf(outputbuf, "0x%08X USB\n", (unsigned int)lockFCS);
	}
	else if (lockFCS & LOCK_FCS_FFUART) {
		count = sprintf(outputbuf, "0x%08X serial\n", (unsigned int)lockFCS);
	}
	else {
		count = sprintf(outputbuf, "0x%08X\n", (unsigned int)lockFCS);
	}
	count++;
	*ppos += count;
	if (count>nbytes)	/* Assume output can be read at one time */
		return -EINVAL;
	if (copy_to_user(buf, outputbuf, count+1))
		return -EFAULT;
	return count;
}

#define LOCK_FCS_PROC_CONTROLABLE_BIT_START	24
#define LOCK_FCS_PROC_CONTROLABLE_BIT_END	31

static ssize_t lock_fcs_write_params(struct file *file, const char *buf,
				       size_t nbytes, loff_t *ppos)
{
	const char *endp;
	int i;
	unsigned long bit;

	endp = buf;
	for (i  = LOCK_FCS_PROC_CONTROLABLE_BIT_END;
		 i >= LOCK_FCS_PROC_CONTROLABLE_BIT_START; i--) {
		if (*endp == '\0') {
			break;
		}
		bit = 1 << i;
		if      ((*endp == '1') && (!(lockFCS & bit))) {
			lock_FCS(bit, 1);
		}
		else if ((*endp == '0') &&   (lockFCS & bit) ) {
			lock_FCS(bit, 0);
		}
		endp++;
	}
	return nbytes+endp-buf;
}

static unsigned int lock_fcs_poll(struct file *fp, poll_table * wait)
{
	poll_wait(fp, &lock_fcs_waitqueue, wait);
	if (change_lockFCS) {
		change_lockFCS = 0;
		return POLLIN | POLLRDNORM;
	}
	else {
		return 0;
	}
}

static struct file_operations proc_lock_fcs_params_operations = {
	read:	lock_fcs_read_params,
	write:	lock_fcs_write_params,
	poll:	lock_fcs_poll,
};

EXPORT_SYMBOL(lock_FCS);

#ifdef CONFIG_APM_CPU_IDLE
#ifdef SHARPSL_NEW_IDLE
static int save_icmr;
static int idletick = 1;
static int save_nice, save_counter;
#endif

static int apm_do_idle(void)
{
#if !defined(CONFIG_SABINAL_DISCOVERY)

#ifdef SHARPSL_NEW_IDLE
	save_icmr = ICMR;
	ICMR = 0;
	ICCR = 0;

	if (idletick != 0) {
#if defined(CONFIG_ARCH_PXA_CORGI) && ! defined(CONFIG_ARCH_PXA_SPITZ)
		if (hinge_count < HINGE_STABLE_COUNT) {
//			printk("hinge moving %d\n", hinge_count);
			idletick = 0;
		}
		else {
			extern unsigned long get_next_time_tick(void);
			idletick = get_next_time_tick();
		}
#else
		idletick = get_next_time_tick();
#endif
		OSMR0 += LATCH * idletick;

		if (idletick > 0 && (OSSR & OSSR_M0))
			OSSR |= OSSR_M0;
	}

    if ( !chg_freq_mode ) {
	    //LCM_LPT1 = 0x0080;
//	    if (!lockFCS || ((lockFCS == LOCK_FCS_FFUART) && (!(FFMSR & MSR_DSR)))) {
#if defined(CONFIG_ARCH_SHARP_SL_J) && !defined(CONFIG_ARCH_PXA_TOSA)
	    if (!(lockFCS & ~LOCK_FCS_FFUART)) {
#else
	    if (!lockFCS) {
#endif
#if defined(CONFIG_ARCH_PXA_POODLE)
		    while(1) {
			    if ( !( LCCR0 & 0x1 ) ||  ( GPLR(74) & GPIO_bit(74))  ) break;
		    }
#endif

#if defined(CONFIG_ARCH_PXA_TOSA)
		    CKEN &= ~CKEN2_AC97;
#endif

#if defined(CONFIG_CPU_PXA27X)
#if defined(CONFIG_FB_SHARPSL_PXA)
		    cpu_xscale_runmode();
		    while (!(ICPR & save_icmr) && OSCR < OSMR0) {
			if (!(LCCR0 & 0x1) || (GPLR(GPIO74_LCD_FCLK) &
					       GPIO_bit(GPIO74_LCD_FCLK)))
			    break;
		    }
		    while (!(ICPR & save_icmr) && OSCR < OSMR0) {
			if (!(LCCR0 & 0x1) || !(GPLR(GPIO74_LCD_FCLK) &
						GPIO_bit(GPIO74_LCD_FCLK)))
			    break;
		    }
		    if (!(ICPR & save_icmr) && OSCR < OSMR0) {
			sharpsl_pxafb_set_clock(1);
			cpu_xscale_sl_change_speed_low();
		    } else {
			chg_freq_mode = 2;
		    }
#else
		    cpu_xscale_sl_change_speed_low();
#endif
#else
		    if ( cccr_reg == 0x145 ) {
			    cpu_xscale_sl_change_speed_121();
		    } else {
			    cpu_xscale_change_speed_121();
		    }
#endif

#if defined(CONFIG_ARCH_PXA_TOSA)
		    CKEN |= CKEN2_AC97;
#endif

	    }
    }
#if defined(CONFIG_ARCH_PXA_SPITZ)
    if (chg_freq_mode == 0)
#endif
    chg_freq_mode = 1;

    //if (!clock_slowed) {
    //	   cpu_xscale_runmode();
    //	   clock_slowed = 1;
    //}

	if (!(ICPR & save_icmr) && (((signed long)(OSCR - OSMR0)) < 0)) {
		MDREFR |= MDREFR_APD;
		cpu_xscale_idle_sl();
	}

#else	// not SHARPSL_NEW_IDLE
  cpu_xscale_runmode();
  clock_slowed = 1;
#endif	// SHARPSL_NEW_IDLE

#else	// not CONFIG_SABINAL_DISCOVERY

#ifdef CONFIG_ARCH_SABINAL
  cpu_xscale_idle();

#else

#if CONFIG_APM_BIOS
	u32	dummy;

	if (apm_bios_call_simple(APM_FUNC_IDLE, 0, 0, &dummy))
		return 0;
#endif

#ifdef ALWAYS_CALL_BUSY
	clock_slowed = 1;
#else
	clock_slowed = (apm_info.bios.flags & APM_IDLE_SLOWS_CLOCK) != 0;
#endif

#endif // CONFIG_ARCH_SABINAL
#endif // CONFIG_SABINAL_DISCOVERY

	return 1;
}

static void apm_do_busy(void)
{

#if !defined(CONFIG_SABINAL_DISCOVERY)

#ifdef SHARPSL_NEW_IDLE

//    if (!lockFCS || ((lockFCS == LOCK_FCS_FFUART) && (!(FFMSR & MSR_DSR)))) {
#if defined(CONFIG_ARCH_SHARP_SL_J)
#if defined(CONFIG_ARCH_PXA_SPITZ)
	if (!(lockFCS & ~LOCK_FCS_FFUART) && chg_freq_mode == 1) {
#else
	if (!(lockFCS & ~LOCK_FCS_FFUART)) {
#endif
#else
	if (!lockFCS) {
#endif
#if defined(CONFIG_ARCH_PXA_POODLE)
		while(1) {
			if ( !( LCCR0 & 0x1 ) ||  ( GPLR(74) & GPIO_bit(74))  ) break;
		}
#endif
#if defined(CONFIG_ARCH_PXA_SPITZ)
		MDREFR |= MDREFR_APD;
#else
		MDREFR &= ~MDREFR_APD;
#endif
#if defined(CONFIG_ARCH_PXA_TOSA)
		CKEN &= ~CKEN2_AC97;
#endif

#if defined(CONFIG_CPU_PXA27X)
#if defined(CONFIG_FB_SHARPSL_PXA)
		while (1) {
		    if (!(LCCR0 & 0x1) || (GPLR(GPIO74_LCD_FCLK) &
					   GPIO_bit(GPIO74_LCD_FCLK)))
			break;
		}
		while (1) {
		    if (!(LCCR0 & 0x1) || !(GPLR(GPIO74_LCD_FCLK) &
					    GPIO_bit(GPIO74_LCD_FCLK)))
			break;
		}
		sharpsl_pxafb_set_clock(0);
#endif
		cpu_xscale_sl_change_speed_high();
#else
#if defined(CONFIG_ARCH_PXA_SHEPHERD) || defined(CONFIG_ARCH_PXA_TOSA)
		if ( cccr_reg == 0x161 ) {
			cpu_xscale_sl_change_speed_161();
		}
		else if ( cccr_reg == 0x145 ) {
			cpu_xscale_sl_change_speed_145();
		} else {
			cpu_xscale_change_speed_241();
		}
#else
		if ( cccr_reg == 0x145 ) {
			cpu_xscale_sl_change_speed_145();
		} else {
			cpu_xscale_change_speed_241();
		}
#endif
#endif

#if defined(CONFIG_ARCH_PXA_TOSA)
		CKEN |= CKEN2_AC97;
#endif

	} else {
#if defined(CONFIG_ARCH_PXA_SPITZ)
		MDREFR |= MDREFR_APD;
#else
		MDREFR &= ~MDREFR_APD;
#endif
	}

    //LCM_LPT1 = 0x0008;
    chg_freq_mode = 0;

#if 1	// adjust OSMR0
	{
		signed long passcount;
		int i = 0;
		passcount = OSCR - OSMR0;
		if ((passcount >= 0) || ( ( idletick != 0) && (passcount < -(idletick+1) * LATCH)) ) {
			// timer irq has occured
			(*(unsigned long *)&jiffies)+=idletick;
			if ((OSSR & OSSR_M0) == 0) {
				// timer irq error!
				unsigned long oscr, osmr0, ossr;
				oscr = OSCR;
				osmr0 = OSMR0;
				ossr = OSSR;
				//printk("OS timer IRQ error !!!\n");
				//printk("-- OSCR %x OSMR0 %x OSSR %x\n", (unsigned int)oscr, (unsigned int)osmr0, (unsigned int)ossr);
				while ((passcount >= 0) && ((OSSR & OSSR_M0) == 0)) {
					OSMR0 += LATCH;
					(*(unsigned long *)&jiffies)++;
					i++;
					passcount = OSCR - OSMR0;
				}
				//printk("-- added OSMR0 %d ticks\n", i);
			}
			idletick = 1;
		}
		else {
			// timer irq doesn't have occured
			// adjust jiffies
			unsigned long passtick;
			passtick = idletick + (passcount / LATCH);
			(*(unsigned long *)&jiffies)+=passtick;
			OSMR0 += LATCH * (passtick - idletick);
			while (((signed long)(OSMR0 - OSCR)) <= 0 &&
			       (OSSR & OSSR_M0) == 0) {
				OSMR0 += LATCH;
				(*(unsigned long *)&jiffies)++;
			}
			idletick = 0;
		}
	}
#endif

	ICMR = save_icmr;

    cpu_xscale_idle_sl_exit();

    //if (clock_slowed) {
    	   cpu_xscale_turbomode();
    //	   clock_slowed = 0;
    //}

#else	// not SHARPSL_NEW_IDLE
   cpu_xscale_idle_sl_exit();
   if (clock_slowed) {
	   cpu_xscale_turbomode();
  	   clock_slowed = 0;
   }
#endif	// SHARPSL_NEW_IDLE

#else	// not CONFIG_SABINAL_DISCOVERY

#ifdef CONFIG_ARCH_SABINAL
   cpu_xscale_idle_exit();

#else

#if CONFIG_APM_BIOS
	u32	dummy;
#endif

	if (clock_slowed) {
#if CONFIG_APM_BIOS
		(void) apm_bios_call_simple(APM_FUNC_BUSY, 0, 0, &dummy);
#endif
		clock_slowed = 0;
	}

#endif 
#endif // CONFIG_SABINAL_DISCOVERY
}
#endif //CONFIG_APM_CPU_IDLE


#ifdef CONFIG_SMP
static int apm_magic(void * unused)
{
	while (1)
		schedule();
}
#endif

static void apm_power_off(void)
{
	DPRINTK("\n");
	/*
	 * This may be called on an SMP machine.
	 */
#ifdef CONFIG_SMP
	/* Some bioses don't like being called from CPU != 0 */
	while (cpu_number_map(smp_processor_id()) != 0) {
		kernel_thread(apm_magic, NULL,
			CLONE_FS | CLONE_FILES | CLONE_SIGHAND | SIGCHLD);
		schedule();
	}
#endif

#ifdef CONFIG_APM_REAL_MODE_POWER_OFF
#else

#if defined(CONFIG_ARCH_SHARP_SL)

	if (pm_send_all(PM_SUSPEND, (void *)3)) {
		printk(KERN_CRIT "Emergency suspend was vetoed!\n" );
		return;
	}
#if !defined(CONFIG_SABINAL_DISCOVERY)
	/* system power down - just like main battery low */
	/* Magic No for battery low irq handler: 0x1234ABCD */
	do_IRQ(IRQ_GPIO_MAIN_BAT_LOW, 0x1234ABCD);
#endif

#else
	(void) apm_set_power_state(APM_STATE_OFF);
#endif

#endif
}

#ifdef CONFIG_APM_DO_ENABLE
static int apm_enable_power_management(int enable)
{
#if CONFIG_APM_BIOS
	u32	eax;
#endif
	DPRINTK("enable=%d\n",enable);

	if ((enable == 0) && (apm_info.bios.flags & APM_BIOS_DISENGAGED))
		return APM_NOT_ENGAGED;
#if CONFIG_APM_BIOS
	if (apm_bios_call_simple(APM_FUNC_ENABLE_PM, APM_DEVICE_BALL,
			enable, &eax))
		return (eax >> 8) & 0xff;
#endif
	if (enable)
		apm_info.bios.flags &= ~APM_BIOS_DISABLED;
	else
		apm_info.bios.flags |= APM_BIOS_DISABLED;
	return APM_SUCCESS;
}
#endif

static int apm_get_power_status(u_char *ac_line_status,
                                u_char *battery_status,
                                u_char *battery_flag,
                                u_char *battery_percentage,
                                u_short *battery_life)
{

#if defined(CONFIG_ARCH_PXA_TOSA)
u_char dumm_status;
u_short dumm_life;
#endif

#ifdef CONFIG_SABINAL_DISCOVERY
		discovery_apm_get_power_status(ac_line_status, 
		battery_status, battery_flag, battery_percentage, battery_life);
		
#endif	// CONFIG_SABINAL_DISCOVERY

#if defined(CONFIG_ARCH_PXA_POODLE) || defined(CONFIG_ARCH_PXA_CORGI)
		sharpsl_apm_get_power_status(ac_line_status, 
		battery_status, battery_flag, battery_percentage, battery_life);
#elif defined(CONFIG_ARCH_PXA_TOSA)
		sharpsl_apm_get_power_status(ac_line_status, 
		battery_status,&dumm_status,&dumm_status,battery_flag,
		battery_percentage,&dumm_status,&dumm_status,
		battery_life,&dumm_life,&dumm_life);
#endif
        return APM_SUCCESS;
}

static int apm_get_bp_status(u_char *ac_line_status,
                                u_char *battery_status,
                                u_char *battery_flag,
                                u_char *battery_percentage,
                                u_short *battery_life)
{

#if defined(CONFIG_SABINAL_DISCOVERY) || defined(CONFIG_ARCH_PXA_TOSA)
#if defined(CONFIG_SABINAL_DISCOVERY)
		discovery_apm_get_bp_status(ac_line_status, 
		battery_status, battery_flag, battery_percentage, battery_life);
#else
		sharpsl_apm_get_bp_status(ac_line_status, 
		battery_status, battery_flag, battery_percentage, battery_life);
#endif	// CONFIG_SABINAL_DISCOVERY
#endif
        return APM_SUCCESS;
}

static int apm_engage_power_management(u_short device, int enable)
{
#if CONFIG_APM_BIOS
	u32	eax;
#endif
	DPRINTK("device=%d enable=%d\n", device, enable);

	if ((enable == 0) && (device == APM_DEVICE_ALL)
	    && (apm_info.bios.flags & APM_BIOS_DISABLED))
		return APM_DISABLED;
#if CONFIG_APM_BIOS
	if (apm_bios_call_simple(APM_FUNC_ENGAGE_PM, device, enable, &eax))
		return (eax >> 8) & 0xff;
#endif
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

#if defined(CONFIG_APM_DISPLAY_BLANK) && defined(CONFIG_VT)
static int apm_console_blank(int blank)
{
	DPRINTK("blank=%d\n",blank);

	blank_state = blank ? APM_STATE_STANDBY : APM_STATE_READY;

	blank_request = 1;
	return 1;
}
#endif

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
#ifdef SHARPSL_NEW_IDLE
	current->nice = save_nice;
	current->counter = save_counter;
#else
	wake_up_interruptible(&apm_waitqueue);
#endif
}

static unsigned long get_cmos_time(void)
{
	return (unsigned long)(RCNR);
}

static void set_time(void)
{
	unsigned long	flags;

	if (got_clock_diff) {	/* Must know time zone in order to set clock */
		save_flags(flags);
		cli();
		CURRENT_TIME = get_cmos_time() + clock_cmos_diff;
		restore_flags(flags);
	}
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
	got_clock_diff = 1;
	restore_flags(flags);
#endif
}

static void reinit_timer(void)
{
#ifdef INIT_TIMER_AFTER_SUSPEND
	unsigned long	flags;

	save_flags(flags);
	cli();

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
#ifdef CONFIG_PCMCIA
		pcmcia_set_detect_interrupt(0, 0, 1);
#if defined(CONFIG_ARCH_PXA_TOSA)
		pcmcia_set_detect_interrupt(1, 0, 1);
#endif
#endif
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
#ifdef CONFIG_PCMCIA
		pcmcia_set_detect_interrupt(0, 1, 0);
#if defined(CONFIG_ARCH_PXA_TOSA)
		pcmcia_set_detect_interrupt(1, 1, 0);
#endif
#endif

#ifdef CONFIG_SABINAL_DISCOVERY
		sleeping = 0;				

		//touch panel 
		CKEN |= CKEN3_SSP;
		SSCR0 = 0x1ab;
		SSCR1 = 0;
		GFER(GPIO_TP_INT) |= GPIO_bit(GPIO_TP_INT);
		GEDR(GPIO_TP_INT) = GPIO_bit(GPIO_TP_INT);		

		Init_Voltage = discovery_read_MBattery(); //520ms
		mdelay(2);
		Init_Voltage = discovery_read_MBattery(); //520ms
		HWR_flag = 1;

//		GPSR1 = GPIO_GPIO57;

		ASIC3_GPIO_INTSTAT_A = 0; //make sure we clean the ASIC interrupt

		//re-init power_on_key
		ASIC3_GPIO_MASK_A &= ~PWR_ON_KEY;
		ASIC3_GPIO_DIR_A &= ~PWR_ON_KEY;
		ASIC3_GPIO_INTYP_A |= PWR_ON_KEY;
		ASIC3_GPIO_ETSEL_A &= ~(PWR_ON_KEY); //failling edge trigger

#endif

		(void) pm_send_all(PM_RESUME, (void *)0);
		is_goto_suspend = 0;

		send_sig_to_all_procs(SIGCONT);
#ifdef CONFIG_PCMCIA
		pcmcia_set_detect_interrupt(0, 1, 1);
#if defined(CONFIG_ARCH_PXA_TOSA)
		pcmcia_set_detect_interrupt(1, 1, 1);
#endif
#endif
		resume_handling = 0;

		break;
	case APM_LOW_BATTERY:
		if (is_goto_suspend)
			return 1; 
		(void) pm_send_all(PM_BATTERY_CRITICAL_LOW, NULL);
		break;
	case APM_STATE_STANDBY:
		if (is_goto_suspend)
			return 1; 
		(void) pm_send_all(PM_BLANK, NULL);
		(void) pm_send_all(PM_STANDBY, NULL);
		break;
	case APM_STATE_READY:
		if (is_goto_suspend)
			return 1; 
		(void) pm_send_all(PM_UNBLANK, NULL);
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

	reinit_timer();
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
#ifndef CONFIG_SABINAL_DISCOVERY
			if (ignore_bounce) {
				if (apm_info.connection_version > 0x100)
					apm_set_power_state(APM_STATE_REJECT);
				break;
			}
#endif
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
			last_resume = jiffies;
			ignore_bounce = 1;
#ifdef CONFIG_SABINAL_DISCOVERY
//			set_time();
			send_event(event);
			queue_event(event, NULL);
#else
			if ((event != APM_NORMAL_RESUME)
			    || (ignore_normal_resume == 0)) {
				set_time();
				send_event(event);
				queue_event(event, NULL);
			}
#endif
			waiting_for_resume = btf_waiting_for_resume = 0;
			break;

		case APM_CAPABILITY_CHANGE:
		case APM_POWER_STATUS_CHANGE:
			break;
			
		case APM_LOW_BATTERY:
			send_event(event);
			queue_event(event, NULL);
				
			break;

		case APM_UPDATE_TIME:
			set_time();
			break;

		case APM_BATTERY_FAULT: 
		case APM_CRITICAL_SUSPEND:
			if (btf_waiting_for_resume)
				return;

			btf_waiting_for_resume = 1;

			/*
			 * We can only hope it worked - we are not allowed
			 * to reject a critical suspend.
			 */
			if ( apm_event_mask & APM_EVT_POWER_BUTTON ) {
				sharpsl_suspend_request = 1;
			} else {
				handle_scancode(SLKEY_OFF|KBDOWN , 1);
				handle_scancode(SLKEY_OFF|KBUP   , 0);
			}
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
#define system_idle() (((nr_running == 1) && !idleCancel))

static void apm_mainloop(void)
{
#ifdef CONFIG_APM_CPU_IDLE
#ifdef SHARPSL_NEW_IDLE
#if defined(CONFIG_ARCH_PXA_CORGI)
	unsigned long gprr;
#endif	
	for (;;) {
#if defined(CONFIG_ARCH_PXA_CORGI) && ! defined(CONFIG_ARCH_PXA_SPITZ)
#if defined(CONFIG_ARCH_PXA_SPITZ)
		gprr = 0;
		if (GPLR(GPIO_SWA) & GPIO_bit(GPIO_SWA))
			gprr |= 0x04;
		if (GPLR(GPIO_SWB) & GPIO_bit(GPIO_SWB))
			gprr |= 0x08;
#else
		gprr = SCP_REG_GPRR & (SCP_SWA | SCP_SWB);
#endif
		if (gprr != sharpsl_hinge_state && ((gprr & 0xc) != 0x4)) {
			if (((sharpsl_hinge_state & 0xc) == 0xc) && ((gprr & 0xc) == 0x8)) {
				// opening
//				printk("hinge opening\n");
			}
			else {
				hinge_count = 0;
				sharpsl_hinge_state = gprr;
//				printk("hinge move? %x\n", sharpsl_hinge_state >> 2);
			}
		}
		else {
			hinge_count++;
			if (hinge_count == HINGE_STABLE_COUNT) {
				// notify Qt of hinge state changed
				handle_scancode(SLKEY_HINGEMOVED|KBDOWN , 1);
				handle_scancode(SLKEY_HINGEMOVED|KBUP   , 0);
//				printk("hinge move! %x\n", sharpsl_hinge_state >> 2);
			}
		}
#endif
		/* Nothing to do, just sleep for the timeout */
		save_nice = current->nice;
		save_counter = current->counter;
		current->nice = 20;
		current->counter = -100;
		schedule();
		current->nice = save_nice;
		current->counter = save_counter;
		if (exit_kapmd)
			break;

		/*
		 * Ok, check all events, check for idle (and mark us sleeping
		 * so as not to count towards the load average)..
		 */
		apm_event_handler();
		if (!system_idle())
			continue;

		apm_do_idle();
		apm_do_busy();
	}
#else
	int timeout = HZ;
	int save_nice, save_counter;
	DECLARE_WAITQUEUE(wait, current);

	add_wait_queue(&apm_waitqueue, &wait);
	set_current_state(TASK_INTERRUPTIBLE);
	
	for (;;) {
		/* Nothing to do, just sleep for the timeout */
#if defined(CONFIG_ARCH_PXA_POODLE) || defined(CONFIG_ARCH_PXA_CORGI) || defined(CONFIG_ARCH_PXA_TOSA)
		timeout = 2*timeout;
		if (timeout > APM_CHECK_TIMEOUT)
#endif
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
	}
	remove_wait_queue(&apm_waitqueue, &wait);

#endif

#else
	int timeout = HZ;
	int save_nice, save_counter;
	DECLARE_WAITQUEUE(wait, current);

	add_wait_queue(&apm_waitqueue, &wait);
	set_current_state(TASK_INTERRUPTIBLE);
	cpu_xscale_turbomode();
	
	for (;;) {
		/* Nothing to do, just sleep for the timeout */
#if defined(CONFIG_ARCH_PXA_POODLE) || defined(CONFIG_ARCH_PXA_CORGI) || defined(CONFIG_ARCH_PXA_TOSA)
		timeout = 2*timeout;
		if (timeout > APM_CHECK_TIMEOUT)
#endif
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
		if (!system_idle())
			continue;

		if (apm_do_idle()) {
			unsigned long start = jiffies;
			while ((!exit_kapmd) && system_idle()) {
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
#endif
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

	if (cmd != APM_IOC_GET_HINGE_STATE) { /* user can read hinge */
	as = filp->private_data;
	if (check_apm_user(as, "ioctl"))
		return -EIO;
	if (!as->suser)
		return -EPERM;
	}
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

#ifdef CONFIG_ARCH_SHARP_SL
#ifdef CONFIG_SABINAL_DISCOVERY
	case APM_IOC_GET_BACKPACK_STATE: {
		return !BACKPACK_IN_DETECT();  /* 0 : not in , 1 : exist */
	} break;
#endif

	case APM_IOC_GET_REGISTER: {
	} break;

#if defined(CONFIG_ARCH_PXA_POODLE) || defined(CONFIG_ARCH_PXA_CORGI) || defined(CONFIG_ARCH_PXA_TOSA)
	case APM_IOC_RESET_PM: {
	    extern int sharpsl_main_bk_flag;
	    sharpsl_main_bk_flag = 1;
	    sharpsl_kick_battery_check(0,0,2);
	} break;
#endif

	case APM_IOC_AUTO_POWER_CANCEL: {
		autoPowerCancel = 0;
	} break;
        case APM_IOC_GET_AUTO_POWER_CANCEL: {
		int temp = autoPowerCancel;
		autoPowerCancel = 1;
		return temp;
	} break;
	case APM_IOC_AUTO_LIGHT_CANCEL: {
		autoLightCancel = 0;
	} break;
	case APM_IOC_GET_AUTO_LIGHT_CANCEL: {
		int temp = autoLightCancel;
		autoLightCancel = 1;
		return temp;
	} break;

	case APM_IOC_GET_AUTO_POWER_TIME: {
	  return ( APOCnt / 100 );
	} break;
	case APM_IOC_GET_AUTO_LIGHT_TIME: {
	  return ( LPOCnt / 100 );
	} break;
	case APM_IOC_SET_AUTO_POWER_TIME: {
	  if ( arg == 0 ) {
	    APOCnt = 0xffffffff;
	  } else {
	    APOCnt = arg * 100;
	  }
	  autoPowerCancel = 0;
	  return APOCnt;
	} break;
	case APM_IOC_SET_AUTO_LIGHT_TIME: {
	  if ( arg == 0 ) {
	    LPOCnt = 0xffffffff;
	  } else {
	    LPOCnt = arg * 100;
	  }
	  autoLightCancel = 1;
	  return LPOCnt;
	} break;
	case APM_IOC_GET_AC_STATUS: {
		return SHARPSL_AC_LINE_STATUS;
	} break;
	case APM_IOC_SET_DAEMON_MODE: {
#if defined(CONFIG_SABINAL_DISCOVERY)
          disable_irq(IRQ_ASIC3_PWR_ON);
#elif defined(CONFIG_ARCH_PXA_POODLE)
          disable_irq(IRQ_GPIO_ON_KEY);
#endif
	  daemon_mode = 1;
	  return 1;
	} break;
	case APM_IOC_RESET_DAEMON_MODE: {
	  sharpsl_suspend_request = 0;
	  daemon_mode = 0;
#if defined(CONFIG_SABINAL_DISCOVERY)
          enable_irq(IRQ_ASIC3_PWR_ON);
#elif defined(CONFIG_ARCH_PXA_POODLE)
          enable_irq(IRQ_GPIO_ON_KEY);
#endif
	  return 1;
	} break;

	case APM_IOC_BATTERY_BACK_CHK: {
	  //return collie_backup_battery; 
#ifdef CONFIG_ARCH_PXA_TOSA
		return sharpsl_bu_battery;
#endif
	} break;

	case APM_IOC_BATTERY_MAIN_CHK: {
#ifndef CONFIG_SABINAL_DISCOVERY
	  return sharpsl_main_battery; 
#endif
	} break;

	case APM_IOC_GET_ON_MODE: {
	  sharpsl_get_on_mode = RCSR;
	  return sharpsl_get_on_mode;
	} break;

	case APM_IOC_GET_ON_MODE_CLEAR: {
	  RCSR = ( RCSR_HWR | RCSR_WDR | RCSR_SMR | RCSR_GPR );
	  return 1;
	} break;

#if defined(CONFIG_ARCH_PXA_POODLE) || defined(CONFIG_ARCH_PXA_CORGI) || defined(CONFIG_ARCH_PXA_TOSA)
	case APM_IOC_SFREQ: {
		int freq;
		get_user(freq, (unsigned int *)(arg));
#if defined(CONFIG_CPU_PXA27X)
		if ( freq != CCCR ) {
			sharpsl_chg_freq = freq & 0x00010000;
			handle_scancode(SLKEY_OFF|KBDOWN , 1);
			mdelay(500);
			handle_scancode(SLKEY_OFF|KBUP   , 0);
			return arg;
		}
#else
		if ( ( freq == 0x0145 ) && ( CCCR != 0x0145 ) ) {
			sharpsl_chg_freq = 0x00010145;
			handle_scancode(SLKEY_OFF|KBDOWN , 1);
			mdelay(500);
			handle_scancode(SLKEY_OFF|KBUP   , 0);
			return arg;
		}
		if ( ( freq == 0x0241 ) && ( CCCR != 0x0241 ) ) {
			sharpsl_chg_freq = 0x00010241;
			handle_scancode(SLKEY_OFF|KBDOWN , 1);
			mdelay(500);
			handle_scancode(SLKEY_OFF|KBUP   , 0);
			return arg;
		}
#endif
		printk("you do not have to change freq\n");
		return freq;
	}

	case APM_IOC_GFREQ: {
		return sharpsl_chg_freq;
	}

	case APM_IOC_GETRTC: {
		return RCNR;
	}

	case APM_IOC_SETRTC: {
		int settime;
		get_user(settime, (unsigned int *)(arg));
		RCNR = settime;
		xtime.tv_sec = settime;
		return RCNR;
	}
	case APM_IOC_KICK_BATTERY_CHECK: {
	    sharpsl_kick_battery_check(0,0,0);
	    return;
	}
#endif

	case APM_IOC_RESET: {
		unsigned long   flags;

		sleeping = 1;
#if defined(CONFIG_ARCH_PXA_POODLE) || defined(CONFIG_ARCH_PXA_CORGI) || defined(CONFIG_ARCH_PXA_TOSA)
		sharpsl_off_mode = 1;
#endif
		save_flags_cli(flags);
#if defined(CONFIG_ARCH_PXA_SHEPHERD) || defined(CONFIG_ARCH_PXA_TOSA)
		sharpsl_restart_nonstop();
#else
		sharpsl_restart();
#endif
	} break;
#if defined(CONFIG_ARCH_PXA_SPITZ)
	case APM_IOC_GET_HINGE_STATE: {
	    int ret = 0;
	    if (GPLR(GPIO_SWA) & GPIO_bit(GPIO_SWA))
		ret |= 0x01;
	    if (GPLR(GPIO_SWB) & GPIO_bit(GPIO_SWB))
		ret |= 0x02;
	    return ret;

	}
#elif defined(CONFIG_ARCH_PXA_CORGI)
	case APM_IOC_GET_HINGE_STATE: {
		return (sharpsl_hinge_state >> 2);
	}
#endif

#if defined(CONFIG_ARCH_PXA_SPITZ)
	case APM_IOCGWUPSRC:
		return (int)get_logical_wakeup_src_mask();
	case APM_IOCSWUPSRC:
		{
			int tmp = (int)get_logical_wakeup_src_mask();
			set_logical_wakeup_src_mask(arg);
			return tmp;
		}
	case APM_IOCGWUPFACT:
		return get_logical_wakeup_factor();
#else
	case APM_IOCGWUPSRC:
		return (int)get_apm_wakeup_src_mask();
	case APM_IOCSWUPSRC:
		{
			int tmp = (int)get_apm_wakeup_src_mask();
			set_apm_wakeup_src_mask(arg);
			return tmp;
		}
	case APM_IOCGWUPFACT:
		return get_apm_wakeup_factor();
#endif
	case APM_IOCGEVTSRC:
		return (int)apm_event_mask;
	case APM_IOCSEVTSRC:
		{
			int tmp = (int)apm_event_mask;
			apm_event_mask = arg;
			return tmp;
		}

#if defined(CONFIG_ARCH_PXA_TOSA)
	case APM_IOC_GET_CARDSLOT_ERROR: {
	    return sharpsl_get_cardslot_error();
	}

	case APM_IOC_BATTERY_JACKET_CHK: {
	  return sharpsl_jacket_battery; 
	} break;

	case APM_IOC_GET_JACKET_STATE:
		return sharpsl_jacket_exist;
#endif
#endif

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
	char *		p;
	unsigned short	dx;
	int		error;
	unsigned char  ac_line_status = 0xff;
	unsigned char  battery_status = 0xff;
	unsigned char  battery_flag   = 0xff;
	unsigned char   percentage     = 0xff;
	int             time_units     = -1;
	char            *units         = "?";

	p = buf;

	if ((smp_num_cpus == 1) &&
		!(error = apm_get_power_status(&ac_line_status,
			&battery_status, &battery_flag, &percentage, &dx))) {
		if (apm_info.connection_version > 0x100) {
			if (dx != 0xffff) {
				units = (dx & 0x8000) ? "min" : "sec";
				time_units = dx & 0x7fff;
			}
		}
	}

	/* Arguments, with symbols from linux/apm_bios.h.  Information is
	   from the Get Power Status (0x0a) call unless otherwise noted.

	   0) Linux driver version (this will change if format changes)
	   1) APM BIOS Version.  Usually 1.0, 1.1 or 1.2.
	   2) APM flags from APM Installation Check (0x00):
	      bit 0: APM_16_BIT_SUPPORT
	      bit 1: APM_32_BIT_SUPPORT
	      bit 2: APM_IDLE_SLOWS_CLOCK
	      bit 3: APM_BIOS_DISABLED
	      bit 4: APM_BIOS_DISENGAGED
	   3) AC line status
	      0x00: Off-line
	      0x01: On-line
	      0x02: On backup power (BIOS >= 1.1 only)
	      0xff: Unknown
	   4) Battery status
	      0x00: High
	      0x01: Low
	      0x02: Critical
	      0x03: Charging
	      0x04: Selected battery not present (BIOS >= 1.2 only)
	      0x7f: Very Low (for SHARP SL)
	      0xff: Unknown
	   5) Battery flag
	      bit 0: High
	      bit 1: Low
	      bit 2: Critical
	      bit 3: Charging
	      bit 5: Very Low (for SHARP SL)
	      bit 7: No system battery
	      0xff: Unknown
	   6) Remaining battery life (percentage of charge):
	      0-100: valid
	      -1: Unknown
	   7) Remaining battery life (time units):
	      Number of remaining minutes or seconds
	      -1: Unknown
	   8) min = minutes; sec = seconds */

	p += sprintf(p, "%s %d.%d 0x%02x 0x%02x 0x%02x 0x%02x %d%% %d %s\n",
		     driver_version,
		     (apm_info.bios.version >> 8) & 0xff,
		     apm_info.bios.version & 0xff,
		     apm_info.bios.flags,
		     ac_line_status,
		     battery_status,
		     battery_flag,
		     percentage,
		     time_units,
		     units);

	return p - buf;
}


#if defined(CONFIG_SABINAL_DISCOVERY) || defined(CONFIG_ARCH_PXA_TOSA)
static int apm_bp_get_info(char *buf, char **start, off_t fpos, int length)
{
	char *		p;
	unsigned short	dx;
	int		error;
	unsigned char  ac_line_status = 0xff;
	unsigned char  battery_status = 0xff;
	unsigned char  battery_flag   = 0xff;
	unsigned char   percentage     = 0xff;
	int             time_units     = -1;
	char            *units         = "?";

	p = buf;

	if ((smp_num_cpus == 1) &&
		!(error = apm_get_bp_status(&ac_line_status,
			&battery_status, &battery_flag, &percentage, &dx))) {
		if (apm_info.connection_version > 0x100) {
			if (dx != 0xffff) {
				units = (dx & 0x8000) ? "min" : "sec";
				time_units = dx & 0x7fff;
			}
		}
	}
	/* Arguments, with symbols from linux/apm_bios.h.  Information is
	   from the Get Power Status (0x0a) call unless otherwise noted.

	   0) Linux driver version (this will change if format changes)
	   1) APM BIOS Version.  Usually 1.0, 1.1 or 1.2.
	   2) APM flags from APM Installation Check (0x00):
	      bit 0: APM_16_BIT_SUPPORT
	      bit 1: APM_32_BIT_SUPPORT
	      bit 2: APM_IDLE_SLOWS_CLOCK
	      bit 3: APM_BIOS_DISABLED
	      bit 4: APM_BIOS_DISENGAGED
	   3) AC line status
	      0x00: Off-line
	      0x01: On-line
	      0x02: On backup power (BIOS >= 1.1 only)
	      0xff: Unknown
	   4) Battery status
	      0x00: High
	      0x01: Low
	      0x02: Critical
	      0x03: Charging
	      0x04: Selected battery not present (BIOS >= 1.2 only)
	      0xff: Unknown
	   5) Battery flag
	      bit 0: High
	      bit 1: Low
	      bit 2: Critical
	      bit 3: Charging
	      bit 7: No system battery
	      0xff: Unknown
	   6) Remaining battery life (percentage of charge):
	      0-100: valid
	      -1: Unknown
	   7) Remaining battery life (time units):
	      Number of remaining minutes or seconds
	      -1: Unknown
	   8) min = minutes; sec = seconds */

	p += sprintf(p, "%s %d.%d 0x%02x 0x%02x 0x%02x 0x%02x %d%% %d %s\n",
		     driver_version,
		     (apm_info.bios.version >> 8) & 0xff,
		     apm_info.bios.version & 0xff,
		     apm_info.bios.flags,
		     ac_line_status,
		     battery_status,
		     battery_flag,
		     percentage,
		     time_units,
		     units);

	return p - buf;
}
#endif


#ifdef CONFIG_SABINAL_DISCOVERY
static int discovery_key_check(void *unused)
{

    daemonize();
    strcpy(current->comm, "kkeychkd");
    sigfillset(&current->blocked);

    while(1) {

      interruptible_sleep_on(&fl_key);

      while(1) {
          interruptible_sleep_on_timeout((wait_queue_head_t*)&queue, KEY_TICK );
#ifdef CONFIG_SABINAL_DISCOVERY
		if ( (ASIC3_GPIO_PSTS_A & PWR_ON_KEY) != 0 ) { //key up
			break;
		}
#else
		if ( GPLR(GPIO_ON_KEY) & GPIO_bit(GPIO_ON_KEY) ) {
			break;
		}
#endif
			if ( ( jiffies - on_press_time ) < 0 ) {

				if ( ( jiffies + (0xffffffff - on_press_time) ) > FLONT_LIGHT_TOGGLE_TIME ) {
					if ( apm_event_mask & APM_EVT_POWER_BUTTON ) {
#ifdef CONFIG_SABINAL_DISCOVERY
						discoveryfl_blank_power_button();
#else
						sharpslfl_blank_power_button();
#endif
					} else {
						handle_scancode(SLKEY_FRONTLIGHT|KBDOWN , 1);
						handle_scancode(SLKEY_FRONTLIGHT|KBUP   , 0);
					}
	            	break;
				}
			
        	} else {        
	            if ( ( jiffies - on_press_time ) > FLONT_LIGHT_TOGGLE_TIME ) {
	
					if ( apm_event_mask & APM_EVT_POWER_BUTTON ) {
#ifdef CONFIG_SABINAL_DISCOVERY
						discoveryfl_blank_power_button();
#else
						tosa_l_blank_power_button();
#endif
					} else {
						handle_scancode(SLKEY_FRONTLIGHT|KBDOWN , 1);
						handle_scancode(SLKEY_FRONTLIGHT|KBUP   , 0);
					}
	            	break;
            	}
	        }
          
       }

    }

}
#endif

static int apm_thread(void *unused)
{
	int		error;
	char *		power_stat;
	char *		bat_stat;

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
	if (smp_num_cpus == 1) {
#if defined(CONFIG_APM_DISPLAY_BLANK) && defined(CONFIG_VT)
		console_blank_hook = apm_console_blank;
#endif
		apm_mainloop();
#if defined(CONFIG_APM_DISPLAY_BLANK) && defined(CONFIG_VT)
		console_blank_hook = NULL;
#endif
	}
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
	struct proc_dir_entry *lock_fcs_proc;
	struct proc_dir_entry *power_mode_proc;

	apm_info.bios = apm_bios_info;
	if (apm_info.bios.version == 0) {
		printk(KERN_INFO "apm: BIOS not found.\n");
		return -ENODEV;
	}
	printk(KERN_INFO "apm: BIOS version %d.%d Flags 0x%02x (Driver version %s)\n",
		((apm_info.bios.version >> 8) & 0xff),
		(apm_info.bios.version & 0xff),
		apm_info.bios.flags,
		driver_version);
	if ((apm_info.bios.flags & APM_32_BIT_SUPPORT) == 0) {
		printk(KERN_INFO "apm: no 32 bit BIOS support\n");
		return -ENODEV;
	}


#if defined(CONFIG_ARCH_PXA_POODLE) || defined(CONFIG_ARCH_PXA_CORGI) || defined(CONFIG_ARCH_PXA_TOSA)
#if defined(CONFIG_CPU_PXA27X)
	cpu_xscale_sl_change_speed_high();
#else
#if defined(CONFIG_ARCH_PXA_SHEPHERD) || defined(CONFIG_ARCH_PXA_TOSA)
	sharpsl_chg_freq = (unsigned int)0x00000161;
	cpu_xscale_sl_change_speed_161();
#else
#if 1	// default 400MHz
	sharpsl_chg_freq = (unsigned int)0x00000241;
#else
	cpu_xscale_sl_change_speed_145_without_lcd();
#endif
#endif
#endif
	cccr_reg = CCCR;
	printk("FCS : CCCR = %x\n",cccr_reg);
#if defined(CONFIG_CPU_PXA27X)
	sharpsl_chg_freq = cccr_reg;
#endif
#endif

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

#ifdef CONFIG_SABINAL_DISCOVERY

	/* read AC LINE STATUS */
	sharpsl_suspend_request = 0;

	ASIC3_GPIO_MASK_A &= ~PWR_ON_KEY;
	ASIC3_GPIO_DIR_A &= ~PWR_ON_KEY;
	ASIC3_GPIO_INTYP_A |= PWR_ON_KEY;
	ASIC3_GPIO_ETSEL_A &= ~(PWR_ON_KEY); //failling edge trigger

	if (request_irq(IRQ_ASIC3_PWR_ON,
			powerirq_handler, SA_INTERRUPT, "apm", powerirq_handler)) {
		printk(KERN_NOTICE "apm: Couldn't get irq %d.\n", IRQ_ASIC1_PWR_ON);
		return -ENODEV;
	}

	sleeping = 0;
	waiting_for_resume = btf_waiting_for_resume = 0;

#else
	GPDR(GPIO_AC_IN) &= ~GPIO_bit(GPIO_AC_IN);
	GPDR(GPIO_MAIN_BAT_LOW) &= ~GPIO_bit(GPIO_MAIN_BAT_LOW);
#if defined(CONFIG_ARCH_PXA_POODLE)
	GPDR(GPIO_ON_KEY) &= ~GPIO_bit(GPIO_ON_KEY);
	set_GPIO_IRQ_edge( GPIO_ON_KEY, GPIO_BOTH_EDGES );
#endif
#if defined(CONFIG_ARCH_PXA_CORGI)
	GPDR(GPIO_KEY_INT) &= ~GPIO_bit(GPIO_KEY_INT);
	set_GPIO_IRQ_edge( GPIO_KEY_INT, GPIO_FALLING_EDGE );
#endif

	sharpsl_suspend_request = 0;

#if defined(CONFIG_ARCH_PXA_POODLE)
	if (request_irq(IRQ_GPIO_ON_KEY,
			powerirq_handler, SA_INTERRUPT, "apm", powerirq_handler)) {
		printk(KERN_NOTICE "apm: Couldn't get irq %d.\n", IRQ_GPIO_ON_KEY);
		return -ENODEV;
	}
#endif
#endif

	apm_proc = create_proc_info_entry("apm", 0, NULL, apm_get_info);
	if (apm_proc)
		SET_MODULE_OWNER(apm_proc);

	lock_fcs_proc = create_proc_entry("lock_fcs", 0, NULL);
	if (lock_fcs_proc) {
		lock_fcs_proc->proc_fops = &proc_lock_fcs_params_operations;
	}

	power_mode_proc = create_proc_entry("power_mode", 0, NULL);
	if (power_mode_proc) {
		power_mode_proc->proc_fops = &proc_power_mode_params_operations;
	}

	kernel_thread(apm_thread, NULL, CLONE_FS | CLONE_FILES | CLONE_SIGHAND | SIGCHLD);
  	
#if defined(CONFIG_SABINAL_DISCOVERY) || defined(CONFIG_ARCH_PXA_TOSA)
#if defined(CONFIG_SABINAL_DISCOVERY)
	kernel_thread( discovery_key_check,  NULL, CLONE_FS | CLONE_FILES | CLONE_SIGHAND | SIGCHLD);
#endif
	{
		struct proc_dir_entry *apm_proc_backpack;

		apm_proc_backpack = create_proc_info_entry("apm_bp", 0, NULL, apm_bp_get_info);
		if (apm_proc_backpack)
			SET_MODULE_OWNER(apm_proc_backpack);
	}
#endif

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
	remove_proc_entry("lock_fcs", NULL);
	remove_proc_entry("power_mode", NULL);

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



/*
 * Local variables:
 *   c-basic-offset: 4
 * End:
 */
