/*
 * pci_dma.c
 * Copyright (C) 2001 Mike Corrigan & Dave Engebretsen, IBM Corporation
 *
 * Dynamic DMA mapping support.
 * 
 * Manages the TCE space assigned to this partition.
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
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307 USA
 */

#include <linux/init.h>
#include <linux/types.h>
#include <linux/slab.h>
#include <linux/mm.h>
#include <linux/spinlock.h>
#include <linux/string.h>
#include <linux/pci.h>
#include <asm/io.h>
#include <asm/prom.h>
#include <asm/rtas.h>
#include <asm/ppcdebug.h>

#include <asm/iSeries/HvCallXm.h>
#include <asm/iSeries/LparData.h>
#include <asm/pci_dma.h>
#include <asm/pci-bridge.h>
#include <asm/iSeries/iSeries_pci.h>

#define DEBUG_TCE 1

/* Initialize so this guy does not end up in the BSS section.
 * Only used to pass OF initialization data set in prom.c into the main 
 * kernel code -- data ultimately copied into tceTables[].
 */
extern struct _of_tce_table of_tce_table[];

struct TceTable   virtBusTceTable;	/* Tce table for virtual bus */

struct TceTable * tceTables[256];	/* Tce tables for 256 busses
					 * Bus 255 is the virtual bus
					 * zero indicates no bus defined
					 */
		  /* allocates a contiguous range of tces (power-of-2 size) */
static long       alloc_tce_range( struct TceTable *, 
				   unsigned order );
		  /* allocates a contiguous range of tces (power-of-2 size)
		   * assumes lock already held
		   */
static long       alloc_tce_range_nolock( struct TceTable *, 
					  unsigned order );
		  /* frees a contiguous range of tces (power-of-2 size) */
static void       free_tce_range( struct TceTable *, 
				  long tcenum, 
				  unsigned order );
		  /* frees a contiguous rnage of tces (power-of-2 size)
		   * assumes lock already held
		   */
static void       free_tce_range_nolock( struct TceTable *, 
				  	 long tcenum, 
					 unsigned order );
		  /* allocates a range of tces and sets them to the pages  */
static 		  dma_addr_t get_tces( struct TceTable *, 
				       unsigned order, 
				       void *page, 
			    	       unsigned numPages,
			    	       int tceType,
			    	       int direction );
static void       free_tces( struct TceTable *, 
			     dma_addr_t tce, 
			     unsigned order,
			     unsigned numPages );
static long       test_tce_range( struct TceTable *, 
				  long tcenum, 
				  unsigned order );

static unsigned   fill_scatterlist_sg( struct scatterlist *sg, int nents, 
		        	       dma_addr_t dma_addr, unsigned long numTces );

static unsigned long num_tces_sg( struct scatterlist *sg, 
				  int nents );
	
static dma_addr_t create_tces_sg( struct TceTable *tbl, 
				  struct scatterlist *sg, 
			 	  int nents, 
				  unsigned numTces,
				  int tceType, 
				  int direction );

static void getTceTableParms( struct pci_controller *phb, 
			      struct TceTable *tce_table_parms );


static unsigned long setTce( unsigned long base, 
			     unsigned long tce_num,
			     unsigned long tce_data);

static long resv_tce_range_top_level( struct TceTable *tbl, 
				      unsigned long dma_addr, 
				      unsigned size );

u8 iSeries_Get_Bus( struct pci_dev * dv )
{
	return 0;
}

/*
 * Given a pci device, return which tce table is assigned to it.
 */
unsigned long get_tce_table_index( struct pci_dev *dev) {
	unsigned long index = 
		((struct pci_controller *)dev->sysdata)->global_number;
	PPCDBG(PPCDBG_TCE, "get_tce_table_index:\n");
	PPCDBG(PPCDBG_TCE, "\tdev = 0x%lx, index = 0x%lx\n", dev, index);
	return(index); 
}

static unsigned __inline__ count_leading_zeros32( unsigned long x )
{
	unsigned lz;
	asm("cntlzw %0,%1" : "=r"(lz) : "r"(x));
	return lz;
}

static void __inline__ build_tce( struct TceTable * tbl, long tcenum, 
			     unsigned long uaddr, int tceType, int direction )
{
	u64 setTceRc;
	union Tce tce;
	
	PPCDBG(PPCDBG_TCE, "build_tce: uaddr = 0x%lx\n", uaddr);
	tce.wholeTce = 0;
	tce.tceBits.rpn = (virt_to_absolute(uaddr)) >> PAGE_SHIFT;
	/* If for virtual bus */
	if ( tceType == TCE_VB ) {
		tce.tceBits.valid = 1;
		tce.tceBits.allIo = 1;
		if ( direction != PCI_DMA_TODEVICE )
			tce.tceBits.readWrite = 1;
	}
	/* If for PCI bus */
	else {
		tce.tceBits.readWrite = 1; // Read allowed 
		if ( direction != PCI_DMA_TODEVICE )
			tce.tceBits.pciWrite = 1;
	}

#ifdef CONFIG_PPC_ISERIES
	setTceRc = HvCallXm_setTce( (u64)tbl->index, (u64)tcenum, tce.wholeTce );
#else
	setTceRc = setTce( (u64)tbl->base, (u64)tcenum, tce.wholeTce );
#endif

	if ( setTceRc ) {
		PPCDBG(PPCDBG_TCE, "build_tce: HvCallXm_setTce failed, rc=%ld,  index=%ld,  tcenum=%0lx, tce=%016lx\n",
			(u64)tbl->index, (u64)tcenum, tce.wholeTce );
	}

}



/* 
 * Build a TceTable structure.  This contains a multi-level bit map which
 * is used to manage allocation of the tce space.
 */
struct TceTable * build_tce_table( struct TceTable * tbl )
{
	unsigned long bits, bytes, totalBytes;
	unsigned long numBits[NUM_TCE_LEVELS], numBytes[NUM_TCE_LEVELS];
	unsigned i, k, m;
	unsigned char * pos, * p, b;

	spin_lock_init( &(tbl->lock) );
	
	tbl->mlbm.maxLevel = 0;

	/* Compute number of bits and bytes for each level of the
	 * multi-level bit map
	 */ 
	totalBytes = 0;
	bits = tbl->size * (PAGE_SIZE / sizeof( union Tce ));
	
	for ( i=0; i<NUM_TCE_LEVELS; ++i ) {
		bytes = (bits+7)/8;
		PPCDBG(PPCDBG_TCE, "build_tce_table: level %d bits=%ld, bytes=%ld\n", i, bits, bytes );
		numBits[i] = bits;
		numBytes[i] = bytes;
		bits /= 2;
		totalBytes += bytes;
	}
	PPCDBG(PPCDBG_TCE, "build_tce_table: totalBytes=%ld\n", totalBytes );
	
	pos = (char *)__get_free_pages( GFP_ATOMIC, get_order( totalBytes )); 
	if ( !pos )
		return NULL;

	memset( pos, 0, totalBytes );
	
	/* For each level, fill in the pointer to the bit map,
	 * and turn on the last bit in the bit map (if the
	 * number of bits in the map is odd).  The highest
	 * level will get all of its bits turned on.
	 */
	
	for (i=0; i<NUM_TCE_LEVELS; ++i) {
		if ( numBytes[i] ) {
			tbl->mlbm.level[i].map = pos;
			tbl->mlbm.maxLevel = i;

			if ( numBits[i] & 1 ) {
				p = pos + numBytes[i] - 1;
				m = (( numBits[i] % 8) - 1) & 7;
				*p = 0x80 >> m;
				PPCDBG(PPCDBG_TCE, "build_tce_table: level %d last bit %x\n", i, 0x80>>m );
			}
		}
		else
			tbl->mlbm.level[i].map = 0;
		pos += numBytes[i];
		tbl->mlbm.level[i].numBits = numBits[i];
		tbl->mlbm.level[i].numBytes = numBytes[i];
		
	}

	/* For the highest level, turn on all the bits */
	
	i = tbl->mlbm.maxLevel;
	p = tbl->mlbm.level[i].map;
	m = numBits[i];
	PPCDBG(PPCDBG_TCE, "build_tce_table: highest level (%d) has all bits set\n", i);
	for (k=0; k<numBytes[i]; ++k) {
		if ( m >= 8 ) {
			/* handle full bytes */
			*p++ = 0xff;
			m -= 8;
		}
		else {
			/* handle the last partial byte */
			b = 0x80;
			*p = 0;
			while (m) {
				*p |= b;
				b >>= 1;
				--m;
			}
		}
	}


	return tbl;
	
}

static long alloc_tce_range( struct TceTable *tbl, unsigned order )
{
	long retval;
	unsigned long flags;
	
	/* Lock the tce allocation bitmap */
	spin_lock_irqsave( &(tbl->lock), flags );

	/* Do the actual work */
	retval = alloc_tce_range_nolock( tbl, order );
	
	/* Unlock the tce allocation bitmap */
	spin_unlock_irqrestore( &(tbl->lock), flags );

	return retval;
}



static long alloc_tce_range_nolock( struct TceTable *tbl, unsigned order )
{
	unsigned long numBits, numBytes;
	unsigned long i, bit, block, mask;
	long tcenum;
	unsigned char * map;

	/* If the order (power of 2 size) requested is larger than our
	 * biggest, indicate failure
	 */
	if ( order > tbl->mlbm.maxLevel ) {
		PPCDBG(PPCDBG_TCE, "alloc_tce_range_nolock: invalid order requested, order = %d\n", 
		       order );
		return -1;
	}
	
	numBits =  tbl->mlbm.level[order].numBits;
	numBytes = tbl->mlbm.level[order].numBytes;
	map =      tbl->mlbm.level[order].map;

	/* Initialize return value to -1 (failure) */
	tcenum = -1;

	/* Loop through the bytes of the bitmap */
	for (i=0; i<numBytes; ++i) {
		if ( *map ) {
			/* A free block is found, compute the block
			 * number (of this size)
			 */
			bit = count_leading_zeros32( *map ) - 24;
			block = (i * 8) + bit;
			/* turn off the bit in the map to indicate
			 * that the block is now in use
			 */
			mask = 0xff ^ (0x80 >> bit);
			*map &= mask;
			/* compute the index into our tce table for
			 * the first tce in the block
			 */
			PPCDBG(PPCDBG_TCE, "alloc_tce_range_nolock: allocating block %ld, (byte=%ld, bit=%ld) order %d\n", block, i, bit, order );
			tcenum = block << order;
			break;
		}
		++map;
	}

#ifdef DEBUG_TCE
	if ( tcenum == -1 ) {
		PPCDBG(PPCDBG_TCE, "alloc_tce_range_nolock: no available blocks of order = %d\n", order );
		if ( order < tbl->mlbm.maxLevel )
			PPCDBG(PPCDBG_TCE, "alloc_tce_range_nolock: trying next bigger size\n" );
		else
			PPCDBG(PPCDBG_TCE, "alloc_tce_range_nolock: maximum size reached...failing\n");
	}
#endif	
	
	/* If no block of the requested size was found, try the next
	 * size bigger.  If one of those is found, return the second
	 * half of the block to freespace and keep the first half
	 */
	if ( ( tcenum == -1 ) && ( order < tbl->mlbm.maxLevel  ) ) {
		tcenum = alloc_tce_range_nolock( tbl, order+1 );
		if ( tcenum != -1 ) {
			free_tce_range_nolock( tbl, tcenum+(1<<order), order );
		}
	}
	
	/* Return the index of the first tce in the block
	 * (or -1 if we failed)
	 */
	return tcenum;
	
}

static void free_tce_range( struct TceTable *tbl, long tcenum, unsigned order )
{
	unsigned long flags;

	/* Lock the tce allocation bitmap */
	spin_lock_irqsave( &(tbl->lock), flags );

	/* Do the actual work */
	free_tce_range_nolock( tbl, tcenum, order );
	
	/* Unlock the tce allocation bitmap */
	spin_unlock_irqrestore( &(tbl->lock), flags );

}

static void	free_tce_range_nolock( struct TceTable *tbl, long tcenum, unsigned order )
{
	unsigned long block;
	unsigned byte, bit, mask, b;
	unsigned char  * map, * bytep;
	
	if ( order > tbl->mlbm.maxLevel ) {
		PPCDBG(PPCDBG_TCE, "free_tce_range: order too large, order = %d, tcenum = %d\n", order, tcenum );
		return;
	}
	
	block = tcenum >> order;
	if ( tcenum != (block << order ) ) {
		PPCDBG(PPCDBG_TCE, "free_tce_range: tcenum %lx is not on appropriate boundary for order %x\n", tcenum, order );
		return;
	}
	if ( block >= tbl->mlbm.level[order].numBits ) {
		PPCDBG(PPCDBG_TCE, "free_tce_range: tcenum %lx is outside the range of this map (order %x, numBits %lx\n", tcenum, order, tbl->mlbm.level[order].numBits );
		return;
	}
#ifdef DEBUG_TCE	
	if ( test_tce_range( tbl, tcenum, order ) ) {
		PPCDBG(PPCDBG_TCE, "free_tce_range: freeing range not completely allocated.\n");
		PPCDBG(PPCDBG_TCE, "free_tce_range:   TceTable %p, tcenum %lx, order %x\n", tbl, tcenum, order );
	}
#endif
	map = tbl->mlbm.level[order].map;
	byte  = block / 8;
	bit   = block % 8;
	mask  = 0x80 >> bit;
	bytep = map + byte;
#ifdef DEBUG_TCE
	PPCDBG(PPCDBG_TCE, "free_tce_range_nolock: freeing block %ld (byte=%d, bit=%d) of order %d\n",block, byte, bit, order);
	if ( *bytep & mask )
		PPCDBG(PPCDBG_TCE, "free_tce_range: already free: TceTable %p, tcenum %lx, order %x\n", tbl, tcenum, order );
#endif	
	*bytep |= mask;

	/* If there is a higher level in the bit map than this we may be
	 * able to buddy up this block with its partner.
	 *   If this is the highest level we can't buddy up
	 *   If this level has an odd number of bits and
	 *      we are freeing the last block we can't buddy up
	 */
	if ( ( order < tbl->mlbm.maxLevel ) &&
	     ( ( 0 == ( tbl->mlbm.level[order].numBits & 1 ) ) ||
	       ( block < tbl->mlbm.level[order].numBits-1 ) ) ) {
	
		/* See if we can buddy up the block we just freed */
		bit  &= 6;		/* get to the first of the buddy bits */
		mask  = 0xc0 >> bit;	/* build two bit mask */
		b     = *bytep & mask;	/* Get the two bits */
		if ( 0 == (b ^ mask) ) { /* If both bits are on */
			/* both of the buddy blocks are free we can combine them */
			*bytep ^= mask;	/* turn off the two bits */
			block = ( byte * 8 ) + bit; /* block of first of buddies */
			tcenum = block << order;
			/* free the buddied block */
			PPCDBG(PPCDBG_TCE, "free_tce_range: buddying up block %ld and block %ld\n", block, block+1);
			free_tce_range_nolock( tbl, tcenum, order+1 ); 
		}	
	}
}

static long	test_tce_range( struct TceTable *tbl, long tcenum, unsigned order )
{
	unsigned long block;
	unsigned byte, bit, mask, b;
	long	retval, retLeft, retRight;
	unsigned char  * map;
	
	map = tbl->mlbm.level[order].map;
	block = tcenum >> order;
	byte = block / 8;		/* Byte within bitmap */
	bit  = block % 8;		/* Bit within byte */
	mask = 0x80 >> bit;		
	b    = (*(map+byte) & mask );	/* 0 if block is allocated, else free */
	if ( b ) 
		retval = 1;		/* 1 == block is free */
	else
		retval = 0;		/* 0 == block is allocated */
	/* Test bits at all levels below this to ensure that all agree */

	if (order) {
		retLeft  = test_tce_range( tbl, tcenum, order-1 );
		retRight = test_tce_range( tbl, tcenum+(1<<(order-1)), order-1 );
		if ( retLeft || retRight ) {
			retval = 2;		
		}
	}

	/* Test bits at all levels above this to ensure that all agree */
	
	return retval;
}

static dma_addr_t get_tces( struct TceTable *tbl, unsigned order, void *page, unsigned numPages, int tceType, int direction )
{
	long tcenum;
	unsigned long uaddr;
	unsigned i;
	dma_addr_t retTce = NO_TCE;

	uaddr = (unsigned long)page & PAGE_MASK;
	
	/* Allocate a range of tces */
	tcenum = alloc_tce_range( tbl, order );
	if ( tcenum != -1 ) {
		/* We got the tces we wanted */
		tcenum += tbl->startOffset;	/* Offset into real TCE table */
		retTce = tcenum << PAGE_SHIFT;	/* Set the return dma address */
		/* Setup a tce for each page */
		for (i=0; i<numPages; ++i) {
			build_tce( tbl, tcenum, uaddr, tceType, direction );
			++tcenum;
			uaddr += PAGE_SIZE;
		}
	}
	else
		PPCDBG(PPCDBG_TCE, "alloc_tce_range failed\n");
	return retTce; 
}

static void free_tces( struct TceTable *tbl, dma_addr_t dma_addr, unsigned order, unsigned numPages )
{
	u64 setTceRc;
	long tcenum, freeTce, maxTcenum;
	unsigned i;
	union Tce tce;

	maxTcenum = (tbl->size * (PAGE_SIZE / sizeof(union Tce))) - 1;
	
	tcenum = dma_addr >> PAGE_SHIFT;
	tcenum -= tbl->startOffset;

	if ( tcenum > maxTcenum ) {
		PPCDBG(PPCDBG_TCE, "free_tces: tcenum > maxTcenum, tcenum = %ld, maxTcenum = %ld\n",
				tcenum, maxTcenum );
		PPCDBG(PPCDBG_TCE, "free_tces:    TCE Table at %16lx\n", (unsigned long)tbl );
		PPCDBG(PPCDBG_TCE, "free_tces:      bus#     %lu\n", (unsigned long)tbl->busNumber );
		PPCDBG(PPCDBG_TCE, "free_tces:      size     %lu\n", (unsigned long)tbl->size );
		PPCDBG(PPCDBG_TCE, "free_tces:      startOff %lu\n", (unsigned long)tbl->startOffset );
		PPCDBG(PPCDBG_TCE, "free_tces:      index    %lu\n", (unsigned long)tbl->index );
		return;
	}
	
	freeTce = tcenum;

	for (i=0; i<numPages; ++i) {
		tce.wholeTce = 0;
#ifdef CONFIG_PPC_ISERIES
		setTceRc = HvCallXm_setTce( (u64)tbl->index, (u64)tcenum, tce.wholeTce );
#else
		setTceRc = setTce( (u64)tbl->base, (u64)tcenum, tce.wholeTce );
#endif

		if ( setTceRc ) {
			PPCDBG(PPCDBG_TCE, "free_tces: HvCallXm_setTce failed, rc=%ld,  index=%ld,  tcenum=%0lx, tce=%016lx\n",
				(u64)tbl->index, (u64)tcenum, tce.wholeTce );
		}

		++tcenum;
	}

	free_tce_range( tbl, freeTce, order );

}

void __init create_virtual_bus_tce_table(void)
{
	struct TceTable *t;
	struct TceTableManagerCB virtBusTceTableParms;
	u64 absParmsPtr;

	virtBusTceTableParms.busNumber = 255;	/* Bus 255 is the virtual bus */
	virtBusTceTableParms.virtualBusFlag = 0xff; /* Ask for virtual bus */
	
	absParmsPtr = virt_to_absolute( (u64)&virtBusTceTableParms );
	HvCallXm_getTceTableParms( absParmsPtr );
	
	virtBusTceTable.size = virtBusTceTableParms.size;
	virtBusTceTable.busNumber = virtBusTceTableParms.busNumber;
	virtBusTceTable.startOffset = virtBusTceTableParms.startOffset;
	virtBusTceTable.index = virtBusTceTableParms.index;

	t = build_tce_table( &virtBusTceTable );
	if ( t ) {
		tceTables[255] = t;
		PPCDBG(PPCDBG_TCE, "Virtual Bus TCE table built successfully.\n");
		PPCDBG(PPCDBG_TCE, "  TCE table size = %ld entries\n", 
				(unsigned long)t->size*(PAGE_SIZE/sizeof(union Tce)) );
		PPCDBG(PPCDBG_TCE, "  TCE table token = %d\n",
				(unsigned)t->index );
		PPCDBG(PPCDBG_TCE, "  TCE table start entry = 0x%lx\n",
				(unsigned long)t->startOffset );
	}
	else 
		PPCDBG(PPCDBG_TCE, "Virtual Bus TCE table failed.\n");
}

void create_pci_bus_tce_table( unsigned long busNumber,
			       unsigned long token )
{
	struct TceTable * t;
	struct TceTable * newTceTable;
	struct TceTableManagerCB pciBusTceTableParms;
#ifdef CONFIG_PPC_ISERIES
	u64 parmsPtr;
#endif
	struct pci_controller *phb = (struct pci_controller *)token;

	PPCDBG(PPCDBG_TCE, "Entering create_pci_bus_tce_table.\n");
	PPCDBG(PPCDBG_TCE, "\tbusNumber = 0x%lx, token = 0x%lx.\n", 
	       busNumber, token);
	if ( busNumber > 254 ) {
		PPCDBG(PPCDBG_TCE, "PCI Bus TCE table failed.\n");
		PPCDBG(PPCDBG_TCE, "  Invalid bus number %u\n", busNumber );
		return;
	}

	newTceTable = kmalloc( sizeof(struct TceTable), GFP_KERNEL );
	PPCDBG(PPCDBG_TCE, "\tnewTceTable = 0x%lx\n", newTceTable);
	
	pciBusTceTableParms.busNumber = busNumber;
	pciBusTceTableParms.virtualBusFlag = 0; 
	
#ifdef CONFIG_PPC_ISERIES
	parmsPtr = virt_to_absolute( (u64)&pciBusTceTableParms );

	/* 
	 * Call HV with the architected data structure to get TCE table info.
	 * Put the returned data into the Linux representation of the TCE
	 * table data.
	 */
	HvCallXm_getTceTableParms( parmsPtr );
	newTceTable->size = pciBusTceTableParms.size;
	newTceTable->busNumber = pciBusTceTableParms.busNumber;
	newTceTable->startOffset = pciBusTceTableParms.startOffset;
	newTceTable->index = pciBusTceTableParms.index;
#else
	getTceTableParms( phb, newTceTable );
#endif
	
	t = build_tce_table( newTceTable );
	if ( t ) {
		tceTables[busNumber] = t;
		PPCDBG(PPCDBG_TCE, "PCI Bus TCE table built successfully.\n");
		PPCDBG(PPCDBG_TCE, "  TCE table size = %ld entries\n", 
				(unsigned long)t->size*(PAGE_SIZE/sizeof(union Tce)) );
		PPCDBG(PPCDBG_TCE, "  TCE table token = %d\n",
				(unsigned)t->index );
		PPCDBG(PPCDBG_TCE, "  TCE table start entry = 0x%lx\n",
				(unsigned long)t->startOffset );
	}
	else {
		kfree( newTceTable );
		PPCDBG(PPCDBG_TCE, "PCI Bus TCE table failed.\n");
		return;
	}

#ifndef CONFIG_PPC_ISERIES
	/* Do not allow DMA's to the 1st 16MB of PCI space in order 
	 * to account for the I/O hole and aliases.
	 *
	 * DRENG: this only really needs to be done on PHB0 & should be
	 * validated against dma-ranges.  This is all somewhat compilcated
	 * by some OF oddities: on a 260, there is no dma-ranges, but there
	 * is a io-hole.  On an F80, there are dma-ranges, but no I/O hole.
	 * Just seems easier to simply map out the first 16MB in all cases.
	 *
	 * The RTAS case here ends up being redundant as it falls into
	 * the 16MB already mapped out.
	 */
	resv_tce_range_top_level(newTceTable, 0, 4096); 
	resv_tce_range_top_level(newTceTable, rtas.base, 
				 (rtas.size) >> PAGE_SHIFT); 

#if 1
	/* Initialize the table to have a one-to-one mapping over RTAS */

	/* DRENG strictly speaking, the RPA says this should be done.
	 * I do not see how it can really be a valid thing to require however.
	 */
	{
		unsigned long tce_entry, *tce_entryp, i;

		tce_entryp = (unsigned long *)newTceTable->base;
		tce_entryp += (rtas.base >> PAGE_SHIFT);
		tce_entry  = (rtas.base & PAGE_MASK) | 0x3;

		for(i = 0; i < (rtas.size >> PAGE_SHIFT); i++) {
			*tce_entryp = tce_entry;

			tce_entryp++; 
			tce_entry += (1UL << PAGE_SHIFT); 
		}
	}
#endif
#endif
}

static void getTceTableParms( struct pci_controller *phb,
			      struct TceTable *newTceTable ) {
#if 0
	struct pci_dev *dev;
	struct device_node *np;
#endif
	phandle node;
	unsigned long i;

	node = ((struct device_node *)(phb->arch_data))->node;

	PPCDBG(PPCDBG_TCE, "getTceTableParms: start\n"); 
	PPCDBG(PPCDBG_TCE, "\tof_tce_table = 0x%lx\n", of_tce_table); 
	PPCDBG(PPCDBG_TCE, "\tphb          = 0x%lx\n", phb); 
	PPCDBG(PPCDBG_TCE, "\tnewTceTable  = 0x%lx\n", newTceTable); 

	i = 0;
	while(of_tce_table[i].node) {
		PPCDBG(PPCDBG_TCE, "\ttce_table[%d].node = 0x%lx\n", 
		       i, of_tce_table[i].node);
		PPCDBG(PPCDBG_TCE, "\ttce_table[%d].base = 0x%lx\n", 
			    i, of_tce_table[i].base);
		PPCDBG(PPCDBG_TCE, "\ttce_table[%d].size = 0x%lx\n", 
			    i, of_tce_table[i].size >> PAGE_SHIFT);
		PPCDBG(PPCDBG_TCE, "\tphb->arch_data->node = 0x%lx\n", node);

		if(of_tce_table[i].node == node) {
			memset((void *)of_tce_table[i].base, 
			       0, of_tce_table[i].size);
			newTceTable->busNumber = phb->bus->number;
			newTceTable->size = 
				(of_tce_table[i].size) >> PAGE_SHIFT; 
			newTceTable->startOffset = 0;
			newTceTable->base = 
				of_tce_table[i].base;
			newTceTable->index = 0;
		}
		i++;
	}

#if 0
	pci_for_each_dev(dev) {
		np = pci_device_to_OF_node(dev);
		node = np->node;
		bus_number = dev->bus->number;
		create_pci_bus_tce_table( unsigned long bus_number );
		node = hose->arch_data->node;
	}
#endif
}

/* Allocates a contiguous real buffer and creates TCEs over it.
 * Returns the virtual address of the buffer and sets dma_handle
 * to the dma address (tce) of the first page.
 */
void *pci_alloc_consistent(struct pci_dev *hwdev, size_t size,
			   dma_addr_t *dma_handle)
{
	struct TceTable * tbl;
	void *ret = NULL;
	unsigned order, nPages, bus;
	dma_addr_t tce;
	int tceType;

	PPCDBG(PPCDBG_TCE, "pci_alloc_consistent:\n");
	PPCDBG(PPCDBG_TCE, "\thwdev      = 0x%16.16lx\n", hwdev);
	PPCDBG(PPCDBG_TCE, "\tsize       = 0x%16.16lx\n", size);
	PPCDBG(PPCDBG_TCE, "\tdma_handle = 0x%16.16lx\n", dma_handle);	

	size = PAGE_ALIGN(size);
	order = get_order(size);
	nPages = 1 << order;

	/* If no pci_dev then use virtual bus */
	if (hwdev == NULL ) {
		bus = 255;
		tceType = TCE_VB;
	}
	else {
#ifdef CONFIG_PCI        
		/* Get the iSeries bus # to use as an index
		 * into the TCE table array
		 */
		bus = get_tce_table_index( hwdev );
		// bus = ISERIES_GET_BUS( hwdev );
		tceType = TCE_PCI;
#else
                BUG();
                return NULL;
#endif /* CONFIG_PCI */
	}
	
	tbl = tceTables[bus];
	if ( tbl ) {
		/* Alloc enough pages (and possibly more) */
		ret = (void *)__get_free_pages( GFP_ATOMIC, order );
		if ( ret ) {
			/* Page allocation succeeded */
			memset(ret, 0, nPages << PAGE_SHIFT);
			/* Set up tces to cover the allocated range */
			tce = get_tces( tbl, order, ret, nPages, tceType,
					PCI_DMA_BIDIRECTIONAL );
			if ( tce == NO_TCE ) {
				PPCDBG(PPCDBG_TCE, "pci_alloc_consistent: get_tces failed\n" );
				free_pages( (unsigned long)ret, order );
				ret = NULL;
			}
			else
			{
				*dma_handle = tce;
			}
		}
		else
			PPCDBG(PPCDBG_TCE, "pci_alloc_consistent: __get_free_pages failed for order = %d\n", order);
	}

	PPCDBG(PPCDBG_TCE, "\tpci_alloc_consistent: dma_handle = 0x%16.16lx\n", *dma_handle);	
	PPCDBG(PPCDBG_TCE, "\tpci_alloc_consistent: return     = 0x%16.16lx\n", ret);	
	return ret;
}

void pci_free_consistent(struct pci_dev *hwdev, size_t size,
			 void *vaddr, dma_addr_t dma_handle)
{
	struct TceTable * tbl;
	unsigned order, nPages, bus;
	
	PPCDBG(PPCDBG_TCE, "pci_free_consistent:\n");
	PPCDBG(PPCDBG_TCE, "\thwdev = 0x%16.16lx, size = 0x%16.16lx, dma_handle = 0x%16.16lx, vaddr = 0x%16.16lx\n", hwdev, size, dma_handle, vaddr);	

	size = PAGE_ALIGN(size);
	order = get_order(size);
	nPages = 1 << order;

	if ( order > 10 )
		PPCDBG(PPCDBG_TCE, "pci_free_consistent: order=%d, size=%d, nPages=%d, dma_handle=%016lx, vaddr=%016lx\n",
			order, size, nPages, (unsigned long)dma_handle, (unsigned long)vaddr );
	
	/* If no pci_dev then use virtual bus */
	if (hwdev == NULL )
		bus = 255;
	else {
#ifdef CONFIG_PCI        
		/* Get the iSeries bus # to use as an index
		 * into the TCE table array
		 */
		bus = get_tce_table_index( hwdev );
		// bus = ISERIES_GET_BUS( hwdev );
#else
                BUG();
                return;
#endif /* CONFIG_PCI */
	}

	if ( bus > 255 ) {
		PPCDBG(PPCDBG_TCE, "pci_free_consistent: invalid bus # %d\n", bus );
		PPCDBG(PPCDBG_TCE, "pci_free_consistent:   hwdev = %08lx\n", (unsigned long)hwdev );
	}
	
	tbl = tceTables[bus];

	if ( tbl ) {
		free_tces( tbl, dma_handle, order, nPages );
		free_pages( (unsigned long)vaddr, order );
	}
}

/* Creates TCEs for a user provided buffer.  The user buffer must be 
 * contiguous real kernel storage (not vmalloc).  The address of the buffer
 * passed here is the kernel (virtual) address of the buffer.  The buffer
 * need not be page aligned, the dma_addr_t returned will point to the same
 * byte within the page as vaddr.
 */
dma_addr_t pci_map_single( struct pci_dev *hwdev, void *vaddr, size_t size, int direction )
{
	struct TceTable * tbl;
	dma_addr_t dma_handle;
	unsigned long uaddr;
	unsigned order, nPages, bus;
	int tceType;

	PPCDBG(PPCDBG_TCE, "pci_map_single:\n");
	PPCDBG(PPCDBG_TCE, "\thwdev = 0x%16.16lx, size = 0x%16.16lx, direction = 0x%16.16lx, vaddr = 0x%16.16lx\n", hwdev, size, direction, vaddr);	
	if ( direction == PCI_DMA_NONE )
		BUG();
	
	dma_handle = NO_TCE;
	
	uaddr = (unsigned long)vaddr;
	nPages = PAGE_ALIGN( uaddr + size ) - ( uaddr & PAGE_MASK );
	order = get_order( nPages & PAGE_MASK );
	nPages >>= PAGE_SHIFT;
	
	/* If no pci_dev then use virtual bus */
	if (hwdev == NULL ) {
		bus = 255;
		tceType = TCE_VB;
	}
	else {
#ifdef CONFIG_PCI        
		/* Get the iSeries bus # to use as an index
		 * into the TCE table array
	 	 */
		bus = get_tce_table_index( hwdev );
		// bus = ISERIES_GET_BUS( hwdev );
		tceType = TCE_PCI;
#else
                BUG();
                return NO_TCE;
#endif /* CONFIG_PCI */

	}
	
	tbl = tceTables[bus];

	if ( tbl ) {
		dma_handle = get_tces( tbl, order, vaddr, nPages, tceType, 
					direction );
		dma_handle |= ( uaddr & ~PAGE_MASK );
	}

	return dma_handle;
}

void pci_unmap_single( struct pci_dev *hwdev, dma_addr_t dma_handle, size_t size, int direction )
{
	struct TceTable * tbl;
	unsigned order, nPages, bus;
	
	PPCDBG(PPCDBG_TCE, "pci_unmap_single:\n");
	PPCDBG(PPCDBG_TCE, "\thwdev = 0x%16.16lx, size = 0x%16.16lx, direction = 0x%16.16lx, dma_handle = 0x%16.16lx\n", hwdev, size, direction, dma_handle);	
	if ( direction == PCI_DMA_NONE )
		BUG();

	nPages = PAGE_ALIGN( dma_handle + size ) - ( dma_handle & PAGE_MASK );
	order = get_order( nPages & PAGE_MASK );
	nPages >>= PAGE_SHIFT;

	if ( order > 10 )
		PPCDBG(PPCDBG_TCE, "pci_unmap_single: order=%d, size=%d, nPages=%d, dma_handle=%016lx\n",
			order, size, nPages, (unsigned long)dma_handle );
	
	/* If no pci_dev then use virtual bus */
	if (hwdev == NULL )
		bus = 255;
	else {
#ifdef CONFIG_PCI        
		/* Get the iSeries bus # to use as an index
		 * into the TCE table array
	 	 */
		bus = get_tce_table_index( hwdev );
		// bus = ISERIES_GET_BUS( hwdev );
#else
                BUG();
                return;
#endif /* CONFIG_PCI */
	}
	
	if ( bus > 255 ) {
		PPCDBG(PPCDBG_TCE, "pci_unmap_single: invalid bus # %d\n", bus );
		PPCDBG(PPCDBG_TCE, "pci_unmap_single:   hwdev = %08lx\n", (unsigned long)hwdev );
	}

	tbl = tceTables[bus];

	if ( tbl ) 
		free_tces( tbl, dma_handle, order, nPages );

}

/* Figure out how many TCEs are actually going to be required
 * to map this scatterlist.  This code is not optimal.  It 
 * takes into account the case where entry n ends in the same
 * page in which entry n+1 starts.  It does not handle the 
 * general case of entry n ending in the same page in which 
 * entry m starts.   
 */
static unsigned long num_tces_sg( struct scatterlist *sg, int nents )
{
	unsigned long nTces, numPages, startPage, endPage, prevEndPage;
	unsigned i;

	prevEndPage = 0;
	nTces = 0;

	for (i=0; i<nents; ++i) {
		/* Compute the starting page number and
		 * the ending page number for this entry
		 */
		startPage = (unsigned long)sg->address >> PAGE_SHIFT;
		endPage = ((unsigned long)sg->address + sg->length - 1) >> PAGE_SHIFT;
		numPages = endPage - startPage + 1;
		/* Simple optimization: if the previous entry ended
		 * in the same page in which this entry starts
		 * then we can reduce the required pages by one.
		 * This matches assumptions in fill_scatterlist_sg and
		 * create_tces_sg
		 */
		if ( startPage == prevEndPage )
			--numPages;
		nTces += numPages;
		prevEndPage = endPage;
		sg++;
	}
	return nTces;
}

/* Fill in the dma data in the scatterlist
 * return the number of dma sg entries created
 */
static unsigned fill_scatterlist_sg( struct scatterlist *sg, int nents, 
				 dma_addr_t dma_addr , unsigned long numTces)
{
	struct scatterlist *dma_sg;
	u32 cur_start_dma;
	unsigned long cur_len_dma, cur_end_virt, uaddr;
	unsigned num_dma_ents;

	dma_sg = sg;
	num_dma_ents = 1;

	/* Process the first sg entry */
	cur_start_dma = dma_addr + ((unsigned long)sg->address & (~PAGE_MASK));
	cur_len_dma = sg->length;
	/* cur_end_virt holds the address of the byte immediately after the
	 * end of the current buffer.
	 */
	cur_end_virt = (unsigned long)sg->address + cur_len_dma;
	/* Later code assumes that unused sg->dma_address and sg->dma_length
	 * fields will be zero.  Other archs seem to assume that the user
	 * (device driver) guarantees that...I don't want to depend on that
	 */
	sg->dma_address = sg->dma_length = 0;
	
	/* Process the rest of the sg entries */
	while (--nents) {
		++sg;
		/* Clear possibly unused fields. Note: sg >= dma_sg so
		 * this can't be clearing a field we've already set
		 */
		sg->dma_address = sg->dma_length = 0;

		/* Check if it is possible to make this next entry
		 * contiguous (in dma space) with the previous entry.
		 */
		
		/* The entries can be contiguous in dma space if
		 * the previous entry ends immediately before the
		 * start of the current entry (in virtual space)
		 * or if the previous entry ends at a page boundary
		 * and the current entry starts at a page boundary.
		 */
		uaddr = (unsigned long)sg->address;
		if ( ( uaddr != cur_end_virt ) &&
		     ( ( ( uaddr | cur_end_virt ) & (~PAGE_MASK) ) ||
		       ( ( uaddr & PAGE_MASK ) == ( ( cur_end_virt-1 ) & PAGE_MASK ) ) ) ) {
			/* This entry can not be contiguous in dma space.
			 * save the previous dma entry and start a new one
			 */
			dma_sg->dma_address = cur_start_dma;
			dma_sg->dma_length  = cur_len_dma;

			++dma_sg;
			++num_dma_ents;
			
			cur_start_dma += cur_len_dma-1;
			/* If the previous entry ends and this entry starts
			 * in the same page then they share a tce.  In that
			 * case don't bump cur_start_dma to the next page 
			 * in dma space.  This matches assumptions made in
			 * num_tces_sg and create_tces_sg.
			 */
			if ((uaddr & PAGE_MASK) == ((cur_end_virt-1) & PAGE_MASK))
				cur_start_dma &= PAGE_MASK;
			else
				cur_start_dma = PAGE_ALIGN(cur_start_dma+1);
			cur_start_dma += ( uaddr & (~PAGE_MASK) );
			cur_len_dma = 0;
		}
		/* Accumulate the length of this entry for the next 
		 * dma entry
		 */
		cur_len_dma += sg->length;
		cur_end_virt = uaddr + sg->length;
	}
	/* Fill in the last dma entry */
	dma_sg->dma_address = cur_start_dma;
	dma_sg->dma_length  = cur_len_dma;

	if ((((cur_start_dma +cur_len_dma - 1)>> PAGE_SHIFT) - (dma_addr >> PAGE_SHIFT) + 1) != numTces)
	  {
	    PPCDBG(PPCDBG_TCE, "fill_scatterlist_sg: numTces %ld, used tces %d\n",
		   numTces,
		   (unsigned)(((cur_start_dma + cur_len_dma - 1) >> PAGE_SHIFT) - (dma_addr >> PAGE_SHIFT) + 1));
	  }
	

	return num_dma_ents;
}

/* Call the hypervisor to create the TCE entries.
 * return the number of TCEs created
 */
static dma_addr_t create_tces_sg( struct TceTable *tbl, struct scatterlist *sg, 
		   int nents, unsigned numTces, int tceType, int direction )
{
	unsigned order, i, j;
	unsigned long startPage, endPage, prevEndPage, numPages, uaddr;
	long tcenum, starttcenum;
	dma_addr_t dmaAddr;

	dmaAddr = NO_TCE;

	order = get_order( numTces << PAGE_SHIFT );
	/* allocate a block of tces */
	tcenum = alloc_tce_range( tbl, order );
	if ( tcenum != -1 ) {
		tcenum += tbl->startOffset;
		starttcenum = tcenum;
		dmaAddr = tcenum << PAGE_SHIFT;
		prevEndPage = 0;
		for (j=0; j<nents; ++j) {
			startPage = (unsigned long)sg->address >> PAGE_SHIFT;
			endPage = ((unsigned long)sg->address + sg->length - 1) >> PAGE_SHIFT;
			numPages = endPage - startPage + 1;
			
			uaddr = (unsigned long)sg->address;

			/* If the previous entry ended in the same page that
			 * the current page starts then they share that
			 * tce and we reduce the number of tces we need
			 * by one.  This matches assumptions made in
			 * num_tces_sg and fill_scatterlist_sg
			 */
			if ( startPage == prevEndPage ) {
				--numPages;
				uaddr += PAGE_SIZE;
			}
			
			for (i=0; i<numPages; ++i) {
			  build_tce( tbl, tcenum, uaddr, tceType,
				     direction );
			  ++tcenum;
			  uaddr += PAGE_SIZE;
			}
			
			prevEndPage = endPage;
			sg++;
		}
		if ((tcenum - starttcenum) != numTces)
	    		PPCDBG(PPCDBG_TCE, "create_tces_sg: numTces %d, tces used %d\n",
		   		numTces, (unsigned)(tcenum - starttcenum));

	}

	return dmaAddr;
}

int pci_map_sg( struct pci_dev *hwdev, struct scatterlist *sg, int nents, int direction )
{
	struct TceTable * tbl;
	unsigned bus, numTces;
	int tceType, num_dma;
	dma_addr_t dma_handle;

	PPCDBG(PPCDBG_TCE, "pci_map_sg:\n");
	PPCDBG(PPCDBG_TCE, "\thwdev = 0x%16.16lx, sg = 0x%16.16lx, direction = 0x%16.16lx, nents = 0x%16.16lx\n", hwdev, sg, direction, nents);	
	/* Fast path for a single entry scatterlist */
	if ( nents == 1 ) {
		sg->dma_address = pci_map_single( hwdev, sg->address, 
					sg->length, direction );
		sg->dma_length = sg->length;
		return 1;
	}
	
	if ( direction == PCI_DMA_NONE )
		BUG();
	
	/* If no pci_dev then use virtual bus */
	if (hwdev == NULL ) {
		bus = 255;
		tceType = TCE_VB;
	}
	else {
#ifdef CONFIG_PCI        
		/* Get the iSeries bus # to use as an index
		 * into the TCE table array
		 */
		bus = get_tce_table_index( hwdev );
		// bus = ISERIES_GET_BUS( hwdev );
		tceType = TCE_PCI;
#else
                BUG();
                return 0;
#endif /* CONFIG_PCI */
	}
	
	tbl = tceTables[bus];

	if ( tbl ) {
		/* Compute the number of tces required */
		numTces = num_tces_sg( sg, nents );
		/* Create the tces and get the dma address */ 
		dma_handle = create_tces_sg( tbl, sg, nents, numTces,
				             tceType, direction );

		/* Fill in the dma scatterlist */
		num_dma = fill_scatterlist_sg( sg, nents, dma_handle, numTces );
	}

	return num_dma;
}

void pci_unmap_sg( struct pci_dev *hwdev, struct scatterlist *sg, int nelms, int direction )
{
	struct TceTable * tbl;
	unsigned order, numTces, bus, i;
	dma_addr_t dma_end_page, dma_start_page;
	
	PPCDBG(PPCDBG_TCE, "pci_unmap_sg:\n");
	PPCDBG(PPCDBG_TCE, "\thwdev = 0x%16.16lx, sg = 0x%16.16lx, direction = 0x%16.16lx, nelms = 0x%16.16lx\n", hwdev, sg, direction, nelms);	

	if ( direction == PCI_DMA_NONE )
		BUG();

	dma_start_page = sg->dma_address & PAGE_MASK;
	for ( i=nelms; i>0; --i ) {
		unsigned k = i - 1;
		if ( sg[k].dma_length ) {
			dma_end_page = ( sg[k].dma_address +
					 sg[k].dma_length - 1 ) & PAGE_MASK;
			break;
		}
	}

	numTces = ((dma_end_page - dma_start_page ) >> PAGE_SHIFT) + 1;
	order = get_order( numTces << PAGE_SHIFT );

	if ( order > 10 )
		PPCDBG(PPCDBG_TCE, "pci_unmap_sg: order=%d, numTces=%d, nelms=%d, dma_start_page=%016lx, dma_end_page=%016lx\n",
			order, numTces, nelms, (unsigned long)dma_start_page, (unsigned long)dma_end_page );
	

	/* If no pci_dev then use virtual bus */
	if (hwdev == NULL )
		bus = 255;
	else {
#ifdef CONFIG_PCI        
		/* Get the iSeries bus # to use as an index
		 * into the TCE table array
		 */
		bus = get_tce_table_index( hwdev );
		// bus = ISERIES_GET_BUS( hwdev );
#else
                BUG();
                return;
#endif /* CONFIG_PCI */
	}
		
	if ( bus > 255 ) {
		PPCDBG(PPCDBG_TCE, "pci_unmap_sg: invalid bus # %d\n", bus );
		PPCDBG(PPCDBG_TCE, "pci_unmap_sg:   hwdev = %08lx\n", (unsigned long)hwdev );
	}


	tbl = tceTables[bus];

	if ( tbl ) 
		free_tces( tbl, dma_start_page, order, numTces );

}

static unsigned long setTce( unsigned long base, 
			     unsigned long tce_num,
			     unsigned long tce_data) {
	union Tce *tce_addr;

	PPCDBG(PPCDBG_TCE, "setTce:\n");
	PPCDBG(PPCDBG_TCE, "\tbase     = 0x%lx\n", base);
	PPCDBG(PPCDBG_TCE, "\ttce_num  = 0x%lx\n", tce_num);
	PPCDBG(PPCDBG_TCE, "\ttce_data = 0x%lx\n", tce_data);

	tce_addr = ((union Tce *)base) + tce_num;

	PPCDBG(PPCDBG_TCE, "\ttce_addr = 0x%lx\n", tce_addr);

	*tce_addr = (union Tce)tce_data;

	/* Make sure the update is visible to hardware. */
	__asm__ __volatile__ ("sync" : : : "memory");

	return(0);
}

unsigned long phb_tce_table_init(struct pci_controller *phb) {
	unsigned int r, cfg_rw, i;	
	unsigned long r64;	
	phandle node;

	PPCDBG(PPCDBG_TCE, "phb_tce_table_init: start.\n"); 

	node = ((struct device_node *)(phb->arch_data))->node;

	PPCDBG(PPCDBG_TCEINIT, "\tphb            = 0x%lx\n", phb); 
	PPCDBG(PPCDBG_TCEINIT, "\tphb->type      = 0x%lx\n", phb->type); 
	PPCDBG(PPCDBG_TCEINIT, "\tphb->phb_regs  = 0x%lx\n", phb->phb_regs); 
	PPCDBG(PPCDBG_TCEINIT, "\tphb->chip_regs = 0x%lx\n", phb->chip_regs); 
	PPCDBG(PPCDBG_TCEINIT, "\tphb: node      = 0x%lx\n", node);
	PPCDBG(PPCDBG_TCEINIT, "\tphb->arch_data = 0x%lx\n", phb->arch_data); 

	i = 0;
	while(of_tce_table[i].node) {
		if(of_tce_table[i].node == node) {
			if(phb->type == phb_type_python) {
				r = *(((unsigned int *)phb->phb_regs) + (0xf10>>2)); 
				PPCDBG(PPCDBG_TCEINIT, "\tTAR(low)    = 0x%x\n", r);
				r = *(((unsigned int *)phb->phb_regs) + (0xf00>>2)); 
				PPCDBG(PPCDBG_TCEINIT, "\tTAR(high)   = 0x%x\n", r);
				r = *(((unsigned int *)phb->phb_regs) + (0xfd0>>2)); 
				PPCDBG(PPCDBG_TCEINIT, "\tPHB cfg(rw) = 0x%x\n", r);
				break;
			} else if(phb->type == phb_type_speedwagon) {
				r64 = *(((unsigned long *)phb->chip_regs) + 
					(0x800>>3)); 
				PPCDBG(PPCDBG_TCEINIT, "\tNCFG    = 0x%lx\n", r64);
				r64 = *(((unsigned long *)phb->chip_regs) + 
					(0x580>>3)); 
				PPCDBG(PPCDBG_TCEINIT, "\tTAR0    = 0x%lx\n", r64);
				r64 = *(((unsigned long *)phb->chip_regs) + 
					(0x588>>3)); 
				PPCDBG(PPCDBG_TCEINIT, "\tTAR1    = 0x%lx\n", r64);
				r64 = *(((unsigned long *)phb->chip_regs) + 
					(0x590>>3)); 
				PPCDBG(PPCDBG_TCEINIT, "\tTAR2    = 0x%lx\n", r64);
				r64 = *(((unsigned long *)phb->chip_regs) + 
					(0x598>>3)); 
				PPCDBG(PPCDBG_TCEINIT, "\tTAR3    = 0x%lx\n", r64);
				cfg_rw = *(((unsigned int *)phb->chip_regs) + 
					   ((0x160 +
					     (((phb->local_number)+8)<<12))>>2)); 
				PPCDBG(PPCDBG_TCEINIT, "\tcfg_rw = 0x%x\n", cfg_rw);
			}
		}
		i++;
	}

	PPCDBG(PPCDBG_TCEINIT, "phb_tce_table_init: done\n"); 
	// create_pci_bus_tce_table(phb->number, (unsigned long)phb); 
}

/*
 * Reserve a range of TCE table entries.
 *
 * Inputs: 
 *   tbl       TCE Table.
 *   dma_addr  PCI Base Address, must be aligned to 2^(NUM_TCE_LEVELS-1).
 *   size      Size to reserve in pages.  Value is rounded up to a multiple
 *             of (2^(NUM_TCE_LEVELS-1)); ie the min size of the allocation
 *             is 2^9 * 4096 = 2MB.
 *
 * This should only be called after the table has been initialized, but
 * before it has been used.
 */
static long resv_tce_range_top_level( struct TceTable *tbl, 
				      unsigned long dma_addr, 
				      unsigned size )
{
	unsigned long i, segments, block;
	long tcenum, maxTcenum;
	unsigned byte, bit, mask;
	unsigned char  *map, *bytep;

	PPCDBG(PPCDBG_TCEINIT, "resv_tce_range_top: start\n"); 
	PPCDBG(PPCDBG_TCEINIT, "\tdma_addr=0x%lx, size=0x%lx\n",
	       dma_addr, size);
	maxTcenum = (tbl->size * (PAGE_SIZE / sizeof(union Tce))) - 1;
	tcenum = (dma_addr >> PAGE_SHIFT) & 
		(~((1UL << (NUM_TCE_LEVELS - 1)) - 1));
	tcenum -= tbl->startOffset;

	PPCDBG(PPCDBG_TCEINIT, "\ttcenum=0x%lx, maxTcenum=0x%lx\n",
	       tcenum, maxTcenum);

	if ( tcenum > maxTcenum ) {
		return(-1);
	}

	segments = (size + (1UL << (NUM_TCE_LEVELS - 1)) - 1) >> 
		(NUM_TCE_LEVELS - 1);
	map = tbl->mlbm.level[NUM_TCE_LEVELS-1].map;
	block = tcenum >> (NUM_TCE_LEVELS -1);

	PPCDBG(PPCDBG_TCEINIT, "\tsegments = 0x%lx, map = 0x%lx\n", 
	       segments, map);

	for(i = 0; i < segments; i++) {
		byte  = block / 8;
		bit   = block % 8;
		mask  = 0x80 >> bit;
		bytep = map + byte;

		*bytep = *bytep & (~mask);

		block++;
	}

	return(0); 
}
