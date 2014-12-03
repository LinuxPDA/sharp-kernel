/*
 * linux/drivers/usb/device/storage_fd/schedule_task.c - schedule task library
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

/******************************************************************************
** Include File
******************************************************************************/
#include <linux/config.h>
#include <linux/fs.h>
#include <linux/string.h>
#include <linux/mm.h>
#include <linux/dcache.h>
#include <linux/init.h>
#include <linux/quotaops.h>
#include <linux/slab.h>
#include <linux/cache.h>
#include <linux/swap.h>
#include <linux/swapctl.h>
#include <linux/prefetch.h>
#include <linux/locks.h>
#include <asm/uaccess.h>

#include "schedule_task.h"

/******************************************************************************
** Macro Define
******************************************************************************/
#define TASK_DESC_NUM   (512)

/******************************************************************************
** Structure Define
******************************************************************************/

/**************************************
** TASK_DESC
**************************************/
typedef struct _TASK_DESC{
    struct _TASK_DESC*  next;
    SCHEDULE_TASK_FUNC  task_entry;
    int                 task_param1;
    int                 task_param2;
    int                 task_param3;
    int                 task_param4;
    int                 task_param5;
    char*               file;
    int                 line;
} TASK_DESC;

/**************************************
** OS_WRP_TASK_DATA
**************************************/
typedef struct{
    volatile TASK_DESC* read_desc;
    volatile TASK_DESC* write_desc;
    TASK_DESC           desc_pool[TASK_DESC_NUM];
    spinlock_t          spin_lock;
    struct tq_struct    task_que;
    unsigned long       use_num;
    unsigned long       max_num;
} TASK_DATA;

/******************************************************************************
** Variable Declaration
******************************************************************************/
static TASK_DATA    TaskData;

/******************************************************************************
** Local Function Prototype
******************************************************************************/
static void task_entry(void*);

/******************************************************************************
** Global Function
******************************************************************************/
void schedule_task_init(void)
{
    int i;
    
    /* 0 clear */
    memset(&TaskData, 0x00, sizeof(TaskData));
    
    /* Read/write pointer initialize */
    TaskData.read_desc = TaskData.write_desc = &TaskData.desc_pool[0];
    
    /* Ling buffer initialize */
    for(i=0; i<(TASK_DESC_NUM-1); i++){
        TaskData.desc_pool[i].next = &TaskData.desc_pool[i+1];
    }
    TaskData.desc_pool[i].next = &TaskData.desc_pool[0];
    
    /* Spin lock initialize */
    spin_lock_init(&TaskData.spin_lock);
    
    /* Task queue initialize */
    PREPARE_TQUEUE(&TaskData.task_que, task_entry, &TaskData);
    
    return;
}

int schedule_task_register(SCHEDULE_TASK_FUNC entry, int param1, int param2,
                           int param3, int param4, int param5)
{
    unsigned long   flags = 0;
    
    spin_lock_irqsave(&TaskData.spin_lock, flags);
    
    /* Free descriptor check */
    if(TaskData.write_desc->next == TaskData.read_desc){
        printk(KERN_INFO "storage_fd: schedule task no descriptor.\n");
        spin_unlock_irqrestore(&TaskData.spin_lock, flags);
        return -1;
    }
    
    /* Descriptor set */
    TaskData.write_desc->task_entry  = entry;
    TaskData.write_desc->task_param1 = param1;
    TaskData.write_desc->task_param2 = param2;
    TaskData.write_desc->task_param3 = param3;
    TaskData.write_desc->task_param4 = param4;
    TaskData.write_desc->task_param5 = param5;
    
    /* Pointer update */
    TaskData.write_desc = TaskData.write_desc->next;
    
    /* Statistics set */
    TaskData.use_num++;
    if(TaskData.use_num > TaskData.max_num){
        TaskData.max_num = TaskData.use_num;
    }
    
    spin_unlock_irqrestore(&TaskData.spin_lock, flags);
    
    /* Task queue register */
    schedule_task(&TaskData.task_que);
    
    return 0;
}

void schedule_task_all_unregister(void)
{
    unsigned long   flags = 0;
    
    spin_lock_irqsave(&TaskData.spin_lock, flags);
    TaskData.read_desc = TaskData.write_desc;
    TaskData.use_num = 0;
    spin_unlock_irqrestore(&TaskData.spin_lock, flags);
}

ssize_t schedule_task_proc_read(struct file* file, char* buf, size_t count,
                                loff_t* pos)
{
    char    string[1024];
    int     len = 0;

    len += sprintf(string + len, "Schedule task max num:0x%d\n",
            TASK_DESC_NUM);

    len += sprintf(string + len, "Schedule task use num:0x%ld\n",
            TaskData.use_num);

    len += sprintf(string + len, "Schedule task max use num:0x%ld\n",
            TaskData.max_num);
    
    *pos += len;
    if(len > count){
        len = -EINVAL;
    }
    else
    if(len > 0 && copy_to_user(buf, string, len)) {
        len = -EFAULT;
    }

    return len;
}

/******************************************************************************
** Local Function
******************************************************************************/
static void task_entry(void* data)
{
    int                 cond;
    unsigned long       flags = 0;
    volatile TASK_DESC* desc;
    
    for(;;){
        
        spin_lock_irqsave(&TaskData.spin_lock, flags);
        desc = TaskData.read_desc;
        cond = (TaskData.read_desc == TaskData.write_desc);
        spin_unlock_irqrestore(&TaskData.spin_lock, flags);
        
        if(cond) break;
        
        /* Task call */
        desc->task_entry(desc->task_param1, desc->task_param2,
                      desc->task_param3, desc->task_param4, desc->task_param5);
        
        spin_lock_irqsave(&TaskData.spin_lock, flags);
        
        /* Pointer update */
        TaskData.read_desc = TaskData.read_desc->next;
        
        /* Statistics set */
        TaskData.use_num--;
        
        spin_unlock_irqrestore(&TaskData.spin_lock, flags);
    }
    
    return;
}

