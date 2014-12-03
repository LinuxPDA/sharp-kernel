#ifndef _PPC64_CURRENT_H
#define _PPC64_CURRENT_H

#include <asm/processor.h>

/*
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version
 * 2 of the License, or (at your option) any later version.
 *
 * 'current' is kept in the Paca which is found via SPRG3
 * (if we change which SPRG points to the Paca, then the
 *  constant below must change!)
 */

struct task_struct;

static inline struct task_struct * __current(void)
{
	struct task_struct * curr;
        unsigned long *addr;

	__asm__ __volatile__ (
		"	mfspr	%1,275\n"	/* get Paca addr from SPRG3 */
		"	ld	%0,16(%1)"	/* get 'current' */
		: "=r" (curr)
                : "r" (addr), "m" (*addr) );

	return curr;
}

#define current (__current())

static inline unsigned long __current_paca(void)
{
        unsigned long paca;

	__asm__ __volatile__ (
		"	mfspr	%0,275\n"	/* get Paca addr from SPRG3 */
		: "=r" (paca) );

	return paca;
}

#define current_paca (__current_paca())


#endif /* !(_PPC64_CURRENT_H) */
