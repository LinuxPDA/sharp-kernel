
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

   $Id: hostfs_file.c,v 1.1 2001/07/02 20:17:51 ak Exp $
*/

#include "hostfs_linux.h"



static ssize_t
hostfs_fo_read(struct file *file, char *buf, size_t len, loff_t *off)
{
/*          DPRINT1(DEVICE_NAME " hostfs_fo_read("); */
/*          DPRINTFILE1(file); */
/*          DPRINT1(", %p, %ld, %p)\n", buf, (long)len, off); */
        return generic_file_read(file, buf, len, off);
}

static int
hostfs_fo_open(struct inode *inode, struct file *file)
{
        struct hf_common_data idata;
        uint oflag = 0, flag = file->f_flags;

        DPRINT1("hostfs_fo_open %ld flags %x\n", inode->i_ino, file->f_flags);

        switch (flag & O_ACCMODE) {
        case O_RDONLY:
                oflag = HF_FREAD;
                break;
#if defined(HF_RW)
        case O_RDWR:
                oflag = HF_FREAD; 
                /* fallthrough */
        case O_WRONLY:
                oflag |= HF_FWRITE;
                break;
#else
        case O_WRONLY:
        case O_RDWR:
                return -EACCES;
#endif
        default:
                return -EINVAL;
        }

        get_host_data(hf_Open, inode->i_ino, &oflag, &idata);

        return 0;
}


static ssize_t
hostfs_fo_write(struct file *file, const char *buf, size_t len, loff_t *off)
{
        DPRINT1(DEVICE_NAME " hostfs_fo_write(");
        DPRINTFILE1(file);
        DPRINT1(", %p, %ld, %p)\n", buf, (long)len, off);

        return generic_file_write(file, buf, len, off);
}


static int
hostfs_fo_release(struct inode *inode, struct file *file)
{
        uint dummy;

        DPRINT1("hostfs_fo_release %ld fcount %d icount %d\n",
        	(long)inode->i_ino, atomic_read(&file->f_count),
        	atomic_read(&inode->i_count));
        get_host_data(hf_Close, inode->i_ino, NULL, &dummy);
        return 0;
}

static int
hostfs_fo_mmap(struct file *file, struct vm_area_struct *area)
{
/*          DPRINT1("hostfs_fo_mmap()\n"); */
        return generic_file_mmap(file, area);
}


struct file_operations hostfs_file_fops = {
        read: hostfs_fo_read,
        write: RW_FUNC(hostfs_fo_write),
        mmap:    hostfs_fo_mmap,
        open: hostfs_fo_open,
	release: hostfs_fo_release,
};
