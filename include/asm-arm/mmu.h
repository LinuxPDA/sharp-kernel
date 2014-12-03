/*
 * ChangeLog:
 *     04-Apr-2003 Sharp for ARM FCSE
 */

#ifndef __ARM_MMU_H
#define __ARM_MMU_H

/* The ARM doesn't have a mmu context */
#ifdef CONFIG_ARM_FCSE
typedef struct {
	int cpu_pid;
} mm_context_t;
#else
typedef struct { } mm_context_t;
#endif

#endif
