/*
 * fs/jffs2/jffs2_proc.h
 *
 * Copyright (C) 2002 Lineo Japan, Inc.
 *
 * May be copied or modified under the terms of the GNU General Public
 * License.  See linux/COPYING for more information.
 *
 *
 * Derived from fs/jffs/jffs_proc.h
 *
 * JFFS -- Journaling Flash File System, Linux implementation.
 *
 * Copyright (C) 2000  Axis Communications AB.
 *
 * Created by Simon Kagstrom <simonk@axis.com>.
 *
 * This is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

/* jffs2_proc.h defines a structure for inclusion in the proc-file system.  */
#ifndef __LINUX_JFFS2_PROC_H__
#define __LINUX_JFFS2_PROC_H__

#include <linux/proc_fs.h>

/* The proc_dir_entry for jffs2 (defined in jffs2_proc.c).  */
extern struct proc_dir_entry *jffs2_proc_root;

int jffs2_register_jffs2_proc_dir(kdev_t dev, struct jffs2_sb_info *c);
int jffs2_unregister_jffs2_proc_dir(struct jffs2_sb_info *c);

#endif /* __LINUX_JFFS2_PROC_H__ */
