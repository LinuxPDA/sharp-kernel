/* 
 * Copyright (C) 2000 Jeff Dike (jdike@karaya.com)
 * Licensed under the GPL
 */

#ifndef __SYSDEP_I386_PTRACE_H
#define __SYSDEP_I386_PTRACE_H

#define UM_MAX_REG (17)
#define UM_MAX_REG_OFFSET (UM_MAX_REG * sizeof(long))

struct sys_pt_regs {
  unsigned long regs[UM_MAX_REG];
};

#define EMPTY_REGS { { [ 0 ... UM_MAX_REG - 1 ] = 0 } }

#define UM_REG(r, n) ((r)->regs[n])

#define UM_IP(r) UM_REG(r, EIP)
#define UM_SP(r) UM_REG(r, UESP)
#define UM_ELF_ZERO(r) UM_REG(r, EDX)

#define UM_SYSCALL_RET(r) UM_REG(r, EAX)
#define UM_SYSCALL_NR(r) UM_REG(r, ORIG_EAX)

#define UM_SYSCALL_ARG1(r) UM_REG(r, EBX)
#define UM_SYSCALL_ARG2(r) UM_REG(r, ECX)
#define UM_SYSCALL_ARG3(r) UM_REG(r, EDX)
#define UM_SYSCALL_ARG4(r) UM_REG(r, ESI)
#define UM_SYSCALL_ARG5(r) UM_REG(r, EDI)
#define UM_SYSCALL_ARG6(r) UM_REG(r, EBP)

#define UM_IP_OFFSET (EIP * sizeof(long))
#define UM_SP_OFFSET (UESP * sizeof(long))
#define UM_ELF_ZERO_OFFSET (EDX * sizeof(long))

#define UM_SYSCALL_RET_OFFSET (EAX * sizeof(long))
#define UM_SYSCALL_NR_OFFSET (ORIG_EAX * sizeof(long))

#define UM_SYSCALL_ARG1_OFFSET (EBX * sizeof(long))
#define UM_SYSCALL_ARG2_OFFSET (ECX * sizeof(long))
#define UM_SYSCALL_ARG3_OFFSET (EDX * sizeof(long))
#define UM_SYSCALL_ARG4_OFFSET (ESI * sizeof(long))
#define UM_SYSCALL_ARG5_OFFSET (EDI * sizeof(long))
#define UM_SYSCALL_ARG6_OFFSET (EBP * sizeof(long))

#define UM_SET_SYSCALL_RETURN(r, result) UM_REG(r, EAX) = (result)

#define UM_FIX_EXEC_STACK(sp) do ; while(0)

#endif

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
