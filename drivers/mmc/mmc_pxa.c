/*
 *  linux/drivers/mmc/mmc_pxa.c 
 *      driver for Cotulla MMC controller 
 *
 *  Authors:    Vladimir Shebordaev, Igor Oblakov   
 *  Copyright:  MontaVista Software Inc.
 *
 *  $Id: mmc_pxa.c,v 0.3.1.12 2002/09/25 19:25:48 ted Exp ted $
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License version 2 as
 *  published by the Free Software Foundation.
 */
#include <linux/version.h>
#include <linux/config.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/errno.h>

#include <linux/slab.h>
#include <linux/sched.h>
#include <linux/delay.h>

#include <asm/hardware.h>
#include <asm/io.h>
#include <asm/irq.h>
#include <asm/dma.h>

#include <asm/uaccess.h>
#include <asm/semaphore.h>

#include <mmc/types.h>
#include <mmc/mmc.h>
#include <mmc/ioctl.h>

#include "types.h"
#include "mmc.h"
#include "mmc_pxa.h"

static mmc_controller_t host = NULL;

/* service routines */
static inline int __pxa_mmc_check_state( mmc_controller_t ctrlr, pxa_mmc_state_t state )
{
    int ret = -1;
    pxa_mmc_hostdata_t hostdata = (pxa_mmc_hostdata_t)ctrlr->host_data;
    
    if ( hostdata->state != state ) {
        MMC_DEBUG( MMC_DEBUG_LEVEL3, 
                "state (%s vs %s)\n", 
                PXA_MMC_STATE_LABEL( hostdata->state ), 
                PXA_MMC_STATE_LABEL( state ) );
        goto error;
    }
    ret = 0;
error:          
    return ret;
}

static inline int __pxa_mmc_set_state( mmc_controller_t ctrlr, pxa_mmc_state_t state )
{
	int ret = -1;
	pxa_mmc_hostdata_t hostdata = (pxa_mmc_hostdata_t)ctrlr->host_data;
    
	MMC_DEBUG( MMC_DEBUG_LEVEL3, 
		"state changed from %s to %s\n", 
		PXA_MMC_STATE_LABEL( hostdata->state ), 
		PXA_MMC_STATE_LABEL( state ) );
	hostdata->state = state;
   
	ret = 0;
	return ret;
}

static inline int pxa_mmc_init_completion( mmc_controller_t ctrlr, u32 mask )
{
	int ret = -1;
	pxa_mmc_hostdata_t hostdata = (pxa_mmc_hostdata_t)ctrlr->host_data;
    
	__ENTER0();
    
	if ( xchg( &hostdata->busy, 1 ) ) {
		MMC_DEBUG( MMC_DEBUG_LEVEL3, "another interrupt "
			"is already been expected\n" );
		goto error;
	}
    
#if CONFIG_MMC_DEBUG_IRQ 
	hostdata->irqcnt = 1000;
#endif
	init_completion( &hostdata->completion );
    
	MMC_I_MASK = MMC_I_MASK_ALL & ~mask;
	ret = 0;
error:
	__LEAVE( "ret=%d", ret );
	return ret;
}

#if CONFIG_MMC_DEBUG_IRQ 
static struct timer_list timer;
static void wait_timeo( unsigned long arg ) {
	pxa_mmc_hostdata_t hostdata = (pxa_mmc_hostdata_t)arg;
	hostdata->timeo = 1;
	complete( &hostdata->completion );
	return;
}
#endif

static inline int pxa_mmc_wait_for_completion( mmc_controller_t ctrlr, u32 mask )
{
	int ret = -1;
	pxa_mmc_hostdata_t hostdata = (pxa_mmc_hostdata_t)ctrlr->host_data;
    
	 __ENTER0();

	if ( !xchg( &hostdata->busy, 1 ) ) {
        	MMC_DEBUG( MMC_DEBUG_LEVEL3, "there were no "
				"interrupt awaited for\n" );
		goto error;
	}

	MMC_DEBUG( MMC_DEBUG_LEVEL3, "mask=%x stat=%x\n", mask, MMC_STAT );
#if CONFIG_MMC_DEBUG_IRQ
    	hostdata->timeo = 0;
    	del_timer( &timer );
	timer.function = wait_timeo;
	timer.expires = jiffies + 1UL*HZ;
	timer.data = (unsigned long)hostdata;
	add_timer( &timer );
#endif
	wait_for_completion( &hostdata->completion );
#if CONFIG_MMC_DEBUG_IRQ
    	del_timer( &timer );
	if ( hostdata->timeo ) {
		MMC_DEBUG( MMC_DEBUG_LEVEL3, "irq timed out: "
				"mask=%x stat=%x\n", mask, MMC_STAT );
		goto error;
	}
#endif	
/*  verify interrupt */
	if ( (mask == ~0UL) || !( hostdata->mmc_i_reg & ~mask ) ) 
		ret = 0;
    
error:
	xchg( &hostdata->busy, 0 ); 
	MMC_DEBUG( MMC_DEBUG_LEVEL3, "mask=0x%x, stat=%x\n", mask, MMC_STAT );
	__LEAVE( 
//	printk( KERN_INFO __FUNCTION__"(): "
		"ret=%d", ret );
	return ret;
}

static inline int pxa_mmc_stop_bus_clock( mmc_controller_t ctrlr )
{
	int ret = -1;
    
	if ( !__pxa_mmc_check_state( ctrlr, PXA_MMC_FSM_CLK_OFF ) )
		goto out;
		
	if ( !__pxa_mmc_check_state( ctrlr, PXA_MMC_FSM_BUFFER_IN_TRANSIT ) ) {
		MMC_DEBUG( MMC_DEBUG_LEVEL3, "BUFFER_IN_TRANSIT\n" );
	        goto error;
	}
	
	if ( pxa_mmc_init_completion( ctrlr, MMC_I_MASK_CLK_IS_OFF ) )
		goto error;
    
	MMC_STRPCL = MMC_STRPCL_STOP_CLK;
	
	if ( pxa_mmc_wait_for_completion( ctrlr, MMC_I_REG_CLK_IS_OFF ) )
		goto error;
    
	MMC_DEBUG( MMC_DEBUG_LEVEL3, "clock is off\n" );
	if ( __pxa_mmc_set_state( ctrlr, PXA_MMC_FSM_CLK_OFF ) )
		goto error;
out:
	ret = 0;
error:
	__LEAVE( "ret=%d", ret );
	return ret;
}

static inline int pxa_mmc_start_bus_clock( mmc_controller_t ctrlr )
{
	int ret = -1;
	pxa_mmc_hostdata_t hostdata = (pxa_mmc_hostdata_t)ctrlr->host_data;    

	if ( (hostdata->state != PXA_MMC_FSM_CLK_OFF)
	     && (hostdata->state != PXA_MMC_FSM_END_IO) ) {
		MMC_DEBUG( MMC_DEBUG_LEVEL3, "illegal state %s\n",
				PXA_MMC_STATE_LABEL( hostdata->state ) );
		goto error;
	}
    
	MMC_STRPCL = MMC_STRPCL_START_CLK;
	wmb();
	MMC_DEBUG( MMC_DEBUG_LEVEL3, "clock is on\n" ); 
	ret = 0;
error:
	return ret;
}

/* 
int pxa_mmc_complete_cmd( mmc_controller_t ctrlr, mmc_response_fmt_t response )

Effects: initializes completion to wait for END_CMD_RES intr,
         waits for intr to occur, checks controller and card status 
Requiers: controller is in CLK_OFF state
Modifies: moves controller to the END_CMD state
Returns: 
*/ 
static mmc_error_t pxa_mmc_complete_cmd( mmc_controller_t ctrlr, mmc_response_fmt_t format, int send_abort )
{
	mmc_error_t ret = MMC_ERROR_GENERIC;
	pxa_mmc_hostdata_t hostdata = (pxa_mmc_hostdata_t)ctrlr->host_data;
	int mask, nwords;
	u32 status;
    
	__ENTER0();
/* FIXME: check arguments */
    
	if ( (hostdata->state != PXA_MMC_FSM_CLK_OFF)
	     && (hostdata->state != PXA_MMC_FSM_END_IO) ) {
		MMC_DEBUG( MMC_DEBUG_LEVEL3, "illegal state %s\n",
				PXA_MMC_STATE_LABEL( hostdata->state ) );
		goto error;
	}
    
	mask = MMC_I_MASK_END_CMD_RES;
	if ( pxa_mmc_init_completion( ctrlr, mask ) )
		goto error;
    
	MMC_PRTBUF = MMC_PRTBUF_BUF_FULL; 
/* start the clock */
	if ( pxa_mmc_start_bus_clock( ctrlr ) )
		goto error;
    
/* wait for END_CMD_RES intr */
	if ( pxa_mmc_wait_for_completion( ctrlr, MMC_I_REG_END_CMD_RES ) )
		goto error;

/* check status */
	if ( hostdata->mmc_stat & MMC_STAT_TIME_OUT_RESPONSE ) {
		ret = MMC_ERROR_TIME_OUT_RESPONSE;
		goto error;
    
	} else if ( hostdata->mmc_stat & MMC_STAT_READ_TIME_OUT ) {
		ret = MMC_ERROR_READ_TIME_OUT;
		goto error;
    
	} else if ( hostdata->mmc_stat & MMC_STAT_RES_CRC_ERROR ) {
		ret = MMC_ERROR_RES_CRC_ERROR;
		goto error;
    
	} else if ( hostdata->mmc_stat & MMC_STAT_CRC_READ_ERROR ) {
		ret = MMC_ERROR_CRC_READ_ERROR;
		goto error;
        
	} else if ( hostdata->mmc_stat & MMC_STAT_CRC_WRITE_ERROR ) {
		ret = MMC_ERROR_CRC_WRITE_ERROR;
		goto error;
	}
    
	nwords = (format == MMC_NORESPONSE) ? 0 :
		(format == MMC_R1) ? 3 : 
		(format == MMC_R2) ? 8 :
		(format == MMC_R3) ? 3 : 
		-1;
	MMC_DEBUG( MMC_DEBUG_LEVEL4, "nwords=%d\n", nwords );
	ret = nwords;
	if ( nwords > 0 ) {
		register int i;

		for ( i = nwords - 1; i >= 0 ; i-- ) {
			u32 res = MMC_RES;
			int ibase = i<<1;

			hostdata->mmc_res[ibase] = ((u8 *)&res)[0];
			hostdata->mmc_res[ibase + 1] = ((u8 *)&res)[1];
			--ret;
		}
#ifdef CONFIG_MMC_DEBUG
		switch ( format ) {
		case MMC_R1:
			MMC_DUMP_R1( ctrlr );
			break;
		case MMC_R2:
			MMC_DUMP_R2( ctrlr );
			break;
		case MMC_R3:
			MMC_DUMP_R3( ctrlr );
			break;
		default:
			MMC_DEBUG( MMC_DEBUG_LEVEL3, 
					"unknown response format\n" );
			ret = MMC_ERROR_GENERIC;
			goto error;
		}
#endif

/* check card status for R1(b) commands */
		if ( format == MMC_R1 ) {
			u8 cmd;
			
			((u8 *)&status)[0] = hostdata->mmc_res[1];
			((u8 *)&status)[1] = hostdata->mmc_res[2];
			((u8 *)&status)[2] = hostdata->mmc_res[3];
			((u8 *)&status)[3] = hostdata->mmc_res[4];
			cmd = PXA_MMC_RESPONSE( ctrlr, 5 )&0x3f;
			MMC_DEBUG( MMC_DEBUG_LEVEL3, 
			//printk( KERN_INFO __FUNCTION__"(): "
				"cmd=%u status: 0x%08x\n",
				cmd, status );
			switch ( cmd ) {
			case 11:
			case 18:
			case 20:
			case 25:
				if ( !(status & 0x00000100) ) /* FIXME */
					goto mmc_error;
			default:
				break;
			}
			if ( status & MMC_CARD_STATUS_OUT_OF_RANGE ) {
				ret = MMC_ERROR_OUT_OF_RANGE;
				goto mmc_error;
		    	} else if ( status & MMC_CARD_STATUS_ADDRESS_ERROR ) {
				ret = MMC_ERROR_ADDRESS_ERROR;
				goto mmc_error;
			} else if ( status & MMC_CARD_STATUS_BLOCK_LEN_ERROR ) {
				ret = MMC_ERROR_ADDRESS_ERROR;
				goto mmc_error;
			} else if ( status & MMC_CARD_STATUS_ERASE_SEQ_ERROR ) {
				ret = MMC_ERROR_ERASE_SEQ_ERROR;
				goto mmc_error;
			} else if ( status & MMC_CARD_STATUS_ERASE_PARAM ) {
				ret = MMC_ERROR_ERASE_PARAM;
				goto mmc_error;
			} else if ( status & MMC_CARD_STATUS_WP_VIOLATION ) {
				ret = MMC_ERROR_WP_VIOLATION;
				goto mmc_error;
			} else if ( status & MMC_CARD_STATUS_CARD_IS_LOCKED ) {
				ret = MMC_ERROR_CARD_IS_LOCKED;
				goto mmc_error;
			} else if ( status & MMC_CARD_STATUS_LOCK_UNLOCK_FAILED ) {
				ret = MMC_ERROR_LOCK_UNLOCK_FAILED;
				goto mmc_error;
			} else if ( status & MMC_CARD_STATUS_COM_CRC_ERROR ) {
				ret = MMC_ERROR_COM_CRC_ERROR;
				goto mmc_error;
			} else if ( status & MMC_CARD_STATUS_ILLEGAL_COMMAND ) {
				ret = MMC_ERROR_ILLEGAL_COMMAND;
				goto mmc_error;
			} else if ( status & MMC_CARD_STATUS_CARD_ECC_FAILED ) {
				ret = MMC_ERROR_CARD_ECC_FAILED;
				goto mmc_error;
			} else if ( status & MMC_CARD_STATUS_CC_ERROR ) {
				ret = MMC_ERROR_CC_ERROR;
				goto mmc_error;
			} else if ( status & MMC_CARD_STATUS_ERROR ) {
				ret = MMC_ERROR_ERROR;
				goto mmc_error;
			} else if ( status & MMC_CARD_STATUS_UNDERRUN ) {
				ret = MMC_ERROR_UNDERRUN;
				goto mmc_error;
			} else if ( status & MMC_CARD_STATUS_OVERRUN ) {
				ret = MMC_ERROR_OVERRUN;
				goto mmc_error;
			} else if ( status & MMC_CARD_STATUS_CID_CSD_OVERWRITE ) {
				ret = MMC_ERROR_CID_CSD_OVERWRITE;
				goto mmc_error;
			} else if ( status & MMC_CARD_STATUS_ERASE_RESET ) {
				ret = MMC_ERROR_ERASE_RESET;
				goto mmc_error;
			}
		}
	}
    
	if ( ret >= 0 )
		if ( __pxa_mmc_set_state( ctrlr,  PXA_MMC_FSM_END_CMD ) ) {	
			ret = MMC_ERROR_GENERIC;
			goto error;
		}
	goto out;
mmc_error:
#if 1
	if ( send_abort ) {
		/* send CMD12 to abort failed transfer */
            	if ( (ret = pxa_mmc_stop_bus_clock( ctrlr )) )
                	goto error;
    
	        MMC_CMD = CMD(12); /* STOP_TRANSMISSION */
        	MMC_CMDAT = MMC_CMDAT_R1;
    		
		if ( (ret = pxa_mmc_complete_cmd( ctrlr, MMC_R1, FALSE )) )
		        goto error;
    		
		ret = -EIO;
		goto error;
	}
#endif
error:
	/* move controller to the IDLE state */
	pxa_mmc_stop_bus_clock( ctrlr );
	__pxa_mmc_set_state( ctrlr, PXA_MMC_FSM_IDLE );
out:
	__LEAVE( "ret=%d", ret );
	return ret;
}

/* 
int pxa_mmc_complete_io( mmc_controller_t ctrlr, mmc_dir_t cmd, mmc_dir_t dir, mmc_transfer_mode_t mode )

Effects: finilizes data transfer request  
Reqires: controller is in the END_BUFFER state
Modifies: moves controller to the IDLE state
Returns: zero upon success or error condition code otherwise
 */
static mmc_error_t pxa_mmc_complete_io( mmc_controller_t ctrlr, mmc_dir_t dir, mmc_transfer_mode_t mode )
{
	int ret = MMC_ERROR_GENERIC; 

	__ENTER( "dir=%d, mode=%d", dir, mode );
    
	if ( __pxa_mmc_check_state( ctrlr, PXA_MMC_FSM_END_IO ) )
		goto error;
    
	switch ( mode ) {
	case MMC_TRANSFER_MODE_STREAM: /* FIXME */
		if ( dir == MMC_WRITE ) {
		/* 1. wait for STOP_CMD intr */
			if ( (ret = pxa_mmc_init_completion( ctrlr,
					MMC_I_MASK_STOP_CMD )) )
				goto error;
			if ( (ret = pxa_mmc_wait_for_completion( ctrlr, 
					MMC_I_REG_STOP_CMD )) )
				goto error;
		}
		/* 2. send CMD12 */
		if ( (ret = pxa_mmc_stop_bus_clock( ctrlr )) )
			goto error;
    
		MMC_CMD = CMD(12); /* STOP_TRANSMISSION */
		MMC_CMDAT = MMC_CMDAT_R1;
		if ( dir == MMC_WRITE ) 
			MMC_CMDAT |= MMC_CMDAT_BUSY;
            
		/* 3. wait for CMD12 to complete */
		MMC_DEBUG( MMC_DEBUG_LEVEL3, "ready for CMD12\n" );
		if ( (ret = pxa_mmc_complete_cmd( ctrlr, MMC_R1, FALSE )) )
			goto error;
            
		/* 4. wait for DATA_TRAN_DONE intr */
		if ( (ret = pxa_mmc_init_completion( ctrlr,
				MMC_I_MASK_DATA_TRAN_DONE )) )
			goto error;
		if ( (ret = pxa_mmc_wait_for_completion( ctrlr, 
				MMC_I_REG_DATA_TRAN_DONE )) )
			goto error;
	    
		if ( dir == MMC_WRITE ) {
		/* 5. wait for PRG_DONE intr */
			if ( (ret = pxa_mmc_init_completion( ctrlr,
					MMC_I_MASK_PRG_DONE )) )
				goto error;
        	        if ( (ret = pxa_mmc_wait_for_completion( ctrlr, 
                	        	MMC_I_REG_PRG_DONE )) )
				goto error;
		}
		break;
	case MMC_TRANSFER_MODE_BLOCK_MULTIPLE:
		/* 1. wait for DATA_TRAN done intr */
		if ( (ret = pxa_mmc_init_completion( ctrlr,
				MMC_I_MASK_DATA_TRAN_DONE )) )
			goto error;
		if ( (ret = pxa_mmc_wait_for_completion( ctrlr, 
				MMC_I_REG_DATA_TRAN_DONE )) )
			goto error;
            
		/* 2. send CMD12 */
		if ( (ret = pxa_mmc_stop_bus_clock( ctrlr )) )
			goto error;
    
		MMC_CMD = CMD(12); /* STOP_TRANSMISSION */
		MMC_CMDAT = MMC_CMDAT_R1;
		if ( dir == MMC_WRITE ) 
			MMC_CMDAT |= MMC_CMDAT_BUSY;
            
		MMC_DEBUG( MMC_DEBUG_LEVEL3, "CMD12\n" );
		if ( (ret = pxa_mmc_complete_cmd( ctrlr, MMC_R1, FALSE )) )
			goto error;
        
		if ( dir == MMC_WRITE ) {
		/* 3. wait for PRG_DONE intr */
			if ( (ret = pxa_mmc_init_completion( ctrlr,
					MMC_I_MASK_PRG_DONE )) )
				goto error;
			if ( (ret = pxa_mmc_wait_for_completion( ctrlr, 
					MMC_I_REG_PRG_DONE )) )
				goto error;
		}
		break;
	case MMC_TRANSFER_MODE_BLOCK_SINGLE:
		/* 1. wait for DATA_TRAN_DONE intr */
		if ( (ret = pxa_mmc_init_completion( ctrlr,
				MMC_I_MASK_DATA_TRAN_DONE )) )
			goto error;
		if ( (ret = pxa_mmc_wait_for_completion( ctrlr, 
				MMC_I_REG_DATA_TRAN_DONE )) )
			goto error;
            
		if ( dir == MMC_WRITE ) {
		/* 2. wait for PRG_DONE intr */
			if ( (ret = pxa_mmc_init_completion( ctrlr,
					MMC_I_MASK_PRG_DONE )) )
				goto error;
			if ( (ret = pxa_mmc_wait_for_completion( ctrlr, 
					MMC_I_REG_PRG_DONE )) )
				goto error;
		}
		break;
	default:
		MMC_DEBUG( MMC_DEBUG_LEVEL3, "unknown transfer mode\n" );
		goto error;
	}
/* move the controller to the IDLE state */ 
	if ( (ret = pxa_mmc_stop_bus_clock( ctrlr )) )
		goto error;
    
	if ( __pxa_mmc_set_state( ctrlr, PXA_MMC_FSM_IDLE ) )
		goto error;

	ret = 0;
error:
	__LEAVE( "ret=%d", ret );
	return ret;
}

static inline int __pxa_mmc_update_acq( mmc_controller_t ctrlr )
{
	int ret = -EINVAL;
	pxa_mmc_hostdata_t hostdata = NULL;
	mmc_card_t card = NULL;
	mmc_card_stack_rec_t fake;
	mmc_card_stack_t stack = &fake;
	u16 argl = 0U, argh = 0U;
	int ncards = 0;

	if ( !ctrlr )
		goto error;

	hostdata = (pxa_mmc_hostdata_t)ctrlr->host_data;
	
	__mmc_card_stack_init( stack );
	
/* max open-drain mode frequency is 400kHZ */
	MMC_CLKRT = MMC_CLKRT_0_3125MHZ;
	MMC_RESTO = MMC_RES_TO_MAX; /* set response timeout */

/*  discover and add cards to the stack */  
/* I. bus operation condition setup */
/*      1) send CMD1 */
	if ( (ret = pxa_mmc_stop_bus_clock( ctrlr )) )
		goto err_free;
    
	argh = 0x00ff;
	argl = 0xc000; /* high voltage cards */

	MMC_CMD = CMD(1);
	MMC_ARGH = argh;
	MMC_ARGL = argl; 
    
	MMC_CMDAT = MMC_CMDAT_INIT|MMC_CMDAT_BUSY|MMC_CMDAT_R3; 
    
	MMC_DEBUG( MMC_DEBUG_LEVEL3, "CMD1(0x%04x%04x)\n", argh, argl );
	ret = pxa_mmc_complete_cmd( ctrlr, MMC_R3, FALSE );
	if ( !ret ) {
		argh = (PXA_MMC_RESPONSE( ctrlr, 4 ) << 8) 
			| PXA_MMC_RESPONSE( ctrlr, 3 );
		argl = (PXA_MMC_RESPONSE( ctrlr, 2 ) << 8)
			| PXA_MMC_RESPONSE( ctrlr, 1 );

	} else if ( ret != MMC_ERROR_TIME_OUT_RESPONSE ) 
		goto err_free;
    
	if ( !argh && !argl ) {
		MMC_DEBUG( MMC_DEBUG_LEVEL3, 
			"assuming full voltage range support\n" );
		argh = 0x00ff;
		argl = 0xff00;
	}
    
/*      2) continuously send CMD1 'till there're busy cards */
	for(;;) {
		if ( (ret = pxa_mmc_stop_bus_clock( ctrlr )) )
			goto err_free;

		MMC_CMD = CMD(1);
		MMC_ARGH = argh;
		MMC_ARGL = argl;

		MMC_CMDAT = MMC_CMDAT_BUSY|MMC_CMDAT_R3;
        
		MMC_DEBUG( MMC_DEBUG_LEVEL3, "CMD1(0x%04x%04x)\n", argh, argl );
		ret = pxa_mmc_complete_cmd( ctrlr, MMC_R3, FALSE );
		if ( ret == MMC_ERROR_TIME_OUT_RESPONSE )
			break; 

		else if ( !ret ) {
			/* busy state reported by LOW signal level 
			 * (MMC v3.2, p.58)
			 *
			 * Thanks to Alexander Samoutin :)
			 */  
			if ( !(PXA_MMC_RESPONSE( ctrlr, 4 ) & 0x80) ) {
				MMC_DEBUG( MMC_DEBUG_LEVEL3, 
						"busy state reported\n" );
				udelay( 20 ); /* approx. 70 clocks */
				continue;
    
			} else 
				break;
		} else
			goto err_free;
	}

/* II. card identification: the cards in Ready state 
 *     are the only expected to respond 
 */     
	for (;;) {
		argh = 0U;
		argl = 0U;
    
	/* 1) send CMD2 */
		if ( (ret = pxa_mmc_stop_bus_clock( ctrlr )) )
			goto err_free;
        
		MMC_CMD = CMD(2);
		MMC_ARGH = argh;
		MMC_ARGL = argl;
		MMC_CMDAT = MMC_CMDAT_R2;
        
		MMC_DEBUG( MMC_DEBUG_LEVEL3, "CMD2(0x%04x%04x)\n", argh, argl );
		ret = pxa_mmc_complete_cmd( ctrlr, MMC_R2, FALSE );
		if ( ret == MMC_ERROR_TIME_OUT_RESPONSE )
			break;
        
		else if ( ret ) /* bus error */
			goto err_free;
        
	        /* TODO: store CID for the card */
        
        	/* 2) assign RCA */
		if ( !++ctrlr->rca_next ) /* overflow */
			++ctrlr->rca_next;
		argh = ctrlr->rca_next; 
        
        	/* 3) send it to the card last responded (CMD3) */
		if ( (ret = pxa_mmc_stop_bus_clock( ctrlr )) )
			goto err_free;
        
		MMC_CMD = CMD(3);
		MMC_ARGH = argh;
		MMC_CMDAT = MMC_CMDAT_R1;

		MMC_DEBUG( MMC_DEBUG_LEVEL3, "CMD3(0x%04x%04x)\n", argh, argl );
		ret = pxa_mmc_complete_cmd( ctrlr, MMC_R1, FALSE );
		if ( ret )   /* CMD3 failed */ 
			goto err_free;

		card = __mmc_card_alloc( sizeof( pxa_mmc_card_data_rec_t ) );
		if ( !card ) {
			MMC_ERROR( "out of memory\n" );
			goto err_free;
		}

		card->info.rca = argh;  
		card->slot = ctrlr->slot_next++; /* FIXME: minor encoding */
		card->ctrlr = ctrlr;
        
		if ( !__mmc_card_stack_add( stack, card ) )
			goto err_free;
        
		MMC_DEBUG( MMC_DEBUG_LEVEL2, "added card: "
				"slot %d, RCA=0x%04x\n", card->slot, argh );
		++ncards;
	}
   
	if ( ncards ) {
/* III. read CSD registers of all cards; DSR support also reported there */
		for ( card = stack->first; card; card = card->next ) {
			pxa_mmc_card_data_t card_data = 
				(pxa_mmc_card_data_t)card->card_data;
	    
			/* 1) send CMD9 */
			argh = card->info.rca;
			argl = 0U;
        
			if ( (ret = pxa_mmc_stop_bus_clock( ctrlr )) )
				goto err_free;
        
			MMC_CMD = CMD(9);
			MMC_ARGH = argh;
			MMC_ARGL = argl;
			MMC_CMDAT = MMC_CMDAT_R2;
        
			MMC_DEBUG( MMC_DEBUG_LEVEL3, 
				"CMD9(0x%04x%04x)\n", argh, argl );
			if ( (ret = pxa_mmc_complete_cmd( ctrlr, MMC_R2, FALSE )) )
				goto err_free; 
       	
			memcpy( &card->info.csd, hostdata->mmc_res, 15 );
			MMC_DUMP_CSD( card );
	    
			card->info.read_bl_len = (1<<card->info.csd.read_bl_len);
			card->info.write_bl_len = (1<<card->info.csd.write_bl_len);
			card->info.capacity = (card->info.csd.c_size + 1) 
					* (1<<(card->info.csd.c_size_mult + 2)) 
					* card->info.read_bl_len;
			MMC_DEBUG( MMC_DEBUG_LEVEL2, "card capacity=%dMb\n", 
					card->info.capacity>>20 );
			card->info.tran_speed = 20*1024; /* FIXME */
			card->info.transfer_mode = MMC_TRANSFER_MODE_BLOCK_SINGLE; 
		/* 2) set bus operation freq */
			card_data->clkrt = __pxa_mmc_clkrt( card->info.tran_speed );
		/* 3) register card with MMC core */
			mmc_register( MMC_REG_TYPE_CARD, card, 0 );
		}
/* IV. set DSR registers of the cards */
#if 0 /* TODO */
		if ( card->info.csd.dsr_imp ) {
			set_dsr = TRUE;
			/*  calculate DSR */
		}
#endif
	}
#if 0 /* TODO */
	if ( set_dsr ) {
			/* send CMD4 */
	}
#endif
/* merge list of the newly inserted cards into controller card stack */
	if ( !ctrlr->stack.ncards ) {
		ctrlr->stack.first = stack->first;
		ctrlr->stack.last = stack->last;
	} else	
		ctrlr->stack.last->next = stack->first;
	
	ctrlr->stack.ncards += stack->ncards;
	
	ret = 0;
	goto out;
err_free:
	__mmc_card_stack_free( stack );
error:
out:
	return ret;
}

/* MMC protocol macros: v3.4, p.120 */
static int pxa_mmc_init_card_stack( mmc_controller_t ctrlr )
{
	int ret = -EIO;
	u16 argl = 0U, argh = 0U;
    
	__ENTER0();
   
	if ( !ctrlr || ctrlr->stack.ncards ) {
		ret = -EINVAL;
		goto error;
	}

/* initialize stack */  
/*     1) send CMD0 */
	if ( (ret = pxa_mmc_stop_bus_clock( ctrlr )) )
		goto error;

/* max open-drain mode frequency is 400kHZ */
	MMC_CLKRT = MMC_CLKRT_0_3125MHZ;
	MMC_RESTO = MMC_RES_TO_MAX; /* set response timeout */
	MMC_SPI = MMC_SPI_DISABLE;
    
	MMC_CMD = CMD(0); /* CMD0 with zero argument */
	MMC_ARGH = argh;
	MMC_ARGL = argl; 
	MMC_CMDAT = 0UL;
    
	MMC_DEBUG( MMC_DEBUG_LEVEL3, "CMD0(0x%04x%04x)\n", argh, argl );
	if ( (ret = pxa_mmc_complete_cmd( ctrlr, MMC_NORESPONSE, FALSE )) )
		goto error;

/* update card stack */
	if ( (ret = __pxa_mmc_update_acq( ctrlr )) )
		goto err_free;
	
/* move the controller to the IDLE state */ 
	if ( (ret = pxa_mmc_stop_bus_clock( ctrlr )) )
		goto err_free;
    
	if ( __pxa_mmc_set_state( ctrlr, PXA_MMC_FSM_IDLE ) )
		goto err_free;
	
	ret = 0;
	MMC_DEBUG( MMC_DEBUG_LEVEL2, "ncards=%d\n", ctrlr->stack.ncards );
	goto out;

err_free:
	__mmc_card_stack_free( &ctrlr->stack ); 
error:
out:
	__LEAVE( "ret=%d", ret );
	return ret;
}

static int pxa_mmc_update_acq( mmc_controller_t ctrlr )
{
	int ret = -EINVAL;
	
	__ENTER0();
	
	if ( !ctrlr )
		goto error;
	
	ret = __pxa_mmc_update_acq( ctrlr );
error:	
	__LEAVE( "ret=%d", ret );
	return ret;
}

/* Intel Corp. states this controller to support up to 2 cards on the MMC bus */
#if 0 
static int pxa_mmc_single_card_acq( mmc_controller_t ctrlr )
{
	int ret = -1;
	__ENTER0();
	__LEAVE0();
	return ret;
}
#endif

static int pxa_mmc_check_card_stack( mmc_controller_t ctrlr )
{
	int ret = -1;
	mmc_card_t card;
	
	__ENTER0();
	
	if ( !ctrlr )
		goto error;
	
	if ( ctrlr->stack.ncards > 0 )	 {
/* for each card in the stack: */
		for( card = ctrlr->stack.first; card; card = card->next ) {
			u16 argh = card->info.rca;
			u16 argl = 0UL;
			
/* 	1) send CMD9( card->rca ) */
			if ( pxa_mmc_stop_bus_clock( ctrlr ) )
				goto error;
			
			/* SanDisk's cards do not respond to CMD9 */	
			MMC_CMD = CMD(13);
			MMC_ARGH = argh;
			MMC_ARGL = argl;
			MMC_CMDAT = MMC_CMDAT_R1;
			
			MMC_DEBUG( MMC_DEBUG_LEVEL3, "CMD13(0x%04x%04x)\n", 
					argh, argl );
			ret = pxa_mmc_complete_cmd( ctrlr, MMC_R1, FALSE );
/* 	2) if card responded, it is still there */
			if ( ret ) 
				card->state = MMC_CARD_STATE_UNPLUGGED;
		}
	}
	ret = 0;	
error:
	__LEAVE( "ret=%d", ret );
	return ret;
}

/* This procedure links the bus master with a single card
 *  1)  cross checks with the internal stack management data if a card still
 *      exists in the slot
 *  2)  send CMD7( card->public.rca )
 *  3)  setup data path and controller options 
 */
static int pxa_mmc_setup_card( mmc_controller_t ctrlr, mmc_card_t card ) 
{
	int ret = -ENODEV;
	pxa_mmc_hostdata_t hostdata;
	pxa_mmc_card_data_t card_data;
	u16 argh = 0U;
#ifdef CONFIG_MMC_DEBUG
	u16 argl = 0U;
#endif
    
	__ENTER0();

	if ( !ctrlr || !card ) {
		ret = -EINVAL;
		goto error;
	}
    
	if ( card->ctrlr != ctrlr ) {
		MMC_DEBUG( MMC_DEBUG_LEVEL3, "card is on another bus\n" );
		goto error;
	}
    
	hostdata = (pxa_mmc_hostdata_t)ctrlr->host_data;
	card_data = (pxa_mmc_card_data_t)card->card_data;

	argh = card->info.rca;
    
/* select requested card */ 
	if ( (ret = pxa_mmc_stop_bus_clock( ctrlr )) ) 
		goto error;
        
	MMC_CMD = CMD(7);
	MMC_ARGH = argh;
	MMC_CMDAT = MMC_CMDAT_R1;

	MMC_DEBUG( MMC_DEBUG_LEVEL3, "CMD7(0x%04x%04x)\n", argh, argl );
	if ( (ret = pxa_mmc_complete_cmd( ctrlr, MMC_R1, FALSE )) )
		goto error;

/* set controller options */
#ifndef CONFIG_MMC_DEBUG
	MMC_CLKRT = card_data->clkrt; 
#endif    
/* move the controller to the IDLE state */ 
	if ( (ret = pxa_mmc_stop_bus_clock( ctrlr )) )
		goto error;
    
	if ( __pxa_mmc_set_state( ctrlr, PXA_MMC_FSM_IDLE ) )
		goto error;
	
	ret = 0;
error:  
	__LEAVE0();
	return ret;
}

static inline int __pxa_mmc_iobuf_init( mmc_controller_t ctrlr, ssize_t cnt )
{
#ifdef PIO
	pxa_mmc_hostdata_t hostdata = (pxa_mmc_hostdata_t)ctrlr->host_data;
#endif 
#ifndef PIO
/* TODO */
#else
	hostdata->iobuf.buf.pos = hostdata->iobuf.iodata;
	hostdata->iobuf.buf.cnt = cnt;
#endif
	return 0;
}
/* TODO: ssize_t pxa_mmc_read_buffer( mmc_controller_t ctrlr, ssize_t cnt )
effects: reads at most cnt bytes from the card to the controller I/O buffer;
       takes care of partial data transfers
requieres: 
modifies: ctrlr->iobuf
returns: number of bytes actually transferred or negative error code if there were any errors
 */
ssize_t pxa_mmc_read_buffer( mmc_controller_t ctrlr, ssize_t cnt )
{
	ssize_t ret = -EIO;
	pxa_mmc_hostdata_t hostdata = (pxa_mmc_hostdata_t)ctrlr->host_data;
#ifndef PIO
	register int ndesc;
	int chan = hostdata->iobuf.buf.chan;
	pxa_dma_desc *desc;
#endif
	__ENTER( 
//	printk( KERN_INFO __FUNCTION__"(): "
		"cnt=%d", cnt );

	if ( (hostdata->state != PXA_MMC_FSM_END_CMD) 
	     && (hostdata->state != PXA_MMC_FSM_END_BUFFER) ) {
		MMC_DEBUG( MMC_DEBUG_LEVEL3,
//		printk( KERN_INFO __FUNCTION__"(): "				
				"unexpected state (%s)",
				PXA_MMC_STATE_LABEL( hostdata->state ) ); 
            goto error;
	}
	
	if ( cnt > hostdata->iobuf.bufsz )
		cnt = hostdata->iobuf.bufsz;

	if ( (ret = __pxa_mmc_iobuf_init( ctrlr, cnt )) )
		goto error;
    
	(void)__pxa_mmc_set_state( ctrlr, PXA_MMC_FSM_BUFFER_IN_TRANSIT );
#ifndef PIO
	if ( pxa_mmc_init_completion( ctrlr, ~MMC_I_MASK_ALL ) ) /* FIXME */
		goto error;
	
	if ( (desc = hostdata->iobuf.buf.last_read_desc) ) {
		desc->ddadr &= ~DDADR_STOP;
		desc->dcmd &= ~(DCMD_ENDIRQEN|DCMD_LENGTH);
		desc->dcmd |= (1<<5);
	}
/* 1) setup descriptors for DMA transfer from the device */
	ndesc = (cnt>>5) - 1; /* FIXME: partial read */
	desc = &hostdata->iobuf.buf.read_desc[ndesc];
	hostdata->iobuf.buf.last_read_desc = desc;	
	/* TODO: partial read */
	desc->ddadr |= DDADR_STOP;
	desc->dcmd |= DCMD_ENDIRQEN;
/* 2) start DMA channel */
	DDADR( chan ) = hostdata->iobuf.buf.read_desc_phys_addr;
	DCSR( chan ) |= DCSR_RUN;
#else
	if ( pxa_mmc_init_completion( ctrlr, MMC_I_MASK_RXFIFO_RD_REQ ) )
		goto error;
#endif

	if ( pxa_mmc_wait_for_completion( ctrlr, ~0UL ) )
		goto error;

	if ( __pxa_mmc_check_state( ctrlr, PXA_MMC_FSM_END_BUFFER ) )
		goto error;

	if ( !(hostdata->mmc_stat & MMC_STAT_ERRORS) ) /* FIXME */  
		ret = cnt;
error:
	__LEAVE( "ret=%d", ret );
	return ret;
}

ssize_t pxa_mmc_write_buffer( mmc_controller_t ctrlr, ssize_t cnt )
{
	ssize_t ret = -EIO;
	pxa_mmc_hostdata_t hostdata = (pxa_mmc_hostdata_t)ctrlr->host_data;
#ifndef PIO
	register int ndesc;
	int chan = hostdata->iobuf.buf.chan;
	pxa_dma_desc *desc;
#endif
	
	__ENTER0();
	if ( (hostdata->state != PXA_MMC_FSM_END_CMD) 
	    && (hostdata->state != PXA_MMC_FSM_END_BUFFER) ) {
		MMC_DEBUG( MMC_DEBUG_LEVEL3, "unexpected state (%s)\n",
			PXA_MMC_STATE_LABEL( hostdata->state ) ); 
		goto error;
	}
    
	if ( cnt > hostdata->iobuf.bufsz )
		cnt = hostdata->iobuf.bufsz;
	
	if ( (ret = __pxa_mmc_iobuf_init( ctrlr, cnt )) )
		goto error;
    
	(void)__pxa_mmc_set_state( ctrlr, PXA_MMC_FSM_BUFFER_IN_TRANSIT );
#ifndef PIO
	if ( pxa_mmc_init_completion( ctrlr, ~MMC_I_MASK_ALL ) ) /* FIXME */
		goto error;
	if ( (desc = hostdata->iobuf.buf.last_write_desc) ) {
		desc->ddadr &= ~DDADR_STOP;
		desc->dcmd &= ~(DCMD_ENDIRQEN|DCMD_LENGTH);
		desc->dcmd |= (1<<5);
	}
/* 1) setup descriptors for DMA transfer to the device */
	ndesc = (cnt>>5) - 1; /* FIXME: partial write */
	desc = &hostdata->iobuf.buf.write_desc[ndesc];
	/* TODO: partial write */
	hostdata->iobuf.buf.last_write_desc = desc;	
	desc->ddadr |= DDADR_STOP;
	desc->dcmd |= DCMD_ENDIRQEN;
/* 2) start DMA channel */
	DDADR( chan ) = hostdata->iobuf.buf.write_desc_phys_addr;
	DCSR( chan ) |= DCSR_RUN;
#else
	if ( pxa_mmc_init_completion( ctrlr, MMC_I_MASK_TXFIFO_WR_REQ ) )
		goto error;
#endif
	if ( pxa_mmc_wait_for_completion( ctrlr, ~0UL ) )
		goto error;
    
	if ( __pxa_mmc_check_state( ctrlr, PXA_MMC_FSM_END_BUFFER ) )
		goto error;

	if ( !(hostdata->mmc_stat & MMC_STAT_ERRORS) ) /* FIXME */  
		ret = cnt;
error:
	__LEAVE( "ret=%d", ret );
	return ret;
}

/* TODO: ssize_t pxa_mmc_copy_from_buffer( ctrlr, mmc_buftype_t to, char *buf, ssize_t cnt )
effects: copies at most cnt bytes from the controller I/O buffer to the user or kernel buffer
         pointed by buf
requiers:
modifies:
returns: number of bytes actually transferred or negative error code if there were any errors
 */ 
ssize_t pxa_mmc_copy_from_buffer( mmc_controller_t ctrlr, mmc_buftype_t to, char *buf, ssize_t cnt )
{
	ssize_t ret = -EIO;
	pxa_mmc_hostdata_t hostdata = (pxa_mmc_hostdata_t)ctrlr->host_data;

	__ENTER( "type=%d, buf=%x, cnt=%ld", to, buf, cnt );

#ifndef PIO
/* TODO: check that DMA channel is not running */
#endif
	switch ( to ) {
	case MMC_USER:
		if ( copy_to_user( buf, hostdata->iobuf.iodata, cnt ) ) {
			ret = -EFAULT;
			goto error;
		}
		break;
        case MMC_KERNEL:
		memcpy( buf, hostdata->iobuf.iodata, cnt );
		break;
        default:
		MMC_DEBUG( MMC_DEBUG_LEVEL3, "unknown buffer type\n" );
		goto error;
	}
	ret = cnt;
error:
	__LEAVE( "ret=%d", ret );
	return ret;
}

ssize_t pxa_mmc_copy_to_buffer( mmc_controller_t ctrlr, mmc_buftype_t to, char *buf, ssize_t cnt )
{
	ssize_t ret = -EIO;
	pxa_mmc_hostdata_t hostdata = (pxa_mmc_hostdata_t)ctrlr->host_data;
#ifndef PIO
/* check that DMA channel is not running */
#endif
	switch ( to ) {
	case MMC_USER:
		if ( copy_from_user( hostdata->iobuf.iodata, buf, cnt ) ) {
			ret = -EFAULT;
			goto error;
		}
		break;
	case MMC_KERNEL:
		memcpy( hostdata->iobuf.iodata, buf, cnt );
		break;
	default:
		MMC_DEBUG( MMC_DEBUG_LEVEL3, "unknown buffer type\n" );
		goto error;
	}
	ret = cnt;
error:
	return ret;
}

/* This procedure sequentally passes the data from the user buffer to the card */
static int pxa_mmc_stream_read( mmc_controller_t ctrlr, mmc_data_transfer_req_t transfer )
{
	int ret = -EIO;
	pxa_mmc_hostdata_t hostdata = (pxa_mmc_hostdata_t)ctrlr->host_data;
	u16 argh = 0UL, argl = 0UL;
	ssize_t size = 0;

	__ENTER( "transfer: cmd=%d mode=%d type=%d blksz=%d "
		 "nob=%d buf=%p cnt=%d addr=%Lx", 
		 transfer->cmd, transfer->mode, transfer->type,
		 transfer->blksz, transfer->nob, transfer->buf, 
		 transfer->cnt, transfer->addr );
    
    
	while ( transfer->cnt > 0 ) {
		size = (transfer->cnt < hostdata->iobuf.blksz) ?
			transfer->cnt : hostdata->iobuf.blksz; 
		/* 1. send CMD11 */
		if ( (ret = pxa_mmc_stop_bus_clock( ctrlr )) )
			goto error;

		argh = transfer->addr >> 16;
		argl = transfer->addr;
		/* 2. setup controller registers to start stream data transfer */
		MMC_CMD = CMD(11); /* READ_DAT_UNTIL_STOP */
		MMC_ARGH = argh;
		MMC_ARGL = argl;
		MMC_NOB = 0xffff;
		MMC_BLKLEN = size;
		MMC_CMDAT = MMC_CMDAT_R1|MMC_CMDAT_READ|MMC_CMDAT_STREAM|MMC_CMDAT_DATA_EN;
#ifndef PIO
		MMC_CMDAT |= MMC_CMDAT_MMC_DMA_EN; 
#endif
		/* 3. wait for cmd to complete */
		MMC_DEBUG( MMC_DEBUG_LEVEL3, "CMD11(0x%04x%04x)\n", argh, argl );
		if ( (ret = pxa_mmc_complete_cmd( ctrlr, MMC_R1, TRUE )) )
			goto error;	
    
		/* 4. transfer the data to the caller supplied buffer */
		if ( (ret = pxa_mmc_read_buffer( ctrlr, size )) < 0 )
			goto error;

		if ( (ret = pxa_mmc_copy_from_buffer( ctrlr, transfer->type, transfer->buf, ret )) < 0 )
			goto error;
        
		if ( __pxa_mmc_set_state( ctrlr, PXA_MMC_FSM_END_IO ) )
			goto error;

		if ( (ret = pxa_mmc_complete_io( ctrlr, transfer->cmd, transfer->mode )) )
			goto error;
	        
		transfer->buf += ret;
		transfer->addr += ret;
		transfer->cnt -= ret;
	}
	ret = 0;    
error:
	__LEAVE( "ret=%d", ret );
	return ret;
}

/* This procedure reads a data block from a card at a given kernel address */ 
static int pxa_mmc_read_block( mmc_controller_t ctrlr, mmc_data_transfer_req_t transfer )
{
	int ret = -ENODEV;
	u16 argh = 0UL, argl = 0UL;

	__ENTER( "transfer: cmd=%d mode=%d type=%d blksz=%d nob=%d buf=%p cnt=%d addr=%Lx", transfer->cmd, transfer->mode, transfer->type, transfer->blksz, transfer->nob, transfer->buf, transfer->cnt, transfer->addr );

/* send CMD16 (SET_BLOCK_LEN) when requested block size is not the default
 * for the current card */  
	if ( transfer->blksz != ctrlr->stack.selected->info.read_bl_len ) {
		argh = transfer->blksz >> 16;
		argl = transfer->blksz;
		if ( (ret = pxa_mmc_stop_bus_clock( ctrlr )) ) 
			goto error;
    
		MMC_CMD = CMD(16); /* SET_BLOCK_LEN */
		MMC_ARGH = argh;
		MMC_ARGL = argl;
		MMC_CMDAT = MMC_CMDAT_R1;
        
		MMC_DEBUG( MMC_DEBUG_LEVEL3, 
				"CMD16(0x%04x%04x)\n", argh, argl );
		if ( (ret = pxa_mmc_complete_cmd( ctrlr, MMC_R1, FALSE )) )
			goto error;
	}
    
/* CMD17 (READ_SINGLE_BLOCK) */
	argh = transfer->addr >> 16;
	argl = transfer->addr;
	if ( (ret = pxa_mmc_stop_bus_clock( ctrlr )) )
		goto error;
	
	MMC_CMD = CMD(17); /* READ_SINGLE_BLOCK */
	MMC_ARGH = argh;
	MMC_ARGL = argl;
	MMC_CMDAT = MMC_CMDAT_R1|MMC_CMDAT_READ|MMC_CMDAT_BLOCK|MMC_CMDAT_DATA_EN;
	MMC_NOB = 1;
	MMC_BLKLEN = transfer->blksz;
#ifndef PIO
	MMC_CMDAT |= MMC_CMDAT_MMC_DMA_EN; 
#endif
    
	MMC_DEBUG( MMC_DEBUG_LEVEL3, "CMD17(0x%04x%04x)\n", argh, argl );
	if ( (ret = pxa_mmc_complete_cmd( ctrlr, MMC_R1, FALSE )) )
		goto error;
    
/* transfer the data to the caller supplied buffer */
	if ( (ret = pxa_mmc_read_buffer( ctrlr, transfer->blksz )) < 0 )
		goto error;

	if ( (ret = pxa_mmc_copy_from_buffer( ctrlr, transfer->type, transfer->buf, ret )) < 0 )
		goto error;
        
	transfer->buf += ret;
	transfer->cnt -= ret;
	transfer->nob -= 1;
    
	if ( __pxa_mmc_set_state( ctrlr, PXA_MMC_FSM_END_IO ) )
		goto error;

	if ( (ret = pxa_mmc_complete_io( ctrlr, transfer->cmd, transfer->mode )) )
		goto error;
    
	ret = 0;    
error:
	__LEAVE( "ret=%d", ret );
	return ret;
}

/* This procedure sequentally reads data blocks from
 * a card to the user buffer. Controller options and block size 
 * are already set by setup_card(). Data alignment and partial
 * data accessibility assumed to be checked by mmc_core */
static int pxa_mmc_read_mblock( mmc_controller_t ctrlr, mmc_data_transfer_req_t transfer )
{
	int ret = -EIO;
	u16 argh = 0UL, argl = 0UL;

	__ENTER( "transfer: cmd=%d mode=%d type=%d blksz=%d nob=%d buf=%p cnt=%d addr=%Lx", transfer->cmd, transfer->mode, transfer->type, transfer->blksz, transfer->nob, transfer->buf, transfer->cnt, transfer->addr );

/* send CMD16 (SET_BLOCK_LEN) when requested block size is not the default
 * for the current card */  
	if ( transfer->blksz != ctrlr->stack.selected->info.read_bl_len ) {
		argh = transfer->blksz >> 16;
		argl = transfer->blksz;
		if ( (ret = pxa_mmc_stop_bus_clock( ctrlr )) )
			goto error;
    
		MMC_CMD = CMD(16); /* SET_BLOCK_LEN */
		MMC_ARGH = argh;
		MMC_ARGL = argl;
		MMC_CMDAT = MMC_CMDAT_R1;
        
		MMC_DEBUG( MMC_DEBUG_LEVEL3, "CMD16(0x%04x%04x)\n", argh, argl );
		if ( (ret = pxa_mmc_complete_cmd( ctrlr, MMC_R1, FALSE )) )
			goto error;
	}
    
	argh = transfer->addr >> 16;
	argl = transfer->addr;
/* 1. stop bus clock */
	if ( (ret = pxa_mmc_stop_bus_clock( ctrlr )) )
		goto error;

/* 2. setup controller registers to start multiple block transfer */
	MMC_CMD = CMD(18); /* READ_MULTIPLE_BLOCK */
	MMC_ARGH = argh;
	MMC_ARGL = argl;
	MMC_NOB = transfer->nob;
	MMC_BLKLEN = transfer->blksz;
	MMC_CMDAT = MMC_CMDAT_R1|MMC_CMDAT_READ|MMC_CMDAT_BLOCK|MMC_CMDAT_DATA_EN;
#ifndef PIO
	MMC_CMDAT |= MMC_CMDAT_MMC_DMA_EN; 
#endif

/* 3. start clock */
	if ( (ret = pxa_mmc_start_bus_clock( ctrlr )) )
		goto error;
    
/* 4. wait for cmd to complete */
	MMC_DEBUG( MMC_DEBUG_LEVEL3, "CMD18(0x%04x%04x)\n", argh, argl );
	if ( (ret = pxa_mmc_complete_cmd( ctrlr, MMC_R1, TRUE )) )
		goto error;
    
/* 6. transfer the data to the caller supplied buffer */
	while ( transfer->cnt > 0 ) {
		if ( (ret = pxa_mmc_read_buffer( ctrlr, transfer->cnt )) < 0 )
			goto error;

		if ( (ret = pxa_mmc_copy_from_buffer( ctrlr, transfer->type, transfer->buf, ret )) < 0 )
			goto error;
        
		transfer->buf += ret;
		transfer->cnt -= ret;
	}
    
	if ( __pxa_mmc_set_state( ctrlr, PXA_MMC_FSM_END_IO ) )
		goto error;

	if ( (ret = pxa_mmc_complete_io( ctrlr, transfer->cmd, transfer->mode )) )
		goto error;
    
	ret = 0;
error:
	__LEAVE( "ret=%d", ret ); /* FIXME */
	return ret;
}

/* Sequentally writes the data from a user buffer to the card */
static int pxa_mmc_stream_write( mmc_controller_t ctrlr, mmc_data_transfer_req_t transfer )
{
	int ret = -EIO;
	pxa_mmc_hostdata_t hostdata = (pxa_mmc_hostdata_t)ctrlr->host_data;
	u16 argh = 0UL, argl = 0UL;
	ssize_t size = 0;
    
	__ENTER( "transfer: cmd=%d mode=%d type=%d blksz=%d ""
		nob=%d buf=%p cnt=%d addr=%Lx", transfer->cmd, 
		transfer->mode, transfer->type, transfer->blksz, 
		transfer->nob, transfer->buf, transfer->cnt, transfer->addr );

	argh = transfer->addr >> 16;
	argl = transfer->addr;
/* 1. stop bus clock */
	if ( (ret = pxa_mmc_stop_bus_clock( ctrlr )) ) 
		goto error;

/* 2. setup controller registers to start stream data transfer */
	MMC_CMD = CMD(20); /* WRITE_DAT_UNTIL_STOP */
	MMC_ARGH = argh;
	MMC_ARGL = argl;
	MMC_NOB = 0xffff;
	MMC_BLKLEN = hostdata->iobuf.blksz;
	MMC_CMDAT = MMC_CMDAT_R1|MMC_CMDAT_WRITE|MMC_CMDAT_STREAM|MMC_CMDAT_DATA_EN;
#ifndef PIO
	MMC_CMDAT |= MMC_CMDAT_MMC_DMA_EN; 
#endif

/* 3. wait for cmd to complete */
	MMC_DEBUG( MMC_DEBUG_LEVEL3, "CMD20(0x%04x%04x)\n", argh, argl );
	if ( (ret = pxa_mmc_complete_cmd( ctrlr, MMC_R1, TRUE )) )
		goto error;
    
/* 4. transfer the data to the caller supplied buffer */
	while ( transfer->cnt > 0 ) {
		size = (transfer->cnt < hostdata->iobuf.blksz) ?
			transfer->cnt : hostdata->iobuf.blksz; 
		if ( (ret = pxa_mmc_copy_to_buffer( ctrlr, 
				transfer->type, transfer->buf, size )) < 0 )
			goto error;
        
		if ( (ret = pxa_mmc_write_buffer( ctrlr, ret )) < 0 )
			goto error;

		transfer->buf += ret;
		transfer->cnt -= ret;
	}
    
	if ( (ret = __pxa_mmc_set_state( ctrlr, PXA_MMC_FSM_END_IO )) )
		goto error;

	if ( (ret = pxa_mmc_complete_io( ctrlr, transfer->cmd, transfer->mode )) )
		goto error;
    
	ret = 0;
error:
	__LEAVE( "ret=%d", ret );
	return ret;
}

/* This procedure writes a data block to a card at a given address */
static int pxa_mmc_write_block( mmc_controller_t ctrlr, mmc_data_transfer_req_t transfer )
{
	int ret = -ENODEV;
	u16 argh = 0UL, argl = 0UL;

	__ENTER( "transfer: cmd=%d mode=%d type=%d blksz=%d "
			"nob=%d buf=%p cnt=%d addr=%llx", transfer->cmd,
			transfer->mode, transfer->type, transfer->blksz,
			transfer->nob, transfer->buf, transfer->cnt, 
			transfer->addr );

/* send CMD16 (SET_BLOCK_LEN) when requested block size is not the default
 * for the current card */  
	if ( transfer->blksz != ctrlr->stack.selected->info.read_bl_len ) {
		argh = transfer->blksz >> 16;
		argl = transfer->blksz;
		if ( (ret = pxa_mmc_stop_bus_clock( ctrlr )) ) 
			goto error;
    
		MMC_CMD = CMD(16); /* SET_BLOCK_LEN */
		MMC_ARGH = argh;
		MMC_ARGL = argl;
		MMC_CMDAT = MMC_CMDAT_R1;
        
		MMC_DEBUG( MMC_DEBUG_LEVEL3, "CMD16(0x%04x%04x)\n", argh, argl );
		if ( (ret = pxa_mmc_complete_cmd( ctrlr, MMC_R1, FALSE )) )
			goto error;
	}
    
/* CMD17 (READ_SINGLE_BLOCK) */
	argh = transfer->addr >> 16;
	argl = transfer->addr;
	if ( (ret = pxa_mmc_stop_bus_clock( ctrlr )) )
		goto error;
    
	MMC_CMD = CMD(24); /* WRITE_BLOCK */
	MMC_ARGH = argh;
	MMC_ARGL = argl;
	MMC_CMDAT = MMC_CMDAT_R1|MMC_CMDAT_WRITE|MMC_CMDAT_BLOCK|MMC_CMDAT_DATA_EN;
#ifndef PIO
	MMC_CMDAT |= MMC_CMDAT_MMC_DMA_EN; 
#endif
	MMC_NOB = 1;
	MMC_BLKLEN = transfer->blksz;
    
	MMC_DEBUG( MMC_DEBUG_LEVEL3, "CMD24(0x%04x%04x)\n", argh, argl );
	if ( (ret = pxa_mmc_complete_cmd( ctrlr, MMC_R1, FALSE )) )
		goto error;
    
/* transfer the data to the caller supplied buffer */
	if ( (ret = pxa_mmc_copy_to_buffer( ctrlr, transfer->type, transfer->buf, transfer->cnt )) < 0 )
		goto error;
        
	if ( (ret = pxa_mmc_write_buffer( ctrlr, ret )) < 0 )
		goto error;

	transfer->buf += ret;
	transfer->cnt -= ret;
	transfer->nob -= 1;
    
	if ( __pxa_mmc_set_state( ctrlr, PXA_MMC_FSM_END_IO ) )
		goto error;

	if ( (ret = pxa_mmc_complete_io( ctrlr, transfer->cmd, transfer->mode )) )
		goto error;
    
	ret = 0;    
error:
	__LEAVE( "ret=%d", ret );
	return ret;
}

/* This procedure sequentally writes data blocks to a card at a given address */
static ssize_t pxa_mmc_write_mblock( mmc_controller_t ctrlr, mmc_data_transfer_req_t transfer )
{
	int ret = -EIO;
	u16 argh = 0UL, argl = 0UL;
    	
	__ENTER( "transfer: cmd=%d mode=%d type=%d blksz=%d "
			"nob=%d buf=%p cnt=%d addr=%Lx", transfer->cmd,
			transfer->mode, transfer->type, transfer->blksz,
			transfer->nob, transfer->buf, transfer->cnt,
			transfer->addr );

/* send CMD16 (SET_BLOCK_LEN) when requested block size is not the default
 * for the current card */  
	if ( transfer->blksz != ctrlr->stack.selected->info.write_bl_len ) {
		argh = transfer->blksz >> 16;
		argl = transfer->blksz;
		if ( (ret = pxa_mmc_stop_bus_clock( ctrlr )) )
			goto error;
    
		MMC_CMD = CMD(16); /* SET_BLOCK_LEN */
		MMC_ARGH = argh;
		MMC_ARGL = argl;
		MMC_CMDAT = MMC_CMDAT_R1;
        
		MMC_DEBUG( MMC_DEBUG_LEVEL3, "CMD16(0x%04x%04x)\n", argh, argl );
		if ( (ret = pxa_mmc_complete_cmd( ctrlr, MMC_R1, FALSE )) )
			goto error;
	}
    
	argh = transfer->addr >> 16;
	argl = transfer->addr;
/* 1. stop bus clock */
	if ( (ret = pxa_mmc_stop_bus_clock( ctrlr )) )
		goto error;

/* 2. setup controller registers to start multiple block transfer */
	MMC_CMD = CMD(25); /* WRITE_MULTIPLE_BLOCK */
	MMC_ARGH = argh;
	MMC_ARGL = argl;
	MMC_NOB = transfer->nob;
	MMC_BLKLEN = transfer->blksz;
	MMC_CMDAT = MMC_CMDAT_R1|MMC_CMDAT_WRITE|MMC_CMDAT_BLOCK|MMC_CMDAT_DATA_EN;
#ifndef PIO
	MMC_CMDAT |= MMC_CMDAT_MMC_DMA_EN; 
#endif

/* 3. start clock */
	if ( (ret = pxa_mmc_start_bus_clock( ctrlr )) )
		goto error;
    
/* 4. wait for cmd to complete */
	MMC_DEBUG( MMC_DEBUG_LEVEL3, "CMD25(0x%04x%04x)\n", argh, argl );
	if ( (ret = pxa_mmc_complete_cmd( ctrlr, MMC_R1, TRUE )) ) 
		goto error;

/* 6. transfer the data to the caller supplied buffer */
	while ( transfer->cnt > 0 ) {
		if ( (ret = pxa_mmc_copy_to_buffer( ctrlr, transfer->type,
				transfer->buf, transfer->cnt )) < 0 )
			goto error;
        
		if ( (ret = pxa_mmc_write_buffer( ctrlr, ret )) < 0 )
			goto error;

		transfer->buf += ret;
		transfer->cnt -= ret;
	}
    
	if ( __pxa_mmc_set_state( ctrlr, PXA_MMC_FSM_END_IO ) )
		goto error;

	if ( (ret = pxa_mmc_complete_io( ctrlr, transfer->cmd, transfer->mode )) )
		goto error;
    
	ret = 0;    
error:
	__LEAVE( "ret=%d", ret );
	return ret;
}

static void pxa_mmc_irq( int irq, void *dev_id, struct pt_regs *regs )
{
	mmc_controller_t ctrlr = (mmc_controller_t)dev_id;
	pxa_mmc_hostdata_t hostdata = (pxa_mmc_hostdata_t)ctrlr->host_data;
#ifdef PIO
	register int i, cnt;
	register char *buf;
#endif    

	hostdata->mmc_i_reg = MMC_I_REG;
	hostdata->mmc_stat = MMC_STAT;
	hostdata->mmc_cmdat = MMC_CMDAT;
#ifdef PIO
	MMC_DEBUG( MMC_DEBUG_LEVEL3, 
		"MMC interrupt: state=%s, status=%x, i_reg=%x,"
		" pos=%p, cnt=%d\n", PXA_MMC_STATE_LABEL( hostdata->state ), 
		hostdata->mmc_stat, hostdata->mmc_i_reg,
		hostdata->iobuf.buf.pos, hostdata->iobuf.buf.cnt );
#else /* DMA */
	MMC_DEBUG( MMC_DEBUG_LEVEL3, 
		"MMC interrupt: state=%s, status=%x, i_reg=%x\n",
		PXA_MMC_STATE_LABEL( hostdata->state ), 
		hostdata->mmc_stat, hostdata->mmc_i_reg );
#endif
#if CONFIG_MMC_DEBUG_IRQ 
	if ( --hostdata->irqcnt <= 0 ) {
		printk( KERN_INFO __FUNCTION__"(): irqcnt exceeded\n" );
		goto complete;
	}
#endif
	switch ( hostdata->state ) {
	case PXA_MMC_FSM_IDLE:
	case PXA_MMC_FSM_CLK_OFF:
	case PXA_MMC_FSM_END_IO:
	case PXA_MMC_FSM_END_BUFFER:
	case PXA_MMC_FSM_END_CMD:
		goto complete;
#ifdef PIO   
	case PXA_MMC_FSM_BUFFER_IN_TRANSIT:
        	if ( hostdata->mmc_stat & MMC_STAT_ERRORS )
         		goto complete;
        
		buf = hostdata->iobuf.buf.pos;
        	cnt = (hostdata->iobuf.buf.cnt < 32) ? 
			hostdata->iobuf.buf.cnt : 32;
		if ( hostdata->mmc_cmdat & MMC_CMDAT_WRITE ) {
			if ( !(hostdata->mmc_stat & MMC_STAT_XMIT_FIFO_EMPTY) )
				break;
			for ( i = 0; i < cnt; i++ ) 
				MMC_TXFIFO = *buf++;
			if ( cnt < 32 ) 
				MMC_PRTBUF = MMC_PRTBUF_BUF_PART_FULL; 
		} else { /* i.e. MMC_CMDAT_READ */
			if( !(hostdata->mmc_stat & MMC_STAT_RECV_FIFO_FULL) )
				break;
			for( i = 0; i < cnt; i++ ) 
				*buf++ = MMC_RXFIFO;
		}
        
		hostdata->iobuf.buf.pos = buf;
		hostdata->iobuf.buf.cnt -= i;
		if ( hostdata->iobuf.buf.cnt <= 0 ) {
			(void)__pxa_mmc_set_state( ctrlr,
					PXA_MMC_FSM_END_BUFFER );
			MMC_DEBUG( MMC_DEBUG_LEVEL3, "buffer transferred\n" );
			goto complete;
		}
		break; 
#endif /* PIO */
	default:
		MMC_DEBUG( MMC_DEBUG_LEVEL3, "unexpected state %d\n", 
				hostdata->state );
		goto complete;
	}   
	return;	
complete:   
	MMC_I_MASK = MMC_I_MASK_ALL;
	complete( &hostdata->completion );
	return;
}

#ifndef PIO
static void pxa_mmc_dma_irq( int irq, void *dev_id, struct pt_regs *regs )
{
	mmc_controller_t ctrlr = (mmc_controller_t)dev_id;
	pxa_mmc_hostdata_t hostdata = (pxa_mmc_hostdata_t)ctrlr->host_data;
	u32 dcsr;
	u32 ddadr;
	int chan = hostdata->iobuf.buf.chan;
	
	ddadr = DDADR( chan );
	dcsr = DCSR( chan );
	DCSR( chan ) = dcsr & ~DCSR_STOPIRQEN;
	
	MMC_DEBUG( MMC_DEBUG_LEVEL3, 
			"MMC DMA interrupt: chan=%d ddadr=0x%08x "
			"dcmd=0x%08x dcsr=0x%08x\n", 
			chan, ddadr, DCMD( chan ), dcsr );
/* bus error */
	if ( dcsr & DCSR_BUSERR ) {
		MMC_DEBUG( MMC_DEBUG_LEVEL3, "bus error on DMA channel %d\n",
				chan );
		__pxa_mmc_set_state( ctrlr, PXA_MMC_FSM_ERROR );
		goto complete;
	}
/* data transfer completed */
	if ( dcsr & DCSR_ENDINTR ) {
		MMC_DEBUG( MMC_DEBUG_LEVEL3, "buffer transferred\n" );
		__pxa_mmc_set_state( ctrlr, PXA_MMC_FSM_END_BUFFER );
		goto complete;
	}
	return;
complete:
	complete( &hostdata->completion );
	return;
}
#endif

static int pxa_mmc_init( mmc_controller_t ctrlr )
{
	int ret = -ENODEV;
	pxa_mmc_hostdata_t hostdata = (pxa_mmc_hostdata_t)ctrlr->host_data;
#ifndef PIO
	register int i;
	register pxa_dma_desc *desc;
#endif
	__ENTER0( );
/* hardware initialization */
/* I. prepare to transfer data */
/* 1. allocate buffer */
#ifndef PIO
	hostdata->iobuf.buf.read_desc = consistent_alloc( GFP_KERNEL,
				(PXA_MMC_IODATA_SIZE>>5)
				* sizeof( pxa_dma_desc ),
				&hostdata->iobuf.buf.read_desc_phys_addr );
	if ( !hostdata->iobuf.buf.read_desc ) {
		ret = -ENOMEM;
		goto error;
	}
	hostdata->iobuf.buf.write_desc = consistent_alloc( GFP_KERNEL,
				(PXA_MMC_IODATA_SIZE>>5)
				* sizeof( pxa_dma_desc ),
				&hostdata->iobuf.buf.write_desc_phys_addr );
	if ( !hostdata->iobuf.buf.write_desc ) {
		ret = -ENOMEM;
		goto error;
	}
	hostdata->iobuf.iodata = consistent_alloc( GFP_ATOMIC, 
					PXA_MMC_IODATA_SIZE,
					&hostdata->iobuf.buf.phys_addr );
#else
	hostdata->iobuf.iodata = kmalloc( PXA_MMC_IODATA_SIZE, GFP_ATOMIC );
#endif
	if ( !hostdata->iobuf.iodata ) {
		ret = -ENOMEM;
		goto error;
	}
/* 2. initialize iobuf */
	hostdata->iobuf.blksz = PXA_MMC_BLKSZ_MAX;
	hostdata->iobuf.bufsz = PXA_MMC_IODATA_SIZE;
	hostdata->iobuf.nob = PXA_MMC_BLOCKS_PER_BUFFER;
#ifndef PIO
  /* request DMA channel */
	if ( (hostdata->iobuf.buf.chan = pxa_request_dma( "MMC", DMA_PRIO_LOW,
					pxa_mmc_dma_irq, ctrlr )) < 0 ) {
		MMC_ERROR( "failed to request DMA channel\n" );
		goto error;
	}
	
	DRCMRRXMMC = hostdata->iobuf.buf.chan | DRCMR_MAPVLD;
	DRCMRTXMMC = hostdata->iobuf.buf.chan | DRCMR_MAPVLD;
	
	for ( i = 0; i < ((PXA_MMC_IODATA_SIZE>>5) - 1); i++ ) {
		desc = &hostdata->iobuf.buf.read_desc[i];
		desc->ddadr = hostdata->iobuf.buf.read_desc_phys_addr
				+ ((i + 1) * sizeof( pxa_dma_desc ));
		desc->dsadr = MMC_RXFIFO_PHYS_ADDR;
		desc->dtadr = hostdata->iobuf.buf.phys_addr + (i<<5);
		desc->dcmd = DCMD_FLOWSRC|DCMD_INCTRGADDR
				|DCMD_WIDTH1|DCMD_BURST32|(1<<5);
		
		desc = &hostdata->iobuf.buf.write_desc[i];
		desc->ddadr = hostdata->iobuf.buf.write_desc_phys_addr
				+ ((i + 1) * sizeof( pxa_dma_desc ));
		desc->dsadr = hostdata->iobuf.buf.phys_addr + (i<<5);
		desc->dtadr = MMC_TXFIFO_PHYS_ADDR;
		desc->dcmd = DCMD_FLOWTRG|DCMD_INCSRCADDR
				|DCMD_WIDTH1|DCMD_BURST32|(1<<5);
	}
	desc = &hostdata->iobuf.buf.read_desc[i];
	desc->ddadr = (hostdata->iobuf.buf.read_desc_phys_addr +
				(i + 1) * sizeof( pxa_dma_desc))|DDADR_STOP;
	desc->dsadr = MMC_RXFIFO_PHYS_ADDR;
	desc->dtadr = hostdata->iobuf.buf.phys_addr + (i<<5);
	desc->dcmd = DCMD_FLOWSRC|DCMD_INCTRGADDR
			|DCMD_WIDTH1|DCMD_BURST32|(1<<5);
		
	desc = &hostdata->iobuf.buf.write_desc[i];
	desc->ddadr = (hostdata->iobuf.buf.write_desc_phys_addr +
				(i + 1) * sizeof( pxa_dma_desc))|DDADR_STOP;
	desc->dsadr = hostdata->iobuf.buf.phys_addr + (i<<5);
	desc->dtadr = MMC_TXFIFO_PHYS_ADDR;
	desc->dcmd = DCMD_FLOWTRG|DCMD_INCSRCADDR
			|DCMD_WIDTH1|DCMD_BURST32|(1<<5);
#endif
/* II. MMC */
/*  1) request irq */
	if ( request_irq( IRQ_MMC, pxa_mmc_irq, 0, "MMC controller", ctrlr ) ) {
		MMC_ERROR( "failed to request IRQ_MMC\n" );
		goto error;
	}
    
/*  2) initialize h/w and ctrlr */
	set_GPIO_mode( GPIO6_MMCCLK_MD );
	set_GPIO_mode( GPIO8_MMCCS0_MD );
	CKEN |= CKEN12_MMC; /* enable MMC unit clock */

	ret = 0;
	goto out;
error:
#ifndef PIO
/* free DMA resources */
	if ( hostdata->iobuf.buf.chan >= 0 ) {
		DRCMRRXMMC = 0;
		DRCMRTXMMC = 0;
		pxa_free_dma( hostdata->iobuf.buf.chan );
	}
	if ( hostdata->iobuf.iodata )
		consistent_free( hostdata->iobuf.iodata, 
				 PXA_MMC_IODATA_SIZE,
				 hostdata->iobuf.buf.phys_addr );
	if ( hostdata->iobuf.buf.read_desc )	
		consistent_free( hostdata->iobuf.buf.read_desc,
				(PXA_MMC_IODATA_SIZE>>5)
				* sizeof( pxa_dma_desc ),
				hostdata->iobuf.buf.read_desc_phys_addr );
	if ( hostdata->iobuf.buf.write_desc )	
		consistent_free( hostdata->iobuf.buf.write_desc,
				(PXA_MMC_IODATA_SIZE>>5)
				* sizeof( pxa_dma_desc ),
				hostdata->iobuf.buf.write_desc_phys_addr );
#else
	kfree( hostdata->iobuf.iodata );
#endif
out:
	__LEAVE( "ret=%d", ret );
	return ret; 
}

static void pxa_mmc_remove( mmc_controller_t ctrlr )
{
	pxa_mmc_hostdata_t hostdata = (pxa_mmc_hostdata_t)ctrlr->host_data;
	
	__ENTER0();

/*  1) free buffer(s) */
#ifndef PIO
	consistent_free( hostdata->iobuf.iodata, PXA_MMC_IODATA_SIZE,
			 hostdata->iobuf.buf.phys_addr );
	consistent_free( hostdata->iobuf.buf.read_desc,
	 		 (PXA_MMC_IODATA_SIZE>>5)
			 * sizeof( pxa_dma_desc ),
			 hostdata->iobuf.buf.read_desc_phys_addr );
	consistent_free( hostdata->iobuf.buf.write_desc,
			 (PXA_MMC_IODATA_SIZE>>5)
			 * sizeof( pxa_dma_desc ),
			 hostdata->iobuf.buf.write_desc_phys_addr );
/*  2) release DMA channel */
	if ( hostdata->iobuf.buf.chan >= 0 ) {
		DRCMRRXMMC = 0;
		DRCMRTXMMC = 0;
		pxa_free_dma( hostdata->iobuf.buf.chan );
	}
#else
	kfree( hostdata->iobuf.iodata );
#endif
/* II. MMC */
/*  1) release irq */
	free_irq( IRQ_MMC, ctrlr );
	CKEN &= ~CKEN12_MMC; /* disable MMC unit clock */

	__LEAVE0();
}

static int pxa_mmc_probe( mmc_controller_t ctrlr )
{
	int ret = -ENODEV;
    
	__ENTER0();
/* TODO: hardware probe: i.g. read registers */
	ret = 1;

	__LEAVE( "ret=%d", ret );
	return ret;
}

#ifdef CONFIG_PM
static int pxa_mmc_suspend( mmc_controller_t ctrlr )
{
	int ret = -EBUSY;
	pxa_mmc_hostdata_t hostdata = (pxa_mmc_hostdata_t)ctrlr->host_data;
	
	__ENTER0();

	MMC_DEBUG( MMC_DEBUG_LEVEL2, "state=%s\n", 
			PXA_MMC_STATE_LABEL( hostdata->state ) );

	if ( hostdata->state == PXA_MMC_FSM_IDLE ) {
		/* save registers */
		SAVED_MMC_CLKRT = MMC_CLKRT;
		SAVED_MMC_RESTO = MMC_RESTO;
		SAVED_MMC_SPI = MMC_SPI;
		SAVED_DRCMRRXMMC = DRCMRRXMMC;
		SAVED_DRCMRTXMMC = DRCMRTXMMC;

#if 0 /* FIXME */
		/* send CMD0 */
		if ( (ret = pxa_mmc_stop_bus_clock( ctrlr )) )
			goto error;

		MMC_CMD = CMD(0); /* CMD0 with zero argument */
		MMC_ARGH = 0UL;
		MMC_ARGL = 0UL; 
		MMC_CMDAT = 0UL;
    
		MMC_DEBUG( MMC_DEBUG_LEVEL3, "CMD0(0x%04x%04x)\n", 0UL, 0UL );
		if ( (ret = pxa_mmc_complete_cmd( ctrlr, MMC_NORESPONSE, 
						FALSE )) ) 
		{
			ret = -EIO;
			goto error;
		}
#endif		
	
		set_GPIO_mode( GPIO6_MMCCLK );
		set_GPIO_mode( GPIO8_MMCCS0 );
		CKEN &= ~CKEN12_MMC; /* disable MMC unit clock */
		
		hostdata->suspended = TRUE;
		ret = 0;
	}
error:
	__LEAVE( "ret=%d", ret );
	return ret;
}

static void pxa_mmc_resume( mmc_controller_t ctrlr )
{
	pxa_mmc_hostdata_t hostdata = (pxa_mmc_hostdata_t)ctrlr->host_data;

	__ENTER0();
	
	if ( hostdata->suspended == TRUE ) {
		set_GPIO_mode( GPIO6_MMCCLK_MD );
		set_GPIO_mode( GPIO8_MMCCS0_MD );
		CKEN |= CKEN12_MMC; /* enable MMC unit clock */
		
		/* restore registers */
		MMC_CLKRT = SAVED_MMC_CLKRT;
		MMC_RESTO = SAVED_MMC_RESTO;
		MMC_SPI = SAVED_MMC_SPI;
		DRCMRRXMMC = SAVED_DRCMRRXMMC;
		DRCMRTXMMC = SAVED_DRCMRTXMMC;

		hostdata->suspended = FALSE;

		mmc_update_card_stack( ctrlr->slot ); /* FIXME */
	}
	
	__LEAVE0();
	return;
}
#endif

static mmc_controller_tmpl_rec_t pxa_mmc_controller_tmpl_rec = {
	owner:			THIS_MODULE,
	name:			"PXA250",
	block_size_max:		PXA_MMC_BLKSZ_MAX,
	nob_max:		PXA_MMC_NOB_MAX,
	probe:			pxa_mmc_probe,
	init:			pxa_mmc_init,
	remove:			__devexit_p( pxa_mmc_remove ),
#ifdef CONFIG_PM
	suspend:		pxa_mmc_suspend,
	resume:			pxa_mmc_resume,
#endif /* CONFIG_PM */
	update_acq:		pxa_mmc_update_acq,
//	single_card_acq:	pxa_mmc_single_card_acq,
	init_card_stack:	pxa_mmc_init_card_stack,
	check_card_stack:	pxa_mmc_check_card_stack,
	setup_card:		pxa_mmc_setup_card,
	stream_read:		pxa_mmc_stream_read,
	read_block:		pxa_mmc_read_block,
	read_mblock:		pxa_mmc_read_mblock,
	stream_write:		pxa_mmc_stream_write,
	write_block:		pxa_mmc_write_block,
	write_mblock:		pxa_mmc_write_mblock
	/* TODO
	sg_io:			pxa_mmc_sg_io
	 */
	/* TODO: 
	 *  erase, 
	 *  write protection,
	 *  lock/password management methods
	 */
};

static int __devinit mmc_pxa_module_init( void )
{
    	int ret = -ENODEV;
	
	__ENTER0();
	host = mmc_register( MMC_REG_TYPE_HOST, &pxa_mmc_controller_tmpl_rec,
			sizeof( pxa_mmc_hostdata_rec_t ) );
	if ( !host ) {
		MMC_DEBUG( MMC_DEBUG_LEVEL0, 
				"failed to register with MMC core\n" );
		goto error;
	}
	
	printk( KERN_INFO "%s MMC controller driver, $Revision: 0.3.1.12 $\n",
			pxa_mmc_controller_tmpl_rec.name );
	ret = 0;
error:
	__LEAVE( "ret=%d", ret );
	return ret;
}

static void __devexit mmc_pxa_module_cleanup( void )
{
	mmc_unregister( MMC_REG_TYPE_HOST, host );
}

EXPORT_NO_SYMBOLS;

MODULE_LICENSE( "GPL" );

module_init( mmc_pxa_module_init );
module_exit( mmc_pxa_module_cleanup );

