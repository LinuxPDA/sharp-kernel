/**********************************************************************
ptproxy.h

Copyright (C) 1999 Lars Brinkhoff.  See the file COPYING for licensing
terms and conditions.
**********************************************************************/

#include <sys/types.h>

typedef struct debugger debugger_state;
typedef struct debugee debugee_state;

struct debugger
{
  pid_t pid;
  long result;
  int wait_options;
  int *wait_status_ptr;
  unsigned stopped : 1;
  void (*handle_trace) (debugger_state *, pid_t);

  debugee_state *debugee;
};

struct debugee
{
  pid_t pid;
  int wait_status;
  unsigned died : 1;
  unsigned event : 1;
  unsigned stopped : 1;
  unsigned trace_singlestep : 1;
  unsigned trace_syscall : 1;
  unsigned traced : 1;
  unsigned zombie : 1;

  debugger_state *debugger;
};

extern void debugger_syscall (debugger_state *, pid_t);
extern void debugger_cancelled_return (debugger_state *, pid_t);
extern void proxy_ptrace (struct debugger *, int, pid_t, long, long, pid_t);
