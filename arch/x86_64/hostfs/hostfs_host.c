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

   $Id: hostfs_host.c,v 1.1 2001/07/02 20:17:51 ak Exp $
*/


#include "hostfs_linux.h"

static volatile void *hostfs_dev_data;

static void
data_to_host(uint *data, int len)
{
        int i;

        for (i = 0; i < len; i++)
                *(uint *)hostfs_dev_data = data[i];
}

static void
data_from_host(uint *data, int len)
{
        int i;

        for (i = 0; i < len; i++)
                data[i] = *(uint *)hostfs_dev_data;
}

spinlock_t get_host_data_lock = SPIN_LOCK_UNLOCKED;
spinlock_t hostfs_position_lock = SPIN_LOCK_UNLOCKED;


int
get_host_data(host_func_t func, host_node_t hnode, void *in_buf, void *out_buf)
{
        uint data;

        if (func != host_funcs[func].func)
                printk(DEVICE_NAME " INTERNAL ERROR: host_funcs_t != func\n");

        spin_lock(&get_host_data_lock);

        data = func;
        data_to_host(&data, 1);
        data_to_host(&hnode, 1);
        data_to_host(in_buf, host_funcs[func].out);
        data_from_host(out_buf, host_funcs[func].in);

        spin_unlock(&get_host_data_lock);

        return 0;
}


/* internal function */
int hf_do_seek(ino_t inode, loff_t off)
{
        struct hf_seek_data odata;
        struct hf_common_data idata;
        
        SET_HI_LO(odata.off, off);
        get_host_data(hf_Seek, inode, &odata, &idata);
        return idata.sys_error;
}



int init_host_fs(void)
{
#if defined(__sparc_v9__)
        hostfs_dev_data = (void *)HOSTFS_DEV;
#elif defined(__alpha)
        hostfs_dev_data = (void *)phys_to_virt(HOSTFS_DEV);
#elif defined(__i386) || defined(__x86_64__)
	hostfs_dev_data = (void *)ioremap(HOSTFS_DEV, 16);
#else
#error "No device mapping for this architecture"
#endif
}
