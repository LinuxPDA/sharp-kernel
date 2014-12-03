/*
 * JFFS2 -- Journalling Flash File System, Version 2.
 *
 * Copyright (C) 2001, 2002 Red Hat, Inc.
 *
 * Created by David Woodhouse <dwmw2@cambridge.redhat.com>
 *
 * For licensing information, see the file 'LICENCE' in this directory.
 *
 *
 * ChangeLog:
 *     05-Dec-2002 SHARP  adjust gc-end conditions
 *     27-Nov-2002 Lineo Japan, Inc.  add effective-gc mode
 *     25-Nov-2002 Lineo Japan, Inc.  end GC when jffs2_garbage_collect_pass
 *       returns -ENOSPC
 *     23-Nov-2002 Lineo Japan, Inc.  reverse precedence between
 *       c->nodemerge_list and c->very_dirty_list
 *     15-Nov-2002 Lineo Japan, Inc.  add nodemerge facility and c->nodemerge_list
 *				      correct indentation
 *     29-Oct-2002 Lineo Japan, Inc.  add reserved blocks for badblock
 *				      add c->cont_gc_count
 *     24-Oct-2002 Lineo Japan, Inc.  add min. available blocks
 *
 */

#define __KERNEL_SYSCALLS__

#include <linux/kernel.h>
#include <linux/jffs2.h>
#include <linux/mtd/mtd.h>
#include <linux/interrupt.h>
#include <linux/completion.h>
#include <linux/mtd/compatmac.h> /* recalc_sigpending() */
#include <linux/unistd.h>
#include "nodelist.h"


static int jffs2_garbage_collect_thread(void *);
static int thread_should_do_effective_gc(struct jffs2_sb_info *c);
static int thread_should_wake(struct jffs2_sb_info *c, int);

void jffs2_garbage_collect_trigger(struct jffs2_sb_info *c)
{
	spin_lock_bh(&c->erase_completion_lock);
        if (c->gc_task && thread_should_wake(c, 0))
                send_sig(SIGHUP, c->gc_task, 1);
	spin_unlock_bh(&c->erase_completion_lock);
}

/* This must only ever be called when no GC thread is currently running */
int jffs2_start_garbage_collect_thread(struct jffs2_sb_info *c)
{
	pid_t pid;
	int ret = 0;

	if (c->gc_task)
		BUG();

	init_MUTEX_LOCKED(&c->gc_thread_start);
	init_completion(&c->gc_thread_exit);

	pid = kernel_thread(jffs2_garbage_collect_thread, c, CLONE_FS|CLONE_FILES);
	if (pid < 0) {
		printk(KERN_WARNING "fork failed for JFFS2 garbage collect thread: %d\n", -pid);
		complete(&c->gc_thread_exit);
		ret = pid;
	} else {
		/* Wait for it... */
		D1(printk(KERN_DEBUG "JFFS2: Garbage collect thread is pid %d\n", pid));
		down(&c->gc_thread_start);
	}
 
	return ret;
}

void jffs2_stop_garbage_collect_thread(struct jffs2_sb_info *c)
{
	if (c->mtd->type == MTD_NANDFLASH) {
		/* stop a eventually scheduled wbuf flush timer */
		del_timer_sync(&c->wbuf_timer);
		/* make sure, that a scheduled wbuf flush task is completed */
		flush_scheduled_tasks();
	}

	spin_lock_bh(&c->erase_completion_lock);
	if (c->gc_task) {
		D1(printk(KERN_DEBUG "jffs2: Killing GC task %d\n", c->gc_task->pid));
		send_sig(SIGKILL, c->gc_task, 1);
	}
	spin_unlock_bh(&c->erase_completion_lock);
	wait_for_completion(&c->gc_thread_exit);
}

static int jffs2_garbage_collect_thread(void *_c)
{
	struct jffs2_sb_info *c = _c;
	int ret = 0;
	jffs2_gcmode_t gcmode = GCMODE_NORMAL;

	daemonize();

	c->gc_task = current;
	up(&c->gc_thread_start);

        sprintf(current->comm, "jffs2_gcd_mtd%d", c->mtd->index);

#if 0
	set_user_nice(current, 10);
#else
	current->nice = 10;
#endif

	for (;;) {
		spin_lock_irq(&current->sigmask_lock);
		if (gcmode == GCMODE_NORMAL)
			siginitsetinv (&current->blocked, sigmask(SIGUSR1) | sigmask(SIGHUP) | sigmask(SIGKILL) | sigmask(SIGSTOP) | sigmask(SIGCONT));
		else
			siginitsetinv (&current->blocked, sigmask(SIGKILL) | sigmask(SIGSTOP) | sigmask(SIGCONT));
		recalc_sigpending();
		spin_unlock_irq(&current->sigmask_lock);

		if (gcmode == GCMODE_NORMAL && (ret == -ENOSPC || !thread_should_wake(c, 1))) {
                        set_current_state (TASK_INTERRUPTIBLE);
			D1(printk(KERN_DEBUG "jffs2_garbage_collect_thread sleeping...\n"));
			/* Yes, there's a race here; we checked thread_should_wake() before
			   setting current->state to TASK_INTERRUPTIBLE. But it doesn't
			   matter - We don't care if we miss a wakeup, because the GC thread
			   is only an optimisation anyway. */
			schedule();
		}
                
		cond_resched();

                /* Put_super will send a SIGKILL and then wait on the sem. 
                 */
                while (signal_pending(current)) {
                        siginfo_t info;
                        unsigned long signr;

                        spin_lock_irq(&current->sigmask_lock);
                        signr = dequeue_signal(&current->blocked, &info);
                        spin_unlock_irq(&current->sigmask_lock);

                        switch(signr) {
                        case SIGSTOP:
                                D1(printk(KERN_DEBUG "jffs2_garbage_collect_thread(): SIGSTOP received.\n"));
                                set_current_state(TASK_STOPPED);
                                schedule();
                                break;

                        case SIGKILL:
                                D1(printk(KERN_DEBUG "jffs2_garbage_collect_thread(): SIGKILL received.\n"));
				spin_lock_bh(&c->erase_completion_lock);
                                c->gc_task = NULL;
				c->effective_gc_count = 0;
				spin_unlock_bh(&c->erase_completion_lock);
				complete_and_exit(&c->gc_thread_exit, 0);

			case SIGHUP:
				D1(printk(KERN_DEBUG "jffs2_garbage_collect_thread(): SIGHUP received.\n"));
				break;

			case SIGUSR1:
				gcmode = GCMODE_EFFECTIVE;
				c->effective_gc_count++;
				D1(printk(KERN_DEBUG "jffs2_garbage_collect_thread(): SIGUSR1 received. effective_gc_count=(%d)\n", c->effective_gc_count));
				break;

			default:
				D1(printk(KERN_DEBUG "jffs2_garbage_collect_thread(): signal %ld received\n", signr));

                        }
                }
		/* We don't want SIGHUP to interrupt us. STOP and KILL are OK though. */
		spin_lock_irq(&current->sigmask_lock);
		siginitsetinv (&current->blocked, sigmask(SIGKILL) | sigmask(SIGSTOP) | sigmask(SIGCONT));
		recalc_sigpending();
		spin_unlock_irq(&current->sigmask_lock);

		if (gcmode == GCMODE_EFFECTIVE && ! thread_should_do_effective_gc(c)) {
			gcmode = GCMODE_NORMAL;
			c->effective_gc_count++;
			ret = 0;
			continue;
		}

		D1(printk(KERN_DEBUG "jffs2_garbage_collect_thread(): pass\n"));
		ret = jffs2_garbage_collect_pass(c, gcmode);
		if (gcmode == GCMODE_EFFECTIVE && ret <= 0) {
			gcmode = GCMODE_NORMAL;
			c->effective_gc_count++;
		}
	}
}

#ifdef CONFIG_JFFS2_NODEMERGE
static inline int thread_should_wake_by_nodemerge_list(struct jffs2_sb_info *c, int from_effective_gc)
{
	if (c->flags & JFFS2_SB_FLAG_GCING_A_BLOCK)
		return 1;
#if 1 /* ... */
	else if (! list_empty(&c->very_dirty_list))
		return 1;
#endif
	else if (! list_empty(&c->nodemerge_list)){
		if(NR_AVAIL_BLOCKS(c) >= JFFS2_RESERVED_BLOCKS_CLEAN){
			return 1;
		}else{
			if(from_effective_gc &&
			   jffs2_get_dirty_block(&c->nodemerge_list) != NULL){
				return 1;
			}else{
				return 0;
			}
		}
	}
	else
		return 0;
}
#else
static inline int thread_should_wake_by_nodemerge_list(struct jffs2_sb_info* c, int from_effective_gc)
{
	return 0;
}
#endif

static int thread_should_do_effective_gc(struct jffs2_sb_info *c)
{
	int ret = 0;

	spin_lock_bh(&c->erase_completion_lock);

	if (thread_should_wake_by_nodemerge_list(c, 1))	// add for /proc/.../.nodemerge
		ret = 1;

	if (NR_AVAIL_BLOCKS(c) < JFFS2_RESERVED_BLOCKS_GCTRIGGER) {
		uint32_t dirty_now, erasing_dirty_now, nonerasing_dirty_now;
		int32_t avail_blocks;

		dirty_now = c->dirty_size;
		erasing_dirty_now = c->erasing_dirty_size;
		nonerasing_dirty_now = ((dirty_now >= erasing_dirty_now) ? (dirty_now - erasing_dirty_now) : 0);
		avail_blocks = NR_AVAIL_BLOCKS(c);

		if(ret){
			if (avail_blocks <= JFFS2_RESERVED_BLOCKS_GCMERGE)
				ret = 0;
		}else{
			if (avail_blocks <= 1 ||
			    dirty_now < c->sector_size ||
			    nonerasing_dirty_now / c->sector_size < JFFS2_RESERVED_BLOCKS_DIRTY)
				ret = 0;
			else
				ret = 1;
		}
	}
	else
		ret = 1;

	spin_unlock_bh(&c->erase_completion_lock);

	return ret;
}

static int thread_should_wake(struct jffs2_sb_info *c, int from_gc_loop)
{
	int ret = 0;

	if (thread_should_wake_by_nodemerge_list(c, 0))
		ret = 1;

	if (NR_AVAIL_BLOCKS(c) < JFFS2_RESERVED_BLOCKS_GCTRIGGER) {
		uint32_t dirty_now, erasing_dirty_now, nonerasing_dirty_now;
		int32_t avail_blocks;

		dirty_now = c->dirty_size;
		erasing_dirty_now = c->erasing_dirty_size;
		nonerasing_dirty_now = ((dirty_now >= erasing_dirty_now) ? (dirty_now - erasing_dirty_now) : 0);
		avail_blocks = NR_AVAIL_BLOCKS(c);

		if(ret){
			if (c->cont_gc_count >= JFFS2_MAX_CONT_GC ||
			    avail_blocks <= JFFS2_RESERVED_BLOCKS_GCMERGE)
				ret = 0;
		}else{
			if (c->cont_gc_count >= JFFS2_MAX_CONT_GC ||
			    avail_blocks <= 1 ||
			    dirty_now < c->sector_size ||
			    nonerasing_dirty_now / c->sector_size < JFFS2_RESERVED_BLOCKS_DIRTY)
				ret = 0;
			else {
				c->cont_gc_count++;
				ret = 1;
			}
		}
	}
	else if (from_gc_loop && ! ret)
		c->cont_gc_count = 0;

	D1(printk(KERN_DEBUG "thread_should_wake(): nr_free_blocks %d, nr_erasing_blocks %d, dirty_size 0x%x, cont_gc_count %d: %s\n", 
		  c->nr_free_blocks, c->nr_erasing_blocks, c->dirty_size, c->cont_gc_count, ret?"yes":"no"));

	return ret;
}

/*
 * Local variables:
 *   c-basic-offset: 8
 * End:
 */
