#ifndef __PPC64_MMU_CONTEXT_H
#define __PPC64_MMU_CONTEXT_H

#include <linux/config.h>
#include <linux/spinlock.h>	
#include <asm/mmu.h>	
#include <asm/ppcdebug.h>	

/*
 * Copyright (C) 2001 PPC 64 Team, IBM Corp
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version
 * 2 of the License, or (at your option) any later version.
 */

#define NO_CONTEXT      	0
#define FIRST_USER_CONTEXT    	0x10    /* First 16 reserved for kernel */
#define NUM_USER_CONTEXT    	0x8000  /* Same as PID_MAX for now... */

struct mmu_context_stack_t {
	spinlock_t lock;
	mm_context_t top;
	mm_context_t stack[NUM_USER_CONTEXT];
};

extern struct mmu_context_stack_t mmu_context_stack;

static inline void
enter_lazy_tlb(struct mm_struct *mm, struct task_struct *tsk, unsigned cpu)
{
}

extern void flush_stab(void);

/*
 * The context number stack has underflowed.
 * meaning: we tried to push a context number that was freed
 * back onto the context stack and the stack was already full.
 */
static inline void
mmu_context_underflow(void)
{
	printk(KERN_DEBUG "mmu_context_underflow\n");
#ifdef CONFIG_XMON
	xmon(0);
#endif
	panic("mmu_context_underflow");
}


/*
 * Set up the context for a new address space.
 */
static inline int
init_new_context(struct task_struct *tsk, struct mm_struct *mm)
{
	mm_context_t top;

        spin_lock( &mmu_context_stack.lock );

	top = mmu_context_stack.top;

        if ( top >= NUM_USER_CONTEXT ) {
                spin_unlock( &mmu_context_stack.lock );
                return -ENOMEM;
        }

        mm->context = mmu_context_stack.stack[top];
        mmu_context_stack.stack[top] = NO_CONTEXT;
        mmu_context_stack.top = top + 1;
        spin_unlock( &mmu_context_stack.lock );

        return 0;
}


/*
 * We're finished using the context for an address space.
 */
static inline void
destroy_context(struct mm_struct *mm)
{
        spin_lock( &mmu_context_stack.lock );

        if ( --mmu_context_stack.top < FIRST_USER_CONTEXT ) {
                spin_unlock( &mmu_context_stack.lock );
                mmu_context_underflow();
        }

        mmu_context_stack.stack[mmu_context_stack.top] = mm->context;

        spin_unlock( &mmu_context_stack.lock );

}




/*
 * switch_mm is the entry point called from the architecture independent
 * code in kernel/sched.c
 */
static inline void
switch_mm(struct mm_struct *prev, struct mm_struct *next,
	  struct task_struct *tsk, int cpu)
{
	tsk->thread.pgdir = next->pgd;	/* cache the pgdir in the thread 
					   maybe not needed any more */
	flush_stab();
}

/*
 * After we have set current->mm to a new value, this activates
 * the context for the new mm so we see the new mappings.
 */
static inline void
activate_mm(struct mm_struct *active_mm, struct mm_struct *mm)
{
	current->thread.pgdir = mm->pgd;
	flush_stab();
}


#define VSID_RANDOMIZER 42470972311
#define VSID_MASK	0xfffffffff


/* This is only valid for kernel (including vmalloc, imalloc and bolted) EA's
 */
static inline unsigned long
get_kernel_vsid( unsigned long ea )
{
	unsigned long ordinal, vsid;
	
	ordinal = (((ea >> 28) & 0x1fffff) * NUM_USER_CONTEXT) | (ea >> 60);
	vsid = (ordinal * VSID_RANDOMIZER) & VSID_MASK;

	ifppcdebug(PPCDBG_HTABSTRESS) {
		/* For debug, this path creates a very poor vsid distribuition.
		 * A user program can access virtual addresses in the form
		 * 0x0yyyyxxxx000 where yyyy = xxxx to cause multiple mappings
		 * to hash to the same page table group.
		 */ 
		ordinal = ((ea >> 28) & 0x1fff) | (ea >> 44);
		vsid = ordinal & VSID_MASK;
	}

	return vsid;
} 

/* This is only valid for user EA's (user EA's do not exceed 2^41 (EADDR_SIZE))
 */
static inline unsigned long
get_vsid( unsigned long context, unsigned long ea )
{
	unsigned long ordinal, vsid;

	ordinal = (((ea >> 28) & 0x1fffff) * NUM_USER_CONTEXT) | context;
	vsid = (ordinal * VSID_RANDOMIZER) & VSID_MASK;

	ifppcdebug(PPCDBG_HTABSTRESS) {
		/* See comment above. */
		ordinal = ((ea >> 28) & 0x1fff) | (context << 16);
		vsid = ordinal & VSID_MASK;
	}

	return vsid;
}

#endif /* __PPC64_MMU_CONTEXT_H */
