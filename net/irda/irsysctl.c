/*********************************************************************
 *                
 * Filename:      irsysctl.c
 * Version:       1.0
 * Description:   Sysctl interface for IrDA
 * Status:        Experimental.
 * Author:        Dag Brattli <dagb@cs.uit.no>
 * Created at:    Sun May 24 22:12:06 1998
 * Modified at:   Fri Jun  4 02:50:15 1999
 * Modified by:   Dag Brattli <dagb@cs.uit.no>
 * 
 *     Copyright (c) 1997, 1999 Dag Brattli, All Rights Reserved.
 *     Copyright (c) 2000-2001 Jean Tourrilhes <jt@hpl.hp.com>
 *      
 *     This program is free software; you can redistribute it and/or 
 *     modify it under the terms of the GNU General Public License as 
 *     published by the Free Software Foundation; either version 2 of 
 *     the License, or (at your option) any later version.
 *  
 *     Neither Dag Brattli nor University of Troms.ANx admit liability nor
 *     provide warranty for any of this software. This material is 
 *     provided "AS-IS" and at no charge.
 *     
 * ChangeLog:
 *	07-02-2002 SHARP	add proc file for peer device list control
 ********************************************************************/

#include <linux/config.h>
#include <linux/mm.h>
#include <linux/ctype.h>
#include <linux/sysctl.h>
#include <asm/segment.h>

#include <net/irda/irda.h>
#include <net/irda/irias_object.h>
#include <net/irda/qos.h>
#include <linux/compatmac.h>

#define NET_IRDA 412 /* Random number */

enum { DISCOVERY=1, DEVNAME, DEBUG, FAST_POLL, DISCOVERY_SLOTS,
       DISCOVERY_TIMEOUT, SLOT_TIMEOUT, MAX_BAUD_RATE, MIN_TX_TURN_TIME,
       MAX_NOREPLY_TIME, WARN_NOREPLY_TIME, LAP_KEEPALIVE_TIME, SPECIFIC_DEV };

extern int  sysctl_discovery;
extern int  sysctl_discovery_slots;
extern int  sysctl_discovery_timeout;
extern int  sysctl_slot_timeout;	/* Candidate */
extern int  sysctl_fast_poll_increase;
int         sysctl_compression = 0;
extern char sysctl_devname[];
extern int  sysctl_max_baud_rate;
extern int  sysctl_min_tx_turn_time;
extern int  sysctl_max_noreply_time;
extern int  sysctl_warn_noreply_time;
extern int  sysctl_lap_keepalive_time;
extern void  *sysctl_specdev;		/* added by SHARP */

#ifdef CONFIG_IRDA_DEBUG
extern unsigned int irda_debug;
#endif

/* this is needed for the proc_dointvec_minmax - Jean II */
static int max_discovery_slots = 16;		/* ??? */
static int min_discovery_slots = 1;
/* IrLAP 6.13.2 says 25ms to 10+70ms - allow higher since some devices
 * seems to require it. (from Dag's comment) */
static int max_slot_timeout = 160;
static int min_slot_timeout = 20;
static int max_max_baud_rate = 16000000;	/* See qos.c - IrLAP spec */
static int min_max_baud_rate = 2400;
static int max_min_tx_turn_time = 10000;	/* See qos.c - IrLAP spec */
static int min_min_tx_turn_time = 0;
static int max_max_noreply_time = 40;		/* See qos.c - IrLAP spec */
static int min_max_noreply_time = 3;
static int max_warn_noreply_time = 3;		/* 3s == standard */
static int min_warn_noreply_time = 1;		/* 1s == min WD_TIMER */
static int max_lap_keepalive_time = 10000;	/* 10s */
static int min_lap_keepalive_time = 100;	/* 100us */
/* For other sysctl, I've no idea of the range. Maybe Dag could help
 * us on that - Jean II */

static int do_specdev(ctl_table *table, int write, struct file *filp,
		      void *buffer, size_t *lenp);

static int do_devname(ctl_table *table, int write, struct file *filp,
		      void *buffer, size_t *lenp)
{
	int ret;

	ret = proc_dostring(table, write, filp, buffer, lenp);
	if (ret == 0 && write) {
		struct ias_value *val;

		val = irias_new_string_value(sysctl_devname);
		if (val)
			irias_object_change_attribute("Device", "DeviceName", val);
	}
	return ret;
}


/* One file */
static ctl_table irda_table[] = {
	{ DISCOVERY, "discovery", &sysctl_discovery,
	  sizeof(int), 0644, NULL, &proc_dointvec },
	{ DEVNAME, "devname", sysctl_devname,
	  65, 0644, NULL, &do_devname, &sysctl_string},
#ifdef CONFIG_IRDA_DEBUG
        { DEBUG, "debug", &irda_debug,
	  sizeof(int), 0644, NULL, &proc_dointvec },
#endif
#ifdef CONFIG_IRDA_FAST_RR
        { FAST_POLL, "fast_poll_increase", &sysctl_fast_poll_increase,
	  sizeof(int), 0644, NULL, &proc_dointvec },
#endif
	{ DISCOVERY_SLOTS, "discovery_slots", &sysctl_discovery_slots,
	  sizeof(int), 0644, NULL, &proc_dointvec_minmax, &sysctl_intvec,
	  NULL, &min_discovery_slots, &max_discovery_slots },
	{ DISCOVERY_TIMEOUT, "discovery_timeout", &sysctl_discovery_timeout,
	  sizeof(int), 0644, NULL, &proc_dointvec },
	{ SLOT_TIMEOUT, "slot_timeout", &sysctl_slot_timeout,
	  sizeof(int), 0644, NULL, &proc_dointvec_minmax, &sysctl_intvec,
	  NULL, &min_slot_timeout, &max_slot_timeout },
	{ MAX_BAUD_RATE, "max_baud_rate", &sysctl_max_baud_rate,
	  sizeof(int), 0644, NULL, &proc_dointvec_minmax, &sysctl_intvec,
	  NULL, &min_max_baud_rate, &max_max_baud_rate },
	{ MIN_TX_TURN_TIME, "min_tx_turn_time", &sysctl_min_tx_turn_time,
	  sizeof(int), 0644, NULL, &proc_dointvec_minmax, &sysctl_intvec,
	  NULL, &min_min_tx_turn_time, &max_min_tx_turn_time },
	{ MAX_NOREPLY_TIME, "max_noreply_time", &sysctl_max_noreply_time,
	  sizeof(int), 0644, NULL, &proc_dointvec_minmax, &sysctl_intvec,
	  NULL, &min_max_noreply_time, &max_max_noreply_time },
	{ WARN_NOREPLY_TIME, "warn_noreply_time", &sysctl_warn_noreply_time,
	  sizeof(int), 0644, NULL, &proc_dointvec_minmax, &sysctl_intvec,
	  NULL, &min_warn_noreply_time, &max_warn_noreply_time },
	{ LAP_KEEPALIVE_TIME, "lap_keepalive_time", &sysctl_lap_keepalive_time,
	  sizeof(int), 0644, NULL, &proc_dointvec_minmax, &sysctl_intvec,
	  NULL, &min_lap_keepalive_time, &max_lap_keepalive_time },
	{ SPECIFIC_DEV, "specific_dev", &sysctl_specdev,
	  4, 0644, NULL, &do_specdev, &sysctl_intvec},
	{ 0 }
};

/* One directory */
static ctl_table irda_net_table[] = {
	{ NET_IRDA, "irda", NULL, 0, 0555, irda_table },
	{ 0 }
};

/* The parent directory */
static ctl_table irda_root_table[] = {
	{ CTL_NET, "net", NULL, 0, 0555, irda_net_table },
	{ 0 }
};

static struct ctl_table_header *irda_table_header;

/*
 * Function irda_sysctl_register (void)
 *
 *    Register our sysctl interface
 *
 */
int irda_sysctl_register(void)
{
	irda_table_header = register_sysctl_table(irda_root_table, 0);
	if (!irda_table_header)
		return -ENOMEM;

	return 0;
}

/*
 * Function irda_sysctl_unregister (void)
 *
 *    Unregister our sysctl interface
 *
 */
void irda_sysctl_unregister(void) 
{
	unregister_sysctl_table(irda_table_header);
}



/*
 * Function do_specdev (*table,write,*filp,*buffer,*lenp)
 *
 *    Reads/writes a string from/to the user buffer on specific_dev.
 *
 *	modified by SHARP
 */
static int do_specdev(ctl_table *table, int write, struct file *filp,
		      void *buffer, size_t *lenp)
{
#define TMP_BUFFER_SIZE		(256)
	int len, left;
	irda_devlist_t	*devlist;
	char  *p, c, buff[TMP_BUFFER_SIZE];

	if (!table->data || !table->maxlen || !*lenp ||
	    (filp->f_pos && !write)) {
		*lenp = 0;
		return 0;
	}

	if(write){
		len = 0;
		p = buffer;
		while (len < *lenp) {
			if(get_user(c, p++))
				return -EFAULT;
			if (c == 0 || c == '\n')
				break;
			len++;
		}
		if (len >= TMP_BUFFER_SIZE)
			len = TMP_BUFFER_SIZE-1;
		if(copy_from_user(buff, buffer, len))
			return -EFAULT;
		buff[len] = 0;
		filp->f_pos += *lenp;
		
		irda_device_list_new( buff );
	}else{
		left = *lenp;
		if(sysctl_specdev == NULL) {
			strcpy( buff, "no device list.\n" );
			len = strlen(buff);
			if (len > left)
				len = left;
			if(copy_to_user(buffer, buff, len))
				return -EFAULT;
			left -= len;
			buffer += len;
		}else{
			devlist = (irda_devlist_t*)hashbin_get_first(sysctl_specdev);
			while(left > 0 && devlist != NULL){
				p = buff;
				sprintf(p, "%s\n", devlist->queue.q_name);
				if (devlist->qos.baud_rate.value){
					p = buff+strlen(buff);
					sprintf(p, " BaudRate=%d\n", devlist->qos.baud_rate.value);
				}
				if (devlist->qos.max_turn_time.value){
					p = buff+strlen(buff);
					sprintf(p, " MaxTurnTime=%d\n", devlist->qos.max_turn_time.value);
				}
				if (devlist->qos.min_turn_time.value){
					p = buff+strlen(buff);
					sprintf(p, " MinTurnTime=%d\n", devlist->qos.min_turn_time.value);
				}
				if (devlist->qos.data_size.value){
					p = buff+strlen(buff);
					sprintf(p, " DataSize=%d\n", devlist->qos.data_size.value);
				}
				if (devlist->qos.window_size.value){
					p = buff+strlen(buff);
					sprintf(p, " WindowSize=%d\n", devlist->qos.window_size.value);
				}
				if (devlist->qos.additional_bofs.value){
					p = buff+strlen(buff);
					sprintf(p, " BOF=%d\n", devlist->qos.additional_bofs.value);
				}
				if (devlist->qos.link_disc_time.value){
					p = buff+strlen(buff);
					sprintf(p, " DisconnectTime=%d\n", devlist->qos.link_disc_time.value);
				}
				
				len = strlen(buff);
				if (len > left)
					len = left;
				if(copy_to_user(buffer, buff, len))
					return -EFAULT;
				left -= len;
				buffer += len;
				
				devlist = (irda_devlist_t*)hashbin_get_next(sysctl_specdev);
			}
		}
		*lenp -= left;
		filp->f_pos += *lenp;
	}
	return 0;
}
#undef TMP_BUFFER_SIZE
