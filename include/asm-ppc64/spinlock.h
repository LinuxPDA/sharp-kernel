#ifdef __KERNEL__
#ifndef __ASM_SPINLOCK_H
#define __ASM_SPINLOCK_H

#include <asm/system.h>

/*
 * Simple spin lock operations.  
 *
 * Type of int is used as a full 64b word is not necessary.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version
 * 2 of the License, or (at your option) any later version.
 */
typedef struct {
	volatile unsigned int lock;
} __spinlock_t;

#define __SPIN_LOCK_UNLOCKED	(__spinlock_t) { 0 }

#define __spin_is_locked(x)	((x)->lock != 0)

static __inline__ int __spin_trylock(__spinlock_t *lock)
{
	unsigned int tmp;

	__asm__ __volatile__(
"1:	lwarx		%0,0,%1		# spin_trylock\n\
	cmpwi		0,%0,0\n\
	li		%0,0\n\
	bne-		2f\n\
	li		%0,1\n\
	stwcx.		%0,0,%1\n\
	bne-		1b\n\
	isync\n\
2:"	: "=&r"(tmp)
	: "r"(&lock->lock)
	: "cr0", "memory");

	return tmp;
}

#ifdef CONFIG_PPC_ISERIES

extern void iSeries_spin_lock(__spinlock_t *lock);

static __inline__  void __spin_lock(__spinlock_t *lock)
{
	if ( ! __spin_trylock(lock) )
		iSeries_spin_lock(lock);
}


#else

static __inline__ void __spin_lock(__spinlock_t *lock)
{
	unsigned int tmp;
	/*
	 * This gets around a nasty gcc bug where it fails compiling
	 * this inline inside the loop in vcs_write.  Making it
	 * volatile forces foo onto the stack and then gcc can cope
	 * (for some unknown reason).  -- paulus
	 */
	volatile unsigned long foo = (unsigned long) &lock->lock;

	__asm__ __volatile__(
	"b		2f		# spin_lock\n\
1:	lwzx		%0,0,%1\n\
	cmpwi		0,%0,0\n\
	bne+		1b\n\
2:	lwarx		%0,0,%1\n\
	cmpwi		0,%0,0\n\
	bne-		1b\n\
	stwcx.		%2,0,%1\n\
	bne-		2b\n\
	isync"
	: "=&r"(tmp)
	: "r"(foo), "r"(1)
	: "cr0", "memory");
}

#endif /* CONFIG_PPC_ISERIES */

static __inline__ void __spin_unlock(__spinlock_t *lock)
{
	__asm__ __volatile__("eieio	# spin_unlock": : :"memory");
	lock->lock = 0;
}

/*
 * Read-write spinlocks, allowing multiple readers
 * but only one writer.
 *
 * NOTE! it is quite common to have readers in interrupts
 * but no interrupt writers. For those circumstances we
 * can "mix" irq-safe locks - any writer needs to get a
 * irq-safe write-lock, but readers can get non-irqsafe
 * read-locks.
 */
typedef struct {
	volatile signed int lock;
} __rwlock_t;

#define __RW_LOCK_UNLOCKED (__rwlock_t) { 0 }

static __inline__ int __read_trylock(__rwlock_t *rw)
{
	unsigned int tmp;
	unsigned int ret;

	__asm__ __volatile__(
"1:	lwarx		%0,0,%2		# __read_trylock\n\
	li		%1,0\n\
	extsw		%0,%0\n\
	addic.		%0,%0,1\n\
	ble-		2f\n\
	stwcx.		%0,0,%2\n\
	bne-		1b\n\
	li		%1,1\n\
	isync\n\
2:"	: "=&r"(tmp), "=&r"(ret)
	: "r"(&rw->lock)
	: "cr0", "memory");

	return ret;
}

#ifdef CONFIG_PPC_ISERIES

extern void iSeries_read_lock(__rwlock_t *rw);

static __inline__  void __read_lock(__rwlock_t *rw)
{
	if ( ! __read_trylock(rw) )
		iSeries_read_lock(rw);
}

#else

static __inline__ void __read_lock(__rwlock_t *rw)
{
	unsigned int tmp;
	volatile unsigned long foo = (unsigned long) &rw->lock;

	__asm__ __volatile__(
	"b		2f		# __read_lock\n\
1:	lwax		%0,0,%1\n\
	cmpwi		0,%0,0\n\
	blt+		1b\n\
2:	lwarx		%0,0,%1\n\
	extsw		%0,%0\n\
	addic.		%0,%0,1\n\
	ble-		1b\n\
	stwcx.		%0,0,%1\n\
	bne-		2b\n\
	isync"
	: "=&r"(tmp)
	: "r"(foo)
	: "cr0", "memory");
}

#endif

static __inline__ void __read_unlock(__rwlock_t *rw)
{
	unsigned int tmp;
	volatile unsigned long foo = (unsigned long) &rw->lock;

	__asm__ __volatile__(
	"eieio				# __read_unlock\n\
1:	lwarx		%0,0,%1\n\
	addic		%0,%0,-1\n\
	stwcx.		%0,0,%1\n\
	bne-		1b"
	: "=&r"(tmp)
	: "r"(foo)
	: "cr0", "memory");
}

static __inline__ int __write_trylock(__rwlock_t *rw)
{
	unsigned int tmp;
	unsigned int ret;

	__asm__ __volatile__(
"1:	lwarx		%0,0,%2		# __write_trylock\n\
	cmpwi		0,%0,0\n\
	li		%1,0\n\
	bne-		2f\n\
	stwcx.		%3,0,%2\n\
	bne-		1b\n\
	li		%1,1\n\
	isync\n\
2:"	: "=&r"(tmp), "=&r"(ret)
	: "r"(&rw->lock), "r"(-1)
	: "cr0", "memory");

	return ret;
}

#ifdef CONFIG_PPC_ISERIES
extern void iSeries_write_lock(__rwlock_t *rw);

static __inline__  void __write_lock(__rwlock_t *rw)
{
	if ( ! __write_trylock(rw) )
		iSeries_write_lock(rw);
}

#else

static __inline__ void __write_lock(__rwlock_t *rw)
{
	unsigned int tmp;

	__asm__ __volatile__(
	"b		2f		# __write_lock\n\
1:	lwax		%0,0,%1\n\
	cmpwi		0,%0,0\n\
	bne+		1b\n\
2:	lwarx		%0,0,%1\n\
	cmpwi		0,%0,0\n\
	bne-		1b\n\
	stwcx.		%2,0,%1\n\
	bne-		2b\n\
	isync"
	: "=&r"(tmp)
	: "r"(&rw->lock), "r"(-1)
	: "cr0", "memory");
}

#endif

static __inline__ void __write_unlock(__rwlock_t *rw)
{
	__asm__ __volatile__("eieio		# __write_unlock": : :"memory");
	rw->lock = 0;
}

static __inline__ int __is_read_locked(__rwlock_t *rw)
{
	return rw->lock > 0;
}

static __inline__ int __is_write_locked(__rwlock_t *rw)
{
	return rw->lock < 0;
}

#endif /* __ASM_SPINLOCK_H */
#endif /* __KERNEL__ */
