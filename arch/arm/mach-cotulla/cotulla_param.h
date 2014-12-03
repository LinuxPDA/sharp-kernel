/* cotulla_parm.h:	sleep parameter for discovery
 *
 * Copyright (C) 2002 Steve Lin (stevelin168@hotmail.com)
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 * May be copied or modified under the terms of the GNU General Public
 * License.  See linux/COPYING for more information.
 *
 */


#define WORD_SIZE       			  (4)
#define SleepState_Data_Start		  (0)

#define SleepState_WakeAddr    		  (SleepState_Data_Start)
#define SleepState_MMUCTL             (SleepState_WakeAddr    + WORD_SIZE )
#define SleepState_MMUTTB       	  (SleepState_MMUCTL  	+ WORD_SIZE )
#define SleepState_MMUDOMAIN    	  (SleepState_MMUTTB  	+ WORD_SIZE )
#define SleepState_SVC_SP       	  (SleepState_MMUDOMAIN   + WORD_SIZE )
#define SleepState_SVC_SPSR     	  (SleepState_SVC_SP  	+ WORD_SIZE )
#define SleepState_FIQ_SPSR     	  (SleepState_SVC_SPSR    + WORD_SIZE )
#define SleepState_FIQ_R8       	  (SleepState_FIQ_SPSR    + WORD_SIZE )
#define SleepState_FIQ_R9       	  (SleepState_FIQ_R8  	+ WORD_SIZE )
#define SleepState_FIQ_R10      	  (SleepState_FIQ_R9  	+ WORD_SIZE )
#define SleepState_FIQ_R11      	  (SleepState_FIQ_R10 	+ WORD_SIZE )
#define SleepState_FIQ_R12      	  (SleepState_FIQ_R11 	+ WORD_SIZE )
#define SleepState_FIQ_SP       	  (SleepState_FIQ_R12 	+ WORD_SIZE )
#define SleepState_FIQ_LR       	  (SleepState_FIQ_SP  	+ WORD_SIZE )
#define SleepState_ABT_SPSR     	  (SleepState_FIQ_LR  	+ WORD_SIZE )
#define SleepState_ABT_SP       	  (SleepState_ABT_SPSR    + WORD_SIZE )
#define SleepState_ABT_LR       	  (SleepState_ABT_SP  	+ WORD_SIZE )
#define SleepState_IRQ_SPSR     	  (SleepState_ABT_LR  	+ WORD_SIZE )
#define SleepState_IRQ_SP       	  (SleepState_IRQ_SPSR    + WORD_SIZE )
#define SleepState_IRQ_LR       	  (SleepState_IRQ_SP  	+ WORD_SIZE )
#define SleepState_UND_SPSR     	  (SleepState_IRQ_LR  	+ WORD_SIZE )
#define SleepState_UND_SP       	  (SleepState_UND_SPSR    + WORD_SIZE )
#define SleepState_UND_LR       	  (SleepState_UND_SP  	+ WORD_SIZE )
#define SleepState_SYS_SP       	  (SleepState_UND_LR  	+ WORD_SIZE )
#define SleepState_SYS_LR       	  (SleepState_SYS_SP  	+ WORD_SIZE )

#define SleepState_GAFR0_X     		  (SleepState_SYS_LR	  	+ WORD_SIZE )
#define SleepState_GAFR1_X     		  (SleepState_GAFR0_X	  	+ WORD_SIZE )
#define SleepState_GAFR0_Y     		  (SleepState_GAFR1_X	  	+ WORD_SIZE )
#define SleepState_GAFR1_Y     		  (SleepState_GAFR0_Y	  	+ WORD_SIZE )
#define SleepState_GAFR0_Z     		  (SleepState_GAFR1_Y	  	+ WORD_SIZE )
#define SleepState_GAFR1_Z     		  (SleepState_GAFR0_Z	  	+ WORD_SIZE )

#define SleepState_GFER_X     		  (SleepState_GAFR1_Z    	+ WORD_SIZE )
#define SleepState_GFER_Y     		  (SleepState_GFER_X    	+ WORD_SIZE )
#define SleepState_GFER_Z     		  (SleepState_GFER_Y    	+ WORD_SIZE )

#define SleepState_GRER_X     		  (SleepState_GFER_Z    	+ WORD_SIZE )
#define SleepState_GRER_Y     		  (SleepState_GRER_X    	+ WORD_SIZE )
#define SleepState_GRER_Z     		  (SleepState_GRER_Y    	+ WORD_SIZE )

#define SleepState_OSMR0        	  (SleepState_GRER_Z    	+ WORD_SIZE )
#define SleepState_OSMR1        	  (SleepState_OSMR0   	+ WORD_SIZE )
#define SleepState_OSMR2        	  (SleepState_OSMR1   	+ WORD_SIZE )
#define SleepState_OSMR3        	  (SleepState_OSMR2   	+ WORD_SIZE )
#define SleepState_OSCR     		  (SleepState_OSMR3    	+ WORD_SIZE )
#define SleepState_OSSR     		  (SleepState_OSCR    	+ WORD_SIZE )
#define SleepState_OWER     		  (SleepState_OSSR	   	+ WORD_SIZE )
#define SleepState_OIER     		  (SleepState_OWER	   	+ WORD_SIZE )
#define SleepState_RTTR     		  (SleepState_OIER    	+ WORD_SIZE )
#define SleepState_RTSR     		  (SleepState_RTTR    	+ WORD_SIZE )
#define SleepState_ICMR     		  (SleepState_RTSR    	+ WORD_SIZE )

#define SleepState_Data_End     	  (SleepState_ICMR		+ WORD_SIZE	)

#define SLEEPDATA_SIZE		    	  (SleepState_Data_End-SleepState_Data_Start)/4
