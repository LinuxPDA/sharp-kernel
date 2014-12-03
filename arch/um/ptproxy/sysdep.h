/**********************************************************************
sysdep.h

Copyright (C) 1999 Lars Brinkhoff.  See the file COPYING for licensing
terms and conditions.
**********************************************************************/

extern int syscall_get_number (pid_t pid);
extern void syscall_cancel (pid_t pid);
extern void syscall_get_args (pid_t pid, long *arg1, long *arg2,
			      long *arg3, long *arg4, long *arg5);
extern void syscall_set_result (pid_t pid, long result);
extern void syscall_continue (pid_t pid);
