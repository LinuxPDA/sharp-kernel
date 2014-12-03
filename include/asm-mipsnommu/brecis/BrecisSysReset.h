/****************************************************************************/
/*                                                                          */ 
/*   Title:   sysReset.h                                                    */
/*                                                                          */
/*  Author:  Phil Le                                                        */
/*                                                                          */
/*  Date:    09/10/00.                                                      */
/*                                                                          */
/****************************************************************************/
/*   $Header:																*/
/*===========================================================================*/
/*                                                                           */
/*  Description:                                                             */
/*    This file defines Triad architecture specific constants.               */
/*    add all register definetion and marco for all register - Phil Le	     */
/*                                                                           */
/*  Special Notes:                                                           */
/*                                                                           */
/*                                                                           */
/*****************************************************************************/
#ifndef _BRECIS_SYS_RESET_H
#define _BRECIS_SYS_RESET_H
/*****************************************************************************/
/* System reset register definetion											 */
/* System Reset Status register												 */
/*																			 */
/*  31-------10 9  8  7  6  5  4  3  2  1  0								 */
/*Reserved      |  |  |  |  |  |  |  |  |  |								 */
/*	           |  |  |  |  |  |  |  |  |  ----> Global Reset				 */
/* 	          |  |  |  |  |  |  |  |  --> Mips Processor SLM And SDRAM		 */
/* 	         |  |  |  |  |  |  |  --> Voice Engine DMA						 */
/* 	        |  |  |  |  |  |  --> Voice Engine Processor					 */
/* 	       |  |  |  |  |  --> Framer Engine DMA								 */
/* 	      |  |  |  |  ----> Framer Engine Processor							 */
/* 	     |  |  |  --------> Ethernet 0										 */
/* 	    |  |  ------------> Ethernet 1										 */
/*     |   ---------------> Security engine									 */
/* 	   -------------------> Peripheral Block								 */
/*****************************************************************************/
#define	GLOBAL_RST	0x1						/* Global Reset */
#define MIPS_RST	0x2						/* MIPS reset   */
#define VE_DMA_RST	0x4						/* Voice Engine DMA reset  */
#define VE_CPU_RST	0x8						/* Voice Processor Reset   */
#define FE_DMA_RST	0x10					/* Framer Engine DMA reset */
#define FE_CPU_RST	0x20					/* Framer Engine Processor */
#define MAC0_RST	0x40					/* Ethernet 0 Reset */
#define MAC1_RST	0x80					/* Ethernet 1 Reset */
#define SE_RST		0x100					/* Security Engine Reset */
#define PER_RST		0x200					/* Peripheral Block Reset */


#ifdef __ASSEMBLER__
/*------------------------------*/
/* x = value			*/
/* t0 = register address        */
/* t1 = value or return value 	*/
/*------------------------------*/
#define WRITE_RESET(x)	\
	li	t0,RST_SET_REG;	\
	move	t1,x;		\
	sw	t1,0(t0)	

#define WRITE_RESET_CLEAR(x)	\
	li	t0,RST_CLR_REG;	\
	move	t1,x;		\
	sw	t1,0(t0)	

#define READ_RESET_STATUS	\
	li	t0,SYS_RST_REG;	\
	lw	t1,0(t0)	
#else

#define WRITE_RESET(x)		     (*RST_SET_REG = x) 
#define WRITE_RESET_CLEAR(x)	 (*RST_CLR_REG = x)  
#define READ_RESET_STATUS()	     (*SYS_RST_REG)

#endif

#endif /* !(_BRECIS_SYS_RESET_H) */
