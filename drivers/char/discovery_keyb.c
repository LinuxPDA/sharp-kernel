/*
 * linux/drivers/char/discovery_keyb.c : Discovery keyborad driver
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
 * May be copied or modified under the terms of the GNU General Public
 * License.  See linux/COPYING for more information.
 *
 * Based on:
 *   drivers/char/iris_keyb.c
 *   drivers/char/kbd_linkup.c - Keyboard Driver for Linkup-7200
 *   Copyright (C) Roger Gammans  1998
 *   Written for Linkup 7200 by Xuejun Tao, ISDcorp
 *   Refer to kbd_7110.c (Cirrus Logic 7110 Keyboard driver)
 *   drivers/char/collie_keyb.c 
 *
 */
#include <linux/config.h>
#include <linux/init.h>
#include <linux/poll.h>
#include <linux/wait.h>
#include <asm/arch/keyboard.h>
#include <asm/uaccess.h>
#include <linux/tqueue.h>
#include <linux/kbd_ll.h>
#include <linux/delay.h>

#ifdef USE_KBD_IRQ		// could be defined in asm/arch/keyboard.h
#include <asm/irq.h>	// for linkup keyboard interrupt
#endif

#include <asm/arch/irqs.h>
#include <asm/arch/discovery_asic3.h>

#ifndef COLLIE_KBD_MAJOR
# define COLLIE_KBD_MAJOR 61
#endif

#ifdef CONFIG_PM
#include <linux/pm.h>
static struct pm_dev *collie_kbd_pm_dev;
#endif

/*
 * common logical driver definition
 */

extern void sharppda_kbd_press(int keycode);
extern void sharppda_kbd_release(int keycode);
extern void sharppda_scan_logdriver_init(void);
//extern void sharppda_scan_key_gpio(void* dummy);
extern void sharppda_scan_key_gpio(int irq);
extern void sharppda_get_actkey_gpio(void*);
#define collie_kbd_tick  sharppda_scan_key_gpio
#define collie_actkey_tick sharppda_get_actkey_gpio

#if 0 //ned
#define	DIRKEY_TICKTIME_TICKTIME	( 500 )
static DECLARE_WAIT_QUEUE_HEAD(dirkey_count);
#endif


/*
 * physical driver depending on COLLIE target.
 */
void collie_kbd_discharge_all(void)
{
}

void collie_kbd_charge_all(void)
{
#if 0 //kemble
					// kstrb_b -> High
//k	LCM_KSC = CHARGE_VALUE;
#endif
}

void collie_kbd_activate_all(void)
{
#if 0 //kemble
					// kstrb_b -> Low
//k	LCM_KSC = DISCHARGE_VALUE;
					// STATE CLEAR
	LCM_KIC &= IRQ_STATE_CLEAR;
#endif
}

void collie_kbd_activate_col(int col)
{
	unsigned short nset;
	unsigned short nbset;

	nset = 0xFF & ~(1 << col);
	nbset = (nset << 8) + nset;
//k	LCM_KSC = nbset;
}

void collie_kbd_reset_col(int col)
{
	unsigned short nbset;

	nbset = ((0xFF & ~(1 << col)) << 8) + 0xFF;
//k	LCM_KSC = nbset;
}







/**********************************************/
static void discovery_syncbutton_irq(int irq,void *dev_id,struct pt_regs *regs)
{
	if( (ASIC3_GPIO_ETSEL_D & WAKE_UP ) == 0 ) /* Is it falling edge ? */
	{
		ASIC3_GPIO_ETSEL_D |= WAKE_UP; /* falling -> rising */
		handle_scancode(KBSCANCDE(Sync_key,KBDOWN),1);
		mdelay(10);
	}
	else
	{
		ASIC3_GPIO_ETSEL_D &= ~WAKE_UP; /* rising -> falling */
		handle_scancode(KBSCANCDE(Sync_key,KBUP),0);
		mdelay(10);
	}
}



/**********************************************/
static void collie_kbd_irq(int irq,void *dev_id,struct pt_regs *regs)
{
#ifdef CONFIG_PM
  extern int autoPowerCancel;
  extern int autoLightCancel;
#endif
//	printk(KERN_EMERG " Into the discovery keyboard handler now ! \n");
//	printk(" irq no. = %d , IRQ_ASIC3_A0 = %d \n",irq, IRQ_ASIC3_A0);
//	collie_kbd_tick(NULL);
	irq -= IRQ_ASIC3_A0; /* offset of irq */
	collie_kbd_tick(irq);

	// auto power/light cancel
#ifdef CONFIG_PM
	autoPowerCancel = 0;
	autoLightCancel = 0;
#endif

//	ASIC3_GPIO_INTSTAT_A &= ~(KEY0+KEY1+KEY2+KEY3+UP_KEY+DOWN_KEY+LEFT_KEY+RIGHT_KEY+ACTION_KEY);
	ASIC3_GPIO_INTSTAT_A &= 0;

#if 0 //ned 5/3/1603
		/* enable interrupt */
	ASIC3_GPIO_MASK_A &= ~(UP_KEY+DOWN_KEY);
		/* level trigger */
	ASIC3_GPIO_INTYP_A &= ~(DIR0_KEY+DIR1_KEY);
		/* low level trigger */
	ASIC3_GPIO_LSEL_A &= ~(DIR0_KEY+DIR1_KEY);
#endif //ned 5/3/1603
//	printk(" GGGGGGGGGGGGGGGGGGGGG \n");
}

static void collie_actkey_irq(int irq,void *dev_id,struct pt_regs *regs)
{
#ifdef CONFIG_PM
  extern int autoPowerCancel;
  extern int autoLightCancel;
#endif

	printk(KERN_EMERG " Into the discovery action key handler now ! \n");
	collie_actkey_tick(NULL);

#ifdef CONFIG_PM
	autoPowerCancel = 0;
	autoLightCancel = 0;
#endif
	/* clear interrupt pending bit */
	ASIC3_GPIO_INTSTAT_A &= ~(KEY0+KEY1+KEY2+KEY3+UP_KEY+DOWN_KEY+LEFT_KEY+RIGHT_KEY+ACTION_KEY);
	printk(" GGGGGGGGGGGGGGGGGGGGG \n");
}



#define	USE_WAKEUP_BUTTON
#ifdef USE_WAKEUP_BUTTON
#include <asm/sharp_keycode.h>
#define	WAKEUP_BUTTON_SCANCODE	SLKEY_SYNCSTART	/* No.117 */
#define	WAKEUP_BUTTON_DELAY	(HZ*100/1000)
extern u32 apm_wakeup_src_mask;
extern u32 apm_wakeup_factor;

static void inline collie_wakeup_button_send(void)
{
#if 0	// kemble
	handle_scancode(WAKEUP_BUTTON_SCANCODE|KBDOWN,1);
	handle_scancode(WAKEUP_BUTTON_SCANCODE|KBUP,0);
#endif
}

static void collie_wakeup_button_timer_proc(unsigned long param)
{
//k	if( GPLR & GPIO_WAKEUP )
		return;
	collie_wakeup_button_send();
}

static struct timer_list  collie_wakeup_button_timer =
	{function:collie_wakeup_button_timer_proc};

static void collie_wakeup_button_irq(int irq,void *dev_id,struct pt_regs *regs)
{
//k if( GPLR & GPIO_WAKEUP )
		return;
	del_timer(&collie_wakeup_button_timer);
	collie_wakeup_button_timer.expires = jiffies + WAKEUP_BUTTON_DELAY;
	add_timer(&collie_wakeup_button_timer);
}

static void collie_wakeup_button_init(void)
{
	init_timer(&collie_wakeup_button_timer);
#if 0	//kemble
	/* GPOI24(int):IN */
	GPDR &= ~GPIO_WAKEUP;

	/* GPIO24:not Alternate */
	GAFR &= ~GPIO_WAKEUP;

	/* GPIO24:FallingEdge */
	set_GPIO_IRQ_edge(GPIO_WAKEUP, GPIO_FALLING_EDGE );

	if( request_irq(IRQ_GPIO_WAKEUP, collie_wakeup_button_irq,
		SA_INTERRUPT, "WakeupButton", NULL)) {
		printk("Could not allocate Wakeup-Button IRQ!\n");
	}
#endif
}

static void collie_wakeup_button_exit()
{
//k	free_irq(IRQ_GPIO_WAKEUP, NULL);
	del_timer(&collie_wakeup_button_timer);
}
#endif // USE_WAKEUP_BUTTON

#ifdef CONFIG_PM
extern void sharppda_scan_logdriver_suspend(void);
extern void sharppda_scan_logdriver_resume(void);

static int collie_kbd_pm_callback(struct pm_dev *pm_dev,
					pm_request_t req, void *data)
{
        switch (req) {
        case PM_SUSPEND:
//ned		disable_irq(LCM_IRQ_KEY);
//		disable_irq(IRQ_ASIC1_KEY_INTERRUPT);
		disable_irq(IRQ_ASIC3_A0);
		disable_irq(IRQ_ASIC3_A1);
		disable_irq(IRQ_ASIC3_A2);
		disable_irq(IRQ_ASIC3_A3);
		disable_irq(IRQ_ASIC3_A4);
		disable_irq(IRQ_ASIC3_A5);
		disable_irq(IRQ_ASIC3_A6);
		disable_irq(IRQ_ASIC3_A7);
		disable_irq(IRQ_ASIC3_A8);
		/* disable KEY_POWER_ON */
		ASIC3_GPIO_PIOD_A &= ~GPIO_P10; /* ouput:0 */
		sharppda_scan_logdriver_suspend();
#ifdef USE_WAKEUP_BUTTON
//k		apm_wakeup_src_mask |= GPIO_WAKEUP;
//k		disable_irq(IRQ_GPIO_WAKEUP);
//k		del_timer(&collie_wakeup_button_timer);
#endif // USE_WAKEUP_BUTTON
       		break;
        case PM_RESUME:
//		enable_irq(IRQ_ASIC1_KEY_INTERRUPT);
		enable_irq(IRQ_ASIC3_A0);
		enable_irq(IRQ_ASIC3_A1);
		enable_irq(IRQ_ASIC3_A2);
		enable_irq(IRQ_ASIC3_A3);
		enable_irq(IRQ_ASIC3_A4);
		enable_irq(IRQ_ASIC3_A5);
		enable_irq(IRQ_ASIC3_A6);
		enable_irq(IRQ_ASIC3_A7);
		enable_irq(IRQ_ASIC3_A8);
		/* enable KEY_POWER_ON */
		ASIC3_GPIO_PIOD_A |= GPIO_P10; /* ouput:1 */
		sharppda_scan_logdriver_resume();
#ifdef USE_WAKEUP_BUTTON
//k		if( apm_wakeup_factor & GPIO_WAKEUP )
//k			collie_wakeup_button_send();
//k		enable_irq(IRQ_GPIO_WAKEUP);
#endif // USE_WAKEUP_BUTTON
		break;
	}
	return 0;
}
#endif

void collie_kbd_hw_init(void)
{ 
//	sharppda_scan_logdriver_init();
//	printk("$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$\n");
//	printk("$$$$$$$$$ $$$ $$$$$$$$$      $$$$$$$$$$$$$ $$$$$ $$$$$$$$$$$$$$$$$$$$$$$$$$$$\n");
//	printk("$$$$$$$$$ $$ $$$$$$$$$$ $$$$$$$$$$$$$$$$$$$ $$$ $$$$$$$$$$$$$$$$$$$$$$$$$$$$$\n");
//	printk("$$$$$$$$$ $ $$$$$$$$$$$      $$$$$$$$$$$$$$$$  $$$$$$$$$$$$$$$$$$$$$$$$$$$$$$\n");
//	printk("$$$$$$$$$ $$ $$$$$$$$$$ $$$$$$$$$$$$$$$$$$$$$  $$$$$$$$$$$$$$$$$$$$$$$$$$$$$$\n");
//	printk("$$$$$$$$$ $$$ $$$$$$$$$     $$$$$$$$$$$$$$$$$  $$$$$$$$$$$$$$$$$$$$$$$$$$$$$$\n");
//	printk("$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$\n");

	/* for DVT-ASIC3 Open KEY_POWER_ON */
	ASIC3_GPIO_DIR_A |= GPIO_P10; /* Dir:output */
	ASIC3_GPIO_PIOD_A |= GPIO_P10; /* ouput:1 */

#if	All_EDGE //ned 5/3/1359
	/* Hardware initial */
		/* enable interrupt */
		printk(" in collie_kbd_hw_init All_EDGE = 1 \n");
	ASIC3_GPIO_MASK_A &= ~(KEY0+KEY1+KEY2+KEY3+UP_KEY+DOWN_KEY+LEFT_KEY+RIGHT_KEY+ACTION_KEY);
		/* input port */
	ASIC3_GPIO_DIR_A  &= ~(KEY0+KEY1+KEY2+KEY3+UP_KEY+DOWN_KEY+LEFT_KEY+RIGHT_KEY+ACTION_KEY);
		/* edge trigger */
	ASIC3_GPIO_INTYP_A |= (KEY0+KEY1+KEY2+KEY3+UP_KEY+DOWN_KEY+LEFT_KEY+RIGHT_KEY+ACTION_KEY);
		/* falling edge trigger */
	ASIC3_GPIO_ETSEL_A &= ~(KEY0+KEY1+KEY2+KEY3+UP_KEY+DOWN_KEY+LEFT_KEY+RIGHT_KEY+ACTION_KEY);
		/* sleep trigger */
	ASIC3_GPIO_INTSLP_A &= ~(KEY0+KEY1+KEY2+KEY3);





	/*** SYNC BUTTON Detection ***/
		/* enable interrupt */
	ASIC3_GPIO_MASK_D &= ~WAKE_UP;
		/* input port */
	ASIC3_GPIO_DIR_D &= ~WAKE_UP;
		/* edge trigger */
	ASIC3_GPIO_INTYP_D |= WAKE_UP;
		/* falling edge trigger */
	ASIC3_GPIO_ETSEL_D &= ~WAKE_UP;

	/* for SYNC BUTTON DTETCT */
	if (request_irq(IRQ_ASIC3_D3, discovery_syncbutton_irq,
		  SA_INTERRUPT, "syncbutton", NULL)) 
	{
		printk("Could not allocate SYNC BUTTON IRQ!\n");
	}




	if (request_irq(IRQ_ASIC3_A0, collie_kbd_irq,
		  SA_INTERRUPT, "keyboard", NULL)) 
	{
		printk("Could not allocate KEYBD IRQ!\n");
	}
	if (request_irq(IRQ_ASIC3_A1, collie_kbd_irq,
		  SA_INTERRUPT, "keyboard", NULL)) 
	{
		printk("Could not allocate KEYBD IRQ!\n");
	}
	if (request_irq(IRQ_ASIC3_A2, collie_kbd_irq,
		  SA_INTERRUPT, "keyboard", NULL)) 
	{
		printk("Could not allocate KEYBD IRQ!\n");
	}
	if (request_irq(IRQ_ASIC3_A3, collie_kbd_irq,
		  SA_INTERRUPT, "keyboard", NULL)) 
	{
		printk("Could not allocate KEYBD IRQ!\n");
	}
	if (request_irq(IRQ_ASIC3_A4, collie_kbd_irq,
		  SA_INTERRUPT, "keyboard", NULL)) 
	{
		printk("Could not allocate KEYBD IRQ!\n");
	}
	if (request_irq(IRQ_ASIC3_A5, collie_kbd_irq,
		  SA_INTERRUPT, "keyboard", NULL)) 
	{
		printk("Could not allocate KEYBD IRQ!\n");
	}
	if (request_irq(IRQ_ASIC3_A6, collie_kbd_irq,
		  SA_INTERRUPT, "keyboard", NULL)) 
	{
		printk("Could not allocate KEYBD IRQ!\n");
	}
	if (request_irq(IRQ_ASIC3_A7, collie_kbd_irq,
		  SA_INTERRUPT, "keyboard", NULL)) 
	{
		printk("Could not allocate KEYBD IRQ!\n");
	}
	if (request_irq(IRQ_ASIC3_A8, collie_kbd_irq,
		  SA_INTERRUPT, "keyboard", NULL)) 
	{
		printk("Could not allocate KEYBD IRQ!\n");
	}
#else  //ned 5/3/1359

/***************************************************************************/


//#if 1 //ned 5/3/1510
	/* Hardware initial */
		/* enable interrupt */
		printk(" in collie_kbd_hw_init All_EDGE = 0 \n");
	ASIC3_GPIO_MASK_A &= ~(KEY0+KEY1+KEY2+KEY3+UP_KEY+DOWN_KEY+LEFT_KEY+RIGHT_KEY+ACTION_KEY);
		/* input port */
	ASIC3_GPIO_DIR_A  &= ~(KEY0+KEY1+KEY2+KEY3+UP_KEY+DOWN_KEY+LEFT_KEY+RIGHT_KEY+ACTION_KEY);
		/* edge trigger */
	ASIC3_GPIO_INTYP_A |= (KEY0+KEY1+KEY2+KEY3+DIR2_KEY+DIR3_KEY+ACTION_KEY);
		/* level trigger */
	ASIC3_GPIO_INTYP_A &= ~(DIR0_KEY+DIR1_KEY);
		/* falling edge trigger */
	ASIC3_GPIO_ETSEL_A &= ~(KEY0+KEY1+KEY2+KEY3+ACTION_KEY);
		/* rising edge trigger */
	ASIC3_GPIO_ETSEL_A |= (DIR2_KEY+DIR3_KEY);
		/* low level trigger */
	ASIC3_GPIO_LSEL_A &= ~(DIR0_KEY+DIR1_KEY);
		/* sleep trigger */
	ASIC3_GPIO_INTSLP_A &= ~(KEY0+KEY1+KEY2+KEY3);


//ned	if (request_irq(IRQ_ASIC1_KEY_INTERRUPT, collie_kbd_irq,
	if (request_irq(IRQ_ASIC3_A0, collie_kbd_irq,
		  SA_INTERRUPT, "keyboard", NULL)) 
	{
		printk("Could not allocate KEYBD IRQ!\n");
	}
	if (request_irq(IRQ_ASIC3_A1, collie_kbd_irq,
		  SA_INTERRUPT, "keyboard", NULL)) 
	{
		printk("Could not allocate KEYBD IRQ!\n");
	}
	if (request_irq(IRQ_ASIC3_A2, collie_kbd_irq,
		  SA_INTERRUPT, "keyboard", NULL)) 
	{
		printk("Could not allocate KEYBD IRQ!\n");
	}
	if (request_irq(IRQ_ASIC3_A3, collie_kbd_irq,
		  SA_INTERRUPT, "keyboard", NULL)) 
	{
		printk("Could not allocate KEYBD IRQ!\n");
	}
	if (request_irq(IRQ_ASIC3_A4, collie_kbd_irq,
		  SA_INTERRUPT, "keyboard", NULL)) 
	{
		printk("Could not allocate KEYBD IRQ!\n");
	}
	if (request_irq(IRQ_ASIC3_A5, collie_kbd_irq,
		  SA_INTERRUPT, "keyboard", NULL)) 
	{
		printk("Could not allocate KEYBD IRQ!\n");
	}
	if (request_irq(IRQ_ASIC3_A6, collie_kbd_irq,
		  SA_INTERRUPT, "keyboard", NULL)) 
	{
		printk("Could not allocate KEYBD IRQ!\n");
	}
	if (request_irq(IRQ_ASIC3_A7, collie_kbd_irq,
		  SA_INTERRUPT, "keyboard", NULL)) 
	{
		printk("Could not allocate KEYBD IRQ!\n");
	}
	if (request_irq(IRQ_ASIC3_A8, collie_kbd_irq,
		  SA_INTERRUPT, "keyboard", NULL)) 
	{
		printk("Could not allocate KEYBD IRQ!\n");
	}
#endif  //ned 5/3/1510


//	kernel_thread(direct_key_poll,  NULL, CLONE_FS | CLONE_FILES | CLONE_SIGHAND | SIGCHLD);


/*	ned
	if (request_irq(IRQ_GPIO52, collie_actkey_irq,
		  SA_INTERRUPT, "actkey", NULL)) 
	{
		printk("Could not allocate KEYBD IRQ!\n");
	}
*/
#ifdef CONFIG_PM
	collie_kbd_pm_dev = pm_register(PM_SYS_DEV, 0, collie_kbd_pm_callback);
#endif

#ifdef USE_WAKEUP_BUTTON
	collie_wakeup_button_init();
#endif //USE_WAKEUP_BUTTON
	printk("keyboard initilaized.\n");
}


#if 0 //ned
static void direct_key_poll(void)
{
	while(1)
	{
		interruptible_sleep_on_timeout((wait_queue_head_t*)&dirkey_count, DIRKEY_TICKTIME );

		if( dirkey_poll == 0 )
		{
			dirkey_poll = 0xff;
			printk(" key_in = %x \n", key_in);
				/* make key */
			if(key_in == Down_Code)
			{
				handle_scancode(KBSCANCDE(Button_code[4],KBDOWN),1);
				dir_key = Button_code[4];
				printk(" make dir key : Button_code = %d \n",dir_key);
			}
			if(key_in == Up_Code)
			{
				handle_scancode(KBSCANCDE(Button_code[5],KBDOWN),1);
				dir_key = Button_code[5];
				printk(" make dir key : Button_code = %d \n",dir_key);
			}
			if(key_in == Left_Code)
			{
				handle_scancode(KBSCANCDE(Button_code[6],KBDOWN),1);
				dir_key = Button_code[6];
				printk(" make dir key : Button_code = %d \n",dir_key);
			}
			if(key_in == Right_Code)
			{
				handle_scancode(KBSCANCDE(Button_code[7],KBDOWN),1);
				dir_key = Button_code[7];
				printk(" make dir key : Button_code = %d \n",dir_key);
			}
		}
		else //dirkey_poll
		{
			dirkey_poll = 0xff;
			printk(" break dir key : Button_code = %d \n",dir_key);
			handle_scancode(KBSCANCDE(dir_key,KBUP),0);
		} //dirkey_poll
	} //while(1)
} //end of thread
#endif //ned














int collie_kbd_translate(unsigned char scancode, unsigned char *keycode_p)
{

	/*
 	* We do this strangley to be more independent of 
 	* our headers..
 	*/
	
//k	*keycode_p = scancode & ~(KBDOWN | KBUP);

	return 1;
}

