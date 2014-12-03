/* deviceinfo.h:	device-specific information
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
 *
 * ChangeLog:
 *      1-Nov-2003 Sharp Corporation   for Tosa
 *
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

#define DI_UNKNOWN	"unknown"
#define STRING_SIZE	32

static int proc_read_deviceinfo(struct file * file, char * buf,
				size_t nbytes, loff_t *ppos);
static int proc_write_deviceinfo(struct file * file, const char * buf,
				 size_t nbytes, loff_t *ppos);

static struct file_operations proc_deviceinfo_operations = {
  read:		proc_read_deviceinfo,
  write:	proc_write_deviceinfo,
};

typedef struct deviceinfo_entry {
  char * name;
  char * description;
  unsigned short low_ino;
  char string_buffer[STRING_SIZE];
} deviceinfo_entry_t;

static deviceinfo_entry_t deviceinfo[] =
{
  /* list must be the first */
  {".list",	"all of device information"},
  /* mandatory entries */
  {"filever",	"file format version"},
  {"vendor",	"device vendor name"},
  {"product",	"product name"},
  {"revision",	"ROM version"},
  /* optional entries */
  {"locale",	"locale"},
  {"serial",	"device individual id"},
  {"checksum",	"ROM checksum"},
  {"bootstr",	"boot string"},
  {"hardno",	"hardware number"},
  {"equipment",	"built-in equipment"},
};

#define NUM_OF_DEVICEINFO_ENTRY	(sizeof(deviceinfo)/sizeof(deviceinfo_entry_t))

static int di_list(char *page, char **start, off_t off,
		   int count, int *eof, void *data)
{
  int	i;
  char	*p = page;
  int	len;

  for (i=1; i<NUM_OF_DEVICEINFO_ENTRY; i++) {
    if (deviceinfo[i].string_buffer[0]==0) {
      p += sprintf(p, "%s:  %s\n",
		   deviceinfo[i].name,
		   DI_UNKNOWN);
    } else {
      p += sprintf(p, "%s:  %s\n",
		   deviceinfo[i].name,
		   deviceinfo[i].string_buffer);
    }
  }

  len = p - page;

  if (len <= off+count) *eof = 1;
  *start = page + off;
  len -= off;
  if (len>count) len = count;
  if (len<0) len = 0;

  return len;
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

  if (deviceinfo[i].string_buffer[0]!=0) {
    count = sprintf(outputbuf, "%s\n", deviceinfo[i].string_buffer);
  } else {
    count = sprintf(outputbuf, "%s\n", DI_UNKNOWN);
  }

  *ppos+=count;
  if (count>nbytes)	/* Assume output can be read at one time */
    return -EINVAL;
  if (copy_to_user(buf, outputbuf, count))
    return -EFAULT;
  return count;
}

static int proc_write_deviceinfo(struct file * file, const char * buf,
				size_t nbytes, loff_t *ppos)
{
  int i_ino = (file->f_dentry->d_inode)->i_ino;
  int count;
  int i;
  char *pt;
  int len;

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

  if (deviceinfo[i].string_buffer[0]!=0)
    return -EINVAL;

  len = STRING_SIZE-1;
  pt = strchr(buf,'\n');
  if (pt) {
    len = pt-buf;
  }
  if (len>STRING_SIZE-1) len = STRING_SIZE-1;

  deviceinfo[i].string_buffer[STRING_SIZE-1]=0;
  strncpy(deviceinfo[i].string_buffer,buf,len);
  count = strlen(deviceinfo[i].string_buffer);

  return nbytes+count;
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
      deviceinfo[i].string_buffer[0] = 0;
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
// eof
