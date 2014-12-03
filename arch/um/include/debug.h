#ifndef __DEBUG_H
#define __DEBUG_H

extern void debugger_proxy(int status, pid_t pid);
extern void child_proxy(pid_t pid, int status);
extern void init_proxy (pid_t, int, int);
extern int start_debugger(char *prog, int startup, int stop, int *debugger_fd);

#endif
