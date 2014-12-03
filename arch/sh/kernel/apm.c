/*
 * linux/arch/sh/kernel/apm.c
 *
 * bios-less APM driver for SH Linux
 *
 * Based on:
 *   arch/arm/mach-sa1100/apm.c
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

#include <linux/reboot.h>
#include <linux/delay.h>

#if defined(CONFIG_CPU_SUBTYPE_SH7727)
#include <asm/sh7727.h>
#elif defined(CONFIG_CPU_SUBTYPE_SH7751R)
#include <asm/sh7751r.h>
#elif defined(CONFIG_CPU_SUBTYPE_SH7760)
#include <asm/sh7760.h>
#elif defined(CONFIG_CPU_SUBTYPE_SH7720)
#include <asm/sh7720.h>
#endif
#include <asm/io.h>

#if defined(CONFIG_CPU_SUBTYPE_SH7720)
#define TSTR TMU_TSTR
#endif

/*
 * Power management requests
 */
enum
{
	PM_SUSPEND_CHECK = PM_LOCK + 1,
	PM_STANDBY,
	PM_SLOW_CLOCK,
	PM_UNSLOW_CLOCK,
	PM_BLANK,
	PM_UNBLANK,
	PM_ILLUMINATION_DEV
};

#define APM_AC_OFFLINE 0
#define APM_AC_ONLINE 1
#define APM_AC_BACKUP 2
#define APM_AC_UNKNOWN 0xFF

#define APM_BATTERY_STATUS_HIGH 0
#define APM_BATTERY_STATUS_LOW  1
#define APM_BATTERY_STATUS_CRITICAL 2
#define APM_BATTERY_STATUS_CHARGING 3
#define APM_BATTERY_STATUS_UNKNOWN 0xFF

#define APM_BATTERY_LIFE_UNKNOWN 0xFFFF
#define APM_BATTERY_LIFE_MINUTES 0x8000
#define APM_BATTERY_LIFE_VALUE_MASK 0x7FFF


//#define DEBUG
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
 * Need to poll the APM BIOS every second
 */
#define APM_CHECK_TIMEOUT	(HZ*2)	/* Is this enough ? */

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

static int              resume_notice = 0;
static int              is_goto_suspend = 0;
static int              blank_request = 0;
static u_short          blank_state;


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
static int			waiting_for_resume;
static int			ignore_normal_resume;
static int			bounce_interval = DEFAULT_BOUNCE_INTERVAL;

#ifdef CONFIG_APM_RTC_IS_GMT
#define	clock_cmos_diff		0
#define	got_clock_diff		1
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

static struct timer_list	apm_wink_timer;
static unsigned long		apm_wink_timehandle = 0;
static int			timerstart_enable = 0;

static DECLARE_WAIT_QUEUE_HEAD(apm_waitqueue);
static DECLARE_WAIT_QUEUE_HEAD(apm_suspend_waitqueue);
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

extern long sys_sync(void);
extern void disable_irq(unsigned int);

void (*pm_power_off)(void);

static int __init apm_driver_version(u_short *val)
{
	*val = apm_bios_info.version;
	return APM_SUCCESS;
}

static void apm_wink_timer_handler(unsigned long);

static int apm_wink_timerstart(void)
{
	apm_wink_timehandle++;

	init_timer(&apm_wink_timer);
	apm_wink_timer.function = apm_wink_timer_handler;
	apm_wink_timer.expires =jiffies + HZ * 3;
	add_timer(&apm_wink_timer);
	return 0;
}

static void apm_wink_timer_handler(unsigned long data)
{
	apm_wink_timehandle--;
	
	mdelay(1000);
	if (timerstart_enable)
		apm_wink_timerstart();
}

static void shutdown_wink_timer_handler(unsigned long);

static int shutdown_wink_timerstart(void)
{
	apm_wink_timehandle++;

	init_timer(&apm_wink_timer);
	apm_wink_timer.function = shutdown_wink_timer_handler;
	apm_wink_timer.expires =jiffies + HZ / 4;
	add_timer(&apm_wink_timer);
	return 0;
}

static void shutdown_wink_timer_handler(unsigned long data)
{
	apm_wink_timehandle--;
	
	if (timerstart_enable)
		shutdown_wink_timerstart();
}

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
		/* Stop LED Wink */
		timerstart_enable = 0;
		if (apm_wink_timehandle) {
			del_timer(&apm_wink_timer);
			apm_wink_timehandle = 0;
		}
                return APM_SUCCESS;
        }

	return APM_SUCCESS;
}

static int set_power_state(u_short what, u_short state)
{
	DPRINTK("what=%d state=%d\n",what,state);

	switch(what) {
	case APM_DEVICE_ALL:
		switch(state) {
		case APM_STATE_OFF:
#ifdef DEBUG
			printk("set_power_state(APM_STATE_OFF)\n");
#endif
			break;
		case APM_STATE_SUSPEND:
			DPRINTK("*** go into suspend\n");
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
		  DPRINTK("APM_STATE_READY\n");
			send_event(APM_STATE_READY);
			break;
		case APM_STATE_STANDBY:
		  DPRINTK("APM_STATE_STANDBY\n");
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

	return 1;
}

static void apm_do_busy(void)
{
#if CONFIG_APM_BIOS
	u32	dummy;
#endif

	if (clock_slowed) {
#if CONFIG_APM_BIOS
		(void) apm_bios_call_simple(APM_FUNC_BUSY, 0, 0, &dummy);
#endif
		clock_slowed = 0;
	}
}
#endif

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
	(void) apm_set_power_state(APM_STATE_OFF);
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

	//DPRINTK("event=%d\n",event);

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

static void set_time(void)
{
}

static void get_time_diff(void)
{
}

static int send_event(apm_event_t event)
{
	switch (event) {
	case APM_SYS_SUSPEND:
	case APM_CRITICAL_SUSPEND:
	case APM_USER_SUSPEND:
		is_goto_suspend = 1;
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
		DPRINTK("sending PM_RESUME ... %d\n",event);
		(void) pm_send_all(PM_RESUME, (void *)0);
		DPRINTK("sending PM_RESUME ... %d done\n",event);
		is_goto_suspend = 0;
		break;
	case APM_STATE_STANDBY:
		if (is_goto_suspend)
                        return 1;
		(void) pm_send_all(PM_BLANK, NULL);
		(void) pm_send_all(PM_STANDBY, NULL);
		DPRINTK("sending PM_STANDBY ... %d done\n",event);
		break;
	case APM_STATE_READY:
		if (is_goto_suspend)
                        return 1;
		(void) pm_send_all(PM_UNBLANK, NULL);
		break;
	}

	return 1;
}

//#define SUSPEND_STANDBY_MODE 1
#undef SUSPEND_STANDBY_MODE

static int suspend_flag = 0;

static unsigned char scsmr2;
static unsigned char scscr2;
static unsigned char scbrr2;
static unsigned char scfcr2;
static int cpu_suspend_err;

#if defined(CONFIG_SH_RTS7751R2D)
#include <asm/irq.h>
#define TSTR		0xFFD80004
static  unsigned short	ipra;
static  unsigned char	tstr;
#endif

int cpu_suspend(void)
{
	int err;

#if defined(CONFIG_RTHAL)
	hard_cli();
#else
	cli();
#endif

	err = apm_set_power_state(APM_STATE_SUSPEND);
	if (err == APM_NO_ERROR)
		err = APM_SUCCESS;
	if (err != APM_SUCCESS)
		apm_error("suspend", err);

#if defined(CONFIG_CPU_SH7727)
	scsmr2 = ctrl_inb(SCSMR2);
	scscr2 = ctrl_inb(SCSCR2);
	scbrr2 = ctrl_inb(SCBRR2);
	scfcr2 = ctrl_inb(SCFCR2);

	ctrl_outb(0xbe,STBCR2);
	ctrl_outb(0xbb,STBCR3);

#ifdef SUSPEND_STANDBY_MODE
	ctrl_outw(0x5a00, WTCNT);
	ctrl_outw(0xa543, WTCSR);
#endif
#endif

#if defined(CONFIG_SH_RTS7751R2D)
	tstr = ctrl_inb(TSTR);
#endif

	ctrl_outb(0, TSTR);

#if defined(CONFIG_RTHAL)
	hard_sti();
#else
	sti();
#endif

#if defined(CONFIG_CPU_SUBTYPE_SH7727)
#ifdef SUSPEND_STANDBY_MODE
	ctrl_outb(0x85, STBCR);
	ctrl_outw(ctrl_inw(MCR) & 0xff78, MCR);
	ctrl_outw(0xa500, RTCNT);
	ctrl_outw(ctrl_inw(MCR) | 0x0006, MCR);
#else
	ctrl_outb(0x05, STBCR);
	suspend_flag = 1;

#if defined(CONFIG_SH_RTS7751R2D)
	ipra = ctrl_inw(INTC_IPRA);
	ctrl_outw((ipra & 0x000f),INTC_IPRA);
#endif
#endif

#else
#ifdef SUSPEND_STANDBY_MODE
#else
	suspend_flag = 1;
#endif
#endif

	asm volatile("sleep");

	return err;
}

/*
 * This function is called from "do_IRQ".
 */
void cpu_resume(void)
{
#if defined(CONFIG_CPU_SUBTYPE_SH7727)
#ifdef SUSPEND_STANDBY_MODE
	if ((ctrl_inb(STBCR) & 0x80) == 0)
		return;
#else
	if (suspend_flag == 0)
		return;
#endif
#else
#ifdef SUSPEND_STANDBY_MODE
#else
	if (suspend_flag == 0)
		return;
#endif
#endif

#if defined(CONFIG_CPU_SUBTYPE_SH7727)
	ctrl_outb(0, STBCR);
	ctrl_outb(0, STBCR2);
	ctrl_outb(0, STBCR3);

	ctrl_outb(scsmr2, SCSMR2);
	ctrl_outb(scscr2, SCSCR2);
	ctrl_outb(scbrr2, SCBRR2);
	ctrl_outb(scfcr2, SCFCR2);
#endif

#if defined(CONFIG_SH_RTS7751R2D)
	ctrl_outb(tstr, TSTR);
#else
	ctrl_outb(0x01, TSTR);
#endif

#ifdef SUSPEND_STANDBY_MODE
#else

#if defined(CONFIG_SH_RTS7751R2D)
	ctrl_outw(ipra,INTC_IPRA);
#endif

	suspend_flag = 0;
#endif
}

static int suspend(void)
{
	cpu_suspend_err = cpu_suspend();
	return 0;
}

static void standby(void)
{
	int	err;

	DPRINTK("\n");

	get_time_diff();
	err = apm_set_power_state(APM_STATE_STANDBY);

	DPRINTK("return from power_state\n");

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
	struct apm_user *	as;

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
		  DPRINTK("APM_SYS_STANDBY/APM_USER_STANDBY\n");
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
			queue_event(event, NULL);
			waiting_for_resume = 1;
			if (suspends_pending <= 0)
				(void) suspend();
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
		  DPRINTK("APM_STANDBY_RESUME 1\n");
				send_event(event);
		  DPRINTK("APM_STANDBY_RESUME 2\n");
				queue_event(event, NULL);
		  DPRINTK("APM_STANDBY_RESUME 3\n");
				mdelay(2000);
		  		for (as = user_list; as != NULL; as = as->next) {
					as->suspend_wait = 0;
					as->suspend_result = ((cpu_suspend_err == APM_SUCCESS) ? 0 : -EIO);
				}
				wake_up_interruptible(&apm_suspend_waitqueue);
			}
			break;

		case APM_CAPABILITY_CHANGE:
		case APM_LOW_BATTERY:
		case APM_POWER_STATUS_CHANGE:
		  //DPRINTK("APM_LOW_BATTERY 1\n");
			send_event(event);
			//DPRINTK("APM_LOW_BATTERY 2\n");
			queue_event(event, NULL);
			//DPRINTK("APM_LOW_BATTERY 3\n");
			break;

		case APM_UPDATE_TIME:
			set_time();
			break;

		case APM_CRITICAL_SUSPEND:
		  	printk("APM_CRITICAL_SUSPEND 1\n");
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
#define system_idle() (nr_running == 1)

static void apm_mainloop(void)
{
	int timeout = HZ;
	DECLARE_WAITQUEUE(wait, current);

	add_wait_queue(&apm_waitqueue, &wait);
	set_current_state(TASK_INTERRUPTIBLE);
	for (;;) {
		/* Nothing to do, just sleep for the timeout */
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
	struct apm_user * as;
	DECLARE_WAITQUEUE(wait, current);

	as = filp->private_data;
	if (check_apm_user(as, "ioctl"))
		return -EIO;
	if (!as->suser)
		return -EPERM;
	switch (cmd) {
	case APM_IOC_STANDBY:
		  DPRINTK("APM_IOC_STANDBY\n");
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
		  DPRINTK("APM_IOC_SUSPEND\n");
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

static int apm(void *unused)
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

	if (debug)
		printk(KERN_INFO "apm: Connection version %d.%d\n",
			(apm_info.connection_version >> 8) & 0xff,
			apm_info.connection_version & 0xff);

#ifdef CONFIG_APM_DO_ENABLE
	if (apm_info.bios.flags & APM_BIOS_DISABLED) {
		/*
		 * This call causes my NEC UltraLite Versa 33/C to hang if it
		 * is booted with PM disabled but not in the docking station.
		 * Unfortunate ...
		 */
		error = apm_enable_power_management(1);
		if (error) {
			apm_error("enable power management", error);
			return -1;
		}
	}
#endif

	if ((apm_info.bios.flags & APM_BIOS_DISENGAGED)
	    && (apm_info.connection_version > 0x0100)) {
		error = apm_engage_power_management(APM_DEVICE_ALL, 1);
		if (error) {
			apm_error("engage power management", error);
			return -1;
		}
	}

	if (debug && (smp_num_cpus == 1)) {
		unsigned char ac_line_status;
		unsigned char battery_status;
		unsigned char battery_flag;
		unsigned char battery_percentage;
		unsigned short battery_life;

		error = apm_get_power_status(&ac_line_status, &battery_status, 
				&battery_flag, &battery_percentage, &battery_life);
		if (error)
			printk(KERN_INFO "apm: power status not available\n");
		else {
			switch (ac_line_status) {
			case APM_AC_OFFLINE: power_stat = "off line"; break;
			case APM_AC_ONLINE: power_stat = "on line"; break;
			case APM_AC_BACKUP: power_stat = "on backup power"; break;
			default: power_stat = "unknown"; break;
			}
			switch (battery_status) {
			case APM_BATTERY_STATUS_HIGH: bat_stat = "high"; break;
			case APM_BATTERY_STATUS_LOW: bat_stat = "low"; break;
			case APM_BATTERY_STATUS_CRITICAL: bat_stat = "critical"; break;
			case APM_BATTERY_STATUS_CHARGING: bat_stat = "charging"; break;
			default: bat_stat = "unknown"; break;
			}
			printk(KERN_INFO 
				"apm: AC %s, battery status %s, battery life ",
				power_stat, bat_stat);
                        if (battery_percentage == 0xff)
                                printk("unknown\n");
                        else
                                printk("%d%%\n", battery_percentage);
			if (apm_info.connection_version > 0x100) {
				printk(KERN_INFO
				       "apm: battery flag 0x%02x, battery life ",
					battery_flag);
				if (battery_life == APM_BATTERY_LIFE_UNKNOWN)
					printk("unknown\n");
				else
					printk("%d %s\n", 
					battery_life & APM_BATTERY_LIFE_VALUE_MASK,
					(battery_life & APM_BATTERY_LIFE_MINUTES) ?
						"minutes" : "seconds");
			}
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

static void apm_hardware_init(void)
{
}

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

	apm_hardware_init();
	printk("apm: Hardware initialized\n");

	return 0;
}

#ifdef MODULE
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
#endif

module_init(apm_init);
#ifdef MODULE
module_exit(apm_exit);
#endif

MODULE_AUTHOR("Stephen Rothwell");
MODULE_DESCRIPTION("Advanced Power Management");
MODULE_PARM(debug, "i");
MODULE_PARM_DESC(debug, "Enable debug mode");
MODULE_PARM(power_off, "i");
MODULE_PARM_DESC(power_off, "Enable power off");
MODULE_PARM(bounce_interval, "i");
MODULE_PARM_DESC(bounce_interval, "Set the number of ticks to ignore suspend bounces");

EXPORT_NO_SYMBOLS;
