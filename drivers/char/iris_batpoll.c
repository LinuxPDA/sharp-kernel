/*
 *  drivers/char/iris_batpoll.c
 *
 *  Driver for Battery Status Polling on the SHARP Iris-1 board
 *
 *
 */
/* -------------------------------------------------------
 *  includes
 * ------------------------------------------------------- */

#ifndef __KERNEL__
#define __KERNEL__
#endif

#include <linux/config.h>
#include <linux/module.h>
#include <linux/errno.h>
#include <linux/signal.h>
#include <linux/sched.h>
#include <linux/timer.h>
#include <linux/interrupt.h>
#include <linux/tty.h>
#include <linux/tty_flip.h>
#include <linux/serial.h>
#include <linux/serial_reg.h>
#include <linux/major.h>
#include <linux/string.h>
#include <linux/fcntl.h>
#include <linux/ptrace.h>
#include <linux/ioport.h>
#include <linux/mm.h>
#include <linux/malloc.h>
#include <linux/init.h>
#include <linux/delay.h>
#include <linux/console.h>
#include <linux/hdreg.h>

#include <asm/system.h>
#include <asm/io.h>
#include <asm/irq.h>
#include <asm/uaccess.h>
#include <asm/bitops.h>
#include <asm/arch/hardware.h>
#include <linux/poll.h>

#include <asm/arch/gpio.h>
#include <asm/arch/fpga.h>

#include <linux/pm.h>

#include <asm/arch/sspads_iris.h>

/* -------------------------------------------------------
 *  setups
 * ------------------------------------------------------- */

#define DEBUGPRINT(s)   /* printk s */ /* ignore */

/* manual version 7 implies changes of ADC table */
/* manual version 8 implies GSM SYNC control */

#define IRIS_HARDWARE_MANUAL_VERSION 8


/* -------------------------------------------------------
 *  linux setup
 * ------------------------------------------------------- */

/* linux/major.h stuff */
#define IRISBAT_MAJOR  63

typedef struct irisbat_status {
  int voltage;
  int ad_value;
  int is_fatal;
  int back_voltage;
  int back_ad_value;
  int back_is_fatal;
  int adjust_value;
} irisbat_status;

typedef struct iristemp_status {
  int temp;
  int ad_value;
} iristemp_status;

/* IOCTL code */
#define IRISBAT_IOCTL_CODE_BASE 0x56a0
#define IRISBAT_FATALSIGN    (IRISBAT_IOCTL_CODE_BASE+1)
#define IRISBAT_SIGNOFF      (IRISBAT_IOCTL_CODE_BASE+2)
#define IRISBAT_GETSTATUS    (IRISBAT_IOCTL_CODE_BASE+3)
#define IRISTEMP_GETSTATUS   (IRISBAT_IOCTL_CODE_BASE+4)

/* -------------------------------------------------------
 *  ADS bits
 * ------------------------------------------------------- */

#define ADS_START_BIT       0x80
#define ADS_BATTERY_MEASURE ( 2 << 4 )
#define ADS_TEMP1_MEASURE   ( 0 << 4 )
#define ADS_TEMP2_MEASURE   ( 7 << 4 )
#define ADS_AUX_MEASURE     ( 6 << 4 )
#define ADS_TABLET_12BIT    0x00
#define ADS_TABLET_8BIT     0x08
#define ADS_SER             0x04
#define ADS_DFR             0x00
#define ADS_POWERUP         0x03
#define ADS_IRQ_EN          0x00

/* -------------------------------------------------------
 *  system interrupts , etc
 * ------------------------------------------------------- */

extern void restore_fullfill_irq_for_batcheck(void);
extern void disable_fullfill_irq_for_batcheck(void);

/* -------------------------------------------------------
 *  Conversion Table
 * ------------------------------------------------------- */

typedef struct t_batt_ad {
  short lower_voltage;
  short lower_ad;
} t_batt_ad;

typedef struct t_temp_ad {
  short lower_temp;
  short lower_ad;
} t_temp_ad;

static  t_batt_ad battery_ad_table[]={
  { 550, 0x8cc },
  { 540, 0x8a3 },
  { 530, 0x87a },
  { 520, 0x851 },
  { 510, 0x828 },  
  { 500, 0x7ff },
  { 490, 0x7d6 },
  { 480, 0x7ad },
  { 470, 0x784 },
  { 460, 0x75b },
  { 450, 0x732 },
  { 440, 0x709 },
  { 430, 0x6e0 },
  { 420, 0x6b7 },
  { 410, 0x68e },
  { 400, 0x666 },
  { 390, 0x63d },
  { 380, 0x614 },
  { 370, 0x5eb },
  { 360, 0x5c2 },
  { 350, 0x599 },
  { 340, 0x570 },
  { 330, 0x547 },
  { 320, 0x51e },
  { 310, 0x4f5 },
  { 300, 0x4cc },
  { 290, 0x4a3 },
  { 280, 0x47a },
  { 270, 0x451 },
  { 260, 0x428 },
  { 250, 0x3ff },
  { 240, 0x3d6 },
  { 230, 0x3ad },
  { 220, 0x384 },
  { 210, 0x35b },
  { 000, 0x000 }   /* must be terminated by 0 */
};



#if IRIS_HARDWARE_MANUAL_VERSION < 7

static t_temp_ad temp_ad_table[]={
  {  520, 0xfe5 },
  {  400, 0xdd6 },
  {  350, 0xcd1 },
  {  300, 0xbb8 },
  {  250, 0xa8e },
  {  200, 0x95a },
  {  150, 0x824 },
  {  100, 0x6f4 },
  {   50, 0x5d3 },
  {    0, 0x4c8 },
  {  -50, 0x3d9 },
  { -100, 0x309 },
  { -150, 0x258 },
  { -200, 0x1c7 },
  { -250, 0x153 },
  { -300, 0x0f8 },
  { -350, 0x0b1 },
  { -400, 0x07d },
  {  000, 0x000 }   /* must be terminated by 0 */
};

#else /* IRIS_HARDWARE_MANUAL_VERSION >= 7 */

static t_temp_ad temp_ad_table[]={
  {  400, 0x3c0 },
  {  350, 0x452 },
  {  300, 0x4f8 },
  {  250, 0x5b4 },
  {  200, 0x686 },
  {  150, 0x76f },
  {  100, 0x8c6 },
  {   50, 0x97c },
  {    0, 0xa99 },
  {  -50, 0xbc0 },
  { -100, 0xce9 },
  { -150, 0xe0b },
  { -200, 0xf20 },
  {  000, 0x000 }   /* must be terminated by 0 */
};


#endif /*  IRIS_HARDWARE_MANUAL_VERSION < 7 */


#define DEFAULT_FATAL_VOLTAGE 370

/* -------------------------------------------------------
 *  low-level driver
 * ------------------------------------------------------- */

#define MEASURE_BAT_BITS1 ( ADS_START_BIT | ADS_BATTERY_MEASURE | ADS_TABLET_12BIT | ADS_SER | ADS_POWERUP )
#define MEASURE_BAT_BITS2 ( ADS_START_BIT | ADS_BATTERY_MEASURE | ADS_TABLET_12BIT | ADS_SER | ADS_POWERUP )
#define MEASURE_TEMP_BITS1 ( ADS_START_BIT | ADS_AUX_MEASURE | ADS_TABLET_12BIT | ADS_SER | ADS_POWERUP )
#define MEASURE_TEMP_BITS2 ( ADS_START_BIT | ADS_AUX_MEASURE | ADS_TABLET_12BIT | ADS_SER | ADS_POWERUP )

#if IRIS_HARDWARE_MANUAL_VERSION >= 8
#define GSM_SYNC_PIN   bitPC0
#endif


#ifndef NEW_SSP_CODE

static int read_battery(void)
{
  int penirq_save;
  int adc_save;
  int retval = 0;

  static int last_retval = 0;

  DEBUGPRINT(("read_battery:\n"));

  if( ! iris_get_access() ){
    return(last_retval);
  }

  disable_fullfill_irq_for_batcheck();

  DEBUGPRINT(("read_battery: got access\n"));

  if( ( penirq_save = iris_is_ssp_penint_enabled() ) ){
    DEBUGPRINT(("read_bat: penirq is already enabled. Disabling...\n"));
    iris_disable_ssp_penint();
    DEBUGPRINT(("read_bat: penirq disabled\n"));
  }else{
    DEBUGPRINT(("read_bat: penirq already disabled\n"));
  }
  if( ( adc_save = iris_is_ssp_adc_enabled() ) ){
    DEBUGPRINT(("read_bat: adc is already enabled.\n"));
  }else{
    DEBUGPRINT(("read_bat: adc is disabled. Enabling...\n"));
    iris_ADCEnable();
    udelay(1);
    DEBUGPRINT(("read_bat: adc is enabled\n"));
  }

  //DEBUGPRINT(("SSP WRITE(1) %x\n",MEASURE_BAT_BITS1));

  iris_SspWrite(MEASURE_BAT_BITS1);
  iris_SspWrite(0);
  iris_SspWrite(0);
  
  //DEBUGPRINT(("SSP READ(1)\n"));

  iris_SspRead();
  iris_SspRead();
  iris_SspRead();

  udelay(50);

  //DEBUGPRINT(("SSP WRITE(2) %x\n",MEASURE_BAT_BITS2));

  iris_SspWrite(MEASURE_BAT_BITS2);
  iris_SspWrite(0);
  iris_SspWrite(0);

  //DEBUGPRINT(("SSP READ(2)\n"));

  iris_SspRead(); /* ignore */
  retval = (unsigned short)iris_SspRead();
  retval <<= 5;
  retval |= ((unsigned short)iris_SspRead()) >> 3;

  udelay(50);

  iris_reset_adc();

  udelay(50);

  if( ! adc_save ){
    DEBUGPRINT(("read_bat: adc restoring...\n"));
    iris_ADCDisable();
    DEBUGPRINT(("read_bat: adc restored\n"));
  }
  if( penirq_save ){
    DEBUGPRINT(("read_bat: penirq restoring...\n"));
    iris_enable_ssp_penint();
    DEBUGPRINT(("read_bat: penirq restored\n"));
  }

  DEBUGPRINT(("read_battery: releasing access...\n"));

  restore_fullfill_irq_for_batcheck();

  iris_release_access();

  DEBUGPRINT(("read_battery: done\n"));

  last_retval = retval;

  return(retval);
}

#else /* NEW_SSP_CODE */


static int read_battery(void)
{
  int retval = 0;

  unsigned char read_bytes[3];
  unsigned char bat1_cmd[3] = { MEASURE_BAT_BITS1 , 0 , 0 };
  unsigned char bat2_cmd[3] = { MEASURE_BAT_BITS2 , 0 , 0 };

  disable_fullfill_irq_for_batcheck();

  begin_adc_disable_irq();
  enable_adc_con();

  exchange_three_bytes(bat1_cmd,read_bytes);

  udelay(50);

  exchange_three_bytes(bat2_cmd,read_bytes);

  retval = (unsigned short)read_bytes[1];
  retval <<= 5;
  retval |= ((unsigned short)read_bytes[2]) >> 3;

  udelay(50);

  send_adcon_reset_cmd();

  disable_adc_con();
  end_adc_enable_irq();

  udelay(50);

  restore_fullfill_irq_for_batcheck();

  return(retval);
}

#endif /* NEW_SSP_CODE */

static signed int voltage_adjust_param = 0;

#define IGNORE_THRESHOLD_BTM  -3365  /* 0xfffff2db */
#define IGNORE_THRESHOLD_TOP  3750 /* 0xea6 */
static void voltage_table_init(void)
{
  //DEBUGPRINT("voltage_table_init\n");
#define BAT_CALIB_PARAM_POS FLASH2_BASE+0x7f0018
  voltage_adjust_param = *((signed int*)(BAT_CALIB_PARAM_POS));
#if 0
  {
    int i = 0;
    int j = 0;
    unsigned char* p = (unsigned char*)(FLASH2_BASE+0x7f0000);
    printk("======= Dumping From %p ===========\n",p);
    for(i=0;i<0x100;i+=0x10){
      printk("%p :",p);
      for(j=0;j<0x10;j++){
	printk("%x ",*p);
	p++;
      }
      printk("\n");
    }
  }
#endif
  //DEBUGPRINT("voltage_adjust param = %d %x\n",voltage_adjust_param,voltage_adjust_param);
  if( voltage_adjust_param < IGNORE_THRESHOLD_BTM 
      || voltage_adjust_param > IGNORE_THRESHOLD_TOP ){
    //DEBUGPRINT("it seems strange value. ignore...\n");
    voltage_adjust_param = 0;
  }
#if 0
  {
    int i = 0;
    int j = 0;
    unsigned char* p = (unsigned char*)(FLASH2_BASE+0x7f0000);
    printk("======= Dumping From %p ===========\n",p);
    for(i=0;i<0x100;i+=0x10){
      printk("%p :",p);
      for(j=0;j<0x10;j++){
	printk("%x ",*p);
	p++;
      }
      printk("\n");
    }
  }
#endif
}
static void temp_table_init(void)
{
  //DEBUGPRINT("temp_table_init\n");
}

static int calc_voltage(int adval)
{
  int diff_voltage;
  int diff_ad;
  int width_voltage;
  int width_ad;
  t_batt_ad* pT = battery_ad_table;
  short upper_voltage;
  short upper_ad;

  static int table_initialized = 0;

  if( ! table_initialized ){
    voltage_table_init();
    table_initialized = 1;
  }

  //DEBUGPRINT("checking adval for V-table %x\n",adval);


  if( adval > pT->lower_ad ) return(pT->lower_voltage);

  upper_voltage = pT->lower_voltage;
  upper_ad = pT->lower_ad;

  for( ; pT->lower_voltage > 0 ; pT++ ){
    if( pT->lower_ad <= adval ){
      
      //DEBUGPRINT("checking for entry (%x,%d)\n",pT->lower_ad,pT->lower_voltage);

      diff_ad = adval - pT->lower_ad;
      width_voltage = upper_voltage - pT->lower_voltage;
      width_ad = upper_ad - pT->lower_ad;
      diff_voltage = width_voltage * diff_ad / width_ad;
      if( voltage_adjust_param ){
	int v = pT->lower_voltage + diff_voltage;
	//DEBUGPRINT("matches. diff_voltage = %d , v = %d\n",diff_voltage,v);
	//DEBUGPRINT("adjust = %d , v' = %d\n",voltage_adjust_param,v + ( v * voltage_adjust_param ) / 0x10000 );
	return( v + ( v * voltage_adjust_param ) / 0x10000 );
      }else{
	//DEBUGPRINT("returns diff_voltage = %d , v = %d\n",diff_voltage,pT->lower_voltage + diff_voltage );
	return( pT->lower_voltage + diff_voltage );
      }
    }
    upper_voltage = pT->lower_voltage;
    upper_ad = pT->lower_ad;
  }

  DEBUGPRINT("no match ??\n");

  return(0);
}

#ifndef NEW_SSP_CODE

static int read_temperature(void)
{
  int penirq_save;
  int adc_save;
  int temp_save;
  /* int temp_save; */
  int retval = 0;

  static int last_retval = 0;

  DEBUGPRINT(("read_temp:\n"));

  if( ! iris_get_access() ){
    return(last_retval);
  }

  DEBUGPRINT(("read_temp: got access\n"));

  if( ( penirq_save = iris_is_ssp_penint_enabled() ) ){
    DEBUGPRINT(("read_temp: penirq is already enabled. Disabling...\n"));
    iris_disable_ssp_penint();
    udelay(50);
    DEBUGPRINT(("read_temp: penirq disabled\n"));
  }else{
    DEBUGPRINT(("read_temp: penirq already disabled\n"));
  }
  if( ( adc_save = iris_is_ssp_adc_enabled() ) ){
    DEBUGPRINT(("read_temp: adc is already enabled.\n"));
  }else{
    DEBUGPRINT(("read_temp: adc is disabled. Enabling...\n"));
    iris_ADCEnable();
    udelay(50);
    DEBUGPRINT(("read_temp: adc is enabled\n"));
  }
#if 1
  if( ( temp_save = iris_is_ssp_tempadc_enabled() ) ){
    DEBUGPRINT(("read_temp: tempadc is already enabled.\n"));
  }else{
    DEBUGPRINT(("read_temp: tempadc is disabled. Enabling...\n"));
    iris_TempADCEnable();
    udelay(50);
    DEBUGPRINT(("read_temp: tempadc is enabled\n"));
  }
#endif

  //DEBUGPRINT(("SSP WRITE(1) %x\n",MEASURE_BAT_BITS1));

  iris_SspWrite(MEASURE_TEMP_BITS1);
  iris_SspWrite(0);
  iris_SspWrite(0);

  //DEBUGPRINT(("SSP READ(1)\n"));

  iris_SspRead();
  iris_SspRead();
  iris_SspRead();

  udelay(50);

  //DEBUGPRINT(("SSP WRITE(2) %x\n",MEASURE_BAT_BITS2));

  iris_SspWrite(MEASURE_TEMP_BITS2);
  iris_SspWrite(0);
  iris_SspWrite(0);

  //DEBUGPRINT(("SSP READ(2)\n"));

  iris_SspRead(); /* ignore */
  retval = (unsigned short)iris_SspRead();
  retval <<= 5;
  retval |= ((unsigned short)iris_SspRead()) >> 3;

  udelay(50);

  iris_reset_adc();

  udelay(50);

#if 1
  if( ! temp_save ){
    DEBUGPRINT(("read_temp: temp restoring...\n"));
    iris_TempADCDisable();
    udelay(50);
    DEBUGPRINT(("read_temp: temp restored\n"));
  }
#endif
  if( ! adc_save ){
    DEBUGPRINT(("read_temp: adc restoring...\n"));
    iris_ADCDisable();
    udelay(50);
    DEBUGPRINT(("read_temp: adc restored\n"));
  }
  if( penirq_save ){
    DEBUGPRINT(("read_temp: penirq restoring...\n"));
    iris_enable_ssp_penint();
    udelay(50);
    DEBUGPRINT(("read_temp: penirq restored\n"));
  }

  DEBUGPRINT(("read_temp: releasing access...\n"));

  iris_release_access();

  DEBUGPRINT(("read_temp: done\n"));

  last_retval = retval;

  return(retval);
}

#else /* NEW_SSP_CODE */


static int read_temperature(void)
{
  int retval = 0;

  unsigned char read_bytes[3];
  unsigned char temp1_cmd[3] = { MEASURE_TEMP_BITS1 , 0 , 0 };
  unsigned char temp2_cmd[3] = { MEASURE_TEMP_BITS2 , 0 , 0 };

  begin_adc_disable_irq();
  enable_adc_con();
  enable_temp_adc();

  exchange_three_bytes(temp1_cmd,read_bytes);

  udelay(50);

  exchange_three_bytes(temp2_cmd,read_bytes);

  retval = (unsigned short)read_bytes[1];
  retval <<= 5;
  retval |= ((unsigned short)read_bytes[2]) >> 3;

  udelay(50);

  send_adcon_reset_cmd();

  disable_temp_adc();
  disable_adc_con();
  end_adc_enable_irq();

  return(retval);
}

#endif /* NEW_SSP_CODE */


static int calc_temperature(int adval)
{
  int diff_temp;
  int diff_ad;
  int width_temp;
  int width_ad;
  t_temp_ad* pT = temp_ad_table;
  short upper_temp;
  short upper_ad;

#if IRIS_HARDWARE_MANUAL_VERSION < 7

  if( adval > pT->lower_ad ) return(pT->lower_temp);

  upper_temp = pT->lower_temp;
  upper_ad = pT->lower_ad;

  for( ; pT->lower_ad > 0 ; pT++ ){
    if( pT->lower_ad <= adval ){
      diff_ad = adval - pT->lower_ad;
      width_temp = upper_temp - pT->lower_temp;
      width_ad = upper_ad - pT->lower_ad;
      diff_temp = width_temp * diff_ad / width_ad;
      return( pT->lower_temp + diff_temp );
    }
    upper_temp = pT->lower_temp;
    upper_ad = pT->lower_ad;
  }

#else /* IRIS_HARDWARE_MANUAL_VERSION >= 7 */

  static int table_initialized = 0;

  if( ! table_initialized ){
    temp_table_init();
    table_initialized = 1;
  }


  if( adval < pT->lower_ad ) return(pT->lower_temp);

  upper_temp = pT->lower_temp;
  upper_ad = pT->lower_ad;

  for( ; pT->lower_ad > 0 ; pT++ ){
    if( pT->lower_ad > adval ){
      diff_ad = pT->lower_ad - adval;
      width_temp = upper_temp - pT->lower_temp;
      width_ad = pT->lower_ad - upper_ad;
      diff_temp = width_temp * diff_ad / width_ad;
      return( pT->lower_temp + diff_temp );
    }
    upper_temp = pT->lower_temp;
    upper_ad = pT->lower_ad;
  }

#endif /* IRIS_HARDWARE_MANUAL_VERSION >= 7 */

  return(0);
}

#define READ_BATTERY_MAIN    0
#define READ_BATTERY_BACKUP  1

#define GPO_MAIN_BAT      ( 1 << 0 )
#define GPO_BACKUP_BAT    ( 1 << 1 )

#define GSM_SYNC0_WAIT_MAX  10

#ifndef NEW_SSP_CODE

static int read_battery_mV(int which)
{
  int readval;
  int voltage;

#if IRIS_HARDWARE_MANUAL_VERSION >= 8
  int re_read_loop = 0;
#endif

  int gpo;

  gpo = IO_FPGA_GPO;
  switch(which){
  case READ_BATTERY_BACKUP:
    gpo &= ~GPO_MAIN_BAT;
    gpo |= GPO_BACKUP_BAT;
    break;
  case READ_BATTERY_MAIN:
  default:
    gpo |= GPO_MAIN_BAT;
    gpo &= ~GPO_BACKUP_BAT;
    break;
  }
  IO_FPGA_GPO = gpo;

  mdelay(100); //mdelay(10);

#if IRIS_HARDWARE_MANUAL_VERSION >= 8
 gsm_sync_wait:
  {
    int syncwait = 0;
    for(syncwait=0;syncwait<GSM_SYNC0_WAIT_MAX;syncwait++){
      if( GET_PCDR(bitPC0) == 0 ) break;
    }
  }
#endif

  iris_init_ssp();
  iris_TempADCEnable();

  if( ( readval = read_battery() ) < 0 ){
    iris_release_ssp();
    return -EINVAL;
  }

#if IRIS_HARDWARE_MANUAL_VERSION >= 8
  {
    if( GET_PCDR(bitPC0) != 0 ){
      if( ++re_read_loop < GSM_SYNC0_WAIT_MAX ){
	goto gsm_sync_wait;
      }
    }
    //printk("re_read_loop %d\n",re_read_loop);
  }
#endif

  voltage = calc_voltage(readval);

  iris_release_ssp();

  gpo &= ~(GPO_MAIN_BAT|GPO_BACKUP_BAT);

  IO_FPGA_GPO = gpo;

  return voltage * 10;
}


#else /* NEW_SSP_CODE */

static int read_battery_mV(int which)
{
  int readval;
  int voltage;

#if IRIS_HARDWARE_MANUAL_VERSION >= 8
  int re_read_loop = 0;
#endif

  int gpo;

  gpo = IO_FPGA_GPO;
  switch(which){
  case READ_BATTERY_BACKUP:
    gpo &= ~GPO_MAIN_BAT;
    gpo |= GPO_BACKUP_BAT;
    break;
  case READ_BATTERY_MAIN:
  default:
    gpo |= GPO_MAIN_BAT;
    gpo &= ~GPO_BACKUP_BAT;
    break;
  }
  IO_FPGA_GPO = gpo;

  mdelay(10);

#if IRIS_HARDWARE_MANUAL_VERSION >= 8
 gsm_sync_wait:
  {
    int syncwait = 0;
    for(syncwait=0;syncwait<GSM_SYNC0_WAIT_MAX;syncwait++){
      if( GET_PCDR(bitPC0) == 0 ) break;
    }
  }
#endif

  iris_init_ssp();

  if( ( readval = read_battery() ) < 0 ){
    iris_release_ssp();
    return -EINVAL;
  }

#if IRIS_HARDWARE_MANUAL_VERSION >= 8
  {
    if( GET_PCDR(bitPC0) != 0 ){
      if( ++re_read_loop < GSM_SYNC0_WAIT_MAX ){
	goto gsm_sync_wait;
      }
    }
    //printk("re_read_loop %d\n",re_read_loop);
  }
#endif

  voltage = calc_voltage(readval);

  iris_release_ssp();

  gpo &= ~(GPO_MAIN_BAT|GPO_BACKUP_BAT);

  return voltage * 10;
}


#endif /* NEW_SSP_CODE */

int iris_read_battery_mV(void)
{
  return read_battery_mV(READ_BATTERY_MAIN);
}

int iris_read_backupbattery_mV(void)
{
  return read_battery_mV(READ_BATTERY_BACKUP);
}

int iris_read_battery_temp(void)
{
  int readval;
  int error;
  int temp;

  iris_init_ssp();
  if( ( readval = read_temperature() ) < 0 ){
    iris_release_ssp();
    return 0;
  }
  temp = calc_temperature(readval);
  iris_release_ssp();
  return temp;
}

EXPORT_SYMBOL(iris_read_battery_mV);
EXPORT_SYMBOL(iris_read_backupbattery_mV);
EXPORT_SYMBOL(iris_read_battery_temp);

/* -------------------------------------------------------
 *  linux char drivers
 * ------------------------------------------------------- */

#ifdef CONFIG_PM
static struct pm_dev *iris_bat_pm_dev;

static int iris_bat_pm_callback(struct pm_dev *pm_dev,
				pm_request_t req, void *data)
{
  static int suspended = 0;
  unsigned long save_bat_gpo = 0;
  switch (req) {
  case PM_STANDBY:
  case PM_BLANK:
  case PM_SUSPEND:
    if( suspended ) break;
    save_bat_gpo = IO_FPGA_GPO & (GPO_MAIN_BAT|GPO_BACKUP_BAT);
    IO_FPGA_GPO &= ~(GPO_MAIN_BAT|GPO_BACKUP_BAT);
    suspended = 1;
    break;
  case PM_UNBLANK:
  case PM_RESUME:
    if( ! suspended ) break;
    IO_FPGA_GPO |= save_bat_gpo;
    suspended = 0;
    break;
  }
  return 0;
}
#endif


static int fatal_voltage;

int irisbat_open(struct inode *inode, struct file *filp)
{
  DEBUGPRINT(("irisbat_open:\n"));
  iris_init_ssp();

#ifndef NEW_SSP_CODE
  iris_TempADCEnable();
#endif /* ! NEW_SSP_CODE */

  return 0;          /* success */
}

int irisbat_release(struct inode *inode, struct file *filp)
{
  DEBUGPRINT(("irisbat_release:\n"));
  iris_release_ssp();
  return 0;
}

static ssize_t irisbat_read(struct file *file, char *buffer, size_t count, loff_t *ppos)
{
  int readval;
  int voltage;
  DEBUGPRINT(("irisbat_read:\n"));
  if( ( readval = read_battery() ) < 0 ){
    DEBUGPRINT(("irisbat_read : read_battery error\n"));
    return -EINVAL;
  }
  voltage = calc_voltage(readval);
  put_user(voltage,(int*)buffer);
  return count;
}

#if 0
static unsigned int irisbat_poll(struct file* filep, poll_table* wait)
{
  return 0;
}

static int irisbat_fasync(int fd, struct file* filep, int mode)
{
  return 0;
}
#endif

static int irisbat_ioctl(struct inode *inode,struct file *file,
			 unsigned int cmd,unsigned long arg)
{
  switch( cmd ) {
  case IRISBAT_GETSTATUS:
    {
      int readval;
      int error;
      int voltage;
      int is_fatal;
      irisbat_status* p_cuser = (irisbat_status*)arg;
      if( ( readval = read_battery() ) < 0 ){
	return -EINVAL;
      }
      voltage = read_battery_mV(READ_BATTERY_MAIN) / 10;
      //voltage = calc_voltage(readval);
      is_fatal = ( fatal_voltage >= voltage ? 1 : 0 );
      error = put_user(readval,&p_cuser->ad_value);
      if( error ) return error;
      error = put_user(voltage,&p_cuser->voltage);
      if( error ) return error;
      error = put_user(is_fatal,&p_cuser->is_fatal);
      if( error ) return error;
      /* read backup bat. */
      voltage = read_battery_mV(READ_BATTERY_BACKUP) / 10;
      is_fatal = ( fatal_voltage >= voltage ? 1 : 0 );
      error = put_user(voltage,&p_cuser->back_voltage);
      if( error ) return error;
      error = put_user(is_fatal,&p_cuser->back_is_fatal);
      if( error ) return error;
      error = put_user(voltage_adjust_param,&p_cuser->adjust_value);
      if( error ) return error;
    }
    break;
  case IRISTEMP_GETSTATUS:
    {
      int readval;
      int error;
      int temp;
      iristemp_status* p_cuser = (iristemp_status*)arg;
      if( ( readval = read_temperature() ) < 0 ){
	return -EINVAL;
      }
      temp = calc_temperature(readval);
      error = put_user(readval,&p_cuser->ad_value);
      if( error ) return error;
      error = put_user(temp,&p_cuser->temp);
      if( error ) return error;
    }
    break;
  default:
    return -EINVAL;
  }
  return 0;
}

/*
 * The file operations
 */

struct file_operations irisbat_fops = {
  read:    irisbat_read,
  ioctl:   irisbat_ioctl,
  open:    irisbat_open,
  release: irisbat_release,
};

int __init irisbat_init(void)
{ 
  DEBUGPRINT(("irisbat init:\n"));
  if ( register_chrdev(IRISBAT_MAJOR,"irisbat",&irisbat_fops) )
    printk("unable to get major %d for iris-bat\n",IRISBAT_MAJOR);

  iris_init_ssp_sys();

  fatal_voltage = DEFAULT_FATAL_VOLTAGE;

#ifdef CONFIG_PM
  iris_bat_pm_dev = pm_register(PM_SYS_DEV, 0, iris_bat_pm_callback);
#endif

#ifndef NEW_SSP_CODE
  iris_TempADCEnable();
#endif /* ! NEW_SSP_CODE */

  printk("irisbat registered\n");
  return 0;
}

__initcall(irisbat_init);
/* =======================================================
 *  end of source
 * ======================================================= */

