/*
 * drivers/pcmcia/sa1100_collie.c
 *
 * PCMCIA implementation routines for Collie
 *
 * (C) Copyright 2001 Lineo Japan, Inc.
 *
 * May be copied or modified under the terms of the GNU General Public
 * License.  See linux/COPYING for more information.
 *
 * Based on
 * drivers/pcmcia/sa1100_assabet.c
 *
 * PCMCIA implementation routines for Assabet
 *
 */
#include <linux/config.h>
#include <linux/kernel.h>
#include <linux/sched.h>
#include <pcmcia/cs_types.h>
#include <pcmcia/ss.h>

#include <asm/hardware.h>
#include <asm/irq.h>
#include <asm/delay.h>
#include <asm/arch/pcmcia.h>

//#define COLLIE_PCMCIA_DEBUG

#ifdef CONFIG_COLLIE_TS
#else
static unsigned short keep_vs[2];
#define	NO_KEEP_VS 0x0001
#endif


void collie_pcmcia_init_reset(void)
{
#ifdef CONFIG_COLLIE_TS
	/* CF_BUS_OFF */
	cf_buf_ctrl = CF_BUS_POWER_OFF;
	CF_BUF_CTRL = cf_buf_ctrl;

#else	/* ! CONFIG_COLLIE_TS */
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
#endif
}

static int collie_pcmcia_init(struct pcmcia_init *init)
{
	int irq, res;


	/* set GPIO_CF_CD & GPIO_CF_IRQ as inputs */
	GPDR &= ~(GPIO_CF_CD|GPIO_CF_IRQ);

	/* Set transition detect */
#ifdef CONFIG_COLLIE_TS
	set_GPIO_IRQ_edge( GPIO_CF_CD, GPIO_BOTH_EDGES );
#else
	set_GPIO_IRQ_edge( GPIO_CF_CD, GPIO_FALLING_EDGE );
#endif
	set_GPIO_IRQ_edge( GPIO_CF_IRQ, GPIO_FALLING_EDGE );

	/* Register interrupts */
	irq = IRQ_GPIO_CF_CD;
	res = request_irq(irq, init->handler, 
		SA_INTERRUPT, "CF_CD", NULL);
	if( res < 0 ) goto irq_err;

	/* enable interrupt */
#ifdef CONFIG_COLLIE_TS
#else
	SCP_REG_IMR = 0x00C0;
	SCP_REG_MCR = 0x0101;
	keep_vs[0] = keep_vs[1] = NO_KEEP_VS;
#endif

	/* There's only one slot, but it's "Slot 0": */
	return 2;

irq_err:
	printk( KERN_ERR "%s: Request for IRQ %d failed\n",
		__FUNCTION__, irq );
	return -1;
}

static int collie_pcmcia_shutdown(void)
{
	/* disable IRQs */
  	free_irq( IRQ_GPIO_CF_CD, NULL );

	/* CF_BUS_OFF */
	collie_pcmcia_init_reset();

	return 0;
}

static int collie_pcmcia_socket_state(struct pcmcia_state_array* state_array)
{
#ifdef CONFIG_COLLIE_TS
	unsigned long levels;
#else
	unsigned short cpr, csr;
	static unsigned short precsr = 0;
#endif

	if(state_array->size<2) return -1;

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
	cpr = SCP_REG_CPR;
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
#endif

	return 1;
}

static int collie_pcmcia_get_irq_info(struct pcmcia_irq_info* info)
{
	if (info->sock > 1)
		return -1;

	if (info->sock == 0) {
		info->irq=IRQ_GPIO_CF_IRQ;
	}

	return 0;
}

static int collie_pcmcia_configure_socket(const struct pcmcia_configure* 
					  configure)
{
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

	case 33:  /* Can only apply 3.3V to the CF slot. */
		cf_buf_ctrl &= ~CF_BUS_POWER_OFF;
		cf_buf_ctrl |=  CF_BUS_POWER_33V;
		break;
	}

	if( configure->flags&SS_RESET )
		cf_buf_ctrl |=  CF_BUS_RESET;
	else	cf_buf_ctrl &= ~CF_BUS_RESET;

	CF_BUF_CTRL = cf_buf_ctrl;

#else	/* ! CONFIG_COLLIE_TS */
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
	int detect;
#ifdef CONFIG_COLLIE_TS
	detect = ((GPLR & GPIO_CF_CD)==0)?1:0;
#else
	detect = (SCP_REG_CSR & 0x0004)? 0:1;
#endif
	//printk("is_card_present: detect=%d\n", detect);
	return detect;
}
