/*
   hostfs for Linux
   Copyright 2001 Virtutech AB
   Copyright 2001 SuSE

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY OR FITNESS FOR A PARTICULAR PURPOSE, GOOD TITLE or
   NON INFRINGEMENT.  See the GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.

   Written by Gustav Hållberg. The author may be reached as
   gustav@virtutech.com.

   Ported by Andi Kleen and Karsten Keil to Linux 2.4.

   $Id: hostfs_linux.h,v 1.2 2001/07/05 18:56:32 ak Exp $
*/


#include <linux/config.h>

#define DRIVER_VERSION "0.2"
#define DEVICE_NAME "[hostfs]"


#if CONFIG_SMP==1
#define __SMP__
#endif

#include <linux/sched.h>
#include <linux/version.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/types.h>
#include <linux/pagemap.h>
#include <linux/fs.h>
#include <linux/unistd.h>
#include <linux/errno.h>
#include <asm/io.h>
#include <linux/init.h>


#define TARGETOS_LINUX
#include "hostfs.h"
#include "hostfs_data.h"

#ifdef CONFIG_HOSTFS_RW
#undef HF_RW
#define HF_RW	1
#else
#undef HF_RW
#endif

#if defined(HF_RW)
#	define RW_FUNC(func) func
#else
#	define RW_FUNC(func) NULL
#endif

#define GET_HI_LO(tgt, src)                                                             \
do {                                                                                    \
        typedef int check_tgt_size[sizeof (tgt) == 8 || sizeof (tgt) == 4 ? 1 : -1];    \
        if (sizeof (tgt) == 4)                                                          \
                tgt = src ## _lo;                                                       \
        else if (sizeof (tgt) == 8) {                                                   \
                tgt = src ## _hi;                                                       \
                tgt <<= 16;                                                             \
                tgt <<= 16;                                                             \
                tgt |= src ## _lo;                                                      \
        } else                                                                          \
                printk("hostfs GET_HI_LO panic, sizeof (" #tgt ") in %s:%d == %dn",  \
                       __FILE__, __LINE__, (int)sizeof (tgt));                          \
} while (0)

#define SET_HI_LO(tgt, src)                                                             \
do {                                                                                    \
        typedef int check_src_size[sizeof (src) == 8 || sizeof (src) == 4 ? 1 : -1];    \
        if (sizeof (src) == 4) {                                                        \
                tgt ## _lo = src;                                                       \
                tgt ## _hi = 0;                                                         \
        } else if (sizeof (src) == 8) {                                                 \
                unsigned long long tmp = src;                                           \
                tgt ## _lo = (uint)src;                                                 \
                tgt ## _hi = tmp >> 32;                                                 \
        }                                                                               \
} while (0)

#define DEBUG_LEVEL 2
#define HOSTFS_BLOCK_BITS 10
#define HOSTFS_BLOCK_SIZE (1 << HOSTFS_BLOCK_BITS)

#define HOSTFS_ROOT_INO 1

#define DPRINT1 if (DEBUG_LEVEL >= 1) printk
#define DPRINTINODE1(inode) do { if (DEBUG_LEVEL >= 1) print_inode(inode); } while (0)
#define DPRINTFILE1(file) do { } while (0)

#define ALIGN(n, a) (((n) + (a) - 1) & ~(a))
#define MIN(a, b) ((a) < (b) ? (a) : (b))

#define NTOHSWAP(field)                                                 \
do {                                                                    \
        int __i;                                                        \
        for (__i = 0; __i < (sizeof (field) + 3) / 4; ++__i) {          \
                ((uint *)&field)[__i] = ntohl(((uint *)&field)[__i]);   \
        }                                                               \
} while(0)

#define HTONSWAP(field) NTOHSWAP(field)

extern struct file_operations hostfs_file_fops;
extern struct file_operations hostfs_file_dirops;

void hostfs_read_inode(struct inode *inode);
void hostfs_write_inode(struct inode *inode, int sync);

extern spinlock_t get_host_data_lock;
extern spinlock_t hostfs_position_lock;



/* host interface */
int get_host_data(host_func_t func, host_node_t hnode, void *in_buf, 
		  void *out_buf);
int hf_do_seek(ino_t inode, loff_t off);
int init_host_fs(void);

