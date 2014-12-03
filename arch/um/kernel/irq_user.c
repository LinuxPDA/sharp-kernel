/* 
 * Copyright (C) 2000 Jeff Dike (jdike@karaya.com)
 * Licensed under the GPL
 */

#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/time.h>
#include "user_util.h"
#include "kern_util.h"
#include "user.h"
#include "process.h"
#include "signal_user.h"

struct irq_fd {
	struct irq_fd *next;
	void *id;
	int fd;
	int irq;
	int pid;
};

static struct irq_fd *active_fds = NULL;
static fd_set active_fd_mask;
static int max_fd = -1;

extern int io_count, intr_count;

void sigio_handler(int sig, void *sc, int usermode)
{
	struct irq_fd *irq_fd, *next;
	struct timeval tv;
	fd_set fds;
	int i, n, fd;

	while(1){
		tv.tv_sec = 0;
		tv.tv_usec = 0;
		fds = active_fd_mask;
		if((n = select(max_fd + 1, &fds, NULL, NULL, &tv)) < 0){
			if(errno == EINTR) continue;
			printk("sigio_handler : select returned %d, "
			       "errno = %d\n", n, errno);
			break;
		}
		if(n == 0) break;
		for(i=0;i<=max_fd;i++){
			if(FD_ISSET(i, &fds)) FD_CLR(i, &active_fd_mask);
		}
		for(irq_fd=active_fds;irq_fd != NULL;irq_fd = next){
			/* These mysterious assignments protect us against
			 * the irq handler freeing the irq from under us.
			 */
			next = irq_fd->next;
			fd = irq_fd->fd;
			if(FD_ISSET(irq_fd->fd, &fds))
				do_IRQ(irq_fd->irq, usermode);
		}
	}
}

int activate_fd(int irq, int fd, void *dev_id)
{
	struct irq_fd *new_fd;
	int pid, retval;

	for(new_fd = active_fds;new_fd;new_fd = new_fd->next){
		if(new_fd->fd == fd){
			printk("Registering fd %d twice\n", fd);
			printk("Irqs : %d, %d\n", new_fd->irq, irq);
			printk("Ids : 0x%x, 0x%x\n", new_fd->id, dev_id);
			return(-EIO);
		}
	}
	if((retval = fcntl(fd, F_SETFL, O_ASYNC | O_NONBLOCK)) < 0){
		printk("Failed to set O_ASYNC and O_NONBLOCK on fd # %d, "
		       "errno = %d\n", fd, errno);
		return(-retval);
	}
	pid = external_pid(NULL);
	if((retval = fcntl(fd, F_SETOWN, pid)) < 0){
		printk("Failed to fcntl F_SETOWN fd %d to pid %d, "
		       "errno = %d\n", pid, errno);
		return(-retval);
	}
	new_fd = um_kmalloc(sizeof(*new_fd));
	if(new_fd == NULL) return(-ENOMEM);
	new_fd->next = active_fds;
	new_fd->id = dev_id;
	new_fd->irq = irq;
	new_fd->fd = fd;
	new_fd->pid = external_pid(NULL);
	active_fds = new_fd;
	if(fd > max_fd) max_fd = fd;
	FD_SET(fd, &active_fd_mask);
	return(0);
}

void free_irq_fd(void *dev_id)
{
	struct irq_fd **prev;

	prev = &active_fds;
	while(*prev != NULL){
		if((*prev)->id == dev_id){
			struct irq_fd *old_fd = *prev;
			*prev = (*prev)->next;
			FD_CLR(old_fd->fd, &active_fd_mask);
			kfree(old_fd);
			return;
		}
		prev = &(*prev)->next;
	}
}

void reactivate_fd(int fd)
{
	FD_SET(fd, &active_fd_mask);
}

void forward_interrupts(int pid)
{
	struct irq_fd *irq;

	for(irq=active_fds;irq != NULL;irq = irq->next){
		if(fcntl(irq->fd, F_SETOWN, pid) < 0){
			int save_errno = errno;
			if(fcntl(irq->fd, F_GETOWN, 0) != pid){
				/* XXX Just remove the irq rather than
				 * print out an infinite stream of these
				 */
				printk("Failed to forward %d to pid %d, "
				       "errno = %d\n", irq->fd, pid, 
				       save_errno);
			}
		}
		irq->pid = pid;
	}
}

void init_irq_signals(int on_sigstack)
{
	int flags;

	flags = on_sigstack ? SA_ONSTACK : 0;
	set_handler(SIGVTALRM, (__sighandler_t) alarm_handler, 
		    flags | SA_NODEFER | SA_RESTART, SIGUSR1, SIGIO, -1);
	set_handler(SIGIO, (__sighandler_t) irq_handler, flags | SA_RESTART,
		    SIGUSR1, SIGIO, -1);
}

/*
 * Overrides for Emacs so that we follow Linus's tabbing style.
 * Emacs will notice this stuff at the end of the file and automatically
 * adjust the settings for this buffer only.  This must remain at the end
 * of the file.
 * ---------------------------------------------------------------------------
 * Local variables:
 * c-file-style: "linux"
 * End:
 */
