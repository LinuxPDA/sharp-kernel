/*
 * linux/mm/freepg_signal.c
 *
 * (C) Copyright 2002 Lineo Japan, Inc.
 *
 * May be copied or modified under the terms of the GNU General Public
 * License.  See linux/COPYING for more information.
 *
 */

#include <linux/sched.h>
#include <linux/signal.h>
#include <linux/freepg_signal.h>


#define NOT_SEND 0
#define LOW 1
#define MIN 2

char freepg_signal_proc[16] = "qpe"; /* process name sending signal */

static struct {
    int cur_level;		/* 0: not send; 1: low; 2: min */
    int last_tick;		/* ticks when sent */
} freepg_signal = {
    cur_level: NOT_SEND,
    last_tick: 0,
};


static void
freepg_signal_send(int level)
{
    int signo = (level == MIN) ? SIGHUP : SIGUSR1;
    struct task_struct* p = NULL;
    struct task_struct* recipient_task = NULL;
    struct siginfo si;

    freepg_signal.cur_level = level;

    if (level == 0)
	return;

    read_lock(&tasklist_lock);
    for_each_task(p) {
	if (strcmp(p->comm, freepg_signal_proc) == 0) {
	    recipient_task = p;
	    break;
	}
    }

    if (recipient_task == NULL)
	goto finish;

    si.si_signo = signo;
    si.si_errno = 0;
    si.si_code = SI_KERNEL;
    si.si_pid = current->pid;
    si.si_uid = current->uid;
    freepg_signal.last_tick = jiffies;
    send_sig_info(signo, &si, recipient_task);

  finish:
    read_unlock(&tasklist_lock);
}


void
freepg_signal_reset(void)
{
    freepg_signal.cur_level = NOT_SEND;
}


void
freepg_signal_low(void)
{
    if (freepg_signal.cur_level >= LOW
	&& time_before(jiffies, freepg_signal.last_tick + 10 * HZ))
	return;

    freepg_signal_send(LOW);
}


void
freepg_signal_min(void)
{
    if (freepg_signal.cur_level >= MIN
	&& time_before(jiffies, freepg_signal.last_tick + 10 * HZ))
	return;

    freepg_signal_send(MIN);
}


void
freepg_signal_check(void)
{
    if (freepg_signal.cur_level == NOT_SEND
	|| time_before(jiffies, freepg_signal.last_tick + 10 * HZ))
	return;

    freepg_signal_send(freepg_signal.cur_level);
}
