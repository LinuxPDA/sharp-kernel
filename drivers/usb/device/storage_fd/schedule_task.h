/*
 * linux/drivers/usb/device/storage_fd/schedule_task.h - schedule task library header
 *
 * Copyright (c) 2003 Lineo Solutions, Inc.
 *
 * Written by Shunnosuke kabata
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
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 */

#ifndef _SCHEDULE_TASK_H_
#define _SCHEDULE_TASK_H_

/******************************************************************************
** Macro Define
******************************************************************************/
typedef int (*SCHEDULE_TASK_FUNC)(int, int, int, int, int);

/******************************************************************************
** Global Function Prototype
******************************************************************************/
void    schedule_task_init(void);
int     schedule_task_register(SCHEDULE_TASK_FUNC, int, int, int, int, int);
void    schedule_task_all_unregister(void);
ssize_t schedule_task_proc_read(struct file*, char*, size_t, loff_t*);

#endif  /* _SCHEDULE_TASK_H_ */

