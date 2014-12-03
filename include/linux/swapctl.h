/*
 * Change Log
 *	12-Nov-2001 Lineo Japan, Inc.
 */

#ifndef _LINUX_SWAPCTL_H
#define _LINUX_SWAPCTL_H

#include <asm/page.h>
#include <linux/fs.h>

typedef struct buffer_mem_v1
{
	unsigned int	min_percent;
	unsigned int	borrow_percent;
	unsigned int	max_percent;
} buffer_mem_v1;
typedef buffer_mem_v1 buffer_mem_t;
extern buffer_mem_t buffer_mem;
extern buffer_mem_t page_cache;

typedef struct freepages_v1
{
	unsigned int	min;
	unsigned int	low;
	unsigned int	high;
} freepages_v1;
typedef freepages_v1 freepages_t;
extern freepages_t freepages;

typedef struct pager_daemon_v1
{
	unsigned int	tries_base;
	unsigned int	tries_min;
	unsigned int	swap_cluster;
} pager_daemon_v1;
typedef pager_daemon_v1 pager_daemon_t;
extern pager_daemon_t pager_daemon;

#ifdef CONFIG_FREEPG_SIGNAL
typedef struct {
	char comm[16];		/* signal recipient process name */
	unsigned int low;	/* once under this number of pages, memory
				 * is low */
	unsigned int mid;	/* once under this number of pages, memory
				 * smacks of shortage */
	unsigned int high;	/* once over this number of pages, memory is
				 * enough */
	unsigned int cur;	/* current freepages */
} freepg_signal_watermark_t;
extern freepg_signal_watermark_t freepg_sig_watermark;
#endif

#endif /* _LINUX_SWAPCTL_H */
