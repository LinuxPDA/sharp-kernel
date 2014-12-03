/*
 * 
 *
 *    Copyright (c) 2000 Mike Corrigan <mikejc@us.ibm.com>
 *    Copyright (c) 1999-2000 Grant Erickson <grant@lcse.umn.edu>
 *
 *    Module name: iSeries_setup.c
 *
 *    Description:
 *      Architecture- / platform-specific boot-time initialization code for
 *      the IBM iSeries LPAR.  Adapted from original code by Grant Erickson and
 *      code by Gary Thomas, Cort Dougan <cort@fsmlabs.com>, and Dan Malek
 *      <dan@net4x.com>.
 *
 *      This program is free software; you can redistribute it and/or
 *      modify it under the terms of the GNU General Public License
 *      as published by the Free Software Foundation; either version
 *      2 of the License, or (at your option) any later version.
 */
 
#include <linux/config.h>
#include <linux/init.h>
#include <linux/threads.h>
#include <linux/smp.h>
#include <linux/param.h>
#include <linux/string.h>
#include <linux/bootmem.h>
#include <linux/blk.h>

#include <asm/processor.h>
#include <asm/machdep.h>
#include <asm/page.h>
#include <asm/mmu.h>
#include <asm/pgtable.h>
#include <asm/mmu_context.h>

#include <asm/time.h>
#include "iSeries_setup.h"
#include <asm/Naca.h>
#include <asm/Paca.h>
#include <asm/iSeries/LparData.h>
#include <asm/iSeries/HvCallHpt.h>
#include <asm/iSeries/HvLpConfig.h>
#include <asm/iSeries/HvCallEvent.h>
#include <asm/iSeries/HvCallSm.h>
#include <asm/iSeries/ItLpQueue.h>
#include <asm/iSeries/IoHriMainStore.h>
#include <asm/iSeries/iSeries_proc.h>
#include <asm/proc_pmc.h>
#include <asm/iSeries/mf.h>
#include <asm/iSeries/vio.h>

/* Function Prototypes */

extern void abort(void);
static void build_iSeries_Memory_Map( void );
static void setup_iSeries_cache_sizes( void );
static void iSeries_bolt_kernel(unsigned long saddr, unsigned long eaddr);
void build_valid_hpte( unsigned long vsid, unsigned long ea, unsigned long pa,
		       pte_t * ptep, unsigned hpteflags, unsigned bolted );
extern void ppcdbg_initialize(void);

/* Global Variables */

unsigned long procFreqHz = 0;
unsigned long procFreqMhz = 0;
unsigned long procFreqMhzHundreths = 0;

unsigned long tbFreqHz = 0;
unsigned long tbFreqMhz = 0;
unsigned long tbFreqMhzHundreths = 0;

int piranha_simulator = 0;

extern char _end[];

extern struct Naca *naca;
extern int rd_size;		/* Defined in drivers/block/rd.c */
extern unsigned long klimit;

/*
 * void __init iSeries_init_early()
 */

void __init
iSeries_init_early(void)
{

	ppcdbg_initialize();
	
#if defined(CONFIG_BLK_DEV_INITRD)
	/*
	 * If the init RAM disk has been configured and there is
	 * a non-zero starting address for it, set it up
	 */

	if ( naca->xRamDisk ) {
		initrd_start = (unsigned long)__va(naca->xRamDisk);
		initrd_end   = initrd_start + naca->xRamDiskSize * PAGE_SIZE;
		initrd_below_start_ok = 1;	// ramdisk in kernel space
		ROOT_DEV = MKDEV( RAMDISK_MAJOR, 0 );

		if ( ((rd_size*1024)/PAGE_SIZE) < naca->xRamDiskSize )
			rd_size = (naca->xRamDiskSize*PAGE_SIZE)/1024;
	} else
	
#endif /* CONFIG_BLK_DEV_INITRD */
	  {
                
		ROOT_DEV = MKDEV( VIODASD_MAJOR, 1 );
	  }

	/* Initialize the table which translate Linux physical addresses to
	 * AS/400 absolute addresses
	 */

	build_iSeries_Memory_Map();

	setup_iSeries_cache_sizes();

	/* Initialize machine-dependency vectors */

	ppc_md.setup_arch	 	= iSeries_setup_arch;
	ppc_md.setup_residual	 	= iSeries_setup_residual;
	ppc_md.get_cpuinfo	 	= iSeries_get_cpuinfo;
	ppc_md.irq_cannonicalize 	= NULL;
	ppc_md.init_IRQ		 	= iSeries_init_IRQ;
	ppc_md.get_irq		 	= iSeries_get_irq;
	ppc_md.init		 	= NULL;

	ppc_md.restart		 	= iSeries_restart;
	ppc_md.power_off	 	= iSeries_power_off;
	ppc_md.halt		 	= iSeries_halt;

	ppc_md.time_init	 	= NULL;
	ppc_md.set_rtc_time	 	= iSeries_set_rtc_time;
	ppc_md.get_rtc_time	 	= iSeries_get_rtc_time;
	ppc_md.calibrate_decr	 	= iSeries_calibrate_decr;
	ppc_md.progress			= iSeries_progress;

	ppc_md.kbd_setkeycode    	= NULL;
	ppc_md.kbd_getkeycode    	= NULL;
	ppc_md.kbd_translate     	= NULL;
	ppc_md.kbd_unexpected_up 	= NULL;
	ppc_md.kbd_leds          	= NULL;
	ppc_md.kbd_init_hw       	= NULL;

#if defined(CONFIG_MAGIC_SYSRQ)
	ppc_md.ppc_kbd_sysrq_xlate	= NULL;
#endif
	
	if ( itLpNaca.xPirEnvironMode == 0 ) 
		piranha_simulator = 1;

	return;

}

/*
 * void __init iSeries_init()
 */

void __init
iSeries_init(unsigned long r3, unsigned long r4, unsigned long r5, 
	   unsigned long r6, unsigned long r7)
{
	/* Associate Lp Event Queue 0 with processor 0 */
	HvCallEvent_setLpEventQueueInterruptProc( 0, 0 );

	{
	/* copy the command line parameter from the primary VSP  */
	char *p, *q;
	HvCallEvent_dmaToSp( cmd_line,
			     2*64*1024,
			     256,
			     HvLpDma_Direction_RemoteToLocal );

	p = q = cmd_line + 255;
	while( p > cmd_line ) {
		if ((*p == 0) || (*p == ' ') || (*p == '\n'))
			--p;
		else
			break;
	}
	if ( p < q )
		*(p+1) = 0;
	}

	iSeries_proc_early_init();	
	mf_init();

	iSeries_proc_callback( &pmc_proc_init );

	viopath_init();

	return;
}


/*
 * The iSeries may have very large memories ( > 128 GB ) and a partition
 * may get memory in "chunks" that may be anywhere in the 2**52 real
 * address space.  The chunks are 256K in size.  To map this to the 
 * memory model Linux expects, the AS/400 specific code builds a 
 * translation table to translate what Linux thinks are "physical"
 * addresses to the actual real addresses.  This allows us to make 
 * it appear to Linux that we have contiguous memory starting at
 * physical address zero while in fact this could be far from the truth.
 * To avoid confusion, I'll let the words physical and/or real address 
 * apply to the Linux addresses while I'll use "absolute address" to 
 * refer to the actual hardware real address.
 *
 * build_iSeries_Memory_Map gets information from the Hypervisor and 
 * looks at the Main Store VPD to determine the absolute addresses
 * of the memory that has been assigned to our partition and builds
 * a table used to translate Linux's physical addresses to these
 * absolute addresses.  Absolute addresses are needed when 
 * communicating with the hypervisor (e.g. to build HPT entries)
 */

static void __init build_iSeries_Memory_Map(void)
{
	u32 loadAreaFirstChunk, loadAreaLastChunk, loadAreaSize;
	u32 nextPhysChunk;
	u32 hptFirstChunk, hptLastChunk, hptSizeChunks, hptSizePages;
	u32 num_ptegs;
	u32 holeFirstChunk, holeSizeChunks;
	u32 totalChunks,moreChunks;
	u32 currChunk, thisChunk, absChunk;
	u32 currDword;
	u32 chunkBit;
	u64 holeStart, holeEnd, holeSize;
	u64 map;
	struct IoHriMainStoreSegment4 * msVpd;
 	
	/* Chunk size on iSeries is 256K bytes */
	totalChunks = (u32)HvLpConfig_getMsChunks();
	klimit = msChunks_alloc(klimit, totalChunks, 1UL<<18);

	/* Get absolute address of our load area
	 * and map it to physical address 0
	 * This guarantees that the loadarea ends up at physical 0
	 * otherwise, it might not be returned by PLIC as the first
	 * chunks
	 */
	
	loadAreaFirstChunk = (u32)addr_to_chunk(itLpNaca.xLoadAreaAddr);
	loadAreaSize =  itLpNaca.xLoadAreaChunks;

	/* Only add the pages already mapped here.  
	 * Otherwise we might add the hpt pages 
	 * The rest of the pages of the load area
	 * aren't in the HPT yet and can still
	 * be assigned an arbitrary physical address
	 */
	if ( (loadAreaSize * 64) > HvPagesToMap )
		loadAreaSize = HvPagesToMap / 64;

	loadAreaLastChunk = loadAreaFirstChunk + loadAreaSize - 1;

	printk( "Mapping load area - physical addr = 0000000000000000\n"
                "                    absolute addr = %016lx\n", 
			chunk_to_addr(loadAreaFirstChunk) );
	printk( "Load area size %dK\n", loadAreaSize*256 );
	
	for (	nextPhysChunk = 0; 
		nextPhysChunk < loadAreaSize; 
		++nextPhysChunk ) {
		msChunks.abs[nextPhysChunk] = loadAreaFirstChunk+nextPhysChunk;
	}
	
	/* Get absolute address of our HPT and remember it so
	 * we won't map it to any physical address
	 */

	hptFirstChunk = (u32)addr_to_chunk(HvCallHpt_getHptAddress());
	hptSizePages =  (u32)(HvCallHpt_getHptPages());
	hptSizeChunks = hptSizePages >> (msChunks.chunk_shift-PAGE_SHIFT);
	hptLastChunk = hptFirstChunk + hptSizeChunks - 1;
	
	printk( "HPT absolute addr = %016lx, size = %dK\n",
			chunk_to_addr(hptFirstChunk), hptSizeChunks*256 );

	/* Fill in the htab_data structure */
	
	/* Fill in size of hashed page table */
	num_ptegs = hptSizePages * (PAGE_SIZE/(sizeof(HPTE)*HPTES_PER_GROUP));
	htab_data.htab_num_ptegs = num_ptegs;
	htab_data.htab_hash_mask = num_ptegs - 1;
	
	/* The actual hashed page table is in the hypervisor, we have no direct access */
	htab_data.htab = NULL;

	/* Determine if absolute memory has any
	 * holes so that we can interpret the
	 * access map we get back from the hypervisor
	 * correctly.
	 */
	
	msVpd = (struct IoHriMainStoreSegment4 *)xMsVpd;
	holeStart = msVpd->nonInterleavedBlocksStartAdr;
	holeEnd   = msVpd->nonInterleavedBlocksEndAdr;
	holeSize = holeEnd - holeStart;

	if ( holeSize ) {
		holeStart = holeStart & 0x000fffffffffffff;
		holeStart = addr_to_chunk(holeStart);
		holeFirstChunk = (u32)holeStart;
		holeSize = addr_to_chunk(holeSize);
		holeSizeChunks = (u32)holeSize;
		printk( "Main store hole: start chunk = %0x, size = %0x chunks\n",
				holeFirstChunk, holeSizeChunks );
	}
	else {
		holeFirstChunk = 0xffffffff;
		holeSizeChunks = 0;
	}
		
	/* Process the main store access map from the hypervisor
	 * to build up our physical -> absolute translation table
	 */

	currChunk = 0;
	currDword = 0;
	moreChunks = totalChunks;

	while ( moreChunks ) {
		map = HvCallSm_get64BitsOfAccessMap( itLpNaca.xLpIndex,
						     currDword );
		thisChunk = currChunk;
		while ( map ) {
			chunkBit = map >> 63;
			map <<= 1;
			if ( chunkBit ) {
				--moreChunks;
				absChunk = thisChunk;
				if ( absChunk >= holeFirstChunk )
					absChunk += holeSizeChunks;
				if ( ( ( absChunk < hptFirstChunk ) ||
				       ( absChunk > hptLastChunk ) ) &&
				     ( ( absChunk < loadAreaFirstChunk ) ||
				       ( absChunk > loadAreaLastChunk ) ) ) {
					msChunks.abs[nextPhysChunk] = absChunk;
					++nextPhysChunk;
				}
			}
			++thisChunk;
		}
		++currDword;
		currChunk += 64;
	}
					
	/* main store size (in chunks) is 
	 *   totalChunks - hptSizeChunks
	 * which should be equal to 
	 *   nextPhysChunk
	 */
	naca->physicalMemorySize = chunk_to_addr(nextPhysChunk);

	/* Bolt kernel mappings for all of memory */
	iSeries_bolt_kernel( 0, naca->physicalMemorySize );

	lmb_init();
	lmb_add( 0, naca->physicalMemorySize );
	lmb_reserve( 0, __pa(klimit));

}

/*
 * Set up the variables that describe the cache line sizes
 * for this machine.
 */

static void __init setup_iSeries_cache_sizes(void)
{
	unsigned i,n;
	naca->iCacheL1LineSize = xIoHriProcessorVpd[0].xInstCacheOperandSize;
	naca->dCacheL1LineSize = xIoHriProcessorVpd[0].xDataCacheOperandSize;
	naca->iCacheL1LinesPerPage = PAGE_SIZE / naca->iCacheL1LineSize;
	naca->dCacheL1LinesPerPage = PAGE_SIZE / naca->dCacheL1LineSize;
	i = naca->iCacheL1LineSize;
	n = 0;
	while ((i=(i/2))) ++n;
	naca->iCacheL1LogLineSize = n;
	i = naca->dCacheL1LineSize;
	n = 0;
	while ((i=(i/2))) ++n;
	naca->dCacheL1LogLineSize = n;

	printk( "D-cache line size = %d  (log = %d)\n",
			(unsigned)naca->dCacheL1LineSize,
			(unsigned)naca->dCacheL1LogLineSize );
	printk( "I-cache line size = %d  (log = %d)\n",
			(unsigned)naca->iCacheL1LineSize,
			(unsigned)naca->iCacheL1LogLineSize );
	
}

/*
 * Bolt the kernel addr space into the HPT
 */

static void __init iSeries_bolt_kernel(unsigned long saddr, unsigned long eaddr)
{
	unsigned long pa;
	unsigned long mode_rw = _PAGE_ACCESSED | _PAGE_COHERENT | PP_RWXX;
	HPTE hpte;

	for (pa=saddr; pa < eaddr ;pa+=PAGE_SIZE) {
		unsigned long ea = (unsigned long)__va(pa);
		unsigned long vsid = get_kernel_vsid( ea );
		unsigned long va = ( vsid << 28 ) | ( pa & 0xfffffff );
		unsigned long vpn = va >> PAGE_SHIFT;
		unsigned long slot = HvCallHpt_findValid( &hpte, vpn );
		if ( hpte.dw0.dw0.v ) {
			/* HPTE exists, so just bolt it */
			HvCallHpt_setSwBits( slot, 0x10, 0 );
		} else {
			/* No HPTE exists, so create a new bolted one */
			build_valid_hpte(vsid, ea, pa, NULL, mode_rw, 1);
		}
	}
}

/*
 * Document me.
 */
void __init
iSeries_setup_arch(void)
{
	void *	eventStack;
	
	/* Setup the Lp Event Queue */

	/* Allocate a page for the Event Stack
	 * The hypervisor wants the absolute real address, so
	 * we subtract out the KERNELBASE and add in the
	 * absolute real address of the kernel load area
	 */
	
	eventStack = alloc_bootmem_pages( LpEventStackSize );
	
	memset( eventStack, 0, LpEventStackSize );
	
	/* Invoke the hypervisor to initialize the event stack */
	
	HvCallEvent_setLpEventStack( 0, eventStack, LpEventStackSize );
	
	/* Initialize fields in our Lp Event Queue */
	
	xItLpQueue.xSlicEventStackPtr = (char *)eventStack;
	xItLpQueue.xSlicCurEventPtr = (char *)eventStack;
	xItLpQueue.xSlicLastValidEventPtr = (char *)eventStack + 
					(LpEventStackSize - LpEventMaxSize);
	xItLpQueue.xIndex = 0;
	
	/* Compute processor frequency */
	procFreqHz = (((1UL<<34) * 1000000) / xIoHriProcessorVpd[0].xProcFreq );
	procFreqMhz = procFreqHz / 1000000;
	procFreqMhzHundreths = (procFreqHz/10000) - (procFreqMhz*100);

	/* Compute time base frequency */
	tbFreqHz = (((1UL<<32) * 1000000) / xIoHriProcessorVpd[0].xTimeBaseFreq );
	tbFreqMhz = tbFreqHz / 1000000;
	tbFreqMhzHundreths = (tbFreqHz/10000) - (tbFreqMhz*100);

	printk("Max  logical processors = %d\n", 
			itVpdAreas.xSlicMaxLogicalProcs );
	printk("Max physical processors = %d\n",
			itVpdAreas.xSlicMaxPhysicalProcs );
	printk("Processor frequency = %lu.%02lu\n",
			procFreqMhz, 
			procFreqMhzHundreths );
	printk("Time base frequency = %lu.%02lu\n",
			tbFreqMhz,
			tbFreqMhzHundreths );
	printk("Processor version = %x\n",
			xIoHriProcessorVpd[0].xPVR );

}

/*
 * int as400_setup_residual()
 *
 * Description:
 *   This routine pretty-prints CPU information gathered from the VPD    
 *   for use in /proc/cpuinfo                               
 *
 * Input(s):
 *  *buffer - Buffer into which CPU data is to be printed.             
 *
 * Output(s):
 *  *buffer - Buffer with CPU data.
 *
 * Returns:
 *   The number of bytes copied into 'buffer' if OK, otherwise zero or less
 *   on error.
 */
int
iSeries_setup_residual(char *buffer)
{
	int len = 0;
	
	len += sprintf(len+buffer,"clock\t\t: %lu.%02luMhz\n",
		procFreqMhz, procFreqMhzHundreths );
	len += sprintf(len+buffer,"time base\t: %lu.%02luMHz\n",
		tbFreqMhz, tbFreqMhzHundreths );
	len += sprintf(len+buffer,"i-cache\t\t: %d\n",
		naca->iCacheL1LineSize);
	len += sprintf(len+buffer,"d-cache\t\t: %d\n",
		naca->dCacheL1LineSize);


	return (len);
}

#ifdef CONFIG_SMP
void iSeries_spin_lock(spinlock_t *lock)
{
	int i;
	for (;;) {
		for (i=1024; i>0; --i) {
			HMT_low();
			if ( lock->lock == 0 ) {
				HMT_medium();
				if ( __spin_trylock(lock) )
					return;
			}
		}
		HMT_medium();
		HvCallCfg_getLps();
	}
}

void iSeries_read_lock(__rwlock_t *rw)
{
	int i;
	for (;;) {
		for (i=1024; i>0; --i) {
			HMT_low();
			if ( rw->lock >= 0 ) {
				HMT_medium();
				if ( __read_trylock(rw) )
					return;
			}
		}
		HMT_medium();
		HvCallCfg_getLps();
	}
}

void iSeries_write_lock(__rwlock_t *rw)
{
	int i;
	for (;;) {
		for (i=1024; i>0; --i) {
			HMT_low();
			if ( rw->lock == 0 ) {
				HMT_medium();
				if ( __write_trylock(rw) )
					return;
			}
		}
		HMT_medium();
		HvCallCfg_getLps();
	}
}

void iSeries_brlock_spin( spinlock_t *lock )
{
	int i;
	for(;;) {
		for (i=1024; i>0; --i) {
			HMT_low();
			if (lock->lock == 0) {
				HMT_medium();
				return;
			}
		
		}
		HMT_medium();
		HvCallCfg_getLps();
	}
}

#endif /* CONFIG_SMP */

int iSeries_get_cpuinfo(char *buffer)
{
	int len = 0;

	len += sprintf(len+buffer,"machine\t\t: 64-bit iSeries Logical Partition\n");

	return len;
}

static char * lpEventTypes[9]= {
/*       0123456789012345678901234567890	 */
	"Hypervisor\t\t",
	"Machine Facilities\t",
	"Session Manager\t\t",
	"SPD I/O\t\t\t",
	"Virtual Bus\t\t",
	"PCI I/O\t\t\t",
	"RIO I/O\t\t\t",
	"Virtual Lan\t\t",
	"Virtual I/O\t\t"
	};

int get_ioevent_data(char *buffer)
{
	int len = 0;
	unsigned i;

	len += sprintf(len+buffer,"LpEventQueue 0\n");
	len += sprintf(len+buffer,"  events processed:\t%lu\n",
			(unsigned long)xItLpQueue.xLpIntCount );
	/* display event counts by type */
	for (i=0; i<HvLpEvent_Type_NumTypes; ++i ) {
		len += sprintf(len+buffer,"    %s%lu\n", 
				lpEventTypes[i],
				(unsigned long)xItLpQueue.xLpIntCountByType[i] );
	}
	
	return len;
}
/*
 * Document me.
 * and Implement me.
 */
void __init
iSeries_init_IRQ(void)
{
	return;
}

/*
 * Document me.
 * and Implement me.
 */
int
iSeries_get_irq(struct pt_regs *regs)
{
	/* -2 means ignore this interrupt */
	return -2;
}

/*
 * Document me.
 */
void
iSeries_restart(char *cmd)
{
	mf_reboot();
}

/*
 * Document me.
 */
void
iSeries_power_off(void)
{
	mf_powerOff();
}

/*
 * Document me.
 */
void
iSeries_halt(void)
{
	mf_powerOff();
}

/*
 * Nothing to do here.
 */
void __init
iSeries_time_init(void)
{
	/* Nothing to do */
}

/* JDH Hack */
unsigned long jdh_time = 0;

/*
 * Set the RTC in the virtual service processor
 * This requires flowing LpEvents to the primary partition
 */
int
iSeries_set_rtc_time(unsigned long time)
{
    mf_setRtcTime(time);
    return 0;
}

/*
 * Get the RTC from the virtual service processor
 * This requires flowing LpEvents to the primary partition
 */
unsigned long
iSeries_get_rtc_time(void)
{
    unsigned long time;

    if ( piranha_simulator )
	    return 0;

    mf_getRtcTime(&time);
    return (time);
}

extern void setup_default_decr(void);

/*
 * void __init iSeries_calibrate_decr()
 *
 * Description:
 *   This routine retrieves the internal processor frequency from the VPD,
 *   and sets up the kernel timer decrementer based on that value.
 *
 */
void __init
iSeries_calibrate_decr(void)
{
	unsigned long	freq;
	unsigned long	cyclesPerUsec;
	unsigned long	tbf;
	
	/* Compute decrementer (and TB) frequency 
	 * in cycles/sec 
	 */
	
	tbf = xIoHriProcessorVpd[0].xTimeBaseFreq;
	
	freq = 0x0100000000;
	freq *= 1000000;		/* 2^32 * 10^6 */
	freq = freq / tbf;		/* cycles / sec */
	cyclesPerUsec = freq / 1000000;	/* cycles / usec */

	/* Set the amount to refresh the decrementer by.  This
	 * is the number of decrementer ticks it takes for 
	 * 1/HZ seconds.
	 */

	tb_ticks_per_jiffy = freq / HZ;
	tb_ticks_per_usec = cyclesPerUsec;
	setup_default_decr();
}

void __init
iSeries_progress( char * st, unsigned short code )
{
	printk( "Progress: [%04x] - %s\n", (unsigned)code, st );
#if 0
	if ( !piranha_simulator )
		mf_displayProgress( code );
#endif
}
