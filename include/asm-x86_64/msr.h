#ifndef X86_64_MSR_H
#define X86_64_MSR_H 1
/*
 * Access to machine-specific registers (available on 586 and better only)
 * Note: the rd* operations modify the parameters directly (without using
 * pointer indirection), this allows gcc to optimize better
 */

#define rdmsr(msr,val1,val2) \
       __asm__ __volatile__("rdmsr" \
			    : "=a" (val1), "=d" (val2) \
			    : "c" (msr))


#define rdmsrl(msr,val) do { unsigned long a__,b__; \
       __asm__ __volatile__("rdmsr" \
			    : "=a" (a__), "=d" (b__) \
			    : "c" (msr)); \
       val = a__ | (b__<<32); \
} while(0); 

#define wrmsr(msr,val1,val2) \
     __asm__ __volatile__("wrmsr" \
			  : /* no outputs */ \
			  : "c" (msr), "a" (val1), "d" (val2))

#define wrmsrl(msr,val) wrmsr(msr,(__u32)((__u64)(val)),((__u64)(val))>>32) 

/* wrmsrl with exception handling */
#define checking_wrmsrl(msr,val) ({ int ret__;						\
	asm volatile("2: wrmsr ; xorl %0,%0\n"						\
		     "1:\n\t"								\
		     ".section .fixup,\"ax\"\n\t"					\
		     "3:  movl %4,%0 ; jmp 1b\n\t"					\
		     ".previous\n\t"							\
 		     ".section __ex_table,\"a\"\n"					\
		     "   .align 8\n\t"							\
		     "   .quad 	2b,3b\n\t"						\
		     ".previous"							\
		     : "=a" (ret__)							\
		     : "c" (msr), "0" ((__u32)val), "d" ((val)>>32), "i" (-EFAULT)); 	\
	ret__; })

#define rdtsc(low,high) \
     __asm__ __volatile__("rdtsc" : "=a" (low), "=d" (high))

#define rdtscl(low) \
     __asm__ __volatile__ ("rdtsc" : "=a" (low) : : "edx")

#define rdtscll(val) \
     __asm__ __volatile__ ("rdtsc" : "=A" (val))

#define write_tsc(val1,val2) wrmsr(0x10, val1, val2)

#define rdpmc(counter,low,high) \
     __asm__ __volatile__("rdpmc" \
			  : "=a" (low), "=d" (high) \
			  : "c" (counter))


/* AMD/K8 specific MSRs */ 
#define MSR_EFER 0xc0000080		/* extended feature register */
#define MSR_STAR 0xc0000081		/* legacy mode SYSCALL target */
#define MSR_LSTAR 0xc0000082 		/* long mode SYSCALL target */
#define MSR_CSTAR 0xc0000083		/* compatibility mode SYSCALL target */
#define MSR_SYSCALL_MASK 0xc0000084	/* EFLAGS mask for syscall */
#define MSR_FS_BASE 0xc0000100		/* 64bit GS base */
#define MSR_GS_BASE 0xc0000101		/* 64bit FS base */
#define MSR_KERNEL_GS_BASE  0xc0000102	/* SwapGS GS shadow (or USER_GS from kernel view) */ 

#define MSR_IA32_PLATFORM_ID           0x017

#define MSR_IA32_APICBASE              0x01b
#define                MSR_IA32_APICBASE_BSP           (1<<8)
#define                MSR_IA32_APICBASE_ENABLE        (1<<11)
#define                MSR_IA32_APICBASE_BASE          (0xfffff<<12)

#define MSR_IA32_UCODE_WRITE           0x079
#define MSR_IA32_UCODE_REV             0x08b

#define MSR_IA32_PERFCTR0              0x0c1
#define MSR_IA32_PERFCTR1              0x0c2

#define MSR_IA32_MCG_CAP               0x179
#define MSR_IA32_MCG_STATUS            0x17a
#define MSR_IA32_MCG_CTL               0x17b

#define MSR_IA32_EVNTSEL0              0x186
#define MSR_IA32_EVNTSEL1              0x187

#define MSR_IA32_DEBUGCTLMSR           0x1d9
#define MSR_IA32_LASTBRANCHFROMIP      0x1db
#define MSR_IA32_LASTBRANCHTOIP                0x1dc
#define MSR_IA32_LASTINTFROMIP         0x1dd
#define MSR_IA32_LASTINTTOIP           0x1de

#define MSR_IA32_MC0_BASE              0x400
#define                MSR_IA32_MC0_CTL_OFFSET         0x0
#define                MSR_IA32_MC0_STATUS_OFFSET      0x1
#define                MSR_IA32_MC0_ADDR_OFFSET        0x2
#define                MSR_IA32_MC0_MISC_OFFSET        0x3
#define                MSR_IA32_MC0_BANK_COUNT         0x4

#define MSR_K7_EVNTSEL0                        0xC0010000
#define MSR_K7_PERFCTR0                        0xC0010004

#endif
