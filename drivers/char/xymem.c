/*
 * linux/drivers/char/xymem.c
 *
 * Copyright (c) 2002 Lineo Japan, Inc.
 * Copyright (c) 2002 Lineo uSolutions, Inc.
 *
 *  May be copied or modified under the terms of the GNU General Public
 *  License.  See linux/COPYING for more information.
 *
 *  X/Y memory driver for Hitachi SH3-DSP
 */
#include <linux/config.h>
#include <linux/types.h>
#include <linux/module.h>
#include <linux/sched.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/fs.h>
#include <linux/delay.h>
#include <linux/fs.h>
#include <linux/poll.h>
#include <linux/string.h>

#include <linux/config.h>

#include <linux/types.h>
#include <linux/module.h>
#include <linux/sched.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/fs.h>
#include <linux/delay.h>
#include <linux/fs.h>
#include <linux/poll.h>
#include <linux/string.h>

#include <asm/uaccess.h>
#include <asm/io.h>
#include <asm/irq.h>

#define XYMEM_MAJOR 127
#define XYMEM_NAME  "xymem"

static struct xymem {
	unsigned long addr;
	size_t        size;
	unsigned long vaddr;
} xymem_s[] = {
	{ 0xa5000000, 16*1024*1024 },
	{ 0xa5007000, 8*1024 },
	{ 0xa5017000, 8*1024 }
};

static ssize_t xymem_read(struct file* filp, char* buf, size_t count, loff_t* l)
{
	int minor = MINOR(filp->f_dentry->d_inode->i_rdev);
	ssize_t len = 0;

	if (minor > 2)
		return -EINVAL;

	if (*l >= xymem_s[minor].size)
		return 0;

	len = xymem_s[minor].size - *l;
	if (count < len) { len = count; }

	copy_to_user(buf, xymem_s[minor].vaddr, len);

	*l += len;

	return len;
}

static ssize_t xymem_write(struct file* filp, const char* buf, size_t count, loff_t* l)
{
	int minor = MINOR(filp->f_dentry->d_inode->i_rdev);
	ssize_t len = 0;

	if (minor > 2)
		return -EINVAL;

	if (*l >= xymem_s[minor].size)
		return 0;

	len = xymem_s[minor].size - *l;
	if (count < len) { len = count; }

	copy_from_user(xymem_s[minor].vaddr, buf, len);

	*l += len;

	return len;
}

int xymem_mmap(struct file* filp, struct vm_area_struct* vma)
{
	int minor = MINOR(filp->f_dentry->d_inode->i_rdev);
	size_t size;

	if (minor > 2)
		return -EINVAL;

	size = vma->vm_end - vma->vm_start;
	if (size > xymem_s[minor].size) {
		size = xymem_s[minor].size;
	}

	vma->vm_flags |= VM_RESERVED | VM_IO;
	vma->vm_page_prot.pgprot &= ~_PAGE_CACHABLE;

	if (remap_page_range(vma->vm_start,
			     xymem_s[minor].addr,
			     size,
			     vma->vm_page_prot))
		return EAGAIN;

	return 0;
}

static struct file_operations xymem_fops = {
	read:	xymem_read,
	write:	xymem_write,
	mmap:	xymem_mmap,
};

int __init xymem_init(void)
{
	int i;

	register_chrdev(XYMEM_MAJOR, XYMEM_NAME, &xymem_fops);

	for (i = 0; i <= 2; i++) {
		xymem_s[i].vaddr =
		  (unsigned long)ioremap(xymem_s[i].addr, xymem_s[i].size);
	}

	printk("SH3DSP memory driver initialized. \n"); 

	return 0;
}

#ifdef MODULE
void __exit xymem_cleanup(void)
{
	del_xymem_timer();

	unregister_chrdev(TS_MAJOR, TS_NAME);

	printk("SH3DSP X/Y memory driver removed.\n");
}
#endif

module_init(xymem_init);
#ifdef MODULE
module_exit(xymem_cleanup);
#endif
