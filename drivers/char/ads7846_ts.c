/*
 * linux/drivers/char/ads7846_ts.c
 *
 * Copyright (c) 2001 Lineo Japan, Inc.
 *
 * May be copied or modified under the terms of the GNU General Public
 * License.  See linux/COPYING for more information.
 *
 * Based on:
 *   cotulla_ts.c
 *
 * Change Log
 * 	01-Jul-2002 SHARP Corporation
 *	31-Jul-2002 Lineo Japan, Inc.  for ARCH_PXA
 *	12-Dec-2002 Sharp Corporation for Poodle and Corgi
 *	27-Feb-2003 Sharp modified for Poodle
 *
 */

/***************************************************************************/

#include <linux/config.h>
#include <linux/init.h>
#include <linux/poll.h>
#include <linux/delay.h>
#include <linux/ioctl.h>

#include <asm/segment.h>
#include <asm/uaccess.h>
#include <asm/arch/hardware.h>
#include <asm/arch/irqs.h>
#include <asm/arch/ads7846_ts.h>

#include <asm/sharp_apm.h>

#ifdef CONFIG_PM
#include <linux/pm.h>
#endif

#if defined(CONFIG_ARCH_PXA_POODLE)
#define	USE_SL5500_QT
#endif
#if defined(CONFIG_ARCH_PXA_CORGI)
//#define	USE_SL5500_QT
#endif

#ifdef CONFIG_PM
static struct pm_dev *touch_pm_dev;
static int cotulla_touch_pm_callback(struct pm_dev* pm_dev,
			       pm_request_t req, void* data);
#endif

struct fasync_struct *fasync;
int ts_type = 0;

static void ts_interrupt(int, void*, struct pt_regs*);
static void ts_interrupt_main(int irq, int isTimer);
static struct timer_list tp_main_timer;
/* for iPaq compatible */
#define IPAQ_COMPATIBLE
#ifdef IPAQ_COMPATIBLE
#define BUFSIZE 128
#define XLIMIT 256
#define YLIMIT 256
static int raw_max_x, raw_max_y, res_x, res_y, raw_min_x, raw_min_y, xyswap;
static int cal_ok, x_rev, y_rev;
 
#ifdef USE_SL5500_QT
typedef struct {
    long x;
    long y;
    long pressure;
    long long millisecs;
} TS_EVENT;
#else
typedef struct {
   short pressure;
   short x;
   short y;
   short millisecs;
} TS_EVENT;
#endif

static TS_EVENT tbuf[BUFSIZE], tc, samples[3];
static DECLARE_WAIT_QUEUE_HEAD(queue);
static int head, tail, sample;
static char pendown = 0;
unsigned long Pressure;
#if defined(CONFIG_ARCH_PXA_CORGI)
static char ispass = 1;
#endif

 #ifdef CONFIG_PM
  #define PMPD_MODE_ACTIVE  0
  #define PMPD_MODE_SUSPEND 1
  #define PMPD_MODE_RESUME  2
  static char PowerDownMode = PMPD_MODE_ACTIVE;
 #endif // CONFIG_PM
#endif
//extern unsigned short touch_panel_intr;

#if defined(CONFIG_ARCH_PXA_POODLE) || defined(CONFIG_ARCH_PXA_CORGI)
extern void pxa_ssp_init(void);
#endif
extern unsigned long ssp_get_dac_val(unsigned long, int);
extern unsigned long ssp_put_dac_val(ulong data, int cs);
extern unsigned long ssp_only_get_dac_val(int cs);
#if defined(CONFIG_ARCH_PXA_CORGI)
#include <asm/arch/corgi.h>
#else
#define CS_ADS7846	2
#endif

unsigned long ads7846_rw_cli(ulong data)
{
	return ssp_get_dac_val(data, CS_ADS7846);
#if 0
        ulong flags,ret;

	save_flags(flags);
	cli();

	SSDR = data;
	while ((SSSR & SSSR_TNF_MSK) != SSSR_TNF_MSK);
	udelay(1);
	while ((SSSR & SSSR_RNE_MSK) != SSSR_RNE_MSK);
	ret = (SSDR);

	restore_flags(flags);
	return ret;
#endif
}

unsigned long ads7846_rw(ulong data)
{
	return ssp_get_dac_val(data, CS_ADS7846);
#if 0
	SSDR = data;
	while ((SSSR & SSSR_TNF_MSK) != SSSR_TNF_MSK);
	udelay(1);
	while ((SSSR & SSSR_RNE_MSK) != SSSR_RNE_MSK);
	return (SSDR);
#endif
}

#if defined(CONFIG_ARCH_PXA_CORGI)

//#define WAIT_AFTER_SYNC_HS 151 /* 41us */
//#define WAIT_AFTER_SYNC_HS 142 /* 38.5us */
static unsigned long wait_after_sync_hs = 0;
#define WAIT_AFTER_SYNC_HS wait_after_sync_hs
#define SyncHS()	while((GPLR(GPIO_HSYNC) & GPIO_bit(GPIO_HSYNC)) == 0);\
while((GPLR(GPIO_HSYNC) & GPIO_bit(GPIO_HSYNC)) != 0);
#define CCNT(a)		asm("mrc p14, 0, %0, C1, C0, 0" : "=r"(a))
#define CCNT_ON()	{int pmnc = 1;asm("mcr p14, 0, %0, C0, C0, 0" : : "r"(pmnc));}
#define CCNT_OFF()	{int pmnc = 0;asm("mcr p14, 0, %0, C0, C0, 0" : : "r"(pmnc));}
#if 1
#define WAIT_HS_333_VGA		5791	// 17.995us
#define WAIT_HS_400_VGA		7013	// 17.615us
#define WAIT_HS_333_QVGA	13969	// 42.100us
#define WAIT_HS_400_QVGA	16621	// 41.750us
#else
#define WAIT_HS_333_VGA		6403	// 19.3us
#define WAIT_HS_400_VGA		7683	// 19.3us
#define WAIT_HS_333_QVGA	13935	// 42.0us
#define WAIT_HS_400_QVGA	16720	// 42.0us
#endif
#define LCD_MODE_480 0
#define LCD_MODE_320 1
#define LCD_MODE_240 2
extern int w100fb_lcdMode;
extern int w100fb_isblank;
extern unsigned long cccr_reg;

static int sync_receive_data_send_cmd(int doRecive,int doSend,int sCmd)
{
	int pos = 0;
	unsigned long timer1,timer2;
	unsigned long wait_time;

	if (w100fb_isblank) {
		/* read SSDR */
		if(doRecive){
			pos = ssp_only_get_dac_val(CS_ADS7846);
		}
		if(doSend) {
			/* dummy command */
			ssp_get_dac_val(sCmd, CS_ADS7846);
			ssp_put_dac_val(sCmd, CS_ADS7846);
		}
		return pos;
	}

	if (cccr_reg == 0x145) {
		// 332MHz
		if (w100fb_lcdMode == LCD_MODE_480) {
			wait_time = WAIT_HS_333_VGA - wait_after_sync_hs;
		}
		else {
			wait_time = WAIT_HS_333_QVGA - wait_after_sync_hs;
		}
	}
	else {
		// 400MHz
		if (w100fb_lcdMode == LCD_MODE_480) {
			wait_time = WAIT_HS_400_VGA - wait_after_sync_hs;
		}
		else {
			wait_time = WAIT_HS_400_QVGA - wait_after_sync_hs;
		}
	}

	CCNT_ON();

	/* polling HSync */
	SyncHS();

	/* get CCNT */
	CCNT(timer1);

	/* read SSDR */
	if(doRecive){
		pos = ssp_only_get_dac_val(CS_ADS7846);
	}

	if(doSend) {
		/* dummy command */
		ssp_get_dac_val(sCmd, CS_ADS7846);

		/* Wait after HSync */
		CCNT(timer2);
		if (w100fb_lcdMode != LCD_MODE_480) {
			while((timer2-timer1) < wait_time){
				CCNT(timer2);
			}
			ssp_put_dac_val(sCmd, CS_ADS7846);
		}
		else {
			/* timeout */
			SyncHS();
			/* get OSCR */
			CCNT(timer1);
			/* Wait after HSync */
			CCNT(timer2);
			while((timer2-timer1) < wait_time){
				CCNT(timer2);
			}
			ssp_put_dac_val(sCmd, CS_ADS7846);
		}
	}

	CCNT_OFF();

	return pos;
}
#endif

static unsigned int read_xydata_sample(unsigned long cmd, int diff, int delay, int sn);


#if defined(CONFIG_ARCH_PXA_POODLE)
#define ABS(a) ( (a)<0 ? -(a) : (a))
#endif

void read_xydata(ts_pos_t *tp)
{

#if defined(CONFIG_ARCH_PXA_POODLE) && !defined(CONFIG_POODLE_TR0) /////////

#if 0
	unsigned long Rx = 1000;
	unsigned long cmd;
	unsigned int dummy,z1,z2;
	unsigned int x,y;


	while(1) {

	/* X-axis */
	cmd = (1u << ADSCTRL_PD0_SH) | (1u << ADSCTRL_PD1_SH) |
			(5u << ADSCTRL_ADR_SH) | (1u << ADSCTRL_STS_SH);
	dummy = ads7846_rw(cmd);
	tp->xd = ads7846_rw(cmd);

	/* Y-axis */
	cmd = (1u << ADSCTRL_PD0_SH) | (1u << ADSCTRL_PD1_SH) |
			(1u << ADSCTRL_ADR_SH) | (1u << ADSCTRL_STS_SH);
	dummy = ads7846_rw(cmd);
	tp->yd = ads7846_rw(cmd);


	cmd = (1u << ADSCTRL_PD0_SH) | (1u << ADSCTRL_PD1_SH) |
			(3u << ADSCTRL_ADR_SH) | (1u << ADSCTRL_STS_SH);
	dummy = ads7846_rw(cmd);
	udelay(1);
	z1 = ads7846_rw(cmd);

	/* Z2 */
	cmd = (1u << ADSCTRL_PD0_SH) | (1u << ADSCTRL_PD1_SH) |
			(4u << ADSCTRL_ADR_SH) | (1u << ADSCTRL_STS_SH);
	dummy = ads7846_rw(cmd);
	udelay(1);
	z2 = ads7846_rw(cmd);


	if (z1 != 0)
		Pressure = Rx * (tp->xd) * ((10*z2/z1) - 1*10) >> 10;
	else 
		Pressure = 0;


        cmd =	(1u << ADSCTRL_PD0_SH) | (1u << ADSCTRL_PD1_SH) |
		(4u << ADSCTRL_ADR_SH) | (1u << ADSCTRL_STS_SH);
	/* Power-Down Enable */
	cmd &= ~(ADSCTRL_PD0_MSK | ADSCTRL_PD1_MSK);
	dummy = ads7846_rw(cmd);

	break;

	}

#elif 1
#define	abscmpmin(x,y,d) (\
		((int)((x)-(y))<(int)(d)) && \
		((int)((y)-(x))<(int)(d)) )
	unsigned long cmd;
	unsigned int t,x,y,z[2];
	int i,j,k;
	int d = 8, c = 10;
	int err = 0;

	for(i=j=k=0,x=y=0;;i=1){
		/* Pressure */
		cmd = (1u << ADSCTRL_PD0_SH) | (1u << ADSCTRL_PD1_SH) |
			(3u << ADSCTRL_ADR_SH) | (1u << ADSCTRL_STS_SH);
		t = ads7846_rw(cmd);
		z[i] = ads7846_rw(cmd);
 
		if( i ) break;

		/* X-axis */
		cmd = (1u << ADSCTRL_PD0_SH) | (1u << ADSCTRL_PD1_SH) |
			(5u << ADSCTRL_ADR_SH) | (1u << ADSCTRL_STS_SH);
		x = ads7846_rw(cmd);
		for(j=0; !err; j++){
			t = x;
			x = ads7846_rw(cmd);
			if( abscmpmin(t,x,d) )
				break;
			if( j > c ){
				err = 1;
//DBG*/				printk("ts: x(%d,%d,%d)\n",t,x,t-x);
			}
		}

		/* Y-axis */
		cmd = (1u << ADSCTRL_PD0_SH) | (1u << ADSCTRL_PD1_SH) |
			(1u << ADSCTRL_ADR_SH) | (1u << ADSCTRL_STS_SH);
		y = ads7846_rw(cmd);
		for(k=0; !err; k++){
			t = y;
			y = ads7846_rw(cmd);
			if( abscmpmin(t,y,d) )
				break;
			if( k > c ){
				err = 1;
//DBG*/				printk("ts: y(%d,%d,%d)\n",t,y,t-y);
			}
		}
	}
	Pressure = 1;
	for(i=0;i<2;i++){
		if( !z[i] )
			Pressure = 0;
	}
	if( Pressure ){
		for(i=0;i<2;i++){
			if( z[i] < 10 )
				err = 1;
		}
		if( x >= 4095 )
			err = 1;
	}

	/* Power-Down Enable */
	cmd &= ~(ADSCTRL_PD0_MSK | ADSCTRL_PD1_MSK);
	t = ads7846_rw(cmd);

	if( err ){
		printk("ts: pxyp=%d(%d/%d,%d/%d)%d\n",
			z[0],x,j,y,k,z[1]);
		x=0; y=0;
	} else {
//DBG*/		printk("pxype=%d,%d,%d,%d,%d\n",z[0],x,y,z[1],err);
	}
	tp->xd = x;
	tp->yd = y;


#else
	unsigned long Rx = 1000;
	unsigned long cmd;
	unsigned int dummy,z1,z2;
	unsigned int x[4],y[4],diff0,diff1,diff2,diff3,i;


	while(1) {
	for(i=0;i<4;i++) {

	/* X-axis */
	cmd = (1u << ADSCTRL_PD0_SH) | (1u << ADSCTRL_PD1_SH) |
			(5u << ADSCTRL_ADR_SH) | (1u << ADSCTRL_STS_SH);
	dummy = ads7846_rw(cmd);
	x[i] = ads7846_rw(cmd);

	/* Y-axis */
	cmd = (1u << ADSCTRL_PD0_SH) | (1u << ADSCTRL_PD1_SH) |
			(1u << ADSCTRL_ADR_SH) | (1u << ADSCTRL_STS_SH);
	dummy = ads7846_rw(cmd);
	y[i] = ads7846_rw(cmd);


	cmd = (1u << ADSCTRL_PD0_SH) | (1u << ADSCTRL_PD1_SH) |
			(3u << ADSCTRL_ADR_SH) | (1u << ADSCTRL_STS_SH);
	dummy = ads7846_rw(cmd);
	udelay(1);
	z1 = ads7846_rw(cmd);

	/* Z2 */
	cmd = (1u << ADSCTRL_PD0_SH) | (1u << ADSCTRL_PD1_SH) |
			(4u << ADSCTRL_ADR_SH) | (1u << ADSCTRL_STS_SH);
	dummy = ads7846_rw(cmd);
	udelay(1);
	z2 = ads7846_rw(cmd);

	}

	diff0 = ABS(x[0]-x[1]);
	diff1 = ABS(x[1]-x[2]);
	diff2 = ABS(x[2]-x[3]);
	diff3 = ABS(x[3]-x[1]);

	//	if ( diff0 > XLIMIT || diff1 > XLIMIT || diff2 > XLIMIT || diff3 > XLIMIT ) {
	//	  printk("continue x\n");
	//		continue;
	//	}

	if ( diff1 < diff2 ) {
		if ( diff1 < diff3 )
			tp->xd = ( x[1] + x[2] ) / 2;
		else
			tp->xd = ( x[3] + x[1] ) / 2;
	} else {
		if ( diff2 < diff3 )
			tp->xd = ( x[2] + x[3] ) / 2;
		else
			tp->xd = ( x[3] + x[1] ) / 2;
	}

	diff0 = ABS(y[0]-y[1]);
	diff1 = ABS(y[1]-y[2]);
	diff2 = ABS(y[2]-y[3]);
	diff3 = ABS(y[3]-y[1]);

	//	if ( diff0 > YLIMIT || diff1 > YLIMIT || diff2 > YLIMIT || diff3 > YLIMIT ) {
	//	  printk("continue y\n");
	//		continue;
	//	}

	if ( diff1 < diff2 ) {
		if ( diff1 < diff3 )
			tp->yd = ( y[1] + y[2] ) / 2;
		else
			tp->yd = ( y[3] + y[1] ) / 2;
	} else {
		if ( diff2 < diff3 )
			tp->yd = ( y[2] + y[3] ) / 2;
		else
			tp->yd = ( y[3] + y[1] ) / 2;
	}


	//	printk("x=%d,y=%d\n",tp->xd,tp->yd);

	if (z1 != 0)
		Pressure = Rx * (tp->xd) * ((10*z2/z1) - 1*10) >> 10;
	else 
		Pressure = 0;


        cmd =	(1u << ADSCTRL_PD0_SH) | (1u << ADSCTRL_PD1_SH) |
		(4u << ADSCTRL_ADR_SH) | (1u << ADSCTRL_STS_SH);
	/* Power-Down Enable */
	cmd &= ~(ADSCTRL_PD0_MSK | ADSCTRL_PD1_MSK);
	dummy = ads7846_rw(cmd);

	break;

	}
#endif

#else //////////////////////////////////////////////////////////////////////

#if defined(CONFIG_ARCH_PXA_CORGI)	// C700

#define TOUCH_PANEL_AVERAGE 1

	unsigned long cmd;
	unsigned int dummy,z1,z2;
	unsigned int x,y,tx,ty;
	int i, fail;
	
	GPDR(GPIO_HSYNC) &= ~GPIO_bit(GPIO_HSYNC);

	tx = 0;
	ty = 0;
	fail = 0;

	for (i = 0; i < TOUCH_PANEL_AVERAGE; i++) {

		/* Y-axis */
		cmd = (1u << ADSCTRL_PD0_SH) | (1u << ADSCTRL_PD1_SH) |
			(1u << ADSCTRL_ADR_SH) | (1u << ADSCTRL_STS_SH);
		sync_receive_data_send_cmd(0, 1, cmd);

		/* Y-axis */
		cmd = (1u << ADSCTRL_PD0_SH) | (1u << ADSCTRL_PD1_SH) |
			(1u << ADSCTRL_ADR_SH) | (1u << ADSCTRL_STS_SH);
		sync_receive_data_send_cmd(1, 1, cmd);

		/* X-axis */
		cmd = (1u << ADSCTRL_PD0_SH) | (1u << ADSCTRL_PD1_SH) |
			(5u << ADSCTRL_ADR_SH) | (1u << ADSCTRL_STS_SH);
		y = sync_receive_data_send_cmd(1, 1, cmd);

		/* Z1 */
		cmd = (1u << ADSCTRL_PD0_SH) | (1u << ADSCTRL_PD1_SH) |
			(3u << ADSCTRL_ADR_SH) | (1u << ADSCTRL_STS_SH);
		x = sync_receive_data_send_cmd(1, 1, cmd);

		/* Z2 */
		cmd = (1u << ADSCTRL_PD0_SH) | (1u << ADSCTRL_PD1_SH) |
			(4u << ADSCTRL_ADR_SH) | (1u << ADSCTRL_STS_SH);
		z1 = sync_receive_data_send_cmd(1, 1, cmd);
		z2 = sync_receive_data_send_cmd(1, 0, cmd);

#if 0
		printk("x %d y %d z1 %d z2 %d\n", x, y, z1, z2);
		if (z1 != 0) {
			printk("pressure %d\n", (x * (z2 -z1) / z1));
		}
#endif
		if ((z1 == 0) || ((x * (z2 -z1) / z1) >= 15000)) {
			fail++;
		}
		else {
			tx += x;
			ty += y;
		}
	}

	/* Power-Down Enable */
	cmd = (1u << ADSCTRL_ADR_SH) | (1u << ADSCTRL_STS_SH);
	dummy = ads7846_rw(cmd);

//	printk("fail %d\n", fail);

	if (fail == TOUCH_PANEL_AVERAGE) {
		Pressure = 0;
//		printk("pen up\n");
	}
	else {
		Pressure = 1;
		tp->xd = tx / (TOUCH_PANEL_AVERAGE - fail);
		tp->yd = ty / (TOUCH_PANEL_AVERAGE - fail);
//		printk("pen position (%d,%d)\n", tx, ty);
	}
#else

#if 0 /* Change: Trace Sample */
	unsigned long Rx = 1000;
	unsigned long cmd;
	unsigned int dummy,z1,z2;

	/* X-axis */
	cmd = (1u << ADSCTRL_PD0_SH) | (1u << ADSCTRL_PD1_SH) |
			(5u << ADSCTRL_ADR_SH) | (1u << ADSCTRL_STS_SH);
	dummy = ads7846_rw(cmd);
	udelay(1);
	tp->xd = ads7846_rw(cmd);

	/* Y-axis */
	cmd = (1u << ADSCTRL_PD0_SH) | (1u << ADSCTRL_PD1_SH) |
			(1u << ADSCTRL_ADR_SH) | (1u << ADSCTRL_STS_SH);
	dummy = ads7846_rw(cmd);
	udelay(1);
	tp->yd = ads7846_rw(cmd);
#else

#if defined(CONFIG_ARCH_PXA_POODLE) || defined(CONFIG_ARCH_PXA_CORGI)
#define	REJECT_XDIFF	100
#define	REJECT_YDIFF	100
#else
#define	REJECT_XDIFF	16 /*32*/
#define	REJECT_YDIFF	16 /*48*/
#endif
#define	SAMPLE_DELAY	0
#define	SAMPLE_COUNT	4
	unsigned long cmd;
	unsigned int dummy, t1, t2;

	/* X-axis */
	cmd =	(1u << ADSCTRL_PD0_SH) | (1u << ADSCTRL_PD1_SH) |
		(5u << ADSCTRL_ADR_SH) | (1u << ADSCTRL_STS_SH);
	t1 = read_xydata_sample(cmd, REJECT_XDIFF, SAMPLE_DELAY, SAMPLE_COUNT);
	if( !t1 ){
//DBG*/		printk("X\n");
		goto sampling_error;
	}
	/* Y-axis */
	cmd =	(1u << ADSCTRL_PD0_SH) | (1u << ADSCTRL_PD1_SH) |
		(1u << ADSCTRL_ADR_SH) | (1u << ADSCTRL_STS_SH);
	t2 = read_xydata_sample(cmd, REJECT_YDIFF, SAMPLE_DELAY, SAMPLE_COUNT);
	if( !t2 ){
//DBG*/		printk("Y\n");
sampling_error:
		t1 = t2 = 0;
	}

	tp->xd = t1;
	tp->yd = t2;
#endif

#if 0 /* Delete: Pressure Check */
	/* Z1 */
	cmd = (1u << ADSCTRL_PD0_SH) | (1u << ADSCTRL_PD1_SH) |
			(3u << ADSCTRL_ADR_SH) | (1u << ADSCTRL_STS_SH);
	dummy = ads7846_rw(cmd);
	udelay(1);
	z1 = ads7846_rw(cmd);

	/* Z2 */
	cmd = (1u << ADSCTRL_PD0_SH) | (1u << ADSCTRL_PD1_SH) |
			(4u << ADSCTRL_ADR_SH) | (1u << ADSCTRL_STS_SH);
	dummy = ads7846_rw(cmd);
	udelay(1);
	z2 = ads7846_rw(cmd);


	if (z1 != 0)
//		Pressure = Rx * (tp->xd/4096) * ((z2/z1) - 1);
		Pressure = Rx * (tp->xd) * ((10*z2/z1) - 1*10) >> 10;
	else 
		Pressure = 0;

//printk("TS:1=%d,2=%d,X=%d,P=%d\n",z1,z2,tp->xd,Pressure);

#endif
		
        cmd =	(1u << ADSCTRL_PD0_SH) | (1u << ADSCTRL_PD1_SH) |
		(4u << ADSCTRL_ADR_SH) | (1u << ADSCTRL_STS_SH);
	/* Power-Down Enable */
	cmd &= ~(ADSCTRL_PD0_MSK | ADSCTRL_PD1_MSK);
	dummy = ads7846_rw(cmd);

#endif	// C700

#endif //////////////////////////////////////////////////////////////////////
}

static unsigned int read_xydata_sample(unsigned long cmd, int diff, int delay, int sn)
{
	unsigned int dummy, t1, t2, st;
	int i;

	dummy = ads7846_rw(cmd);
	udelay(1);
	t1 = ads7846_rw(cmd);
	udelay(1);
	t2 = ads7846_rw(cmd);
	if( ((signed)(t1 - t2) > diff) ||
	    ((signed)(t2 - t1) > diff) ){
//DBG*/		printk("TS:%d,%d,%d(%d)",dummy,t1,t2,t1-t2);
		return 0;
	}
	udelay(1+delay);

	t1 = (unsigned)-1;
	t2 = 0;
	st = 0;
	if( !sn ) sn = 1;
	for(i=0; i<sn; i++){
		unsigned int sd = ads7846_rw(cmd);
		st += sd;
		if( t1 > sd ) t1 = sd;
		if( t2 < sd ) t2 = sd;
		udelay(1);
	}
	if( sn > 3 ){
//DBG*/		if( (t2 - t1) > diff )
//DBG*/			printk("TS:%d-%d(%d),%d(%d)\n",t1,t2,(t2-t1),st,st/sn);
		st -= t1 + t2;
		sn -= 2;
	}
	return st / sn;
}

#ifdef IPAQ_COMPATIBLE
static void print_par(void)
{
     printk(" Kernel ==> cal_ok = %d\n",cal_ok);
     printk(" Kernel ==> raw_max_x = %d\n",raw_max_x);
     printk(" Kernel ==> raw_max_y = %d\n",raw_max_y);
     printk(" Kernel ==> res_x = %d\n",res_x);
     printk(" Kernel ==> res_y = %d\n",res_y);
     printk(" Kernel ==> raw_min_x = %d\n",raw_min_x);
     printk(" Kernel ==> raw_min_y = %d\n",raw_min_y);
     printk(" Kernel ==> xyswap = %d\n",xyswap);
     printk(" Kernel ==> x_rev = %d\n",x_rev);
     printk(" Kernel ==> y_rev = %d\n",y_rev);
}

void ts_clear(void)
{
    int i;
	
    for (i =0; i < BUFSIZE; i++) {
	 tbuf[i].pressure = 0;
	 tbuf[i].x = 0;
	 tbuf[i].y = 0;
	 tbuf[i].millisecs = 0;
    }
    head = tail = pendown = 0;
}

static TS_EVENT get_data(void)
{
   int last = tail;
   if (++tail == BUFSIZE) { tail = 0; }
   return tbuf[last];
}

static void new_data(void)
{
	int diff0, diff1, diff2, diff3;

#ifdef CONFIG_PM
	if( PowerDownMode != PMPD_MODE_ACTIVE )
		return;
#endif

	if (tc.x > X_AXIS_MAX || tc.x < X_AXIS_MIN ||
			tc.y > Y_AXIS_MAX || tc.y < Y_AXIS_MIN) {
		//printk("over range (%d,%d)\n",tc.x,tc.y);
		return;
	}
	if (tc.pressure) {
#if !defined(CONFIG_ARCH_PXA_CORGI) && !defined(CONFIG_ARCH_PXA_POODLE)
#if !defined(CONFIG_POODLE_TR0)
		if (sample < 3) {
			samples[sample].x = tc.x;
			samples[sample++].y = tc.y;
			return;
		}
		sample = 0;

		diff0 = abs(tc.x - samples[2].x);
		diff1 = abs(tc.y - samples[2].y);
		if (diff0 == 0 && diff1 == 0) {
			return;
		}
		/* Check the variance between X samples (discard if not
		   similar), then choose the closest pair */
		diff0 = abs(samples[0].x - samples[1].x);
		diff1 = abs(samples[1].x - samples[2].x);
		diff2 = abs(samples[2].x - tc.x);
		diff3 = abs(tc.x - samples[1].x);

		if (diff0 > XLIMIT || diff1 > XLIMIT || diff2 > XLIMIT
				|| diff3 > XLIMIT) {
			return;
		}
		if (diff1 < diff2) {
			if (diff1 < diff3)
				tc.x = (samples[1].x + samples[2].x) / 2;
			else
				tc.x = (tc.x + samples[1].x) / 2;
		} else {
			if (diff2 < diff3)
				tc.x = (samples[2].x + tc.x) / 2;
			else
				tc.x = (tc.x + samples[1].x) / 2;
		}

		/* Do the same for Y */
		diff0 = abs(samples[0].y - samples[1].y);
		diff1 = abs(samples[1].y - samples[2].y);
		diff2 = abs(samples[2].y - tc.y);
		diff3 = abs(tc.y - samples[1].y);

		if (diff0 > YLIMIT || diff1 > YLIMIT || diff2 > YLIMIT
					|| diff3 > YLIMIT) {
			return;
		}
		if (diff1 < diff2) {
			if (diff1 < diff3)
				tc.y = (samples[1].y + samples[2].y) / 2;
			else
				tc.y = (tc.y + samples[1].y) / 2;
		} else {
			if (diff2 < diff3)
				tc.y = (samples[2].y + tc.y) / 2;
			else
				tc.y = (tc.y + samples[1].y) / 2;
		}
#endif
#endif
	} else
		if (pendown == 0) {
			return;
		}
#if !defined(CONFIG_ARCH_PXA_CORGI) && !defined(CONFIG_ARCH_PXA_POODLE)
#if !defined(CONFIG_POODLE_TR0)
		else sample = 0;
#endif
#endif

	tc.millisecs = jiffies;
#if defined(CONFIG_ARCH_PXA_CORGI)
	if (tc.pressure == 0) {
		if (ispass == 0) {
			samples[0].pressure = 0;
			tbuf[head++] = samples[0];
			if (head >= BUFSIZE) { head = 0; }
			if (head == tail && ++tail >= BUFSIZE) { tail = 0; }
		}
		ispass = 1;
	}
	else {
		if (ispass == 0) {
			tbuf[head++] = samples[0];
			samples[0] = tc;
			if (head >= BUFSIZE) { head = 0; }
			if (head == tail && ++tail >= BUFSIZE) { tail = 0; }
		}
		else if (ispass == 1) {
			ispass = 2;
		}
		else {
			ispass = 0;
			samples[0] = tc;
		}
	}
#else
	tbuf[head++] = tc;
	if (head >= BUFSIZE) { head = 0; }
	if (head == tail && ++tail >= BUFSIZE) { tail = 0; }
#endif
	if (fasync)
		kill_fasync(&fasync, SIGIO, POLL_IN);
	wake_up_interruptible(&queue);
}
#endif

static void cotulla_main_ts_timer(unsigned long irq)
{
//	ts_interrupt(irq, NULL, NULL);
//	touch_panel_intr++;
	ts_interrupt_main(irq, 1);
}

static void ts_interrupt(int irq, void *dev_id, struct pt_regs *regs)
{
//	touch_panel_intr++;
	ts_interrupt_main(irq, 0);
}

static void ts_interrupt_main(int irq, int isTimer)
{
	ts_pos_t pos_dt;
	static TS_EVENT before_data = { pressure: 0 };
#if defined(CONFIG_ARCH_PXA_POODLE) && !defined(CONFIG_POODLE_TR0)
	if ( !isTimer || Pressure ) {
#else
	if ((GPLR(GPIO_TP_INT) & GPIO_bit(GPIO_TP_INT)) != GPIO_bit(GPIO_TP_INT)) {
#endif
		/* critical section */
		cli();
		/* Disable Falling Edge */
		GFER(GPIO_TP_INT) &= ~GPIO_bit(GPIO_TP_INT);
		read_xydata(&pos_dt);
//		sti(); /* move: TouchPanel SpeedUp */
		GEDR(GPIO_TP_INT) = GPIO_bit(GPIO_TP_INT); /* Clear detect */
		sti();
//		if( GPLR(GPIO_TP_INT) & GPIO_bit(GPIO_TP_INT) )
//			printk("!(%d,%d)",pos_dt.xd,pos_dt.yd);
		tc.x = pos_dt.xd;
		tc.y = pos_dt.yd;
#if defined(CONFIG_ARCH_PXA_POODLE)
		lock_FCS(POWER_MODE_TOUCH, 1);	// not enter FCS mode.
#if defined(CONFIG_POODLE_TR0)
		if( pos_dt.xd && pos_dt.yd ){
#else
//		if( pos_dt.xd && pos_dt.yd && Pressure ){
		if( pos_dt.xd && pos_dt.yd && Pressure && isTimer ){
#endif
			tc.pressure = 500;
#elif defined(CONFIG_ARCH_PXA_CORGI)
		if( pos_dt.xd && pos_dt.yd && Pressure ) {
			tc.pressure = 500;
#else
		if( pos_dt.xd && pos_dt.yd ){
			tc.pressure = 1;
#endif
			before_data = tc;
			pendown = 1;
			new_data();
		}
#if defined(CONFIG_ARCH_PXA_POODLE) && !defined(CONFIG_POODLE_TR0)
//		else if( !pendown )
		else if( !isTimer )
			Pressure = 1;
#endif
		mod_timer(&tp_main_timer, jiffies + HZ / 100);
	} else {
#if !defined(CONFIG_ARCH_PXA_POODLE) || defined(CONFIG_POODLE_TR0)
#if defined(CONFIG_ARCH_PXA_CORGI)
		if( pendown == 1 || pendown == 2 ){
#else
		if( pendown == 1 ){
#endif
			mod_timer(&tp_main_timer, jiffies + HZ / 100);
			pendown ++;
			return;
		}
#endif
		if (pendown) {
			tc = before_data;
			tc.pressure = 0;
			new_data();
		}
#if defined(CONFIG_ARCH_PXA_POODLE)
		lock_FCS(POWER_MODE_TOUCH, 0);	// enter FCS mode.
#endif
		/* Clear detect */
		GEDR(GPIO_TP_INT) = GPIO_bit(GPIO_TP_INT);
		/* Enable Falling Edge */
		GFER(GPIO_TP_INT) |= GPIO_bit(GPIO_TP_INT);
		pendown = 0;
#ifdef CONFIG_PM
		PowerDownMode = PMPD_MODE_ACTIVE;
#endif
	}
}

static void ts_hardware_init(void)
{
	unsigned int dummy;
	
#if defined(CONFIG_ARCH_PXA_POODLE) || defined(CONFIG_ARCH_PXA_CORGI)
	pxa_ssp_init();
#else
	/* Enable the Synchronous Serial Port clock */
	CKEN |= CKEN3_SSP;

	SSCR0 = 0x1ab;
	SSCR1 = 0;
#endif

/*
	SSCR0 = (0x0b << SSCR0_DSS) | (2 << SSCR0_FRF) | (0 << SSCR0_ECS) |
		(1 << SSCR0_SSE) | (2 << SSCR0_SCR);

	SSCR1 = (0 << SSCR1_RIE) | (0 << SSCR1_TIE) | (0 << SSCR1_LBM) |
		(0 << SSCR1_SPO) | (0 << SSCR1_SPH) | (0 << SSCR1_MWDS) |
		(3 << SSCR1_TFT) | (3 << SSCR1_RFT);
*/


	/* Initiaize ADS7846 Difference Reference mode */
	dummy=ads7846_rw((1u << ADSCTRL_ADR_SH) | (1u << ADSCTRL_STS_SH));
	mdelay(5);
	dummy=ads7846_rw((3u << ADSCTRL_ADR_SH) | (1u << ADSCTRL_STS_SH));
	mdelay(5);
	dummy=ads7846_rw((4u << ADSCTRL_ADR_SH) | (1u << ADSCTRL_STS_SH));
	mdelay(5);
	dummy=ads7846_rw((5u << ADSCTRL_ADR_SH) | (1u << ADSCTRL_STS_SH));
	mdelay(5);

	/* Enable Falling Edge */
#if 0
	GFER(GPIO_TP_INT) |= GPIO_bit(GPIO_TP_INT);
#else
	set_GPIO_IRQ_edge(GPIO_TP_INT, GPIO_FALLING_EDGE);
#endif
	/* Clear detect */
	GEDR(GPIO_TP_INT) = GPIO_bit(GPIO_TP_INT);
}


static int ts_open(struct inode *, struct file *);
static int ts_release(struct inode *, struct file *);
static int ts_fasync(int fd, struct file *filp, int on);
static ssize_t ts_read(struct file *, char *, size_t, loff_t *);
static ssize_t ts_write(struct file *, const char *, size_t, loff_t *);
static unsigned int ts_poll(struct file *, poll_table *);
static int ts_ioctl(struct inode *, struct file *, unsigned int, unsigned long);

struct file_operations ts_fops = {
	open:		ts_open,
	release:	ts_release,
	fasync:		ts_fasync,
	read:		ts_read,
	write:		ts_write,
	poll:		ts_poll,
	ioctl:		ts_ioctl,
};

int ts_cotulla_cleanup(void)
{
	unregister_chrdev(TS_MAJOR, "ts");
	free_irq(IRQ_GPIO_TP_INT, NULL);
	return 0;
}

int ts_cotulla_init(void)
{
        if ( register_chrdev(TS_MAJOR,"ts",&ts_fops) )
                printk("unable to get major %d for touch screen\n", TS_MAJOR);

	ts_clear();
	raw_max_x = X_AXIS_MAX;
	raw_max_y = Y_AXIS_MAX;
	raw_min_x = X_AXIS_MIN;
	raw_min_y = Y_AXIS_MIN;
#if defined(CONFIG_ARCH_PXA_CORGI)
#ifdef USE_SL5500_QT
	res_x = 480;
	res_y = 640;

	cal_ok = 1;
	x_rev = 1;
	y_rev = 0;
	xyswap = 1;
#else
	res_x = 480;
	res_y = 640;

	cal_ok = 1;
	x_rev = 1;
	y_rev = 0;
	xyswap = 1;
#endif
#else
#ifdef USE_SL5500_QT
	res_x = 240;
	res_y = 320;

	cal_ok = 1;

#if defined(CONFIG_ARCH_SHARP_SL_J)
	x_rev = 1;
	y_rev = 0;
#else
	x_rev = 0;
	y_rev = 1;
#endif
	xyswap = 1;
#else
	res_x = 240;
	res_y = 320;

	cal_ok = 1;
	x_rev = 1;
	y_rev = 1;
	xyswap = 0;
#endif
#endif

	ts_hardware_init();
	init_timer(&tp_main_timer);
	tp_main_timer.data = IRQ_GPIO(6);	// ???
	tp_main_timer.function = cotulla_main_ts_timer;

	if (request_irq(IRQ_GPIO_TP_INT, ts_interrupt, SA_INTERRUPT, "ts", NULL)) {
		return -EBUSY;
	}

#ifdef CONFIG_PM
	touch_pm_dev = pm_register(PM_COTULLA_DEV, 0, cotulla_touch_pm_callback);
#endif

	printk("Cotulla Touch Screen driver initialized\n");

	return 0;
}

module_init(ts_cotulla_init);
module_exit(ts_cotulla_cleanup);


static int ts_open(struct inode *inode, struct file *file)
{
	kdev_t dev = inode->i_rdev;
	if( MINOR(dev) == 1 )
		ts_type = 0;
	else 	
		ts_type = 1;

	return 0;
}


static int ts_release(struct inode *inode, struct file *file)
{
	ts_fasync(-1, file, 0);
	return 0;
}

static int ts_fasync(int fd, struct file *filp, int on)
{
	int retval;

	retval = fasync_helper(fd, filp, on, &fasync);
	if (retval < 0)
		return retval;
	return 0;
}

static ssize_t ts_read(struct file *file, char *buffer, size_t count, loff_t *ppos)
{
	DECLARE_WAITQUEUE(wait, current);
	TS_EVENT t, r;
	int i;
	
	if (head == tail) {
		if (file->f_flags & O_NONBLOCK) {
			return -EAGAIN;
		}
		add_wait_queue(&queue, &wait);
		current->state = TASK_INTERRUPTIBLE;
		while ((head == tail)&& !signal_pending(current)) {
			schedule();
			current->state = TASK_INTERRUPTIBLE;
		}
		current->state = TASK_RUNNING;
		remove_wait_queue(&queue, &wait);
	}
	
	for (i = count ; i >= sizeof(TS_EVENT);
			i -= sizeof(TS_EVENT), buffer += sizeof(TS_EVENT)) {
		if (head == tail)
			break;
		t = get_data();

		if (xyswap) {
		    short tmp = t.x;
		    t.x = t.y;
		    t.y = tmp;
		}
		if (cal_ok) {
			r.x = (x_rev) ? ((raw_max_x - t.x) * res_x) / (raw_max_x - raw_min_x)
		 		: ((t.x - raw_min_x) * res_x) / (raw_max_x - raw_min_x);
			r.y = (y_rev) ? ((raw_max_y - t.y) * res_y) / (raw_max_y - raw_min_y)
				: ((t.y - raw_min_y) * res_y) / (raw_max_y - raw_min_y);
		} else {
			r.x = t.x;
			r.y = t.y;
		}

		r.pressure = t.pressure;
		r.millisecs = t.millisecs;
		//printk("ts:ts_read(x, y, p) = (%d, %d, %d)\n", (unsigned int)r.x, (unsigned int)r.y, (unsigned int)r.pressure);

		copy_to_user(buffer,&r,sizeof(TS_EVENT));
	}
	return count - i;
}

static ssize_t	ts_write(struct file *file, const char *buffer, size_t count, loff_t *ppos)
{
#if defined(CONFIG_ARCH_PXA_CORGI)
	unsigned long param;
	char *endp;
	wait_after_sync_hs = simple_strtoul(buffer, &endp, 0);
	return count+endp-buffer;
#else
	return 0;
#endif
}

static unsigned int ts_poll(struct file *filp, poll_table *wait)
{
	poll_wait(filp, &queue, wait);
	if (head != tail)
		return POLLIN | POLLRDNORM;
	return 0;
}

static int ts_ioctl(struct inode *inode, struct file *file, unsigned int cmd, unsigned long arg)
{
	switch(cmd){
        case 3:
            raw_max_x = arg;
            break;
        case 4:
            raw_max_y = arg;
            break;
        case 5:
            res_x = arg;
            break;
        case 6:
            res_y = arg;
            break;
        case 10:
            raw_min_x = arg;
            break;
        case 11:
            raw_min_y = arg;
            break;
        case 12:            /* New attribute for portrait modes */
            xyswap = arg;
        case 13:         /* 0 = Enable calibration ; 1 = Calibration OK */
            cal_ok = arg;
        case 14:         /* Clear all buffer data */
	    ts_clear();
	    break;	
	case 15:         /* X axis reversed setting */
	    x_rev = arg;
	    break;
	case 16:         /* Y axis reversed setting */
            y_rev = arg;
	    break;
	case 17:         /* Clear all buffer data */
	    print_par();
	    break;
	default:
		return -EINVAL;
	}

	return 0;
}

#ifdef CONFIG_PM
void cotulla_touch_suspend() {
	unsigned long cmd;
	unsigned int dummy;

	GFER(GPIO_TP_INT) &= ~GPIO_bit(GPIO_TP_INT);
	if( pendown ){
		del_timer(&tp_main_timer);
		tc.pressure = 0;
		new_data();
		pendown = 0;
	}
	PowerDownMode = PMPD_MODE_SUSPEND;

	cmd = (1u << ADSCTRL_ADR_SH) | (1u << ADSCTRL_STS_SH);

	dummy = ads7846_rw(cmd);

#ifdef CONFIG_SABINAL_DISCOVERY
	SSCR0 = 0x2b;

	CKEN &= ~CKEN3_SSP;
#endif
	GEDR(GPIO_TP_INT) = GPIO_bit(GPIO_TP_INT);

}

void cotulla_touch_resume() {
	unsigned long cmd;
	unsigned int dummy;

#if defined(CONFIG_ARCH_PXA_POODLE) || defined(CONFIG_ARCH_PXA_CORGI)
	pxa_ssp_init();
#else
	CKEN |= CKEN3_SSP;
	SSCR0 = 0x1ab;
	SSCR1 = 0;
#endif
#if 0
	GEDR(GPIO_TP_INT) = GPIO_bit(GPIO_TP_INT);
	GFER(GPIO_TP_INT) |= GPIO_bit(GPIO_TP_INT);
//	GEDR(GPIO_TP_INT) = GPIO_bit(GPIO_TP_INT);		
#else
	PowerDownMode = PMPD_MODE_RESUME;
	mod_timer(&tp_main_timer, jiffies + HZ / 10);
#endif

	cmd =   (1u << ADSCTRL_PD0_SH) | (1u << ADSCTRL_PD1_SH) |
		(4u << ADSCTRL_ADR_SH) | (1u << ADSCTRL_STS_SH);
	cmd &= ~(ADSCTRL_PD0_MSK | ADSCTRL_PD1_MSK);
	dummy = ads7846_rw(cmd);
}

static int cotulla_touch_pm_callback(struct pm_dev* pm_dev,
			       pm_request_t req, void* data)
{
	switch (req) {
	case PM_SUSPEND:
		cotulla_touch_suspend();
		break;
		
	case PM_RESUME:
		cotulla_touch_resume();
		break;
		
	}

	return 0;
}
#endif
