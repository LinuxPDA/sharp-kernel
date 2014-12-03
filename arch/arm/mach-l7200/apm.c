/*
 * linux/arch/arm/mach-l7200/apm.c
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
#include <asm/hardware.h>
#ifdef CONFIG_IRIS
#include <asm/arch/irqs.h>
#include <asm/arch/fpga_iris.h>
#endif	// CONFIG_IRIS
#include <asm/arch/power.h>

#ifdef CONFIG_IRIS
extern int iris_read_battery_mV(void);
extern int iris_read_backupbattery_mV(void);
//static int iris_read_battery_mV(void) {return 5000;}
extern int iris_read_battery_temp(void);
extern int set_led_status(int which,int status);

#define NEXT_GET_POWER_STATUS_DETECTS_MAIN_BAT  0
#define NEXT_GET_POWER_STATUS_DETECTS_BACK_BAT  1
static int iris_get_battery_mode = NEXT_GET_POWER_STATUS_DETECTS_MAIN_BAT;

#define BAT_THRE_CASE_H_TEMP_WITHOUT_COMM  0
#define BAT_THRE_CASE_H_TEMP_WITH_COMM  1
#define BAT_THRE_CASE_M_TEMP_WITHOUT_COMM  2  
#define BAT_THRE_CASE_M_TEMP_WITH_COMM  3
#define BAT_THRE_CASE_L_TEMP_WITHOUT_COMM  4
#define BAT_THRE_CASE_L_TEMP_WITH_COMM  5
#define BAT_THRE_CASE_C_TEMP_WITHOUT_COMM  6
#define BAT_THRE_CASE_C_TEMP_WITH_COMM  7

static int bat_threshold_cur_case = BAT_THRE_CASE_H_TEMP_WITHOUT_COMM;

#if 1
#define BATTERY_CRITICAL_THRESHOLD_TIMEOUT  (HZ*60*10)
#else /* for Debug */
#define BATTERY_CRITICAL_THRESHOLD_TIMEOUT  (HZ*30)
#endif

static struct {
  int full; 
  int high;    /* GOOD */
  int low;     /* Low */
  int critical;/* Very Low */
} iris_battery_thresh[] = {
  /* these are cases for under frontligh-OFF mode */
  { 4200, 3790, 3640, 3640 }, /* 10C or upper , Without COMM */
  { 4200, 3740, 3600, 3600 }, /* 10C or upper , With COMM */
  { 4200, 3700, 3550, 3550 }, /* 0 - 10C , Without COMM */
  { 4200, 3600, 3530, 3530 }, /* 0 - 10C , With COMM */
  { 4200, 3900, 3530, 3530 }, /* -10 - 0C , Without COMM */
  { 4200, 3850, 3530, 3530 }, /* -10 - 0C , With COMM */
  { 4200, 3950, 3530, 3530 }, /* -10 - 0C , Without COMM */
  { 4200, 4000, 3530, 3530 }, /* -10 - 0C , With COMM */
  /* these are cases for under frontligh-ON mode */
  { 4200, 3740, 3590, 3590 }, /* 10C or upper , Without COMM */
  { 4200, 3690, 3550, 3550 }, /* 10C or upper , With COMM */
  { 4200, 3650, 3530, 3530 }, /* 0 - 10C , Without COMM */
  { 4200, 3550, 3530, 3530 }, /* 0 - 10C , With COMM */
  { 4200, 3850, 3530, 3530 }, /* -10 - 0C , Without COMM */
  { 4200, 3800, 3530, 3530 }, /* -10 - 0C , With COMM */
  { 4200, 4000, 3530, 3530 }, /* -10 - 0C , Without COMM */
  { 4200, 4050, 3530, 3530 }, /* -10 - 0C , With COMM */
};

#define THRESHOLD_FL_ON_OFFSET  8

int iris_ac_line_status;	/* APM_AC_ONLINE / APM_AC_OFFLINE */
int iris_battery_charge_full;	/* 1 : full / 0:  */
int iris_battery_ok;		/* 1: OK / 0: FATAL */

int iris_get_on_mode = 0;

#endif	// CONFIG_IRIS

//#define DEBUG	1
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
#if 0
#define APM_CHECK_TIMEOUT	(HZ)
#else
/*#define APM_CHECK_TIMEOUT	(HZ*60*5)*/ /* Is this enough ? */
#define APM_CHECK_TIMEOUT	(HZ*2) /* Is this enough ? */
#endif

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


#ifdef CONFIG_IRIS /////////////// add-start
/*
 * Global
 */
int				autoPowerCancel = 0;
int				autoLightCancel = 0;

#include <linux/ioctl.h>

#include <asm/arch/iris_apm.h>

int iris_now_suspended = 0;

#endif  ////////////// add-end


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

static int __init apm_driver_version(u_short *val)
{
	*val = apm_bios_info.version;
	return APM_SUCCESS;
}

#ifdef CONFIG_IRIS

#include <asm/sharp_char.h>


/* ioctl */
extern u32 apm_wakeup_src_mask;
extern u32 apm_wakeup_factor;
static u32 apm_event_mask = (APM_EVT_POWER_BUTTON 
				| APM_EVT_BATTERY_FAULT | APM_EVT_BATTERY_STATUS);


#define POWER_SRC	(bitWSRC2 | bitWSRC8)

static struct timer_list iris_critical_timer;
static int battery_fatal_ratch = 0;
static int battery_critical_ratch = 0;

static void iris_set_led_status_in_apm(int which,int status)
{
  //printk("iris_set_led_status_in_apm %d\n",status);
  set_led_status(which,status);
}

static int critical_timer_running = 0;

static void battery_critical_handler(unsigned long __unused)
{
  DPRINTK("BATTERY FATAL TIMEOUT\n");
  critical_timer_running = 0;
  battery_critical_ratch = 1;
  send_event(APM_POWER_STATUS_CHANGE);
  queue_event(APM_POWER_STATUS_CHANGE,NULL);
  send_event(APM_LOW_BATTERY);
  queue_event(APM_LOW_BATTERY,NULL);
}

static void set_critical_bat_timer(void)
{
  if( ! critical_timer_running ){
    //printk("BATTERY FATAL COUNTDOWN START\n");
    del_timer(&iris_critical_timer);
    iris_critical_timer.function = battery_critical_handler;
    iris_critical_timer.expires = jiffies + BATTERY_CRITICAL_THRESHOLD_TIMEOUT;
    add_timer(&iris_critical_timer);
    critical_timer_running = 1;
  }
}

static void cancel_critical_bat_timer(void)
{
  del_timer(&iris_critical_timer);
  battery_critical_ratch = 0;
  battery_fatal_ratch = 0;
  critical_timer_running = 0;
}


static void DetectFatalHook(int mV)
{
  /* Prepare Counter and Add Count UP routine , if needed */
  if( ! battery_fatal_ratch ){
    DPRINTK("Critical Start Timer\n");
    set_critical_bat_timer();
    return;
  }
  battery_fatal_ratch = 1;
  DPRINTK("DetectFatalHook\n");
  send_event(APM_POWER_STATUS_CHANGE);
  queue_event(APM_POWER_STATUS_CHANGE,NULL);
  send_event(APM_LOW_BATTERY);
  queue_event(APM_LOW_BATTERY,NULL);
}

static void DetectACInputHook(void)
{
  DPRINTK("BATTERY ACInputHook\n");
  cancel_critical_bat_timer();
}

static void DetectGoodHook(int mv)
{
  DPRINTK("BATTERY GOOD HOOK\n");
  cancel_critical_bat_timer();
}

static void DetectLowHook(int mv)
{
  DPRINTK("BATTERY LOW HOOK\n");
  cancel_critical_bat_timer();
}

static int GetFixedUpVoltage(void)
{
  int voltage;
  voltage = iris_read_battery_mV();
  return voltage; /* Currently , return directly current value */
}

static int GetFixedUpBackupVoltage(void)
{
  int voltage;
  voltage = iris_read_backupbattery_mV();
  return voltage; /* Currently , return directly current value */
}

static void VoltageDetectHook(int mv)
{
  if ( mv < iris_battery_thresh[bat_threshold_cur_case].critical) {
    if( iris_get_battery_mode != NEXT_GET_POWER_STATUS_DETECTS_BACK_BAT ){
      DetectFatalHook(mv);
      iris_set_led_status_in_apm(SHARP_LED_BATTERY,LED_BATTERY_VERY_LOW);
    }
  } else if( mv < iris_battery_thresh[bat_threshold_cur_case].high) {
    if( iris_get_battery_mode != NEXT_GET_POWER_STATUS_DETECTS_BACK_BAT ){
      DetectLowHook(mv);
      iris_set_led_status_in_apm(SHARP_LED_BATTERY,LED_BATTERY_LOW);
    }
  } else {
    if( iris_get_battery_mode != NEXT_GET_POWER_STATUS_DETECTS_BACK_BAT ){
      DetectGoodHook(mv);
      iris_set_led_status_in_apm(SHARP_LED_BATTERY,LED_BATTERY_GOOD);
    }
  }
}

static void VoltageStatusSuspendHook(void)
{
  battery_critical_ratch = 0;
  battery_fatal_ratch = 0;
  critical_timer_running = 0;
}

static void VoltageStatusResumeHook(void)
{
  battery_critical_ratch = 0;
  battery_fatal_ratch = 0;
  critical_timer_running = 0;
}

extern int gsm_modem_status;

extern void (*set_charge_led_hook)(int);

void send_charger_status_changed_event(int status)
{
  send_event(APM_POWER_STATUS_CHANGE);
  queue_event(APM_POWER_STATUS_CHANGE,NULL);
}


void set_current_power_cond(void)
{
  int temp = 0;
  int t = 0;
  temp = iris_read_battery_temp();
  if( temp > 100 ){
    t = 0;
  }else if( temp > 0 ){
    t = 1;
  }else if( temp > -100 ){
    t = 2;
  }else{
    t = 3;
  }
  DPRINTK("APM:set_current_power_cond %d %d\n",temp,gsm_modem_status);
  bat_threshold_cur_case = t * 2;
  if( gsm_modem_status == GSM_PHONE_IN_ANALOG_MODE
      || gsm_modem_status == GSM_PHONE_IN_DATA_MODE ){
    bat_threshold_cur_case++;
  }
  if( ( FPGA_GPO & (1<<9) ) ){
    /* currently , 100% duty */
    bat_threshold_cur_case += THRESHOLD_FL_ON_OFFSET;
  }else if( IO_PMPCON & 0x00ff ){
    /* any of PWM1{H,L} are not zero */
    bat_threshold_cur_case += THRESHOLD_FL_ON_OFFSET;
  }
  DPRINTK("APM:set_current_power_cond = %d\n",bat_threshold_cur_case);
}

static void powerirq_handler(int irq, void *dev_id, struct pt_regs *regs)
{
	int irqs;

	if( IO_FPGA_Status & bitWSRC0 ){
	  IO_FPGA_ECL |= bitWSRC0;  /* clear WSRC0 edge int. */
	  IO_FPGA_CLEX = bitWSRC0;  /* clear WSRC0 edge int. */
	}

	if( iris_now_suspended ) return;

	if (IO_FPGA_Status & POWER_SRC) {
		DPRINTK("irq=%d FPGA_Status=%04x\n",irq,IO_FPGA_Status);

		irqs = IO_FPGA_Status & POWER_SRC;
		disable_fpga_irq(irqs);
		clear_fpga_int(irqs);

		if (irqs & bitWSRC2) {	/* Externel Power */
			if (IO_FPGA_LSEL & bitWSRC2) {
				iris_ac_line_status = APM_AC_OFFLINE;
        			set_fpga_int_trigger_lowlevel(bitWSRC2);
				terminate_charger();
				{
				  int voltage;
				  voltage = GetFixedUpVoltage();
				  set_current_power_cond();
				  if (voltage >= 0) {
				    VoltageDetectHook(voltage);
				  }
				}
			} else {
				iris_ac_line_status = APM_AC_ONLINE;
        			set_fpga_int_trigger_highlevel(bitWSRC2);
				DetectACInputHook();
				start_charger();
			}

			send_event(APM_POWER_STATUS_CHANGE);
			queue_event(APM_POWER_STATUS_CHANGE,NULL);

		}
		if (irqs & bitWSRC8) {	/* Detect Battery Cover */
			if (IO_FPGA_LSEL & bitWSRC8) {
			  /* Battery Cover Open !! */
			  extern void emerge_off(void);
				iris_battery_ok = 0;
				set_fpga_int_trigger_lowlevel(bitWSRC8);				emerge_off();
			} else {
				iris_battery_ok = 1;
				set_fpga_int_trigger_highlevel(bitWSRC8);
			}
		}
#if 0
		if (irqs & bitWSRC9) {	/* Battery Charge Full */
			if (IO_FPGA_LSEL & bitWSRC9) {
				iris_battery_charge_full = 1;
				set_fpga_int_trigger_lowlevel(bitWSRC9);
			} else {
				iris_battery_charge_full = 0;
				set_fpga_int_trigger_highlevel(bitWSRC9);
			}
		}
#endif

		clear_fpga_int(irqs);
		enable_fpga_irq(irqs);

		DPRINTK("iris_ac_line_status=%d iris_battery_ok=%d iris_battery_charge_full=%d\n",iris_ac_line_status,iris_battery_ok,iris_battery_charge_full);
	}
}
#endif	// CONFIG_IRIS

#ifdef CONFIG_IRIS
static int reported_lowbat = 0;
#endif	// CONFIG_IRIS

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

#ifdef CONFIG_IRIS
	/* Power Off (Suspend) Button */
#if 0
	if (0) {
		/* power on/off button is depressed */
		DPRINTK("power button\n");
		*event = APM_USER_SUSPEND;
		return APM_SUCCESS;
	}
#endif
	/* AC Status Changed */
	if( 0 ){
	  static int prev_ac_status = APM_AC_UNKNOWN;
	  if( iris_ac_line_status != prev_ac_status ){
	    DPRINTK("ZZZ AC Status Changed\n");
	    /* AC Status Changed */
	    *event = APM_POWER_STATUS_CHANGE;
	    prev_ac_status = iris_ac_line_status;
	    return APM_SUCCESS;
	  }else{
	    /* AC Status Not-Changed */
	  }
	}
	/* Battery Charging Status Changed */
	if( 0 ){
	  extern int charger_is_charging(void);
	  static int prev_charge_status = -1;
	  int s;
	  s = charger_is_charging();
	  if( s != prev_charge_status ){
	    DPRINTK("ZZZ Charger Status Changed\n");
	    /* Charger Status Changed */
	    *event = APM_POWER_STATUS_CHANGE;
	    prev_charge_status = s;
	    return APM_SUCCESS;
	  }else{
	    /* AC Status Not-Changed */
	  }
	}
	/* Battery NG */
	if ( apm_event_mask & APM_EVT_BATTERY_FAULT ){
	  if (!iris_battery_ok && iris_ac_line_status == APM_AC_OFFLINE) {
	    DPRINTK("battery fatal\n");
	    *event = APM_CRITICAL_SUSPEND;
	    return APM_SUCCESS;
	  }
	}
	/* Battery Charge FULL */
	if ( apm_event_mask & APM_EVT_BATTERY_STATUS ){
	  if (iris_ac_line_status == APM_AC_ONLINE) {
		static int is_report = 0;
	       	if (iris_battery_charge_full) {
			if (!is_report) {
				is_report = 1;
				DPRINTK("power status  change [charge full]\n");
				*event = APM_POWER_STATUS_CHANGE;
				return APM_SUCCESS;
			}
			iris_battery_charge_full = 0;
		} else {
			is_report = 0;
		}
	  }
	}
	/* Batery Status */
	if ( apm_event_mask & APM_EVT_BATTERY_STATUS )
	{
		static int is_first = 1;
		static int prev_voltage;
		static int prev_ac_line_status;
		int voltage;

		voltage = GetFixedUpVoltage();
		set_current_power_cond();
		if (voltage >= 0) {
		  VoltageDetectHook(voltage);
			if (iris_ac_line_status == APM_AC_OFFLINE
					&& voltage < iris_battery_thresh[bat_threshold_cur_case].critical) {
			  //DPRINTK("XXXXXXXXXX %d\n",voltage);
			  if( ! reported_lowbat ){
			    reported_lowbat = 1;
				*event = APM_LOW_BATTERY;
				return APM_SUCCESS;
			  }
			}
			if (prev_ac_line_status != iris_ac_line_status ||
					(prev_voltage-10 > voltage
						|| voltage > prev_voltage+10) ) {
#if 0
				if (prev_ac_line_status != iris_ac_line_status) {
					/* Flont Light contrast */
					struct pm_dev *dev;
				       	dev = pm_find(PM_ILLUMINATION_DEV, NULL);
					if (dev && blank_state != APM_STATE_STANDBY) {
						DPRINTK("FL contrust :callback=%08x\n",
							dev->callback);
						pm_send(dev, PM_BLANK, NULL);
						pm_send(dev, PM_UNBLANK, NULL);
					}
				}
#endif
				DPRINTK("power status  change AC=%d V=%d->%d\n",
					(prev_ac_line_status != iris_ac_line_status),
					prev_voltage, voltage);
				prev_ac_line_status = iris_ac_line_status;
				prev_voltage = voltage;
				if (!is_first) {
					*event = APM_POWER_STATUS_CHANGE;
					return APM_SUCCESS;
				}
			}
			is_first = 0;
		}
	}
#endif	// CONFIG_IRIS
	return APM_SUCCESS;
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
			l7200_suspend();
			resume_notice = 1;
			break;
		case APM_STATE_STANDBY:
			DPRINTK("*** go into standby\n");
			send_event(APM_STATE_STANDBY);
#ifdef CONFIG_IRIS
			DPRINTK("--- standby Enter\n");
			l7200_standby();
			DPRINTK("--- standby Exit\n");
			send_event(APM_STATE_READY);
			DPRINTK("--- done send_eventf\n");
			DPRINTK("--- done APM_STATE_STANDBY\n");
#endif /* CONFIG_IRIS */
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

#ifdef CONFIG_IRIS
static int iris_in_idle = 0;
#undef INTERNAL_CLOCK_DOWN_ON_IDLE
static unsigned long run_rfsh_val = 0;
#define SDRAM_RFSH_MASK 0xffff
#define ALTD_EN_BITS  (1 << 9)  /* means ALTD_EN at idle */
#define ALTD_SEL_BITS  (1 << 13)  /* means PLLCLKRAW / 4 at ALTD_EN */
#endif

#ifdef CONFIG_APM_CPU_IDLE
static int apm_do_idle(void)
{
#ifdef CONFIG_IRIS

  unsigned long flags;

  extern int l7200_idle(void);

  IO_IRIS1_DEBUG_LED = 0x01;

  save_flags(flags); cli();

  if( ! iris_in_idle ){
#ifdef INTERNAL_CLOCK_DOWN_ON_IDLE
    run_rfsh_val = IO_SDRAMRFSH & SDRAM_RFSH_MASK;
    IO_SYS_CLOCK_SELECT |= ALTD_SEL_BITS;
    IO_SDRAMRFSH = run_rfsh_val >> 2;
#endif /* INTERNAL_CLOCK_DOWN_ON_IDLE */
    iris_in_idle = 1;
  }

#ifdef INTERNAL_CLOCK_DOWN_ON_IDLE
  IO_SYS_CLOCK_ENABLE |= ALTD_EN_BITS;
#endif /* INTERNAL_CLOCK_DOWN_ON_IDLE */

  restore_flags(flags);

  IO_IRIS1_DEBUG_LED = 0x03;

  l7200_idle();

  IO_IRIS1_DEBUG_LED = 0x01;


#else /* ! CONFIG_IRIS */



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



#endif /* ! CONFIG_IRIS */





	return 1;
}

static void apm_do_busy(void)
{

#if CONFIG_IRIS

  unsigned long	flags;

  save_flags(flags); cli();

#ifdef INTERNAL_CLOCK_DOWN_ON_IDLE
  IO_SYS_CLOCK_ENABLE &= ~ALTD_EN_BITS;
#endif /* INTERNAL_CLOCK_DOWN_ON_IDLE */

  if( iris_in_idle ){ 
#ifdef INTERNAL_CLOCK_DOWN_ON_IDLE
    IO_SDRAMRFSH = run_rfsh_val;
    //IO_SYS_CLOCK_SELECT |= ALTD_SEL_BITS;
#endif /* INTERNAL_CLOCK_DOWN_ON_IDLE */

    IO_IRIS1_DEBUG_LED = 0x00;

    iris_in_idle = 0;
  }

  restore_flags(flags);

  IO_IRIS1_DEBUG_LED = 0x04;

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
#ifdef CONFIG_IRIS
	suspend();
#else	// CONFIG_IRIS
	(void) apm_set_power_state(APM_STATE_OFF);
#endif	// CONFIG_IRIS
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

#ifdef CONFIG_IRIS
static int iris_apm_get_power_status(u_char *ac_line_status,
                                u_char *battery_status,
                                u_char *battery_flag,
                                u_char *battery_percentage,
                                u_short *battery_life)
{
	int voltage;
	if( iris_get_battery_mode == NEXT_GET_POWER_STATUS_DETECTS_BACK_BAT ){
	  /* detect Backup Battery */
	  voltage = GetFixedUpBackupVoltage();
	}else{
	  /* detect Main Battery */
	  voltage = GetFixedUpVoltage();
	}

	if (voltage < 0)
		return voltage;

	set_current_power_cond();

	VoltageDetectHook(voltage);

	*ac_line_status =  iris_ac_line_status;

	if( iris_get_battery_mode == NEXT_GET_POWER_STATUS_DETECTS_BACK_BAT ){

		*battery_status = APM_BATTERY_STATUS_HIGH;
		*battery_flag = (1 << 0);

	} else if( battery_critical_ratch ) {
	  //printk("battery_critical_ratch\n");
		*battery_status = APM_BATTERY_STATUS_CRITICAL;
		*battery_flag = (1 << 2);
		iris_set_led_status_in_apm(SHARP_LED_BATTERY,LED_BATTERY_VERY_LOW);
	} else if (voltage < iris_battery_thresh[bat_threshold_cur_case].low) {
		*battery_status = APM_BATTERY_STATUS_VERY_LOW;
		*battery_flag = (1 << 1);
		iris_set_led_status_in_apm(SHARP_LED_BATTERY,LED_BATTERY_VERY_LOW);
	} else if (voltage < iris_battery_thresh[bat_threshold_cur_case].high) {
		*battery_status = APM_BATTERY_STATUS_LOW;
		*battery_flag = (1 << 1);
		iris_set_led_status_in_apm(SHARP_LED_BATTERY,LED_BATTERY_LOW);
	} else {
		*battery_status = APM_BATTERY_STATUS_HIGH;
		*battery_flag = (1 << 0);
		iris_set_led_status_in_apm(SHARP_LED_BATTERY,LED_BATTERY_GOOD);
	}

	if (iris_ac_line_status == APM_AC_ONLINE) {
		*battery_status = APM_BATTERY_STATUS_CHARGING;
		*battery_flag |= (1 << 3);
	}

	if (iris_ac_line_status == APM_AC_ONLINE && iris_battery_charge_full) {
		*battery_percentage = 100;
	} else {
	  if( voltage > iris_battery_thresh[bat_threshold_cur_case].full ){
	    *battery_percentage = 100;
	  }else if( voltage > iris_battery_thresh[bat_threshold_cur_case].high ){
	    *battery_percentage = 100;
	  }else if( voltage > iris_battery_thresh[bat_threshold_cur_case].low ){
	    *battery_percentage = 50;
	  }else if( voltage > iris_battery_thresh[bat_threshold_cur_case].critical ){
	    *battery_percentage = 0;
	  }else{
	    *battery_percentage = 0;
	  }
	}
	*battery_life = APM_BATTERY_LIFE_UNKNOWN;

	return APM_SUCCESS;
}
#endif	// CONFIG_IRIS

static int apm_get_power_status(u_char *ac_line_status,
                                u_char *battery_status,
                                u_char *battery_flag,
                                u_char *battery_percentage,
                                u_short *battery_life)
{
#ifdef CONFIG_IRIS
        iris_apm_get_power_status(ac_line_status, 
			battery_status, battery_flag, battery_percentage, battery_life);
#endif	// CONFIG_IRIS
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
		DPRINTK(KERN_NOTICE "apm: %s: %s\n", str, error_table[i].msg);
	else
		DPRINTK(KERN_NOTICE "apm: %s: unknown error code %#2.2x\n",
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
			    DPRINTK(KERN_ERR "apm: an event queue overflowed\n");
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
	return (unsigned long)(IO_RTCDR);
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

static int send_event(apm_event_t event)
{
  //DPRINTK("event=%d\n",event);

	switch (event) {
	case APM_SYS_SUSPEND:
	case APM_CRITICAL_SUSPEND:
	case APM_USER_SUSPEND:
		is_goto_suspend = 1;
		/* map all suspends to ACPI D3 */
		if (pm_send_all(PM_SUSPEND, (void *)3)) {
			if (event == APM_CRITICAL_SUSPEND) {
				DPRINTK(KERN_CRIT "apm: Critical suspend was vetoed, expect armageddon\n" );
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

/* these functions must be called on scheduling enabled */
extern void iris_ssp_before_suspend_hook_under_sche(void);
extern void iris_ssp_after_suspend_hook_under_sche(void);


static int suspend(void)
{
	int		err;
	struct apm_user	*as;

	DPRINTK("\n");

#ifdef CONFIG_IRIS
	iris_now_suspended = 1;
	iris_ssp_before_suspend_hook_under_sche();
#endif /* CONFIG_IRIS */

	VoltageStatusSuspendHook();

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

#ifdef CONFIG_IRIS
	iris_ssp_after_suspend_hook_under_sche();
	iris_now_suspended = 0;
#endif /* CONFIG_IRIS */

	sti();
	queue_event(APM_NORMAL_RESUME, NULL);

	VoltageStatusResumeHook();

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

#ifdef CONFIG_IRIS
	iris_now_suspended = 1;
	iris_ssp_before_suspend_hook_under_sche();
#endif /* CONFIG_IRIS */

	VoltageStatusSuspendHook();

	get_time_diff();
#ifdef CONFIG_IRIS
	cli();
#endif /* CONFIG_IRIS */
	err = apm_set_power_state(APM_STATE_STANDBY);

	DPRINTK("return from power_state\n");

#ifdef CONFIG_IRIS
	reinit_timer();
	set_time();
#endif /* CONFIG_IRIS */
	if ((err != APM_SUCCESS) && (err != APM_NO_ERROR))
		apm_error("standby", err);
#ifdef CONFIG_IRIS
	iris_ssp_after_suspend_hook_under_sche();
	iris_now_suspended = 0;

	DPRINTK("interrupt enabling\n");

	sti();

	VoltageStatusResumeHook();

	DPRINTK("done\n");
#endif /* CONFIG_IRIS */
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

#ifdef CONFIG_IRIS
	//DPRINTK("\n");
	reported_lowbat = 0;

	if( iris_now_suspended ) return;
#endif	// CONFIG_IRIS

	while ((event = get_event()) != 0) {
		if (debug) {
			if (event <= NR_APM_EVENT_NAME)
				DPRINTK(KERN_DEBUG "apm: received %s notify\n",
				       apm_event_name[event - 1]);
			else
				DPRINTK(KERN_DEBUG "apm: received unknown "
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
			if (waiting_for_resume)
				return;
			DPRINTK("APM_SYS_SUSPEND\n");
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
		  DPRINTK("APM_STANDBY_RESUME 1\n");
				send_event(event);
		  DPRINTK("APM_STANDBY_RESUME 2\n");
				queue_event(event, NULL);
		  DPRINTK("APM_STANDBY_RESUME 3\n");
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
		  DPRINTK("APM_CRITICAL_SUSPEND 1\n");
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
				DPRINTK(KERN_DEBUG "apm: setting state busy\n");
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
		DPRINTK(KERN_ERR "apm: %s passed bad filp\n", func);
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

#ifdef CONFIG_IRIS
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
	case APM_IOC_GET_AC_STATUS: {
	  return (( iris_ac_line_status == APM_AC_ONLINE ) ? APM_AC_ONLINE : APM_AC_OFFLINE);
	} break;

	case APM_IOCGWUPSRC:
		return (int)apm_wakeup_src_mask;
	case APM_IOCSWUPSRC:
		{
			int tmp = (int)apm_wakeup_src_mask;
			apm_wakeup_src_mask = arg;
			return tmp;
		}
	case APM_IOCGWUPFACT:
		return (int)apm_wakeup_factor;
	case APM_IOCGEVTSRC:
		return (int)apm_event_mask;
	case APM_IOCSEVTSRC:
		{
			int tmp = (int)apm_event_mask;
			apm_event_mask = arg;
			return tmp;
		}

	case APM_IOC_BATTERY_MAIN_MODE: {
	  iris_get_battery_mode = NEXT_GET_POWER_STATUS_DETECTS_MAIN_BAT;
	  return 1;
	} break;
	case APM_IOC_BATTERY_BACK_MODE: {
	  iris_get_battery_mode = NEXT_GET_POWER_STATUS_DETECTS_BACK_BAT;
	  return 1;
	} break;


        case APM_IOC_GET_ON_MODE: {
          return iris_get_on_mode;
        } break;
	
        case APM_IOC_RESET: {
#define MMCCT_ENABLE      0x01
#define SYSCLOCK_MIRM_EN  ((__u32)0x40)
	  IO_MMCCT &= ~MMCCT_ENABLE;
	  IO_SYS_CLOCK_ENABLE &= ~SYSCLOCK_MIRM_EN;
	  machine_restart(NULL);
        } break;

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
			DPRINTK(KERN_ERR "apm: filp not in user list\n");
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
		DPRINTK(KERN_ERR "apm: cannot allocate struct of size %d bytes\n",
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
		DPRINTK(KERN_INFO "apm: Connection version %d.%d\n",
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
			DPRINTK(KERN_INFO "apm: power status not available\n");
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
			DPRINTK(KERN_INFO 
				"apm: AC %s, battery status %s, battery life ",
				power_stat, bat_stat);
                        if (battery_percentage == 0xff)
                                DPRINTK("unknown\n");
                        else
                                DPRINTK("%d%%\n", battery_percentage);
			if (apm_info.connection_version > 0x100) {
				DPRINTK(KERN_INFO
				       "apm: battery flag 0x%02x, battery life ",
					battery_flag);
				if (battery_life == APM_BATTERY_LIFE_UNKNOWN)
					DPRINTK("unknown\n");
				else
					DPRINTK("%d %s\n", 
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
#ifndef CONFIG_IRIS
		console_blank_hook = apm_console_blank;
#endif /* CONFIG_IRIS */
#endif
		apm_mainloop();
#if defined(CONFIG_APM_DISPLAY_BLANK) && defined(CONFIG_VT)
#ifndef CONFIG_IRIS
		console_blank_hook = NULL;
#endif /* CONFIG_IRIS */
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
	apm_info.bios = apm_bios_info;
	if (apm_info.bios.version == 0) {
		DPRINTK(KERN_INFO "apm: BIOS not found.\n");
		return -ENODEV;
	}
	DPRINTK(KERN_INFO
		"apm: BIOS version %d.%d Flags 0x%02x (Driver version %s)\n",
		((apm_info.bios.version >> 8) & 0xff),
		(apm_info.bios.version & 0xff),
		apm_info.bios.flags,
		driver_version);
	if ((apm_info.bios.flags & APM_32_BIT_SUPPORT) == 0) {
		DPRINTK(KERN_INFO "apm: no 32 bit BIOS support\n");
		return -ENODEV;
	}

	/*
	 * Fix for the Compaq Contura 3/25c which reports BIOS version 0.1
	 * but is reportedly a 1.0 BIOS.
	 */
	if (apm_info.bios.version == 0x001)
		apm_info.bios.version = 0x100;

	if (apm_disabled) {
		DPRINTK(KERN_NOTICE "apm: disabled on user request.\n");
		return -ENODEV;
	}
	if ((smp_num_cpus > 1) && !power_off) {
		DPRINTK(KERN_NOTICE "apm: disabled - APM is not SMP safe.\n");
		return -ENODEV;
	}
	if (PM_IS_ACTIVE()) {
		DPRINTK(KERN_NOTICE "apm: overridden by ACPI.\n");
		return -ENODEV;
	}
	pm_active = 1;

#ifdef CONFIG_IRIS
	set_charge_led_hook = send_charger_status_changed_event;
	iris_get_on_mode = 0; /* get boot mode */
	bat_threshold_cur_case = BAT_THRE_CASE_H_TEMP_WITHOUT_COMM;
	iris_init_ssp_sys(); /* init SSP for voltage check */
	clear_fpga_int(POWER_SRC);
	iris_ac_line_status = APM_AC_ONLINE;
        set_fpga_int_trigger_lowlevel(bitWSRC2);	/* Externel Power */
	/* read AC LINE STATUS */
	iris_ac_line_status = ( ( IO_FPGA_RawStat & bitWSRC2 ) ? APM_AC_ONLINE : APM_AC_OFFLINE );
	if( iris_ac_line_status == APM_AC_ONLINE ){
	  set_fpga_int_trigger_highlevel(bitWSRC2);
	  /* begin battery charge */
	  start_charger();
	}else{
	  terminate_charger();
	  set_fpga_int_trigger_lowlevel(bitWSRC2);
	}
	DPRINTK("apm: Current AC Status : %d\n",iris_ac_line_status);
	iris_battery_ok = 1;
	set_fpga_int_trigger_lowlevel(bitWSRC8);	/* Detect Battery Cover */
	iris_battery_charge_full = 0;
#if 0
	set_fpga_int_trigger_highlevel(bitWSRC9);	/* Battery Charge Full */
#endif
	if (request_irq(IRQ_INTF,
			powerirq_handler, SA_SHIRQ, "apm", powerirq_handler)) {
		return -ENODEV;
	}
	clear_fpga_int(POWER_SRC);
	enable_fpga_irq(POWER_SRC);
	init_timer(&iris_critical_timer);

#if 0 /* Checks Current SDRAM Settings */
	printk("======= Checinking Current SDRAM Settings ===========\n");
	printk("SDRAMCFG  = %x\n",IO_SDRAMCFG);
	printk("SDRAMRFSH = %x\n",IO_SDRAMRFSH);
	printk("SDRAMWBFT = %x\n",IO_SDRAMWBFT);
	{
	  int i = 0;
	  int j = 0;
	  unsigned char* p = (unsigned char*)(FLASH2_BASE+0x7f0000);
	  printk("======= Dumping From %p ===========\n",p);
	  for(i=0;i<0x100;i+=0x10){
	    printk("%p :",p);
	    for(j=0;j<0x10;j++){
	      printk("%x ",*p);
	      p++;
	    }
	    printk("\n");
	  }
	}
#endif
#ifdef INTERNAL_CLOCK_DOWN_ON_IDLE
	//printk("SettingUp ALTD_EN\n");
	run_rfsh_val = IO_SDRAMRFSH & SDRAM_RFSH_MASK;
	//printk("SettingUp ALTD_EN : done (%x)\n",run_rfsh_val);
#endif


#endif	// CONFIG_IRIS

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

#ifdef CONFIG_IRIS
	free_irq(IRQ_INTF, powerirq_handler);
#endif	// CONFIG_IRIS

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

EXPORT_NO_SYMBOLS;
