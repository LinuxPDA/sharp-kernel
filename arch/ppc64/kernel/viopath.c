/*
 *  arch/ppc64/viopath.c
 *
 *  iSeries Virtual I/O Message Path code
 *
 *  Author: Dave Boutcher <boutcher@us.ibm.com>
 * (C) Copyright 2000 IBM Corporation
 * 
 * This program is free software;  you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) anyu later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of 
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.  
 *
 * You should have received a copy of the GNU General Public License 
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
 *
 */
#ifndef _HVLPEVENT_H
#include <asm/iSeries/HvLpEvent.h>
#endif
#ifndef _HVLPConfig_H
#include <asm/iSeries/HvLpConfig.h>
#endif
#ifndef _HVCallCfg_H
#include <asm/iSeries/HvCallCfg.h>
#endif

#include <linux/config.h>
#include <asm/uaccess.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/vmalloc.h>
#include <linux/string.h>
#include <linux/proc_fs.h>
#include <linux/pci.h>
#include <linux/wait.h>

#ifndef _VIO_H
#include <asm/iSeries/vio.h>
#endif
#ifndef _MF_H
#include <asm/iSeries/mf.h>
#endif
#ifndef _ISERIES_PROC_H
#include <asm/iSeries/iSeries_proc.h>
#endif

/* Status of the path to each other partition in the system.
 * This is overkill, since we will only ever establish connections
 * to our hosting partition and the primary partition on the system.
 * But this allows for other support in the future.
 */
static struct viopathStatus {
  int isOpen:1;                  /* Did we open the path?            */
  int isActive:1;                /* Do we have a mon msg outstanding */
#if defined(CONFIG_VIOCONS)
  int xCharUsers;              
#endif 
#if defined(CONFIG_VIODASD)
  int xBlockUsers;
#endif
#if defined(CONFIG_VIOCD)
  int xCDUsers;
#endif
#if defined(CONFIG_VIOTAPE)
  int xTapeUsers;
#endif
  int xConfigUsers;
  HvLpInstanceId mSourceInst;
  HvLpInstanceId mTargetInst;
  int numberAllocated;
  spinlock_t statuslock;
  unsigned long statuslockFlags;
} viopathStatus[HVMAXARCHITECTEDLPS];

/* We use this structure to handle asynchronous responses.  The caller
 * blocks on the semaphore and the handler posts the semaphore.
 */
struct doneAllocParms_t {
  struct semaphore *sem;
  int number;
};

/* Put a sequence number in each mon msg.  The value is not
 * important.  Start at something other than 0 just for
 * readability.  wrapping this is ok.
 */
static u8 viomonseq = 22;

/* Our hosting logical partition.  We get this at startup
 * time, and different modules access this variable directly.
 */
HvLpIndex viopath_hostLp;

/* For each kind of incoming event we set a pointer to a
 * routine to call.
 */
static vio_event_handler_t *vio_handleCharEvent;
static vio_event_handler_t *vio_handleBlockEvent;
static vio_event_handler_t *vio_handleCDEvent;
static vio_event_handler_t *vio_handleTapeEvent;

/*
 * For each kind of event we allocate a buffer that is
 * guaranteed not to cross a page boundary
 */
void * vio_char_event_buffer;
void * vio_block_event_buffer;
void * vio_cd_event_buffer;
void * vio_tape_event_buffer;

/***************************************************************************
 * A page to build an lp event in                      
 ***************************************************************************/
unsigned long VIOReqPage;

/***************************************************************************
 * Handle reads from the proc file system
 ***************************************************************************/
static int proc_read(char *buf, char **start, off_t offset,
			  int blen, int *eof, void *data)
{
  HvLpEvent_Rc hvrc;
  DECLARE_MUTEX_LOCKED(Semaphore);
  dma_addr_t dmaa = pci_map_single( NULL, buf, PAGE_SIZE, PCI_DMA_FROMDEVICE);
  int len = PAGE_SIZE;
  
  if (len > blen)
    len = blen;

  memset(buf,0x00,len);
  hvrc = HvCallEvent_signalLpEventFast(viopath_hostLp,
				       HvLpEvent_Type_VirtualIo,
				       viomajorsubtype_config | vioconfigget,
				       HvLpEvent_AckInd_DoAck,
				       HvLpEvent_AckType_ImmediateAck,
				       viopath_sourceinst(viopath_hostLp),
				       viopath_targetinst(viopath_hostLp),
				       (u64)(unsigned long)&Semaphore,
				       VIOVERSION << 16,
				       ((u64)dmaa) << 32,
				       len,
				       0,
				       0);
  if (hvrc != HvLpEvent_Rc_Good)
    {
      printk("viopath hv error on op %d\n",(int)hvrc);
    }
  
  down(&Semaphore);

  pci_unmap_single( NULL, dmaa, PAGE_SIZE, PCI_DMA_FROMDEVICE);

  *eof = 1;
  return strlen(buf);
}

/***************************************************************************
 * Handle writes to our proc file system
 ***************************************************************************/
static int proc_write(struct file *file, const char *buffer,
			      unsigned long count, void *data)
{
  printk("viopath: in proc_write, got %ld bytes starting with %c\n",
	 count, buffer[0]);
  return count;
}

/***************************************************************************
 * setup our proc file system entries
 ***************************************************************************/
void vio_proc_init(struct proc_dir_entry *iSeries_proc)
{
  struct proc_dir_entry *ent;
  ent = create_proc_entry("config", S_IFREG|S_IRUSR, iSeries_proc);
  if (!ent) return;
  ent->nlink = 1;
  ent->data = NULL;
  ent->read_proc = proc_read;
  ent->write_proc = proc_write;
}

/* Boot time initialization of the vio path code.  Needs 
 * to be called before any VIO components start
 */
void viopath_init(void)
{
  int i;
  memset(viopathStatus,0x00,sizeof(viopathStatus));
  for (i=0; i<HVMAXARCHITECTEDLPS; i++)
    spin_lock_init(&viopathStatus[i].statuslock);

  vio_handleCharEvent = NULL;
  vio_handleBlockEvent = NULL;
  vio_handleCDEvent = NULL;
  vio_handleTapeEvent = NULL;

  /*
   * Figure out our hosting partition.  This isn't allowed to change
   * while we're active
   */
  viopath_hostLp = HvCallCfg_getHostingLpIndex(HvLpConfig_getLpIndex());

  /*
   * If we have a valid hosting LP, create a proc file system entry
   * for config information
   */
  if (viopath_hostLp != HvLpIndexInvalid)
    {
      iSeries_proc_callback(&vio_proc_init);
    }
}

/* See if a given LP is active.  Allow for invalid lps to be passed in
 * and just return invalid
 */
int viopath_isactive(HvLpIndex lp)
{
  if (lp == HvLpIndexInvalid)
    return 0;
  if (lp < HVMAXARCHITECTEDLPS)
    return  viopathStatus[lp].isActive;
  else
    return 0;
}

/* We cache the source and target instance ids for each
 * partition.  
 */
HvLpInstanceId viopath_sourceinst(HvLpIndex lp)
{
  return viopathStatus[lp].mSourceInst;
}

HvLpInstanceId viopath_targetinst(HvLpIndex lp)
{
  return viopathStatus[lp].mTargetInst;
}

/* We have a lock for the status for each partition.  We lock
 * when we change things in our viopathstatus structure
 */
void viopath_statuslock(HvLpIndex lp)
{
  spin_lock_irqsave(&viopathStatus[lp].statuslock, 
		    viopathStatus[lp].statuslockFlags);
  
}

void viopath_statusunlock(HvLpIndex lp)
{
  spin_unlock_irqrestore(&viopathStatus[lp].statuslock, 
			 viopathStatus[lp].statuslockFlags);
}

/* Send a monitor message.  This is a message with the acknowledge
 * bit on that the other side will NOT explicitly acknowledge.  WHen
 * the other side goes down, the hypervisor will acknowledge any
 * outstanding messages....so we will know when the other side dies.
 */
static void sendMonMsg(HvLpIndex remoteLp)
{
  HvLpEvent_Rc hvrc;

  viopathStatus[remoteLp].mSourceInst = 
    HvCallEvent_getSourceLpInstanceId(remoteLp, HvLpEvent_Type_VirtualIo);
  viopathStatus[remoteLp].mTargetInst = 
    HvCallEvent_getTargetLpInstanceId(remoteLp, HvLpEvent_Type_VirtualIo);

	 
  hvrc  = HvCallEvent_signalLpEventFast(remoteLp,
					HvLpEvent_Type_VirtualIo,
					viomajorsubtype_monitor,
					HvLpEvent_AckInd_DoAck,
					HvLpEvent_AckType_DeferredAck,
					viopathStatus[remoteLp].mSourceInst,
					viopathStatus[remoteLp].mTargetInst,
					viomonseq++, 0, 0, 0, 0, 0);
  
  printk("viopath: send mon msg %d, setting sinst %d, tinst %d\n",
	 viomonseq,
	 viopathStatus[remoteLp].mSourceInst,
	 viopathStatus[remoteLp].mTargetInst);
  if (hvrc == HvLpEvent_Rc_Good)
    {
      viopathStatus[remoteLp].isActive = 1;
    }
  else
    {
      viopathStatus[remoteLp].isActive = 0;
    }
}

static void handleMonitorEvent(struct HvLpEvent *event)
{
  HvLpIndex remoteLp;
  if (event->xFlags.xFunction == HvLpEvent_Function_Int)
    {
      printk("viopath: got monitor INT event\n");
      remoteLp = event->xSourceLp;
      if (!viopathStatus[remoteLp].isActive)
	sendMonMsg(remoteLp);
    }
  else
    {
      remoteLp = event->xTargetLp;
      printk("viopath: got monitor ACK event %d, sinst %d, tinst %d\n",
	     (int)event->xCorrelationToken,
	     event->xSourceInstanceId,
	     event->xTargetInstanceId);
      /* Other partition went away! */
      if ((event->xSourceInstanceId != viopathStatus[remoteLp].mSourceInst) ||
	  (event->xTargetInstanceId != viopathStatus[remoteLp].mTargetInst))
	{
	  printk("viopath: ignoring ack....mismatched instances\n");
	}
      else
	{
	  printk("viopath: closing path %d\n",remoteLp);
	  
	  viopathStatus[remoteLp].isActive = 0;
	  
	  if (vio_handleBlockEvent!= NULL)
	    (*vio_handleBlockEvent)(NULL);
	  
	  if (vio_handleCharEvent != NULL)
	    (*vio_handleCharEvent)(NULL);
	  
	  if (vio_handleCDEvent != NULL)
	    (*vio_handleCDEvent)(NULL);

	  if (vio_handleTapeEvent != NULL)
	    (*vio_handleTapeEvent)(NULL);
	}
    }
}

void vio_setBlockHandler(vio_event_handler_t *beh)
{
  vio_handleBlockEvent = *beh;
}

void vio_clearBlockHandler(void)
{
  vio_handleBlockEvent = NULL;
}

void vio_setCharHandler(vio_event_handler_t *ceh)
{
  vio_handleCharEvent = *ceh;
}

void vio_clearCharHandler(void)
{
  vio_handleCharEvent = NULL;
}

void vio_setCDHandler(vio_event_handler_t *ceh)
{
  vio_handleCDEvent = *ceh;
}

void vio_clearCDHandler(void)
{
  vio_handleCDEvent = NULL;
}

void vio_setTapeHandler(vio_event_handler_t *ceh)
{
  vio_handleTapeEvent = *ceh;
}

void vio_clearTapeHandler(void)
{
  vio_handleTapeEvent = NULL;
}

static void vio_handleEvent(struct HvLpEvent *event, struct pt_regs *regs)
{
  HvLpIndex remoteLp;
  if (event->xFlags.xFunction == HvLpEvent_Function_Int)
    {
      remoteLp = event->xSourceLp;
      if (event->xSourceInstanceId != viopathStatus[remoteLp].mTargetInst)
	{
	  printk("viopath: int msg rcvd, source inst (%d) doesnt match (%d)\n",
		 viopathStatus[remoteLp].mTargetInst,
		 event->xSourceInstanceId);
	  return;
	}
      
      if (event->xTargetInstanceId != viopathStatus[remoteLp].mSourceInst)
	{
	  printk("viopath: int msg rcvd, target inst (%d) doesnt match (%d)\n",
		 viopathStatus[remoteLp].mSourceInst,
		 event->xTargetInstanceId);
	  return;
	}
    }
  else
    {
      remoteLp = event->xTargetLp;
      if (event->xSourceInstanceId != viopathStatus[remoteLp].mSourceInst)
	{
	  printk("viopath: ack msg rcvd, source inst (%d) doesnt match (%d)\n",
		 viopathStatus[remoteLp].mSourceInst,
		 event->xSourceInstanceId);
	  return;
	}
      
      if (event->xTargetInstanceId != viopathStatus[remoteLp].mTargetInst)
	{
	  printk("viopath: ack msg rcvd, target inst (%d) doesnt match (%d)\n",
		 viopathStatus[remoteLp].mTargetInst,
		 event->xTargetInstanceId);
	  return;
	}
    }
    
      
  switch (event->xSubtype & VIOMAJOR_SUBTYPE_MASK) {
  case viomajorsubtype_config: 
    up((struct semaphore *)event->xCorrelationToken);
    break;

  case viomajorsubtype_monitor: 
    handleMonitorEvent(event);
    break;
  case viomajorsubtype_blockio: 
    if (vio_handleBlockEvent)
      {
	(*vio_handleBlockEvent)(event);
      }
    else
      {
	printk("vio: unexpected virtual blockio event\n");
	if (event->xFlags.xAckInd == HvLpEvent_AckInd_DoAck)
	  {
	    event->xRc = HvLpEvent_Rc_InvalidSubtype;
	    HvCallEvent_ackLpEvent(event); 
	  }
      }
    break;
  case viomajorsubtype_chario: 
    if (vio_handleCharEvent)
      {
	(*vio_handleCharEvent)(event);
      }
    else
      {
	printk("vio: unexpected virtual chario event\n");
	if (event->xFlags.xAckInd == HvLpEvent_AckInd_DoAck)
	  {
	    event->xRc = HvLpEvent_Rc_InvalidSubtype;
	    HvCallEvent_ackLpEvent(event); 
	  }
      }
    break;
  case viomajorsubtype_cdio: 
    if (vio_handleCDEvent)
      {
	(*vio_handleCDEvent)(event);
      }
    else
      {
	printk("vio: unexpected virtual cd event\n");
	if (event->xFlags.xAckInd == HvLpEvent_AckInd_DoAck)
	  {
	    event->xRc = HvLpEvent_Rc_InvalidSubtype;
	    HvCallEvent_ackLpEvent(event); 
	  }
      }
    break;
  case viomajorsubtype_tape: 
    if (vio_handleTapeEvent)
      {
	(*vio_handleTapeEvent)(event);
      }
    else
      {
	printk("vio: unexpected virtual tape event\n");
	if (event->xFlags.xAckInd == HvLpEvent_AckInd_DoAck)
	  {
	    event->xRc = HvLpEvent_Rc_InvalidSubtype;
	    HvCallEvent_ackLpEvent(event); 
	  }
      }
    break;
  default:
    printk("vio: unexpected virtual io event subtype %d\n",event->xSubtype);
    if (event->xFlags.xAckInd == HvLpEvent_AckInd_DoAck)
      {
	event->xRc = HvLpEvent_Rc_InvalidSubtype;
	HvCallEvent_ackLpEvent(event); 
      }
  }
}


static void viopath_donealloc(void *parm, int number)
{
    struct doneAllocParms_t *doneAllocParmsp = (struct doneAllocParms_t *)parm;
    doneAllocParmsp->number = number;
    up(doneAllocParmsp->sem);
}

int viopath_open(HvLpIndex remoteLp, int subtype)
{
  HvLpEvent_Rc hvrc; 
  struct doneAllocParms_t doneAllocParms;
  DECLARE_MUTEX_LOCKED(Semaphore);

  if ((remoteLp >= HvMaxArchitectedLps) || (remoteLp == HvLpIndexInvalid))
    return -EINVAL;

  if (VIOReqPage == 0)
    {
      /* Get a page to build read/write LP events in    */
      VIOReqPage = get_free_page(GFP_KERNEL);
      if (!VIOReqPage)
	{
	  printk("viopath: error allocating I/O memory\n");
	  return -ENOMEM;
	}
      
      vio_char_event_buffer = (void *)VIOReqPage;
      vio_block_event_buffer = (void *)(VIOReqPage+256);
      vio_cd_event_buffer = (void *)(VIOReqPage+512);
      vio_tape_event_buffer = (void *)(VIOReqPage+768);
    }

  if ( (0) 
#if defined(CONFIG_VIODASD)
       || (subtype == viomajorsubtype_blockio)
#endif
#if defined(CONFIG_VIOCONS)
       || (subtype == viomajorsubtype_chario)
#endif 
#if defined(CONFIG_VIOCD)
       || (subtype == viomajorsubtype_cdio)
#endif 
#if defined(CONFIG_VIOTAPE)
       || (subtype == viomajorsubtype_tape)
#endif        
       )
    {
      viopath_statuslock(remoteLp);
      switch (subtype) {
#if defined(CONFIG_VIODASD)
      case viomajorsubtype_blockio:
	viopathStatus[remoteLp].xBlockUsers++;
	break;
#endif
#if defined(CONFIG_VIOCONS)
      case viomajorsubtype_chario:
	viopathStatus[remoteLp].xCharUsers++;
	break;
#endif
#if defined(CONFIG_VIOCD)
      case viomajorsubtype_cdio:
	viopathStatus[remoteLp].xCDUsers++;
	break;
#endif
#if defined(CONFIG_VIOTAPE)
      case viomajorsubtype_tape:
	viopathStatus[remoteLp].xTapeUsers++;
	break;
#endif
	  default:
      }
      if (!viopathStatus[remoteLp].isOpen)
	{
	  HvCallEvent_openLpEventPath(remoteLp, HvLpEvent_Type_VirtualIo);
	  
	  viopathStatus[remoteLp].mSourceInst = HvCallEvent_getSourceLpInstanceId(remoteLp, HvLpEvent_Type_VirtualIo);
	  viopathStatus[remoteLp].mTargetInst = HvCallEvent_getTargetLpInstanceId(remoteLp, HvLpEvent_Type_VirtualIo);

	  printk("viopath: open, setting sinst %d, tinst %d\n",
		 viopathStatus[remoteLp].mSourceInst,
		 viopathStatus[remoteLp].mTargetInst);
	 
	  doneAllocParms.sem = &Semaphore;

	  mf_allocateLpEvents(remoteLp,
			      HvLpEvent_Type_VirtualIo,
			      250, /* TODO: Put a sizeof VIOLpEvent in here! */
			      25,  /* TODO: Work out the real number */
			      &viopath_donealloc,
			      &doneAllocParms);

	  down(&Semaphore);

	  viopathStatus[remoteLp].numberAllocated = doneAllocParms.number;

	  HvLpEvent_registerHandler(HvLpEvent_Type_VirtualIo, &vio_handleEvent);

	  viopathStatus[remoteLp].isOpen = 1;
	  
	  hvrc  = HvCallEvent_signalLpEventFast(remoteLp,
						HvLpEvent_Type_VirtualIo,
						viomajorsubtype_monitor,
						HvLpEvent_AckInd_DoAck,
						HvLpEvent_AckType_DeferredAck,
						viopathStatus[remoteLp].mSourceInst,
						viopathStatus[remoteLp].mTargetInst,
						0, 0, 0, 0, 0, 0);
	  
	  if (hvrc == HvLpEvent_Rc_Good)
	    {
	      viopathStatus[remoteLp].isActive = 1;
	    }
	  else
	    {
	      viopathStatus[remoteLp].isActive = 0;
	    }
	}
      viopath_statusunlock(remoteLp);

      return 0;
    }
  else /* invalid subtype */
    {
      printk("viopath: invalid path subtype %d\n",subtype);
      return -EINVAL;
    }
}

int viopath_close(HvLpIndex remoteLp, int subtype)
{
  printk("viopath: close(%d,%4.4x)\n",remoteLp,subtype);
  
  if ((remoteLp >= HvMaxArchitectedLps) || (remoteLp == HvLpIndexInvalid))
    return -EINVAL;

  if ( (0) 
#if defined(CONFIG_VIODASD)
       || (subtype == viomajorsubtype_blockio)
#endif
#if defined(CONFIG_VIOCONS)
       || (subtype == viomajorsubtype_chario)
#endif 
#if defined(CONFIG_VIOCD)
       || (subtype == viomajorsubtype_cdio)
#endif 
#if defined(CONFIG_VIOTAPE)
       || (subtype == viomajorsubtype_tape)
#endif 
       )
    {
      viopath_statuslock(remoteLp);
      switch (subtype) {
#if defined(CONFIG_VIODASD)
      case viomajorsubtype_blockio:
	viopathStatus[remoteLp].xBlockUsers--;
	break;
#endif
#if defined(CONFIG_VIOCONS)
      case viomajorsubtype_chario:
	viopathStatus[remoteLp].xCharUsers--;
	break;
#endif
#if defined(CONFIG_VIOCD)
      case viomajorsubtype_cdio:
	viopathStatus[remoteLp].xCDUsers--;
	break;
#endif
#if defined(CONFIG_VIOTAPE)
      case viomajorsubtype_tape:
	viopathStatus[remoteLp].xTapeUsers--;
	break;
#endif
      default:
      }

      if ((viopathStatus[remoteLp].isOpen)
#if defined(CONFIG_VIODASD)
	  && (viopathStatus[remoteLp].xBlockUsers == 0)
#endif
#if defined(CONFIG_VIOCONS)
	  && (viopathStatus[remoteLp].xCharUsers == 0)
#endif 
#if defined(CONFIG_VIOCD)
	  && (viopathStatus[remoteLp].xCDUsers == 0)
#endif 
#if defined(CONFIG_VIOTAPE)
	  && (viopathStatus[remoteLp].xTapeUsers == 0)
#endif 
	  )
	{
	  printk("viopath: closing event path\n");
	  HvCallEvent_closeLpEventPath(remoteLp, HvLpEvent_Type_VirtualIo);
	  viopathStatus[remoteLp].isOpen = 0;
	  viopathStatus[remoteLp].isActive = 0;
	}
      viopath_statusunlock(remoteLp);

      return 0;
    }
  else /* invalid subtype */
    {
      printk("viopath: invalid path subtype %d\n",subtype);
      return -EINVAL;
    }
}
