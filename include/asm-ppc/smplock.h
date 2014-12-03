/*
 * BK Id: SCCS/s.smplock.h 1.10 10/23/01 08:09:35 trini
 */
/*
 * <asm/smplock.h>
 *
 * Default SMP lock implementation
 */
#ifdef __KERNEL__
#ifndef __ASM_SMPLOCK_H__
#define __ASM_SMPLOCK_H__

#include <linux/interrupt.h>
#include <linux/spinlock.h>

extern spinlock_t kernel_flag;

#ifdef CONFIG_SMP
#define kernel_locked()		spin_is_locked(&kernel_flag)
#else
#ifdef CONFIG_PREEMPT
#define kernel_locked()		preempt_get_count()
#else
#define kernel_locked()		1
#endif
#endif

/*
 * Release global kernel lock and global interrupt lock
 */
#define release_kernel_lock(task, cpu) \
do { \
	if (task->lock_depth >= 0) \
		spin_unlock(&kernel_flag); \
	release_irqlock(cpu); \
	__sti(); \
} while (0)

/*
 * Re-acquire the kernel lock
 */
#define reacquire_kernel_lock(task) \
do { \
	if (task->lock_depth >= 0) \
		spin_lock(&kernel_flag); \
} while (0)


/*
 * Getting the big kernel lock.
 *
 * This cannot happen asynchronously,
 * so we only need to worry about other
 * CPU's.
 */
static __inline__ void lock_kernel(void)
{
#ifdef CONFIG_PREEMPT
	if (current->lock_depth == -1)
		spin_lock(&kernel_flag);
	++current->lock_depth;
#else
	if (!++current->lock_depth)
		spin_lock(&kernel_flag);
#endif
}

static __inline__ void unlock_kernel(void)
{
	if (--current->lock_depth < 0)
		spin_unlock(&kernel_flag);
}
#endif /* __ASM_SMPLOCK_H__ */
#endif /* __KERNEL__ */
