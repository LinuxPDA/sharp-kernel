/*
 *  cerf_ucb1400gpio.c
 *
 *  UCB1400 GPIO control stuff for the cerf.
 *
 *  Copyright (C) 2002 Intrinsyc Software Inc.
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License version 2 as
 *  published by the Free Software Foundation.
 *
 *  History:
 *    Mar 2002: Initial version [FB]
 *    Jun 2002: Removed ac97 dependency [FB]
 * 
 */
#include <linux/config.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/sched.h>
#include <linux/errno.h>
#include <linux/string.h>
#include <linux/ctype.h>
#include <linux/mm.h>
#include <linux/init.h>
#include <linux/delay.h>

#include <asm/system.h>
#include <asm/hardware.h>
#include <asm/io.h>
#include <asm/irq.h>
#include <asm/uaccess.h>

#include "ucb1x00.h"

/*
 * Set this to zero to remove all the debug statements via
 * dead code elimination.
 */
#define DEBUGGING       0

#if DEBUGGING
static unsigned int ucb_debug = DEBUGGING;
#else
#define ucb_debug       0
#endif

#define UP	1
#define DOWN	0

/* -- -- */

void cerf_ucb1400gpio_lcd_enable( void)
{
	struct ucb1x00 * ucb = ucb1x00_get();
	if( ucb_debug > 2) printk( KERN_INFO "Enabling LCD.\n");
	/* Enable [not] LCD_RESET to enable the LCD display */
	ucb1x00_io_set_dir( ucb, 0, UCB1400_GPIO_LCD_RESET);
	ucb1x00_io_write( ucb, UCB1400_GPIO_LCD_RESET, 0);

	/* Enable the Contrast circuit */
	ucb1x00_io_set_dir( ucb, 0, UCB1400_GPIO_CONT_ENA);
	ucb1x00_io_write( ucb, UCB1400_GPIO_CONT_ENA, 0);
}

void cerf_ucb1400gpio_lcd_disable( void)
{
	struct ucb1x00 * ucb = ucb1x00_get();
	if( ucb_debug > 2) printk( KERN_INFO "Disabling LCD.\n");
	/* Disable the Contrast circuit  */
	ucb1x00_io_set_dir( ucb, 0, UCB1400_GPIO_CONT_ENA);
	ucb1x00_io_write( ucb, 0, UCB1400_GPIO_CONT_ENA);

	/* Disable [not] LCD_RESET to enable the LCD display */
	ucb1x00_io_set_dir( ucb, 0, UCB1400_GPIO_LCD_RESET);
	ucb1x00_io_write( ucb, 0, UCB1400_GPIO_LCD_RESET);
}

void cerf_ucb1400gpio_lcd_contrast_step( int direction)
{
	struct ucb1x00 * ucb = ucb1x00_get();
        // Assert the chip select and the up modifier
        ucb1x00_io_set_dir( ucb, 0, 
			(UCB1400_GPIO_CONT_CS |
                         UCB1400_GPIO_CONT_DOWN |
                         UCB1400_GPIO_CONT_INC));

	if( direction == DOWN)
	{
		if( ucb_debug > 3)
			printk(KERN_INFO "cerf_ucb1400gpio_lcd_contrast_step: "
				"stepping up\n");
		//goin' up
		ucb1x00_io_write( ucb, UCB1400_GPIO_CONT_DOWN, 0);
	}
	else
	{
		if( ucb_debug > 3)
			printk(KERN_INFO "cerf_ucb1400gpio_lcd_contrast_step: "
				"stepping down\n");
		//goin' down
		ucb1x00_io_write( ucb, 0, UCB1400_GPIO_CONT_DOWN);
	}

	ucb1x00_io_write( ucb, 0, UCB1400_GPIO_CONT_CS);

        // Assert the line up, down then up again
	ucb1x00_io_write( ucb, 0, UCB1400_GPIO_CONT_INC);
        udelay(1);
	ucb1x00_io_write( ucb, UCB1400_GPIO_CONT_INC, 0);
        udelay(1);
	ucb1x00_io_write( ucb, 0, UCB1400_GPIO_CONT_INC);

        // Deassert the chip select and the up modifier
	ucb1x00_io_write( ucb, 0, UCB1400_GPIO_CONT_DOWN);
	ucb1x00_io_write( ucb, UCB1400_GPIO_CONT_CS, 0);
}

/* -- -- */

void cerf_ucb1400gpio_irda_enable( void)
{
	struct ucb1x00 * ucb = ucb1x00_get();
	printk( KERN_INFO "Enabling IRDA.\n");
	/* Enable IRDA (active low) */
	ucb1x00_io_set_dir( ucb, 0, UCB1400_GPIO_IRDA_ENABLE);
	ucb1x00_io_write( ucb, 0, UCB1400_GPIO_IRDA_ENABLE);
}

void cerf_ucb1400gpio_irda_disable( void)
{
	struct ucb1x00 * ucb = ucb1x00_get();
	printk( KERN_INFO "Disabling IRDA.\n");
	/* Disable IRDA (active low) */
	ucb1x00_io_set_dir( ucb, 0, UCB1400_GPIO_IRDA_ENABLE);
	ucb1x00_io_write( ucb, UCB1400_GPIO_IRDA_ENABLE, 0);
}

/* -- -- */

void cerf_ucb1400gpio_bt_enable( void)
{
	struct ucb1x00 * ucb = ucb1x00_get();
	printk( KERN_INFO "Enabling Bluetooth.\n");
	/* Enable BT (active low) */
	ucb1x00_io_set_dir( ucb, 0, UCB1400_GPIO_BT_ENABLE);
	ucb1x00_io_write( ucb, 0, UCB1400_GPIO_BT_ENABLE);
}

void cerf_ucb1400gpio_bt_disable( void)
{
	struct ucb1x00 * ucb = ucb1x00_get();
	printk( KERN_INFO "Disabling Bluetooth.\n");
	/* Disable BT (active low) */
	ucb1x00_io_set_dir( ucb, 0, UCB1400_GPIO_BT_ENABLE);
	ucb1x00_io_write( ucb, UCB1400_GPIO_BT_ENABLE, 0);
}

/* -- -- */

/* -- Enable Bluetooth and IRDA automatically via pseudo module -- */
#if defined(CONFIG_BLUEZ) || defined(CONFIG_IRDA)
static int __init cerf_ucb1400gpio_module_init (void)
{
#ifdef CONFIG_BLUEZ
        cerf_ucb1400gpio_bt_enable();
#endif

#ifdef CONFIG_IRDA
        cerf_ucb1400gpio_irda_enable();
#endif
	return 0;
}

static void __exit cerf_ucb1400gpio_module_exit (void)
{
#ifdef CONFIG_BLUEZ
        cerf_ucb1400gpio_bt_disable();
#endif

#ifdef CONFIG_IRDA
        cerf_ucb1400gpio_irda_disable();
#endif
}

module_init(cerf_ucb1400gpio_module_init);
module_exit(cerf_ucb1400gpio_module_exit);
#endif

