/* include/asm-mipsnommu/rwsem.h
 *
 * (c) Michael Leslie <mleslie@lineo.com> Lineo Inc. 2001
 *
 * based on semaphore.h and i386/rwsem.h
 */
/*
 * rw mutexes (should that be mutices? =) -- throw rw spinlocks and
 * semaphores together, and this is what we end up with...
 *
 * The lock is initialized to BIAS.  This way, a writer subtracts BIAS ands
 * gets 0 for the case of an uncontended lock.  Readers decrement by 1 and
 * see a positive value when uncontended, negative if there are writers
 * waiting (in which case it goes to sleep).
 *
 * The value 0x01000000 supports up to 128 processors and lots of processes.
 * BIAS must be chosen such that subtracting BIAS once per CPU will result
 * in the int remaining negative.  In terms of fairness, this should result
 * in the lock flopping back and forth between readers and writers under
 * heavy use.
 *
 * Once we start supporting machines with more than 128 CPUs, we should go
 * for using a 64bit atomic type instead of 32bit as counter. We shall
 * probably go for bias 0x80000000 then, so that single sethi can set it.
 * */


#ifndef _ASM_MIPS_RWSEM_H
#define _ASM_MIPS_RWSEM_H

#ifndef _LINUX_RWSEM_H
#error please dont include asm/rwsem.h directly, use linux/rwsem.h instead
#endif

#ifdef __KERNEL__

#include <linux/list.h>
#include <linux/spinlock.h>


#define RW_LOCK_BIAS		0x01000000

struct rw_semaphore {
	atomic_t		count;
	/* bit 0 means read bias granted;
	   bit 1 means write bias granted.  */
	unsigned long		granted;
	wait_queue_head_t	wait;
	wait_queue_head_t	write_bias_wait;
#if WAITQUEUE_DEBUG
	long			__magic;
	atomic_t		readers;
	atomic_t		writers;
#endif
};


#if WAITQUEUE_DEBUG
#define __RWSEM_DEBUG_INIT	, ATOMIC_INIT(0), ATOMIC_INIT(0)
#else
#define __RWSEM_DEBUG_INIT	/* */
#endif

/* mleslie note: do not remove this block - it could be used to implement
 * the newly defined arm/rwsem.h */

#define __RWSEM_INITIALIZER(name,count)					\
	{ ATOMIC_INIT(count), 0,					\
	  __WAIT_QUEUE_HEAD_INITIALIZER((name).wait),			\
	  __WAIT_QUEUE_HEAD_INITIALIZER((name).write_bias_wait)		\
	  __SEM_DEBUG_INIT(name) __RWSEM_DEBUG_INIT }

#define __DECLARE_RWSEM_GENERIC(name,count) \
	struct rw_semaphore name = __RWSEM_INITIALIZER(name,count)


#define DECLARE_RWSEM(name) \
	__DECLARE_RWSEM_GENERIC(name, RW_LOCK_BIAS)
#define DECLARE_RWSEM_READ_LOCKED(name) \
	__DECLARE_RWSEM_GENERIC(name, RW_LOCK_BIAS-1)
#define DECLARE_RWSEM_WRITE_LOCKED(name) \
	__DECLARE_RWSEM_GENERIC(name, 0)


static inline void init_rwsem(struct rw_semaphore *sem)
{
	atomic_set(&sem->count, RW_LOCK_BIAS);
	sem->granted = 0;
	init_waitqueue_head(&sem->wait);
	init_waitqueue_head(&sem->write_bias_wait);
#if WAITQUEUE_DEBUG
	sem->__magic = (long)&sem->__magic;
	atomic_set(&sem->readers, 0);
	atomic_set(&sem->writers, 0);
#endif
}

#if 0
/* The expensive part is outlined.  */
extern void __down_read(struct rw_semaphore *sem, int count);
extern void __down_write(struct rw_semaphore *sem, int count);
extern void __rwsem_wake(struct rw_semaphore *sem, unsigned long readers);
#endif


static inline void __down_read(struct rw_semaphore *sem)
{
	int count;

#if WAITQUEUE_DEBUG
	CHECK_MAGIC(sem->__magic);
#endif

	count = atomic_dec_return(&sem->count);
	if (count < 0) {
		__down_read(sem, count);
	}
	mb();

#if WAITQUEUE_DEBUG
	if (sem->granted & 2)
		BUG();
	if (atomic_read(&sem->writers))
		BUG();
	atomic_inc(&sem->readers);
#endif
}

static inline void __down_write(struct rw_semaphore *sem)
{
	int count;

#if WAITQUEUE_DEBUG
	CHECK_MAGIC(sem->__magic);
#endif

	count = atomic_sub_return(RW_LOCK_BIAS, &sem->count);
	if (count) {
		__down_write(sem, count);
	}
	mb();

#if WAITQUEUE_DEBUG
	if (atomic_read(&sem->writers))
		BUG();
	if (atomic_read(&sem->readers))
		BUG();
	if (sem->granted & 3)
		BUG();
	atomic_inc(&sem->writers);
#endif
}

/* When a reader does a release, the only significant case is when
   there was a writer waiting, and we've bumped the count to 0: we must
   wake the writer up.  */

static inline void __up_read(struct rw_semaphore *sem)
{
#if WAITQUEUE_DEBUG
	CHECK_MAGIC(sem->__magic);
	if (sem->granted & 2)
		BUG();
	if (atomic_read(&sem->writers))
		BUG();
	atomic_dec(&sem->readers);
#endif

	mb();
	if (atomic_inc_return(&sem->count) == 0)
		__rwsem_wake(sem, 0);
}

/*
 * Releasing the writer is easy -- just release it and wake up any sleepers.
 */
static inline void __up_write(struct rw_semaphore *sem)
{
	int count;

#if WAITQUEUE_DEBUG
	CHECK_MAGIC(sem->__magic);
	if (sem->granted & 3)
		BUG();
	if (atomic_read(&sem->readers))
		BUG();
	if (atomic_read(&sem->writers) != 1)
		BUG();
	atomic_dec(&sem->writers);
#endif

	mb();
	count = atomic_add_return(RW_LOCK_BIAS, &sem->count);
	if (count - RW_LOCK_BIAS < 0 && count >= 0) {
		/* Only do the wake if we're no longer negative.  */
		__rwsem_wake(sem, count);
	}
}

#endif /* __KERNEL__ */
#endif /* _ASM_MIPS_RWSEM_H */
                                                                               
