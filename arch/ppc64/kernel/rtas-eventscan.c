/*
 *  arch/ppc64/kernel/rtas-eventscan.c
 *  
 *  Copyright (c) 2000 Tilmann Bitterberg
 *  (tilmann@bitterberg.de)
 *
 *  Error processing of errors found by rtas even-scan routine
 *  which is done with every heartbeat. (chrp_setup.c)
 */

#include <linux/sched.h>
#include <linux/slab.h>

#include <asm/machdep.h> /* for ppc_md */
#include <asm/rtas.h>
#include <asm/rtas-eventscan.h>

static long rtas_handle_error_log(struct rtas_error_log *, long, char *);
static const char *rtas_errorlog_check_type (struct rtas_error_log *);

/* ****************************************************************** */

#define rtas_errorlog_check_severity(x) \
        (_errlog_severity[x->severity])
#define rtas_errorlog_check_target(x) \
        (_errlog_target[x->target])
#define rtas_errorlog_check_initiator(x) \
        (_errlog_initiator[x->initiator])
#define rtas_errorlog_check_extended(x) \
        (_errlog_extended[x->extended])
#define rtas_errorlog_disect_extended(x) \
        do { /* implement me */ } while(0)

/* ****************************************************************** */
char rtas_event_buffer[NR_CPUS][1024];

long
rtas_event_scan(void)
{
        struct rtas_error_log *log = rtas.event_scan.log;
        unsigned long size = rtas.event_scan.log_size;
	long cnt = 0;
	char *buf;
		
	buf = rtas_event_buffer[smp_processor_id()];

        for(;;) {
                long status = 1;
                call_rtas( "event-scan", 4, 1, &status, EVENT_SCAN_ALL_EVENTS,
                        0, __pa(log), size);

                if ( status == 1 ) /* No errors/events found */
                        break;

                /* New error log returned */
		cnt += 1;
		if (rtas_handle_error_log(log, status, buf) != 0)
			printk("%s", buf);

		if (status == -1) {
                	/* Hardware error */
                	panic("RTAS: event-scan reported hardware error\n");
			return 0;
		}
        }
        ppc_md.heartbeat_count = ppc_md.heartbeat_reset;
	return cnt;
}


/* ****************************************************************** */
/* 
 * EVENT-SCAN
 * The whole stuff below here doesn't take any action when it found
 * an error, it just prints as much information as possible and 
 * then its up to the user to decide what to do.
 *
 * Returns 0 if no errors were found
 * Returns 1 if there may be more errors
 */
static long
rtas_handle_error_log(struct rtas_error_log *log, long status, char *buffer)
{
const char *_errlog_severity[] = {
#ifdef VERBOSE_ERRORS
	"No Error\n\t\
Should require no further information",
	"Event\n\t\
This is not really an error, it is an event. I use events\n\t\
to communicate with RTAS back and forth.",
	"Warning\n\t\
Indicates a non-state-losing error, either fully recovered\n\t\
by RTAS or not needing recovery. Ignore it.",
	"Error sync\n\t\
May only be fatal to a certain program or thread. Recovery\n\t\
and continuation is possible, if I only had a handler for\n\t\
this. Less serious",
	"Error\n\t\
Less serious, but still causing a loss of data and state.\n\t\
I can't tell you exactly what to do, You have to decide\n\t\
with help from the target and initiator field, what kind\n\t\
of further actions may take place.",
	"Fatal\n\t\
Represent a permanent hardware failure and I believe this\n\t\
affects my overall performance and behaviour. I would not\n\t\
attempt to continue normal operation."
#else
	"No Error",
	"Event",
	"Warning",
	"Error sync",
	"Error",
	"Fatal"
#endif /* VERBOSE_ERRORS */
};

const char *_errlog_extended[] = {
#ifdef VERBOSE_ERRORS
	"Not present\n\t\
Sad, the RTAS call didn't return an extended error log.",
	"Present\n\t\
The extended log is present and hopefully it contains a lot of\n\t\
useful information, which leads to the solution of the problem."
#else
	"Not present",
	"Present"
#endif /* VERBOSE_ERRORS */
};

const char *_errlog_initiator[] = { 
	"Unknown or not applicable",
	"CPU",
	"PCI",
	"ISA",
	"Memory",
	"Power management"
};

const char *_errlog_target[] = { 
	"Unknown or not applicable",
	"CPU",
	"PCI",
	"ISA",
	"Memory",
	"Power management"
};
	int n = 0;

	if (status == -1) {
		n += sprintf ( buffer+n, KERN_WARNING "event-scan: "
			"Unable to get errors. Do you a "
			"favor and throw this box away\n");
		return n;
	}
	n += sprintf ( buffer+n, KERN_INFO "event-scan: "
			"Error Log version %u\n", log->version);

	switch (log->disposition) {
		case DISP_FULLY_RECOVERED:
			/* there was an error, but everything is fine now */
			break;
		case DISP_NOT_RECOVERED:
			n += sprintf (buffer+n, KERN_WARNING "event-scan: "
				"We have a really serious problem!\n");
		case DISP_LIMITED_RECOVERY:
			n += sprintf (buffer+n, KERN_WARNING "event-scan: "
				"Error classification\n");
			n += sprintf (buffer+n, KERN_WARNING "event-scan: "
				"Severity  : %s\n", 
				rtas_errorlog_check_severity (log));
			n += sprintf (buffer+n, KERN_WARNING "event-scan: "
				"Initiator : %s\n", 
				rtas_errorlog_check_initiator (log));
			n += sprintf (buffer+n, KERN_WARNING "event-scan: "
				"Target    : %s\n", 
				rtas_errorlog_check_target (log));
			n += sprintf (buffer+n, KERN_WARNING "event-scan: "
				"Type      : %s\n", 
				rtas_errorlog_check_type (log));
			n += sprintf (buffer+n, KERN_WARNING "event-scan: "
				"Ext. log  : %s\n", 
				rtas_errorlog_check_extended (log));
			if (log->extended)
				rtas_errorlog_disect_extended (log);
			break;
		default:
			/* nothing */
			break;
	}
	return n;
}

/* ****************************************************************** */

static const char *
rtas_errorlog_check_type (struct rtas_error_log *log)
{
	const char *_errlog_type[] = {
		"unknown type",
		"too many tries failed",
		"TCE error",
		"RTAS device failed",
		"target timed out",
		"parity error on data",			/* 5 */
		"parity error on address",
		"parity error on external cache",
		"access to invalid address",
		"uncorrectable ECC error",
		"corrected ECC error"			/* 10 */
	};
	if (log->type == TYPE_EPOW) 
		return "EPOW"; 
	if (log->type >= TYPE_PMGM_POWER_SW_ON)
		return "PowerMGM Event (not handled right now)";
	return _errlog_type[log->type];
}

