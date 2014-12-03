/*
 * linux/drivers/char/iris_ts.c
 *
 * Based on:
 *   ts_sa1110.c -- Driver for touch screen
 *   Author: Bin Zhang copy from Xuejun's ts-7110.c
 *   Copyright (c) 1999 ISDCorp
 *
 * ChangeLog:
 *   03-05-2001	 Lineo Japan, Inc. ...
 *   04-02-2001	                   New code
 */

#include <linux/config.h>
#include <linux/init.h>
#include <linux/poll.h>

#include <linux/pm.h>

#include <asm/segment.h>
#include <asm/uaccess.h>
#include <asm/arch/irqs.h>
#include <asm/irq.h>
#include <asm/delay.h>
#include <asm/hardware.h>

#include <asm/arch/sib.h>
#include <asm/arch/fpga.h>

#include <asm/arch/touchscr_iris.h>
#include <asm/arch/sspads_iris.h>

#include <asm/sharp_ts.h>

#define DPRINTK(x, args...)  /* printk(__FUNCTION__ ": " x,##args) */


#ifndef NEW_SSP_CODE
//---------------------------------------------------

#ifdef CONFIG_PM
static struct pm_dev *iris_ts_pm_dev;
#endif

#if 0 /* moved to sharp_ts.h */
/* ioctl command	*/
#define	TSCTL_CALIBRATE	0x5680
#endif /* moved to sharp_ts.h */

/* MINOR number 1 for mone lcd , 
   min_x[0] for mono, min_x[1] for color
*/

//---------------------------------------------------
// SYS_CLOCK_ENABLE
#define TMS_EN_BIT	0x00000004	// 3M6 enable bit

#define	TABLET_CS_BIT	0x08
#define THERM_STB_BIT	0x10

#define WSRC_PENIRQ_BIT	((ushort)bitWSRC6)

#define ADINT_TIMING	178

// SSP Master1
#define SSCR0	0x80041000
#define SSCR1	0x80041004
#define SSDR	0x80041008
//#define SSSR	0x8004100C

#define IO_SSCR0	__IOL(SSCR0)
#define IO_SSCR1	__IOL(SSCR1)
#define IO_SSDR	__IOL(SSDR)
#define IO_SSSR	__IOL(SSSR)

	// SSCR0 (16bits)
		// Data Size Select
#define DSS_MASK	0x000F
#define DSS_16		0x000F
#define DSS_15		0x000E
#define DSS_14		0x000D
#define DSS_13		0x000C
#define DSS_12		0x000B
#define DSS_11		0x000A
#define DSS_10		0x0009
#define DSS_9		0x0008
#define DSS_8		0x0007
#define DSS_7		0x0006
#define DSS_6		0x0005
#define DSS_5		0x0004
#define DSS_4		0x0003

		// Frame Format
#define FRF_MASK	0x0030
#define FRF_MOT		0x0000
#define FRF_TISS	0x0001
#define FRF_NSM		0x0010

		// SYCLK Polarity
#define SPO_MASK	0x0040
#define SPO_NL_PH	0x0000
#define SPO_NH_PL	0x0040

		// SYCLK Phase
#define SPH_MASK	0x0080
#define SPH_TRAIL	0x0000
#define SPH_LEAD	0x0080

		// Serial Clock Rate
#define SCR_MASK	0xFF00
//#define SCR			0x1200
#define SCR			0x0000

#define SSCR0_ENA	(DSS_8|FRF_MOT|SPO_NL_PH|SPH_TRAIL|SCR)

	// SSCR1 (16bits)
#define RIM	0x0001	// Receive FIFO Interupt Mask
#define TIM	0x0002	// Transimit FIFO Interupt Mask
#define LBM	0x0004	// Loop Back Mode
#define SSE 0x0008	// SSP Enable

#define SSCR1_ENA	(SSE)

	// SSSR (16bits)
#define TFE	0x0001	// Transmit FIFO is empty			(R)
#define TNF	0x0002	// Transmit FIFO is not full		(R)
#define RNE	0x0004	// Recieve FIFO is not empty		(R)
#define BSY 0x0008	// Busy								(R)
#define TFS	0x0010	// Transmit FIFO Service Request	(R)
#define RFS	0x0020	// Recieve FIFO Service Request		(R)
#define ROR	0x0040	// Receive FIFO Overrun				(R/W)
#define RFF	0x0080	// Recieve FIFO is full				(R)

	// SSDR (16bits)
#define SSDR_PUP_X16		0x00D3
#define SSDR_PUP_Y16		0x0093
#define SSDR_PUP_Z1_16		0x00B3
#define SSDR_PUP_Z2_16		0x00C3
#define SSDR_READ_X16		0x00D0
#define SSDR_READ_Y16		0x0090
#define SSDR_READ_Z1_16		0x00B0
#define SSDR_READ_Z2_16		0x00C0

#define TS_STATUS_NODATA	0
#define TS_STATUS_FREE	1
#define TS_STATUS_DOWN	2
//#define TS_STATUS_UP		3


static unsigned long down_pressure = 10000;
static unsigned long release_pressure = 10000;
static unsigned long init_pressure = 10000;

#if 0
#define INIT_PRESSURE           9000
#define RELEASE_PRESSURE        9000
#else
#define INIT_PRESSURE           (init_pressure)
#define RELEASE_PRESSURE        (release_pressure)
#endif

static void load_default_values_from_rom(void)
{
  down_pressure = (*(unsigned long*)(FLASH2_BASE + 0xd0));
  release_pressure = (*(unsigned long*)(FLASH2_BASE + 0xd4));
  init_pressure = ( down_pressure > release_pressure ? down_pressure : release_pressure );
  DPRINTK("LOAD ROM TS VALUES : DOWN: %d UP: %d --> INIT: %d\n",
	 down_pressure,release_pressure,init_pressure);
}

//---------------------------------------------------

typedef struct pt_regs	PT_REGS;

//---------------------------------------------------

static int	read_ts( int *x, int *y, int *p);
//static int	SspWrite(unsigned char data);
#define SspWrite(data) iris_SspWrite((data))
//static unsigned char	SspRead(void);
#define SspRead()  iris_SspRead()
//static void	ADCEnable(void);
#define ADCEnable() iris_ADCEnable()
//static void	ADCDisable(void);
#define ADCDisable() iris_ADCDisable()
static int	read_sample( int *x , int *y);
static void	FormatReadBuf(tsEvent *buf, int x, int y, int pressure);

/*	Tablet sampling data queue operations	*/
extern int	set_sample_to_queue(tsEvent *sample);

/*	SSP operations	*/
//static void	init_ssp(void);
#define init_ssp() iris_init_ssp()
//static void	release_ssp(void);
#define release_ssp() iris_release_ssp()
/* !!!!!! MOVED TO iris_ssp.c !!!!!! */
//static void	enable_ssp_int(void);
/* !!!!!! MOVED TO iris_ssp.c !!!!!! */
//static void	disable_sspint(void);
/* !!!!!! this is not currentry used !!!!!!! */
//static void	sspint_handler(int irq, void *dev_id, PT_REGS *regs);

/*	Pen IRQ operations	*/
static int	init_penirq(void);
static void	release_penirq(void);
static void	enable_penirq(void);
inline void	disable_penirq(void);
static void	penirq_handler(int irq, void *dev_id, PT_REGS *regs);
static void	tablet_sample_timer(ulong c);

/*	AD Start Interrupt operations	*/
static int	init_adstart_int(void);
static void	release_adstart_int(void);
inline void	enable_adstart_int(void);
inline void	disable_adstart_int(void);
/* static void	adstart_int_handler(int irq, void *dev_id, PT_REGS *reg); */
/* static void	wait_adstart_int(void); */

//---------------------------------------------------

int	ts_status;
static int	adstart_int_flg = 0;
static struct timer_list timer_func_list;

//---------------------------------------------------

//---------------------------------------------------

#ifdef CONFIG_PM

static int penirq_initialized = 0;


static int iris_ts_pm_callback(struct pm_dev *pm_dev,
			       pm_request_t req, void *data)
{
  static int penirq_save = 0;
  static int suspended = 0;
  static int timer_deleted_force = 0;
	switch (req) {
	case PM_STANDBY:
	case PM_BLANK:
	case PM_SUSPEND:
	  if( suspended ) break;
	  //printk("ts_suspend:");
	  if( timer_pending(&timer_func_list) ){
	    timer_deleted_force = 1;
	  }else{
	    timer_deleted_force = 0;
	  }
	  del_timer(&timer_func_list);
	  if( ( penirq_save = iris_is_ssp_penint_enabled() ) ){
	    //printk("disabling penint\n");
	    iris_disable_ssp_penint();
	  }
	  //printk("ts_suspend: done");
	  suspended = 1;
		break;
	case PM_UNBLANK:
	case PM_RESUME:
	  if( ! suspended ) break;
	  //printk("ts_resume:");
	  if( penirq_save ){
	    if( IO_FPGA_RawStat & WSRC_PENIRQ_BIT ){
	      //printk("enabling timer 3\n");
	      timer_func_list.expires = jiffies+HZ*2;
	      add_timer(&timer_func_list);
	    }else{
	      //printk("enabling penint\n");
	      iris_enable_ssp_penint();
	    }
	  }else{
	    if( IO_FPGA_RawStat & WSRC_PENIRQ_BIT ){
	      //printk("enabling timer 2\n");
	      timer_func_list.expires = jiffies+HZ*2;
	      add_timer(&timer_func_list);
	    }else if( penirq_initialized ){
	      //printk("enabling interrupt 2\n");
	      iris_enable_ssp_penint();
	    }
	  }
#if 0
	  if( timer_deleted_force ){
	    printk("enabling timer\n");
	    timer_func_list.expires = jiffies+(HZ/100);
	    add_timer(&timer_func_list);
	  }
#endif
	  printk("ts_resume: done");
	  suspended = 0;
                break;
	}
	return 0;
}
#endif

int __init iris_ts_arch_init(void)
{
        init_timer(&timer_func_list);
        timer_func_list.expires = 0;
        timer_func_list.data = 0;
        timer_func_list.function = tablet_sample_timer;

	/* !!!!!! MOVED TO iris_ssp.c !!!!!! */
	//IO_SYS_CLOCK_ENABLE |= TMS_EN_BIT;

	/* set CS(PE3) output	*/
	/* !!!!!! MOVED TO iris_ssp.c !!!!!! */
	//IO_PEDDR |= (TABLET_CS_BIT|THERM_STB_BIT);

	//ADCDisable();
	
	/* !!!!! This function works as the same function !!!!!! */
	iris_init_ssp_sys();

#ifdef CONFIG_PM
	iris_ts_pm_dev = pm_register(PM_SYS_DEV, 0, iris_ts_pm_callback);
#endif

	load_default_values_from_rom();

	return 0;
}
module_init(iris_ts_arch_init);

static int read_sample( int *x , int *y)
{
	ushort	SampleX, SampleY, SampleZ1, SampleZ2;
	int	pressure;
#if 1
	ulong	flags;
#endif
	/* !!!!!! for shared SSP !!!!!! */
	int irq_enabled_save;

	static ushort last_SampleX = 0,last_SampleY = 0;
	static int last_pressure = 0;


	/* !!!!!! get access for shared SSP !!!!!! */

	if( ! iris_get_access() ){
	  *x = (int)last_SampleX;
	  *y = (int)last_SampleY;
	  pressure = last_pressure;
	  return pressure;
	}

	if( ( irq_enabled_save = iris_is_ssp_penint_enabled() ) ){
	  //printk("read_sample : irq is already enabled...\n");
	  iris_disable_ssp_penint();
	  //printk("read_sample : disabled irq\n");
	}else{
	  //printk("read_sample : not enabled irq\n");
	}

	// X-Position

	SspWrite(SSDR_PUP_X16);
	SspWrite(0);
	SspWrite(0);
	SspRead();
	SspRead();
	SspRead();

	udelay(50);

#if 1
	save_flags(flags);
	cli();
	IO_FPGA_ADICLR = 0;	/* AD start interrupt clear	*/
	while (0 == (IO_IRQRAWSTATUS&0x20000000))	;
#endif

	SspWrite(SSDR_READ_X16);
#if 1
	restore_flags(flags);
#endif
	SspWrite(0);
	SspWrite(0);
	SspRead();
	SampleX = (unsigned short)SspRead() & 0x7f;
	SampleX <<= 5;
	SampleX += ((unsigned short)SspRead() & 0xf8) >> 3;

	// Y-Position

	SspWrite(SSDR_PUP_Y16);
	SspWrite(0);
	SspWrite(0);
	SspRead();
	SspRead();
	SspRead();

	udelay(50);

#if 1
	save_flags(flags);
	cli();
	IO_FPGA_ADICLR = 0;	/* AD start interrupt clear	*/
	while (0 == (IO_IRQRAWSTATUS&0x20000000))	;
#endif

	SspWrite(SSDR_READ_Y16);
#if 1
	restore_flags(flags);
#endif
	SspWrite(0);
	SspWrite(0);
	SspRead();
	SampleY = (unsigned short)SspRead() & 0x7f;
	SampleY <<= 5;
	SampleY += ((unsigned short)SspRead() & 0xf8) >> 3;

	// Z1-Position

	SspWrite(SSDR_PUP_Z1_16);
	SspWrite(0);
	SspWrite(0);
	SspRead();
	SspRead();
	SspRead();

	udelay(50);

	SspWrite(SSDR_READ_Z1_16);
	SspWrite(0);
	SspWrite(0);
	SspRead();
	SampleZ1 = (unsigned short)SspRead() & 0x7f;
	SampleZ1 <<= 5;
	SampleZ1 += ((unsigned short)SspRead() & 0xf8) >> 3;

	// Z2-Position

	SspWrite(SSDR_PUP_Z2_16);
	SspWrite(0);
	SspWrite(0);
	SspRead();
	SspRead();
	SspRead();

	udelay(50);

	SspWrite(SSDR_READ_Z2_16);
	SspWrite(0);
	SspWrite(0);
	SspRead();
	SampleZ2 = (unsigned short)SspRead() & 0x7f;
	SampleZ2 <<= 5;
	SampleZ2 += ((unsigned short)SspRead() & 0xf8) >> 3;

	/* reset tablet and set low-mode */
	iris_reset_adc();

	if (SampleZ1 == 0 || SampleX == 0) {
		pressure = INIT_PRESSURE+1;
	}
	else {
		pressure = (int)SampleX*((int)SampleZ2/(int)SampleZ1-1);
	}
	
//	printk("X : %x,\tY : %x,\tZ1 : %x,\tpressure : %x\n", SampleX, SampleY, SampleZ1, pressure);

	*x = (int)SampleX;
	*y = (int)SampleY;

	last_SampleX = SampleX;
	last_SampleY = SampleY;

//	printk("X: %x,\tY: %x,\tpressure: %x(Z1: %x, Z2: %x)\n", SampleX, SampleY, pressure, SampleZ1, SampleZ2);

	/* !!!!!! restoreing access permission for shared SSP !!!!!! */
	//	if( irq_enabled_save ){
	//	  printk("read_sample : irq was enabled before...\n");
	//	  iris_enable_ssp_penint();
	//	  printk("read_sample : enabled irq\n");
	//	}
	iris_release_access();

	last_pressure = pressure;

	return pressure;
}


static int read_ts( int *x, int *y , int *p)
{
	int	pressure;
	int	tx,ty;

	pressure = read_sample(&tx,&ty);
	*x = tx;
	*y = ty;
	*p = pressure;
	return 1;
}
        
	
int iris_ts_open_arch(struct inode *inode, struct file *file)
{
	init_ssp();	/* Init SSP Master1	*/

#if 1
	if (0 == init_penirq()) {
//		printk("a: %x %x\n", IO_FPGA_Status, IO_FPGA_Enable);
//		printk("init_penirq - success\n");
		enable_penirq();
//		printk("b: %x %x\n", IO_FPGA_Status, IO_FPGA_Enable);
	}
#endif
#if 0
	if (0 == init_adstart_int()) {
		wait_adstart_int();
		release_adstart_int();
	}
#endif

#if 0
//	printk("ADCEnable() - Start\n");
	ADCEnable();
//	printk("ADCEnable() - Finish\n");
#endif

#if 1
	init_adstart_int();
	enable_adstart_int();
#endif

/*	printk("Test - Start\n");
	ts_test();
	printk("Test - Finish\n");*/

	return 0;
}



int iris_ts_release_arch(struct inode *inode, struct file *file)
{
#if 1
	release_penirq();
#endif

#if 1
	disable_adstart_int();
	release_adstart_int();
#endif

#if 1
	ADCDisable();
#endif

	release_ssp();

//	release_sample_queue();

	return 0;
}


static void FormatReadBuf(tsEvent *buf, int x, int y, int pressure)
{
	tsEvent	ReadBuf;
	struct timeval	tv;

	ReadBuf.x = x;
	ReadBuf.y = y;
	ReadBuf.pressure = pressure;

	do_gettimeofday(&tv);
	ReadBuf.timestump = (long long)tv.tv_sec*(long long)1E6+(long long)tv.tv_usec;

//	copy_to_user(buf, &ReadBuf, sizeof(ReadBuf));
	memcpy(buf, &ReadBuf, sizeof(tsEvent));
}


//****************************************************************************
//
// ADCEnable configures the synchronous serial interface.
//
//****************************************************************************
/* !!!!!! MOVED TO iris_ssp.c !!!!!! */
//static void ADCEnable(void)
//{
//	/* Tablet controler CS - Low	*/
//	IO_PEDR &= (~TABLET_CS_BIT);
//	udelay(1);
//}

//****************************************************************************
//
// ADCDisable powers down the synchronous serial interface.
//
//****************************************************************************
/* !!!!!! MOVED TO iris_ssp.c !!!!!! */
//static void ADCDisable(void)
//{
//	IO_PEDR |= TABLET_CS_BIT;	/* Tablet controler CS - High	*/
//
//	barrier();
//}


/* !!!!!! MOVED TO iris_ssp.c !!!!!! */
//static int SspWrite(unsigned char data)
//{
//	while (	IO_SSSR & (ulong)BSY
//		|| (IO_SSSR & (ulong)TNF) == 0) {
//		;
//	}
//
//	IO_SSDR = (unsigned long)data;
//
//	return (1);
//}

/* !!!!!! MOVED TO iris_ssp.c !!!!!! */
//static unsigned char SspRead(void)
//{
//	while (		(IO_SSSR & (unsigned long)BSY)
//			||	(IO_SSSR & (unsigned long)RNE) == 0) {
//		;
//	}
//
//	return (unsigned char)IO_SSDR;
//}

/*****************************************************************************************
	Pen IRQ operations
*****************************************************************************************/

static int init_penirq(void)
{
	int	err;

	/* !!!!!! MOVED TO iris_ssp.c , iris_init_ssp !!!!!! */
	//disable_penirq();
	//
	//clear_fpga_int(WSRC_PENIRQ_BIT);

	err = request_irq(IRQ_INTF, penirq_handler, SA_SHIRQ, "ts", penirq_handler);
	if (err) {
		return err;
	}
	/* !!!!!! MOVED TO iris_ssp.c , iris_init_ssp !!!!!! */
	//set_fpga_int_trigger_lowlevel(WSRC_PENIRQ_BIT);

	/* !!!!!! MOVED TO iris_ssp.c !!!!!! */
	//IO_PEDR &= (~TABLET_CS_BIT);
	//udelay(1);
	/* !!!!!! .... and ADCEnable() has same functionarity !!!!!!! */
	ADCEnable();

	barrier();

#ifdef CONFIG_PM
	penirq_initialized = 1;
#endif /* CONFIG_PM */

	return 0;
}

static void release_penirq(void)
{
	free_irq(IRQ_INTF, penirq_handler);
	/* !!!!!! MOVED TO iris_ssp.c !!!!!! */
	//disable_penirq();
	/* !!!!!! ... and iris_disable_ssp_penint has same functionarity !!!!!! */
	iris_disable_ssp_penint();
	del_timer(&timer_func_list);

#ifdef CONFIG_PM
	penirq_initialized = 0;
#endif /* CONFIG_PM */

}

static void enable_penirq(void)
{
  if( ! iris_get_access() ){
    extern void iris_enable_ssp_penint_fpga(void);
    iris_enable_ssp_penint_fpga();
    return;
  }
/* !!!!!! MOVED TO iris_ssp.c !!!!!! */
//	SspWrite(0x80);
//	SspRead();
///*	SspWrite(0);
//	SspRead();
//	SspWrite(0);
//	SspRead();*/                                  
//
//	ADCDisable();
//
//	clear_fpga_int(WSRC_PENIRQ_BIT);
//	IO_FPGA_EnReset = WSRC_PENIRQ_BIT;
//	enable_fpga_irq(WSRC_PENIRQ_BIT);
//
  /* !!!!!! this function has same functionarity !!!!!!!! */
  iris_enable_ssp_penint();
	iris_release_access();
	barrier();
}

inline void disable_penirq(void)
{
/* !!!!!! MOVED TO iris_ssp.c !!!!!! */
//#if 0
//	IO_FPGA_EnReset = WSRC_PENIRQ_BIT;
//#else
//	disable_fpga_irq(WSRC_PENIRQ_BIT);
//#endif
//	clear_fpga_int(WSRC_PENIRQ_BIT);

  /* !!!!!! this function has same functionarity !!!!!!!! */
  iris_disable_ssp_penint();
	barrier();
}


static void penirq_handler(int irq, void *dev_id, PT_REGS *reg)
{
#if 1
  extern int autoPowerCancel;
  extern int autoLightCancel;
#endif

  extern int iris_now_suspended;

  if( iris_now_suspended ) return;

	if (IO_FPGA_Status&WSRC_PENIRQ_BIT) {
	  DPRINTK("1");
//		printk("1: %x %x\n", IO_FPGA_Status, IO_FPGA_Enable);
		disable_penirq();
//		printk("2: %x %x\n", IO_FPGA_Status, IO_FPGA_Enable);

		ADCEnable();

		del_timer(&timer_func_list);

		timer_func_list.expires = jiffies+(HZ/100);
		timer_func_list.data = 0;
		add_timer(&timer_func_list);

#if 1  // auto power/light cancel
		autoPowerCancel = 0;
		autoLightCancel = 0;
#endif

		disable_fpga_irq(WSRC_PENIRQ_BIT);
		clear_fpga_int(WSRC_PENIRQ_BIT);

	}

	barrier();
}

static void tablet_sample_timer(ulong c)
{
	int	x, y, pressure;
	tsEvent	read_pos;
	static int	is_sample_timer_wait = 0;	
	static int      down_suc = 0;

	DPRINTK("1\n");

	del_timer(&timer_func_list);

	if (is_sample_timer_wait) {
	DPRINTK("1-1\n");
		is_sample_timer_wait = 0;
		enable_penirq();
	DPRINTK("1-2\n");
	}
	else {

	DPRINTK("2\n");
		read_ts(&x, &y, &pressure);
	DPRINTK("3\n");

		FormatReadBuf(&read_pos, x, y, pressure);
		set_sample_to_queue(&read_pos);

	DPRINTK("4\n");

		if( pressure <= INIT_PRESSURE && pressure > 0 ){
			timer_func_list.expires = jiffies+(HZ/100);
			timer_func_list.data = 0;
			add_timer(&timer_func_list);
			{
			  if( ! down_suc ){
			    down_suc = 1;
			  }
			}
		}
		else {
			is_sample_timer_wait = 1;
#if 1 /* for Iris QA0.5 or later */
			timer_func_list.expires = jiffies+(HZ * 30 / 1000);
#else
			timer_func_list.expires = jiffies+(HZ/100);
#endif
			timer_func_list.data = 0;
			add_timer(&timer_func_list);
			{
			  down_suc = 0;
			}
		}
	DPRINTK("5\n");
	}
}



/*****************************************************************************************
	AD Start Interrupt operations
*****************************************************************************************/

static int init_adstart_int(void)
{
#if 0
	int	err;
#endif

	adstart_int_flg = 0;
	IO_FPGA_ADINT = ADINT_TIMING;

	disable_adstart_int();

#if 0
	err = request_irq(IRQ_UCB1200, adstart_int_handler,
			SA_SHIRQ|SA_INTERRUPT, "ts", adstart_int_handler);
	if (err) {
		return err;
	}
#endif

//	printk("AD start interrupt initialised.\n");

	barrier();

	return 0;
}

static void release_adstart_int(void)
{
	disable_adstart_int();
#if 0
	free_irq(IRQ_UCB1200, adstart_int_handler);
#endif
	adstart_int_flg = 0;

	barrier();
}

inline void enable_adstart_int(void)
{
  //enable_irq(IRQ_UCB1200);
	IO_FPGA_ADICLR = 0;	/* AD start interrupt clear	*/
	IO_FPGA_ADIEN = 1;	/* AD start interrupt disable	*/
}

inline void disable_adstart_int(void)
{
  //disable_irq(IRQ_UCB1200);
	IO_FPGA_ADIEN = 0;	/* AD start interrupt disable	*/
	IO_FPGA_ADICLR = 0;	/* AD start interrupt clear	*/
}

#if 0
static void adstart_int_handler(int irq, void *dev_id, PT_REGS *reg)
{
	IO_FPGA_GPO |= (ushort)1<<9;
	disable_adstart_int();
	adstart_int_flg = 1;
//	printk("AD start interrupted.\n");

	barrier();
}
#endif

#if 0
static void wait_adstart_int(void)
{
	IO_FPGA_GPO &= ~((ushort)1<<9);
	adstart_int_flg = 0;
	enable_adstart_int();

	while (0==adstart_int_flg) {
		schedule();
	}
	disable_adstart_int();
	adstart_int_flg = 0;

	barrier();
}
#endif

#endif /* ! NEW_SSP_CODE */

/*
 *    old SSP code Ends here
 *
 */

/* ================================================================
 *    NEW SSP CODE
 *
 */

#ifdef NEW_SSP_CODE

static void penirq_handler(int irq, void *dev_id, struct pt_regs *reg);
static void tablet_sample_timer(unsigned long __unused);
static void nextirq_wait_timer(unsigned long __unused);

static struct timer_list sampling_timer = {
  function: tablet_sample_timer
};

static struct timer_list nextirq_timer = {
  function: nextirq_wait_timer
};

/*
 *    upper layer stuff
 */
extern int set_sample_to_queue(tsEvent *sample);

/*
 *    misc wrappers
 */

#define adc_enable()  enable_adc_con()
#define adc_disable() disable_adc_con()

/*
 *    Format Stuff
 */

static void FormatReadBuf(tsEvent *buf, int x, int y, int pressure)
{
  tsEvent ReadBuf;
  struct timeval tv;

  ReadBuf.x = x;
  ReadBuf.y = y;
  ReadBuf.pressure = pressure;

  do_gettimeofday(&tv);
  ReadBuf.timestump = (long long)tv.tv_sec*(long long)1E6+(long long)tv.tv_usec;

  memcpy(buf, &ReadBuf, sizeof(tsEvent));
}

/*
 *    AD Start INT Stuff
 */
#define IRQ_ADINT IRQ_UCB1200
#define ADINT_TIMING	178

static void adstart_int_disable(void)
{
  IO_FPGA_ADIEN = 0;	/* AD start interrupt disable	*/
  IO_FPGA_ADICLR = 0;	/* AD start interrupt clear	*/
}

static void adstart_int_enable(void)
{
  IO_FPGA_ADICLR = 0;	/* AD start interrupt clear	*/
  IO_FPGA_ADIEN = 1;	/* AD start interrupt disable	*/
}

static void adstart_int_setup(void)
{
  IO_FPGA_ADINT = ADINT_TIMING;
  adstart_int_disable();
  barrier();
}

static void adstart_int_open(void)
{
}

static void adstart_int_release(void)
{
}


/*
 *    Sampling Stuff
 */

#define SSDR_PUP_X16            0x00D3
#define SSDR_PUP_Y16            0x0093
#define SSDR_PUP_Z1_16          0x00B3
#define SSDR_PUP_Z2_16          0x00C3
#define SSDR_READ_X16           0x00D0
#define SSDR_READ_Y16           0x0090
#define SSDR_READ_Z1_16         0x00B0
#define SSDR_READ_Z2_16         0x00C0

static unsigned long down_pressure = 10000;
static unsigned long release_pressure = 10000;
static unsigned long init_pressure = 10000;

#if 0
#define INIT_PRESSURE           9000
#define RELEASE_PRESSURE        9000
#else
#define INIT_PRESSURE           (init_pressure)
#define RELEASE_PRESSURE        (release_pressure)
#endif

static void load_default_values_from_rom(void)
{
  down_pressure = (*(unsigned long*)(FLASH2_BASE + 0xd0));
  release_pressure = (*(unsigned long*)(FLASH2_BASE + 0xd4));
  init_pressure = ( down_pressure > release_pressure ? down_pressure : release_pressure );
  DPRINTK("LOAD ROM TS VALUES : DOWN: %d UP: %d --> INIT: %d\n",
	 down_pressure,release_pressure,init_pressure);
}

static int ref_count = 0;

static int read_sample(tsEvent *buf)
{
  unsigned char xpos_cmd[3] = { SSDR_PUP_X16 , 0 , 0 };
  unsigned char ypos_cmd[3] = { SSDR_PUP_Y16 , 0 , 0 };
  unsigned char zpos1_cmd[3] = { SSDR_PUP_Z1_16 , 0 , 0 };
  unsigned char zpos2_cmd[3] = { SSDR_PUP_Z2_16 , 0 , 0 };

  unsigned char read_bytes[3];
  unsigned long flags;
  unsigned short x,y,z1,z2;
  int ix,iy,ip;

  DPRINTK("1\n");
  begin_adc_disable_irq();

  DPRINTK("2\n");
  exchange_three_bytes(xpos_cmd,read_bytes);

  udelay(50);

  save_flags(flags); cli();
  IO_FPGA_ADICLR = 0;  /* AD start interrupt clear */
  while( 0 == ( IO_IRQRAWSTATUS & ( 1 << IRQ_ADINT ) ) );
  exchange_three_bytes(xpos_cmd,read_bytes);
  restore_flags(flags);
  x = (unsigned short)read_bytes[1];
  x <<= 4;
  x |= ((unsigned short)read_bytes[2]) >> 4;

  DPRINTK("3\n");
  exchange_three_bytes(ypos_cmd,read_bytes);

  udelay(50);

  save_flags(flags); cli();
  IO_FPGA_ADICLR = 0;  /* AD start interrupt clear */
  while( 0 == ( IO_IRQRAWSTATUS & ( 1 << IRQ_ADINT ) ) );
  exchange_three_bytes(ypos_cmd,read_bytes);
  restore_flags(flags);
  y = (unsigned short)read_bytes[1];
  y <<= 4;
  y |= ((unsigned short)read_bytes[2]) >> 4;

  DPRINTK("4\n");
  exchange_three_bytes(zpos1_cmd,read_bytes);

  udelay(50);
  
  exchange_three_bytes(zpos1_cmd,read_bytes);
  z1 = (unsigned short)read_bytes[1];
  z1 <<= 4;
  z1 |= ((unsigned short)read_bytes[2]) >> 4;
  
  DPRINTK("5\n");
  exchange_three_bytes(zpos2_cmd,read_bytes);

  udelay(50);

  exchange_three_bytes(zpos2_cmd,read_bytes);
  z2 = (unsigned short)read_bytes[1];
  z2 <<= 4;
  z2 |= ((unsigned short)read_bytes[2]) >> 4;

  send_adcon_reset_cmd();

  DPRINTK("6\n");
  end_adc_enable_irq();
  DPRINTK("7\n");
    
  if( z1 == 0 || z2 == 0 ){
    ip = INIT_PRESSURE+1;
  }else{
    ip = (int)x*((int)z2/(int)z1-1);
  }
  ix = (int)x;
  iy = (int)y;

  FormatReadBuf(buf,ix,iy,ip);

  DPRINTK("8\n");
  return ip;
}

/*
 *    Interval Sampling Stuff
 */
static void tablet_sample_timer(unsigned long __unused)
{
  int pressure;
  tsEvent read_pos;

  DPRINTK("\n");

  del_timer(&sampling_timer);
  del_timer(&nextirq_timer);

  pressure = read_sample(&read_pos);

  set_sample_to_queue(&read_pos);

  if( pressure <= INIT_PRESSURE && pressure > 0 ){
    sampling_timer.expires = jiffies+(HZ/100);
    add_timer(&sampling_timer);
  }
  else {
    nextirq_timer.expires = jiffies+(HZ * 30 / 1000);
    add_timer(&nextirq_timer);
  }
}

/*
 *    IRQ Handler Stuff
 */

#define WSRC_PENIRQ_BIT	((ushort)bitWSRC6)

static void nextirq_wait_timer(unsigned long __unused)
{
  del_timer(&sampling_timer);
  del_timer(&nextirq_timer);
  send_adcon_reset_cmd();
  adc_disable();
  enable_tablet_irq();
}

static void penirq_handler(int irq, void *dev_id, struct pt_regs *reg)
{
  extern int autoPowerCancel;
  extern int autoLightCancel;

  DPRINTK("\n");

  if( IO_FPGA_Status & WSRC_PENIRQ_BIT ){

    DPRINTK("mine\n");

    disable_tablet_irq();
    adc_enable();
    del_timer(&sampling_timer);
    del_timer(&nextirq_timer);

    sampling_timer.expires = jiffies+(HZ/100);
    sampling_timer.data = 0;
    add_timer(&sampling_timer);

    autoPowerCancel = 0;
    autoLightCancel = 0;

    clear_tablet_irq();
    disable_tablet_irq();
  }
}

/*
 *  Power Management Stuff
 */

#ifdef CONFIG_PM
static struct pm_dev *iris_ts_pm_dev;
#endif

#ifdef CONFIG_PM

static int iris_ts_pm_callback(struct pm_dev *pm_dev,
			       pm_request_t req, void *data)
{
  static int suspended = 0;
  switch (req) {
  case PM_BLANK:
  case PM_STANDBY:
  case PM_SUSPEND:
    if( suspended ) break;
    DPRINTK("suspend\n");
    del_timer(&sampling_timer);
    del_timer(&nextirq_timer);
    disable_tablet_irq();
    iris_ssp_enter_suspend();
    suspended = 1;
    break;
  case PM_UNBLANK:
  case PM_RESUME:
    if( ! suspended ) break;
    DPRINTK("resume\n");
    iris_ssp_exit_suspend();
    if( IO_FPGA_RawStat & WSRC_PENIRQ_BIT ){
      sampling_timer.expires = jiffies+HZ*2;
      add_timer(&sampling_timer);
    }else if( ref_count > 0 ){
      send_adcon_reset_cmd();
      enable_tablet_irq();
    }
    suspended = 0;
    break;
  }
  return 0;
}
#endif

/*
 *   I/O Stuff
 */

int iris_ts_open_arch(struct inode *inode, struct file *file)
{
  int err;
  iris_init_ssp(); /* Init SSP */
  if( ! ref_count ){
    DPRINTK("new open\n");
    del_timer(&sampling_timer);
    del_timer(&nextirq_timer);
    err = request_irq(IRQ_INTF, penirq_handler, SA_SHIRQ, "ts", penirq_handler);
    if (err) {
      DPRINTK("err\n");
      return err;
    }
    adstart_int_open();
    adstart_int_setup();
    adstart_int_enable();
  }
  ref_count++;
  send_adcon_reset_cmd();
  enable_tablet_irq();
  return 0;
}


int iris_ts_release_arch(struct inode *inode, struct file *file)
{
  ref_count--;
  if( ! ref_count ){
    DPRINTK("last close\n");
    send_adcon_reset_cmd();
    disable_tablet_irq();
    free_irq(IRQ_INTF, penirq_handler);
    del_timer(&sampling_timer);
    del_timer(&nextirq_timer);
    iris_release_ssp();
    adstart_int_disable();
    adstart_int_release();
  }
  return 0;
}


int __init iris_ts_arch_init(void)
{
  init_timer(&sampling_timer);  /* initialize interval timer */
  init_timer(&nextirq_timer);  /* initialize interval timer */
  iris_init_ssp_sys(); /* init SSP whole system */
#ifdef CONFIG_PM
  iris_ts_pm_dev = pm_register(PM_SYS_DEV, 0, iris_ts_pm_callback);
#endif
  load_default_values_from_rom();
  return 0;
}

module_init(iris_ts_arch_init);


#endif /* NEW_SSP_CODE */

