/*
 * c 2001 PPC 64 Team, IBM Corp
 *
 *      This program is free software; you can redistribute it and/or
 *      modify it under the terms of the GNU General Public License
 *      as published by the Free Software Foundation; either version
 *      2 of the License, or (at your option) any later version.
 */

#define __KERNEL__ 1
#include <asm/types.h>
#include <asm/page.h>
#include <stddef.h>
#include <linux/config.h>
#include <linux/threads.h>
#include <asm/processor.h>
#include <asm/ptrace.h>

#include <asm/iSeries/ItLpPaca.h>
#include <asm/Paca.h>


/* The Paca is an array with one entry per processor.  Each contains an 
 * ItLpPaca, which contains the information shared between the 
 * hypervisor and Linux.  Each also contains an ItLpRegSave area which
 * is used by the hypervisor to save registers.
 * On systems with hardware multi-threading, there are two threads
 * per processor.  The Paca array must contain an entry for each thread.
 * The VPD Areas will give a max logical processors = 2 * max physical
 * processors.  The processor VPD array needs one entry per physical
 * processor (not thread).
 */
#define PACAINITDATA(number,start,lpq,asrr,asrv) \
{ (struct ItLpPaca *)(((char *)(&xPaca[number]))+offsetof(struct Paca, xLpPaca)),    \
  (struct ItLpRegSave *)(((char *)(&xPaca[number]))+offsetof(struct Paca, xRegSav)), \
  0,            /* Current                */ \
  0,            /* R21 Save               */ \
  0,            /* R22 Save               */ \
  0,            /* Kernel stack addr save */ \
  (number), 	/* Paca Index        */ \
  0, 		/* HW Proc Number    */ \
  0,            /* CCR Save          */ \
  0,            /* MSR Save          */ \
  0,            /* LR Save           */ \
  0,            /* Pointer to thread */ \
  {(asrr),          /* Real pointer to segment table */ \
   (asrv),          /* Virt pointer to segment table */ \
   REGION_COUNT-1}, /* Round robin index */ \
  {0,0,0,0,0,0,0,0},/* Segment cache */ \
  0,            /* Kernel TOC address                */ \
  0,            /* R1 Save                           */ \
  (lpq),        /* &xItLpQueue,                      */ \
  {0,0,0,{0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0}, SPIN_LOCK_UNLOCKED, 0}, /*xRtas */ \
  (start),      /* Processor start */ \
  {0,0,0},      /* Resv              */ \
  0,            /* Default Decrementer */ \
  0,		  /* next_jiffy_update_tb    */ \
  {0,0,0,0,0,0,0,0,0,0,0,0},       /* Resv */ \
  { /* LpPaca                                             */ \
    xDesc:	0xd397d781, /* "LpPa"                     */ \
    xSize:	sizeof(struct ItLpPaca),   /*             */ \
    xFPRegsInUse:	1,				     \
    xDynProcStatus:	2,				     \
    xEndOfQuantum:	0xffffffffffffffff /*             */ \
  },                                                      \
  { /* LpRegSave                                          */ \
    0xd397d9e2, /* "LpRS"                                 */ \
    sizeof(struct ItLpRegSave) /*                         */ \
  },                                                      \
  0, /* pvr */                                               \
  0, /* pgd_cache */                                         \
  0, /* pmd_cache */                                         \
  0, /* pte_cache */                                         \
  0, /* pgtable_cache_sz */                                  \
  0, /* prof_multiplier */                                   \
  0  /* prof_counter */                                      \
}

struct Paca xPaca[maxPacas] __page_aligned = {
#ifdef CONFIG_PPC_ISERIES
	PACAINITDATA( 0, 1, &xItLpQueue, 0, 0xc000000000005000),
#else
	PACAINITDATA( 0, 1, 0, 0x5000, 0xc000000000005000),
#endif
	PACAINITDATA( 1, 0, 0, 0, 0),
	PACAINITDATA( 2, 0, 0, 0, 0),
	PACAINITDATA( 3, 0, 0, 0, 0),
	PACAINITDATA( 4, 0, 0, 0, 0),
	PACAINITDATA( 5, 0, 0, 0, 0),
	PACAINITDATA( 6, 0, 0, 0, 0),
	PACAINITDATA( 7, 0, 0, 0, 0),
	PACAINITDATA( 8, 0, 0, 0, 0),
	PACAINITDATA( 9, 0, 0, 0, 0),
	PACAINITDATA(10, 0, 0, 0, 0),
	PACAINITDATA(11, 0, 0, 0, 0),
	PACAINITDATA(12, 0, 0, 0, 0),
	PACAINITDATA(13, 0, 0, 0, 0),
	PACAINITDATA(14, 0, 0, 0, 0),
	PACAINITDATA(15, 0, 0, 0, 0),
	PACAINITDATA(16, 0, 0, 0, 0),
	PACAINITDATA(17, 0, 0, 0, 0),
	PACAINITDATA(18, 0, 0, 0, 0),
	PACAINITDATA(19, 0, 0, 0, 0),
	PACAINITDATA(20, 0, 0, 0, 0),
	PACAINITDATA(21, 0, 0, 0, 0),
	PACAINITDATA(22, 0, 0, 0, 0),
	PACAINITDATA(23, 0, 0, 0, 0),
	PACAINITDATA(24, 0, 0, 0, 0),
	PACAINITDATA(25, 0, 0, 0, 0),
	PACAINITDATA(26, 0, 0, 0, 0),
	PACAINITDATA(27, 0, 0, 0, 0),
	PACAINITDATA(28, 0, 0, 0, 0),
	PACAINITDATA(29, 0, 0, 0, 0),
	PACAINITDATA(30, 0, 0, 0, 0),
	PACAINITDATA(31, 0, 0, 0, 0),
	PACAINITDATA(32, 0, 0, 0, 0),
	PACAINITDATA(33, 0, 0, 0, 0),
	PACAINITDATA(34, 0, 0, 0, 0),
	PACAINITDATA(35, 0, 0, 0, 0),
	PACAINITDATA(36, 0, 0, 0, 0),
	PACAINITDATA(37, 0, 0, 0, 0),
	PACAINITDATA(38, 0, 0, 0, 0),
	PACAINITDATA(39, 0, 0, 0, 0),
	PACAINITDATA(40, 0, 0, 0, 0),
	PACAINITDATA(41, 0, 0, 0, 0),
	PACAINITDATA(42, 0, 0, 0, 0),
	PACAINITDATA(43, 0, 0, 0, 0),
	PACAINITDATA(44, 0, 0, 0, 0),
	PACAINITDATA(45, 0, 0, 0, 0),
	PACAINITDATA(46, 0, 0, 0, 0),
	PACAINITDATA(47, 0, 0, 0, 0)
};
