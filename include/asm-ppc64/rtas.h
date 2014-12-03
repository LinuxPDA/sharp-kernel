#ifndef _PPC64_RTAS_H
#define _PPC64_RTAS_H

#include <linux/spinlock.h>
#include <asm/rtas-eventscan.h>

/*
 * Definitions for talking to the RTAS on CHRP machines.
 *
 * Copyright (C) 2001 Peter Bergner
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version
 * 2 of the License, or (at your option) any later version.
 */

typedef u32 rtas_arg_t;

struct rtas_args {
	u32 token;
	u32 nargs;
	u32 nret; 
	rtas_arg_t args[16];
	spinlock_t lock;
	rtas_arg_t *rets;     /* Pointer to return values in args[]. */
};  


struct rtas_event_scan {
	unsigned long rate;		/* units = calls/min */
	unsigned long log_size;
	struct rtas_error_log *log;	/* virtual address pointer */
};

struct rtas_t {
	unsigned long entry;		/* physical address pointer */
	unsigned long base;		/* physical address pointer */
	unsigned long size;
	spinlock_t lock;

	struct device_node *dev;	/* virtual address pointer */
	struct rtas_event_scan event_scan;
};

extern struct rtas_t rtas;

extern void enter_rtas(struct rtas_args *);
extern long call_rtas(const char *, int, int, unsigned long *, ...);
extern void phys_call_rtas(int, int, int, ...);
extern void phys_call_rtas_display_status(char);
extern void call_rtas_display_status(char);
extern void rtas_restart(char *cmd);
extern void rtas_power_off(void);
extern void rtas_halt(void);


#endif /* _PPC64_RTAS_H */

