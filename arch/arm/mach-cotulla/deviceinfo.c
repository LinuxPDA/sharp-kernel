/* deviceinfo.c:	device-specific information
 * Copyright (C) 2001  SHARP
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include	<linux/config.h>
#include	<linux/kernel.h>
#include	<linux/module.h>
#include	<linux/init.h>
#include	<linux/proc_fs.h>
#include	<linux/ioport.h>
#include	<asm/uaccess.h>
#include	<asm/arch/hardware.h>
#include	"deviceinfo.h"

#define	MODULE_NAME	"deviceinfo"
#define	DEVINFO_DIRNAME	"deviceinfo"

static int proc_read_deviceinfo(struct file * file, char * buf,
				size_t nbytes, loff_t *ppos);

static struct file_operations proc_deviceinfo_operations = {
	read:	proc_read_deviceinfo,
};

typedef struct deviceinfo_entry {
	char * (*fn)(void);
	char * name;
	char * description;
	unsigned short low_ino;
} deviceinfo_entry_t;

static deviceinfo_entry_t deviceinfo[] =
{
  /* list must be the first */
  {NULL,	".list",	"all of device information"},
  /* mandatory entries */
  {di_filever,	"filever",	"file format version"},
  {di_vender,	"vendor",	"device vendor name"},
  {di_product,	"product",	"product name"},
  {di_revision,	"revision",	"ROM version"},
  /* optional entries */
  {di_locale,	"locale",	"locale"},
  {di_serial,	"serial",	"device individual id"},
  {di_checksum,	"checksum",	"ROM checksum"},
  {di_bootstr,	"bootstr",	"boot string"}
};

#define NUM_OF_DEVICEINFO_ENTRY	(sizeof(deviceinfo)/sizeof(deviceinfo_entry_t))

static int di_list(char *page, char **start, off_t off,
		   int count, int *eof, void *data)
{
	int	i;
	char	*p = page;
	int	len;

	for (i=1; i<NUM_OF_DEVICEINFO_ENTRY; i++) {
		p += sprintf(p, "%s:  %s\n",
			     deviceinfo[i].name,
			     deviceinfo[i].fn());
	}

	len = p - page;

	if (len <= off+count) *eof = 1;
	*start = page + off;
	len -= off;
	if (len>count) len = count;
	if (len<0) len = 0;

	return len;
}

char *di_filever(void)		/* deviceinfo format version */
{
	return "1.0";
}


static int proc_read_deviceinfo(struct file * file, char * buf,
				size_t nbytes, loff_t *ppos)
{
	int i_ino = (file->f_dentry->d_inode)->i_ino;
	char outputbuf[128];
	int count;
	int i;

	deviceinfo_entry_t	*current_di=NULL;
	if (*ppos>0) /* Assume reading completed in previous read*/
		return 0;
	for (i=0;i<NUM_OF_DEVICEINFO_ENTRY;i++) {
		if (deviceinfo[i].low_ino==i_ino) {
			current_di = &deviceinfo[i];
			break;
		}
	}
	if (current_di==NULL)
		return -EINVAL;

	count = sprintf(outputbuf, "%s\n", deviceinfo[i].fn());
	*ppos+=count;
	if (count>nbytes)	/* Assume output can be read at one time */
		return -EINVAL;
	if (copy_to_user(buf, outputbuf, count))
		return -EFAULT;
	return count;
}

static struct proc_dir_entry	*deviceinfodir;

static int __init init_deviceinfo(void)
{
	struct proc_dir_entry	*entry;
	int	i;

	deviceinfodir = proc_mkdir(DEVINFO_DIRNAME, &proc_root);
	if (deviceinfodir == NULL) {
		printk(KERN_ERR MODULE_NAME": can't create /proc/" DEVINFO_DIRNAME "\n");
		return -ENOMEM;
	}

	create_proc_read_entry(deviceinfo[0].name,
			       S_IRUSR | S_IRGRP | S_IROTH,
			       deviceinfodir,
			       di_list,
			       NULL);

	for (i=1; i<NUM_OF_DEVICEINFO_ENTRY; i++) {
		entry = create_proc_entry(deviceinfo[i].name,
					  S_IRUSR | S_IRGRP | S_IROTH,
					  deviceinfodir);
		if (entry) {
			deviceinfo[i].low_ino = entry->low_ino;
			entry->proc_fops = &proc_deviceinfo_operations;
		} else {
			printk( KERN_ERR MODULE_NAME
				": can't create /proc/" DEVINFO_DIRNAME
				"/%s\n", deviceinfo[i].name);
			return -ENOMEM;
		}
	}

	return 0;
}

static void __exit cleanup_deviceinfo(void)
{
	int i;
	for (i=0; i<NUM_OF_DEVICEINFO_ENTRY; i++)
		remove_proc_entry(deviceinfo[i].name, deviceinfodir);
	remove_proc_entry(DEVINFO_DIRNAME, &proc_root);
}

module_init(init_deviceinfo);
module_exit(cleanup_deviceinfo);

MODULE_AUTHOR("SHARP");
MODULE_DESCRIPTION("Device specific information");

EXPORT_NO_SYMBOLS;
