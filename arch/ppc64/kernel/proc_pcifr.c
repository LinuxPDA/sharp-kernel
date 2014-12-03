/************************************************************************/
/* File pcifr_proc.c created by Allan Trautman on Thu Aug  2 2001.      */
/************************************************************************/
/* Supports the ../proc/ppc64/pcifr for the pci flight recorder.        */
/* Copyright (C) 20yy  <Allan H Trautman> <IBM Corp>                    */
/*                                                                      */
/* This program is free software; you can redistribute it and/or modify */
/* it under the terms of the GNU General Public License as published by */
/* the Free Software Foundation; either version 2 of the License, or    */
/* (at your option) any later version.                                  */
/*                                                                      */
/* This program is distributed in the hope that it will be useful,      */ 
/* but WITHOUT ANY WARRANTY; without even the implied warranty of       */
/* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the        */
/* GNU General Public License for more details.                         */
/*                                                                      */
/* You should have received a copy of the GNU General Public License    */ 
/* along with this program; if not, write to the:                       */
/* Free Software Foundation, Inc.,                                      */ 
/* 59 Temple Place, Suite 330,                                          */ 
/* Boston, MA  02111-1307  USA                                          */
/************************************************************************/
#include <linux/config.h>
#include <stdarg.h>
#include <linux/kernel.h>

#include <linux/proc_fs.h>
#include <asm/uaccess.h>
#include <asm/time.h>

//#include <linux/pci.h>
//#include <asm/pci-bridge.h>

#include <asm/flight_recorder.h>
#include "pci.h"

void pci_Fr_TestCode(void);

static spinlock_t proc_pcifr_lock;
struct flightRecorder* PciFr = NULL;

/************************************************************************/
/* Forward declares.                                                    */
/************************************************************************/
static struct proc_dir_entry *pciFr_proc_root = NULL;
int proc_pciFr_read_proc(char *page, char **start, off_t off, int count, int *eof, void *data);
int proc_pciFr_write_proc(struct file *file, const char *buffer, unsigned long count, void *data);

/************************************************************************/
/* Create entry ../proc/ppc64/pcifr                                     */
/************************************************************************/
void proc_pciFr_init(struct proc_dir_entry *proc_ppc64_root) {
    if (proc_ppc64_root == NULL) return;

    /* Read = User,Group,Other, Write User */ 
    printk("PCI: Creating ../proc/ppc64/pcifr \n");
    spin_lock(&proc_pcifr_lock);
    pciFr_proc_root = create_proc_entry("pcifr", S_IFREG | S_IRUGO | S_IWUSR, proc_ppc64_root);
    spin_unlock(&proc_pcifr_lock);

    if (pciFr_proc_root == NULL) return;

    pciFr_proc_root->nlink = 1;
    pciFr_proc_root->data = (void *)0;
    pciFr_proc_root->read_proc  = proc_pciFr_read_proc;
    pciFr_proc_root->write_proc = proc_pciFr_write_proc;

	PciFr = alloc_Flight_Recorder(NULL,"PciFr", 4096);
	
 	pci_Fr_TestCode();

}
/*******************************************************************************/
/* Get called when client reads the ../proc/ppc64/pcifr.                       */
/*******************************************************************************/
int proc_pciFr_read_proc(char *page, char **start, off_t off, int count, int *eof, void *data) {
    int  BufferLen;
    printk("PCI: Dump Flight Recorder!\n");
    BufferLen = fr_Dump(PciFr, page, count);
    *eof = BufferLen;
    return BufferLen;			
}
/*******************************************************************************/
/* Gets called when client writes to ../proc/ppc64/pcifr                       */
/*******************************************************************************/
int proc_pciFr_write_proc(struct file *file, const char *buffer, unsigned long count, void *data) {
 	pci_Fr_TestCode();
	return count;
}

/*******************************************************************************
 * Test Code
 *******************************************************************************/
void pci_Fr_TestCode(void) {
	int StackVariable = 10;

	fr_Log_Entry(PciFr,"1. First Log Entry");
	fr_Log_Entry(PciFr,"2. Stack Variable(10) %d",StackVariable);
	fr_Log_Entry(PciFr,"%d. Pointer 0x%16LX",3,PciFr);

	LOGFR(PciFr,"4. Test Log Hi there.");
	LOGFR(PciFr,"5. Stack Variable(10) %d",StackVariable);
	LOGFR(PciFr,"%d. Pointer 0x%16LX",6,PciFr);

	PCIFR("8. Hi there.");
	PCIFR("9. Stack Variable(10) %d",StackVariable);
	PCIFR("%d. Variable and pointer 0x%16LX",10,PciFr);
}
