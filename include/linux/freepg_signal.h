/*
 * linux/include/linux/freepg_signal.h
 *
 * (C) Copyright 2002 Lineo Japan, Inc.
 *
 * May be copied or modified under the terms of the GNU General Public
 * License.  See linux/COPYING for more information.
 *
 */

#ifndef _LINUX_FREEPG_SIGNAL_H
#define _LINUX_FREEPG_SIGNAL_H

#include <linux/config.h>

#ifdef CONFIG_FREEPG_SIGNAL
extern char freepg_signal_proc[16];

extern void freepg_signal_reset(void);
extern void freepg_signal_low(void);
extern void freepg_signal_min(void);
extern void freepg_signal_check(void);
#else
extern inline void freepg_signal_reset(void) { }
extern inline void freepg_signal_low(void) { }
extern inline void freepg_signal_min(void) { }
extern inline void freepg_signal_check(void) { }
#endif

#endif
