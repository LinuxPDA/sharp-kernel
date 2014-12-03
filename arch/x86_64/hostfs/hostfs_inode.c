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

   $Id: hostfs_inode.c,v 1.5 2001/07/05 18:56:32 ak Exp $
*/

#include "hostfs_linux.h"


static struct address_space_operations hostfs_file_aops;
static struct inode_operations hostfs_symlink_iops, hostfs_dir_iops; 

static struct inode_operations hostfs_file_iops;

void hostfs_read_inode(struct inode *inode)
{
        struct hf_getattr_data data;
        uint mask;

        get_host_data(hf_GetAttr, inode->i_ino, &mask, &data);
        if (data.sys_error) {
                printk(DEVICE_NAME "get_host_data(hf_GetAttr, %ld, ...) had an error\n",
                       inode->i_ino);
                return;
        }

/*          DPRINT1("hostfs_read_inode(%ld) flags %lx\n", inode->i_ino, (long)inode->i_flags); */

        inode->i_flags = 0;
        inode->i_mode = data.mode;
        inode->i_uid = data.uid;
        inode->i_gid = data.gid;
        inode->i_nlink = data.link;
        GET_HI_LO(inode->i_size,  data.size);
        GET_HI_LO(inode->i_atime, data.atime);
        GET_HI_LO(inode->i_mtime, data.mtime);
        GET_HI_LO(inode->i_ctime, data.ctime);
        inode->i_blksize = data.blksize;
        GET_HI_LO(inode->i_blocks, data.blocks);

        if (S_ISLNK(inode->i_mode)) {
                inode->i_op = &hostfs_symlink_iops;
        } else if (S_ISDIR(inode->i_mode)) {
                inode->i_op = &hostfs_dir_iops;
		inode->i_fop = &hostfs_file_dirops; 
        } else {
		inode->i_fop = &hostfs_file_fops; 
		inode->i_op = &hostfs_file_iops;
                inode->i_mapping->a_ops = &hostfs_file_aops;
        }
}

#if defined(HF_RW)
void hostfs_write_inode(struct inode *inode, int sync)
{
        /* 
         * it might be pointless to implement this function since
         * supposedly all changes will have been updated by
         * notify_change anyway, but I'm sure it can't hurt
         */

        struct hf_setattr_data odata;
        struct hf_common_data idata;

        DPRINT1(DEVICE_NAME " hostfs_write_inode(%ld)\n", inode->i_ino);
        
        memset(&odata, 0, sizeof odata);
        odata.set_atime = 1;
        SET_HI_LO(odata.atime, inode->i_atime);
        odata.set_mtime = 1;
        SET_HI_LO(odata.mtime, inode->i_mtime);
        odata.set_mode = 1;
        odata.mode = inode->i_mode;
        odata.set_uid = 1;
        odata.uid = inode->i_uid;
        odata.set_gid = 1;
        odata.gid = inode->i_gid;
        get_host_data(hf_SetAttr, inode->i_ino, &odata, &idata);
}

static int
hostfs_setattr(struct dentry *dentry, struct iattr *iattr)
{
        struct hf_setattr_data odata;
        struct hf_common_data idata;
        struct inode *inode = dentry->d_inode;
        int error;

        DPRINT1(DEVICE_NAME " hostfs_notify_change(%ld, 0x%lx)\n", inode->i_ino, (long)iattr->ia_valid);

        if ((error = inode_change_ok(inode, iattr)))
            return error;
            
        memset(&odata, 0, sizeof odata);
        if (iattr->ia_valid & ATTR_ATIME) {
            odata.set_atime = 1;
            SET_HI_LO(odata.atime, iattr->ia_atime);
        }
        if (iattr->ia_valid & ATTR_MTIME) {
            odata.set_mtime = 1;
            SET_HI_LO(odata.mtime, iattr->ia_mtime);
        }
        if (iattr->ia_valid & ATTR_MODE) {
            odata.set_mode = 1;
            odata.mode = iattr->ia_mode;
        }
        if (iattr->ia_valid & ATTR_UID) {
            odata.set_uid = 1;
            odata.uid = iattr->ia_uid;
        }
        if (iattr->ia_valid & ATTR_GID) {
            odata.set_gid = 1;
            odata.gid = iattr->ia_gid;
        }
        get_host_data(hf_SetAttr, inode->i_ino, &odata, &idata);

        DPRINT1("         hf_SetAttr returned %d\n", idata.sys_error);

        if (idata.sys_error)
            return -idata.sys_error;

        inode_setattr(inode, iattr);
        return 0;
}
#endif

#if defined(HF_RW)
static int hostfs_prepare_write(struct file *file, struct page *page, unsigned offset, unsigned to)
{
	kmap(page);
	return 0;
}

static int hostfs_do_writepage(struct inode *inode, struct page *page, 
			       unsigned from, unsigned to)
{
	int offset = (page->index << PAGE_CACHE_SHIFT)+from; 	
	int bytes = to-from;
	unsigned long int address = (unsigned long) page_address(page)+from;
	uint ino = inode->i_ino;
	int res;

	if (page->index > (0x7fffffff/PAGE_CACHE_SIZE)-1)
		return -EFBIG;

        spin_lock(&hostfs_position_lock);

        DPRINT1("hostfs_do_writepage: offset:%u bytes:%u address:%lx\n",offset,bytes,address);

        hf_do_seek(ino, offset);

	res = 0;
        while (bytes > 0) {
                struct hf_write_data odata;
                struct hf_common_data idata;
                uint cnt = MIN(bytes, XFER_DATA_WORDS * 4);
                odata.append = 0;
                odata.size = cnt;
                memcpy(odata.data, (char *)address, cnt);
                HTONSWAP(odata.data);
                get_host_data(hf_Write, ino, &odata, &idata);
                if (idata.sys_error) {
                        res = -idata.sys_error;
                        DPRINT1(DEVICE_NAME " failed with errcode %d\n", res);
			ClearPageUptodate(page);
                        goto got_error;
                }
                address += cnt;
                bytes -= cnt;
		res += cnt;
        }
        DPRINT1(DEVICE_NAME "wrote %d bytes\n", (int)res);

	SetPageUptodate(page);

 got_error:
        spin_unlock(&hostfs_position_lock);

        return res;
	
} 

static int hostfs_writepage(struct page *page)
{
	struct inode *inode = page->mapping->host;
	int err;
	int bytes = PAGE_CACHE_SIZE; 
	if (page->index >= (inode->i_size >> PAGE_CACHE_SHIFT))
		bytes = inode->i_size & (PAGE_CACHE_SIZE-1); 
	kmap(page);
	err = hostfs_do_writepage(inode, page, 0, bytes);  
	kunmap(page);
	return err;
} 

static int
hostfs_commit_write(struct file *file, struct page *page, unsigned from, unsigned to)
{
	struct inode *inode = page->mapping->host;
	int err;
	
	err = hostfs_do_writepage(inode, page, from, to); 
	kunmap(page);
	if (err < 0) 
		return err;
	{
		loff_t s = ((page->index) << PAGE_CACHE_SHIFT) + to; 
		if (s > inode->i_size)
			inode->i_size = s;
	} 
	return 0;
}
#endif

static int
hostfs_inode_readpage(struct file *file, struct page *page)
{
        unsigned long address = (unsigned long) page_address(page);
        struct inode *inode = file->f_dentry->d_inode;
        uint count = PAGE_CACHE_SIZE;
        int err;

	if (page->index > (0xffffffff/PAGE_CACHE_SIZE)-1) 
		return -EFBIG; 
	ClearPageError(page);
        count = PAGE_SIZE;

/*          DPRINT1("readpage seek %ld\n", (long)page->offset); */

	kmap(page); 

        spin_lock(&hostfs_position_lock);		

        if ((err = hf_do_seek(inode->i_ino, page->index << PAGE_CACHE_SHIFT))) {
                err = -err;
                goto out_error;
        }

        while (count > 0) {
                struct hf_read_data data;
                uint to_read = MIN(count, XFER_DATA_WORDS * 4);

                get_host_data(hf_Read, inode->i_ino, &to_read, &data);
                if (data.sys_error) {
                        err = -data.sys_error; 
                        goto out_error;
                }

/*                  DPRINT1("readpage count %d out of %d\n", data.size, count); */

		if (!data.size) { 
			memset((char*)address,0,count); 
			break;
		}

                NTOHSWAP(data.data);

                memcpy((char *)address, data.data, data.size);


                address += data.size;
                count -= data.size;
        }

        err = 0;
        spin_unlock(&hostfs_position_lock);
        
	SetPageUptodate(page);

 out_error:

	kunmap(page); 

/*          DPRINT1("postuse %ld\n", (long)atomic_read(&page->count)); */
        
        return err;
}

static int
hostfs_inode_readlink(struct dentry *dentry, char *buf, int size)
{
	struct hf_readlink_data data;

	get_host_data(hf_Readlink, dentry->d_inode->i_ino, NULL, &data);
	if (data.sys_error)
		return -data.sys_error;
	return vfs_readlink(dentry, buf, size, data.filename);
}

static int
hostfs_inode_follow_link(struct dentry *dentry, struct nameidata *nd)
{
/*          DPRINT1("hostfs_inode_follow_link ino %ld %ld %d\n", base->d_inode->i_ino, dentry->d_inode->i_ino, follow); */
	struct hf_readlink_data data;

	get_host_data(hf_Readlink, dentry->d_inode->i_ino, NULL, &data);
	if (data.sys_error)
		return -data.sys_error;
        return vfs_follow_link(nd, data.filename); 
}

static struct dentry *
hostfs_inode_lookup(struct inode *dir, struct dentry *dentry)
{
        struct hf_lookup_data odata;
        struct hf_hnode_data idata;
        struct inode *inode;

/*          DPRINT1("hostfs_inode_lookup(%ld, dent %p = %s)\n", dir->i_ino, dentry, dentry->d_name.name); */

	/* protect lots of stupid strcpy()s */
	if (dentry->d_name.len > HOSTFS_FILENAME_LEN-1)
		return ERR_PTR(-ENAMETOOLONG); 

	memcpy(odata.filename, dentry->d_name.name, dentry->d_name.len);
	odata.filename[dentry->d_name.len] = 0;
	HTONSWAP(odata.filename);
	get_host_data(hf_Lookup, dir->i_ino, &odata, &idata);
 
        if (idata.sys_error) {
                if (idata.sys_error != ENOENT)
                        return ERR_PTR(-idata.sys_error);
                inode = NULL;
        } else
                inode = iget(dir->i_sb, idata.hnode);

        d_add(dentry, inode);

        return ERR_PTR(0);
}

#if defined(HF_RW)
static int
hostfs_inode_mkdir(struct inode *dir, struct dentry *dentry, int mode)
{
        struct hf_mkdir_data odata;
        struct hf_hnode_data idata;
        struct inode *inode;

        DPRINT1("hostfs_inode_mkdir()\n");

        odata.mode = mode;
        strcpy(odata.dirname, dentry->d_name.name);
        HTONSWAP(odata.dirname);

        get_host_data(hf_MkDir, dir->i_ino, &odata, &idata);
        if (idata.sys_error)
                return -idata.sys_error;;
        
        inode = iget(dir->i_sb, idata.hnode);
        hostfs_read_inode(inode);
        mark_inode_dirty(inode);
        d_instantiate(dentry, inode);

        return 0;
}

static int
hostfs_inode_rmdir(struct inode *dir, struct dentry *dentry)
{
        struct hf_remove_data odata;
        struct hf_common_data idata;

        DPRINT1("hostfs_inode_rmdir()\n");

        strcpy(odata.filename, dentry->d_name.name);
        HTONSWAP(odata.filename);
        get_host_data(hf_Remove, dir->i_ino, &odata, &idata);
        mark_inode_dirty(dir);
 
        return -idata.sys_error;
}
#endif

#if defined(HF_RW)
static int
hostfs_inode_create(struct inode *dir, struct dentry *dentry, int mode)
{
        struct hf_create_data odata;
        struct hf_hnode_data idata;
        struct inode *inode;

        DPRINT1("hostfs_inode_create(%ld, mode = %d)\n", dir->i_ino, mode);

        odata.mode = mode & S_IALLUGO;
        odata.excl = 0;
        odata.trunc = 0;
        strncpy(odata.filename, dentry->d_name.name, sizeof(odata.filename));
        HTONSWAP(odata.filename);

        get_host_data(hf_Create, dir->i_ino, &odata, &idata);

        if (idata.sys_error) {
                return -idata.sys_error;
        }

        inode = iget(dir->i_sb, idata.hnode);
        hostfs_read_inode(inode);
        DPRINT1("    read inode mode 0%o  uid %d\n", inode->i_mode, inode->i_uid);
        mark_inode_dirty(inode);
        d_instantiate(dentry, inode);

        return 0;
}

static void
hostfs_inode_truncate(struct inode *inode)
{
        struct hf_setattr_data odata;
        struct hf_common_data idata;

        DPRINT1("hostfs_inode_truncate(inode = %ld)\n", inode->i_ino);

        memset(&odata, 0, sizeof odata);
        odata.set_size = 1;
        SET_HI_LO(odata.size, inode->i_size);
        get_host_data(hf_SetAttr, inode->i_ino, &odata, &idata);

        if (idata.sys_error) {
                printk("[hostfs] call to truncate() failed with error %d!\n", idata.sys_error);
        }
}

static int
hostfs_unlink(struct inode *dir, struct dentry *dentry)
{
        struct hf_remove_data odata;
        struct hf_common_data idata;

        DPRINT1("hostfs_unlink(dir = %ld, dentry = %s)\n", dir->i_ino, dentry->d_name.name);

        strcpy(odata.filename, dentry->d_name.name);
        HTONSWAP(odata.filename);
        get_host_data(hf_Remove, dir->i_ino, &odata, &idata);

        return -idata.sys_error;
}

#endif




static struct inode_operations hostfs_dir_iops = {
        create: RW_FUNC(hostfs_inode_create),
        lookup: hostfs_inode_lookup,
        unlink: RW_FUNC(hostfs_unlink),
        mkdir: RW_FUNC(hostfs_inode_mkdir),
        rmdir: RW_FUNC(hostfs_inode_rmdir),
};

static struct inode_operations hostfs_symlink_iops = {
        readlink: hostfs_inode_readlink,
        follow_link: hostfs_inode_follow_link,
};

static struct inode_operations hostfs_file_iops = {
        truncate: RW_FUNC(hostfs_inode_truncate),
	setattr: RW_FUNC(hostfs_setattr)
};

static struct address_space_operations hostfs_file_aops = { 
	readpage: hostfs_inode_readpage,
	prepare_write: RW_FUNC(hostfs_prepare_write), 
	commit_write: RW_FUNC(hostfs_commit_write), 
	writepage: RW_FUNC(hostfs_writepage),
}; 
