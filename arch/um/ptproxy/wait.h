/**********************************************************************
wait.h

Copyright (C) 1999 Lars Brinkhoff.  See the file COPYING for licensing
terms and conditions.
**********************************************************************/

enum wait_type
{
  WAIT_WAIT,
  WAIT_WAITPID,
  WAIT_WAIT3,
  WAIT_WAIT4
};

extern void proxy_wait (struct debugger *debugger, enum wait_type type,
			pid_t arg1, int *arg2, int arg3, void *arg4);
extern void proxy_wait_return (struct debugger *debugger, pid_t unused);
