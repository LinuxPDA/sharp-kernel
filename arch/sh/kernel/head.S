/* $Id: head.S,v 1.17 2000/03/06 12:44:24 gniibe Exp $
 *
 *  arch/sh/kernel/head.S
 *
 *  Copyright (C) 1999, 2000  Niibe Yutaka & Kaz Kojima
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file "COPYING" in the main directory of this archive
 * for more details.
 *
 * Head.S contains the SH exception handlers and startup code.
 */
#include <linux/config.h>
#include <linux/linkage.h>

	.section	.empty_zero_page, "aw"
ENTRY(empty_zero_page)
	.long	1		/* MOUNT_ROOT_RDONLY */
	.long	0		/* RAMDISK_FLAGS */
	.long	0x0200		/* ORIG_ROOT_DEV */
	.long	1		/* LOADER_TYPE */
	.long	0x00360000	/* INITRD_START */
	.long	0x000a0000	/* INITRD_SIZE */
	.long	0
	.balign 4096,0,4096

#if defined(CONFIG_XIP_KERNEL)
#if defined(CONFIG_CPU_SH4)
#if defined(CONFIG_CPU_SUBTYPE_SH7750R) || \
    defined(CONFIG_CPU_SUBTYPE_SH7751R) || \
    defined(CONFIG_CPU_SUBTYPE_SH7760)
	. = . + 28672		/* EMODE */
#else
	. = . + 12288		/* NOT EMODE */
#endif
#endif
#endif

#if defined(CONFIG_XIP_KERNEL)
#if defined(CONFIG_SH_7760_SOLUTION_ENGINE)
        .section        .xtext, "aw"
#else
        .text
#endif
#else
        .text
#endif

/*
 * Condition at the entry of _stext:
 *
 *   BSC has already been initialized.
 *   INTC may or may not be initialized.
 *   VBR may or may not be initialized.
 *   MMU may or may not be initialized.
 *   Cache may or may not be initialized.
 *   Hardware (including on-chip modules) may or may not be initialized. 
 *
 */
ENTRY(_stext)
#if defined(CONFIG_XIP_KERNEL)
#if defined(CONFIG_SH_MS7290CP)
	mov.l	CCR1_A, r1
	mov.l	CCR1_D, r0
	mov.l	r0, @r1

	mov.l	MMUCR_A, r1
	mov.l	MMUCR_D, r0
	mov.l	r0, @r1

	mov.l	STBCR_A, r1
	mov	#0, r0
	mov.b	r0, @r1

	mov.l	STBCR2_A, r1
	mov	#0, r0
	mov.b	r0, @r1

	mov.l	STBCR3_A, r1
	mov	#8, r0
	mov.b	r0, @r1

	mov.l	STBCR4_A, r1
	mov	#0, r0
	mov.b	r0, @r1

	mov.l	STBCR5_A, r1
	mov	#0, r0
	mov.b	r0, @r1

	mov.l	WTCSR_A, r1
	mov.w	WTCSR_D, r0
	mov.w	r0, @r1

	mov.l	WTCNT_A, r1
	mov.w	WTCNT_D, r0
	mov.w	r0, @r1

	mov.l	RWTCSR_A, r1
	mov.w	RWTCSR_D, r0
	mov.w	r0, @r1

	mov.l	FRQCR_A, r1
	mov.w	FRQCR_D, r0
	mov.w	r0, @r1

	mov.l	STBCR4_A, r1
	mov	#0, r0
	mov.b	r0, @r1

	mov.l	UCLKCR_A, r1
	mov.w	UCLKCR_D, r0
	mov.w	r0, @r1

	mov.l	FCLKCR_A, r1
	mov.w	FCLKCR_D, r0
	mov.w	r0, @r1

	mov.l	STBCR4_A, r1
	mov	#0, r0
	mov.b	r0, @r1

	mov.l	CCR1_A, r1
	mov.l	CCR1_D2, r0
	mov.l	r0, @r1

	mov.l	CMNCR_A, r1
	mov.l 	CMNCR_D, r0
	mov.l	r0, @r1

	mov.l	CS0BCR_A, r1
	mov.l 	CS0BCR_D, r0
	mov.l	r0, @r1

	mov.l	CS2BCR_A, r1
	mov.l 	CS2BCR_D, r0
	mov.l	r0, @r1

	mov.l	CS3BCR_A, r1
	mov.l 	CS3BCR_D, r0
	mov.l	r0, @r1

	mov.l	CS4BCR_A, r1
	mov.l 	CS4BCR_D, r0
	mov.l	r0, @r1

	mov.l	CS5ABCR_A, r1
	mov.l 	CS5ABCR_D, r0
	mov.l	r0, @r1

	mov.l	CS5BBCR_A, r1
	mov.l 	CS5BBCR_D, r0
	mov.l	r0, @r1

	mov.l	CS6ABCR_A, r1
	mov.l 	CS6ABCR_D, r0
	mov.l	r0, @r1

	mov.l	CS6BBCR_A, r1
	mov.l 	CS6BBCR_D, r0
	mov.l	r0, @r1

	mov.l	CS0WCR_A, r1
	mov.l 	CS0WCR_D, r0
	mov.l	r0, @r1

	mov.l	CS2WCR_A, r1
	mov.l 	CS2WCR_D, r0
	mov.l	r0, @r1

	mov.l	CS3WCR_A, r1
	mov.l 	CS3WCR_D, r0
	mov.l	r0, @r1

	mov.l	CS4WCR_A, r1
	mov.l 	CS4WCR_D, r0
	mov.l	r0, @r1

	mov.l	CS5AWCR_A, r1
	mov.l 	CS5AWCR_D, r0
	mov.l	r0, @r1

	mov.l	CS5BWCR_A, r1
	mov.l 	CS5BWCR_D, r0
	mov.l	r0, @r1

	mov.l	CS6AWCR_A, r1
	mov.l 	CS6AWCR_D, r0
	mov.l	r0, @r1

	mov.l	CS6BWCR_A, r1
	mov.l 	CS6BWCR_D, r0
	mov.l	r0, @r1

	mov.l	SR_D, r0
	ldc	r0, sr

	mov.l	LED_A, r1
	mov.w	LED_D, r0
	mov.w	r0, @r1
	
#elif defined(CONFIG_SH_MS7727RP)
	mov.l	CCR_A, r1
	mov.l	CCR_DI, r0
	mov.l	r0, @r1

	mov.l	FRQCR_A, r1
	mov.l	FRQCR_D, r0
	mov.w	r0,@r1

	mov.l	BCR1_A, r1
	mov.l	BCR1_D, r0
	mov.w	r0,@r1

	mov.l	BCR2_A, r1
	mov.l	BCR2_D, r0
	mov.w	r0,@r1

	mov.l	WCR1_A, r1
	mov.l	WCR1_D, r0
	mov.w	r0,@r1

	mov.l	WCR2_A, r1
	mov.l	WCR2_D, r0
	mov.w	r0,@r1

	mov.l	MCR_A, r1
	mov.l	MCR_D, r0
	mov.w	r0,@r1

	mov.l	RTCNT_A, r1
	mov.l	RTCNT_D, r0
	mov.w	r0,@r1

	mov.l	RTCOR_A, r1
	mov.l	RTCOR_D, r0
	mov.w	r0,@r1

	mov.l	RTCSR_A, r1
	mov.l	RTCSR_D, r0
	mov.w	r0,@r1

	mov.l	RFCR_A, r1
	mov.l	RFCR_D, r0
	mov.w	r0,@r1

	mov.l	RFCR_A, r1
	mov	#8, r3
1:
	mov.w	@r1, r0
	extu.w	r0, r2
	cmp/hi	r3, r2
	bf	1b

	mov.l	SDMR3_A, r1
	mov	#0, r0
	mov.b	r0,@r1

	mov.l	CCR_A, r1
	mov.l	CCR_D, r0
	mov.l	r0, @r1
#elif defined(CONFIG_SH_MS7710SE)
        mov.l   FRQCR_A,r1      /* FRQCR Address */
        mov.l   FRQCR_D,r0      /* FRQCR Data */
        mov.w   r0,@r1
                                                                               
        mov.l   CMNCR_A,r1      /* CMNCR Address */
        mov.l   CMNCR_D,r0      /* CMNCR Data */
        mov.l   r0,@r1
                                                                               
        mov.l   CS0BCR_A,r1     /* CS0BCR Address */
        mov.l   CS0BCR_D,r0     /* CS0BCR Data */
        mov.l   r0,@r1
                                                                               
        mov.l   CS2BCR_A,r1     /* CS2BCR Address */
        mov.l   CS2BCR_D,r0     /* CS2BCR Data */
        mov.l   r0,@r1
                                                                               
        mov.l   CS3BCR_A,r1     /* CS3BCR Address */
        mov.l   CS3BCR_D,r0     /* CS3BCR Data */
        mov.l   r0,@r1
                                                                               
        mov.l   CS4BCR_A,r1     /* CS4BCR Address */
        mov.l   CS4BCR_D,r0     /* CS4BCR Data */
	        mov.l   r0,@r1
                                                                               
        mov.l   CS5ABCR_A,r1    /* CS5ABCR Address */
        mov.l   CS5ABCR_D,r0    /* CS5ABCR Data */
        mov.l   r0,@r1
                                                                               
        mov.l   CS5BBCR_A,r1    /* CS5BBCR Address */
        mov.l   CS5BBCR_D,r0    /* CS5BBCR Data */
        mov.l   r0,@r1
                                                                               
        mov.l   CS6ABCR_A,r1    /* CS6ABCR Address */
        mov.l   CS6ABCR_D,r0    /* CS6ABCR Data */
        mov.l   r0,@r1
                                                                               
        mov.l   CS6BBCR_A,r1    /* CS6BBCR Address */
        mov.l   CS6BBCR_D,r0    /* CS6BBCR Data */
        mov.l   r0,@r1
                                                                               
        mov.l   CS0WCR_A,r1     /* CS0WCR Address */
        mov.l   CS0WCR_D,r0     /* CS0WCR Data */
        mov.l   r0,@r1
                                                                               
        mov.l   CS2WCR_A,r1     /* CS2WCR Address */
        mov.l   CS2WCR_D,r0     /* CS2WCR Data */
        mov.l   r0,@r1
                                                                               
        mov.l   CS3WCR_A,r1     /* CS3WCR Address */
        mov.l   CS3WCR_D,r0     /* CS3WCR Data */
        mov.l   r0,@r1
                                                                               
        mov.l   CS4WCR_A,r1     /* CS4WCR Address */
        mov.l   CS4WCR_D,r0     /* CS4WCR Data */
        mov.l   r0,@r1
                                                                               
        mov.l   CS5AWCR_A,r1    /* CS5AWCR Address */
        mov.l   CS5AWCR_D,r0    /* CS5AWCR Data */
        mov.l   r0,@r1
                                                                               
        mov.l   CS5BWCR_A,r1    /* CS5BWCR Address */
        mov.l   CS5BWCR_D,r0    /* CS5BWCR Data */
        mov.l   r0,@r1
                                                                               
        mov.l   CS6AWCR_A,r1    /* CS6AWCR Address */
        mov.l   CS6AWCR_D,r0    /* CS6AWCR Data */
        mov.l   r0,@r1
                                                                               
        mov.l   CS6BWCR_A,r1    /* CS6BWCR Address */
        mov.l   CS6BWCR_D,r0    /* CS6BWCR Data */
        mov.l   r0,@r1
                                                                               
        mov.l   SDCR_A,r1       /* SDCR Address */
        mov.l   SDCR_D,r0       /* SDCR Data */
        mov.l   r0,@r1
                                                                               
        ! Do you need PCMCIA setting?
        ! If so, please add the lines here...
                                                                               
        mov.l   RTCSR_A,r1      /* RTCSR Address */
        mov.l   RTCSR_D,r0      /* RTCSR Data */
        mov.l   r0,@r1
                                                                               
        mov.l   RTCNT_A,r1      /* RTCNT Address */
        mov.l   RTCNT_D,r0      /* RTCNT Data */
        mov.l   r0,@r1
                                                                               
        mov.l   RTCOR_A,r1      /* RTCOR Address */
        mov.l   RTCOR_D,r0      /* RTCOR Data */
        mov.l   r0,@r1
                                                                               
        mov.l   SDCR_A,r1       /* SDCR Address */
        mov.l   SDCR_D_2,r0     /* SDCR Data_2 */
        mov.l   r0,@r1
                                                                               
        /* Wait DRAM refresh 8 times */
        nop
        mov.l   r14,@-r15
        add     #-4,r15
        mov     r15,r14
        mov     #0,r1
        mov.l   r1,@r14
.L2:
        mov.l   @r14,r2
        mov     #7,r1
        cmp/gt  r1,r2
        bf      .L5
        bra     .L3
        nop
        .align 4
.L5:
.L6:
        mov.l   .L12,r1
        mov.l   @r1,r0
        and     #128,r0
        tst     r0,r0
        bt      .L6
        mov.l   @r1,r2
        mov.l   .L10,r1
        and     r1,r2
        mov.l   .L11,r1
        or      r1,r2
        mov.l   .L12,r1
        mov.l   r2,@r1
        mov.l   @r14,r1
        add     #1,r1
        mov.l   r1,@r14
        bra     .L2
        nop
        .align 4
.L3:
        add     #4,r14
        mov     r14,r15
        mov.l   @r15+,r14
        nop
.L13:
       .align 2
                                                                               
        mov.l   SDMR3_A,r1      /* Set SDRAM mode */
        mov     #0,r0
        mov.w   r0,@r1
                                                                               
        mov.l   PTCR_A,r1       /* Set Port T to SCIF mode */
        mov     #0,r0
        mov.w   r0,@r1
	
#elif defined(CONFIG_SH_SOLUTION_ENGINE)
#if  defined(CONFIG_CPU_SH3)
	mov.l	CCR_A, r1
	mov.l	CCR_DI, r0
	mov.l	r0, @r1

	mov.l	FRQCR_A, r1
	mov.l	FRQCR_D, r0
	mov.w	r0,@r1

	mov.l	BCR1_A, r1
	mov.l	BCR1_D, r0
	mov.w	r0,@r1

	mov.l	BCR2_A, r1
	mov.l	BCR2_D, r0
	mov.w	r0,@r1

	mov.l	WCR1_A, r1
	mov.l	WCR1_D, r0
	mov.w	r0,@r1

	mov.l	WCR2_A, r1
	mov.l	WCR2_D, r0
	mov.w	r0,@r1

	mov.l	MCR_A, r1
	mov.l	MCR_D, r0
	mov.w	r0,@r1

	mov.l	RTCNT_A, r1
	mov.l	RTCNT_D, r0
	mov.w	r0,@r1

	mov.l	RTCOR_A, r1
	mov.l	RTCOR_D, r0
	mov.w	r0,@r1

	mov.l	RTCSR_A, r1
	mov.l	RTCSR_D, r0
	mov.w	r0,@r1

	mov.l	RFCR_A, r1
	mov.l	RFCR_D, r0
	mov.w	r0,@r1

	mov.l	RFCR_A, r1
	mov	#8, r3
1:
	mov.w	@r1, r0
	extu.w	r0, r2
	cmp/hi	r3, r2
	bf	1b

	mov.l	SDMR3_A, r1
	mov	#0, r0
	mov.b	r0,@r1

	mov.l	CCR_A, r1
	mov.l	CCR_D, r0
	mov.l	r0, @r1

#elif defined(CONFIG_CPU_SH4)
	mov.l	CCR_A,r1	/* CCR Address */
	mov.l	CCR_DI,r0	/* CCR Data */
	mov.l	r0,@r1
#if 0
	mov.l	FRQCR_A, r1
	mov.l	FRQCR_D, r0
	mov.w	r0, @r1
#endif
	mov.l	BCR1_A, r1
	mov.l	BCR1_D, r0
	mov.l	r0, @r1

	mov.l	BCR2_A, r1
	mov.l	BCR2_D, r0
	mov.w	r0, @r1

	mov.l	WCR1_A, r1
	mov.l	WCR1_D, r0
	mov.l	r0, @r1

	mov.l	WCR2_A, r1
	mov.l	WCR2_D, r0
	mov.l	r0, @r1

	mov.l	WCR3_A, r1
	mov.l	WCR3_D, r0
	mov.l	r0, @r1

	mov.l	MCR_A, r1
	mov.l	MCR_D1, r0
	mov.l	r0, @r1

	mov.l	SDMR3_A, r1
	mov	#0, r0
	mov.b	r0, @r1

	mov.l	RTCNT_A, r1
	mov.l	RTCNT_D, r0
	mov.w	r0, @r1

	mov.l	RTCOR_A, r1
	mov.l	RTCOR_D, r0
	mov.w	r0, @r1

	mov.l	RTCSR_A, r1
	mov.l	RTCSR_D, r0
	mov.w	r0, @r1

	mov.l	RFCR_A, r1
	mov.l	RFCR_D, r0
	mov.w	r0, @r1

	mov	#30, r3
1:
	mov.w	@r1, r0
	extu.w	r0, r2
	cmp/hi	r3, r2
	bf	1b

	mov.l	MCR_A, r1
	mov.l	MCR_D2, r0
	mov.l	r0, @r1

	mov.l	SDMR3_A, r1
	mov	#0, r0
	mov.b	r0, @r1

	mov.l	CCR_A,r1	/* CCR Address */
	mov.l	CCR_D,r0	/* CCR Data */
	mov.l	r0,@r1
#endif
#elif defined(CONFIG_SH_RTS7751R2D)
	mov.l	FRQCR_A,r1	/* FRQCR Address */
	mov.l	FRQCR_D,r0	/* FRQCR Data */
	mov.w	r0,@r1

	mov.l	MMUCCR_A,r1	/* MMUCCR Address */
	mov.l	MMUCCR_D,r0	/* MMUCCR Data */
	mov.w	r0,@r1

/*
	mov.l	MMUPTEA_A,r1
	mov.l	MMUPTEA_D,r0
	mov.l	r0,@r1
*/

	mov.l	BCR1_A,r1	/* BCR1 Address */
	mov.l	BCR1_D,r0	/* BCR1 Data */
	mov.l	r0,@r1

	mov.l	BCR2_A,r1	/* BCR2 Address */
	mov.l	BCR2_D,r0	/* BCR2 Data */
	mov.w	r0,@r1

	mov.l	BCR3_A,r1	/* BCR3 Address */
	mov.l	BCR3_D,r0	/* BCR3 Data */
	mov.w	r0,@r1

	mov.l	BCR4_A,r1	/* BCR4 Address */
	mov.l	BCR4_D,r0	/* BCR4 Data */
	mov.l	r0,@r1

	mov.l	WCR1_A,r1	/* WCR1 Address */
	mov.l	WCR1_D,r0	/* WCR1 Data */
	mov.l	r0,@r1

	mov.l	WCR2_A,r1	/* WCR2 Address */
	mov.l	WCR2_D,r0	/* WCR2 Data */
	mov.l	r0,@r1

	mov.l	WCR3_A,r1	/* WCR3 Address */
	mov.l	WCR3_D,r0	/* WCR3 Data */
	mov.l	r0,@r1

	mov.l	PCR_A,r1	/* PCR Address */
	mov.l	PCR_D,r0	/* PCR Data */
	mov.w	r0,@r1

	mov.l	LED_A,r1	/* LED Address */
	mov	#0xff,r0	/* LED ALL 'on' */
	mov.w	r0,@r1

	mov.l	MCR_A,r1	/* MCR Address */
	mov.l	MCR_D1,r0	/* MCR Data1 */
	mov.l	r0,@r1

	! Do you need PCMCIA setting?	
	! If so, please add the lines here...

	mov.l	RTCNT_A,r1	/* RTCNT Address */
	mov.l	RTCNT_D,r0	/* RTCNT Data */
	mov.w	r0,@r1

	mov.l	RTCOR_A,r1	/* RTCOR Address */
	mov.l	RTCOR_D,r0	/* RTCOR Data */
	mov.w	r0,@r1

	mov.l	RFCR_A,r1	/* RFCR Address */
	mov.l	RFCR_D,r0	/* RFCR Data */
	mov.w	r0,@r1		/* Clear reflesh counter */

	mov.l	RTCSR_A,r1	/* RTCSR Address */
	mov.l	RTCSR_D,r0	/* RTCSR DATA */
	mov.w	r0,@r1

	mov.l	SDMR3_A,r1	/* Set SDRAM mode */
	mov	#0x55,r0
	mov.b	r0,@r1

	/* Wait DRAM refresh 30 times */
	mov.l	RFCR_A,r1	/* RFCR Address */
	mov	#30,r3
1:
	mov.w	@r1,r0
	extu.w	r0,r2
	cmp/hi	r3,r2
	bf	1b

	mov.l	MCR_A,r1	/* MCR Address */
	mov.l	MCR_D2,r0	/* MCR Data2 */
	mov.l	r0,@r1

	mov.l	SDMR3_A,r1	/* Set SDRAM mode */
	mov	#0,r0
	mov.b	r0,@r1

	mov.l	IRLMASK_A,r1	/* IRLMASK Address */
	mov.l	IRLMASK_D,r0	/* IRLMASK Data */
	mov.l	r0,@r1

#if defined(CONFIG_RTS7751R2D_VER_10)
	mov.l	WAITSET_A,r1	/* AREA4 WAITSET Address */
	mov.l	WAITSET_D,r0	/* AREA4 WAITSET Data */
	mov.w	r0,@r1
#endif

	mov.l	FPGA_IRLMON_A,r1
	mov.w	@r1,r2
	mov.w	FPGA_CF_CDINT,r1
	and	r2,r1
	extu.w	r1,r1
	tst	r1,r1
	bt	NEXT
	nop
	mov.l	FPGA_CFCTL_A,r1		/* FPGA_CFCTL Address */
	mov.l	FPGA_CFCTL_D,r0		/* FPGA_CFCTL Data */
	mov.w	r0,@r1
	mov.l	FPGA_CFPOW_A,r1		/* FPGA_CFPOW Address */
	mov.l	FPGA_CFPOW_D,r0		/* FPGA_CFPOW Data */
	mov.w	r0,@r1
	mov.l	FPGA_CFCDINTCLR_A,r1	/* FPGA_CFCDINTCLR Address */
	mov.l	FPGA_CFCDINTCLR_D,r0	/* FPGA_CFCDINTCLR Data */
	mov.w	r0,@r1
NEXT:
#elif defined(CONFIG_SH_7760_SOLUTION_ENGINE)
        mov.l   CCR_A, r1
        mov.l   CCR_DI, r0
        mov.l   r0, @r1

        mov.l   FRQCR_A, r1
        mov.l   FRQCR_D, r0
        mov.w   r0, @r1

        mov.l   BCR1_A, r1
        mov.l   BCR1_D, r0
        mov.l   r0, @r1

        mov.l   BCR2_A, r1
        mov.l   BCR2_D, r0
        mov.w   r0, @r1

        mov.l   BCR3_A, r1
        mov.l   BCR3_D, r0
        mov.w   r0, @r1

        mov.l   BCR4_A, r1
        mov.l   BCR4_D, r0
        mov.w   r0, @r1

        mov.l   WCR1_A, r1
        mov.l   WCR1_D, r0
        mov.l   r0, @r1

        mov.l   WCR2_A, r1
        mov.l   WCR2_D, r0
        mov.l   r0, @r1

        mov.l   WCR3_A, r1
        mov.l   WCR3_D, r0
        mov.l   r0, @r1

        mov.l   WCR4_A, r1
        mov.l   WCR4_D, r0
        mov.l   r0, @r1

        mov.l   PCR_A, r1
        mov.w   PCR_D, r0
        mov.w   r0, @r1

        mov.l   RTCNT_A, r1
        mov.l   RTCNT_D, r0
        mov.w   r0, @r1

        mov.l   RTCOR_A, r1
        mov.l   RTCOR_D, r0
        mov.w   r0, @r1

        mov.l   RFCR_A, r1
        mov.l   RFCR_D, r0
        mov.w   r0,@ r1

        mov.l   MCR_A, r1
        mov.l   MCR_D0, r0
        mov.l   r0, @r1

        mov.l   SDMR3_A, r1
        mov     #0, r0
        mov.b   r0, @r1

        mov.l   RTCSR_A, r1
        mov.l   RTCSR_D0, r0
        mov.w   r0, @r1

        mov.l   RFCR_A, r1
        mov     #30, r3
1:
        mov.w   @r1, r0
        extu.w  r0, r2
        cmp/hi  r3, r2
        bf      1b

        mov.l   MCR_A, r1
        mov.l   MCR_D1, r0
        mov.l   r0, @r1

        mov.l   SDMR3_A, r1
        mov     #0, r0
        mov.b   r0, @r1

        mov.l   RTCSR_A, r1
        mov.l   RTCSR_D1, r0
        mov.w   r0, @r1

        mov.l   LED_A, r1
        mov     #0xff, r0
        mov.w   r0, @r1
#if 0
        mov.l   CCR_A, r1
        mov.l   CCR_D, r0
        mov.l   r0, @r1
#endif
#elif defined(CONFIG_SH_MS7720RP)
	mov.l	FRQCR_A,r1	/* FRQCR Address */
	mov.l	FRQCR_D,r0	/* FRQCR Data */
	mov.w	r0,@r1
	
	mov.l	CMNCR_A,r1	/* CMNCR Address */
	mov.l	CMNCR_D,r0	/* CMNCR Data */
	mov.l	r0,@r1

	mov.l	CS0BCR_A,r1	/* CS0BCR Address */
	mov.l	CS0BCR_D,r0	/* CS0BCR Data */
	mov.l	r0,@r1

	mov.l	CS2BCR_A,r1	/* CS2BCR Address */
	mov.l	CS2BCR_D,r0	/* CS2BCR Data */
	mov.l	r0,@r1

	mov.l	CS3BCR_A,r1	/* CS3BCR Address */
	mov.l	CS3BCR_D,r0	/* CS3BCR Data */
	mov.l	r0,@r1

	mov.l	CS4BCR_A,r1	/* CS4BCR Address */
	mov.l	CS4BCR_D,r0	/* CS4BCR Data */
	mov.l	r0,@r1

	mov.l	CS5ABCR_A,r1	/* CS5ABCR Address */
	mov.l	CS5ABCR_D,r0	/* CS5ABCR Data */
	mov.l	r0,@r1

	mov.l	CS5BBCR_A,r1	/* CS5BBCR Address */
	mov.l	CS5BBCR_D,r0	/* CS5BBCR Data */
	mov.l	r0,@r1

	mov.l	CS6ABCR_A,r1	/* CS6ABCR Address */
	mov.l	CS6ABCR_D,r0	/* CS6ABCR Data */
	mov.l	r0,@r1

	mov.l	CS6BBCR_A,r1	/* CS6BBCR Address */
	mov.l	CS6BBCR_D,r0	/* CS6BBCR Data */
	mov.l	r0,@r1

	mov.l	CS0WCR_A,r1	/* CS0WCR Address */
	mov.l	CS0WCR_D,r0	/* CS0WCR Data */
	mov.l	r0,@r1

	mov.l	CS2WCR_A,r1	/* CS2WCR Address */
	mov.l	CS2WCR_D,r0	/* CS2WCR Data */
	mov.l	r0,@r1

	mov.l	CS3WCR_A,r1	/* CS3WCR Address */
	mov.l	CS3WCR_D,r0	/* CS3WCR Data */
	mov.l	r0,@r1

	mov.l	CS4WCR_A,r1	/* CS4WCR Address */
	mov.l	CS4WCR_D,r0	/* CS4WCR Data */
	mov.l	r0,@r1

	mov.l	CS5AWCR_A,r1	/* CS5AWCR Address */
	mov.l	CS5AWCR_D,r0	/* CS5AWCR Data */
	mov.l	r0,@r1

	mov.l	CS5BWCR_A,r1	/* CS5BWCR Address */
	mov.l	CS5BWCR_D,r0	/* CS5BWCR Data */
	mov.l	r0,@r1

	mov.l	CS6AWCR_A,r1	/* CS6AWCR Address */
	mov.l	CS6AWCR_D,r0	/* CS6AWCR Data */
	mov.l	r0,@r1

	mov.l	CS6BWCR_A,r1	/* CS6BWCR Address */
	mov.l	CS6BWCR_D,r0	/* CS6BWCR Data */
	mov.l	r0,@r1

	mov.l	SDCR_A,r1	/* SDCR Address */
	mov.l	SDCR_D,r0	/* SDCR Data */
	mov.l	r0,@r1

	! Do you need PCMCIA setting?
	! If so, please add the lines here...

	mov.l	RTCSR_A,r1	/* RTCSR Address */
	mov.l	RTCSR_D,r0	/* RTCSR Data */
	mov.l	r0,@r1

	mov.l	RTCNT_A,r1	/* RTCNT Address */
	mov.l	RTCNT_D,r0	/* RTCNT Data */
	mov.l	r0,@r1

	mov.l	RTCOR_A,r1	/* RTCOR Address */
	mov.l	RTCOR_D,r0	/* RTCOR Data */
	mov.l	r0,@r1

	mov.l	SDCR_A,r1	/* SDCR Address */
	mov.l	SDCR_D_2,r0	/* SDCR Data_2 */
	mov.l	r0,@r1

	/* Wait DRAM refresh 8 times */
	nop
        mov.l   r14,@-r15
        add     #-4,r15
        mov     r15,r14
        mov     #0,r1
        mov.l   r1,@r14
.L2:
        mov.l   @r14,r2
        mov     #7,r1
        cmp/gt  r1,r2
        bf      .L5
        bra     .L3
        nop
        .align 4
.L5:
.L6:
        mov.l   .L12,r1
        mov.l   @r1,r0
        and     #128,r0
        tst     r0,r0
        bt      .L6
        mov.l   @r1,r2
        mov.l   .L10,r1
        and     r1,r2
        mov.l   .L11,r1
        or      r1,r2
        mov.l   .L12,r1
        mov.l   r2,@r1
        mov.l   @r14,r1
        add     #1,r1
        mov.l   r1,@r14
        bra     .L2
        nop
        .align 4
.L3:
        add     #4,r14
        mov     r14,r15
        mov.l   @r15+,r14
        nop
.L13:
       .align 2

	mov.l	SDMR3_A,r1	/* Set SDRAM mode */
	mov	#0,r0
	mov.w	r0,@r1

	mov.l	PTCR_A,r1	/* Set Port T to SCIF mode */
	mov	#0,r0
	mov.w	r0,@r1
#else
#error Unkown platform	
#endif
	mov.l	VBR_INIT, r0
	ldc	r0, vbr

	mov.l	XSTART_A, r0
	jmp	@r0
	 nop
	.align	2
XSTART_A:	.long	xstart

#if defined(CONFIG_SH_MS7290CP)
LED_A:		.long	0xb0000000
LED_D:		.word	0xff00
		.word	0x0000
	
CCR1_A:		.long	0xffffffec
MMUCR_A:	.long	0xffffffe0
STBCR_A:	.long	0xa415ff82
STBCR2_A:	.long	0xa415ff88
STBCR3_A:	.long	0xa40a0000
STBCR4_A:	.long	0xa40a0004
STBCR5_A:	.long	0xa40a0010
WTCSR_A:	.long	0xa415ff86
WTCNT_A:	.long	0xa415ff84
RWTCSR_A:	.long	0xa4520004
FRQCR_A:	.long	0xa415ff80
UCLKCR_A:	.long	0xa40a0008
FCLKCR_A:	.long	0xa40a000c

CMNCR_A:	.long	0xa4fd0000
CS0BCR_A:	.long	0xa4fd0004
CS2BCR_A:	.long	0xa4fd0008
CS3BCR_A:	.long	0xa4fd000c
CS4BCR_A:	.long	0xa4fd0010
CS5ABCR_A:	.long	0xa4fd0014
CS5BBCR_A:	.long	0xa4fd0018
CS6ABCR_A:	.long	0xa4fd001c
CS6BBCR_A:	.long	0xa4fd0020
CS0WCR_A:	.long	0xa4fd0024
CS2WCR_A:	.long	0xa4fd0028
CS3WCR_A:	.long	0xa4fd002c
CS4WCR_A:	.long	0xa4fd0030
CS5AWCR_A:	.long	0xa4fd0034
CS5BWCR_A:	.long	0xa4fd0038
CS6AWCR_A:	.long	0xa4fd003c
CS6BWCR_A:	.long	0xa4fd0040

CCR1_D:		.long	0x00000008
#if 0
CCR1_D2:	.long	0x00000005
#else
CCR1_D2:	.long	0x00000003
#endif
MMUCR_D:	.long	0x00000004
WTCSR_D:	.word	0xa507
WTCNT_D:	.word	0x5a00
RWTCSR_D:	.word	0xa504
FRQCR_D:	.word	0x1404
UCLKCR_D:	.word	0xa5c0
FCLKCR_D:	.word	0x5a41
SR_D:		.long	0x400000f0

CMNCR_D:	.long	0x00000013	   
CS0BCR_D:	.long	0x12410400	   
CS2BCR_D:	.long	0x36db0400	   
CS3BCR_D:	.long	0x12410400	   
CS4BCR_D:	.long	0x12410400	   
CS5ABCR_D:	.long	0x36db0400	   
CS5BBCR_D:	.long	0x36db0400	   
CS6ABCR_D:	.long	0x12410400	   
CS6BBCR_D:	.long	0x12410400	   
CS0WCR_D:	.long	0x00000180
CS2WCR_D:	.long	0x00000500
CS3WCR_D:	.long	0x00000200
CS4WCR_D:	.long	0x00000180
CS5AWCR_D:	.long	0x00000500
CS5BWCR_D:	.long	0x00000500
CS6AWCR_D:	.long	0x00000180
CS6BWCR_D:	.long	0x00000180
#else
#if defined(CONFIG_CPU_SH3)
#if defined(CONFIG_SH_MS7720RP)
FRQCR_A:	.long	0xA415FF80
CMNCR_A:	.long	0xA4FD0000
CS0BCR_A:	.long	0xA4FD0004
CS2BCR_A:	.long	0xA4FD0008
CS3BCR_A:	.long	0xA4FD000C
CS4BCR_A:	.long	0xA4FD0010
CS5ABCR_A:	.long	0xA4FD0014
CS5BBCR_A:	.long	0xA4FD0018
CS6ABCR_A:	.long	0xA4FD001C
CS6BBCR_A:	.long	0xA4FD0020
CS0WCR_A:	.long	0xA4FD0024
CS2WCR_A:	.long	0xA4FD0028
CS3WCR_A:	.long	0xA4FD002C
CS4WCR_A:	.long	0xA4FD0030
CS5AWCR_A:	.long	0xA4FD0034
CS5BWCR_A:	.long	0xA4FD0038
CS6AWCR_A:	.long	0xA4FD003C
CS6BWCR_A:	.long	0xA4FD0040
SDCR_A:		.long	0xA4FD0044
RTCSR_A:	.long	0xA4FD0048
RTCNT_A:	.long	0xA4FD004C
RTCOR_A:	.long	0xA4FD0050
SDMR3_A:	.long	0xA4FD58C0
PTCR_A:		.long	0xA405011E
#elif defined(CONFIG_SH_MS7710SE)
FRQCR_A:        .long   0xA415FF80
CMNCR_A:        .long   0xA4FD0000
CS0BCR_A:       .long   0xA4FD0004
CS2BCR_A:       .long   0xA4FD0008
CS3BCR_A:       .long   0xA4FD000C
CS4BCR_A:       .long   0xA4FD0010
CS5ABCR_A:      .long   0xA4FD0014
CS5BBCR_A:      .long   0xA4FD0018
CS6ABCR_A:      .long   0xA4FD001C
CS6BBCR_A:      .long   0xA4FD0020
CS0WCR_A:       .long   0xA4FD0024
CS2WCR_A:       .long   0xA4FD0028
CS3WCR_A:       .long   0xA4FD002C
CS4WCR_A:       .long   0xA4FD0030
CS5AWCR_A:      .long   0xA4FD0034
CS5BWCR_A:      .long   0xA4FD0038
CS6AWCR_A:      .long   0xA4FD003C
CS6BWCR_A:      .long   0xA4FD0040
SDCR_A:         .long   0xA4FD0044
RTCSR_A:        .long   0xA4FD0048
RTCNT_A:        .long   0xA4FD004C
RTCOR_A:        .long   0xA4FD0050
SDMR3_A:        .long   0xA4FD58C0
PTCR_A:         .long   0xA405011E
#else
CCR_A:		.long	0xFFFFFFEC
FRQCR_A:	.long	0xFFFFFF80
BCR1_A:		.long	0xFFFFFF60
BCR2_A:		.long	0xFFFFFF62
WCR1_A:		.long	0xFFFFFF64
WCR2_A:		.long	0xFFFFFF66
MCR_A:		.long	0xFFFFFF68
DCR_A:		.long	0xFFFFFF6A
RTCSR_A:	.long	0xFFFFFF6E
RTCNT_A:	.long	0xFFFFFF70
RTCOR_A:	.long	0xFFFFFF72
RFCR_A:		.long	0xFFFFFF74
SDMR3_A:	.long	0xFFFFE080
#endif
#if defined(CONFIG_SH_MS7720RP)
FRQCR_D:	.long	0x1103
CMNCR_D:	.long	0x00000018
CS0BCR_D:	.long	0x36db0400
CS2BCR_D:	.long	0x36db0400
CS3BCR_D:	.long	0x36db4600
CS4BCR_D:	.long	0x36db0400
CS5ABCR_D:	.long	0x36db0400
CS5BBCR_D:	.long	0x36db0200
CS6ABCR_D:	.long	0x36db0400
CS6BBCR_D:	.long	0x36db0400
CS0WCR_D:	.long	0x00000500
CS2WCR_D:	.long	0x00000500
CS3WCR_D:	.long	0x00006D1B
CS4WCR_D:	.long	0x00000500
CS5AWCR_D:	.long	0x00000500
CS5BWCR_D:	.long	0x00000500
CS6AWCR_D:	.long	0x00000500
CS6BWCR_D:	.long	0x00000500
SDCR_D:		.long	0x00000011
RTCSR_D:	.long	0xa55a0010
RTCNT_D:	.long	0xa55a001f
RTCOR_D:	.long	0xa55a001f
SDCR_D_2:	.long	0x00000811
.L12:		.long	-1526923192
.L10:		.long	65407
.L11:		.long	-1520828416
#elif defined(CONFIG_SH_MS7710SE)
FRQCR_D:        .long   0x1204
CMNCR_D:        .long   0x00009018
CS0BCR_D:       .long   0x24920000
CS2BCR_D:       .long   0x36db0600
CS3BCR_D:       .long   0x12484600
CS4BCR_D:       .long   0x24920400
CS5ABCR_D:      .long   0x36db0400 /* ??? */
CS5BBCR_D:      .long   0x36db0600
CS6ABCR_D:      .long   0x36db0400 /* ??? */
CS6BBCR_D:      .long   0x12490400
CS0WCR_D:       .long   0x00000380
CS2WCR_D:       .long   0x00000500
CS3WCR_D:       .long   0x00002509
CS4WCR_D:       .long   0x00000380
CS5AWCR_D:      .long   0x00000500 /* ??? */
CS5BWCR_D:      .long   0x00000500
CS6AWCR_D:      .long   0x00000180 /* ??? */
CS6BWCR_D:      .long   0x00000180
SDCR_D:         .long   0x00000809
RTCSR_D:        .long   0xa55a0014
RTCNT_D:        .long   0xa55a0000
RTCOR_D:        .long   0xa55a003a
SDCR_D_2:       .long   0x00000811 /* ??? */
.L12:           .long   -1526923192
.L10:           .long   65407
.L11:           .long   -1520828416
#elif defined(CONFIG_SH_SOLUTION_ENGINE)
CCR_DI:		.long	0x0008
CCR_D:		.long	0x0000
#if defined(CONFIG_CPU_SUBTYPE_SH7709S)
FRQCR_D:	.long	0xA101
BCR1_D:		.long	0x0008
BCR2_D:		.long	0x2EF0
BCR3_D:		.long	0x0000
WCR1_D:		.long	0x0C30
WCR2_D:		.long	0xBFDD
#else
FRQCR_D:	.long	0x0122
BCR1_D:		.long	0x0008
BCR2_D:		.long	0x2EF0
BCR3_D:		.long	0x0000
WCR1_D:		.long	0x0F30
WCR2_D:		.long	0x7FDB
#endif
MCR_D:		.long	0x002C
DCR_D:		.long	0x0000
RTCSR_D:	.long	0xA508
RTCNT_D:	.long	0xA500
RTCOR_D:	.long	0xA580
RFCR_D:		.long	0xA400
#elif defined(CONFIG_SH_MS7727RP)
CCR_DI:		.long	0x0008
CCR_D:		.long	0x0000
FRQCR_D:	.long	0x0112
BCR1_D:		.long	0x0008
BCR2_D:		.long	0x2FF0
WCR1_D:		.long	0xAF32
WCR2_D:		.long	0xBFDD
MCR_D:		.long	0x512C
RTCSR_D:	.long	0xA518
RTCNT_D:	.long	0xA500
RTCOR_D:	.long	0xA50C
RFCR_D:		.long	0xA400
#elif defined(CONFIG_SH_PFMDS11)
CCR_DI:		.long	0x0008
CCR_D:		.long	0x0000
FRQCR_D:	.long	0x0112
BCR1_D:		.long	0x0008
BCR2_D:		.long	0x2FF0
WCR1_D:		.long	0xAF32
WCR2_D:		.long	0xBFDD
MCR_D:		.long	0x512C
RTCSR_D:	.long	0xA518
RTCNT_D:	.long	0xA500
RTCOR_D:	.long	0xA50C
RFCR_D:		.long	0xA400
#else
#error unknown platform for SH3
#endif
#elif defined(CONFIG_CPU_SH4)
FRQCR_A:	.long	0xFFC00000
BCR1_A:		.long	0xFF800000
BCR2_A:		.long	0xFF800004
BCR3_A:         .long   0xFF800050
BCR4_A:         .long   0xFE0A00F0
CCR_A:		.long	0xFF00001C
WCR1_A:		.long	0xFF800008
WCR2_A:		.long	0xFF80000C
WCR3_A:		.long	0xFF800010
WCR4_A:         .long   0xFE0A0028
RTCNT_A:	.long	0xFF800020
RTCOR_A:	.long	0xFF800024
RTCSR_A:	.long	0xFF80001C
#if defined(CONFIG_SH_RTS7751R2D)
SDMR3_A:	.long	0xFF9400cc
#else
#if defined(CONFIG_SH_7760_SOLUTION_ENGINE)
SDMR3_A:        .long   0xFF94008C
#else
SDMR3_A:	.long	0xFF940110
#endif
#endif
MCR_A:		.long	0xFF800014
PCR_A:		.long	0xFF800018
RFCR_A:		.long	0xFF800028
PCTRA_A:	.long	0xFF80002C
PCTRB_A:	.long	0xFF800040
GPIOC_A:	.long	0xFF800048
#if defined(CONFIG_SH_7760_SOLUTION_ENGINE)
LED_A:          .long   0xA1400000
#endif
#if defined(CONFIG_SH_SOLUTION_ENGINE)
FRQCR_D:	.long	0x00000E13
BCR1_D:		.long	0x00000008
BCR2_D:		.long	0x2E3C
CCR_DI:		.long	0x0008
CCR_D:		.long	0x0000
WCR1_D:		.long	0x02720777
WCR2_D:		.long	0xFFFE4FFF
WCR3_D:		.long	0x01717771
RTCNT_D:	.long	0xA500
RTCOR_D:	.long	0xA510
RTCSR_D:	.long	0xA518
MCR_D1:		.long	0x8801001C
MCR_D2:		.long	0xC801001C
RFCR_D:		.long	0xA400
#elif defined(CONFIG_SH_7760_SOLUTION_ENGINE)
#if defined(CONFIG_SH_7760_SOLUTION_ENGINE_NB)
FRQCR_D:        .long   0x00000E13
BCR1_D:         .long   0x00080009
BCR2_D:         .long   0xAAE8
BCR3_D:         .long   0x0000
BCR4_D:         .long   0x00000000
WCR1_D:         .long   0x72121222
WCR2_D:         .long   0x7DFE46E7
WCR3_D:         .long   0x01777111
WCR4_D:         .long   0x00000001
PCR_D:          .long   0x0408
RTCNT_D:        .long   0xA500
RTCOR_D:        .long   0xA50D
RTCSR_D0:       .long   0xA508
RTCSR_D1:       .long   0xA518
MCR_D0:         .long   0x080941F4
MCR_D1:         .long   0x480941F4
RFCR_D:         .long   0xA400
CCR_DI:         .long   0x0808
CCR_D:          .long   0x0103
#else
FRQCR_D:        .long   0x00000E13
BCR1_D:         .long   0x00080008
BCR2_D:         .long   0xAAD8
BCR3_D:         .long   0x0000
BCR4_D:         .long   0x00000000
WCR1_D:         .long   0x72771722
WCR2_D:         .long   0x7FFE4EE7
WCR3_D:         .long   0x01777711
WCR4_D:         .long   0x00000001
PCR_D:          .long   0x0000
RTCNT_D:        .long   0xA500
RTCOR_D:        .long   0xA50D
RTCSR_D0:       .long   0xA508
RTCSR_D1:       .long   0xA518
MCR_D0:         .long   0x080941F4
MCR_D1:         .long   0x480941F4
RFCR_D:         .long   0xA400
CCR_DI:         .long   0x0808
CCR_D:          .long   0x0103
#endif
#elif defined(CONFIG_SH_RTS7751R2D)
FRQCR_D:	.long	0x00000e0a	/* 03/07/15 modify */
BCR1_D:		.long	0x00180008	/* 2003/07/15 modify */
BCR2_D:		.long	0xabe8		/* 2003/06/05 modify */
BCR3_A:		.long	0xFF800050	/* BCR3 Address */
BCR3_D:		.long	0x0000		/*  */
BCR4_A:		.long	0xFE0A00F0	/* BCR4 Address */
BCR4_D:		.long	0x00000010	/* 2003/06/05 modify */
WCR1_D:		.long	0x33333333
WCR2_D:		.long	0xcff86fbf	/* 03/06/05 modify */ 
WCR3_D:		.long	0x07777707	/* 03/07/15 modify */
LED_A:		.long	0x04000036	/* LED Address */
RTCNT_D:	.long	0xA500		/* RTCNT Write Code A5h Data 00h */
RTCOR_D:	.long	0xA534		/* RTCOR Write Code  */
RTCSR_D:	.long	0xA510		/* RTCSR Write Code */
SMDR3_D:	.long	0x55
MCR_D1:		.long	0x081901F4	/* MRSET:'0' */
MCR_D2:		.long	0x481901F4	/* MRSET:'1' */
RFCR_D:		.long	0xA400		/* RFCR Write Code A4h Data 00h */
PCR_D:		.long	0x0000		/* 03/07/15 modify */
MMUCCR_A:	.long	0xff000010	/* MMUCCR Address */
MMUCCR_D:	.long	0x00000000	/* MMUCCR Data */
IRLMASK_A:	.long	0xa4000004	/* IRLMASK Address */
IRLMASK_D:	.long	0x00000000	/* IRLMASK Data */
WAITSET_A:	.long	0x08000002
WAITSET_D:	.long	0x00000001

FPGA_CFCTL_A:		.long	0xa4000004	/* FPGA_CFCTL Address */
FPGA_CFCTL_D:		.long	0x0432		/* FPGA_CFCTL Data */
FPGA_CFPOW_A:		.long	0xa4000006	/* FPGA_CFPOW Address */
FPGA_CFPOW_D:		.long	0x0002		/* FPGA_CFPOW Data */
FPGA_CFCDINTCLR_A:	.long	0xa400002a	/* FPGA_CFCDINTCLR Address */
FPGA_CFCDINTCLR_D:	.long	0x0001		/* FPGA_CFCDINTCLR Data */
FPGA_IRLMON_A:		.long	0xa4000002	/* FPGA_IRLMON Address */
FPGA_CF_CDINT:		.long	0x0100		/* CF_CDINT_MON */

BOOT_CONFIG_WORD:
	.long	0xb0800000
	
#else
#error unknown platform for SH4
#endif
#else	
#error unknown CPU
#endif
#endif

VBR_INIT:
	.long	_stext

	.align	2
xstart:
#endif
	!			Initialize Status Register
	mov.l	1f, r0		! MD=1, RB=0, BL=0, IMASK=0xF
	ldc	r0, sr
	!			Initialize global interrupt mask
	mov	#0, r0
	ldc	r0, r6_bank
	!
	mov.l	2f, r0
	mov	r0, r15		! Set initial r15 (stack pointer)
	mov	#0x20, r1	!
	shll8	r1		! r1 = 8192
	sub	r1, r0		!
	ldc	r0, r7_bank	! ... and init_task
	!
#if defined(CONFIG_XIP_KERNEL)
#if defined(CONFIG_SH_7760_SOLUTION_ENGINE)
        !                       Copy xip data
        mov.l   ex, r0          ! _extext
        mov.l   te, r1          ! _text
        mov.l   3f, r2          ! _bss_start
#else
	!                       Copy xip data
	mov.l   et, r0		! _etext
	mov.l   da, r1		! _data
	mov.l	3f, r2		! __bss_start
#endif
9:
	mov.l	@r0, r3
	mov.l	r3, @r1
	add	#4, r0
	add	#4, r1
	cmp/hs	r2, r1
	bf/s	9b		! while (r1 < r2)
	 nop
#endif
	!
#if defined(__SH4__)
	!                       Initialize fpu
	mov.l   7f, r0
	jsr     @r0
	 nop
#endif
	!			Enable cache
	mov.l	6f, r0
	jsr	@r0
	 nop
	!			Clear BSS area
	mov.l	3f, r1
	add	#4, r1
	mov.l	4f, r2
	mov	#0, r0
9:	cmp/hs	r2, r1
	bf/s	9b		! while (r1 < r2)
	 mov.l	r0,@-r2
	!			Start kernel
	mov.l	5f, r0
	jmp	@r0
	 nop

	.balign 4
1:	.long	0x400000F0		! MD=1, RB=0, BL=0, FD=0, IMASK=0xF
2:	.long	SYMBOL_NAME(stack)
3:	.long	SYMBOL_NAME(__bss_start)
4:	.long	SYMBOL_NAME(_end)
5:	.long	SYMBOL_NAME(start_kernel)
6:	.long	SYMBOL_NAME(cache_init)
#if defined(__SH4__)
7:	.long   SYMBOL_NAME(fpu_init)
#endif
#if defined(CONFIG_XIP_KERNEL)
#if defined(CONFIG_SH_7760_SOLUTION_ENGINE)
ex:     .long   SYMBOL_NAME(_extext)
te:     .long   SYMBOL_NAME(_text)
#else
et:	.long	SYMBOL_NAME(_etext)
da:	.long	SYMBOL_NAME(_data)
#endif
#endif
