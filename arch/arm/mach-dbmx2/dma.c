/*
 * linux/arch/arm/mach-dbmx2/dma.c
 *
 * Copyright (C) 2003 Lineo Solutions, Inc.
 *
 * Based on:
 *  linux/arch/arm/mach-mx2ads/dma.c
 *
 *  Copyright (C) 1995-2000 Russell King
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 *  Front-end to the DMA handling.  This handles the allocation/freeing
 *  of DMA channels, and provides a unified interface to the machines
 *  DMA facilities.
 */
#include <linux/config.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/sched.h>
#include <linux/mman.h>
#include <linux/init.h>
#include <linux/spinlock.h>
#include <linux/interrupt.h>	

#include <linux/proc_fs.h>

#include <asm/dma.h>		

#include <asm/arch/hardware.h>
#include <asm/arch/mx2.h>
#include <asm/arch/pll.h>

//#define DEBUG

#ifdef DEBUG
#define TRACE(fmt, args...) printk("%s: " fmt, __FUNCTION__ , ## args)
#else
#define TRACE(fmt, args...)
#endif

static struct proc_dir_entry * g_proc_dir;

static mx2_dma_t dma_chan[MX2_DMA_CHANNELS];
 
/*
 * DMA interface functions
 */
static spinlock_t dma_list_lock;

static void dbmx2_dma_irq_handler(int irq, void *dev_id, struct pt_regs *regs)
{
	mx2_dma_t *dma = (mx2_dma_t *) dev_id;
	int channel;
	int mode=0;
	
	channel = dma - dma_chan;
	
	TRACE("Channel %d\n", channel);	
	if(_reg_DMA_DISR&(1<<channel))
		mode |= DMA_DONE;
				
	if(_reg_DMA_DBTOSR&(1<<channel))
		mode |= DMA_BURST_TIMEOUT;
			
	if(_reg_DMA_DSESR&(1<<channel))
		mode |= DMA_TRANSFER_ERROR;
				
	if(_reg_DMA_DBOSR&(1<<channel))
		mode |= DMA_BUFFER_OVERFLOW;
	
	if(_reg_DMA_DRTOSR&(1<<channel))
		mode |= DMA_REQUEST_TIMEOUT;
			
	dma->status = mode;
		
	if(dma->callback)
	{
		dma->arg = &mode;	// pass this as the argument
		dma->callback(dma->arg);
	}
	else TRACE("ERROR: No callback function!\n");

	_reg_DMA_DISR	= 1<<channel;
	_reg_DMA_DBTOSR	= 1<<channel;
	_reg_DMA_DRTOSR	= 1<<channel;
	_reg_DMA_DSESR	= 1<<channel;
	_reg_DMA_DBOSR	= 1<<channel;
	
	dma->active = 0;
}

int dbmx2_get_dma_list(char *buf)
{
	mx2_dma_t *dma;

	char *p = buf;
	int i;
	p+=sprintf(p,"MX2 DMA:\n");
	for (i = 0, dma = dma_chan; i < MX2_DMA_CHANNELS; i++, dma++) {
		if (dma->in_use)
			p += sprintf(p, "DMA Channel   %2d: %s\n", i, dma->device_name);
		else
			p += sprintf(p, "DMA Channel   %2d: free\n", i);
	}

	return p - buf;
}

int  dbmx2_dma_get_config(dmach_t channel, dma_request_t *p_request_t)
{	
	dma_regs_t *regs;
	
	if(!p_request_t)
	{
		TRACE("ERROR: Request pointer is NULL!\n");
		return -1;
	}
		
	if ((unsigned)channel >= MX2_DMA_CHANNELS)
	{
		TRACE("ERROR: Invalid channel\n");
		return -EINVAL;
	}

	if(!dma_chan[channel].in_use)
	{
		TRACE("ERROR: Channel is not in use\n");
		return -EINVAL;
	}
	
	regs = dma_chan[channel].regs;
	
	p_request_t->dir            =regs->Ctl&DMA_CTL_MDIR? 1:0;
	p_request_t->burstLength    =regs->BurstLength;
	p_request_t->repeat 	    =regs->Ctl&DMA_CTL_RPT? 1:0;
	p_request_t->sourceType     =DMA_CTL_GET_SMOD(regs->Ctl);
	p_request_t->sourceAddr     =(u32*)regs->SourceAddr;
	p_request_t->sourcePort     =DMA_CTL_GET_SSIZ(regs->Ctl);
	p_request_t->destType  	    =DMA_CTL_GET_DMOD(regs->Ctl);
	p_request_t->destAddr 	    =(u32*)regs->DestAddr;
	p_request_t->destPort  	    =DMA_CTL_GET_DSIZ(regs->Ctl);
	p_request_t->request 	    =regs->RequestSource;
	p_request_t->count	    =regs->Count;
	   
	p_request_t->callback	    =dma_chan[channel].callback;
	p_request_t->arg	    =dma_chan[channel].arg;
	
	return 0;
}

int  dbmx2_dma_set_config(dmach_t channel, dma_request_t *p_request_t)
{
	dma_regs_t *regs;

	if(!p_request_t)
	{	
		TRACE("ERROR request\n");
		return -1;
	}
	if ((unsigned)channel >= MX2_DMA_CHANNELS)
	{
		TRACE("ERROR channel\n");
		return -EINVAL;  
	}
	if(!dma_chan[channel].in_use)
	{
		TRACE("ERROR channel not in used\n");
		return -EINVAL;
	}
	if(p_request_t->dir>2)
	{
		TRACE("ERROR DIR\n");
		return -1;
	}
	if(p_request_t->repeat>2)
	{
		TRACE("ERROR repeat\n");
		return -1;
	}
	if(p_request_t->sourceType>3)
	{
		TRACE("ERROR memory source type\n");
		return -1;
	}
	if(p_request_t->destType>3)
	{
		TRACE("ERROR destType\n");
		return -1;	
	}
	if(p_request_t->destPort>3)
	{
		TRACE("ERROR dest Port \n");
		return -1;
	}
	if(p_request_t->sourcePort>3)
	{
		TRACE("ERROR source Port \n");
		return -1;
	}
	regs=dma_chan[channel].regs;
	
	regs->BurstLength	=p_request_t->burstLength;
	regs->SourceAddr	=(u32)p_request_t->sourceAddr;	
	regs->DestAddr		=(u32)p_request_t->destAddr;		
	regs->RequestSource	=p_request_t->request;	
	regs->Count		=p_request_t->count;

	if(p_request_t->dir)
		regs->Ctl	|=DMA_CTL_MDIR;
	else
		regs->Ctl	&=~DMA_CTL_MDIR;
	
	if(p_request_t->repeat)
		regs->Ctl	|=DMA_CTL_RPT;
	else
		regs->Ctl	&=~DMA_CTL_RPT;
	
	DMA_CTL_SET_SMOD(regs->Ctl,p_request_t->sourceType);
	DMA_CTL_SET_SSIZ(regs->Ctl,p_request_t->sourcePort);
	DMA_CTL_SET_DMOD(regs->Ctl,p_request_t->destType); 	
	DMA_CTL_SET_DSIZ(regs->Ctl,p_request_t->destPort); 
	
	dma_chan[channel].callback	=p_request_t->callback;
	dma_chan[channel].arg		=p_request_t->arg;
	return 0;    
}                

int dbmx2_request_dma (int *channel, const char *device_name)
{                
	mx2_dma_t *dma = NULL;
	dma_regs_t *regs;
	int err, i, chan;
	
	if((*channel) != -1)
	{
		if(*channel < 0 || *channel >= MX2_DMA_CHANNELS)
			return -1;
	}
	err = 0;
	
	spin_lock(&dma_list_lock);
	

	if (*channel != -1)
	{
		chan = *channel;
		if(dma_chan[chan].in_use)
		{
			err = -EBUSY;
			return err;
		}
		else
		{
			dma = &dma_chan[chan];
		}
	}
	else
	{
		for(i = 0; i < MX2_DMA_CHANNELS; i++)
		{
			if(!dma_chan[i].in_use)
			{
				dma = &dma_chan[i];
				break;
			}
		}
	}

	if(!err)
	{
		if(dma)
			dma->in_use = 1;
		else
			err = -ENOSR;
	}

	
	spin_unlock(&dma_list_lock);

	// enable DMA module
	_reg_DMA_DCR |= 0x1;

	err = request_irq(dma->irpt, dbmx2_dma_irq_handler, SA_INTERRUPT,
			  device_name, (void *) dma);
			  
	if (err)
	{
		printk(KERN_ERR
		       "%s: unable to request IRQ %d for DMA channel\n",
		       device_name, dma->irpt);
		dma->in_use = 0;
		return err;
	}

	*channel = dma - dma_chan;
	dma->device_name = device_name;
	dma->callback = NULL;
	dma->arg=NULL;
	
	_reg_DMA_DIMR &= ~(1<<(*channel)); //unmask interrupt;

	regs = dma->regs;
	memset((void*)&(regs->Ctl),0,sizeof(regs->Ctl));
	
	dma=dma_chan;
	for(i=0;i<MX2_DMA_CHANNELS;i++)
	{
		if(dma->in_use)
		{	
			_reg_DMA_DCR|=0x1;
			break;
		}
		dma++;
	}
	
	return 0;
}

void dbmx2_free_dma(dmach_t channel)
{
	mx2_dma_t *dma;
	int i;

	if ((unsigned)channel >= MX2_DMA_CHANNELS)
		return;

	dma = &dma_chan[channel];
	
	if (dma->active)
	{
		printk(KERN_ERR "Trying to free an active DMA Channel %d\n", channel);
		return;
	}
	
	if (!dma->in_use) {
		printk(KERN_ERR "Trying to free free DMA%d\n", channel);
		return;
	}

	free_irq(dma->irpt, (void *) dma);

	dma->in_use = 0;
	_reg_DMA_DIMR |= (1<<channel); //mask interrupt;
	TRACE("freed\n");
	
	dma=dma_chan;
	for(i=0;i<MX2_DMA_CHANNELS;i++)
	{
		if(dma->in_use)
		{	
			return;
		}
		dma++;
	}
	_reg_DMA_DCR&=~0x1; //none of the channels is in-use
}

int dbmx2_dma_start(dmach_t channel)
{
	mx2_dma_t *dma = &dma_chan[channel];

	if ((unsigned)channel >= MX2_DMA_CHANNELS)
	{
		TRACE("ERROR: Invalid channel\n");
		return -EINVAL;
	}
	if (!dma->in_use)
	{
		TRACE("ERROR: DMA not in use\n");
		return -EINVAL;
	}
	
	dma->status=0;	

	_reg_DMA_DISR	= 1<<channel;
	_reg_DMA_DRTOSR	= 1<<channel;
	_reg_DMA_DBTOSR	= 1<<channel;
	_reg_DMA_DSESR	= 1<<channel;
	_reg_DMA_DBOSR	= 1<<channel;
	
	dma->regs->Ctl&=~DMA_CTL_CEN;
	
	dma->regs->Ctl&=~DMA_CTL_REN;
		
	if(DMA_CTL_GET_SMOD(dma->regs->Ctl)==DMA_TYPE_FIFO)
		dma->regs->Ctl|=DMA_CTL_REN;
	
	if(DMA_CTL_GET_DMOD(dma->regs->Ctl)==DMA_TYPE_FIFO)
		dma->regs->Ctl|=DMA_CTL_REN;
	
	dma->active = 1;
	dma->regs->Ctl|=DMA_CTL_CEN;
	
	return 0;
}

int dbmx2_dma_get_status(dmach_t channel, int *status)
{
	mx2_dma_t *dma = &dma_chan[channel];

	if((unsigned)channel >= MX2_DMA_CHANNELS)
	 	return -EINVAL;
	if(!dma->in_use)
		return -EINVAL;
	
	if(_reg_DMA_DISR&(1<<channel))
		dma[channel].status|=DMA_DONE;
	
	if(_reg_DMA_DBTOSR&(1<<channel))
		dma[channel].status|=DMA_BURST_TIMEOUT;
	
	if(_reg_DMA_DRTOSR&(1<<channel))
		dma[channel].status|=DMA_REQUEST_TIMEOUT;
	
	if(_reg_DMA_DSESR&(1<<channel))
		dma[channel].status|=DMA_TRANSFER_ERROR;
	
	if(_reg_DMA_DBOSR&(1<<channel))
		dma[channel].status|=DMA_BUFFER_OVERFLOW;

	*status=dma[channel].status;
	
	return 0;
		
}

int dbmx2_get_dma_residue(dmach_t channel)
{
	if((unsigned)channel >= MX2_DMA_CHANNELS)
	 	return -EINVAL;
	
	return _reg_DMA_CNTR(1<<channel);
}


int dbmx2_dump_dma_register(dmach_t channel)
{
	mx2_dma_t *dma = &dma_chan[channel];
	
	if((unsigned)channel >= MX2_DMA_CHANNELS)
	 	return -EINVAL;
	
	if(!dma->in_use)
		return -EINVAL;
	
	printk("DMA COMMON REGISTER\n");
	printk("DMA CONTROL             DMA_DCR: %08x\n", _reg_DMA_DCR);
	printk("DMA Interrupt status    DMA_DISR: %08x\n",_reg_DMA_DISR);
	printk("DMA Interrupt Mask      DMA_DIMR: %08x\n",_reg_DMA_DIMR);
	printk("DMA Burst Time Out      DMA_DBTOSR: %08x\n",_reg_DMA_DBTOSR);
	printk("DMA request Time Out    DMA_DRTOSR: %08x\n", _reg_DMA_DRTOSR);
	printk("DMA Transfer Error      DMA_DSESR: %08x\n",_reg_DMA_DSESR);
	printk("DMA DMA_Overflow        DMA_DBOSR: %08x\n",_reg_DMA_DBOSR);
	printk("DMA Burst Time OutCtl   DMA_BurstTOCtl: %08x\n",_reg_DMA_DBTOCR);

	printk("DMA Chan %2d  Sourc     SourceAddr: %08x\n", channel, dma->regs->SourceAddr);
	printk("DMA Chan %2d  dest      DestAddr: %08x\n", channel,dma->regs->DestAddr);
	printk("DMA Chan %2d  count     Count: %08x\n", channel, dma->regs->Count);
	printk("DMA Chan %2d  Ctl       Ctl: %08x\n", channel,dma->regs->Ctl);
	printk("DMA Chan %2d  request   RequestSource: %08x\n", channel,dma->regs->RequestSource);
	printk("DMA Chan %2d  burstL    BurstLength: %08x\n", channel,dma->regs->BurstLength);
	printk("DMA Chan %2d  requestTO ReqTimeout: %08x\n", channel,dma->regs->ReqTimeout);
	printk("DMA Chan %2d  BusUtilt  BusUtilt: %08x\n", channel,dma->regs->BusUtilt);
	
	return 0;
}

/* Enable DMA channel */
void dbmx2_enable_dma (dmach_t channel)
{
	mx2_dma_t *dma = dma_chan + channel;

	if (channel >= MAX_DMA_CHANNELS)
		goto bad_dma;
		
	if (!dma->in_use)
	{
		TRACE("ERROR: DMA not in use\n");
		goto bad_dma;
	}

	dma->active = 1;
	TRACE("Enabling channel %d\n", channel);
	_reg_DMA_DISR	= 1<<channel;
	_reg_DMA_DBTOSR	= 1<<channel;
	_reg_DMA_DRTOSR	= 1<<channel;
	_reg_DMA_DSESR	= 1<<channel;
	_reg_DMA_DBOSR	= 1<<channel;
	_reg_DMA_CCR(channel) &= ~1;	// clear CEN bit
	_reg_DMA_CCR(channel) |= 1;	// set CEN bit
	return;

bad_dma:
	printk(KERN_ERR "dma: trying to enable DMA%d\n", channel);

//	dbmx2_dma_start(channel);

}

void dbmx2_disable_dma (dmach_t channel)
{
	mx2_dma_t *dma = dma_chan + channel;

	if (channel >= MAX_DMA_CHANNELS)
		goto bad_dma;

	dma->active = 0;
	_reg_DMA_CCR(channel) &= 0xFFFFFFFE;	// clear CEN bit
	return;

bad_dma:
	printk(KERN_ERR "dma: trying to disable DMA%d\n", channel);
}

void dbmx2_set_dma_mode (dmach_t channel, dmamode_t mode)
{
	mx2_dma_t *dma = dma_chan + channel;

	if (channel >= MAX_DMA_CHANNELS)
		goto bad_dma;

	if (!dma->in_use)
	{
		TRACE("Settings mode for an unallocated DMA channel\n");
	}

	if (dma->active)
		printk(KERN_ERR "dma%d: altering DMA mode while DMA active\n", channel);

	dma->mode = mode;
	return;

bad_dma:
	printk(KERN_ERR "dma: trying to set mode of DMA%d\n", channel);
}

void dbmx2_set_dma_addr (dmach_t channel, unsigned long physaddr)
{
	mx2_dma_t *dma = dma_chan + channel;

	if (channel >= MAX_DMA_CHANNELS)
		goto bad_dma;

	if (!dma->in_use)
	{
		TRACE("Settings address for an unallocated DMA channel\n");
	}

	if (dma->active)
		printk(KERN_ERR "dma%d: altering DMA address while DMA active\n", channel);
	
	if ((dma->mode & DMA_MODE_MASK) == DMA_MODE_READ)
		_reg_DMA_DAR(channel)	= physaddr;
	else	// i.e. mode is DMA_MODE_WRITE
		_reg_DMA_SAR(channel)	= physaddr;
	return;

bad_dma:
	printk(KERN_ERR "dma: trying to set address of DMA%d\n", channel);
}

void dbmx2_set_dma_count (dmach_t channel, unsigned long count)
{
	mx2_dma_t *dma = dma_chan + channel;

	if (channel >= MAX_DMA_CHANNELS)
		goto bad_dma;

	if (!dma->in_use)
	{
		TRACE("Settings count for an unallocated DMA channel\n");
	}

	if (dma->active)
		printk(KERN_ERR "dma%d: altering DMA count while DMA active\n", channel);

	_reg_DMA_CNTR(channel) = count;
	return;

bad_dma:
	printk(KERN_ERR "dma: trying to set count of DMA%d\n", channel);
}

int dbmx2_dma_set_callback(dmach_t channel, dma_callback_t cb)
{
	mx2_dma_t *dma = &dma_chan[channel];

	if ((unsigned)channel >= MAX_DMA_CHANNELS)
		return -EINVAL;

	if (!dma->in_use)
	{
		TRACE("Settings callback for an unallocated DMA channel\n");
	}
	
	if (dma->active)
	{
		printk(KERN_ERR "dma%d: changing call back function while DMA active\n", channel);
		return -EINVAL;
	}

	dma->callback = cb;
	TRACE("cb = %p\n", cb);
	return 0;
}

EXPORT_SYMBOL(dbmx2_dma_start);
EXPORT_SYMBOL(dbmx2_request_dma);
EXPORT_SYMBOL(dbmx2_dma_set_config);
EXPORT_SYMBOL(dbmx2_dma_get_config);
EXPORT_SYMBOL(dbmx2_free_dma);
EXPORT_SYMBOL(dbmx2_dma_get_status);
EXPORT_SYMBOL(dbmx2_get_dma_residue);
EXPORT_SYMBOL(dbmx2_dump_dma_register);
EXPORT_SYMBOL(dbmx2_disable_dma);
EXPORT_SYMBOL(dbmx2_set_dma_mode);
EXPORT_SYMBOL(dbmx2_set_dma_addr);
EXPORT_SYMBOL(dbmx2_set_dma_count);
EXPORT_SYMBOL(dbmx2_dma_set_callback);
EXPORT_SYMBOL(dbmx2_enable_dma);
EXPORT_SYMBOL(dbmx2_get_dma_list);


void __init arch_dma_init(void)
{
	int i;
	//*(volatile u32 *)0xf0000000=0; //enable AITC
	//*(volatile u32 *)0xf3014008=0xE00; //enable three board interrupt; 
	//TRACE("Begin Init\n");	
	mx_module_clk_open(HCLK_MODULE_DMA);
	mx_module_clk_open(IPG_MODULE_DMA);
	for(i=0;i<MX2_DMA_CHANNELS;i++)
	{
		dma_chan[i].in_use=0;
		dma_chan[i].active=0;
		dma_chan[i].regs=(dma_regs_t*)(IO_ADDRESS(DMA_CH_BASE(i)));
		dma_chan[i].callback=0;
		dma_chan[i].device_name=0;
		dma_chan[i].irpt=i+32;		//Dma channel interrupt number
	}
	
	_reg_DMA_DCR=0x2;//reset DMA;
	
	return;
}
