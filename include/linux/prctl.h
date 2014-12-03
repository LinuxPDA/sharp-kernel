/*
 * ChangLog:
 *	12-Dec-2002 Lineo Japan, Inc.
 */

#ifndef _LINUX_PRCTL_H
#define _LINUX_PRCTL_H

/* Values to pass as first argument to prctl() */

#define PR_SET_PDEATHSIG  1  /* Second arg is a signal */
#define PR_GET_PDEATHSIG  2  /* Second arg is a ptr to return the signal */

/* Get/set current->mm->dumpable */
#define PR_GET_DUMPABLE   3
#define PR_SET_DUMPABLE   4

/* Get/set unaligned access control bits (if meaningful) */
#define PR_GET_UNALIGN	  5
#define PR_SET_UNALIGN	  6
# define PR_UNALIGN_NOPRINT	1	/* silently fix up unaligned user accesses */
# define PR_UNALIGN_SIGBUS	2	/* generate SIGBUS on unaligned user access */

/* Get/set whether or not to drop capabilities on setuid() away from uid 0 */
#define PR_GET_KEEPCAPS   7
#define PR_SET_KEEPCAPS   8

/* Get/set floating-point emulation control bits (if meaningful) */
#define PR_GET_FPEMU  9
#define PR_SET_FPEMU 10
# define PR_FPEMU_NOPRINT	1	/* silently emulate fp operations accesses */
# define PR_FPEMU_SIGFPE	2	/* don't emulate fp operations, send SIGFPE instead */

/* Get/set oom_kill survival level */
#define PR_GET_OOM_KILL_SURVIVAL_LEVEL 257
#define PR_SET_OOM_KILL_SURVIVAL_LEVEL 258

#endif /* _LINUX_PRCTL_H */
