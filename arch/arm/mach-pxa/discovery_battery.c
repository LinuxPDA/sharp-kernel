/*
 * linux/arch/arm/mach-pxa/discovery_battery.c
 *
 * Battery routines for collie (SHARP)
 *
 * Copyright (C) 2002 Steve Lin (stevelin168@hotmail.com)
 *
 *
 * Based on:
 * linux/arch/arm/mach-sa1100/collie_battery.c
 *
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
 * ChangeLog:
 *	27-AUG-2002 Steve Lin for Discovery
 *	31-Jul-2002 Lineo Japan, Inc.  for ARCH_PXA
 *	16-Jan-2003 SHARP named kernel threads
 *
 *  modify: TX/RX I2C for DEMO data is ok.
 *
 */


/* this driver support the following functions
 *      - apm_get_power_status
 *      - charge proc
 */
#include <linux/config.h>
#include <linux/module.h>
#include <linux/poll.h>
#include <linux/types.h>
#include <linux/stddef.h>
#include <linux/timer.h>
#include <linux/fcntl.h>
#include <linux/malloc.h>
#include <linux/stat.h>
#include <linux/proc_fs.h>
#include <linux/miscdevice.h>
#include <linux/init.h>
#include <linux/sched.h>
#include <linux/pm.h>
#include <linux/kernel.h>
#include <linux/smp_lock.h>
#include <linux/apm_bios.h>
#include <linux/delay.h>
#include <linux/interrupt.h>
#include <asm/hardware.h>
#include <asm/irq.h>
#include <asm/io.h>
#include <asm/sharp_char.h>

/* added new header files */
#include <asm/arch/ads7846_ts.h>
#include <asm/arch/discovery_asic3.h>
#include <asm/arch/discovery_gpio.h>
#include <asm/arch/discovery_battery.h>
#include <asm/arch/i2sc.h>
#include <asm/sharp_apm.h>

/*** prototype *********************************************************************/
int discovery_read_MBattery(void);
int discovery_read_Temper(void);
int discovery_read_IMIN(void);
static void discovery_charge_on(void);
static void discovery_charge_off(void);
int set_led_status(int ,int);
int GetMainLevel( unsigned int);
unsigned short Get_init_Temp_DAC(void);
static void discovery_bat_i2c_init(unsigned char);
void clr_driver_waste_bits(unsigned short);
void set_driver_waste_bits(unsigned short);
void BackPack_Process(void);
int discovery_read_MBattery(void);
int Get_DAC_Value(int);
unsigned int ads7846_rw_cli(unsigned long);
int discovery_get_main_battery(void);
unsigned short Get_voltage(unsigned short);

unsigned char 	index_temp, second_battery_index , second_battery_index, second_charge_status=0, cf_3V_on;
unsigned long 	ac_interrupt=0;
unsigned long	Init_Voltage;
int		HWR_flag;

/*** local variables *************************************************************/
static struct file_operations collie_battery_fops = {};

/*
 * The battery device is one of the misc char devices.
 * This is its minor number.
 */
#define	BATTERY_MINOR_DEV	215

static struct miscdevice battery_device = {
	BATTERY_MINOR_DEV,
	"battery",
	&collie_battery_fops
};

#define BATT_CHARGE_ON()	GPCR1 = GPIO_GPIO(56)
#define BATT_CHARGE_OFF()	GPSR1 = GPIO_GPIO(56)

#define	BATT_SAM_LOW()		GPCR1 = GPIO_GPIO(57)
#define BATT_SAM_HIGH()		GPSR1 = GPIO_GPIO(57)

/* for ASIC1 setting */
#if AC_IN_LEVEL	/* 1:has AC_IN=High, 0:has AC_IN=Low */
#define COLLIE_AC_LINE_STATUS (( ASIC3_GPIO_PSTS_D & AC_IN )? APM_AC_ONLINE : APM_AC_OFFLINE )
#else
#define COLLIE_AC_LINE_STATUS (( ASIC3_GPIO_PSTS_D & AC_IN )? APM_AC_OFFLINE : APM_AC_ONLINE)
#endif
#define	AC_IN_DETECT()		( ASIC3_GPIO_PSTS_D & AC_IN ) /* if AC_IN,it=0,if no AC_IN,it=0x02 */
#define BACKPACK_IN_DETECT()	( ASIC3_GPIO_PSTS_D & BACKPACK_DETECT ) /* 0: exist , 1: not in */

#define CHARGE_LED_ON()    set_led_status(SHARP_LED_CHARGER,LED_CHARGER_CHARGING)
#define CHARGE_LED_OFF()   set_led_status(SHARP_LED_CHARGER,LED_CHARGER_OFF)
#define CHARGE_LED_ERR()   set_led_status(SHARP_LED_CHARGER,LED_CHARGER_ERROR)


#define COLLIE_BATTERY_STATUS_HIGH	APM_BATTERY_STATUS_HIGH
#define COLLIE_BATTERY_STATUS_LOW	APM_BATTERY_STATUS_LOW
#define COLLIE_BATTERY_STATUS_VERYLOW	APM_BATTERY_STATUS_VERY_LOW
#define COLLIE_BATTERY_STATUS_CRITICAL	APM_BATTERY_STATUS_CRITICAL

int discovery_main_battery   = APM_BATTERY_STATUS_UNKNOWN; //COLLIE_BATTERY_STATUS_HIGH;
u_char discovery_second_battery   = APM_BATTERY_STATUS_UNKNOWN; //COLLIE_BATTERY_STATUS_HIGH;
int collie_ac_status = APM_AC_OFFLINE;
u_char DAC_B = 0, DAC_I = 0, DAC_T = 0, c_time;

static unsigned char version;  // Backpack version
static unsigned char checksum; // Backpack checksum
#ifdef CONFIG_PCMCIA
// This variable is defined by drivers/pcmcia/cotulla_discovery.c
extern unsigned char vcc_on;
#else
static unsigned char vcc_on;
#endif
static u8 need_delay;
static int first_poll = 1;
//extern void set_cf_irq();

static unsigned long backpackptr;

#ifdef CONFIG_PM
static struct pm_dev *battery_pm_dev;	 /* registered PM device */
#endif

#define BP_WAIT_TICK         		( 500 / 10 )  // 500msec

static DECLARE_WAIT_QUEUE_HEAD(queue);
static DECLARE_WAIT_QUEUE_HEAD(minute_cnt);
static DECLARE_WAIT_QUEUE_HEAD(bp_queue);
static DECLARE_WAIT_QUEUE_HEAD(bp_detect);
static __u32  on_bp_detect_time=0;
static unsigned char bp_pull_out = 0;
static unsigned short TX_COUNT = 0;
static unsigned short tmp ;

#define DEBUGPRINT(s)   /*printk s*/
#define DEBUGPRINT2(s)   /*printk s*/

#define FIX_STSCHG_WAKEUP 	1

spinlock_t my_lock = SPIN_LOCK_UNLOCKED;
unsigned short backinited = 0;

char TimeOut(unsigned long dwStart)				// check if I2C wait timeout
{
	unsigned long dwCurrent = jiffies;
	unsigned long dwDiff;

	if (dwCurrent >= dwStart)
		dwDiff = dwCurrent - dwStart;
	else
		dwDiff = dwStart - dwCurrent +1;

	if (dwDiff > 10)
	{
		return 1;
	}
	else
		return 0;
}

unsigned char READ_I2C_BYTE(unsigned char byDeviceAddress)
{
	unsigned char	byTemp,temp;
	unsigned short	wReturn;
	long	dwTickCount;
	unsigned short   rvalue;

 	up(&i2c_sem);
	spin_lock(&my_lock);
	
	I2C_ICR |= I2C_ICR_SCLE;
	I2C_ICR |= I2C_ICR_IUE;

	I2C_IDBR = byDeviceAddress | I2C_ISR_RWM;

	I2C_ICR &= ~I2C_ICR_ALDIE;
	I2C_ICR &= ~I2C_ICR_STOP;
	I2C_ICR |= I2C_ICR_START;
	I2C_ICR |= I2C_ICR_TB;

	udelay(100);
	dwTickCount = jiffies;
	while (1)
	{
		if (!in_interrupt())
			schedule();
		if (TimeOut(dwTickCount))		// check if timeout
		{
			byTemp = 0xFF;
			break;
		}

		if( ((I2C_ISR & I2C_ISR_RWM ) == I2C_ISR_RWM) && ( (I2C_ISR & I2C_ISR_UB) == I2C_ISR_UB)&& ( (I2C_ISR & I2C_ISR_ITE)== I2C_ISR_ITE) )
			break;			// ub -> unit busy, ite -> IDBR transmit empty

	}

	I2C_ISR = I2C_ISR_ITE;

	I2C_ICR &= ~I2C_ICR_START;
	I2C_ICR |= I2C_ICR_STOP;
	I2C_ICR |= I2C_ICR_ACK;
	I2C_ICR |= I2C_ICR_TB;

	dwTickCount = jiffies;
	while (1)
	{
		if (!in_interrupt())
			schedule();
		if (TimeOut(dwTickCount))		// check if timeout
		{
			byTemp = 0xFF;
			break;
		}

		if( ((I2C_ISR & I2C_ISR_RWM) == 1 ) && ( (I2C_ISR & I2C_ISR_UB) == I2C_ISR_UB ) && ( (I2C_ISR & I2C_ISR_IRF) == I2C_ISR_IRF )&& ( (I2C_ISR & I2C_ISR_ACK) == I2C_ISR_ACK ))
			break;			// ub -> unit busy, irf -> IDBR receive full

	}
	I2C_ISR = I2C_ISR_IRF;
	
	if (byTemp == 0xFF)
		temp = (unsigned char)I2C_IDBR;
	else	
		byTemp = (unsigned char)I2C_IDBR;

	I2C_ICR &= ~(I2C_ICR_START | I2C_ICR_STOP | I2C_ICR_TB | I2C_ICR_ACK);		

	udelay(1);

	I2C_ICR &= ~(I2C_ICR_IUE | I2C_ICR_SCLE);
	mdelay(1);
	spin_unlock(&my_lock);
 	down(&i2c_sem); 
	
	return byTemp;
}

unsigned char WRITE_I2C_BYTE(unsigned char byDeviceAddress,unsigned char byRegisterAddress,unsigned char bData)
{    	  
	long dwTickCount;
	int		ret_val;

 	up(&i2c_sem); 

	ret_val = 0;
	spin_lock(&my_lock);

	I2C_ICR |= I2C_ICR_SCLE;
	I2C_ICR |= I2C_ICR_IUE;
	I2C_IDBR = byDeviceAddress;

	I2C_ICR &= ~I2C_ICR_ALDIE;
	I2C_ICR &= ~I2C_ICR_STOP;
	I2C_ICR |= I2C_ICR_START;
	I2C_ICR |= I2C_ICR_TB;		// not stop
	udelay(500);
	dwTickCount = jiffies;
	while (1)
	{
		if (!in_interrupt())
			schedule();
		if (TimeOut(dwTickCount))		// check if timeout
		{
			ret_val = 1;
			break;
		}
		if( ((I2C_ISR & I2C_ISR_RWM ) ==0 ) && ( (I2C_ISR & I2C_ISR_UB) == I2C_ISR_UB)&& ( (I2C_ISR & I2C_ISR_ITE)== I2C_ISR_ITE) )
				break;			// ub -> unit busy, ite -> IDBR transmit empty
	}

	I2C_ISR = I2C_ISR_ITE;

	I2C_IDBR = byRegisterAddress;	
	I2C_ICR &= ~I2C_ICR_STOP;
	I2C_ICR &= ~I2C_ICR_START;
	I2C_ICR |= I2C_ICR_TB;

	dwTickCount = jiffies;
	udelay(500);
	while (1)
	{
		if (!in_interrupt())
			schedule();
		if (TimeOut(dwTickCount))		// check if timeout
		{
			ret_val = 1;
			break;
		}
		if( ((I2C_ISR & I2C_ISR_RWM) ==0 ) && ( (I2C_ISR & I2C_ISR_UB) == I2C_ISR_UB)&& ( (I2C_ISR & I2C_ISR_ITE)==I2C_ISR_ITE) )
			break;			// ub -> unit busy, ite -> IDBR transmit empty
	}

	I2C_ISR = I2C_ISR_ITE;
	I2C_IDBR = bData;	

	I2C_ICR &= ~I2C_ICR_START;
	I2C_ICR |= I2C_ICR_STOP;
	I2C_ICR |= I2C_ICR_TB;

	dwTickCount = jiffies;
	while (1)
	{
		if (!in_interrupt())
			schedule();
		if (TimeOut(dwTickCount))		// check if timeout
		{
			ret_val = 1;
			break;
		}
		if( ((I2C_ISR & I2C_ISR_RWM) == 0 ) && ( (I2C_ISR & I2C_ISR_ITE) == I2C_ISR_ITE ) )
			break;			// ub -> unit busy, ite -> IDBR transmit empty
	}
	I2C_ISR = I2C_ISR_ITE;

	I2C_ICR &= ~( I2C_ICR_ALDIE | I2C_ICR_TB | I2C_ICR_START | I2C_ICR_STOP);
	
	mdelay(1);
	spin_unlock(&my_lock);
 	down(&i2c_sem);

	if (ret_val)	//time out error
			I2C_ICR &= ~(I2C_ICR_IUE | I2C_ICR_SCLE);

	return ret_val;

}

int discovery_apm_get_power_status(u_char *ac_line_status,
                                u_char *battery_status,
                                u_char *battery_flag,
                                u_char *battery_percentage,
                                u_short *battery_life)
{
	*ac_line_status = collie_ac_status;

	*battery_status = discovery_main_battery;

	/* set battery percentage */
	*battery_percentage = battery_index;

	/* In charging stage ,battery status */
	if ( charge_status ) 
	{
		*battery_status = APM_BATTERY_STATUS_CHARGING;
		*battery_flag = (1 << 3);
	}else{
		if( !(*battery_status) )  
			*battery_flag = 0x01;
		else if (*battery_status == APM_BATTERY_STATUS_VERY_LOW) {
			*battery_flag = 0x20;
		} else
			*battery_flag = *battery_status << 1;
	}

	/* set battery life */
	*battery_life = APM_BATTERY_LIFE_UNKNOWN;

	return APM_SUCCESS;
}

int discovery_apm_get_bp_status(u_char *ac_line_status,
                                u_char *battery_status,
                                u_char *battery_flag,
                                u_char *battery_percentage,
                                u_short *battery_life)
{
	*ac_line_status = APM_AC_OFFLINE;//collie_ac_status;

	if ( !BACKPACK_IN_DETECT()){
		/* In charging stage ,battery status */
		if ( second_charge_status/*charge_status*/ ) 
		{
			*battery_status = APM_BATTERY_STATUS_CHARGING;
			*battery_flag = (1 << 3);//0x08;
		}
		else{
			*battery_status = discovery_second_battery;
			if( !(*battery_status) )  
				*battery_flag = 0x01;
			else if (*battery_status == APM_BATTERY_STATUS_VERY_LOW) {
				*battery_flag = 0x20;
			} else
				*battery_flag = *battery_status << 1;
		}
	}
	else{
		*battery_status = 0x02;
		*battery_flag = 0x04;//(1 << 2);
	}
	
	/* set battery percentage */
	*battery_percentage = second_battery_index;
	/* set battery life */
	*battery_life = APM_BATTERY_LIFE_UNKNOWN;

	return APM_SUCCESS;
}

static void discovery_charge_on(void)
{
	CHARGE_LED_ON();
	BATT_CHARGE_ON();

	/* charge status flag set */
	charge_status = 1;
	Charge_time = 0;
}

static void discovery_charge_off(void)
{
	BATT_CHARGE_OFF();

	/* charge status flag reset */
	charge_status = 0;
	Charge_time = 0;
	imin_count = 0;
	temp_count = 0;
	poll_flag &= ~BattBad_flag;
}


static void discovery_minute(void)
{
	daemonize();
	strcpy(current->comm, "kminuted");
	//sigfillset(&current->blocked);

	while(1)
	{
		interruptible_sleep_on_timeout((wait_queue_head_t*)&minute_cnt, DISCOVERY_MINUTE_TICKTIME );
		Charge_time++;
	}
}

unsigned short Get_voltage(unsigned short DACvolt)
{
	unsigned short	i;
	i= (unsigned short) ((DACvolt * num_2500)/num_4096);
	return i;
}

/*	Get Battery Index */
int Get_battery_index( unsigned short Temp_V , unsigned short Vim )
{
	unsigned short	i,battery_index,icount=0,DVim;

	for(i=0;i<QTY_TV_TX;i++)
	{
		if( Temp_V < TV_TX_Tab[i].tv )
		{
			Tx = TV_TX_Tab[i].tx;
			break;
		}
	}
	while(1)
	{
		if( Temp_V <= BattIndexTab[icount].batt_v )
			break; /* find it */
		icount++;
	}
	for( i=0;i<Qty_PrCnt;i++)
	{
		if( Vim >= (BattIndexTab[icount].vimval[i]) )
			break; /* find it */
	}
	i = i>6? 6:i; /* if non in table then be the last one */
	Vthx = BattIndexTab[icount].vimval[i];
	battery_index = BattPerCent[i];
	if( battery_index == 0 )
	{
		RCu = 0;	
	}
	else
	{
		DVim = DeltavimTab[icount].deltavim[i];
		RCu = ((((((Vim-Vthx)*num_20)/DVim)+battery_index)*num_80)*(num_100-(Tx<num_10?(num_10-Tx):0 ))  );
		if(RCu>num_820000)
		{RCu = num_820000;}
	}
	return battery_index;
}

//return 0: no charge
//		 1: charging
int	second_bat_charge_status(void) 
{
	unsigned char	bat_flag;

	DEBUGPRINT(("second_bat_charge_status:\n"));
	if (!BACKPACK_IN_DETECT() && backinited) {
		
		if ( second_charge_status) 
			return 1;
		else
			return 0;
	}
	return 0;	
}
 
static void discovery_battery_main_thread(void)
{
	unsigned long	Voltage, Imin, Temper;
	unsigned long	period;
	
	// daemonize();
	strcpy(current->comm, "kbattd");
	// sigfillset(&current->blocked);

	while(1) 
	{
		short		dTx,nowTx=0,Pd_k; /* maybe negative */
		unsigned short	i,DACvolt=0,Vim,Vmis,Vmxs,Vmx,new_index,Temp_V,icount=0,DAC_T,Pvm_k,Pbm_k,iMin,Para_L,DVim;
		unsigned short	Pnfm_k=0;
		unsigned int Pdc;

		VMAX = 0x6BB;

		period = CURRENT_TIME;
		interruptible_sleep_on_timeout((wait_queue_head_t*)&queue, DISCOVERY_APO_TICKTIME );

		if ((CURRENT_TIME - period) < 1 && (ac_interrupt==0)) {
			ac_interrupt = 0;
			break;
		} else {
			ac_interrupt = 0;
		}

		Voltage = discovery_read_MBattery();
		Temper = discovery_read_Temper();
		Imin = discovery_read_IMIN();

		collie_ac_status =  COLLIE_AC_LINE_STATUS;

#if AC_IN_LEVEL	/* 1:has AC_IN=High, 0:has AC_IN=Low */
		if ( (AC_IN_DETECT() == 0) )
#else
		if ( !(AC_IN_DETECT() == 0) )
#endif
		{
			IQTR_stage = Q_stage;
			discovery_charge_off();
			CHARGE_LED_OFF();
		} else if(  IQTR_stage == Q_stage  ) {
			if( Temper > Temper_V47 )
			{
				IQTR_stage = T_stage; /* Not Q_stage , leave it */
				discovery_charge_on();
			}
		}

		/* check battery full & Temperature */
		while(1)
		{
			if( IQTR_stage != T_stage ) break;

			/* if in discharge status then no need to check */
			if( charge_status == 0 ) break;

			if(Imin < (IMIN_LIMIT + ((driver_waste&Flite_waste)?0x48:num_0)+ ((driver_waste&BackPack_waste)?0x8a:num_0) + ( (driver_waste&BackPack_waste)? ((driver_waste&Flite_waste)?0x1d:num_0 ) : num_0  )   ) )
			{
				imin_count++;
				if ( imin_count > num_6 ) /* battery full & charging over 6*5sec(poll time) */
				{
					DEBUGPRINT(("  by IMIN too small, ENTER CHARGE OFF \n"));
					poll_flag |= driver_min_curr;
					IQTR_stage = R_stage ;
					discovery_charge_off();
					if (second_bat_charge_status())
						CHARGE_LED_ON();
					else
						CHARGE_LED_OFF();

					for(i=0;i<3;i++) {
						DACvolt += discovery_read_MBattery();
					}
					DACvolt /= 3; /* get average Vims */
					VMAX = DACvolt; // update VMAX
					DEBUGPRINT(("Update VMAX:%x\n",VMAX));

				}
				break;
			} else if( Temper < Temper_V50 ) {	// Charge disabled
				temp_count++;
				if ( temp_count > 3 ) /* battery full */
				{
					DEBUGPRINT(("  by Temp too high, ENTER CHARGE OFF \n"));
					IQTR_stage = R_stage;
					discovery_charge_off();
					CHARGE_LED_OFF();
					break;
				}
			} else if( ( Voltage < BattBad_V ) && ( Charge_time > Min_120 ) ) {
				DEBUGPRINT(("  by battery Bad, ENTER CHARGE OFF \n"));
				poll_flag |= BattBad_flag;	/* set battery bad flag */
				IQTR_stage = R_stage;
				discovery_charge_off();
				if (second_bat_charge_status())
					CHARGE_LED_ON();
				else
					CHARGE_LED_OFF();
				break;
			} else if( Charge_time > hour_8 ){
				DEBUGPRINT(("  by charge time over 8hours, ENTER CHARGE OFF \n"));
				IQTR_stage = R_stage;
				discovery_charge_off();
				CHARGE_LED_OFF();
				break;
			} else {
				/* nothing happened */
				break;
			}
		} //end of while(1) 

		/* check re-charging condition when in the discharge status & AC_IN */
		if( (IQTR_stage == R_stage) && (charge_status == 0) )
		{
			
#if AC_IN_LEVEL
			if( ( RCu <= 801000 )&&((poll_flag&BattBad_flag)==0)&& (AC_IN_DETECT()) )
#else
			if( ( RCu <= 801000 )&&((poll_flag&BattBad_flag)==0)&&(AC_IN_DETECT()==0) )
#endif
			{
				imin_count++;
					DEBUGPRINT(("  ReCharge now , do Charging job \n"));
					imin_count = 0;
					discovery_charge_on();
					IQTR_stage = T_stage;
			} else if (second_bat_charge_status()) {
				CHARGE_LED_ON();
			} else {
				CHARGE_LED_OFF();
			}
		}


		/****************************************************/
		/*                get battery status                */
		/****************************************************/

		if(((!(poll_flag&First_read))&&((HWR_flag)==0x01))||(((RCSR&RCSR_HWR)==0x00)&&(Reg_ini_volt==0)&&(!(poll_flag&First_read))))
		{
			poll_flag |= First_read; /* set poll_flag prevent come in again */
			DEBUGPRINT((" Reg_blob_ctrl&quick_off_on = %x \n",Reg_blob_ctrl&quick_off_on));

			/*** 1st time cold boot or warm wake =>quick_off_on=0 ***/  /* Stage (1),(6) */
			if(!(Reg_blob_ctrl&quick_off_on))
			{
				for(i=0;i<3;i++)
				{
					DACvolt += discovery_read_MBattery();
				}
				DACvolt /= 3; /* get average Vims */
				
				if (HWR_flag)
					DACvolt = Init_Voltage;
				
				DEBUGPRINT(("*****  Stage(1),(6), Battery Initial Voltage=%x, HWR_flag=%x, Init_Voltage=%x\n",DACvolt,HWR_flag,Init_Voltage));
				HWR_flag = 0;

				/* Pull low the Hareware Battery Sample pin */
#if BATT_SAM_LEVEL
				BATT_SAM_HIGH();
#else
				BATT_SAM_LOW();
#endif
				Vmis = DACvolt;
				Vim = 1000 * (Vmis * 42) / (VMAX*10) ;	// Deborah
				DEBUGPRINT(("Vmi %d, Vmis %d, VMAX %d\n",Vim,Vmis,VMAX));
				DAC_T = Get_init_Temp_DAC();
				Temp_V = Get_voltage(DAC_T);
				/* respond the Battery Index */
				battery_index = Get_battery_index( Temp_V , Vim );
				Reg_RCL = ((RCu + (RCu*(Tx<num_10?(num_10-Tx):0))/num_100)*num_6)/num_100;
				
				discovery_main_battery = GetMainLevel( battery_index );
				DEBUGPRINT((" Stage (1-3),(6-3), Vim = %d, DAC_T = %x, Temp_V = %d, Battery_Index = %d \n\n",Vim,DAC_T,Temp_V,battery_index));
			} else {
				/*** warm wake => quick_off_on = 1 ***/ /* Stage (7) */
				DAC_T = Get_init_Temp_DAC();
				Temp_V = Get_voltage(DAC_T);
				for(i=0;i<QTY_TV_TX;i++)
				{
					if( Temp_V < TV_TX_Tab[i].tv )
					{
						Tx = TV_TX_Tab[i].tx;
						break;
					}
				}
				RCu =((Reg_RCL*(num_100-(Tx<num_10?(num_10-Tx):0 )))/num_6);
				if(RCu>num_820000)
					{RCu = num_820000;}
#if BATT_SAM_LEVEL
				BATT_SAM_HIGH();
#else
				BATT_SAM_LOW();
#endif
				battery_index = RCu /(num_90 * (num_100-(Tx<num_10?(num_10-Tx):0 )) );
				DEBUGPRINT((" Stage (7-1), Pre-Battery_index = %d \n",battery_index));
				for( i=0;i<Qty_PrCnt;i++)
				{
					if( battery_index >= BattPerCent[i])
					{
						battery_index = BattPerCent[i];
						break;
					}
				}
				discovery_main_battery = GetMainLevel( battery_index );
				Reg_blob_ctrl &= ~quick_off_on;
				DEBUGPRINT((" Stage (7-2), Temp_V = %d, Battery_Index = %d , RCu = %d \n\n",Temp_V,battery_index,RCu));
			}
		}
		
		if(((RCSR&RCSR_HWR)==0x00) && (Reg_ini_volt!=0) && (!(poll_flag&First_read)) ) 
		{
			poll_flag |= First_read; /* set poll_flag prevent come in again */

			/* wake up form warm boot => Reg_ini_volt!=0 */
			if(Reg_minc_temp==0)	/* no imin < imin limit event in blob */  /* Stage (8) */
			{
				Temp_V = Get_voltage(Reg_ini_temp);
				Vmis = Reg_ini_volt;
				Vim = 1000 * (Vmis * 42) / (VMAX*10) ;
				DEBUGPRINT(("Vmi %d, Vmis %d, VMAX %d\n",Vim,Vmis,VMAX));
				for(i=0;i<QTY_TV_TX;i++)
				{
					if( Temp_V < TV_TX_Tab[i].tv )
					{
						Tx = TV_TX_Tab[i].tx;
						break;
					}
				}
				while(1)
				{
					if( Temp_V <= BattIndexTab[icount].batt_v )
					break; /* find it */
					icount++;
				}
				for( i=0;i<Qty_PrCnt;i++)
				{
					if( Vim >= (BattIndexTab[icount].vimval[i]) )
						break; /* find it */
				}
				i = i>6? 6:i; /* if non in table then be the last one */
				Vthx = BattIndexTab[icount].vimval[i];
				battery_index = BattPerCent[i]; /* %(tempo) */
				DEBUGPRINT((" Stage (8-1), Temp_V = %d, Vim = %d, Vthx =%d, battery_index = %d \n",Temp_V,Vim,Vthx,battery_index));
				DAC_T = Get_init_Temp_DAC();
				Temp_V = Get_voltage(DAC_T);
				for(i=0;i<QTY_TV_TX;i++)
				{
					if( Temp_V < TV_TX_Tab[i].tv )
					{
						Tx = TV_TX_Tab[i].tx;
						break;
					}
				}
				while(1)
				{
					if( Temp_V <= BattIndexTab[icount].batt_v )
					break; /* find it */
					icount++;
				}
				for( i=0;i<Qty_PrCnt;i++)
				{
					if( Vim >= (BattIndexTab[icount].vimval[i]) )
					break; /* find it */
				}
				i = i>6? 6:i; /* if non in table then be the last one */
				Vthx = BattIndexTab[icount].vimval[i];
				DVim = DeltavimTab[icount].deltavim[i];
				RCu = ((((((Vim-Vthx)*num_20)/DVim)+battery_index)*num_80)*(num_100-(Tx<num_10?(num_10-Tx):0 ))  );
				if(RCu>num_820000)
					{RCu = num_820000;}
				DEBUGPRINT((" Stage (8-2), Temp_V = %d, Tx = %d, Vthx =%d, DVim = %d ,RCu1 = %d \n",Temp_V,Tx,Vthx,DVim,RCu));
				RCu = RCu + ((Reg_RCL*(num_100-(Tx<num_10?(num_10-Tx):0 )))/num_6);
				if(RCu>num_820000)
					{RCu = num_820000;}
				DEBUGPRINT((" Stage (8-3), RCu2 = %d ",RCu));
			} else {
				/* happened imin < imin limit in blob */  /* Stage (9) */
				Temp_V = Get_voltage(Reg_minc_temp);
				for(i=0;i<QTY_TV_TX;i++)
				{
					if( Temp_V < TV_TX_Tab[i].tv )
					{
						Tx = TV_TX_Tab[i].tx;
						break;
					}
				}
				RCu = ((Reg_RCL*(num_100-(Tx<num_10?(num_10-Tx):0 )))/num_6);
				if(RCu>num_820000)
				{RCu = num_820000;}
				DEBUGPRINT((" Stage (9-1), Temp_V = %d, Tx = %d, RCu = %d ",Temp_V,Tx,RCu));
			}
			
#if BATT_SAM_LEVEL
			BATT_SAM_HIGH();
#else
			BATT_SAM_LOW();
#endif
			battery_index = RCu /(num_90 * (num_100-(Tx<num_10?(num_10-Tx):0 )) );
			DEBUGPRINT((" Stage (8-4),(9-2), Pre-Battery_index = %d \n",battery_index));
			for( i=0;i<Qty_PrCnt;i++)
			{
				if( battery_index >= BattPerCent[i])
				{
					battery_index = BattPerCent[i];
					break;
				}
			}
			discovery_main_battery = GetMainLevel( battery_index );
			DEBUGPRINT((" Stage (8-5),(9-3), Battery_Index = %d \n\n",battery_index));
		}
		
#if AC_IN_LEVEL
		if ((AC_IN_DETECT()==0))
#else
		if (!(AC_IN_DETECT()==0))
#endif
		{
			/* *** Start of discharged routine (Here no AC_IN)*/  /* Stage (2) */
			DACvolt = discovery_read_MBattery();
			Vmxs = DACvolt;
			Vmx = 1000 * (Vmxs * 42) / (VMAX*10) ;
			DEBUGPRINT(("Vmi %d, Vmis %d, VMAX %d\n",Vim,Vmis,VMAX));

			Temp_V = Get_voltage(Temper);
			while(1)
			{
				if( Temp_V <= BattIndexTab[icount].batt_v )
				break; /* find it */
				icount++;
			}
			for( i=0;i<Qty_PrCnt;i++)
			{
				if( Vmx >= (BattIndexTab[icount].vimval[i]) )
				break; /* find it */
			}
			i = i>6? 6:i; /* if non in table then be the last one */
			Vthx = BattIndexTab[icount].vimval[i];
			DEBUGPRINT((" Stage(2-1),icount = %d, i = %d, Vmx = %d, Temp_V = %d, Vthx = %d \n",icount,i,Vmx,Temp_V,Vthx));
			Pvm_k = ( ( num_80 * Vol_S[(driver_waste&Volume_mask)] ) / num_10 );
			DEBUGPRINT((" Stage(2-1.1) Vol :%d SD : %d,IRDA : %d, TOUCH(count): %d,E2MI :%d, Tx_count: %d\n", driver_waste&Volume_mask,driver_waste&SD_waste,driver_waste&IrDA_waste,touch_panel_intr, E2MI, TX_COUNT));
			Pbm_k =(( Bri_S[((driver_waste&Bright_mask)>>3)] )* num_90 )/ num_100;
			if (driver_waste & NFLED_on_waste)
				Pnfm_k = 15;
			else if (driver_waste & NFLED_blink_waste)
				Pnfm_k = 8;
			else
				Pnfm_k = 0;
			Pd_k = (55+((driver_waste&SD_waste)?num_20:0)+((driver_waste&IrDA_waste)?num_10:0)+((driver_waste&Flite_waste)?Pbm_k:0)+((driver_waste&Audio_waste)?Pvm_k:0)) + (touch_panel_intr/10) + (5 * ( BACKPACK_IN_DETECT() ? 0:1 ) ) + ((keyboard_intr)/2) + (TX_COUNT * 5 / 6 ) + Pnfm_k;
			touch_panel_intr = keyboard_intr = 0;
			if(Pd_k >= E2MI)
			{
				i = Vmx+(Pd_k-E2MI)/num_9000;
				Para_L = (Vthx*Vthx*num_100)/(i*i);
				DEBUGPRINT((" Stage(2-2), i = %d , Para_L = %d \n",i,Para_L));
			} else {
				Para_L = 100;
				DEBUGPRINT((" Stage(2-3), Para_L = %d \n",Para_L));
			}
			Pdc = ((( (Pd_k * Para_L)* num_1900) / num_36) / Vmx) - (E2MI * 50 / 36);
			DEBUGPRINT((" Stage(2-4), Pdc=%d, Pd_k=%d, Pvm_k=%d, Pbm_k=%d Pnfm_k=%d\n",Pdc,Pd_k,Pvm_k,Pbm_k,Pnfm_k));
			for(i=0;i<QTY_TV_TX;i++)
			{
				if( Temp_V < TV_TX_Tab[i].tv )
				{
					nowTx = TV_TX_Tab[i].tx;
					break;
				}
			}
			dTx = nowTx-Tx;
			DEBUGPRINT((" Stage(2-5), dTx = %d, nowTx = %d, Tx = %d \n",dTx,nowTx,Tx));
			if (RCu != 0) {
				if( (nowTx>=num_10) || (dTx==0) )
				{
					RCu = RCu - Pdc;
				} else {
					if (dTx > 0)
						RCu = (RCu+((dTx * RCu)/num_100))-Pdc;
					else {
						tmp = (~dTx + 1);
						RCu = RCu - (RCu * tmp )/100 - Pdc;
					}		
					
				}
			}
			Tx = nowTx;
			new_index = RCu /(num_80 * (num_100-(Tx<num_10?(num_10-Tx):0 )) );
			DEBUGPRINT((" Stage(2-6), RCu = %d, new_index = %d \n",RCu,new_index));
			if(new_index < battery_index) {battery_index = new_index;}
			for( i=0;i<Qty_PrCnt;i++)
			{
				if( battery_index >= BattPerCent[i])
				{
					battery_index = BattPerCent[i];
					break;
				}
			}
			discovery_main_battery = GetMainLevel( battery_index );
			DEBUGPRINT((" Stage (2-7), Battery_Index = %d \n\n",battery_index));
			/* ### end of discharged routine */
		} else {
		if(charge_status)
		{
			/* Here AC_IN and Battery Charging */  /* Stage (3) */
			iMin = Imin;
			DACvolt = discovery_read_MBattery();
			Vmxs = DACvolt;
			Vmx = 1000 * (Vmxs * 42) / (VMAX*10) ;
			DEBUGPRINT(("Vmi %d, Vmis %d, VMAX %d\n",Vim,Vmis,VMAX));
			Temp_V = Get_voltage(Temper);
			while(1)
			{
				if( Temp_V <= BattIndexTab[icount].batt_v )
				break; /* find it */
				icount++;
			}
			for( i=0;i<Qty_PrCnt;i++)
			{
				if( Vmx >= (BattIndexTab[icount].vimval[i]) )
				break; /* find it */
			}
			i = i>6? 6:i; /* if non in table then be the last one */
			Vthx = BattIndexTab[icount].vimval[i];
			DEBUGPRINT((" Stage(3-1), iMin = %d, Vmx = %d, Temp_V = %d, Vthx = %d \n",iMin,Vmx,Temp_V,Vthx));
			Pvm_k = ( ( num_80 * Vol_S[(driver_waste&Volume_mask)] ) / num_10 );
			Pbm_k = (( Bri_S[((driver_waste&Bright_mask)>>3)] )* num_90 )/ num_100;
			DEBUGPRINT((" Stage(3-1.1) Vol :%d SD : %d,IRDA : %d, TOUCH(count): %d,E2MI :%d,TX_COUNT :%d\n", driver_waste&Volume_mask,driver_waste&SD_waste,driver_waste&IrDA_waste,touch_panel_intr, E2MI, TX_COUNT));
			if (driver_waste & NFLED_on_waste)
				Pnfm_k = 15;
			else if (driver_waste & NFLED_blink_waste)
				Pnfm_k = 8;
			else
				Pnfm_k = 0;
			Pd_k = (55+((driver_waste&SD_waste)?num_20:0)+((driver_waste&IrDA_waste)?num_10:0)+((driver_waste&Flite_waste)?Pbm_k:0)+((driver_waste&Audio_waste)?Pvm_k:0)) + (touch_panel_intr/10) + (5 * ( BACKPACK_IN_DETECT() ? 0:1 ) ) + ((keyboard_intr)/2) + ((TX_COUNT * 5 )/6) + Pnfm_k;
			touch_panel_intr = keyboard_intr = 0;

			if(Pd_k >= (((700-((driver_waste&BackPack_waste)?num_200:0))*iMin)/num_2293))
			{
				i = Vmx+(Pd_k-(((700-((driver_waste&BackPack_waste)?num_200:0))*iMin)/num_2293))/num_9000;
				Para_L = (Vthx*Vthx*num_100)/(i*i);
				DEBUGPRINT((" Stage(3-2), i = %d , Para_L = %d \n",i,Para_L));
			} else {
				Para_L = 100;
				DEBUGPRINT((" Stage(3-3), Para_L = %d \n",Para_L));
			}
			Pdc =( ((Pd_k*Para_L*38) / Vmx) - ((700-((driver_waste&BackPack_waste)?num_200:0))*iMin / num_2293) )*num_5000 / num_3600;
			DEBUGPRINT((" Stage(3-4), Pdc=%d, Pd_k=%d, Pvm_k=%d, Pbm_k=%d Pnfm_k=%d\n",Pdc,Pd_k,Pvm_k,Pbm_k,Pnfm_k));
			for(i=0;i<QTY_TV_TX;i++)
			{
				if( Temp_V < TV_TX_Tab[i].tv )
				{
					nowTx = TV_TX_Tab[i].tx;
					break;
				}
			}
			dTx = nowTx-Tx;
			DEBUGPRINT((" Stage(3-5), dTx = %d, nowTx = %d, Tx = %d \n",dTx,nowTx,Tx));

			if (RCu != 0) {
				if( (nowTx>=10) || (dTx==0) )
				{
					RCu = RCu - Pdc;
				} else {
					if (dTx > 0)
						RCu = (RCu+((dTx * RCu)/num_100))-Pdc;
					else {
						tmp = (~dTx + 1);
						RCu = RCu - (RCu * tmp )/100 - Pdc;
					}		
					
				}
				if(RCu>num_820000)
				{RCu = num_820000;}
			}
			
			Tx = nowTx;
			battery_index = RCu /(num_80 * (num_100-(Tx<num_10?(num_10-Tx):0 )) );
			DEBUGPRINT((" Stage(3-6), RCu = %d, battery_index = %d \n",RCu,battery_index));
			for( i=0;i<Qty_PrCnt;i++)
			{
				if( battery_index >= BattPerCent[i])
				{
					battery_index = BattPerCent[i];
					break;
				}
			}
			discovery_main_battery = GetMainLevel( battery_index );
			DEBUGPRINT((" Stage (3-7), Battery_Index = %d \n\n",battery_index));
		} else {
			/* Here AC_IN but Battery Full */  /* Stage (4) */
			if( poll_flag & driver_min_curr )
			{
				poll_flag &= ~driver_min_curr;
				RCu = num_8200*(num_100-(Tx<num_10?(num_10-Tx):0 ));
				//if(RCu>num_810000)
				//{RCu = num_810000;}
				DEBUGPRINT((" Stage (4-1), RCu = %d \n",RCu));
			} else {
				DACvolt = discovery_read_MBattery();
				Vmxs = DACvolt;
				Vmx = 1000 * (Vmxs * 42) / (VMAX*10) ;
				DEBUGPRINT(("Vmi %d, Vmis %d, VMAX %d\n",Vim,Vmis,VMAX));
				Temp_V = Get_voltage(Temper);
				while(1)
				{
					if( Temp_V <= BattIndexTab[icount].batt_v )
					break; /* find it */
					icount++;
				}
				for( i=0;i<Qty_PrCnt;i++)
				{
					if( Vmx >= (BattIndexTab[icount].vimval[i]) )
					break; /* find it */
				}
				i = i>6? 6:i; /* if non in table then be the last one */
				Vthx = BattIndexTab[icount].vimval[i];
				DEBUGPRINT((" Stage(4-2), Vmx = %d, Temp_V = %d, Vthx = %d \n",Vmx,Temp_V,Vthx));
				Pvm_k = ( ( num_80 * Vol_S[(driver_waste&Volume_mask)] ) / num_10 );
				Pbm_k = (( Bri_S[((driver_waste&Bright_mask)>>3)] )* num_90 )/ num_100;
				DEBUGPRINT((" Stage(4-1.1) Vol :%d SD : %d,IRDA : %d, TOUCH(count): %d,E2MI :%d\n,TX_COUNT :%d", driver_waste&Volume_mask,driver_waste&SD_waste,driver_waste&IrDA_waste,touch_panel_intr, E2MI,TX_COUNT));
				if (driver_waste & NFLED_on_waste)
					Pnfm_k = 15;
				else if (driver_waste & NFLED_blink_waste)
					Pnfm_k = 8;
				else
					Pnfm_k = 0;
				Pd_k = (55+((driver_waste&SD_waste)?num_20:0)+((driver_waste&IrDA_waste)?num_10:0)+((driver_waste&Flite_waste)?Pbm_k:0)+((driver_waste&Audio_waste)?Pvm_k:0)) + (touch_panel_intr/10) + (5 * ( BACKPACK_IN_DETECT() ? 0:1 ) ) + ((keyboard_intr)/2) + (TX_COUNT * 5 / 6) + Pnfm_k;
				touch_panel_intr = keyboard_intr = 0;
				i = Vmx+Pd_k/num_9000;
				Para_L = (Vthx*Vthx*num_100)/(i*i);
				DEBUGPRINT((" Stage(4-3), i = %d , Para_L = %d \n",i,Para_L));
				Pdc = ((((Pd_k * Para_L)* num_1900) / num_36) / Vmx);
				DEBUGPRINT((" Stage(4-4), Pdc=%d, Pd_k=%d, Pvm_k=%d, Pbm_k=%d Pnfm_k=%d\n",Pdc,Pd_k,Pvm_k,Pbm_k,Pnfm_k));
				for(i=0;i<QTY_TV_TX;i++)
				{
					if( Temp_V < TV_TX_Tab[i].tv )
					{
						nowTx = TV_TX_Tab[i].tx;
						break;
					}
				}
				dTx = nowTx-Tx;
				DEBUGPRINT((" Stage(4-5), dTx = %d, nowTx = %d, Tx = %d \n",dTx,nowTx,Tx));

				if (RCu != 0) {
					if( (nowTx>=num_10) || (dTx==0) )
					{
						RCu = RCu - Pdc;
						DEBUGPRINT((" Stage(4-6), RCu = %d \n",RCu));
					} else {
						if (dTx > 0)
							RCu = (RCu+((dTx * RCu)/num_100))-Pdc;
						else {
							tmp = (~dTx + 1);
							RCu = RCu - (RCu * tmp )/100 - Pdc;
						}		
					
					}
				}
				Tx = nowTx;
			} //end of poll_flag

			new_index = RCu /(num_80 * (num_100-(Tx<num_10?(num_10-Tx):0 )) );
			DEBUGPRINT((" Stage(4-8), new_index = %d \n",new_index));
			DEBUGPRINT((" Stage(4-8.1), battery_index = %d \n",battery_index));
			if(new_index < battery_index) {battery_index = new_index;}
			for( i=0;i<Qty_PrCnt;i++)
			{
				if( battery_index >= BattPerCent[i])
				{
					battery_index = BattPerCent[i];
					break;
				}
			}
			discovery_main_battery = GetMainLevel( battery_index );
			DEBUGPRINT((" Stage (4-9), Battery_Index = %d \n\n",battery_index));
		}
		}

		Reg_RCL = ((RCu + (RCu*(Tx<num_10?(num_10-Tx):0))/num_100)*num_6)/num_100;

#if POLL_MCU //ned, Richard 07/19
		if (!BACKPACK_IN_DETECT() && backinited)
		{
			if (first_poll) {
				first_poll = 0;
				continue;	
			}
			
			ASIC3_GPIO_PIOD_C |=  BACK_I2C_ON ;
			mdelay(10);
			if (!WRITE_I2C_BYTE(MICRON_DEVICE_ADDR,0x07,STUFF_DATA)){
				c_time = READ_I2C_BYTE(MICRON_DEVICE_ADDR);
				if (c_time == 0xff) {
					DEBUGPRINT2(("i2c read error:1\n"));
					goto Error;
				} else {
					DEBUGPRINT2(("dac= %d\n",c_time));
				}	
			} else {
				DEBUGPRINT2(("i2c error:1\n"));
				goto Error;
			}

			schedule();
			if (!WRITE_I2C_BYTE(MICRON_DEVICE_ADDR,0x24,STUFF_DATA)) {
				index_temp = READ_I2C_BYTE(MICRON_DEVICE_ADDR);
				if (index_temp == 0xff) {
					DEBUGPRINT2(("i2c read error:2\n"));
					goto Error;
				} else {
					if( (index_temp & 0x80) == 0x80 )
						second_charge_status = 1;
					else 
						second_charge_status = 0;
					second_battery_index = index_temp & 0x7F ;
					DEBUGPRINT2(("charg= %d\n",second_charge_status));
					DEBUGPRINT2(("index= %d\n",second_battery_index));
				}
			} else {
				DEBUGPRINT2(("i2c error:2\n"));
				goto Error;
			}
			
			if ( second_battery_index >= 40)
				discovery_second_battery = 0;
			else if( second_battery_index >= 20)
				discovery_second_battery = 1;
			else if (second_battery_index >= 5)
				discovery_second_battery = 0x7F;
			else if(second_battery_index>=0)
				discovery_second_battery =2;
			
			DEBUGPRINT2(("status= %d\n",discovery_second_battery));
			
			schedule();
			if (!WRITE_I2C_BYTE(MICRON_DEVICE_ADDR,0x26,STUFF_DATA)){
				E2MI = READ_I2C_BYTE(MICRON_DEVICE_ADDR);
				if (Voltage < 0x5EA) 
					E2MI =0;

				if (E2MI == 0xff) {
					DEBUGPRINT2(("i2c read error:3\n"));
					goto Error;
				} else {
					DEBUGPRINT2(("E2MI= %d\n",E2MI));
				}
			} else {
				E2MI = 0;
				DEBUGPRINT2(("i2c error:3\n"));
				goto Error;
			}

			schedule();
			if (!WRITE_I2C_BYTE(MICRON_DEVICE_ADDR,0x29,STUFF_DATA)){
				TX_COUNT = READ_I2C_BYTE(MICRON_DEVICE_ADDR);
#if AC_IN_LEVEL
				if ( AC_IN_DETECT() ) {
#else
				if ( (AC_IN_DETECT() == 0) ) {
#endif
						TX_COUNT = 0;
				}		
				if (TX_COUNT == 0xff) {
					DEBUGPRINT2(("i2c read error:4\n"));
					goto Error;
				} else {
					DEBUGPRINT2(("TX_COUNT= %d\n",TX_COUNT));
				}
			} else {
				TX_COUNT = 0;
				DEBUGPRINT2(("i2c error:4\n"));
				goto Error;
			}

			if (version == 0) {
				schedule();
       			if (!WRITE_I2C_BYTE(MICRON_DEVICE_ADDR, 0x02, 0xFF)) {
       				version = READ_I2C_BYTE(MICRON_DEVICE_ADDR);
					if (version == 0xff) {
						DEBUGPRINT2(("i2c read error:5\n"));
						goto Error;
					}
       			} else {
       				DEBUGPRINT2(("%s, version I2CError\n", __FUNCTION__));
       				version = 0;            		
					goto Error;
       			}
			}
			
		    if (checksum == 0) {
				schedule();
		       	if (!WRITE_I2C_BYTE(MICRON_DEVICE_ADDR, 0x03, 0xFF)) {
	        			checksum = READ_I2C_BYTE(MICRON_DEVICE_ADDR);
						if (checksum == 0xff) {
							DEBUGPRINT2(("i2c read error:6\n"));
							goto Error;
						}
        		} else {
        				DEBUGPRINT2(("%s, checksum I2CError\n", __FUNCTION__));
 	           			checksum = 0;
						goto Error;
				}
			}			
			
Error:			ASIC3_GPIO_PIOD_C &=  ~BACK_I2C_ON ;

		} else {//BACKPACK_IN_DETECT() not in
			E2MI = 0;					
			backinited = 0;
			TX_COUNT = 0;
		}
#endif //ned, End Richard 0719
	} //end of while(1)

} //end of discovery_battery_main_thread()


unsigned short Get_init_Temp_DAC(void)
{
	short	i,j,DAC_T=0;
				while(1)
				{
					i=DAC_T;
					DAC_T = discovery_read_Temper();
					j=((i-DAC_T)>0)?(i-DAC_T):(DAC_T-i);
					if( j < num_x49 )
					{
						DAC_T = (i+DAC_T)/2;
						break;
					}
				}
				return DAC_T;
}



/* for Pd parameters */
void clr_driver_waste_bits(unsigned short x)
{
	driver_waste &= ~x;
}

void set_driver_waste_bits(unsigned short x)
{
	driver_waste |= x;
}

EXPORT_SYMBOL(clr_driver_waste_bits);
EXPORT_SYMBOL(set_driver_waste_bits);
EXPORT_SYMBOL(driver_waste);

/* detect ACIN edge and setting the rising/falling trigger */
void detect_ACIN_edge_and_set(void)
{
#if AC_IN_LEVEL
	if(AC_IN_DETECT())
#else
	if( AC_IN_DETECT() == 0)
#endif
	{
		DEBUGPRINT((" AC_IN is In now \n"));

#if AC_IN_LEVEL
		ASIC3_GPIO_ETSEL_D &= ~AC_IN; /* rising -> falling */
#else
		ASIC3_GPIO_ETSEL_D |= AC_IN; /* falling -> rising */
#endif
	}
	else
	{
		DEBUGPRINT((" AC_IN is not In \n"));

#if AC_IN_LEVEL
		ASIC3_GPIO_ETSEL_D |= AC_IN; /* falling -> rising */
#else
		ASIC3_GPIO_ETSEL_D &= ~AC_IN; /* rising -> falling */
#endif
		BATT_CHARGE_OFF();
		CHARGE_LED_OFF();
	}
}

static void Collie_ac_interrupt(int irq, void *dummy, struct pt_regs *fp)
{
	DEBUGPRINT((KERN_EMERG " Into Collie_ac_interrupt \n"));

	detect_ACIN_edge_and_set();
	ac_interrupt = 1;
	
	wake_up_interruptible((wait_queue_head_t*)&queue);
}

static void bp_power_on()
{
	/* rising edge trigger */
	ASIC3_GPIO_ETSEL_D |= BACKPACK_DETECT;
	/* turn on backpack power & i2c bus */
	ASIC3_GPIO_PIOD_C |= ( BACK_PWR_ON );
	ASIC3_GPIO_PIOD_C &= ~BACK_RESET ; 
	mdelay(100);
	ASIC3_GPIO_PIOD_C |= BACK_RESET ; 
	mdelay(50);
	set_driver_waste_bits(BackPack_waste);
	
}

static void bp_power_off()
{
	/* falling edge trigger */
	ASIC3_GPIO_ETSEL_D &= ~BACKPACK_DETECT;
	/* turn off backpack power & i2c bus */
	ASIC3_GPIO_PIOD_B |= BUFFER_OE;
	backpackptr = ioremap(0x14000000, 4);    // Richard 0710  Output '0' to physical add. 0x14000000 to avoid .. 
	if (NULL != backpackptr) {
		*((u16*)backpackptr) = 0;		     // Richard 0710  
		iounmap(backpackptr);
	}
	ASIC3_GPIO_PIOD_B &= ~BUFFER_OE;
	ASIC3_GPIO_PIOD_C &= ~BACK_RESET;
	ASIC3_GPIO_PIOD_C &= ~( BACK_PWR_ON );
	clr_driver_waste_bits(BackPack_waste);

	version = 0;
       	checksum = 0;
	vcc_on = 0;
}

static int backpack_detect_check(void *unused)
{
	daemonize();
	strcpy(current->comm, "kbpchkd");
	sigfillset(&current->blocked);
	while(1){
		interruptible_sleep_on(&bp_detect);
		if (need_delay)
			mdelay(500);

		while(1){
		
			if ( BACKPACK_IN_DETECT() && (bp_pull_out == 1) ) {	// backpack is really out
				// power off sequence
				DEBUGPRINT(("Turn off BackPack power \n"));
				bp_power_off();
				backinited = 0;
				need_delay = 1;
				first_poll = 1;
				break;
			}
			if ( !BACKPACK_IN_DETECT() && (bp_pull_out == 0) ) {	// backpack is really in
				//power on sequence
				DEBUGPRINT(("Turn on BackPack power \n"));
				bp_power_on();
				backinited = 1;
				first_poll = 1;
				break;
			}
			if ( !BACKPACK_IN_DETECT() && (bp_pull_out == 1) ) {	// backpack is in/out/in
				// prevent from abnormal case
				bp_pull_out = 0;
				backinited = 0;
				continue;
			}			
		}	
	}	
}

static void discovery_backpack_irq(int irq,void *dev_id,struct pt_regs *regs)
{
	if (BACKPACK_IN_DETECT()) {	// backpack is detected out
		bp_pull_out = 1;
		wake_up(&bp_detect);
	} else	// backpack is detected in
	{
		bp_pull_out = 0;
		wake_up(&bp_detect);
	}
//	set_cf_irq();

}

void BackPack_Process(void)
{
	if (BACKPACK_IN_DETECT()) /* HIGH :BACKPACK NOT IN */
	{
		/* falling edge trigger */
		ASIC3_GPIO_ETSEL_D &= ~BACKPACK_DETECT;
		/* turn off backpack power & i2c bus */
		ASIC3_GPIO_PIOD_B |= BUFFER_OE;
		backpackptr = ioremap(0x14000000, 4);    // Richard 0710  Output '0' to physical add. 0x14000000 to avoid .. 
		if (NULL != backpackptr) {
			*((u16*)backpackptr) = 0;		     // Richard 0710  
    			iounmap(backpackptr);
    		}
		ASIC3_GPIO_PIOD_B &= ~BUFFER_OE;
		ASIC3_GPIO_PIOD_C &= ~BACK_RESET;
		ASIC3_GPIO_PIOD_C &= ~( BACK_PWR_ON );
		clr_driver_waste_bits(BackPack_waste);
	}
	else	/* LOW : BACKPACK IN */
	{
		ASIC3_GPIO_ETSEL_D |= BACKPACK_DETECT;
		/* turn on backpack power & i2c bus */
		ASIC3_GPIO_PIOD_C |= ( BACK_PWR_ON );
		ASIC3_GPIO_PIOD_C &= ~BACK_RESET; 
		mdelay(100);
		ASIC3_GPIO_PIOD_C |= BACK_RESET;		
		mdelay(50);
		set_driver_waste_bits(BackPack_waste);

		backinited = 1;
	}
//	set_cf_irq();
}

int discovery_get_main_battery(void)
{
#if AC_IN_LEVEL	/* 1:has AC_IN=High, 0:has AC_IN=Low */
	if( (!(poll_flag&First_read))||(AC_IN_DETECT()) )	/* if not yet to get the battery index reply unknow status */
#else
	if( (!(poll_flag&First_read))||(!AC_IN_DETECT()) )	/* if not yet to get the battery index reply unknow status */
#endif
	{
		return APM_BATTERY_STATUS_UNKNOWN;
	}
	else
	{
		if( battery_index > discovery_main_battery_thresh_fl.high )
			return APM_BATTERY_STATUS_UNKNOWN;
		else if( battery_index > discovery_main_battery_thresh_fl.low )
			return APM_BATTERY_STATUS_UNKNOWN;
		else if( battery_index > discovery_main_battery_thresh_fl.verylow )
			return APM_LOW_BATTERY;
		else if( battery_index == 0 )
			return APM_BATTERY_FAULT;
	}
	return APM_BATTERY_STATUS_UNKNOWN;
}
EXPORT_SYMBOL(discovery_get_main_battery);

void discovery_bat_i2c_init(u8 byDeviceAddress)
{
		/* enable I2C clock */
		CKEN |= 0x4100;//GPIO_P14;
   		I2C_ICR |= I2C_ICR_UR;  // reset
   		I2C_ISR = 0;    // clear all ISR
   		I2C_ICR &= ~I2C_ICR_UR; // clear reset
		I2C_ICR &= ~I2C_ICR_FM;  // slow mode (0=100K,1=400K)
   		I2C_ICR &= ~I2C_ICR_SADIE;
   		I2C_ICR &= ~I2C_ICR_ALDIE;
   		I2C_ICR &= ~I2C_ICR_SSDIE;
   		I2C_ICR &= ~I2C_ICR_BEIE;
   		I2C_ICR &= ~I2C_ICR_IRFIE;
   		I2C_ICR &= ~I2C_ICR_ITEIE;
   		I2C_ISAR = (char)byDeviceAddress;//MICRON_DEVICE_ADDR;
    		
   		while(I2C_ISR & I2C_ISR_IBB);    // wait bus busy
    		
}
EXPORT_SYMBOL(discovery_bat_i2c_init);

int GetMainLevel( unsigned int battery_index )
{
	if( !(poll_flag & First_read) ) {
		return APM_BATTERY_STATUS_UNKNOWN;
	} else {
		if( battery_index > discovery_main_battery_thresh_fl.high )
			return APM_BATTERY_STATUS_HIGH;
		else if( battery_index > discovery_main_battery_thresh_fl.low )
			return APM_BATTERY_STATUS_LOW;
		else if( battery_index > discovery_main_battery_thresh_fl.verylow )
			return APM_BATTERY_STATUS_VERY_LOW;
		else if( battery_index == 0 )
			return APM_BATTERY_STATUS_CRITICAL;
		else
			return APM_BATTERY_STATUS_UNKNOWN;
	}

}

/*
 *	Get Battery voltage DAC value
 */
int discovery_read_MBattery(void)
{
	return Get_DAC_Value(BATT_CHL);
}

/*
 *	Get IMIN voltage DAC value
 */
int discovery_read_IMIN(void)
{
	/* select the IMIN output in MUX */
	ASIC3_GPIO_PIOD_C &= ~A_IN_SEL0;
	ASIC3_GPIO_PIOD_C |= A_IN_SEL1;
#ifdef MUX_EN_LEVEL
	ASIC3_GPIO_PIOD_C |= MUX_EN;
#else
	ASIC3_GPIO_PIOD_C &= ~MUX_EN;
#endif	
	mdelay(1);
	return Get_DAC_Value(MUX_CHL);
}


/* 
 * Get Temperature DAC data
 */
int discovery_read_Temper(void)
{
	/* select the temperature output in MUX */
	ASIC3_GPIO_PIOD_C &= ~A_IN_SEL0;
	ASIC3_GPIO_PIOD_C &= ~A_IN_SEL1;
#ifdef MUX_EN_LEVEL
	ASIC3_GPIO_PIOD_C |= MUX_EN;
#else
	ASIC3_GPIO_PIOD_C &= ~MUX_EN;
#endif
	mdelay(1);
	return Get_DAC_Value(MUX_CHL);
}


/* 
 * Translate Analog signal to Digital data
 */
int	Get_DAC_Value(int channel)
{
	unsigned long cmd;
	unsigned int dummy;
	int voltage;

	/* translate ADC */
	cmd = (1u << ADSCTRL_PD0_SH) | (1u << ADSCTRL_PD1_SH) |(1u << ADSCTRL_DFR_SH)|
		(channel << ADSCTRL_ADR_SH) | (1u << ADSCTRL_STS_SH);
	dummy = ads7846_rw_cli(cmd);
	udelay(1);
	voltage = ads7846_rw_cli(cmd);

	cmd &= ~(ADSCTRL_PD0_MSK | ADSCTRL_PD1_MSK);
	dummy = ads7846_rw_cli(cmd);
	return voltage;
}


#ifdef CONFIG_PM
static int Collie_battery_pm_callback(struct pm_dev *pm_dev, pm_request_t req, void *data)
{
	switch (req)
	{
		case PM_SUSPEND:
			/* Disable I2C clock */
			CKEN &= ~GPIO_P14;

#ifdef MUX_EN_LEVEL
			ASIC3_GPIO_PIOD_C &= ~MUX_EN; /* for power comsumption */
#else
			ASIC3_GPIO_PIOD_C |= MUX_EN; /* for power comsumption */
#endif

#ifdef FIX_STSCHG_WAKEUP
			if (BACKPACK_IN_DETECT()) { // backpack not exist	
				disable_irq(IRQ_ASIC3_CF_STSCHG);
				disable_irq(IRQ_ASIC3_BACKPART_DET);
			}
#else
			disable_irq(IRQ_ASIC3_BACKPART_DET);
#endif

			/* turn off backpack power & i2c bus */
			ASIC3_GPIO_PIOD_C &= ~BACK_RESET;
			ASIC3_GPIO_PIOD_C &= ~( BACK_PWR_ON + BACK_I2C_ON );

			poll_flag &= ~First_read;

			/* initial blob's parameters */
			Reg_RCL = ((RCu + (RCu*(Tx<num_10?(num_10-Tx):0))/num_100)*num_6)/num_100;
			DEBUGPRINT((" (Tx<num_10?(num_10-Tx):0) = %d \n",(Tx<num_10?(num_10-Tx):0)));
			DEBUGPRINT((" (RCu*(Tx<num_10?(num_10-Tx):0))/num_100 = %d  \n",(RCu*(Tx<num_10?(num_10-Tx):0))/num_100));
			DEBUGPRINT((" ((RCu + (RCu*(Tx<num_10?(num_10-Tx):0))/num_100)*num_6) = %d \n",((RCu + (RCu*(Tx<num_10?(num_10-Tx):0))/num_100)*num_6)));
			DEBUGPRINT((" RCu = %d, Tx = %d, Reg_RCL = %d \n\n",RCu,Tx,Reg_RCL));
			/* set first_poll_flag to ensure into blob to get the 1st value */
			Reg_blob_ctrl &= ~quick_off_on;
			Reg_blob_ctrl |= flag_first_poll;
			Reg_ini_volt=0;
			Reg_ini_temp=0;
			Reg_minc_temp=0;
			Reg_1Read_DAC=0;		/* for read 1st DAC value from POWER OFF */

			if (IQTR_stage == R_stage){
				ASIC3_SLEEP_PARAM2 |= 0x0002; //R stage.
			} else { 
				ASIC3_SLEEP_PARAM2 &= 0xfffc; //Q stage.
			}

			ASIC3_SLEEP_PARAM1 = 0x00;
				
#if AC_IN_LEVEL	/* 1:has AC_IN=High, 0:has AC_IN=Low */
			AC_IN_DETECT()?(Reg_blob_ctrl |= flag_part_wake) : (Reg_blob_ctrl &= ~flag_part_wake);
#else
			AC_IN_DETECT()?(Reg_blob_ctrl &= ~flag_part_wake) : (Reg_blob_ctrl |= flag_part_wake);
#endif
   			version = 0;
           	checksum = 0;
       		backinited = 0;
			first_poll = 1;			 
			break;

		case PM_RESUME:

			discovery_bat_i2c_init(MICRON_DEVICE_ADDR);		

			/* for AC_IN */
			ASIC3_GPIO_MASK_D |= AC_IN;   /* enable interrupt */
			ASIC3_GPIO_DIR_D  &= ~AC_IN; /* input */
			ASIC3_GPIO_INTYP_D |= AC_IN; /* trigger : edge */

			/* for MUX */
			ASIC3_GPIO_DIR_C |= (MUX_EN+A_IN_SEL0+A_IN_SEL1); /* output */

			/*** for BACKPACK DETECT PIN ***/
			ASIC3_GPIO_MASK_D &= ~BACKPACK_DETECT;
			ASIC3_GPIO_DIR_D &= ~BACKPACK_DETECT;
			ASIC3_GPIO_INTYP_D |= BACKPACK_DETECT; 

			/*** for BACKPACK PWR ON & I2C BUS ENABLE PIN ***/
			ASIC3_GPIO_DIR_C |= ( BACK_PWR_ON + BACK_I2C_ON );

			/* DETECT THE STATUS OF PACKPACK PIN */
			need_delay = 0;
			BackPack_Process();
			need_delay = 1;

			enable_irq(IRQ_ASIC3_AC_IN);
			enable_irq(IRQ_ASIC3_BACKPART_DET);
#ifdef FIX_STSCHG_WAKEUP
			enable_irq(IRQ_ASIC3_CF_STSCHG);
#endif
			/* initial parameters */
			if (ASIC3_SLEEP_PARAM2 & 0x02) {
				IQTR_stage = R_stage; //R stage.
				discovery_charge_off();
				if (second_bat_charge_status())
					CHARGE_LED_ON();
				else
					CHARGE_LED_OFF();
			} else { 
				IQTR_stage = Q_stage; //Q stage.
			}

			poll_flag &= ~First_read; /* set poll_flag prevent come in again */
			/* charge status flag reset */
			charge_status = 0;
			/* dis-arm the condition */
			Charge_time = 0;
			imin_count = 0;
			temp_count = 0;
			poll_flag &= ~BattBad_flag;
			
			Collie_ac_interrupt(0, NULL, NULL);

			break;
	}
	return 0;
}
#endif

#ifdef CONFIG_PROC_FS
struct proc_dir_entry *proc_batt;
struct proc_dir_entry *proc_bp;

typedef struct collie_battery_entry 
{
	int*		addr;
	int		def_value;
	char*		name;
	char*		description;
	unsigned short	low_ino;
} collie_battery_entry_t;

static collie_battery_entry_t collie_battery_params[] = 
{
	/*  { addr,	def_value,	name,	    description }*/
	{ &msglevel,	0,		"msglevel",    "debug message output level" }
};
#define NUM_OF_BATTERY_ENTRY	(sizeof(collie_battery_params)/sizeof(collie_battery_entry_t))

typedef struct command_entry {
	u8 cmdnum;
	char* name;
	char* description;
	unsigned short low_ino;
} command_t;

static command_t command[] = 
{
	{0x02,"UPVER","micro-P version"},
	{0x03,"UPCHKSUM","micro-P checksum"},
	{0x01,"CF3V","CF 3V ON/OFF"}
};
#define NUM_OF_COMMAND_ENTRY	(sizeof(command)/sizeof(command_t))

static ssize_t collie_battery_read_params(struct file *file, char *buf,
				      size_t nbytes, loff_t *ppos)
{
	int i_ino = (file->f_dentry->d_inode)->i_ino;
	char outputbuf[15];
	int count;
	int i;
	collie_battery_entry_t	*current_param = NULL;

	if (*ppos>0) /* Assume reading completed in previous read*/
		return 0;
	for (i=0; i<NUM_OF_BATTERY_ENTRY; i++)
	{
		if (collie_battery_params[i].low_ino==i_ino)
		{
			current_param = &collie_battery_params[i];
			break;
		}
	}
	if (current_param==NULL)
	{
		return -EINVAL;
	}
	count = sprintf(outputbuf, "0x%08X\n",
	*((volatile unsigned int *) current_param->addr));

	*ppos += count;
	if (count>nbytes)	/* Assume output can be read at one time */
		return -EINVAL;
	if (copy_to_user(buf, outputbuf, count))
		return -EFAULT;
	return count;
}

static ssize_t collie_battery_write_params(struct file *file, const char *buf,
				       size_t nbytes, loff_t *ppos)
{
	int			i_ino = (file->f_dentry->d_inode)->i_ino;
	collie_battery_entry_t	*current_param = NULL;
	int			i;
	unsigned long		param;
	char			*endp;

	for (i=0; i<NUM_OF_BATTERY_ENTRY; i++) 
	{
		if(collie_battery_params[i].low_ino==i_ino)
		{
			current_param = &collie_battery_params[i];
			break;
		}
	}
	if (current_param==NULL)
	{
		return -EINVAL;
	}

	param = simple_strtoul(buf,&endp,0);
	if (param == -1)
	{
		*current_param->addr = current_param->def_value;
	}
	else
	{
		*current_param->addr = param;
	}
	return nbytes+endp-buf;
}

static struct file_operations proc_params_operations = {
	read:	collie_battery_read_params,
	write:	collie_battery_write_params,
};

static int proc_bp_read(struct file * file, char * buf,
		size_t nbytes, loff_t *ppos)
{
	int i_ino = (file->f_dentry->d_inode)->i_ino;
	char outputbuf[5];
	command_t* current_command = NULL;
	int count=0;
	int i;

	if (*ppos>0) /* Assume reading completed in previous read*/
		return 0;

	for (i = 0;i < NUM_OF_COMMAND_ENTRY;i++) {
		if (command[i].low_ino == i_ino) {
			current_command = &command[i];
			break;
		}
	}
	if (current_command == NULL)
		return -EINVAL;

	switch (i) {
	case 0: // Backpack Version
		count = sprintf(outputbuf, "%d\n", version);
        	break;
	case 1: // uP Checksum
		count = sprintf(outputbuf, "%d\n", checksum);
		break;
	}

	*ppos+=count;
	if (count>nbytes)  /* Assume output can be read at one time */
		return -EINVAL;
	if (copy_to_user(buf, outputbuf, count))
		return -EFAULT;
	return count;
}

static int proc_bp_write(struct file * file, const char * buffer,
		size_t count, loff_t *ppos)
{

	return count;
}

static struct file_operations proc_bp_operations = {
	read:	proc_bp_read,
	write:	proc_bp_write
};
#endif

int __init Collie_battery_init(void)
{
	int err;

	DEBUGPRINT(("Collie_battery_init: \n"));

	CKEN |= CKEN3_SSP;
	SSCR0 = 0x1ab;
	SSCR1 = 0;

	ac_interrupt=0;
	Init_Voltage = discovery_read_MBattery();
	mdelay(2);
	Init_Voltage = discovery_read_MBattery();

	if (RCSR&RCSR_HWR)
		HWR_flag = 1;
	else {
		HWR_flag = 0;
		Reg_blob_ctrl |= quick_off_on;
	}
		
	first_poll = 1;			 

	discovery_bat_i2c_init(MICRON_DEVICE_ADDR);
	spin_lock_init(&my_lock);

	/* for AC_IN */
	ASIC3_GPIO_MASK_D |= AC_IN;   /* enable interrupt */
	ASIC3_GPIO_DIR_D  &= ~AC_IN; /* input */
	ASIC3_GPIO_INTYP_D |= AC_IN; /* trigger : edge */
	detect_ACIN_edge_and_set();

	/* for MUX */
	ASIC3_GPIO_DIR_C |= (MUX_EN+A_IN_SEL0+A_IN_SEL1); /* output */


	/*** for BACKPACK DETECT PIN ***/
	ASIC3_GPIO_MASK_D &= ~BACKPACK_DETECT;
	/* input port */
	ASIC3_GPIO_DIR_D &= ~BACKPACK_DETECT;
	/* edge trigger */
	ASIC3_GPIO_INTYP_D |= BACKPACK_DETECT;

	/*** for BACKPACK PWR ON & I2C BUS ENABLE PIN ***/
	ASIC3_GPIO_DIR_C |= ( BACK_PWR_ON + BACK_I2C_ON + BACK_RESET);
	ASIC3_GPIO_PIOD_C &= ~ BACK_PWR_ON ;
	ASIC3_GPIO_PIOD_C &= ~ BACK_I2C_ON ;
	ASIC3_GPIO_PIOD_C &= ~BACK_RESET;
	
	GPCR0 = GPIO_DISCOVERY_CF_RESET;
	
	ASIC3_GPIO_DIR_B |= BUFFER_OE;
	ASIC3_GPIO_PIOD_B |= BUFFER_OE;
	backpackptr = ioremap(0x14000000, 4);    // Richard 0710  Output '0' to physical add. 0x14000000 to avoid .. 
	if (NULL != backpackptr) {
		*((u16*)backpackptr) = 0;		     // Richard 0710  
    		iounmap(backpackptr);
    	}
	ASIC3_GPIO_PIOD_B &= ~BUFFER_OE;

#if BATT_SAM_LEVEL
	BATT_SAM_HIGH(); 
#else
	BATT_SAM_LOW();
#endif

	/* Register interrupt handler. */
	if ((err = request_irq(IRQ_ASIC3_AC_IN, Collie_ac_interrupt, SA_INTERRUPT,
			 "ACIN", Collie_ac_interrupt))) 
	{
		DEBUGPRINT(("Could not get irq %d.\n", IRQ_ASIC1_AC_IN));
		return 1;
	}

	if ((err = request_irq(IRQ_ASIC3_BACKPART_DET, discovery_backpack_irq, SA_INTERRUPT,
			 "BackPackIn", discovery_backpack_irq))) 
	{
		DEBUGPRINT(("Could not get irq %d.\n", IRQ_ASIC3_BACKPART_DET));
		return 1;
	}

	/* regist to Misc driver */
	misc_register(&battery_device);

	kernel_thread(discovery_battery_main_thread, NULL, CLONE_FS | CLONE_FILES | CLONE_SIGHAND | SIGCHLD);
	kernel_thread(discovery_minute,  NULL, CLONE_FS | CLONE_FILES | CLONE_SIGHAND | SIGCHLD);
  
  	kernel_thread(backpack_detect_check,  NULL, CLONE_FS | CLONE_FILES | CLONE_SIGHAND | SIGCHLD);//Deborah 0712

	/* DETECT THE STATUS OF PACKPACK PIN */
	mdelay(1);
	need_delay = 0;
	BackPack_Process();
	need_delay = 1;
#ifdef CONFIG_PM
	battery_pm_dev = pm_register(PM_COTULLA_DEV, 0, Collie_battery_pm_callback);
#endif

#ifdef CONFIG_PROC_FS
	{
		int	i;
		struct proc_dir_entry *entry;

		proc_batt = proc_mkdir("driver/battery", NULL);
		if (proc_batt == NULL) 
		{
			free_irq(IRQ_ASIC3_AC_IN, Collie_ac_interrupt);

			misc_deregister(&battery_device);
			DEBUGPRINT((KERN_ERR "can't create /proc/driver/battery\n"));
			return -ENOMEM;
		}
		for (i=0; i<NUM_OF_BATTERY_ENTRY; i++)
		{
			entry = create_proc_entry(collie_battery_params[i].name,
					  S_IWUSR |S_IRUSR | S_IRGRP | S_IROTH,
					  proc_batt);
			if (entry)
			{
				collie_battery_params[i].low_ino = entry->low_ino;
				entry->proc_fops = &proc_params_operations;
			}
			else
			{
				int	j;
				for (j=0; j<i; j++)
				{
					remove_proc_entry(collie_battery_params[i].name, proc_batt);
				}
				remove_proc_entry("driver/battery", &proc_root);
				proc_batt = 0;
				free_irq(IRQ_ASIC3_AC_IN, Collie_ac_interrupt);
//				free_irq(IRQ_ASIC3_BACKPART_DET, discovery_backpack_irq);
				misc_deregister(&battery_device);
				DEBUGPRINT((KERN_ERR "ts: can't create /proc/driver/ts/threshold\n"));
				return -ENOMEM;
			}
		}
		
		proc_bp = proc_mkdir("bp", NULL);

		for(i=0;i<NUM_OF_COMMAND_ENTRY;i++) {
			entry = create_proc_entry(command[i].name, S_IWUSR |S_IRUSR | S_IRGRP | S_IROTH, proc_bp);
			if(entry) {
				command[i].low_ino = entry->low_ino;
				entry->proc_fops = &proc_bp_operations;
			} else {
				return(-ENOMEM);
			}
		}
	}
#endif

	return 0;
}

module_init(Collie_battery_init);


#ifdef MODULE
int init_module(void)
{
	Collie_battery_init();
	return 0;
}

void cleanup_module(void)
{
	free_irq(IRQ_ASIC3_AC_IN, Collie_ac_interrupt);
	free_irq(IRQ_ASIC3_BACKPART_DET, discovery_backpack_irq);
#ifdef CONFIG_PROC_FS
	{
	int	i;
	for (i=0; i<NUM_OF_COLLIE_BATTERY_ENTRY; i++) {
		remove_proc_entry(collie_battery_params[i].name, proc_batt);
	}
	remove_proc_entry("driver/battery", NULL);
	proc_batt = 0;
      
	for(i=0;i<NUM_OF_COMMAND_ENTRY;i++) {
		remove_proc_entry(command[i].name, proc_bp);
	}
	remove_proc_entry("bp", NULL);
	proc_bp = 0;
	}
#endif

	misc_deregister(&battery_device);
}
#endif /* MODULE */

EXPORT_SYMBOL(READ_I2C_BYTE);
EXPORT_SYMBOL(WRITE_I2C_BYTE);
