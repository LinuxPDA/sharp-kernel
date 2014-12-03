/**********************************************************************
wait.c

Copyright (C) 1999 Lars Brinkhoff.  See the file COPYING for licensing
terms and conditions.

**********************************************************************/

#include <errno.h>
#include <sys/wait.h>
#include <sys/ptrace.h>

#include "ptproxy.h"
#include "sysdep.h"
#include "wait.h"

void
proxy_wait (struct debugger *debugger, enum wait_type type, pid_t pid,
	    int *status, int options, void *rusage)
{
  debugger->wait_status_ptr = status;
  debugger->wait_options = options;
}

void
proxy_wait_return (struct debugger *debugger, pid_t unused)
{
  debugger->stopped = 0;

  if (debugger->debugee->died)
    {
      debugger->result = -ECHILD;
      debugger_cancelled_return (debugger, -1);
      return;
    }

  if (debugger->wait_options & __WCLONE)
    {
      debugger->result = -ECHILD;
      debugger_cancelled_return (debugger, -1);
      return;
    }

  if (debugger->debugee->zombie && debugger->debugee->event)
    debugger->debugee->died = 1;

  if (debugger->wait_options & WNOHANG)
    {
      if (debugger->debugee->event)
	{
	  debugger->debugee->event = 0;
	  ptrace (PTRACE_POKEDATA, debugger->pid,
		  debugger->wait_status_ptr, debugger->debugee->wait_status);
	  /* if (wait4)
	     ptrace (PTRACE_POKEDATA, pid, rusage_ptr, ...); */
	  debugger->result = debugger->debugee->pid;
	}
      else
	{
	  debugger->result = 0;
	}
      debugger_cancelled_return (debugger, -1);
      return;
    }

  if (debugger->debugee->event)
    {
      debugger->debugee->event = 0;
      ptrace (PTRACE_POKEDATA, debugger->pid,
	      debugger->wait_status_ptr, debugger->debugee->wait_status);
      /* if (wait4)
	   ptrace (PTRACE_POKEDATA, pid, rusage_ptr, ...); */
      debugger->result = debugger->debugee->pid;
      debugger_cancelled_return (debugger, -1);
      return;
    }

  debugger->stopped = 1;
}
