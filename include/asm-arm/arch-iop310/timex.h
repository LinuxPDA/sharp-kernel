/*
 * linux/include/asm-arm/arch-iop80310/timex.h
 *
 * XScale architecture timex specifications
 */


#ifdef CONFIG_ARCH_IQ80310

/* This is for the IQ-80310 eval board */
#define CLOCK_TICK_RATE 33000000 /* Underlying HZ */

#else

/* This is for a timer based on the XS80200's PMU counter */
#define CLOCK_TICK_RATE 600000000 /* Underlying HZ */

#endif
