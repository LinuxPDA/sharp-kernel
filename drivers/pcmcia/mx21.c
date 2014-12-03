#include <linux/kernel.h>
#include <linux/sched.h>
#include <linux/ioport.h>

#include <asm/hardware.h>
#include <asm/irq.h>

#include "mx21.h"
#include "mx_generic.h"

#ifdef CONFIG_MTD_NAND_MX2ADS
extern int mx2_nand_probed;
#endif

void * dev_id;
static void mx21_pcmcia_gpio_config()
{
	#define GPIOF_GIUS_V	IO_ADDRESS(0x10015520)
	#define GPIOF_DDIR_V	IO_ADDRESS(0x10015500)
	#define GPIOF_OCR1_V	IO_ADDRESS(0x10015504)
	#define GPIOF_OCR2_V	IO_ADDRESS(0x10015508)
	#define GPIOF_ICONFA1_V	IO_ADDRESS(0x1001550c)
	#define GPIOF_ICONFA2_V	IO_ADDRESS(0x10015510)
	#define GPIOF_PUEN_V	IO_ADDRESS(0x10015540)
	#define GPIOF_DR_V	IO_ADDRESS(0x1001551c)

	#define GPIOE_GIUS_V	IO_ADDRESS(0x10015420)
	#define GPIOE_DDIR_V	IO_ADDRESS(0x10015400)
	#define GPIOE_OCR1_V	IO_ADDRESS(0x10015404)
	#define GPIOE_OCR2_V	IO_ADDRESS(0x10015408)
	#define GPIOE_ICONFA1_V	IO_ADDRESS(0x1001540c)
	#define GPIOE_ICONFA2_V	IO_ADDRESS(0x10015410)
	#define GPIOE_PUEN_V	IO_ADDRESS(0x10015440)
	#define GPIOE_DR_V	IO_ADDRESS(0x1001541c)

typedef unsigned long U32;
#if 0
	printk("use Irwen's setting.\n");
	*(U32 *)GPIOF_GIUS_V=0x00607FFF; //set pcmcia used pins
	*(U32 *)GPIOF_DDIR_V=0x0060003f;
	*(U32 *)GPIOF_OCR1_V=0x00000000 ;
	*(U32 *)GPIOF_OCR2_V=0x00003c00 ;	//for cs4, cs5 which control output voltage
	*(U32 *)GPIOF_ICONFA1_V=0x00000000 ;
	*(U32 *)GPIOF_ICONFA2_V=0x00000000 ;
	*(U32 *)GPIOF_PUEN_V=0xFFFFFFFF;
	*(U32 *)GPIOF_DR_V=0xFFFFFFFF;
	*(U32 *)GPIOF_DR_V=0x00200000;//0x00200000;
#else
 	printk("use my setting.\n");
 	*(U32*)GPIOF_GIUS_V|=0x00607FFF; //set pcmcia used pins
#if 0
 	*(U32*)GPIOF_DDIR_V|=0x0060043f; //configure pins to output
 	*(U32*)GPIOF_DDIR_V&=0xffff843f; //configure pin to input
	*(U32*)GPIOF_OCR1_V&=0x00cff000 ;
#else
 	*(U32*)GPIOF_DDIR_V|=0x0060003f; //configure pins to output
 	*(U32*)GPIOF_DDIR_V&=0xffff803f; //configure pin to input
 	*(U32*)GPIOF_OCR1_V&=0x00cff000 ;
#endif

 	*(U32*)GPIOF_OCR2_V|=0x00003c00 ; //for cs4, cs5 which control output voltage
 	*(U32*)GPIOF_ICONFA1_V=0x00000000 ;
 	*(U32*)GPIOF_ICONFA2_V=0x00000000 ;
 	*(U32*)GPIOF_PUEN_V=0xFFFFFFFF;
#endif

 	*(U32*)GPIOE_GIUS_V|=(0x1<<5); //SPKOUT
 	*(U32*)GPIOE_DDIR_V|=(0x1<<5); //output
 	*(U32*)GPIOE_OCR1_V&=~(0x3<<10); //A_IN
 	*(U32*)GPIOE_PUEN_V|=(0x1<<5);
 
	//set cs5,cs4 to B'' //0x400000 = 3.3v 0x200000 = 5v
 	*(U32*)GPIOF_DR_V&=0xFF9FFFFF;
 	*(U32*)GPIOF_DR_V|=0x00400000;
 	//delay 1s for charging external card. 
 	{
		unsigned long time;
		time=jiffies+HZ;
		while(time>jiffies){};
	}

	pgcr_write(PGCR_ATASEL,0);
	pgcr_write(PGCR_ATAMODE,0);
	pgcr_write(PGCR_LPMEN,0);
	pgcr_write(PGCR_SPKREN,0);
	pgcr_write(PGCR_POE,1);
	pgcr_write(PGCR_RESET,1);  //soft reset card
	{
		unsigned long time;
		time=jiffies+HZ;
		while(time>jiffies){};
	}
	pgcr_write(PGCR_RESET,0);
	{
		unsigned long time;
		time=jiffies+HZ;
		while(time>jiffies){};
	}
	//udelay(100*1000); //100ms  
}

void mx21_pcmcia_reset(void)
{
	pgcr_write(PGCR_ATASEL,0);
	pgcr_write(PGCR_ATAMODE,0);
	pgcr_write(PGCR_LPMEN,0);
	pgcr_write(PGCR_SPKREN,0);
	pgcr_write(PGCR_POE,1);
	pgcr_write(PGCR_RESET,1);  //soft reset card
	{
		unsigned long time;
		time=jiffies+HZ;
		while(time>jiffies){};
	}
	pgcr_write(PGCR_RESET,0);
	{
		unsigned long time;
		time=jiffies+HZ;
		while(time>jiffies){};
	}
	//udelay(100*1000); //100ms  
}

static void mx21_pcmcia_irpt_config()
{
	//Enable irq
	per_write(PER_ERRINTEN,0);
	per_write(PER_PWRONEN,0);
	per_write(PER_RDYRE,0);
	per_write(PER_RDYFE,0);
	per_write(PER_RDYHE,0);
	per_write(PER_RDYLE,1);
	per_write(PER_BVDE2,0);
	per_write(PER_BVDE1,0);
	per_write(PER_CDE2,1);
	per_write(PER_CDE1,1);
	per_write(PER_WPE,0);
	per_write(PER_VSE2,0);
	per_write(PER_VSE1,0);
}

static void mx21_pcmcia_controller_init()
{
	int i;
	for(i=0;i<7;i++)
	{
		pbr_write(i,PBR_PBA,0);
		por_write(i,POR_PV,0);
		pofr_write(i,POFR_POFA,0);
	}
}

static int mx21_pcmcia_init(struct pcmcia_init *init)
{
	int res;

#ifdef CONFIG_MTD_NAND_MX2ADS
	if (mx2_nand_probed)
		return -1;
#endif

 	mx21_pcmcia_gpio_config();
 	mx21_pcmcia_irpt_config();
 	mx21_pcmcia_controller_init();
 /*
  * Register interrupt.
  */
	res = request_irq(INT_PCMCIA, init->handler, SA_SHIRQ|SA_INTERRUPT,
				  "PCMCIA", init->handler);
	if (res)
	{
		printk(KERN_ERR "%s: request for PCMCIA failed\n",
			__FUNCTION__);
		return -1;
	}
	dev_id=init->handler;

    printk("pcmcia module has been initialised\n");
	return 1;
}

/*
 * Release all resources.
 */
static int mx21_pcmcia_shutdown(void)
{
	free_irq(INT_PCMCIA, dev_id);
	return 0;
}

static int mx21_pcmcia_socket_state(struct pcmcia_state_array *state)
{
	unsigned long status;

	if (state->size > 1 )
		return -1;

	state->state[0].poweron = pipr_read(PIPR_POWERON)? 1 : 0;
	state->state[0].ready  = pipr_read(PIPR_RDY)? 1 : 0;
	state->state[0].bvd2   = pipr_read(PIPR_BVD2)? 1 : 0;
	state->state[0].bvd1   = pipr_read(PIPR_BVD1)? 1 : 0;
	state->state[0].detect = pipr_read(PIPR_CD)? 0 : 1;
	//printk("\npipr_cd is %d, detect is %d.\n",pipr_read(PIPR_CD),state->state[0].detect);
	state->state[0].wrprot = pipr_read(PIPR_WP)? 1 : 0;

	state->state[0].vs_3v  =  1;
	state->state[0].vs_Xv  =  0;

	return 1;
}

static int mx21_pcmcia_get_irq_info(struct pcmcia_irq_info *info)
{
	if (info->sock > 1)
		return -1;

	info->irq = INT_PCMCIA;
	return 0;
}

static int mx21_pcmcia_configure_socket(const struct pcmcia_configure *configure)
{
	static int enable_flag = -1;

	if (configure->irq) {
		if (enable_flag != 1) {
			enable_irq(INT_PCMCIA);
			enable_flag = 1;
		}
	} else {
		if (enable_flag != 0) {
			disable_irq(INT_PCMCIA);
			enable_flag = 0;
		}
	}
	return 0;
}

/*
 * Enable card status IRQs on (re-)initialisation.  This can
 * be called at initialisation, power management event, or
 * pcmcia event.
 */
static int mx21_pcmcia_socket_init(int sock)
{
	return 0;
}

/*
 * Disable card status IRQs on suspend.
 */
static int mx21_pcmcia_socket_suspend(int sock)
{
	return 0;
}

struct pcmcia_low_level mx21_pcmcia_ops = { 
	init:			mx21_pcmcia_init,
	shutdown:		mx21_pcmcia_shutdown,
	socket_state:		mx21_pcmcia_socket_state,
	get_irq_info:		mx21_pcmcia_get_irq_info,
	configure_socket:	mx21_pcmcia_configure_socket,

	socket_init:		mx21_pcmcia_socket_init,
	socket_suspend:		mx21_pcmcia_socket_suspend,
};
