/*
 * Copyright (C) Paul Mackerras 1997.
 *
 * Updates for PPC64 by Todd Inglett & Dave Engebretsen.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version
 * 2 of the License, or (at your option) any later version.
 */
#define __KERNEL__
#include "zlib.h"
#include <asm/bootinfo.h>
#include <asm/processor.h>
#include <asm/page.h>
#include <string.h>

#define _ALIGN(addr,size)	(((addr)+(size)-1)&(~((size)-1)))

extern void *finddevice(const char *);
extern int getprop(void *, const char *, void *, int);
extern void printf(const char *fmt, ...);
extern int sprintf(char *buf, const char *fmt, ...);
void gunzip(void *, int, unsigned char *, int *);
void *claim(unsigned int, unsigned int, unsigned int);
void flush_cache(void *, int);
void pause(void);

#define RAM_START	0x00000000
#define RAM_END		(64<<20)

#define BOOT_START	((unsigned long)_start)
#define BOOT_END	((unsigned long)(_end + 0xFFF) & ~0xFFF)

#define RAM_FREE	((unsigned long)(_end+0x1000)&~0xFFF)

// Value picked to match that used by yaboot
#define PROG_START	0x01400000

char *avail_ram;
char *begin_avail, *end_avail;
char *avail_high;
unsigned int heap_use;
unsigned int heap_max;

extern char _end[];
extern char image_data[];
extern int image_len;
extern char initrd_data[];
extern int initrd_len;
extern char sysmap_data[];
extern int sysmap_len;
extern int uncompressed_size;

static char scratch[128<<10];	/* 128kB of scratch space for gunzip */

void
chrpboot(int a1, int a2, void *prom)
{
	unsigned sa, len;
	void *dst, *claim_addr;
	unsigned char *im;
	unsigned initrd_start, initrd_size;
	extern char _start;
    
	printf("chrpboot starting: loaded at 0x%x\n\r", (unsigned)&_start);
	printf("                   initrd_len =  0x%x\n\r", (unsigned)initrd_len);

	if (initrd_len) {
		initrd_size = initrd_len;
		initrd_start = (RAM_END - initrd_size) & ~0xFFF;
		a1 = initrd_start;
		a2 = initrd_size;
		claim(initrd_start, RAM_END - initrd_start, 0);
		printf("initial ramdisk moving 0x%x <- 0x%x (%x bytes)\n\r",
		       initrd_start, (unsigned)initrd_data, initrd_size);
		memcpy((void *)initrd_start, (void *)initrd_data, initrd_size);
	}

	im = image_data;
	len = image_len;
	uncompressed_size = _ALIGN(uncompressed_size, (1<<12));	/* page align it */

	for(claim_addr = PROG_START; 
	    claim_addr <= PROG_START * 8; 
	    claim_addr += 0x100000) {
		printf("    trying: 0x%08lx\n\r", claim_addr);
		dst = claim(claim_addr, uncompressed_size, 0);
		if (dst != (void *)-1) break;
	}
	if (dst == (void *)-1) {
		printf("claim error, can't allocate kernel memory\n\r");
		return;
	}

	if (im[0] == 0x1f && im[1] == 0x8b) {
		avail_ram = scratch;
		begin_avail = avail_high = avail_ram;
		end_avail = scratch + sizeof(scratch);
		printf("gunzipping (0x%x <- 0x%x:0x%0x)...",
		       (unsigned)dst, (unsigned)im, (unsigned)im+len);
		gunzip(dst, uncompressed_size, im, &len);
		printf("done %u bytes\n\r", len);
		printf("%u bytes of heap consumed, max in use %u\n\r",
		       (unsigned)(avail_high - begin_avail), heap_max);
	} else {
		memmove(dst, im, len);
	}

	flush_cache(dst, len);
	make_bi_recs((unsigned long)dst + len);

	sa = (unsigned long)dst;
	printf("start address = 0x%x\n\r", (unsigned) sa);

	(*(void (*)(int, int, void *))sa)(a1, a2, prom);

	printf("returned?\n\r");

	pause();
}

void make_bi_recs(unsigned long addr)
{
	struct bi_record *rec;
	    
	rec = (struct bi_record *)_ALIGN(addr + (1<<20) -1, (1<<20));

	rec->tag = BI_FIRST;
	rec->size = sizeof(struct bi_record);
	rec = (struct bi_record *)((unsigned long)rec + rec->size);
	    
	rec->tag = BI_BOOTLOADER_ID;
	sprintf( (char *)rec->data, "chrpboot");
	rec->size = sizeof(struct bi_record) + strlen("chrpboot") + 1;
	rec = (struct bi_record *)((unsigned long)rec + rec->size);
	    
	rec->tag = BI_MACHTYPE;
	rec->data[0] = _MACH_chrp;
	rec->data[1] = 1;
	rec->size = sizeof(struct bi_record) + sizeof(unsigned long);
	rec = (struct bi_record *)((unsigned long)rec + rec->size);
#if 0
	rec->tag = BI_SYSMAP;
	rec->data[0] = (unsigned long)sysmap_data;
	rec->data[1] = sysmap_len;
	rec->size = sizeof(struct bi_record) + sizeof(unsigned long);
	rec = (struct bi_record *)((unsigned long)rec + rec->size);
#endif
	rec->tag = BI_LAST;
	rec->size = sizeof(struct bi_record);
	rec = (struct bi_record *)((unsigned long)rec + rec->size);
}

struct memchunk {
	unsigned int size;
	unsigned int pad;
	struct memchunk *next;
};

static struct memchunk *freechunks;

void *zalloc(void *x, unsigned items, unsigned size)
{
	void *p;
	struct memchunk **mpp, *mp;

	size *= items;
	size = _ALIGN(size, sizeof(struct memchunk));
	heap_use += size;
	if (heap_use > heap_max)
		heap_max = heap_use;
	for (mpp = &freechunks; (mp = *mpp) != 0; mpp = &mp->next) {
		if (mp->size == size) {
			*mpp = mp->next;
			return mp;
		}
	}
	p = avail_ram;
	avail_ram += size;
	if (avail_ram > avail_high)
		avail_high = avail_ram;
	if (avail_ram > end_avail) {
		printf("oops... out of memory\n\r");
		pause();
	}
	return p;
}

void zfree(void *x, void *addr, unsigned nb)
{
	struct memchunk *mp = addr;

	nb = _ALIGN(nb, sizeof(struct memchunk));
	heap_use -= nb;
	if (avail_ram == addr + nb) {
		avail_ram = addr;
		return;
	}
	mp->size = nb;
	mp->next = freechunks;
	freechunks = mp;
}

#define HEAD_CRC	2
#define EXTRA_FIELD	4
#define ORIG_NAME	8
#define COMMENT		0x10
#define RESERVED	0xe0

#define DEFLATED	8

void gunzip(void *dst, int dstlen, unsigned char *src, int *lenp)
{
    z_stream s;
    int r, i, flags;

    /* skip header */
    i = 10;
    flags = src[3];
    if (src[2] != DEFLATED || (flags & RESERVED) != 0) {
	printf("bad gzipped data\n\r");
	exit();
    }
    if ((flags & EXTRA_FIELD) != 0)
	i = 12 + src[10] + (src[11] << 8);
    if ((flags & ORIG_NAME) != 0)
	while (src[i++] != 0)
	    ;
    if ((flags & COMMENT) != 0)
	while (src[i++] != 0)
	    ;
    if ((flags & HEAD_CRC) != 0)
	i += 2;
    if (i >= *lenp) {
	printf("gunzip: ran out of data in header\n\r");
	exit();
    }

    s.zalloc = zalloc;
    s.zfree = zfree;
    r = inflateInit2(&s, -MAX_WBITS);
    if (r != Z_OK) {
	printf("inflateInit2 returned %d\n\r", r);
	exit();
    }
    s.next_in = src + i;
    s.avail_in = *lenp - i;
    s.next_out = dst;
    s.avail_out = dstlen;
    r = inflate(&s, Z_FINISH);
    if (r != Z_OK && r != Z_STREAM_END) {
	printf("inflate returned %d msg: %s\n\r", r, s.msg);
	exit();
    }
    *lenp = s.next_out - (unsigned char *) dst;
    inflateEnd(&s);
}
