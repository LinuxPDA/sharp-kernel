/*
 *  linux/drivers/char/corgi_rc.c
 *
 *  Copyright (c) 2002 LINEO Japan, Inc.
 *
 * May be copied or modified under the terms of the GNU General Public
 * License.  See linux/COPYING for more information.
 *
 * Change Log
 * 12-Dec-2002 Sharp Corporation
 * 04-Aug-2004 Lineo Solutions, Inc.  for Spitz
 */

#include <linux/sched.h>
#include <linux/interrupt.h>
#include <linux/signal.h>
#include <linux/kmod.h>
#include <linux/init.h>
#include <linux/poll.h>
#include <linux/mm.h>
#include <linux/slab.h>
#include <linux/string.h>
#include <linux/kbd_ll.h>
#include <linux/pm.h>
#include <linux/delay.h>

#include <asm/irq.h>
#include <asm/hardware.h>
#include <asm/sharp_keycode.h>
#include <asm/arch/keyboard_corgi.h>
#include <asm/arch/corgi.h>
#if defined(CONFIG_SOUND_CORGI) || defined(CONFIG_SOUND_SPITZ)
#include <asm/sharp_char.h>
#endif

extern void pxa_ssp_init(void);

//#define REMOCON_POWER_CONTROL		1
#undef REMOCON_POWER_CONTROL

#define REMOCON_MINOR		12

#if defined(CONFIG_ARCH_PXA_SPITZ)

#define RELEASE_HI		238
#define MAX_VOL_DWN		237
#define MIN_VOL_DWN		202
#define MAX_MUTE		201
#define MIN_MUTE		168
#define MAX_REWIND		167
#define MIN_REWIND		135
#define MAX_VOL_UP		134
#define MIN_VOL_UP		105
#define MAX_FORWARD		104
#define MIN_FORWARD		74
#define MAX_PLAY		73
#define MIN_PLAY		42
#define MAX_STOP		41
#define MIN_STOP		12
#define MAX_EARPHONE		11

#else

#define TRIAL_BOARD1
#if defined(TRIAL_BOARD0)
#define RELEASE_HI		240
#define MAX_VOL_DWN		232
#define MIN_VOL_DWN		212
#define MAX_VOL_UP		194
#define MIN_VOL_UP		173
#define MAX_FORWARD		155
#define MIN_FORWARD		135
#define MAX_REWIND		116
#define MIN_REWIND		96
#define MAX_STOP		78
#define MIN_STOP		57
#define MAX_PLAY		39
#define MIN_PLAY		19
#define MAX_EARPHONE	6

#elif defined(TRIAL_BOARD1)
#define RELEASE_HI		230
#define MAX_VOL_DWN		186
#define MIN_VOL_DWN		170
#define MAX_VOL_UP		132
#define MIN_VOL_UP		115
#define MAX_FORWARD		93
#define MIN_FORWARD		77
#define MAX_REWIND		58
#define MIN_REWIND		46
#define MAX_STOP		35
#define MIN_STOP		27
#define MAX_PLAY		13
#define MIN_PLAY		7
#define MAX_EARPHONE	6
#endif

#endif

#define MAXCTRL_PD0_SH		0
#define MAXCTRL_PD1_SH		1
#define MAXCTRL_SGL_SH		2
#define MAXCTRL_UNI_SH		3
#define MAXCTRL_SEL_SH		4
#define MAXCTRL_STR_SH		7
#define MAXCTRL_PD1_MSK		2

#define REMOTE_CONTROL_REL	1
#define REMOTE_CONTROL_VOLUP	2
#define REMOTE_CONTROL_VOLDOWN	3
#define REMOTE_CONTROL_FF	4
#define REMOTE_CONTROL_REW	5
#define REMOTE_CONTROL_STOP	6
#define REMOTE_CONTROL_PLAY	7
#define REMOTE_CONTROL_PHONE	8
#define REMOTE_CONTROL_REP	9
#if defined(CONFIG_ARCH_PXA_SPITZ)
#define REMOTE_CONTROL_MUTE     10
#endif

#define RC_POLL_TIMER		(HZ/100)
#define RC_PHONE_TIMER		(HZ/2)
#define RC_SUSPEND_TIMER	(HZ*2)


struct timer_list remocon_timers;
static void remocon_timer(unsigned long);
#ifdef REMOCON_POWER_CONTROL
struct timer_list suspend_timer;
static int is_suspend_timer = 0;
#endif
extern int ssp_get_max1111_val(unsigned long);
extern unsigned long ssp_get_dac_val(unsigned long, int);
extern unsigned short set_scoop_gpio(unsigned short);
extern unsigned short reset_scoop_gpio(unsigned short);

#ifdef CONFIG_PM
static struct pm_dev *remocon_pm_dev;
#endif

#if DEBUG
#define DPRINTK(fmt, args...) printk( fmt , ## args)
#else
#define DPRINTK(fmt, args...)
#endif

#if defined(CONFIG_SOUND_CORGI) || defined(CONFIG_SOUND_SPITZ)
extern void Corgi_hp_in_interrupt(void);
static int remocon_dev_stat = HPJACK_STATE_NONE;
#endif

static int remocon_scan_state = 8;
static int read_first = REMOTE_CONTROL_REL;
static int last_key = SLKEY_RCREL;
static int button_type = SLKEY_RCREL;
static int remocon_noise_count = 0;

#ifdef REMOCON_POWER_CONTROL
static void remocon_suspend_timer(unsigned long dummy)
{
	handle_scancode(SLKEY_OFF|KBDOWN , 1);
	handle_scancode(SLKEY_OFF|KBUP   , 0);
}
#endif

int get_dac_value(void)
{
	unsigned long cmd;

	/* translate ADC */
	cmd = (1u << MAXCTRL_PD0_SH) | (1u << MAXCTRL_PD1_SH) |
		(1u << MAXCTRL_SGL_SH) | (1u << MAXCTRL_UNI_SH) |
		(0  << MAXCTRL_SEL_SH) | (1u <<MAXCTRL_STR_SH);

	return ssp_get_max1111_val(cmd);
}

int get_remocon_raw(void)
{
	int val, type = 0;

	val = get_dac_value();
	DPRINTK("val=%d\n", val);

	// Button release
	if (val >= RELEASE_HI)
	  type = REMOTE_CONTROL_REL;
	// Volume down
	else if (val >= MIN_VOL_DWN && val <= MAX_VOL_DWN)
	  type = REMOTE_CONTROL_VOLDOWN;
	// Volume up
	else if (val >= MIN_VOL_UP && val <= MAX_VOL_UP)
	  type = REMOTE_CONTROL_VOLUP;
	// FF
	else if (val >= MIN_FORWARD && val <= MAX_FORWARD)
	  type = REMOTE_CONTROL_FF;
	// Rew
	else if (val >= MIN_REWIND && val <= MAX_REWIND)
	  type = REMOTE_CONTROL_REW;
	// Stop
	else if (val >= MIN_STOP && val <= MAX_STOP)
	  type = REMOTE_CONTROL_STOP;
	// Play
	else if (val >= MIN_PLAY && val <= MAX_PLAY)
	  type = REMOTE_CONTROL_PLAY;
	// Mute
#if defined(CONFIG_ARCH_PXA_SPITZ)
	// Mute
	else if (val >= MIN_MUTE && val <= MAX_MUTE)
	  type = REMOTE_CONTROL_MUTE;
#endif
	// ear phone
	else if (val <= MAX_EARPHONE)
	  type = REMOTE_CONTROL_PHONE;

	return type;

}


static void remocon_interrupt(int irq, void *dev_id, struct pt_regs *regs)
{
#if defined(CONFIG_SOUND_SPITZ)
	extern int sharp_HPstatus;
	if (sharp_HPstatus!=HPJACK_STATE_REMOCON) return;
#endif
#if defined(CONFIG_SOUND_CORGI) || defined(CONFIG_SOUND_SPITZ)
	if(( remocon_scan_state >= 4 ) && ( remocon_scan_state <= 6 )){
		disable_irq(irq);
		// eject remocon ?
		if(( get_remocon_raw() == REMOTE_CONTROL_PHONE )
		   &&( remocon_dev_stat == HPJACK_STATE_REMOCON )){
			remocon_dev_stat = HPJACK_STATE_NONE;
		}
#if defined(CONFIG_SOUND_CORGI)
		Corgi_hp_in_interrupt();
#endif
		return;
	}
#endif
	disable_irq(irq);
#if defined(CONFIG_ARCH_PXA_SPITZ)
	reset_scoop2_gpio(SCP2_AKIN_PULLUP);
#else
	reset_scoop_gpio(SCP_AKIN_PULLUP);
#endif
	read_first = get_remocon_raw();
	remocon_scan_state = 0;
	remocon_noise_count = 0;
	last_key = read_first;
	remocon_timers.function = remocon_timer;
	remocon_timers.expires = jiffies + RC_POLL_TIMER;
	add_timer(&remocon_timers);
}

static void remocon_timer(unsigned long dummy)
{
	int type;
	int	is_irqen = 0, interval = 0;
	DPRINTK( "remocon_timer(%d)\n",remocon_scan_state);
	if( remocon_scan_state < 6 ){
		type = get_remocon_raw();
	}else{
		type = 0;
	}
	switch( remocon_scan_state ){
	case 0:	case 1: case 2:
		interval = RC_POLL_TIMER;
		if( last_key == type ){
			remocon_scan_state ++;
		}else{
			last_key = type;
			remocon_scan_state = 0;
			remocon_noise_count++;
		}
		break;
	case 3:
		if( last_key != type ){
			type = last_key;
			remocon_noise_count++;
		}
		DPRINTK( "noise count:%d\n", remocon_noise_count );
		interval = RC_POLL_TIMER;
		switch(type) {
		case REMOTE_CONTROL_REL:
			DPRINTK("release\n");
			remocon_scan_state = 7;
#if defined(CONFIG_SOUND_CORGI) || defined(CONFIG_SOUND_SPITZ)
			// it looks like a head-phone (AD value 3 typ.) at ejecting remocon
			if(( read_first == REMOTE_CONTROL_PHONE )||( remocon_noise_count > 3 )){
				remocon_dev_stat = HPJACK_STATE_NONE;
			}
#endif
			break;
		case REMOTE_CONTROL_VOLUP:
#if defined(CONFIG_SOUND_CORGI) || defined(CONFIG_SOUND_SPITZ)
			remocon_dev_stat = HPJACK_STATE_REMOCON;
#endif
			DPRINTK("vol up\n");
			handle_scancode(SLKEY_RCVOLUP|KBDOWN, 1);
			button_type = SLKEY_RCVOLUP;
			break;
		case REMOTE_CONTROL_VOLDOWN:
#if defined(CONFIG_SOUND_CORGI) || defined(CONFIG_SOUND_SPITZ)
			remocon_dev_stat = HPJACK_STATE_REMOCON;
#endif
			DPRINTK("vol down\n");
			handle_scancode(SLKEY_RCVOLDWN|KBDOWN, 1);
			button_type = SLKEY_RCVOLDWN;
			break;
		case REMOTE_CONTROL_FF:
#if defined(CONFIG_SOUND_CORGI) || defined(CONFIG_SOUND_SPITZ)
			remocon_dev_stat = HPJACK_STATE_REMOCON;
#endif
			DPRINTK("forward\n");
			handle_scancode(SLKEY_RCFF|KBDOWN, 1);
			button_type = SLKEY_RCFF;
			break;
		case REMOTE_CONTROL_REW:
#if defined(CONFIG_SOUND_CORGI) || defined(CONFIG_SOUND_SPITZ)
			remocon_dev_stat = HPJACK_STATE_REMOCON;
#endif
			DPRINTK("rewind\n");
			handle_scancode(SLKEY_RCREW|KBDOWN, 1);
			button_type = SLKEY_RCREW;
			break;
		case REMOTE_CONTROL_STOP:
#if defined(CONFIG_SOUND_CORGI) || defined(CONFIG_SOUND_SPITZ)
			remocon_dev_stat = HPJACK_STATE_REMOCON;
#endif
			DPRINTK("stop\n");
#ifdef REMOCON_POWER_CONTROL
			if (!is_suspend_timer) {
				is_suspend_timer = 1;
				suspend_timer.function = remocon_suspend_timer;
				suspend_timer.expires = jiffies + RC_SUSPEND_TIMER;
				add_timer(&suspend_timer);
			}
#endif
			handle_scancode(SLKEY_RCSTP|KBDOWN, 1);
			button_type = SLKEY_RCSTP;
			break;
		case REMOTE_CONTROL_PLAY:
#if defined(CONFIG_SOUND_CORGI) || defined(CONFIG_SOUND_SPITZ)
			remocon_dev_stat = HPJACK_STATE_REMOCON;
#endif
			DPRINTK("play\n");
			handle_scancode(SLKEY_RCPLY|KBDOWN, 1);
			button_type = SLKEY_RCPLY;
			break;
#if defined(CONFIG_ARCH_PXA_SPITZ)
		case REMOTE_CONTROL_MUTE:
#if defined(CONFIG_SOUND_CORGI) || defined(CONFIG_SOUND_SPITZ)
			remocon_dev_stat = HPJACK_STATE_REMOCON;
#endif
			DPRINTK("mute\n");
			handle_scancode(SLKEY_RCMUTE|KBDOWN, 1);
			button_type = SLKEY_RCMUTE;
			break;
#endif
		case REMOTE_CONTROL_PHONE:
#if defined(CONFIG_SOUND_CORGI) || defined(CONFIG_SOUND_SPITZ)
			remocon_dev_stat = HPJACK_STATE_HEADPHONE;
#endif
			DPRINTK("phone\n");
			remocon_scan_state = 7;
			break;
		default:
#if defined(CONFIG_SOUND_CORGI) || defined(CONFIG_SOUND_SPITZ)
			// it looks like a head-phone (AD value 3 typ.) at ejecting remocon
			if(( read_first == REMOTE_CONTROL_PHONE )||( remocon_noise_count > 3 )){
				remocon_dev_stat = HPJACK_STATE_NONE;
			}
#endif
			DPRINTK("out of range\n");
			remocon_scan_state = 7;
			break;
		}
#ifdef REMOCON_POWER_CONTROL
		if (type != REMOTE_CONTROL_STOP) {
			del_timer(&suspend_timer);
			is_suspend_timer = 0;
		}
#endif
#if defined(CONFIG_SOUND_CORGI)
		Corgi_hp_in_interrupt();
#endif
		if( remocon_scan_state == 3 ){
			remocon_scan_state ++;
#if defined(CONFIG_SOUND_CORGI) || defined(CONFIG_SOUND_SPITZ)
			// to catch eject remocon...
			enable_irq(IRQ_GPIO_AK_INT);
#endif
		}
		break;
	case 4: case 5:	// release wait
		interval = RC_POLL_TIMER;
		if( type != last_key ){
			remocon_scan_state ++;
		}else{
			remocon_scan_state = 4;
		}
		break;
	case 6:	// release
		DPRINTK("release 1\n");
		handle_scancode(button_type|KBUP, 0);
		// go below
	case 7:
#ifdef REMOCON_POWER_CONTROL
		del_timer(&suspend_timer);
		is_suspend_timer = 0;
#endif
		remocon_scan_state = 8;
#if defined(CONFIG_ARCH_PXA_SPITZ)
		set_scoop2_gpio(SCP2_AKIN_PULLUP);
#else
		set_scoop_gpio(SCP_AKIN_PULLUP);
#endif
		is_irqen = 1;
		break;
	default:
		is_irqen = 1;
		return;
	}

	if( is_irqen ){
		enable_irq(IRQ_GPIO_AK_INT);
	}else{
		if( interval != 0 ){
			remocon_timers.function = remocon_timer;
			remocon_timers.expires = jiffies + interval;
			add_timer(&remocon_timers);
		}
	}
}


#ifdef CONFIG_PM
static int remocon_pm_callback(struct pm_dev *pm_dev,
			   pm_request_t req, void *data)
{
	switch (req) {
	case PM_SUSPEND:
		disable_irq(IRQ_GPIO_AK_INT);
		del_timer(&remocon_timers);
#ifdef REMOCON_POWER_CONTROL
		del_timer(&suspend_timer);
#endif
#if defined(CONFIG_ARCH_PXA_SPITZ)
		set_scoop2_gpio(SCP2_AKIN_PULLUP);
#else
		set_scoop_gpio(SCP_AKIN_PULLUP);
#endif
		remocon_scan_state = 8;
		read_first = REMOTE_CONTROL_REL;
		last_key = SLKEY_RCREL;
		button_type = SLKEY_RCREL;
		break;
	case PM_RESUME:
#if defined(CONFIG_ARCH_PXA_SPITZ)
		set_scoop2_gpio(SCP2_AKIN_PULLUP);
#else
		set_scoop_gpio(SCP_AKIN_PULLUP);
#endif
		enable_irq(IRQ_GPIO_AK_INT);
		break;
	}
	return 0;
}
#endif

static int __init remocon_init(void)
{
#if defined(CONFIG_ARCH_PXA_SPITZ)
	printk("spitz remote controller\n");
	SCP2_REG_GPCR |= SCP2_AKIN_PULLUP;
	SCP2_REG_GPWR |= SCP2_AKIN_PULLUP;
#else
	printk("corgi remote controller\n");
	SCP_REG_GPCR |= SCP_AKIN_PULLUP;
	SCP_REG_GPWR |= SCP_AKIN_PULLUP;
#endif
#if defined(CONFIG_SOUND_CORGI) || defined(CONFIG_SOUND_SPITZ)
	set_GPIO_IRQ_edge(GPIO_AK_INT, GPIO_BOTH_EDGES);
#else
	set_GPIO_IRQ_edge(GPIO_AK_INT, GPIO_RISING_EDGE);
#endif
	if (request_irq(IRQ_GPIO_AK_INT, remocon_interrupt, 0, "remocon", NULL)) {
		return -EBUSY;
	}
	init_timer(&remocon_timers);
#ifdef REMOCON_POWER_CONTROL
	init_timer(&suspend_timer);
#endif
#ifdef CONFIG_PM
	remocon_pm_dev = pm_register(PM_SYS_DEV, 0, remocon_pm_callback);
#endif

	return 0;
}

static void __exit remocon_exit(void)
{
#if defined(CONFIG_ARCH_PXA_SPITZ)
	SCP2_REG_GPWR &= ~SCP2_AKIN_PULLUP;
#else
	SCP_REG_GPWR &= ~SCP_AKIN_PULLUP;
#endif
	free_irq(IRQ_GPIO_AK_INT, NULL);
#ifdef CONFIG_PM
	pm_unregister(remocon_pm_dev);
#endif
}

int corgi_wakeup_remocon_hook(void)
{
	int status = 0;
    int	type, last_type;
    int	i, count;
#ifdef REMOCON_POWER_CONTROL
    int wait_release = 0;
#endif

	pxa_ssp_init();
#if defined(CONFIG_ARCH_PXA_SPITZ)
	reset_scoop2_gpio(SCP2_AKIN_PULLUP);
#else
	reset_scoop_gpio(SCP_AKIN_PULLUP);
#endif

	count = 0;
	last_type = get_remocon_raw();
	for( i=0; i<50; i++ ){
		mdelay(20);
		type = get_remocon_raw();
		if( last_type == type ){
			if( ++count > 2 ) break;
		}else{
			last_type = type;
			count = 0;
		}
	}

	if( count > 2 ){
		switch(type) {
		case REMOTE_CONTROL_PLAY:
#ifdef REMOCON_POWER_CONTROL
			status = 1;
			wait_release = 1;
#endif
#if defined(CONFIG_SOUND_SPITZ)
		    status = 1;
		    handle_scancode(SLKEY_RCPLY|KBDOWN, 1);
		    handle_scancode(SLKEY_RCPLY|KBUP, 0);
#endif
			// go below
		case REMOTE_CONTROL_VOLUP:
		case REMOTE_CONTROL_VOLDOWN:
		case REMOTE_CONTROL_FF:
		case REMOTE_CONTROL_REW:
		case REMOTE_CONTROL_STOP:
#if defined(CONFIG_SOUND_CORGI) || defined(CONFIG_SOUND_SPITZ)
			remocon_dev_stat = HPJACK_STATE_REMOCON;
#endif
			break;
		case REMOTE_CONTROL_PHONE:
#if defined(CONFIG_SOUND_CORGI) || defined(CONFIG_SOUND_SPITZ)
			remocon_dev_stat = HPJACK_STATE_HEADPHONE;
#endif
			break;
		case REMOTE_CONTROL_REL:
		default:
#if defined(CONFIG_SOUND_CORGI) || defined(CONFIG_SOUND_SPITZ)
			remocon_dev_stat = HPJACK_STATE_NONE;
#endif
			break;
		}
#ifdef REMOCON_POWER_CONTROL
		if( wait_release ){
			while (get_remocon_raw() != REMOTE_CONTROL_REL);
		}
#endif
	}else{
#if defined(CONFIG_SOUND_CORGI) || defined(CONFIG_SOUND_SPITZ)
		remocon_dev_stat = HPJACK_STATE_NONE;
#endif
	}

#if defined(CONFIG_ARCH_PXA_SPITZ)
	set_scoop2_gpio(SCP2_AKIN_PULLUP);
#else
	set_scoop_gpio(SCP_AKIN_PULLUP);
#endif

	return status;
}

#if defined(CONFIG_SOUND_CORGI) || defined(CONFIG_SOUND_SPITZ)
int	get_remocon_state( void )
{
	return remocon_dev_stat;
}
#endif

module_init(remocon_init);
module_exit(remocon_exit);
