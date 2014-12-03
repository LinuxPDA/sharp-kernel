/*
 * PowerPC64 Segment Translation Support.
 *
 * Dave Engebretsen and Mike Corrigan {engebret|mikejc}@us.ibm.com
 *    Copyright (c) 2001 Dave Engebretsen
 * 
 *      This program is free software; you can redistribute it and/or
 *      modify it under the terms of the GNU General Public License
 *      as published by the Free Software Foundation; either version
 *      2 of the License, or (at your option) any later version.
 */

#include <asm/pgtable.h>
#include <asm/mmu.h>
#include <asm/mmu_context.h>
#include <asm/Paca.h>
#include <asm/Naca.h>
#include <asm/pmc.h>

inline int make_ste(unsigned long stab, 
		    unsigned long esid, unsigned long vsid);
inline void make_slbe(unsigned long esid, unsigned long vsid);
extern unsigned long reloc_offset(void);
extern struct Naca *naca;

/*
 * Initialize the segment table for this processor with the set
 * of kernel addresses.
 */

void
stab_initialize(unsigned long skip_esid_c, unsigned long stab) {
	unsigned long offset = reloc_offset();
	unsigned long esid, last_esid, vsid; 

	/*
	 * Put in STEs for the 0xC00000000 - 0xF00000000 segments.
	 */
	esid = GET_ESID(KERNELBASE);

	/* 
	 * If asked to skip loading esid 0xC00000000 then skip it.
	 */
	if ( skip_esid_c )
		esid += (REGION_STRIDE >> SID_SHIFT);

	/* 
	 * Put entries into the STAB for the first segment of the 4 kernel 
	 * regions (0xC - 0xF).  These are left in the table during flush.
	 * All other kernel segments (including bolted) are demand faulted.
	 */
	last_esid = GET_ESID(KERNELBASE) + (2 * (REGION_STRIDE >> SID_SHIFT));

	for (; esid <= last_esid; esid += (REGION_STRIDE >> SID_SHIFT)) {
		/* Map the esid to an EA & find the vsid we need. */
		vsid = get_kernel_vsid(esid << SID_SHIFT); 
		if(!__is_processor(PV_POWER4)) { 
			make_ste(stab, esid, vsid);
		} else {
			make_slbe(esid, vsid );
		}
	}
}

/*
 * Create a segment table entry for the given esid/vsid pair.
 */ 
inline int
make_ste(unsigned long stab, unsigned long esid, unsigned long vsid) {
	unsigned long entry, group, old_esid, castout_entry, i;
	unsigned int global_entry;
	STE *ste, *castout_ste;

	/* Search the primary group first. */
	global_entry = (esid & 0x1f) << 3;
	ste = (STE *)(stab | ((esid & 0x1f) << 7)); 

	/*
	 * Find an empty entry, if one exists.
	 */
	for(group = 0; group < 2; group++) {
		for(entry = 0; entry < 8; entry++, ste++) {
			if(!(ste->dw0.dw0.v)) {
				ste->dw1.dw1.vsid = vsid;
				/* Order VSID updte */
				__asm__ __volatile__ ("eieio" : : : "memory");
				ste->dw0.dw0.esid = esid;
				ste->dw0.dw0.v  = 1;
				ste->dw0.dw0.kp = 1;
				/* Order update     */
				__asm__ __volatile__ ("sync" : : : "memory"); 

				return(global_entry | entry);
			}
		}
		/* Now search the secondary group. */
		global_entry = ((~esid) & 0x1f) << 3;
		ste = (STE *)(stab | (((~esid) & 0x1f) << 7)); 
	}

	/*
	 * Could not find empty entry, pick one with a round robin selection.
	 * Search all entries in the two groups.  Note that the first time
	 * we get here, we start with entry REGION_COUNT - 1 so the initializer
	 * can be common with the SLB castout code.
	 */

	/* This assumes we never castout when initializing the stab. */
	PMC_SW_PROCESSOR(stab_capacity_castouts); 

	castout_entry = get_paca()->xStab_data.next_round_robin;
	for(i = 0; i < 16; i++) {
		if(castout_entry < 8) {
			global_entry = (esid & 0x1f) << 3;
			ste = (STE *)(stab | ((esid & 0x1f) << 7)); 
			castout_ste = ste + castout_entry;
		} else {
			global_entry = ((~esid) & 0x1f) << 3;
			ste = (STE *)(stab | (((~esid) & 0x1f) << 7)); 
			castout_ste = ste + (castout_entry - 8);
		}

		if((((castout_ste->dw0.dw0.esid) >> 32) == 0) ||
		   (((castout_ste->dw0.dw0.esid) & 0xffffffff) > 0)) {
			/* Found an entry to castout.  It is either a user */
			/* region, or a secondary kernel segment.          */
			break;
		}

		castout_entry = (castout_entry + 1) & 0xf;
	}

	get_paca()->xStab_data.next_round_robin = (castout_entry + 1) & 0xf;

	/* Modify the old entry to the new value. */

	/* Force previous translations to complete. DRENG */
	__asm__ __volatile__ ("isync" : : : "memory" );

	castout_ste->dw0.dw0.v = 0;
	__asm__ __volatile__ ("sync" : : : "memory" );    /* Order update */
	castout_ste->dw1.dw1.vsid = vsid;
	__asm__ __volatile__ ("eieio" : : : "memory" );   /* Order update */
	old_esid = castout_ste->dw0.dw0.esid;
	castout_ste->dw0.dw0.esid = esid;
	castout_ste->dw0.dw0.v  = 1;
	castout_ste->dw0.dw0.kp = 1;
	__asm__ __volatile__ ("slbie  %0" : : "r" (old_esid << SID_SHIFT)); 
	/* Ensure completion of slbie */
	__asm__ __volatile__ ("sync" : : : "memory" );  

	return(global_entry | (castout_entry & 0x7));
}

/*
 * Create a segment buffer entry for the given esid/vsid pair.
 */ 
inline void make_slbe(unsigned long esid, unsigned long vsid) {
	unsigned long entry, castout_entry;
	slb_dword0 castout_esid_data;
	union {
		unsigned long word0;
		slb_dword0    data;
	} esid_data;
	union {
		unsigned long word0;
		slb_dword1    data;
	} vsid_data;
	
	/*
	 * Find an empty entry, if one exists.
	 */
	for(entry = 0; entry < naca->slb_size; entry++) {
		__asm__ __volatile__("slbmfee  %0,%1" 
				     : "=r" (esid_data) : "r" (entry)); 
		if(!esid_data.data.v) {
			/* 
			 * Write the new SLB entry.
			 */
			vsid_data.word0 = 0;
			vsid_data.data.vsid = vsid;
			vsid_data.data.kp = 1;

			esid_data.word0 = 0;
			esid_data.data.esid = esid;
			esid_data.data.v = 1;
			esid_data.data.index = entry;

			/* slbie not needed as no previous mapping existed. */
			/* Order update  */
			__asm__ __volatile__ ("isync" : : : "memory");
			__asm__ __volatile__ ("slbmte  %0,%1" 
					      : : "r" (vsid_data), 
					      "r" (esid_data)); 
			/* Order update  */
			__asm__ __volatile__ ("isync" : : : "memory");
			return;
		}
	}
	
	/*
	 * Could not find empty entry, pick one with a round robin selection.
	 */

	PMC_SW_PROCESSOR(stab_capacity_castouts); 

	castout_entry = get_paca()->xStab_data.next_round_robin;
	__asm__ __volatile__("slbmfee  %0,%1" 
			     : "=r" (castout_esid_data) 
			     : "r" (castout_entry)); 

	entry = castout_entry; 
	castout_entry++; 
	if(castout_entry >= naca->slb_size) {
		castout_entry = REGION_COUNT - 1; 
	}
	get_paca()->xStab_data.next_round_robin = castout_entry;

	/* Invalidate the old entry. */
	castout_esid_data.v = 0; /* Set the class to 0 */
	/* slbie not needed as the previous mapping is still valid. */
	__asm__ __volatile__("slbie  %0" : : "r" (castout_esid_data)); 
	
	/* 
	 * Write the new SLB entry.
	 */
	vsid_data.word0 = 0;
	vsid_data.data.vsid = vsid;
	vsid_data.data.kp = 1;
	
	esid_data.word0 = 0;
	esid_data.data.esid = esid;
	esid_data.data.v = 1;
	esid_data.data.index = entry;
	
	__asm__ __volatile__ ("isync" : : : "memory");   /* Order update */
	__asm__ __volatile__ ("slbmte  %0,%1" 
			      : : "r" (vsid_data), "r" (esid_data)); 
	__asm__ __volatile__ ("isync" : : : "memory" );   /* Order update */
}

/*
 * Allocate a segment table entry for the given ea.
 */
int ste_allocate ( unsigned long ea, 
		   unsigned long trap) 
{
	unsigned long vsid, esid;

	PMC_SW_PROCESSOR(stab_faults); 

	/* Check for invalid effective addresses. */
	if (!IS_VALID_EA(ea)) {
		return 1;
	}
	
	/* Kernel or user address? */
	if (REGION_ID(ea) >= KERNEL_REGION_ID) {
		vsid = get_kernel_vsid( ea );
	} else {
		struct mm_struct *mm = current->mm;
		if ( mm ) {
			vsid = get_vsid(mm->context, ea );
		} else {
			return 1;
		}
	}

	esid = GET_ESID(ea);
	if (trap == 0x380 || trap == 0x480) {
		make_slbe(esid, vsid); 
	} else {
		unsigned char top_entry, stab_entry, *segments, i; 

		stab_entry = make_ste(get_paca()->xStab_data.virt, esid, vsid);
		PMC_SW_PROCESSOR_A(stab_entry_use, stab_entry & 0xf); 

		segments = get_paca()->xSegments;		
		top_entry = segments[0];
		if(top_entry < (STAB_CACHE_SIZE - 1)) {
			top_entry++;
			segments[top_entry] = stab_entry;
			if(top_entry == STAB_CACHE_SIZE - 1) top_entry = 0xff;
			segments[0] = top_entry;
		}
	}
	
	return(0); 
}
 
/* 
 * Flush all entries from the segment table of the current processor.
 * Kernel and Bolted entries are not removed as we cannot tolerate 
 * faults on those addresses.
 */
void flush_stab(void) {
	STE *stab = (STE *) get_paca()->xStab_data.virt;
	unsigned char *segments = get_paca()->xSegments;
	unsigned long flags, i;
	
	if(!__is_processor(PV_POWER4)) {
		unsigned long entry;
		STE *ste;

		/* Force previous translations to complete. DRENG */
		__asm__ __volatile__ ("isync" : : : "memory");

		__save_and_cli(flags);
		if(segments[0] != 0xff) {
			for(i = 1; i <= segments[0]; i++) {
				ste = stab + segments[i]; 
				ste->dw0.dw0.v = 0;
				PMC_SW_PROCESSOR(stab_invalidations); 
			}
		} else {
			/* Invalidate all entries. */
                        ste = stab;

		        /* Never flush the first four entries. */ 
		        ste += 4;
			for(entry = 4;
			    entry < (PAGE_SIZE / sizeof(STE)); 
			    entry++, ste++) {
				ste->dw0.dw0.v = 0;
				PMC_SW_PROCESSOR(stab_invalidations); 
			}
		}

		*((unsigned long *)segments) = 0;
		__restore_flags(flags);

		/* Invalidate the SLB. */
		/* Force invals to complete. */
		__asm__ __volatile__ ("sync" : : : "memory");  
		/* Flush the SLB.            */
		__asm__ __volatile__ ("slbia" : : : "memory"); 
		/* Force flush to complete.  */
		__asm__ __volatile__ ("sync" : : : "memory");  
	} else {
		/* Invalidate the entire SLB (except entry 1), and then put  */
		/* back in the bolted range translation.                     */
		/* This only does the first bolted segment presently. DRENG  */
		unsigned long msr, tmp;

		/*                      esid                       v    idx  */
		slb_dword0 esid_data = {GET_ESID(BOLTEDBASE), 1, 0,  1}; 

		/*                      vsid ks kp                           */
		slb_dword1 vsid_data = {0,    0, 1, 0, 0, 0, 0};
		vsid_data.vsid = get_kernel_vsid(BOLTEDBASE);

		PMC_SW_PROCESSOR(stab_invalidations); 

		__asm__ __volatile__("\n\
                  mfmsr  %0           \n\
                  rldicl %1,%0,48,1   \n\
                  rldicl %1,%1,16,0   \n\
                  mtmsrd %1           \n\
                  isync               \n\
                  slbia               \n\
                  isync               \n\
                  slbmte %2,%3        \n\
                  isync               \n\
                  mtmsrd %0"
                  : "=&r"(msr), "=&r"(tmp)
                  : "r" (vsid_data), "r" (esid_data)
                  : "memory"); 
	}
}
