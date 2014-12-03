/*
 * linux/inclide/linux/pthread.h
 *
 * Copyright (C) 2003 Lineo Solutions, Inc.
 *
 * Based on:
 *	rtai/posix/include/rtai_pthread.h
 */
//////////////////////////////////////////////////////////////////////////////
//
//      Copyright (©) 1999 Zentropic Computing, All rights reserved
//
// Authors:             Steve Papacharalambous (stevep@zentropix.com)
// Original date:       Thu 15 Jul 1999
//
//
// This library is free software; you can redistribute it and/or
// modify it under the terms of the GNU Lesser General Public
// License as published by the Free Software Foundation; either
// version 2 of the License, or (at your option) any later version.
//
// This library is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
// Lesser General Public License for more details.
//
// You should have received a copy of the GNU Lesser General Public
// License along with this library; if not, write to the Free Software
// Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA.
//
// pthreads interface for Real Time Linux.
//
///////////////////////////////////////////////////////////////////////////////

#ifndef __LINUX_PTHREAD_H
#define __LINUX_PTHREAD_H 1

#include <linux/sched.h>
#include <asm/signal.h>

#include <linux/rt_sched.h>
#include <asm/rthal.h>

typedef unsigned long pthread_t;

struct rt_pthread_data;
typedef struct rt_pthread_data rt_pthread_data_t;
typedef rt_pthread_data_t*     rt_pthread_t;

typedef struct {
	int                 detachstate;
	int                 schedpolicy;
	struct sched_param  schedparam;
	int                 inheritsched;
	int                 scope;
} pthread_attr_t;

typedef struct {
	int                 mutexkind;
} pthread_mutexattr_t;

typedef struct {
	int                 dummy;
} pthread_condattr_t;

typedef struct {
	RTH*                m_owner;
	int                 m_kind;
	struct rt_semaphore m_semaphore;
} pthread_mutex_t;

typedef struct {
	struct rt_semaphore c_waiting;
} pthread_cond_t;

struct pthread_start_args {
	void*              (*start_routine)(void*);
	void*              arg;
	sigset_t           mask;
	int                schedpolicy;
	struct sched_param schedparam;
};

typedef int rt_jmp_buf[6];

struct rt_pthread_data {
	rt_pthread_t              p_nextlive;
	rt_pthread_t              p_prevlive;
	rt_pthread_t              p_nextwaiting;
	pthread_t                 p_tid;
	int                       p_pid;
	int                       p_priority;
	int                       p_policy;
	int*                      p_spinlock;
	int                       p_signal;
	rt_jmp_buf*               p_signal_jmp;
	rt_jmp_buf*               p_cancel_jmp;
	char                      p_terminated;
	char                      p_detached;
	char                      p_exited;
	void*                     p_retval;
	int                       p_retcode;
	rt_pthread_t              p_joining;
	char                      p_cancelstate;
	char                      p_canceltype;
	char                      p_canceled;
	int                       p_errno;
	struct pthread_start_args p_start_args;
	RTH                       thread;
};

struct __pthread_cleanup_buffer {
	void                             (*routine)(void *);
	void*                            arg;
	int                              canceltype;
	struct __pthread_cleanup_buffer* prev;
};

pthread_t pthread_self(void);
int       pthread_create(pthread_t*, pthread_attr_t*, void *(*)(void *),
			 void*);
void      pthread_exit(void*);
int       pthread_attr_init(pthread_attr_t*);
int       pthread_attr_destroy(pthread_attr_t*);
int       pthread_attr_setdetachstate(pthread_attr_t*, int);
int       pthread_attr_getdetachstate(const pthread_attr_t*, int*);
int       pthread_attr_setschedparam(pthread_attr_t*,
				     const struct sched_param*);
int       pthread_attr_getschedparam(const pthread_attr_t*,
				     struct sched_param*);
int       pthread_attr_setschedpolicy(pthread_attr_t*, int);
int       pthread_attr_getschedpolicy(const pthread_attr_t*, int*);
int       pthread_attr_setinheritsched(pthread_attr_t*, int);
int       pthread_attr_getinheritsched(const pthread_attr_t*, int*);
int       pthread_attr_setscope(pthread_attr_t*, int);
int       pthread_attr_getscope(const pthread_attr_t*, int*);
int       pthread_mutex_init(pthread_mutex_t*, const pthread_mutexattr_t*);
int       pthread_mutex_destroy(pthread_mutex_t*);
int       pthread_mutexattr_init(pthread_mutexattr_t*);
int       pthread_mutexattr_destroy(pthread_mutexattr_t*);
int       pthread_mutexattr_setkind_np(pthread_mutexattr_t*, int);
int       pthread_mutexattr_getkind_np(const pthread_mutexattr_t*, int*);
int       pthread_setschedparam(pthread_t, int, const struct sched_param*);
int       pthread_getschedparam(pthread_t, int*, struct sched_param*);
int       pthread_mutex_trylock(pthread_mutex_t*);
int       pthread_mutex_lock(pthread_mutex_t*);
int       pthread_mutex_unlock(pthread_mutex_t*);
int       pthread_cond_init(pthread_cond_t*, const pthread_condattr_t*);
int       pthread_cond_destroy(pthread_cond_t*);
int       pthread_condattr_init(pthread_condattr_t*);
int       pthread_condattr_destroy(pthread_condattr_t*);
int       pthread_cond_wait(pthread_cond_t*, pthread_mutex_t*);
int       pthread_cond_timedwait(pthread_cond_t*, pthread_mutex_t*,
				 const struct timespec*);
int       pthread_cond_signal(pthread_cond_t*);
int       pthread_cond_broadcast(pthread_cond_t*);
int       pthread_suspend_np(pthread_t);
int       pthread_resume_np(pthread_t, int);

int       sched_yield(void);
int       nanosleep(const struct timespec*, struct timespec*);

enum { PTHREAD_CANCEL_ENABLE,   PTHREAD_CANCEL_DISABLE };
enum { PTHREAD_CANCEL_DEFERRED, PTHREAD_CANCEL_ASYNCHRONOUS };
enum { PTHREAD_INHERIT_SCHED,   PTHREAD_EXPLICIT_SCHED };
enum { PTHREAD_CREATE_JOINABLE, PTHREAD_CREATE_DETACHED };
enum { PTHREAD_SCOPE_SYSTEM,    PTHREAD_SCOPE_PROCESS };
enum { PTHREAD_COUNT_RESUME_NP, PTHREAD_FORCE_RESUME_NP };
enum { PTHREAD_MUTEX_FAST_NP,
       PTHREAD_MUTEX_RECURSIVE_NP,
       PTHREAD_MUTEX_ERRORCHECK_NP
};
                          

#define PTHREAD_MUTEX_INITIALIZER {0, PTHREAD_MUTEX_FAST_NP, {{0, 0, 0}, RSEM_BIN, 1, 0, RTQ_FIFO}}
#define PTHREAD_COND_INITIALIZER {{{0, 0, 0}, RSEM_BIN, 1, 0, RTQ_FIFO}}

#endif /* __LINUX_PTHREAD_H */
