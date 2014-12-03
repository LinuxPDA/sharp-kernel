/*
 * 
 * Common prep/pmac/chrp boot and setup code.
 *
 * Copyright (C) 2001 PPC64 Team, IBM Corp
 *
 *      This program is free software; you can redistribute it and/or
 *      modify it under the terms of the GNU General Public License
 *      as published by the Free Software Foundation; either version
 *      2 of the License, or (at your option) any later version.
 */

#include <linux/config.h>
#include <linux/module.h>
#include <linux/string.h>
#include <linux/sched.h>
#include <linux/init.h>
#include <linux/reboot.h>
#include <linux/delay.h>
#include <linux/blk.h>
#include <linux/ide.h>
#include <asm/init.h>
#include <asm/io.h>
#include <asm/prom.h>
#include <asm/processor.h>
#include <asm/pgtable.h>
#include <asm/bootinfo.h>
#include <asm/smp.h>
#include <asm/elf.h>
#include <asm/machdep.h>
#include <asm/iSeries/LparData.h>
#include <asm/Naca.h>
#include <asm/Paca.h>
#include <asm/ppcdebug.h>

extern unsigned long klimit;
/* extern void *stab; */
extern HTAB htab_data;
extern unsigned long loops_per_jiffy;

int have_of = 1;

extern void  chrp_init(unsigned long r3,
		       unsigned long r4,
		       unsigned long r5,
		       unsigned long r6,
		       unsigned long r7);

extern void chrp_init_map_io_space( void );
extern void iSeries_init( void );
extern void iSeries_init_early( void );
extern void mm_init_ppc64( void ); 

unsigned long decr_overclock = 1;
unsigned long decr_overclock_proc0 = 1;
unsigned long decr_overclock_set = 0;
unsigned long decr_overclock_proc0_set = 0;

#ifdef CONFIG_XMON
extern void xmon_map_scc(void);
#endif

char saved_command_line[256];
unsigned char aux_device_present;
struct int_control_struct int_control =
{
	__no_use_cli,
	__no_use_sti,
	__no_use_restore_flags,
	__no_use_save_flags
};
struct ide_machdep_calls ppc_ide_md;
int parse_bootinfo(void);

void parse_cmd_line(unsigned long r3, unsigned long r4, unsigned long r5,
		  unsigned long r6, unsigned long r7);

unsigned long DMA_MODE_READ, DMA_MODE_WRITE;
int _machine = _MACH_chrp; /* DRENG prom.c needs this assumption, a better way needs to be found. */

#ifdef CONFIG_MAGIC_SYSRQ
unsigned long SYSRQ_KEY;
#endif /* CONFIG_MAGIC_SYSRQ */

struct machdep_calls ppc_md;
struct Naca *naca;

/*
 * Perhaps we can put the pmac screen_info[] here
 * on pmac as well so we don't need the ifdef's.
 * Until we get multiple-console support in here
 * that is.  -- Cort
 * Maybe tie it to serial consoles, since this is really what
 * these processors use on existing boards.  -- Dan
 */ 
struct screen_info screen_info = {
	0, 25,			/* orig-x, orig-y */
	0,			/* unused */
	0,			/* orig-video-page */
	0,			/* orig-video-mode */
	80,			/* orig-video-cols */
	0,0,0,			/* ega_ax, ega_bx, ega_cx */
	25,			/* orig-video-lines */
	1,			/* orig-video-isVGA */
	16			/* orig-video-points */
};

/*
 * These are used in binfmt_elf.c to put aux entries on the stack
 * for each elf executable being started.
 */
int dcache_bsize;
int icache_bsize;
int ucache_bsize;

/*
 * Initialize the PPCDBG state.  Called before relocation has been enabled.
 */
void ppcdbg_initialize(void) {
	unsigned long offset = reloc_offset();
	struct Naca *_naca = RELOC(naca);

	_naca->debug_switch = PPC_DEBUG_DEFAULT;
}

void naca_init(void) {
   naca->xRamDisk = NULL;	
   naca->xRamDiskSize = 0;		/* in pages */
}

/* 
 * Initialize a set of PACA's, one for each processor.
 *
 * At this point, relocation is on, but we have not done any other
 * setup of the mm subsystem.
 */
void paca_init(void) {
#if 0
  int processorCount = naca->processorCount, i;
  struct Paca *paca[];

  /* Put the array of paca's on a page boundary & allocate 1/2 page of */
  /* storage for each.                                                 */  
  klimit += (PAGE_SIZE-1) & PAGE_MASK;
  naca->xPaca = paca[0] = klimit;
  klimit += ((PAGE_SIZE>>1) * processorCount); 

  for(i=0; i<processorCount; i++) {
    paca[0]->xPacaIndex = i;
  }
#endif
}

/*
 * Determine the type of system this we are running on.  
 * For the PPC64 kernel, we assume chrp.
 */
void identify_machine(void) {
  if ( itLpNaca.xLparInstalled == 1 )
          _machine = _MACH_iSeries;
  else
	  _machine = _MACH_chrp;
}

/*
 * Do some initial setup of the system.  The paramters are those which 
 * were passed in from the bootloader.
 */
void setup_system(unsigned long r3, unsigned long r4, unsigned long r5,
		  unsigned long r6, unsigned long r7) {

  identify_machine();
  
  switch (_machine) {
  	case _MACH_chrp:
    		parse_bootinfo();
    		chrp_init_map_io_space();
		break;
	case _MACH_iSeries:
		iSeries_init_early();
    		break;
  	default:
  }

  udbg_init();

  udbg_puts("\n-----------------------------------------------------\n");
  udbg_puts("Naca Info...\n\n");
  udbg_puts("naca                       = 0x");
  udbg_puthex((unsigned long)naca);
  udbg_putc('\n');

  udbg_puts("naca->processorCount       = 0x");
  udbg_puthex(naca->processorCount);
  udbg_putc('\n');

  udbg_puts("naca->physicalMemorySize   = 0x");
  udbg_puthex(naca->physicalMemorySize);
  udbg_putc('\n');

  udbg_puts("naca->dCacheL1LineSize     = 0x");
  udbg_puthex(naca->dCacheL1LineSize);
  udbg_putc('\n');

  udbg_puts("naca->dCacheL1LogLineSize  = 0x");
  udbg_puthex(naca->dCacheL1LogLineSize);
  udbg_putc('\n');

  udbg_puts("naca->dCacheL1LinesPerPage = 0x");
  udbg_puthex(naca->dCacheL1LinesPerPage);
  udbg_putc('\n');

  udbg_puts("naca->iCacheL1LineSize     = 0x");
  udbg_puthex(naca->iCacheL1LineSize);
  udbg_putc('\n');

  udbg_puts("naca->iCacheL1LogLineSize  = 0x");
  udbg_puthex(naca->iCacheL1LogLineSize);
  udbg_putc('\n');

  udbg_puts("naca->iCacheL1LinesPerPage = 0x");
  udbg_puthex(naca->iCacheL1LinesPerPage);
  udbg_putc('\n');

  udbg_puts("naca->serialPortAddr       = 0x");
  udbg_puthex(naca->serialPortAddr);
  udbg_putc('\n');

  udbg_puts("naca->interrupt_controller = 0x");
  udbg_puthex(naca->interrupt_controller);
  udbg_putc('\n');

  udbg_printf("\nHTAB Info ...\n\n"); 
  udbg_printf("htab_data.htab      = 0x%lx\n", htab_data.htab);
  udbg_printf("htab_data.num_ptegs = 0x%lx\n", htab_data.htab_num_ptegs);

  udbg_puts("\n-----------------------------------------------------\n");


  if ( _machine == _MACH_chrp ) {
      finish_device_tree();
      chrp_init(r3, r4, r5, r6, r7);
  }

  mm_init_ppc64();
  
  switch (_machine) {
  case _MACH_chrp: // DRENG _MACH_chrp needs to be _MACH_pSeries now
	// The following relies on the device tree being fully configured.
	parse_cmd_line(r3, r4, r5, r6, r7);

	break;
  case _MACH_iSeries:
	iSeries_init();
	break;
  default:
    // Need a way to die here-prob have to display something to the panel DRENG
  }
}
 
/*
 * I really need to add multiple-console support... -- Cort
 */
int __init pmac_display_supported(char *name)
{
	return 0;
}
void __init pmac_find_display(void)
{
}

void machine_restart(char *cmd)
{
	ppc_md.restart(cmd);
}
  
void machine_power_off(void)
{
	ppc_md.power_off();
}
  
void machine_halt(void)
{
	ppc_md.halt();
}
  
/*
 * get_cpuinfo - Get information on one CPU for use by procfs.
 *
 *	Prints info on the next CPU into buffer.  Beware, doesn't check for
 *	buffer overflow.  Current implementation of procfs assumes that the
 *	resulting data is <= 1K.
 *
 * Args:
 *	buffer	-- you guessed it, the data buffer
 *	cpu_np	-- Input: next cpu to get (start at 0).  Output: Updated.
 *
 *	Returns number of bytes written to buffer.
 */

int get_cpuinfo(char *buffer, unsigned *cpu_np)
{
	unsigned long len = 0;
	unsigned long i;
	unsigned int pvr;
	unsigned short maj, min;
	
#ifdef CONFIG_SMP
#define CPU_PRESENT(x) (cpu_callin_map[(x)])
#else
#define CPU_PRESENT(x) ((x)==0)
#define smp_num_cpus 1
#endif

	i = *cpu_np;
	while (i < NR_CPUS && (cpu_online_map & (1 << i)) == 0) {
		++i;
	}
	if (n >= NR_CPUS) {
		/* Insert summary data as a fake CPU at NR_CPUS. */
		if (n > NR_CPUS) {
			*cpu_np = NR_CPUS + 1;
			return (0);
		}
#ifdef CONFIG_SMP
		{ unsigned long bogosum = 0;

		for ( i = 0; i < smp_num_cpus ; i++ )
		{
			if ( !CPU_PRESENT(i) )
				continue;
			bogosum += naca->loops_per_jiffy;
		}
		len += sprintf(buffer+len,"total bogomips\t: %lu.%02lu\n",
			       (bogosum+2500)/(500000/HZ),
			       (bogosum+2500)/(5000/HZ) % 100);
		}
#endif /* CONFIG_SMP */
		if (ppc_md.get_cpuinfo != NULL)
		{
			len += ppc_md.get_cpuinfo(buffer+len);
		}
		*cpu_np = NR_CPUS;
		return (len);
	}
	*cpu_np = i + 1;

	len += sprintf(len+buffer,"processor\t: %lu\n",i);
	len += sprintf(len+buffer,"cpu\t\t: ");

	pvr = naca->paca[i].pvr;

	switch (PVR_VER(pvr))
	{
	case PV_PULSAR:
		len += sprintf(len+buffer, "RS64-III (pulsar)\n");
		break;
	case PV_POWER4:
		len += sprintf(len+buffer, "POWER4 (gp)\n");
		break;
	case PV_ICESTAR:
		len += sprintf(len+buffer, "RS64-III (icestar)\n");
		break;
	case PV_SSTAR:
		len += sprintf(len+buffer, "RS64-IV (sstar)\n");
		break;
	case PV_630:
		len += sprintf(len+buffer, "POWER3 (630)\n");
		break;
	case PV_630p:
		len += sprintf(len+buffer, "POWER3 (630+)\n");
		break;
	default:
		len += sprintf(len+buffer, "Unknown (%08x)\n", pvr);
		break;
	}
	
	/*
	 * Assume here that all clock rates are the same in a
	 * smp system.  -- Cort
	 */
	if ( _machine != _MACH_iSeries ) {
	struct device_node *cpu_node;
	int *fp;
		
	cpu_node = find_type_devices("cpu");
	if ( !cpu_node ) break;
	{
	  int s;
	  for ( s = 0; (s < i) && cpu_node->next ;
		s++, cpu_node = cpu_node->next )
	    /* nothing */ ;
	}
	fp = (int *) get_property(cpu_node, "clock-frequency", NULL);
	if ( !fp ) break;
	len += sprintf(len+buffer, "clock\t\t: %dMHz\n",
		       *fp / 1000000);
	}

	if (ppc_md.setup_residual != NULL)
	{
		len += ppc_md.setup_residual(buffer + len);
	}
	
	maj = (pvr >> 8) & 0xFF;
	min = pvr & 0xFF;

	len += sprintf(len+buffer, "revision\t: %hd.%hd\n", maj, min);

	len += sprintf(buffer+len, "bogomips\t: %lu.%02lu\n\n",
		       ((naca->loops_per_jiffy)+2500)/(500000/HZ),
		       ((naca->loops_per_jiffy)+2500)/(5000/HZ) % 100);

	return len;
}

/*
 * Fetch the cmd_line from open firmware. */
void parse_cmd_line(unsigned long r3, unsigned long r4, unsigned long r5,
		  unsigned long r6, unsigned long r7) {
    struct device_node *chosen;
    char *p;

#ifdef CONFIG_BLK_DEV_INITRD
    if (r3 && r4 && r4 != 0xdeadbeef)
      {
	if (r3 < KERNELBASE)
	  r3 += KERNELBASE;
	initrd_start = r3;
	initrd_end = r3 + r4;
	ROOT_DEV = MKDEV(RAMDISK_MAJOR, 0);
	initrd_below_start_ok = 1;
      }
#endif

    cmd_line[0] = 0;
    chosen = find_devices("chosen");
    if (chosen != NULL) {
      p = get_property(chosen, "bootargs", NULL);
      if (p != NULL)
	strncpy(cmd_line, p, sizeof(cmd_line));
    }
    cmd_line[sizeof(cmd_line) - 1] = 0;

    /* Look for mem= option on command line */
    if (strstr(cmd_line, "mem=")) {
	char *p, *q;
	unsigned long maxmem = 0;
	extern unsigned long __max_memory;

	for (q = cmd_line; (p = strstr(q, "mem=")) != 0; ) {
	    q = p + 4;
	    if (p > cmd_line && p[-1] != ' ')
		continue;
	    maxmem = simple_strtoul(q, &q, 0);
	    if (*q == 'k' || *q == 'K') {
		maxmem <<= 10;
		++q;
	    } else if (*q == 'm' || *q == 'M') {
		maxmem <<= 20;
		++q;
	    }
	}
	__max_memory = maxmem;
    }
    ppc_md.progress("id mach: done", 0x200);
}

int parse_bootinfo(void)
{
#if 0
  /* DRENG this stuff is needs to be completly redone in the context of
   * yaboot.  Remove for now - I think we can wait on this for some time.
   */
	struct bi_record *rec;
	extern char __bss_start[];
	extern char *sysmap;
	extern unsigned long sysmap_size;

	rec = (struct bi_record *)_ALIGN((ulong)__bss_start+(1<<20)-1,(1<<20));
	if ( rec->tag != BI_FIRST )
	{
		/*
		 * This 0x10000 offset is a terrible hack but it will go away when
		 * we have the bootloader handle all the relocation and
		 * prom calls -- Cort
		 */
		rec = (struct bi_record *)_ALIGN((ulong)__bss_start+0x10000+(1<<20)-1,(1<<20));
		if ( rec->tag != BI_FIRST )
			return -1;
	}
	for ( ; rec->tag != BI_LAST ;
	      rec = (struct bi_record *)((ulong)rec + rec->size) )
	{
		ulong *data = rec->data;
		switch (rec->tag)
		{
		case BI_CMD_LINE:
			memcpy(cmd_line, (void *)data, rec->size);
			break;
		case BI_SYSMAP:
			sysmap = (char *)((data[0] >= (KERNELBASE)) ? data[0] :
					  (data[0]+KERNELBASE));
			sysmap_size = data[1];
			break;
#ifdef CONFIG_BLK_DEV_INITRD
		case BI_INITRD:
			initrd_start = data[0];
			initrd_end = data[0] + rec->size;
			break;
#endif /* CONFIG_BLK_DEV_INITRD */
		}
	}
#endif	
	return 0;
}

void __init ppc_init(void)
{
	/* clear the progress line */
	ppc_md.progress(" ", 0xffff);

	if (ppc_md.init != NULL) {
		ppc_md.init();
	}
}

/*
 * Called into from start_kernel, after lock_kernel has been called.
 * Initializes bootmem, which is unsed to manage page allocation until
 * mem_init is called.
 */
void __init setup_arch(char **cmdline_p)
{
	extern int panic_timeout;
	extern char _etext[], _edata[];
	extern void do_init_bootmem(void);

#ifdef CONFIG_XMON
	xmon_map_scc();
	if (strstr(cmd_line, "xmon"))
		xmon(0);
#endif /* CONFIG_XMON */
#ifdef CONFIG_KDB
	xmon_map_scc();	/* in kdb/start.c --need to rename TAI */
#endif
	ppc_md.progress("setup_arch:enter", 0x3eab);

#if defined(CONFIG_KGDB)
	kgdb_map_scc();
	set_debug_traps();
	breakpoint();
#endif
	/*
	 * Set cache line size based on type of cpu as a default.
	 * Systems with OF can look in the properties on the cpu node(s)
	 * for a possibly more accurate value.
	 */
	dcache_bsize = naca->dCacheL1LineSize; 
	icache_bsize = naca->iCacheL1LineSize; 

	/* reboot on panic */
	panic_timeout = 180;

	init_mm.start_code = PAGE_OFFSET;
	init_mm.end_code = (unsigned long) _etext;
	init_mm.end_data = (unsigned long) _edata;
	init_mm.brk = (unsigned long) klimit;
	
	/* Save unparsed command line copy for /proc/cmdline */
	strcpy(saved_command_line, cmd_line);
	*cmdline_p = cmd_line;

	/* set up the bootmem stuff with available memory */
	do_init_bootmem();
	ppc_md.progress("setup_arch:bootmem", 0x3eab);

	ppc_md.setup_arch();

	paging_init();
	ppc_md.progress("setup_arch: exit", 0x3eab);
}

void ppc_generic_ide_fix_driveid(struct hd_driveid *id)
{
        int i;
	unsigned short *stringcast;

	id->config         = __le16_to_cpu(id->config);
	id->cyls           = __le16_to_cpu(id->cyls);
	id->reserved2      = __le16_to_cpu(id->reserved2);
	id->heads          = __le16_to_cpu(id->heads);
	id->track_bytes    = __le16_to_cpu(id->track_bytes);
	id->sector_bytes   = __le16_to_cpu(id->sector_bytes);
	id->sectors        = __le16_to_cpu(id->sectors);
	id->vendor0        = __le16_to_cpu(id->vendor0);
	id->vendor1        = __le16_to_cpu(id->vendor1);
	id->vendor2        = __le16_to_cpu(id->vendor2);
	stringcast = (unsigned short *)&id->serial_no[0];
	for (i = 0; i < (20/2); i++)
	        stringcast[i] = __le16_to_cpu(stringcast[i]);
	id->buf_type       = __le16_to_cpu(id->buf_type);
	id->buf_size       = __le16_to_cpu(id->buf_size);
	id->ecc_bytes      = __le16_to_cpu(id->ecc_bytes);
	stringcast = (unsigned short *)&id->fw_rev[0];
	for (i = 0; i < (8/2); i++)
	        stringcast[i] = __le16_to_cpu(stringcast[i]);
	stringcast = (unsigned short *)&id->model[0];
	for (i = 0; i < (40/2); i++)
	        stringcast[i] = __le16_to_cpu(stringcast[i]);
	id->dword_io       = __le16_to_cpu(id->dword_io);
	id->reserved50     = __le16_to_cpu(id->reserved50);
	id->field_valid    = __le16_to_cpu(id->field_valid);
	id->cur_cyls       = __le16_to_cpu(id->cur_cyls);
	id->cur_heads      = __le16_to_cpu(id->cur_heads);
	id->cur_sectors    = __le16_to_cpu(id->cur_sectors);
	id->cur_capacity0  = __le16_to_cpu(id->cur_capacity0);
	id->cur_capacity1  = __le16_to_cpu(id->cur_capacity1);
	id->lba_capacity   = __le32_to_cpu(id->lba_capacity);
	id->dma_1word      = __le16_to_cpu(id->dma_1word);
	id->dma_mword      = __le16_to_cpu(id->dma_mword);
	id->eide_pio_modes = __le16_to_cpu(id->eide_pio_modes);
	id->eide_dma_min   = __le16_to_cpu(id->eide_dma_min);
	id->eide_dma_time  = __le16_to_cpu(id->eide_dma_time);
	id->eide_pio       = __le16_to_cpu(id->eide_pio);
	id->eide_pio_iordy = __le16_to_cpu(id->eide_pio_iordy);
	for (i = 0; i < 2; i++)
		id->words69_70[i] = __le16_to_cpu(id->words69_70[i]);
        for (i = 0; i < 4; i++)
                id->words71_74[i] = __le16_to_cpu(id->words71_74[i]);
	id->queue_depth	   = __le16_to_cpu(id->queue_depth);
	for (i = 0; i < 4; i++)
		id->words76_79[i] = __le16_to_cpu(id->words76_79[i]);
	id->major_rev_num  = __le16_to_cpu(id->major_rev_num);
	id->minor_rev_num  = __le16_to_cpu(id->minor_rev_num);
	id->command_set_1  = __le16_to_cpu(id->command_set_1);
	id->command_set_2  = __le16_to_cpu(id->command_set_2);
	id->cfsse          = __le16_to_cpu(id->cfsse);
	id->cfs_enable_1   = __le16_to_cpu(id->cfs_enable_1);
	id->cfs_enable_2   = __le16_to_cpu(id->cfs_enable_2);
	id->csf_default    = __le16_to_cpu(id->csf_default);
	id->dma_ultra      = __le16_to_cpu(id->dma_ultra);
	id->word89         = __le16_to_cpu(id->word89);
	id->word90         = __le16_to_cpu(id->word90);
	id->CurAPMvalues   = __le16_to_cpu(id->CurAPMvalues);
	id->word92         = __le16_to_cpu(id->word92);
	id->hw_config      = __le16_to_cpu(id->hw_config);
	for (i = 0; i < 32; i++)
		id->words94_125[i]  = __le16_to_cpu(id->words94_125[i]);
	id->last_lun       = __le16_to_cpu(id->last_lun);
	id->word127        = __le16_to_cpu(id->word127);
	id->dlf            = __le16_to_cpu(id->dlf);
	id->csfo           = __le16_to_cpu(id->csfo);
	for (i = 0; i < 26; i++)
		id->words130_155[i] = __le16_to_cpu(id->words130_155[i]);
	id->word156        = __le16_to_cpu(id->word156);
	for (i = 0; i < 3; i++)
		id->words157_159[i] = __le16_to_cpu(id->words157_159[i]);
	for (i = 0; i < 96; i++)
		id->words160_255[i] = __le16_to_cpu(id->words160_255[i]);
}

void exception_trace(unsigned long trap) {
  unsigned long x, srr0, srr1, reg20, reg1, reg21;

  asm("mflr %0" : "=r" (x) :);
  asm("mfspr %0,0x1a" : "=r" (srr0) :);
  asm("mfspr %0,0x1b" : "=r" (srr1) :);
  asm("mr %0,1" : "=r" (reg1) :);
  asm("mr %0,20" : "=r" (reg20) :);
  asm("mr %0,21" : "=r" (reg21) :);

  udbg_puts("\n");
  udbg_puts("Took an exception : "); udbg_puthex(x); udbg_puts("\n");
  udbg_puts("   "); udbg_puthex(reg1); udbg_puts("\n");
  udbg_puts("   "); udbg_puthex(reg20); udbg_puts("\n");
  udbg_puts("   "); udbg_puthex(reg21); udbg_puts("\n");
  udbg_puts("   "); udbg_puthex(srr0); udbg_puts("\n");
  udbg_puts("   "); udbg_puthex(srr1); udbg_puts("\n");
}

/* This should only be called on processor 0 during calibrate decr */
void setup_default_decr(void)
{
	struct Paca * paca = (struct Paca *)mfspr(SPRG3);

	if ( decr_overclock_set && !decr_overclock_proc0_set )
		decr_overclock_proc0 = decr_overclock;

	paca->default_decr = tb_ticks_per_jiffy / decr_overclock_proc0;	
	paca->next_jiffy_update_tb = get_tb() + tb_ticks_per_jiffy;
}

int set_decr_overclock_proc0( char * str )
{
	unsigned long val = simple_strtoul( str, NULL, 0 );
	if ( ( val >= 1 ) && ( val <= 48 ) ) {
		decr_overclock_proc0_set = 1;
		decr_overclock_proc0 = val;
		printk("proc 0 decrementer overclock factor of %ld\n", val);
	}
	else
		printk("invalid proc 0 decrementer overclock factor of %ld\n", val);
	return 1;
}

int set_decr_overclock( char * str )
{
	unsigned long val = simple_strtoul( str, NULL, 0 );
	if ( ( val >= 1 ) && ( val <= 48 ) ) {
		decr_overclock_set = 1;
		decr_overclock = val;
		printk("decrementer overclock factor of %ld\n", val);
	}
	else
		printk("invalid decrementer overclock factor of %ld\n", val);
	return 1;

}

__setup("decr_overclock_proc0=", set_decr_overclock_proc0 );
__setup("decr_overclock=", set_decr_overclock );

