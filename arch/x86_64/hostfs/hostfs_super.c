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

   $Id: hostfs_super.c,v 1.3 2001/07/05 18:56:32 ak Exp $
*/


#include "hostfs_linux.h"

static struct super_operations hostfs_super_ops;


static int
hostfs_statfs(struct super_block *sb, struct statfs *buf)
{
        struct hf_vfsstat_data data;
  
        get_host_data(hf_VfsStat, 0, NULL, &data);

        memset(buf, 0, sizeof *buf);
        buf->f_type = HOSTFS_MAGIC;
        buf->f_bsize = data.bsize;
        buf->f_blocks = data.blocks;
        buf->f_bfree = data.bfree;
        buf->f_bavail = data.bavail;
        buf->f_files = data.files;
        buf->f_ffree = data.ffree;
        buf->f_namelen = HOSTFS_FILENAME_LEN;

	return 0;
}


static struct super_block *
hostfs_read_super(struct super_block *sb, void *data, int silent)
{
        struct inode *root_inode;
        struct hf_handshake_data odata;
        struct hf_handshake_reply_data idata;

        odata.version = HOSTFS_VERSION;
        get_host_data(hf_Handshake, 0, &odata, &idata);

        if (idata.sys_error || idata.magic != HOSTFS_MAGIC) {
                printk(DEVICE_NAME " Handshake with Simics module failed!\n");
                goto out_fail;
        }

        printk(DEVICE_NAME " mounted\n");

        sb->s_op = &hostfs_super_ops;
        sb->s_magic = HOSTFS_MAGIC;
        sb->s_blocksize = HOSTFS_BLOCK_SIZE;
        sb->s_blocksize_bits = HOSTFS_BLOCK_BITS;
#if !defined(HF_RW)
	sb->s_flags |= MS_RDONLY;
#endif
        root_inode = new_inode(sb);
        if (!root_inode)
                goto out_fail;
        root_inode->i_ino = HOSTFS_ROOT_INO;
        hostfs_read_inode(root_inode);
        sb->s_root = d_alloc_root(root_inode);
        if (!sb->s_root)
                goto out_no_root;
        
        return sb;

 out_no_root:
        printk(DEVICE_NAME " get root inode failed\n");
        iput(root_inode);
        
 out_fail:
        return NULL;
}


static void
hostfs_put_super(struct super_block *sb)
{
        struct hf_common_data idata;
        get_host_data(hf_Unmount, 0, NULL, &idata);
        printk(DEVICE_NAME " unmounted\n");
}


static struct super_operations hostfs_super_ops = {
        read_inode: hostfs_read_inode,
        write_inode: RW_FUNC(hostfs_write_inode), 
        put_super: hostfs_put_super,
        statfs:   hostfs_statfs,
};

static DECLARE_FSTYPE(host_fs_type, "simicsfs", hostfs_read_super, 0);

static int
__init init_hostfs(void)
{
        printk(DEVICE_NAME " loaded\n");
        init_host_fs();
        return register_filesystem(&host_fs_type);
}

static void
__exit cleanup_hostfs(void)
{
        unregister_filesystem(&host_fs_type);
        printk(DEVICE_NAME " unloaded\n");
}

EXPORT_NO_SYMBOLS;

module_init(init_hostfs)
module_exit(cleanup_hostfs)
