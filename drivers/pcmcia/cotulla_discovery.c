/*
 * drivers/pcmcia/cotulla_discovery.c
 *
 * PCMCIA implementation routines for Discovery
 *
 * Copyright (C) 2002 Richard Rau (richard.rau@msa.hinet.net)
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
 * This code is based on drivers/pcmcia/sa1100_collie.c
 *
 * May be copied or modified under the terms of the GNU General Public
 * License.  See linux/COPYING for more information.
 *
 */
#include <linux/config.h>
#include <linux/kernel.h>
#include <linux/sched.h>
#include <linux/delay.h>
#include <pcmcia/cs_types.h>
#include <pcmcia/ss.h>

#include <asm/hardware.h>
#include <asm/irq.h>
#include <asm/io.h>
#include <asm/arch/pcmcia.h>
#include <asm/arch/discovery_gpio.h>
#include <asm/arch/discovery_asic3.h>
#include <asm/arch/i2sc.h>

#ifdef CONFIG_COLLIE_TS
#else
static unsigned short keep_vs[2];
#define	NO_KEEP_VS 0x0001
#endif
static unsigned char keep_reset_done = 0;
unsigned char vcc_on = 0;
static unsigned long backpackptr;

extern void set_GPIO_IRQ_edge(int gpio_mask, int edge, int gpio_num);
extern void clr_discovery_bpgpio(u8 x);
extern void set_discovery_bpgpio(u8 x);

/**********************/
/* I2C Bus definition */
/**********************/
#define	MICRON_DEVICE_ADDR	0x4A
/* IO CONTROL 00~1F */
#define	CMD_CF_3V_ON		0x01
/* READ INFORMATION */
#define	CMD_READ_EBAT		0x21
#define	CMD_READ_IMIN		0x22
#define	CMD_READ_TEMP		0x23
#define	CMD_READ_BATTID		0x24
#define	CMD_READ_STATUS		0x25
/* GET PIN STATUS */
#define	CMD_PIN_STATUS		0x41
#define	STUFF_DATA			0xd4 //0xff
#define	CF_3V_OPEN			0x01 //turn on
#define	CF_3V_CLOSE			0x00 //turn off

unsigned char cf_detect;

void collie_pcmcia_init_reset(void)
{
#ifdef CONFIG_COLLIE_TS
	/* CF_BUS_OFF */
	cf_buf_ctrl = CF_BUS_POWER_OFF;
	CF_BUF_CTRL = cf_buf_ctrl;

#else	/* ! CONFIG_COLLIE_TS */

/*
#define	SCP_INIT_DATA(adr,dat)	(((adr)<<16)|(dat))
#define	SCP_INIT_DATA_END	((unsigned long)-1)
	static const unsigned long scp_init[] =
	{
		SCP_INIT_DATA(SCP_MCR,0x0100),
		SCP_INIT_DATA(SCP_CDR,0x0000),  // 04
		SCP_INIT_DATA(SCP_CPR,0x0000),  // 0C
		SCP_INIT_DATA(SCP_CCR,0x0000),  // 10
		SCP_INIT_DATA(SCP_IMR,0x0000),  // 18
		SCP_INIT_DATA(SCP_IRM,0x00FF),  // 14
		SCP_INIT_DATA(SCP_ISR,0x0000),  // 1C
		SCP_INIT_DATA(SCP_IRM,0x0000),
		SCP_INIT_DATA_END
	};
	int	i;
	for(i=0; scp_init[i] != SCP_INIT_DATA_END; i++)
	{
		int	adr = scp_init[i] >> 16;
		SCP_REG(adr) = scp_init[i] & 0xFFFF;
	}
	keep_vs[0] = NO_KEEP_VS;
*/
#endif
}

void set_cf_irq()
{
	if (ASIC3_GPIO_PSTS_D & CF_DETECT) {
		ASIC3_GPIO_ETSEL_D &= ~CF_DETECT;	// not exist
		cf_detect = 0;
	} else {
		ASIC3_GPIO_ETSEL_D |= CF_DETECT;	// exist
		cf_detect = 1;
	}	
}

static void discovery_cf_status_change(int irq, void *dev, struct pt_regs *regs){
 	printk("CF status falling detected! \n");
}

static int collie_pcmcia_init(struct pcmcia_init *init)
{
	int irq, res;

	keep_reset_done = 0;
	vcc_on = 0;

	//----- set CF_IRQ, CF_DETECT pin as input -----
	GPDR(0) &= ~ GPIO_DISCOVERY_CF_IRQ;
		
	//----- set CF_RESET as output -----
	GPCR(0) = GPIO_DISCOVERY_CF_RESET;
	GPDR(0) |= GPIO_DISCOVERY_CF_RESET;
	
	ASIC3_GPIO_MASK_D &= ~CF_DETECT;
	ASIC3_GPIO_DIR_D &= ~CF_DETECT;
//	ASIC3_GPIO_INTYP_D &= ~CF_DETECT; // Level
//	ASIC3_GPIO_LSEL_D &= ~CF_DETECT;
	ASIC3_GPIO_INTYP_D |= CF_DETECT; // Edge	
	set_cf_irq();

	ASIC3_GPIO_MASK_D &= ~CF_STSCHG;
	ASIC3_GPIO_DIR_D &= ~CF_STSCHG;
	ASIC3_GPIO_INTYP_D |= CF_STSCHG; // Edge	
	ASIC3_GPIO_ETSEL_D &= ~CF_STSCHG;

  	set_GPIO_IRQ_edge(GPIO_DISCOVERY_CF_IRQ, GPIO_FALLING_EDGE, 0);

  	// Register interrupts
  	irq = IRQ_ASIC3_CF_DETECT;
  	res = request_irq( irq, init->handler, SA_INTERRUPT, "PCMCIA_CD0", NULL );
  	if( res < 0 )
   		goto irq_err;
   		
 	res = request_irq( IRQ_ASIC3_CF_STSCHG, discovery_cf_status_change, SA_INTERRUPT, "PCMCIA_CD0", NULL );
  	if( res < 0 )
   		goto irq_err;

	backpackptr = ioremap(0x14000000, 4);    // Richard 0710  Output '0' to physical add. 0x14000000 to avoid .. 
	if (NULL != backpackptr) {
		*((u16*)backpackptr) = 0;		     // Richard 0710  
    		iounmap(backpackptr);
    	}

  	return 1;

irq_err:
  	printk( KERN_ERR __FUNCTION__ ": Request for IRQ %u failed\n", irq );
  	return -1;

/*
	// set GPIO_CF_CD & GPIO_CF_IRQ as inputs
	GPDR &= ~(GPIO_CF_CD|GPIO_CF_IRQ);

	// Set transition detect
#ifdef CONFIG_COLLIE_TS
	set_GPIO_IRQ_edge( GPIO_CF_CD, GPIO_BOTH_EDGES );
#else
	set_GPIO_IRQ_edge( GPIO_CF_CD, GPIO_FALLING_EDGE );
#endif
	set_GPIO_IRQ_edge( GPIO_CF_IRQ, GPIO_FALLING_EDGE );

	// Register interrupts
	irq = IRQ_GPIO_CF_CD;
	res = request_irq(irq, init->handler, 
		SA_INTERRUPT, "CF_CD", NULL);
	if( res < 0 ) goto irq_err;

	// enable interrupt
#ifdef CONFIG_COLLIE_TS
#else
	SCP_REG_IMR = 0x00C0;
	SCP_REG_MCR = 0x0101;
	keep_vs[0] = keep_vs[1] = NO_KEEP_VS;
#endif
*/
	// There's only one slot, but it's "Slot 0":
	//return 2;
/*
irq_err:
	printk( KERN_ERR "%s: Request for IRQ %d failed\n",
		__FUNCTION__, irq );
	return -1;
*/
}

static int collie_pcmcia_shutdown(void)
{
	//----- disable IRQs -----
  	free_irq( IRQ_ASIC3_CF_DETECT, NULL );

  	//----- Disable CF bus: -----
//	GPLR(0) &= ~GPIO_DISCOVERY_CF_RESET;

	return 0;
}

static int collie_pcmcia_socket_state(struct pcmcia_state_array* state_array)
{
	unsigned long levels;
	unsigned short wASIC1Level;;

	if(state_array->size < 1) 		// CYB
		return -1;

	memset(state_array->state, 0, 
	       (state_array->size)*sizeof(struct pcmcia_state));
	
#ifdef CONFIG_COLLIE_TS
	levels = GPLR;

	state_array->state[0].detect = ((levels & GPIO_CF_CD)==0)?1:0;
	state_array->state[0].ready  = (levels & GPIO_CF_IRQ)?1:0;
	state_array->state[0].bvd1   = 1; /* Not available */
	state_array->state[0].bvd2   = 1; /* Not available */
	state_array->state[0].wrprot = 0; /* Not available */
	state_array->state[0].vs_3v  = 1; /* Can only apply 3.3V */
	state_array->state[0].vs_Xv  = 0;

#else	/* ! CONFIG_COLLIE_TS */
  	levels=GPLR(0);
//  	wASIC1Level = DISCOVERY_ASIC1_GPIO_STATUS;
//	wASIC1Level = ASIC3_GPIO_PSTS_D;
//	printk("GPLR0 %x, state %d\n", GPLR(0), levels & GPIO_DISCOVERY_CF_IRQ);
//  	state_array->state[0].detect=(( ASIC3_GPIO_PSTS_D & CF_DETECT)==0)?1:0;
	state_array->state[0].detect= cf_detect;
  	state_array->state[0].ready=((levels & GPIO_DISCOVERY_CF_IRQ) && (ASIC3_GPIO_PSTS_B & BUFFER_OE) && ((GPLR(0) & GPIO_DISCOVERY_CF_RESET) == 0))?1:0;
  	state_array->state[0].bvd1= 0;
  	state_array->state[0].bvd2= 0;
  	state_array->state[0].wrprot=0; /* Not available */
  	state_array->state[0].vs_3v=1;
  	state_array->state[0].vs_Xv=0;

/*
	cpr = SCP_REG_CPR;
	//SCP_REG_CDR = 0x0002;
	SCP_REG_IRM = 0x00FF;
	SCP_REG_ISR = 0x0000;
	SCP_REG_IRM = 0x0000;
	csr = SCP_REG_CSR;
	if( csr & 0x0004 ){
		// card eject
		SCP_REG_CDR = 0x0000;
		keep_vs[0] = NO_KEEP_VS;
	}
	else if( !(keep_vs[0] & NO_KEEP_VS) ){
		// keep vs1,vs2
		SCP_REG_CDR = 0x0000;
		csr |= keep_vs[0];
	}
	else if( cpr & 0x0003 ){
		// power on
		SCP_REG_CDR = 0x0000;
		keep_vs[0] = (csr & 0x00C0);
	}
	else{	// card detect
		SCP_REG_CDR = 0x0002;
	}

#ifdef COLLIE_PCMCIA_DEBUG
	printk("%s(): cpr=%04X, csr=%04X\n", __FUNCTION__, cpr, csr);
#endif

	state_array->state[0].detect = (csr & 0x0004)? 0:1;
	state_array->state[0].ready  = (csr & 0x0002)? 1:0;
	state_array->state[0].bvd1   = (csr & 0x0010)? 1:0;
	state_array->state[0].bvd2   = (csr & 0x0020)? 1:0;
	state_array->state[0].wrprot = (csr & 0x0008)? 1:0;
	state_array->state[0].vs_3v  = (csr & 0x0040)? 0:1;
	state_array->state[0].vs_Xv  = (csr & 0x0080)? 0:1;

	if( (cpr & 0x0080) && ((cpr & 0x8040) != 0x8040) ){
		printk(KERN_ERR "%s(): CPR=%04X, Low voltage!\n",
			__FUNCTION__, cpr);
	}
*/
#endif

	return 1;
}

static int collie_pcmcia_get_irq_info(struct pcmcia_irq_info* info)
{
	if (info->sock > 1)
		return -1;

	if (info->sock == 0) {
		info->irq=IRQ_DISCOVERY_CF_IRQ;
	}

	return 0;
}

#define	BPGPIO_DISCOVERY_CF_3V_ON		(1 << 3)
#define	BPGPIO_DISCOVERY_CF_5V_ON		(1 << 4)
#define	BPGPIO_DISCOVERY_CF_RESET		(1 << 0)

static int collie_pcmcia_configure_socket(const struct pcmcia_configure* 
					  configure)
{
	unsigned long flags;
	unsigned char ret_on3v;

	if(configure->sock > 1)
		return -1;

	if(ASIC3_GPIO_PSTS_D & BACKPACK_DETECT)
		return 0;

	save_flags_cli(flags);
	
	switch (configure->vcc)
	{
		case 0:
//			printk(" %s() VCC off\n", __FUNCTION__);
			backpackptr = ioremap(0x14000000, 4);    // Richard 0710  Output '0' to physical add. 0x14000000 to avoid .. 
			if (NULL != backpackptr) {
				*((u16*)backpackptr) = 0;		     // Richard 0710  
    				iounmap(backpackptr);
    			}
    			
			ASIC3_GPIO_PIOD_B &= ~BUFFER_OE;
			while (vcc_on) {			
        			ASIC3_GPIO_PIOD_C |=  BACK_I2C_ON;
				mdelay(10);
        			if (!WRITE_I2C_BYTE(MICRON_DEVICE_ADDR, CMD_CF_3V_ON, CF_3V_CLOSE)) {
        				if (0 == READ_I2C_BYTE(MICRON_DEVICE_ADDR))	{
        					vcc_on = 0;
        					printk("turn power off succeed\n");
        				}
        			}
        			ASIC3_GPIO_PIOD_C &=  ~BACK_I2C_ON;
        			
        			if (vcc_on) {
        				printk("turn power off failed\n");
        				ASIC3_GPIO_PIOD_C &= ~BACK_RESET; 
					mdelay(100);
					ASIC3_GPIO_PIOD_C |= BACK_RESET;
				}
				if (ASIC3_GPIO_PSTS_D & BACKPACK_DETECT) { // Backpack remove
					restore_flags(flags);
					return -1;				
				}
			}
			keep_reset_done = 0;
			break;
		
		case 33:
//			printk(" %s() VCC 3V\n", __FUNCTION__);
			if (!vcc_on)  {
				while (!vcc_on) {			
        				ASIC3_GPIO_PIOD_C |=  BACK_I2C_ON;
					mdelay(10);
        				if (!WRITE_I2C_BYTE(MICRON_DEVICE_ADDR, CMD_CF_3V_ON, CF_3V_OPEN)) {
        					ret_on3v = READ_I2C_BYTE(MICRON_DEVICE_ADDR);
        					if (1 == ret_on3v)	{
        						vcc_on = 1;
        						printk("turn on 3v succeed\n");
        					} else if (2 == ret_on3v) {
        						printk("turn on 3v faliled because battery low\n");
        						ASIC3_GPIO_PIOD_C &=  ~BACK_I2C_ON;
							restore_flags(flags);
							return -1;		// Tune on 3v failed
        					}					
         				}         				
        				ASIC3_GPIO_PIOD_C &=  ~BACK_I2C_ON;
        			
        				if (!vcc_on) {
        					printk("turn on 3v failed\n");
        					ASIC3_GPIO_PIOD_C &= ~BACK_RESET; 
						mdelay(100);
						ASIC3_GPIO_PIOD_C |= BACK_RESET;
					}
					if (ASIC3_GPIO_PSTS_D & BACKPACK_DETECT) { // Backpack remove
						restore_flags(flags);
						return -1;				
					}
				}
				ASIC3_GPIO_PIOD_B |= BUFFER_OE;

			}
			if( !keep_reset_done )
				GPSR(0) = GPIO_DISCOVERY_CF_RESET;
			break;
		case 50:
//			printk(" %s() VCC 5V, but we turn on 3V\n", __FUNCTION__);
			if (!vcc_on)  {
				while (!vcc_on) {			
        				ASIC3_GPIO_PIOD_C |=  BACK_I2C_ON;
					mdelay(10);
        				if (!WRITE_I2C_BYTE(MICRON_DEVICE_ADDR, CMD_CF_3V_ON, CF_3V_OPEN)) {
        					ret_on3v = READ_I2C_BYTE(MICRON_DEVICE_ADDR);
        					if (1 == ret_on3v)	{
        						vcc_on = 1;
        						printk("turn on 3v succeed\n");
        					} else if (2 == ret_on3v) {
        						printk("turn on 3v faliled because battery low\n");
        						ASIC3_GPIO_PIOD_C &=  ~BACK_I2C_ON;
							restore_flags(flags);
							return -1;		// Tune on 3v failed
        					}					
         				}         				
        				ASIC3_GPIO_PIOD_C &=  ~BACK_I2C_ON;
        			
        				if (!vcc_on) {
        					printk("turn on 3v failed\n");
        					ASIC3_GPIO_PIOD_C &= ~BACK_RESET; 
						mdelay(100);
						ASIC3_GPIO_PIOD_C |= BACK_RESET;
					}
					if (ASIC3_GPIO_PSTS_D & BACKPACK_DETECT) { // Backpack remove
						restore_flags(flags);
						return -1;				
					}
				}
				ASIC3_GPIO_PIOD_B |= BUFFER_OE;

			}
			if( !keep_reset_done )
				GPSR(0) = GPIO_DISCOVERY_CF_RESET;
			break;
		
		default:
			printk(" %s(): unrecognized Vcc %u\n", __FUNCTION__, configure->vcc);
			restore_flags(flags);
			return -1;
	}
	
	if (configure->reset) {	
//		printk(" %s() configure->reset\n", __FUNCTION__);
		GPSR(0) = GPIO_DISCOVERY_CF_RESET;
		keep_reset_done = 1;
//	} else {
	} else if( keep_reset_done ) {
//		printk(" %s() not configure->reset\n", __FUNCTION__);
		GPCR(0) = GPIO_DISCOVERY_CF_RESET;
	}
	
	/* Silently ignore Vpp, output enable, speaker enable. */
	
	restore_flags(flags);
	
	return 0;	
/*	
	unsigned long flags;
#ifdef CONFIG_COLLIE_TS
#else
	unsigned short cpr, ncpr, ccr, nccr, mcr, nmcr, imr, nimr;
#endif

	if (configure->sock > 1)
		return -1;

	if (configure->sock != 0)
	  	return 0;

#ifdef COLLIE_PCMCIA_DEBUG
	printk("%s(): sk=%d, vc=%d, vp=%d, m=%04X, oe=%d, rs=%d, io=%d\n",
		__FUNCTION__, configure->sock, 
		configure->vcc, configure->vpp, configure->masks,
		configure->flags&SS_OUTPUT_ENA ? 1:0,
		configure->flags&SS_RESET ? 1:0,
		configure->flags&SS_IOCARD ? 1:0);
#endif

	switch( configure->vcc ){
	case	0:	break;
	case 	33:	break;
	case	50:	break;
	default:
		printk(KERN_ERR "%s(): unrecognized Vcc %u\n",
			__FUNCTION__, configure->vcc);
		return -1;
	}
	if( (configure->vpp!=configure->vcc) && (configure->vpp!=0) ){
		printk(KERN_ERR "%s(): CF slot cannot support Vpp %u\n",
			__FUNCTION__, configure->vpp);
		return -1;
	}

	save_flags_cli(flags);

#ifdef CONFIG_COLLIE_TS
	switch (configure->vcc) {
	case 0:
		cf_buf_ctrl |= CF_BUS_POWER_OFF;
		break;

	case 50:
#if 0
		cf_buf_ctrl &= ~CF_BUS_POWER_OFF;
		cf_buf_ctrl |=  CF_BUS_POWER_50V;
		break;
#else
	  	printk(KERN_WARNING
		       "%s(): CS asked for 5V, applying 3.3V...\n",
		       __FUNCTION__);
#endif

	case 33:  // Can only apply 3.3V to the CF slot.
		cf_buf_ctrl &= ~CF_BUS_POWER_OFF;
		cf_buf_ctrl |=  CF_BUS_POWER_33V;
		break;
	}

	if( configure->flags&SS_RESET )
		cf_buf_ctrl |=  CF_BUS_RESET;
	else	cf_buf_ctrl &= ~CF_BUS_RESET;

	CF_BUF_CTRL = cf_buf_ctrl;

#else	// ! CONFIG_COLLIE_TS
	nmcr = (mcr = SCP_REG_MCR) & ~0x0010;
	ncpr = (cpr = SCP_REG_CPR) & ~0x0083;
	nccr = (ccr = SCP_REG_CCR) & ~0x0080;
	nimr = (imr = SCP_REG_IMR) & ~0x003E;

	ncpr |= (configure->vcc == 33) ? 0x0001: 
		(configure->vcc == 50) ? 0x0002:
		0;
	nmcr |= (configure->flags&SS_IOCARD)? 0x0010: 0;
	ncpr |= (configure->flags&SS_OUTPUT_ENA)? 0x0080: 0;
	nccr |= (configure->flags&SS_RESET)? 0x0080: 0;
	nimr |=	((configure->masks&SS_DETECT) ? 0x0004: 0)|
		((configure->masks&SS_READY)  ? 0x0002: 0)|
		((configure->masks&SS_BATDEAD)? 0x0010: 0)|
		((configure->masks&SS_BATWARN)? 0x0020: 0)|
		((configure->masks&SS_STSCHG) ? 0x0010: 0)|
		((configure->masks&SS_WRPROT) ? 0x0008: 0);

	if( mcr != nmcr )
		SCP_REG_MCR = nmcr;
	if( cpr != ncpr )
		SCP_REG_CPR = ncpr;
	if( ccr != nccr )
		SCP_REG_CCR = nccr;
	if( imr != nimr )
		SCP_REG_IMR = nimr;
#endif

	restore_flags(flags);

	return 0;
*/	
}

struct pcmcia_low_level collie_pcmcia_ops = { 
	collie_pcmcia_init,
	collie_pcmcia_shutdown,
	collie_pcmcia_socket_state,
	collie_pcmcia_get_irq_info,
	collie_pcmcia_configure_socket
};

int is_pcmcia_card_present(int slot)
{
	return cf_detect;
}

EXPORT_SYMBOL(vcc_on);
EXPORT_SYMBOL(set_cf_irq);
