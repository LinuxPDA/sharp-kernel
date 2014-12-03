/*
 * drivers/pcmcia/pxa/pxa_sharpsl.c
 *
 * PCMCIA implementation routines for Poodle/Corgi
 *
 * (C) Copyright 2001 Lineo Japan, Inc.
 *
 * May be copied or modified under the terms of the GNU General Public
 * License.  See linux/COPYING for more information.
 *
 * Based on
 * drivers/pcmcia/sa1100_collie.c
 * drivers/pcmcia/sa1100_assabet.c
 *
 * PCMCIA implementation routines for Assabet
 *
 * ChangLog:
 *	12-Dec-2002 Sharp Corporation for Poodle and Corgi
 *	26-Feb-2004 Lineo Solutions, Inc.  2 slot support for Tosa
 *	07-Jul-2004 Lineo Solutions, Inc.  for Spitz
 *	17-Sep-2004 Sharp Corporation for Spitz
 */
#include <linux/config.h>
#include <linux/kernel.h>
#include <linux/sched.h>
#include <pcmcia/cs_types.h>
#include <pcmcia/ss.h>

#include <asm/hardware.h>
#include <asm/irq.h>
#include <asm/io.h>
#include <asm/delay.h>
#include <asm/arch/pcmcia.h>
#ifdef CONFIG_SABINAL_DISCOVERY
#include <asm/arch/discovery_gpio.h>
#include <asm/arch/discovery_asic3.h>
#include <asm/arch/i2sc.h>
#endif
#include <asm/sharp_apm.h>

#include <linux/delay.h>

#if defined(CONFIG_ARCH_PXA_TOSA)
#include <asm/arch/keyboard_tosa.h>
#include <asm/sharp_char.h>
#include <asm/sharp_keycode.h>
#endif

//#define SHARPSL_PCMCIA_DEBUG

#ifdef CONFIG_SABINAL_DISCOVERY

static unsigned char keep_reset_done = 0;
unsigned char vcc_on = 0;
static unsigned long backpackptr;

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

u8 is16 = 1;

#else

static unsigned char keep_vs[2];
#define	NO_KEEP_VS 0x0001
static unsigned char keep_rd[2];
#define RESET_DONE 0x0001
#endif

#if defined(CONFIG_ARCH_PXA_TOSA)
extern void set_jacketslot_error( int ); // tosa_battery.c
extern int current_jacketslot_error(void);
extern void force_cardslot_suspend(int num); // ../cs.c
#endif

void sharpsl_pcmcia_init_reset(void)
{
#if defined(CONFIG_ARCH_PXA_POODLE) || defined(CONFIG_ARCH_PXA_CORGI) || defined(CONFIG_ARCH_PXA_TOSA)
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
	keep_rd[0] = 0;
#if defined(CONFIG_PCMCIA_SHARP_SL_2SLOT)
	for(i=0; scp_init[i] != SCP_INIT_DATA_END; i++)
	{
		int	adr = scp_init[i] >> 16;
		SCP2_REG(adr) = scp_init[i] & 0xFFFF;
	}
	keep_vs[1] = NO_KEEP_VS;
	keep_rd[1] = 0;
#endif
#endif
}

static void sharpsl_cf_status_change(int irq, void *dev, struct pt_regs *regs){
 	printk("CF status falling detected! \n");
}

static int sharpsl_pcmcia_init(struct pcmcia_init *init)
{
	int irq, res;

#ifdef CONFIG_SABINAL_DISCOVERY
	keep_reset_done = 0;
	vcc_on = 0;

	//----- set CF_IRQ, CF_DETECT pin as input -----
	GPDR0 &= ~ GPIO_DISCOVERY_CF_IRQ;
		
	//----- set CF_RESET as output -----
	GPCR0 = GPIO_DISCOVERY_CF_RESET;
	GPDR0 |= GPIO_DISCOVERY_CF_RESET;
	
	ASIC3_GPIO_MASK_D &= ~CF_DETECT;
	ASIC3_GPIO_DIR_D &= ~CF_DETECT;
//	ASIC3_GPIO_INTYP_D &= ~CF_DETECT; // Level
//	ASIC3_GPIO_LSEL_D &= ~CF_DETECT;
	ASIC3_GPIO_INTYP_D |= CF_DETECT; // Edge	
	ASIC3_GPIO_ETSEL_D &= ~CF_DETECT;
	
	ASIC3_GPIO_MASK_D &= ~CF_STSCHG;
	ASIC3_GPIO_DIR_D &= ~CF_STSCHG;
	ASIC3_GPIO_INTYP_D |= CF_STSCHG; // Edge	
	ASIC3_GPIO_ETSEL_D &= ~CF_STSCHG;

  	set_GPIO_IRQ_edge(GPIO_DISCOVERY_CF_IRQ, GPIO_FALLING_EDGE);

  	// Register interrupts
  	irq = IRQ_ASIC3_CF_DETECT;
  	res = request_irq( irq, init->handler, SA_INTERRUPT, "PCMCIA_CD0", NULL );
  	if( res < 0 )
   		goto irq_err;
   		
 	res = request_irq( IRQ_ASIC3_CF_STSCHG, sharpsl_cf_status_change, SA_INTERRUPT, "PCMCIA_CD0", NULL );
  	if( res < 0 )
   		goto irq_err;

	backpackptr = ioremap(0x14000000, 4);    // Richard 0710  Output '0' to physical add. 0x14000000 to avoid .. 
	if (NULL != backpackptr) {
		*((u16*)backpackptr) = 0;		     // Richard 0710  
    		iounmap(backpackptr);
    	}

  	return 1;
#else
#if defined(CONFIG_ARCH_PXA_POODLE)
	// for SL-5600/B500 HW.
	if ( SCP_REG(SCP_CSR) & 0x0004 ) {
	  // cf not insert
	  SCP_REG(SCP_CPR) = 0x0081;
	  mdelay(100);
	  SCP_REG(SCP_CPR) = 0x0000;
	} else {
	  // cf insert
	}
#endif

	/* set GPIO_CF_CD & GPIO_CF_IRQ as inputs */
	GPDR(GPIO_CF_CD) &= ~GPIO_bit(GPIO_CF_CD);
	GPDR(GPIO_CF_IRQ) &= ~GPIO_bit(GPIO_CF_IRQ);
#if defined(CONFIG_PCMCIA_SHARP_SL_2SLOT)
	GPDR(GPIO_CF2_IRQ) &= ~GPIO_bit(GPIO_CF2_IRQ);
#endif

	/* Set transition detect */
	set_GPIO_IRQ_edge( GPIO_CF_CD, GPIO_FALLING_EDGE );
	set_GPIO_IRQ_edge( GPIO_CF_IRQ, GPIO_FALLING_EDGE );
#if defined(CONFIG_PCMCIA_SHARP_SL_2SLOT)
	set_GPIO_IRQ_edge( GPIO_CF2_IRQ, GPIO_FALLING_EDGE );
#endif

	/* Register interrupts */
	irq = IRQ_GPIO_CF_CD;
	res = request_irq(irq, init->handler, 
		SA_INTERRUPT, "CF_CD", NULL);
	if( res < 0 ) goto irq_err;

	/* enable interrupt */
	SCP_REG_IMR = 0x00C0;
	SCP_REG_MCR = 0x0101;
#if defined(CONFIG_PCMCIA_SHARP_SL_2SLOT)
	SCP2_REG_IMR = 0x00C0;
	SCP2_REG_MCR = 0x0101;
#endif
	keep_vs[0] = keep_vs[1] = NO_KEEP_VS;

	/* There's only one slot, but it's "Slot 0": */
	return 2;
#endif

irq_err:
	printk( KERN_ERR "%s: Request for IRQ %d failed\n",
		__FUNCTION__, irq );
	return -1;
}

static int sharpsl_pcmcia_shutdown(void)
{
	/* disable IRQs */
#ifdef CONFIG_SABINAL_DISCOVERY
  	free_irq( IRQ_ASIC3_CF_DETECT, NULL );
#else
  	free_irq( IRQ_GPIO_CF_CD, NULL );
#endif

	/* CF_BUS_OFF */
	sharpsl_pcmcia_init_reset();

	return 0;
}

static int sharpsl_pcmcia_socket_state(struct pcmcia_state_array* state_array)
{
#ifdef CONFIG_SABINAL_DISCOVERY
	unsigned long levels;
	unsigned short wASIC1Level;;
#else
	unsigned short cpr, csr;
//	static unsigned short precsr = 0;
#endif

	if(state_array->size<2) return -1;

	memset(state_array->state, 0, 
	       (state_array->size)*sizeof(struct pcmcia_state));

#ifdef CONFIG_SABINAL_DISCOVERY
  	levels=GPLR(0);
//  	wASIC1Level = DISCOVERY_ASIC1_GPIO_STATUS;
	wASIC1Level = ASIC3_GPIO_PSTS_D;
//	printk("GPLR0 %x, state %d\n", GPLR(0), levels & GPIO_DISCOVERY_CF_IRQ);
  	state_array->state[0].detect=((wASIC1Level & CF_DETECT)==0)?1:0;
  	state_array->state[0].ready=(levels & GPIO_DISCOVERY_CF_IRQ)?1:0;
  	state_array->state[0].bvd1= 0;
  	state_array->state[0].bvd2= 0;
  	state_array->state[0].wrprot=0; /* Not available */
  	state_array->state[0].vs_3v=1;
  	state_array->state[0].vs_Xv=0;

// Richard 0521
  is16 = (ASIC3_GPIO_PSTS_D & CF_IOIS) ? 0 : 1;
#else
	cpr = SCP_REG_CPR;
#if defined(CONFIG_ARCH_PXA_TOSA)
	if( (cpr & 0x0080) && ((cpr & 0x8040) != 0x8040) ){
	  extern int sharpsl_main_bk_flag;

	  printk(KERN_ERR "%s(): CPR=%04X, Low voltage!\n",
		 __FUNCTION__, cpr);
	  if ( sharpsl_main_bk_flag ) {
	    handle_scancode(SLKEY_OFF|KBDOWN , 1);
	    mdelay(30);
	    handle_scancode(SLKEY_OFF|KBUP   , 0);
	    mdelay(30);
	  }
	}
#endif
	//SCP_REG_CDR = 0x0002;
	SCP_REG_IRM = 0x00FF;
	SCP_REG_ISR = 0x0000;
	SCP_REG_IRM = 0x0000;
	csr = SCP_REG_CSR;
	if( csr & 0x0004 ){
		/* card eject */
		SCP_REG_CDR = 0x0000;
		keep_vs[0] = NO_KEEP_VS;
	}
	else if( !(keep_vs[0] & NO_KEEP_VS) ){
		/* keep vs1,vs2 */
		SCP_REG_CDR = 0x0000;
		csr |= keep_vs[0];
	}
	else if( cpr & 0x0003 ){
		/* power on */
		SCP_REG_CDR = 0x0000;
		keep_vs[0] = (csr & 0x00C0);
	}
	else{	/* card detect */
		SCP_REG_CDR = 0x0002;
	}

#ifdef SHARPSL_PCMCIA_DEBUG
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
#if defined(CONFIG_ARCH_PXA_TOSA)
#if 0
	  if (!current_cardslot_error(0)) {
	    force_cardslot_suspend(0);
	  }
#endif
#else
		printk(KERN_ERR "%s(): CPR=%04X, Low voltage!\n",
			__FUNCTION__, cpr);
#endif
	}
#if defined(CONFIG_PCMCIA_SHARP_SL_2SLOT)
	cpr = SCP2_REG_CPR;
	//SCP_REG_CDR = 0x0002;
	SCP2_REG_IRM = 0x00FF;
	SCP2_REG_ISR = 0x0000;
	SCP2_REG_IRM = 0x0000;
	csr = SCP2_REG_CSR;
	if( csr & 0x0004 ){
		/* card eject */
		SCP2_REG_CDR = 0x0000;
		keep_vs[1] = NO_KEEP_VS;
	}
	else if( !(keep_vs[1] & NO_KEEP_VS) ){
		/* keep vs1,vs2 */
		SCP2_REG_CDR = 0x0000;
		csr |= keep_vs[1];
	}
	else if( cpr & 0x0003 ){
		/* power on */
		SCP2_REG_CDR = 0x0000;
		keep_vs[1] = (csr & 0x00C0);
	}
	else{	/* card detect */
#if defined(CONFIG_ARCH_PXA_SPITZ)
		SCP2_REG_CDR = 0x0000;
#else
		SCP2_REG_CDR = 0x0002;
#endif
	}

#ifdef SHARPSL_PCMCIA_DEBUG
	printk("%s(): slot1 cpr=%04X, csr=%04X\n", __FUNCTION__, cpr, csr);
#endif

	state_array->state[1].detect = (csr & 0x0004)? 0:1;
	state_array->state[1].ready  = (csr & 0x0002)? 1:0;
	state_array->state[1].bvd1   = (csr & 0x0010)? 1:0;
	state_array->state[1].bvd2   = (csr & 0x0020)? 1:0;
	state_array->state[1].wrprot = (csr & 0x0008)? 1:0;
	state_array->state[1].vs_3v  = (csr & 0x0040)? 0:1;
	state_array->state[1].vs_Xv  = (csr & 0x0080)? 0:1;

#if !defined(CONFIG_ARCH_PXA_SPITZ)
	if( (cpr & 0x0080) && ((cpr & 0x8040) != 0x8040) ){
#if !defined(CONFIG_ARCH_PXA_TOSA)
		printk(KERN_ERR "%s(): slot1 CPR=%04X, Low voltage!\n",
			__FUNCTION__, cpr);
#else
		if (!current_cardslot_error(1)) {
		  force_cardslot_suspend(1);
		  set_jacketslot_error(1);
		}
#endif
	}
#endif
#endif
#endif

	return 1;
}

static int sharpsl_pcmcia_get_irq_info(struct pcmcia_irq_info* info)
{
	if (info->sock > 1)
		return -1;

	if (info->sock == 0) {
#ifdef CONFIG_SABINAL_DISCOVERY
		info->irq=IRQ_DISCOVERY_CF_IRQ;
#else
		info->irq=IRQ_GPIO_CF_IRQ;
#endif
#if defined(CONFIG_PCMCIA_SHARP_SL_2SLOT)
	} else if (info->sock == 1) {
		info->irq=IRQ_GPIO_CF2_IRQ;
#endif
	}

	return 0;
}

static int sharpsl_pcmcia_configure_socket(unsigned int sock,
					   socket_state_t *state)
{
	unsigned long flags;
#ifndef CONFIG_SABINAL_DISCOVERY
	unsigned short cpr, ncpr, ccr, nccr, mcr, nmcr, imr, nimr;
#endif
	int sharpsl_batt_err = 0;

	if (sock > 1)
		return -1;

#if !defined(CONFIG_PCMCIA_SHARP_SL_2SLOT)
	if (sock != 0)
	  	return 0;
#endif

#ifdef SHARPSL_PCMCIA_DEBUG
	printk("%s(): sk=%d, vc=%d, vp=%d, m=%04X, oe=%d, rs=%d, io=%d\n",
		__FUNCTION__, sock, 
		state->Vcc, state->Vpp, state->csc_mask,
		state->flags&SS_OUTPUT_ENA ? 1:0,
		state->flags&SS_RESET ? 1:0,
		state->flags&SS_IOCARD ? 1:0);
#endif

#ifdef CONFIG_SABINAL_DISCOVERY

	if(ASIC3_GPIO_PSTS_D & BACKPACK_DETECT)
		return 0;

	save_flags_cli(flags);
	
	switch (state->Vcc)
	{
		case 0:
			//printk(" %s() VCC off\n", __FUNCTION__);
			backpackptr = ioremap(0x14000000, 4);    // Richard 0710  Output '0' to physical add. 0x14000000 to avoid .. 
			if (NULL != backpackptr) {
				*((u16*)backpackptr) = 0;		     // Richard 0710  
    				iounmap(backpackptr);
    			}
    			
			ASIC3_GPIO_PIOD_B &= ~BUFFER_OE;
			ASIC3_GPIO_PIOD_C |=  BACK_I2C_ON;
			if (WRITE_I2C_BYTE(MICRON_DEVICE_ADDR, CMD_CF_3V_ON, CF_3V_CLOSE))
				printk("%s: I2C command failed. turn off\n", __FUNCTION__);
			ASIC3_GPIO_PIOD_C &=  ~BACK_I2C_ON;
			keep_reset_done = 0;
			vcc_on = 0;
			break;
		
		case 33:
			//printk(" %s() VCC 3V\n", __FUNCTION__);
			if (!vcc_on)  {	
				ASIC3_GPIO_PIOD_C |=  BACK_I2C_ON;
				if (WRITE_I2C_BYTE(MICRON_DEVICE_ADDR, CMD_CF_3V_ON, CF_3V_OPEN)) {
					printk("%s: I2C command failed. Write on3v\n", __FUNCTION__);
					ASIC3_GPIO_PIOD_C &=  ~BACK_I2C_ON;
					restore_flags(flags);
					return -1;		// Tune on 3v failed
				}
				if (!READ_I2C_BYTE(MICRON_DEVICE_ADDR))	{
					printk("%s: I2C command failed\n", __FUNCTION__);
					ASIC3_GPIO_PIOD_C &=  ~BACK_I2C_ON;
					restore_flags(flags);
					return -1;		// Tune on 3v failed
				}
				ASIC3_GPIO_PIOD_C &=  ~BACK_I2C_ON;
				ASIC3_GPIO_PIOD_B |= BUFFER_OE;
				vcc_on = 1;
			}
			if( !keep_reset_done )
				GPSR0 = GPIO_DISCOVERY_CF_RESET;
			break;
		case 50:
			//printk(" %s() VCC 5V, but we turn on 3V\n", __FUNCTION__);
			if (!vcc_on)  {
				ASIC3_GPIO_PIOD_C |=  BACK_I2C_ON ;
				if (WRITE_I2C_BYTE(MICRON_DEVICE_ADDR, CMD_CF_3V_ON, CF_3V_OPEN)) {
					printk("%s: I2C command failed. Write on5v\n", __FUNCTION__);
					ASIC3_GPIO_PIOD_C &=  ~BACK_I2C_ON;
					restore_flags(flags);
					return -1;		// Tune on 3v failed
				}
				if (!READ_I2C_BYTE(MICRON_DEVICE_ADDR)) {
					ASIC3_GPIO_PIOD_C &=  ~BACK_I2C_ON;
					restore_flags(flags);
					return -1;		// Tune on 3v failed
				}
				ASIC3_GPIO_PIOD_C &=  ~BACK_I2C_ON ;
				ASIC3_GPIO_PIOD_B |= BUFFER_OE;
				vcc_on = 1;
			}
			if( !keep_reset_done )
				GPSR0 = GPIO_DISCOVERY_CF_RESET;
			break;
		
		default:
			printk(" %s(): unrecognized Vcc %u\n", __FUNCTION__, state->Vcc);
			restore_flags(flags);
			return -1;
	}
	
	if (state->flags & SS_RESET) {	
//		printk(" %s() SS_RESET\n", __FUNCTION__);
		GPSR0 = GPIO_DISCOVERY_CF_RESET;
		keep_reset_done = 1;
//	} else {
	} else if( keep_reset_done ) {
//		printk(" %s() not SS_RESET\n", __FUNCTION__);
		GPCR0 = GPIO_DISCOVERY_CF_RESET;
	}
	
	/* Silently ignore Vpp, output enable, speaker enable. */
	
	restore_flags(flags);

#else

#if defined(CONFIG_PM)
#if defined(CONFIG_PCMCIA_SHARP_SL_2SLOT)
	if (sock == 1) { // for socket1
	  switch( state->Vcc ){
	  case	0:  sharpsl_batt_err = change_power_mode(LOCK_FCS_PCMCIA2, 0); break;
	  case 	33: sharpsl_batt_err = change_power_mode(LOCK_FCS_PCMCIA2, 1); break;
	  case	50: sharpsl_batt_err = change_power_mode(LOCK_FCS_PCMCIA2, 1); break;
	  default:
	    printk(KERN_ERR "%s(): unrecognized Vcc %u\n",
		   __FUNCTION__, state->Vcc);
	    return -1;
	  }
	} else {
#endif
	switch( state->Vcc ){
	case	0:  sharpsl_batt_err = change_power_mode(LOCK_FCS_PCMCIA, 0); break;
	case 	33: sharpsl_batt_err = change_power_mode(LOCK_FCS_PCMCIA, 1); break;
	case	50: 
#if defined(CONFIG_ARCH_PXA_TOSA)
		if (sock == 0) {
			printk(KERN_ERR "%s(): unrecognized Vcc %u\n",
				__FUNCTION__, state->Vcc);
			return -1;
		}
#endif
		sharpsl_batt_err = change_power_mode(LOCK_FCS_PCMCIA, 1); break;
	default:
		printk(KERN_ERR "%s(): unrecognized Vcc %u\n",
			__FUNCTION__, state->Vcc);
		return -1;
	}
#if defined(CONFIG_PCMCIA_SHARP_SL_2SLOT)
	}
#endif

	if ( !sharpsl_batt_err )
  		return -1;
#endif

	if (sock == 0) {
		if( (state->Vpp!=state->Vcc) && (state->Vpp!=0) ){
			printk(KERN_ERR "%s(): CF slot cannot support Vpp %u\n",
				__FUNCTION__, state->Vpp);
			return -1;
		}
		save_flags_cli(flags);

		nmcr = (mcr = SCP_REG_MCR) & ~0x0010;
		ncpr = (cpr = SCP_REG_CPR) & ~0x0083;
		nccr = (ccr = SCP_REG_CCR) & ~0x0080;
		nimr = (imr = SCP_REG_IMR) & ~0x003E;

#if defined(CONFIG_ARCH_PXA_SPITZ)
		ncpr |= (state->Vcc == 33) ? 0x0002: 
			(state->Vcc == 50) ? 0x0002:
			0;
#else
		ncpr |= (state->Vcc == 33) ? 0x0001: 
			(state->Vcc == 50) ? 0x0002:
			0;
#endif
		nmcr |= (state->flags&SS_IOCARD)? 0x0010: 0;
		ncpr |= (state->flags&SS_OUTPUT_ENA)? 0x0080: 0;
		nccr |= (state->flags&SS_RESET)? 0x0080: 0;
		nimr |=	((state->csc_mask&SS_DETECT) ? 0x0004: 0)|
			((state->csc_mask&SS_READY)  ? 0x0002: 0)|
			((state->csc_mask&SS_BATDEAD)? 0x0010: 0)|
			((state->csc_mask&SS_BATWARN)? 0x0020: 0)|
			((state->csc_mask&SS_STSCHG) ? 0x0010: 0)|
			((state->csc_mask&SS_WRPROT) ? 0x0008: 0);

		if( !(ncpr & 0x0003) )
			keep_rd[0] = 0;
		else if( !(keep_rd[0] & RESET_DONE) ){
			if( nccr & 0x0080 )
				keep_rd[0] |= RESET_DONE;
			else nccr |= 0x0080;
		}

		if( mcr != nmcr )
			SCP_REG_MCR = nmcr;
		if( cpr != ncpr ) {

#if defined(CONFIG_ARCH_PXA_SPITZ)
		  if(ncpr & 0x0002){
		    SCP_REG_GPWR |= SCP_CF_POWER;
		    if(!(cpr & 0x0002)){
		      mdelay(5);
		    }
		    if(cpr & 0x0004){
		      ncpr |= 0x0004;
		    }
 		  }else{
		    if(cpr & 0x0002){
		      SCP_REG_CPR &= ~0x0002;
		    }
		    if(!(cpr & 0x0004)){
		      mdelay(1);
		      SCP_REG_GPWR &= ~SCP_CF_POWER;
		    }
		  }
#endif
			SCP_REG_CPR = ncpr;
		}
		if( ccr != nccr )
			SCP_REG_CCR = nccr;
		if( imr != nimr )
			SCP_REG_IMR = nimr;

		restore_flags(flags);
#if defined(CONFIG_PCMCIA_SHARP_SL_2SLOT)
	} else if (sock == 1) {
		if( (state->Vpp!=state->Vcc) && (state->Vpp!=0) ){
			printk(KERN_ERR "%s(): CF slot1 cannot support Vpp %u\n",
				__FUNCTION__, state->Vpp);
			return -1;
		}
		save_flags_cli(flags);

		nmcr = (mcr = SCP2_REG_MCR) & ~0x0010;
		ncpr = (cpr = SCP2_REG_CPR) & ~0x0083;
		nccr = (ccr = SCP2_REG_CCR) & ~0x0080;
		nimr = (imr = SCP2_REG_IMR) & ~0x003E;

		ncpr |= (state->Vcc == 33) ? 0x0001: 
			(state->Vcc == 50) ? 0x0002:
			0;
		nmcr |= (state->flags&SS_IOCARD)? 0x0010: 0;
		ncpr |= (state->flags&SS_OUTPUT_ENA)? 0x0080: 0;
		nccr |= (state->flags&SS_RESET)? 0x0080: 0;
		nimr |=	((state->csc_mask&SS_DETECT) ? 0x0004: 0)|
			((state->csc_mask&SS_READY)  ? 0x0002: 0)|
			((state->csc_mask&SS_BATDEAD)? 0x0010: 0)|
			((state->csc_mask&SS_BATWARN)? 0x0020: 0)|
			((state->csc_mask&SS_STSCHG) ? 0x0010: 0)|
			((state->csc_mask&SS_WRPROT) ? 0x0008: 0);

		if( !(ncpr & 0x0003) )
			keep_rd[1] = 0;
		else if( !(keep_rd[1] & RESET_DONE) ){
			if( nccr & 0x0080 )
				keep_rd[1] |= RESET_DONE;
			else nccr |= 0x0080;
		}

		if( mcr != nmcr )
			SCP2_REG_MCR = nmcr;
		if( cpr != ncpr )
			SCP2_REG_CPR = ncpr;
		if( ccr != nccr )
			SCP2_REG_CCR = nccr;
		if( imr != nimr )
			SCP2_REG_IMR = nimr;

		restore_flags(flags);
#endif
	}
#endif

	return 0;
}

#if defined(CONFIG_ARCH_PXA_TOSA)
static int sharpsl_pcmcia_socket_init(int sock)
{
        /* enable interrupt */
	switch(sock) {
		case 0:
        		SCP_REG_IMR = 0x00C0;
        		SCP_REG_MCR = 0x0101;
			break;
		case 1:
        		SCP2_REG_IMR = 0x00C0;
        		SCP2_REG_MCR = 0x0101;
			break;
		default:
			return -1;
	}
	return 0;
}
#endif

struct pcmcia_low_level sharpsl_pcmcia_ops = { 
	sharpsl_pcmcia_init,
	sharpsl_pcmcia_shutdown,
	sharpsl_pcmcia_socket_state,
	sharpsl_pcmcia_get_irq_info,
	sharpsl_pcmcia_configure_socket,
#if defined(CONFIG_ARCH_PXA_TOSA)
	sharpsl_pcmcia_socket_init,
#endif
};

int is_pcmcia_card_present(int irq)
{
	int detect;
#ifdef CONFIG_SABINAL_DISCOVERY
  	detect = ((ASIC3_GPIO_PSTS_D & CF_DETECT) ? 0 : 1);  	
	//printk("is_card_present: irq=%d detect=%d\n", irq, detect);
	return detect;
#else
	int active = 0;
	if (irq == IRQ_GPIO_CF_IRQ) {
		active = (SCP_REG_CPR & 0x0003)? 1:0;
		detect = (SCP_REG_CSR & 0x0004)? 0:1;
#if defined(CONFIG_PCMCIA_SHARP_SL_2SLOT)
	} else if (irq == IRQ_GPIO_CF2_IRQ) {
		active = (SCP2_REG_CPR & 0x0003)? 1:0;
		detect = (SCP2_REG_CSR & 0x0004)? 0:1;
#endif
	} else {
		/* unknown IRQ : return TRUE */
		return 1;
	}
	//printk("is_card_present: irq=%d detect=%d active=%d\n", irq, detect, active);
	return detect & active;
#endif
}

int sharpsl_pcmcia_irq_to_sock(int irq)
{
#if defined(CONFIG_PCMCIA_SHARP_SL_2SLOT)
	if (irq == IRQ_GPIO_CF2_IRQ) {
		return 1;
	}
#endif
	return 0;
}
