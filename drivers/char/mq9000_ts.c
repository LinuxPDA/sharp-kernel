/*
 *      mq9000_ts.c  --  Touch screen driver for the MQ9000 SPI to the 
 *			 ADS7846 touch screen controller.
 *
 * 	Copyright 2002 MediaQ Inc.
 * 	Author: MediaQ Inc.
 *
 *  This program is free software; you can redistribute  it and/or modify it
 *  under  the terms of  the GNU General  Public License as published by the
 *  Free Software Foundation;  either version 2 of the  License, or (at your
 *  option) any later version.
 *
 *  THIS  SOFTWARE  IS PROVIDED   ``AS  IS'' AND   ANY  EXPRESS OR IMPLIED
 *  WARRANTIES,   INCLUDING, BUT NOT  LIMITED  TO, THE IMPLIED WARRANTIES OF
 *  MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.  IN
 *  NO  EVENT  SHALL   THE AUTHOR  BE    LIABLE FOR ANY   DIRECT, INDIRECT,
 *  INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 *  NOT LIMITED   TO, PROCUREMENT OF  SUBSTITUTE GOODS  OR SERVICES; LOSS OF
 *  USE, DATA,  OR PROFITS; OR  BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
 *  ANY THEORY OF LIABILITY, WHETHER IN  CONTRACT, STRICT LIABILITY, OR TORT
 *  (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 *  THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 *  You should have received a copy of the  GNU General Public License along
 *  with this program; if not, write  to the Free Software Foundation, Inc.,
 *  675 Mass Ave, Cambridge, MA 02139, USA.
 *
 * Notes:
 *
 */
#define FILTER_ON		1	// Trun on the data fielter

//#define USE_GPIO_SPI

#include <linux/module.h>
#include <linux/version.h>

#include <linux/init.h>
#include <linux/fs.h>
#include <linux/delay.h>
#include <linux/poll.h>
#include <linux/string.h>
#include <linux/ioport.h>       /* request_region */
#include <linux/interrupt.h>    /* mark_bh */

#include <asm/irq.h>
#include <asm/arch/platform.h>
#include <asm/arch/hardware.h>
#include <asm/arch/irqs.h>
#include <asm/arch/MQK_spi.h>


#define TS_NAME "MQ9000-ts"
#define TS_MAJOR 11

#define PFX TS_NAME

#define err(format, arg...) printk(KERN_ERR PFX ": " format "\n" , ## arg)
//#define info(format, arg...) printk(KERN_INFO PFX ": " format "\n" , ## arg)
//#define info(format, arg...) printk( format "\n" , ## arg)
#define info(format, arg...) 
#define info1(format, arg...)
//#define info1(format, arg...) printk( format "\n" , ## arg)
#define info2(format, arg...)
//#define info2(format, arg...) printk( format "\n" , ## arg)
#define warn(format, arg...) printk(KERN_WARNING PFX ": " format "\n" , ## arg)


// SSI Status register bit defines
#define SSISTAT_BF    (1<<4)
#define SSISTAT_OF    (1<<3)
#define SSISTAT_UF    (1<<2)
#define SSISTAT_DONE  (1<<1)
#define SSISTAT_BUSY  (1<<0)

// SSI Interrupt Pending and Enable register bit defines
#define SSIINT_OI     (1<<3)
#define SSIINT_UI     (1<<2)
#define SSIINT_DI     (1<<1)

// SSI Address/Data register bit defines
#define SSIADAT_D         (1<<24)
#define SSIADAT_ADDR_BIT  16
#define SSIADAT_ADDR_MASK (0xff<<SSIADAT_ADDR_BIT)
#define SSIADAT_DATA_BIT  0
#define SSIADAT_DATA_MASK (0xfff<<SSIADAT_DATA_BIT)

// SSI Enable register bit defines
#define SSIEN_CD (1<<1)
#define SSIEN_E  (1<<0)

// SSI Config register bit defines
#define SSICFG_AO (1<<24)
#define SSICFG_DO (1<<23)
#define SSICFG_ALEN_BIT 20
#define SSICFG_ALEN_MASK (0x7<<SSICFG_ALEN_BIT)
#define SSICFG_DLEN_BIT 16
#define SSICFG_DLEN_MASK (0xf<<SSICFG_DLEN_BIT)
#define SSICFG_DD (1<<11)
#define SSICFG_AD (1<<10)
#define SSICFG_BM_BIT 8
#define SSICFG_BM_MASK (0x3<<SSICFG_BM_BIT)
#define SSICFG_CE (1<<7)
#define SSICFG_DP (1<<6)
#define SSICFG_DL (1<<5)
#define SSICFG_EP (1<<4)

// Bus Turnaround Selection
#define SCLK_HOLD_HIGH 0
#define SCLK_HOLD_LOW  1
#define SCLK_CYCLE     2

/*
 * Default config for SSI0:
 *
 *   - transmit MSBit first
 *   - expect MSBit first on data receive
 *   - address length 7 bits
 *   - expect data length 12 bits
 *   - do not disable Direction bit
 *   - do not disable Address bits
 *   - SCLK held low during bus turnaround
 *   - Address and Data bits clocked out on falling edge of SCLK
 *   - Direction bit high is a read, low is a write
 *   - Direction bit precedes Address bits
 *   - Active low enable signal
 */

#define DEFAULT_SSI_CONFIG \
    (SSICFG_AO | SSICFG_DO | (6<<SSICFG_ALEN_BIT) | (11<<SSICFG_DLEN_BIT) |\
    (SCLK_HOLD_LOW<<SSICFG_BM_BIT) | SSICFG_DP | SSICFG_EP)


// ADS7846 Control Byte bit defines
#define ADS7846_ADDR_BIT  	4
#define ADS7846_ADDR_MASK 	(0x7<<ADS7846_ADDR_BIT)
#define ADS7846_MEASURE_X  	(0x5<<ADS7846_ADDR_BIT)
#define ADS7846_MEASURE_Y  	(0x1<<ADS7846_ADDR_BIT)
#define ADS7846_MEASURE_Z1 	(0x3<<ADS7846_ADDR_BIT)
#define ADS7846_MEASURE_Z2 	(0x4<<ADS7846_ADDR_BIT)
#define ADS7846_8BITS     	(1<<3)
#define ADS7846_12BITS    	0
#define ADS7846_SER       	(1<<2)
#define ADS7846_DFR       	0
#define ADS7846_PWR_BIT   	0
#define ADS7846_PD      	0
#define ADS7846_ADC_ON  	(0x1<<ADS7846_PWR_BIT)
#define ADS7846_REF_ON  	(0x2<<ADS7846_PWR_BIT)
#define ADS7846_REF_ADC_ON 	(0x3<<ADS7846_PWR_BIT)

#define MEASURE_12BIT_X \
    (ADS7846_MEASURE_X | ADS7846_12BITS | ADS7846_DFR | ADS7846_PD)
#define MEASURE_12BIT_Y \
    (ADS7846_MEASURE_Y | ADS7846_12BITS | ADS7846_DFR | ADS7846_PD)
#define MEASURE_12BIT_Z1 \
    (ADS7846_MEASURE_Z1 | ADS7846_12BITS | ADS7846_DFR | ADS7846_PD)
#define MEASURE_12BIT_Z2 \
    (ADS7846_MEASURE_Z2 | ADS7846_12BITS | ADS7846_DFR | ADS7846_PD)

#define MEASURE_INIT	0x90
#define MEASURE_X	0xD3
#define MEASURE_Y	0x93
#define MEASURE_Z1	0xB3
#define MEASURE_Z2	0xC3

typedef enum {
	IDLE = 0,
	ACQ_X,
	ACQ_Y,
	ACQ_Z1,
	ACQ_Z2
} acq_state_t;


//#define PANEL_4
#define PANEL_28

#ifdef PANEL_4

#define TS_DISPLAY_X	320
#define TS_DISPLAY_Y	240
#define TS_X_OFFSET	2280
#define TS_Y_OFFSET	160
#define TS_X_SCALE      TS_DISPLAY_X/(1810-130)
#define TS_Y_SCALE	TS_DISPLAY_Y/(1900-160)
#endif

#ifdef PANEL_28

#define TS_DISPLAY_X	240
#define TS_DISPLAY_Y	320
#define TS_X_OFFSET	130
#define TS_Y_OFFSET	(4096-1880)	
#define TS_X_SCALE      TS_DISPLAY_X/(1830-130)
#define TS_Y_SCALE	TS_DISPLAY_Y/(1880-130)

#endif


#define A300_QTOPIA
#if defined(A300_QTOPIA)
#define X_AXIS_MAX	1795
#define Y_AXIS_MAX	1895
#define X_AXIS_MIN	150
#define Y_AXIS_MIN	130
static int raw_max_x, raw_max_y, res_x, res_y, raw_min_x, raw_min_y, xyswap;
static int cal_ok, x_rev, y_rev;
#endif

/* +++++++++++++ Lifted from include/linux/h3600_ts.h ++++++++++++++*/
typedef struct {
	unsigned short pressure;  // touch pressure
	unsigned short x;         // calibrated X
	unsigned short y;         // calibrated Y
	unsigned short millisecs; // timestamp of this event
} TS_EVENT;

typedef struct {
	unsigned char status;
	unsigned char xh;
	unsigned char xl;
	unsigned char yh;
	unsigned char yl;
} C_TS_EVENT;

typedef struct {
	int xscale;
	int xtrans;
	int yscale;
	int ytrans;
	int xyswap;
} TS_CAL;

/* Use 'f' as magic number */
#define IOC_MAGIC  'f'

#define TS_GET_RATE             _IO(IOC_MAGIC, 8)
#define TS_SET_RATE             _IO(IOC_MAGIC, 9)
#define TS_GET_CAL              _IOR(IOC_MAGIC, 10, TS_CAL)
#define TS_SET_CAL              _IOW(IOC_MAGIC, 11, TS_CAL)

/* +++++++++++++ Done lifted from include/linux/h3600_ts.h +++++++++*/


#define EVENT_BUFSIZE 128

/*
 * Which pressure equation to use from ADS7846 datasheet.
 * The first equation requires knowing only the X plate
 * resistance, but needs 4 measurements (X, Y, Z1, Z2).
 * The second equation requires knowing both X and Y plate
 * resistance, but only needs 3 measurements (X, Y, Z1).
 * The second equation is preferred because of the shorter
 * acquisition time required.
 */
enum {
	PRESSURE_EQN_1 = 0,
	PRESSURE_EQN_2
};


/*
 * The touch screen's X and Y plate resistances, used by
 * pressure equations.
 */
#define DEFAULT_X_PLATE_OHMS 580
#define DEFAULT_Y_PLATE_OHMS 580

/*
 * Pen up/down pressure resistance thresholds.
 *
 * FIXME: these are bogus and will have to be found empirically.
 *
 * These are hysteresis points. If pen state is up and pressure
 * is greater than pen-down threshold, pen transitions to down.
 * If pen state is down and pressure is less than pen-up threshold,
 * pen transitions to up. If pressure is in-between, pen status
 * doesn't change.
 *
 * This wouldn't be needed if PENIRQ* from the ADS7846 were
 * routed to an interrupt line on the Au1000. This would issue
 * an interrupt when the panel is touched.
 */
#define DEFAULT_PENDOWN_THRESH_OHMS 100
#define DEFAULT_PENUP_THRESH_OHMS    80

typedef struct {
	int baudrate;
	u32 clkdiv;
	acq_state_t acq_state;            // State of acquisition state machine
	int x_raw, y_raw, z1_raw, z2_raw; // The current raw acquisition values
	TS_CAL cal;                       // Calibration values
	// The X and Y plate resistance, needed to calculate pressure
	int x_plate_ohms, y_plate_ohms;
	// pressure resistance at which pen is considered down/up
	int pendown_thresh_ohms;
	int penup_thresh_ohms;
	int pressure_eqn;                 // eqn to use for pressure calc
	int pendown;                      // 1 = pen is down, 0 = pen is up
	TS_EVENT event_buf[EVENT_BUFSIZE];// The event queue
	int nextIn, nextOut;
	int event_count;
	struct fasync_struct *fasync;     // asynch notification
	struct timer_list acq_timer;      // Timer for triggering acquisitions
	wait_queue_head_t wait;           // read wait queue
	spinlock_t lock;
	struct tq_struct chug_tq;
	int x_raw0, y_raw0, z1_raw0, z2_raw0; // The current raw acquisition values
	int prvX,prvY;
	int acq_count;
	int ts_pendown;
	int pendown_sent;
	int skipCount;
} MQ9000_ts_t;

static MQ9000_ts_t MQ9000_ts;

void sp_regs(void);
void SetupSPI(void);


#ifdef FILTER_ON	
#define MAX_Z			99999999
#define PRESSURE_THRESHOLD	30000
#define PEN_DOWN_MIN_SAMPLES	4

#define QT_QWS_MOVE_LIMIT	100
#define QT_QWS_JITTER_LIMIT	2	
#define QT_QWS_SKIP_COUNT	2

void	DataFilter(MQ9000_ts_t *ts)
{
	int dx,dy, dxSqr, dySqr;

	if(ts->x_raw == 0)
		return;

	if(ts->y_raw == 0)
	{
		if(!(ts->ts_pendown) && ts->pendown_sent)
		{
			ts->pendown_sent = 0;
			// send pen up signal
			// if it is pen up
			ts->x_raw = ts->prvX;
			ts->y_raw = ts->prvY;	
			ts->z2_raw = 0x0; // 
			info2("f u %d %d",ts->x_raw,ts->y_raw); 
		}
		return;
	}


	if(ts->ts_pendown)
	{
		#if 1 // New Try
			dx = (ts->x_raw - ts->prvX) >> 1;
			dy = (ts->y_raw - ts->prvY) >> 1;
			dxSqr = dx*dx;
			dySqr = dy*dy;
		#endif

		// if it is pen down
		if(ts->acq_count == 1)
		{
			// this is the first input
			ts->prvX = ts->x_raw;
			ts->prvY = ts->x_raw;
			ts->x_raw = 0; // skip this data
			info2("f1"); 
		}
		else
		{
			info2("f%d %d, %d",ts->acq_count,dxSqr, dySqr); 
		#if 1 // New Try
			ts->prvX = (ts->prvX + ts->x_raw) >> 1;
			ts->prvY = (ts->prvY + ts->y_raw) >> 1;

			if(ts->acq_count < PEN_DOWN_MIN_SAMPLES)
			{
				ts->x_raw = 0; // skip this data
				ts->pendown_sent = 0;
				return;
			}
			
			if(dxSqr +dySqr > QT_QWS_MOVE_LIMIT * QT_QWS_MOVE_LIMIT)
			{
				// Skip this data completely
				ts->x_raw = 0;
				return;
			}
			
			if( (dxSqr +dySqr > QT_QWS_JITTER_LIMIT * QT_QWS_JITTER_LIMIT) || (ts->skipCount > QT_QWS_SKIP_COUNT))
			{
				ts->x_raw = ts->prvX;
				ts->y_raw = ts->prvY;	
				ts->z2_raw = 0x40; // first pen down action
				ts->pendown_sent = 1;
				ts->skipCount = 0;
				info2("f d %d %d",ts->x_raw,ts->y_raw); 
				return;
			}
			else
			{
				// Skip this data completely
				ts->x_raw = 0;
				ts->skipCount++;
				return;
			}
				
		#else

			ts->prvX = (ts->prvX + ts->x_raw) >> 1;
			ts->prvY = (ts->prvY + ts->y_raw) >> 1;

			if(ts->acq_count < PEN_DOWN_MIN_SAMPLES)
			{
				ts->x_raw = 0; // skip this data
				ts->pendown_sent = 0;
				return;
			}
			
			ts->x_raw = ts->prvX;
			ts->y_raw = ts->prvY;	
			if(ts->acq_count == PEN_DOWN_MIN_SAMPLES)
			{
				ts->z2_raw = 0x40; // first pen down action
				ts->pendown_sent = 1;
				info2("f d %d %d",ts->x_raw,ts->y_raw); 
			}
#if 0
			if(ts->acq_count == PEN_DOWN_MIN_SAMPLES+30)
			{
				ts->z2_raw = 0x40; // first pen down action
				info2("f d %d %d",ts->x_raw,ts->y_raw); 
			}
#endif
			else
			{
				ts->z2_raw = 0x00; // pen still down
				info2("f h %d %d",ts->x_raw,ts->y_raw); 
			}
		#endif
		}
	}
	else
	{
		if(ts->pendown_sent)
		{
			ts->pendown_sent = 0;
			// send pen up signal
			// if it is pen up
			ts->x_raw = ts->prvX;
			ts->y_raw = ts->prvY;	
			ts->z2_raw = 0x0; // pen up action
			info2("f u %d %d",ts->x_raw,ts->y_raw); 
		}

	}
}
void	CheckPressure(MQ9000_ts_t *ts)
{
	if(ts->z1_raw ==0)
		ts->z1_raw = MAX_Z;
	else
		ts->z1_raw = ts->x_raw * (0x1000/ts->z1_raw -1) - ts->y_raw;

	if(ts->z1_raw > PRESSURE_THRESHOLD)
	{
		//pen up
		ts->y_raw = 0;	
		info2("p0"); 
	}
	else
	{	// pen down

	}
}
#else

#define DataFilter(a)
#define CheckPressure(a)

#endif


#ifdef USE_GPIO_SPI
#define MQ_WAIT_1MS	19
void MQ_Wait(unsigned int delay_millis)
{
#if 0
	unsigned int j,i,k;
#endif
	unsigned int j;

	for(j=0; j< delay_millis; ++j)
	{
	#if 0
		for(i=0; i< MQ_WAIT_1MS; ++i)
		{
			for(k=0; k< 1000; ++k)
			{
			}
		}
	#endif
	}
}
#define MQ_Wait1(a) 
#if 0
void MQ_Wait1(unsigned int delay_millis)
{
	#if 0
	ULONG j,i,k;

	for(j=0; j< 2; ++j)
	{
		for(i=0; i< 2; ++i)
		{
		}
	}
	#endif
}
#endif
#if 1
#define DOUT(data)	\
	{\
	if(data)\
		pMQRegs->sp.sp04.GpioDataDir |= MISO_DATA_HIGH;\
	else\
		pMQRegs->sp.sp04.GpioDataDir &= ~MISO_DATA_HIGH;\
	}
#define DIN() (unsigned char )((pMQRegs->sp.sp04.GpioDataDir & MOSI_DATA_IN)>>22)
#define DCLK(data) \
	{\
		if(data)\
			pMQRegs->sp.sp04.GpioDataDir |= SCK_DATA_HIGH;\
		else\
			pMQRegs->sp.sp04.GpioDataDir &= ~SCK_DATA_HIGH;\
	}

#else
void DOUT(unsigned data)
{
	volatile MQ9000REGS	*pMQRegs = (MQ9000REGS *)MQ9000_REGS_VBASE;

	if(data)
		pMQRegs->sp.sp04.GpioDataDir |= MISO_DATA_HIGH;
	else
		pMQRegs->sp.sp04.GpioDataDir &= ~MISO_DATA_HIGH;
}

unsigned char DIN()
{
	volatile MQ9000REGS	*pMQRegs = (MQ9000REGS *)MQ9000_REGS_VBASE;
	unsigned char data8;

	data8 = (unsigned char )((pMQRegs->sp.sp04.GpioDataDir & MOSI_DATA_IN)>>22);

	return data8;
}


void DCLK(unsigned char data)
{
	volatile MQ9000REGS	*pMQRegs = (MQ9000REGS *)MQ9000_REGS_VBASE;

	if(data)
		pMQRegs->sp.sp04.GpioDataDir |= SCK_DATA_HIGH;
	else
		pMQRegs->sp.sp04.GpioDataDir &= ~SCK_DATA_HIGH;
}
#endif

void GPIO_SPI_8BITS(unsigned char DataOut, unsigned char *pDataIn)
{
	volatile MQ9000REGS	*pMQRegs = (MQ9000REGS *)MQ9000_REGS_VBASE;
#if 0
	unsigned int i,j;
#endif
	unsigned int i;
	*pDataIn = 0;

	for(i = 0; i< 8; ++i)
	{
		*pDataIn <<= 1;
		DOUT((DataOut &0x80 ? 1 : 0));
		DataOut <<= 1;
		MQ_Wait1(1);
		DCLK(1);
		*pDataIn |= DIN();
		MQ_Wait1(1);
		DCLK(0);
	}
//#ifdef SYS_TEST
#if 0
	for(i=0; i< 10; ++i)
	{
		for(j=0; j<5; ++j)
		{

		}
	}
#endif
}
void GPIO_SPI(unsigned short int channel, unsigned char *pData1, unsigned char *pData2)
{
#if 1 // Sample Twice

	GPIO_SPI_8BITS((unsigned char)(channel & 0xFF), pData1);
	GPIO_SPI_8BITS((unsigned char)(0), pData1);
	GPIO_SPI_8BITS((unsigned char)(channel & 0xFF), pData1);
	GPIO_SPI_8BITS((unsigned char)(0), pData1);
	GPIO_SPI_8BITS((unsigned char)(0), pData2);
	
#else
	GPIO_SPI_8BITS((unsigned char)(channel & 0xFF), pData1);
	GPIO_SPI_8BITS((unsigned char)((channel>>8) & 0xFF), pData1);
	GPIO_SPI_8BITS((unsigned char)(0), pData2);
#endif
}
#endif 

// It shoud be always positive.
int ReadADS7846E(unsigned short channel)
{
	volatile MQ9000REGS	*pMQRegs = (MQ9000REGS *)MQ9000_REGS_VBASE;
	unsigned char		bData1,bData2;
	unsigned int		adcData = 0x00;  // insert dummy data here

//	info("ReadADS7846E(0x%04x)", channel);
#ifdef USE_GPIO_SPI

	pMQRegs->sp.sp04.GpioDataDir	&= 	0xFFFFFFF7;
	MQ_Wait1(1);

	GPIO_SPI(channel, &bData1, &bData2);

	adcData	= (((unsigned int)bData1)<<8) | ((unsigned int)bData2) ;
	MQ_Wait1(1);
	// Disable SPI SS
	pMQRegs->sp.sp04.GpioDataDir	|= 	0x00000008;

#else // SPI mode
	// Enable SPI SS
	pMQRegs->sp.sp04.GpioDataDir	&= 	0xFFFFFFF7;
	
	pMQRegs->sp.sp02.sp02R16[0]		= 	channel;
	pMQRegs->sp.sp02.sp02R16[0]		= 	0;
	pMQRegs->sp.sp02.sp02R16[0]		= 	0;
	while( ( (pMQRegs->sp.sp06.Status & RV_FIFO_COUNT_MASK)>> 24 ) != 3)
	{
			//Wait for data ready
			// Sleep(1);
		//MQ_HwrDelay(1);	
	  mdelay(1);
	}

	bData1	= pMQRegs->sp.sp02.sp02R8[0];
	bData1	= pMQRegs->sp.sp02.sp02R8[0];
	bData2	= pMQRegs->sp.sp02.sp02R8[0];
	adcData	= (((unsigned int)bData1)<<8) | ((unsigned int)bData2) ;


	// Disable SPI SS
	pMQRegs->sp.sp04.GpioDataDir	|= 	0x00000008;

#endif // USE_GPIO_SPI

//	info("ReadADS7846E(0x%04x)=0x%04x", channel,adcData);
	return (int)((adcData&0x0FFFF)>>4);
}

void sp_regs(void)
{
	//volatile MQ9000REGS	*pMQRegs = (MQ9000REGS *)MQ9000_REGS_VBASE;

	// ====================================
	// Monitor IN registers 
	// ====================================
	info("in00 Address =0x%08x", &(pMQRegs->in.in00.Control));
	info("in00 Control =0x%08x", pMQRegs->in.in00.Control);
	info("in01 IRQMask =0x%08x", pMQRegs->in.in01.IRQMask);
	info("in02 FIQMask =0x%08x", pMQRegs->in.in02.FIQMask);
	info("in04 RawStatus=0x%08x", pMQRegs->in.in04.RawStatus);
	info("in05 IRQStatus=0x%08x", pMQRegs->in.in05.IRQStatus);
	info("in06 FIQStatus=0x%08x", pMQRegs->in.in06.FIQStatus);

	// ====================================
	// Monitor Timer,GP registers 
	// ====================================
	info("gp00 Address	 =0x%08x", &(pMQRegs->gp.gp00.TimerCtrl));
	info("gp00.TimerCtrl	 =0x%08x", pMQRegs->gp.gp00.TimerCtrl);
	info("gp17.Gpio116_119	 =0x%08x", pMQRegs->gp.gp17.Gpio116_119);
	info("gp18.Gpio116_119Int=0x%08x", pMQRegs->gp.gp18.Gpio116_119Int);
	info("gp19.Gpio116_119IntStat=0x%08x", pMQRegs->gp.gp19.Gpio116_119IntStat);
	info("gp1A.GpioModeEnable    =0x%08x", pMQRegs->gp.gp1A.GpioModeEnable);

	// ====================================
	// Monitor SPI registers
	// ====================================
	info("sp00 Address	=0x%08x", &(pMQRegs->sp.sp00.Control));
	info("sp00.Control	=0x%08x", pMQRegs->sp.sp00.Control);
	info("sp01.Enable	=0x%08x", pMQRegs->sp.sp01.Enable);	
	///info("sp02.Data	=0x%08x", pMQRegs->sp.sp02.Data);
	info("sp03.TImeGapCmpr	=0x%08x", pMQRegs->sp.sp03.TimeGapCmpr);
	info("sp04.GpioDataDir	=0x%08x", pMQRegs->sp.sp04.GpioDataDir);
	info("sp05.DataCount	=0x%08x", pMQRegs->sp.sp05.DataCount);	
	info("sp06.Status	=0x%08x", pMQRegs->sp.sp06.Status);
	//info("sp07.LEDCntrl	=0x%08x", pMQRegs->sp.sp07.LEDCntrl);
}


#ifdef USE_GPIO_SPI
void SetupSPI(void)
{
	//Work around by using GPIO
	
	volatile MQ9000REGS	*pMQRegs = (MQ9000REGS *)MQ9000_REGS_VBASE;

	// program GPIO direction in SPI module
	pMQRegs->sp.sp00.Control 	= 0; 		// make sure it is in GPIO mode 
	pMQRegs->sp.sp04.GpioDataDir	= 
					( SS_OUT_ENABLE  	| 
					SCK_OUT_ENABLE 		|
					MISO_OUT_ENABLE		|
					MOSI_IN_ENABLE 		|
					PULLUP_REGS_INACTIVE 	|
					SS_DATA_HIGH 	
					);

	// Set polarity & Enable GPIO Mode
	set_GPIO_IRQ_edge(IRQ_TO_Timer_GPIO(IRQ_KATANATS), GPIO_FALLING_EDGE);
	// Enable GPIO119 input mode
	pMQRegs->gp.gp17.Gpio116_119		= (pMQRegs->gp.gp17.Gpio116_119 & ~(GPIO119_GP17R_BITS)) | (
											GPIO119_IN_ENABLE	); 	
}
#else
void SetupSPI()
{
	volatile MQ9000REGS	*pMQRegs = (MQ9000REGS *)MQ9000_REGS_VBASE;

	//======================
	//	GPIO_INIT();
	//======================
	
	pMQRegs->sp.sp00.Control 	= SRCLK_PLL1; 		// spiWREG32(SPIM_CNTRL_REG, SRCLK_PLL1);
	pMQRegs->sp.sp04.GpioDataDir	&= 0x000C0000; 		// spiWREG32(SPIM_GPIO_REG, spiRREG32(SPIM_GPIO_REG) & 0x000C0000); 
	pMQRegs->sp.sp04.GpioDataDir	|= (	SS_OUT_ENABLE 	| 
						SCK_OUT_ENABLE 	|
						MISO_IN_ENABLE 	|
						MOSI_OUT_ENABLE |
						PULLUP_REGS_INACTIVE |
						SS_DATA_HIGH 	|
						SCK_DATA_HIGH 	|
						MISO_DATA_HIGH 	|
						MOSI_DATA_HIGH 	);
    											
								// spiMOrREG32(SPIM_GPIO_REG, 
								// 		  SS_OUT_ENABLE | 
								// 		  SCK_OUT_ENABLE |
								// 		  MISO_IN_ENABLE | 
								// 		  MOSI_OUT_ENABLE | 
								// 		  PULLUP_REGS_INACTIVE |
								// 		  SS_DATA_HIGH | 
								// 		  SCK_DATA_HIGH | 
								// 		  MISO_DATA_HIGH | 
								// 		  MOSI_DATA_HIGH 
								// 		  ); 

#if 0 // T2, T3
	pMQRegs->cc.cc02.gpioControl0	|= GPIO2_IN_ENABLE;	//  ccMOrREG32(GPIO_CNTRL0_REG, GPIO2_IN_ENABLE);
	
#else // MQ9000
	// Set polarity & Enable GPIO Mode
	set_GPIO_IRQ_edge(IRQ_TO_Timer_GPIO(IRQ_KATANATS), GPIO_FALLING_EDGE);
	pMQRegs->gp.gp17.Gpio116_119		= (pMQRegs->gp.gp17.Gpio116_119 & ~(GPIO119_GP17R_BITS)) | (
							GPIO119_IN_ENABLE	); 	

#endif
	//======================
	//	SPI_INIT();
	//======================
	pMQRegs->sp.sp00.Control &= DTS_16BITS_MASK;		// spiMAndREG32(SPIM_CNTRL_REG, DTS_16BITS_MASK);
	pMQRegs->sp.sp00.Control |=(MASTER_ENABLE	|
					MSB_TX_FIRST 	|
					DTS_8BITS 	|
					SCK_POLARITY_LOW |
					SCK_PHASE_0 	|
					SINGLE_TX 		|
					SPI_FDx_12 		|
					SPI_SDx_12 		|
					TX_FIFO_SW_RESET_ENABLE |
					RV_FIFO_SW_RESET_ENABLE |
					SS_MODE ); 

							// spiMOrREG32(SPIM_CNTRL_REG, 
							// 				MASTER_ENABLE |
							// 				MSB_TX_FIRST | 
							// 				DTS_8BITS | 
							// 				SCK_POLARITY_LOW | 
							// 				SCK_PHASE_0 | 
							// 				SINGLE_TX | 
							// 				SPI_FDx_12 | 
							// 				SPI_SDx_2 | 
							// 				TX_FIFO_SW_RESET_ENABLE |
							// 				RV_FIFO_SW_RESET_ENABLE |
							// 				SS_MODE  
							// 				); 
	pMQRegs->sp.sp01.Enable	= ( TX_PAD_DISABLE |
		  			RV_FUNC_DISABLE |
		  			SPI_INT_DISABLE | 
          				UM_SPI_TX_COMPLETE);

							//	     spiWREG32(SPIM_ENABLE_MASK_REG,
							//					TX_PAD_DISABLE | 
							//					RV_FUNC_DISABLE |
							//					SPI_INT_DISABLE |
							//					UM_SPI_TX_COMPLETE 
							//					);
							//					

	pMQRegs->sp.sp00.Control &= 0xFFFFFFFC;			// spiMAndREG32(SPIM_CNTRL_REG, 0xFFFFFFFCL); 
	pMQRegs->sp.sp01.Enable	|= ( TX_PAD_ENABLE |
					RV_FUNC_ENABLE |
				SPI_TRANS_ENABLE | 
				SPI_MODE_ENABLE );

					//	     spiMOrREG32(SPIM_ENABLE_MASK_REG,
					//		  TX_PAD_ENABLE | 
					//		  RV_FUNC_ENABLE |
					//		  SPI_TRANS_ENABLE |
					//		  SPI_MODE_ENABLE 
					//		  );
					//
	pMQRegs->sp.sp03.TimeGapCmpr	=  0;					// spiWREG32(SPIM_TGCMP_REG, 1);

}
#endif // USE_GPIO_SPI

/*
 * This is a bottom-half handler that is scheduled after
 * raw X,Y,Z1,Z2 coordinates have been acquired, and does
 * the following:
 *
 *   - computes touch screen pressure resistance
 *   - if pressure is above a threshold considered to be pen-down:
 *         - compute calibrated X and Y coordinates
 *         - queue a new TS_EVENT
 *         - signal asynchronously and wake up any read
 */
static void
chug_raw_data(void* private)
{
	MQ9000_ts_t* ts = (MQ9000_ts_t*)private;
	int	x,y;
	int	pendown_event=0;

	TS_EVENT event;
#if 0
	int Rt, Xcal, Ycal;
#endif
	int Xcal, Ycal;
	unsigned long flags;

	info("chug_raw_data() jiffies=%d ",jiffies); 
	// timestamp this new event.
	event.millisecs = jiffies;

	CheckPressure(ts); 
	DataFilter(ts);

#if 0
	// Calculate touch pressure resistance
	if (ts->pressure_eqn == PRESSURE_EQN_2) {
		Rt = (ts->x_plate_ohms * ts->x_raw *
		      (4096 - ts->z1_raw)) / ts->z1_raw;
		Rt -= (ts->y_plate_ohms * ts->y_raw);
		Rt = (Rt + 2048) >> 12; // round up to nearest ohm
	} else {
		Rt = (ts->x_plate_ohms * ts->x_raw *
		      (ts->z2_raw - ts->z1_raw)) / ts->z1_raw;
		Rt = (Rt + 2048) >> 12; // round up to nearest ohm
	}
#endif
	//info("chug_raw_data() pendown=%d, Rt=%d, pendown_thresh_ohms=%d, penup_thresh_ohms=%d ",
	//		ts->pendown,Rt,ts->pendown_thresh_ohms,ts->penup_thresh_ohms); 
	info1("chug_raw_data() Rt=%d, x_raw=%d, y_raw=%d, z1_raw=%d",
			Rt,ts->x_raw,ts->y_raw,ts->z1_raw); 


#if 1 // force to pendown as default
      // 
	ts->pendown = 1;

#else
	// hysteresis
	if (!ts->pendown && Rt > ts->pendown_thresh_ohms)
		ts->pendown = 1;
	else if (ts->pendown && Rt < ts->penup_thresh_ohms)
		ts->pendown = 0;
#endif
	info("chug_raw_data() pendown=%d, time=%d", ts->pendown,event.millisecs); 

	if (ts->pendown) {

#ifdef	FILTER_ON 
		if(ts->x_raw == 0)
		{
			goto do_skip;
		}

		if(ts->y_raw == 0)
		{
			// no pen pressure
			goto do_skip;
		}
#endif

		// Pen is down
		// Calculate calibrated X,Y
#if 1
		// this  depends on the orientation of the panel

	#ifdef PANEL_4
		x = (4096 - ts->y_raw);	// 1810 << y_raw << 130
		y = (ts->x_raw);
	#endif
	#ifdef PANEL_28
		x = (ts->x_raw);
		y = (0x1000 - ts->y_raw);
	#endif

		// find out the real data

#if defined(A300_QTOPIA)
		Xcal = (int)(ts->x_raw);
		Ycal = (int)(ts->y_raw);
#else
		Xcal = (int)((x - TS_X_OFFSET) * TS_X_SCALE);
		Ycal = (int)((y - TS_Y_OFFSET) * TS_Y_SCALE); 
		if(Xcal < 0)
			Xcal = 0;

		if(Ycal < 0)
			Ycal = 0;

		if(Xcal >= TS_DISPLAY_X)
			Xcal = TS_DISPLAY_X -1;

		if(Ycal >= TS_DISPLAY_Y)
			Ycal = TS_DISPLAY_Y -1;

#endif // A300_QTOPIA
#else
		Xcal = ((ts->cal.xscale * ts->x_raw) >> 8) + ts->cal.xtrans;
		Ycal = ((ts->cal.yscale * ts->y_raw) >> 8) + ts->cal.ytrans;
#endif

		info("chug_raw_data() Xcal=%d, Ycal=%d", Xcal,Ycal); 
		info1("chug_raw_data() Xcal=%d, Ycal=%d", Xcal,Ycal); 

		event.x = (unsigned short)Xcal;
		event.y = (unsigned short)Ycal;
#if defined(A300_QTOPIA)
		event.pressure = (unsigned short)ts->z2_raw ? 1 : 0;
#else
#if 1
		if(ts->z2_raw == 0x41)
		{
			pendown_event = 1;
			event.pressure = 0x40; 
		}
		else
		{
			pendown_event = 0;
			event.pressure = (unsigned short)ts->z2_raw;
		}
#else
		event.pressure = (unsigned short)Rt;
#endif
#endif // A300_QTOPIA

		
		//==================================================
		// Add the event to the event queue if necessary
		//==================================================

		// add this event to the event queue
		spin_lock_irqsave(&ts->lock, flags);
do_again:
#if defined(A300_QTOPIA)
		if (event.x > (X_AXIS_MAX * 2) || event.y > (Y_AXIS_MAX * 2)) {
			if (event.pressure == 0 && ts->nextIn > 0)
				ts->event_buf[(ts->nextIn - 1)].pressure = 0;
		} else {
			if (event.x > X_AXIS_MAX) event.x = X_AXIS_MAX;
			if (event.x < X_AXIS_MIN) event.x = X_AXIS_MIN;
			if (event.y > Y_AXIS_MAX) event.y = Y_AXIS_MAX;
			if (event.y < Y_AXIS_MIN) event.y = Y_AXIS_MIN;
			ts->event_buf[ts->nextIn++] = event;
		}
#else
		ts->event_buf[ts->nextIn++] = event;
#endif

		info2("%d %d %d",event.x,event.y,event.pressure); 
		if (ts->nextIn == EVENT_BUFSIZE)
			ts->nextIn = 0;
		if (ts->event_count < EVENT_BUFSIZE) {
			ts->event_count++;
			info1("c_r_d() e_count=%d",ts->event_count);
		} else {
			// throw out the oldest event
#if 1 //TZYYWEI FIX
			++ts->nextOut;  //move the nextOut point up
#endif
			if (++ts->nextOut == EVENT_BUFSIZE)
				ts->nextOut = 0;
		}

		if(pendown_event)
		{
			pendown_event = 0;
			goto do_again;
		}

		spin_unlock_irqrestore(&ts->lock, flags);

		//==================================================
		// End of adding
		//==================================================
do_skip:

		// async notify
		if (ts->fasync)
			kill_fasync(&ts->fasync, SIGIO, POLL_IN);
		// wake up any read call
		if (waitqueue_active(&ts->wait))
			wake_up_interruptible(&ts->wait);
	}
}

/*
 * Raw X,Y,pressure acquisition timer function. This triggers
 * the start of a new acquisition. Its duration between calls
 * is the touch screen polling rate.
 */
static void
MQ9000_acq_timer(unsigned long data)
{
	MQ9000_ts_t* ts = (MQ9000_ts_t*)data;
	volatile MQ9000REGS	*pMQRegs = (MQ9000REGS *)MQ9000_REGS_VBASE;
	unsigned long flags;
	++ts->acq_count;

	spin_lock_irqsave(&ts->lock, flags);

#if 1 //TZYYWE_TRY

	if(!(pMQRegs->gp.gp17.Gpio116_119 & GPIO119_DATA))
	{
		// pen is down, try to measure some more points data here

		ts->x_raw0 = ts->x_raw = ReadADS7846E( MEASURE_X );
		ts->y_raw0 = ts->y_raw = ReadADS7846E( MEASURE_Y );
		ts->z1_raw0 = ts->z1_raw = ReadADS7846E( MEASURE_Z1 );
		ts->z2_raw0 = ts->z2_raw = 0;
//	info("MQ9000_acq_timer() x_raw=%d, y_raw=%d, z1_raw=%d ",ts->x_raw, ts->y_raw,ts->z1_raw); 
		info2("t1"); 

		queue_task(&ts->chug_tq, &tq_immediate);
		mark_bh(IMMEDIATE_BH);
		ReadADS7846E( MEASURE_INIT );
	}
	else
	{
		if(ts->ts_pendown)
		{
			info2("t0"); 
			ts->ts_pendown = 0;

			// finished the measure here, resent the last measurement to indicate that it is pen up.
			ts->x_raw = ts->x_raw0; 
			ts->y_raw = ts->y_raw0;
			ts->z1_raw = ts->z1_raw0;
			ts->z2_raw = 0x40;
			queue_task(&ts->chug_tq, &tq_immediate);
			mark_bh(IMMEDIATE_BH);
			ReadADS7846E( MEASURE_INIT );
		}
	}
	// here the pen is down do the measure.
#endif

	// start acquisition with X coordinate
	ts->acq_state = ACQ_X;

	// schedule next acquire
	ts->acq_timer.expires = jiffies + HZ/100;
//	ts->acq_timer.expires = jiffies + HZ / 100;
	if (ts->ts_pendown == 1)
		add_timer(&ts->acq_timer);

	spin_unlock_irqrestore(&ts->lock, flags);
}

#if defined(A300_QTOPIA)
static void
mqts_interrupt(int irq, void *dev_id, struct pt_regs *regs)
{
	volatile MQ9000REGS	*pMQRegs = (MQ9000REGS *)MQ9000_REGS_VBASE;
	MQ9000_ts_t *ts = (MQ9000_ts_t*)dev_id;

	info("mqts_interrupt() "); 

	spin_lock(&ts->lock);

	if((pMQRegs->gp.gp17.Gpio116_119 & GPIO119_DATA))
	{
		del_timer(&ts->acq_timer);
		ts->ts_pendown = 0;	// pen up
		queue_task(&ts->chug_tq, &tq_immediate);
		mark_bh(IMMEDIATE_BH);
		ReadADS7846E( MEASURE_INIT );
	} else if (ts->ts_pendown == 0) {
		ts->ts_pendown = 1;
		add_timer(&ts->acq_timer);
	}

	spin_unlock(&ts->lock);
}
#else
static void
mqts_interrupt(int irq, void *dev_id, struct pt_regs *regs)
{
	volatile MQ9000REGS	*pMQRegs = (MQ9000REGS *)MQ9000_REGS_VBASE;
	MQ9000_ts_t *ts = (MQ9000_ts_t*)dev_id;

	info("mqts_interrupt() "); 

	spin_lock(&ts->lock);

	if((pMQRegs->gp.gp17.Gpio116_119 & GPIO119_DATA))
	{
		info2("i0"); 
		ts->ts_pendown = 0;	// pen up
		goto done;
	}
		info2("i1"); 
	ts->ts_pendown = 1;		// pen down, start measurement here	
	ts->acq_count =  0;

	switch (ts->acq_state) {
	case IDLE:
		info("mqts_interrupt() IDLE "); 
//		break;
	case ACQ_X:
		ts->x_raw = ReadADS7846E( MEASURE_X );
		info("mqts_interrupt() ts->x_raw=%d",ts->x_raw); 
		ts->acq_state = ACQ_Y;
//		break;
	case ACQ_Y:
		ts->y_raw = ReadADS7846E( MEASURE_Y );
		info("mqts_interrupt() ts->y_raw=%d",ts->y_raw); 
		ts->acq_state = ACQ_Z1;
//		break;
	case ACQ_Z1:
		ts->z1_raw = ReadADS7846E( MEASURE_Z1 );
		ts->z2_raw = 0x40;
		info("mqts_interrupt() ts->z1_raw=%d",ts->z1_raw); 
		if (ts->pressure_eqn == PRESSURE_EQN_2) {
			// don't acq Z2, using 2nd eqn for touch pressure
			ts->acq_state = IDLE;
			// got the raw stuff, now mark BH
			queue_task(&ts->chug_tq, &tq_immediate);
			mark_bh(IMMEDIATE_BH);
		} else {
			ts->acq_state = ACQ_Z2;
		}
		break;
	case ACQ_Z2:
		ts->z2_raw = ReadADS7846E( MEASURE_Z2 );
		info("mqts_interrupt() ts->z2_raw=%d",ts->z2_raw); 
		ts->acq_state = IDLE;
		// got the raw stuff, now mark BH
		queue_task(&ts->chug_tq, &tq_immediate);
		mark_bh(IMMEDIATE_BH);
		break;
	}

done:

#if 1
	ReadADS7846E( MEASURE_INIT );
		// clear up interrupt status if it is there
	pMQRegs->gp.gp19.Gpio116_119IntStat = GPIO119_INT_STATUS; 	//Enable Timer Modele for GPIO119 interrupt
	pMQRegs->in.in04.RawStatus = UM_TIMER ;
#endif
	spin_unlock(&ts->lock);
	info("mqts_interrupt() Done=======>");
}
#endif // A300_QTOPIA


/* +++++++++++++ File operations ++++++++++++++*/

static int
MQ9000_fasync(int fd, struct file *filp, int mode)
{
	MQ9000_ts_t* ts = (MQ9000_ts_t*)filp->private_data;
	info("MQ9000ts_fasync() =======>");
	return fasync_helper(fd, filp, mode, &ts->fasync);
#if 0
	info("MQ9000ts_fasync() done=======>");
	return 1;
#endif
}

static int
MQ9000_ioctl(struct inode * inode, struct file *filp,
	     unsigned int cmd, unsigned long arg)
{
	MQ9000_ts_t* ts = (MQ9000_ts_t*)filp->private_data;

	info("MQ9000ts_ioctl() =======>");

	switch(cmd) {
	case TS_GET_RATE:       /* TODO: what is this? */
		break;
	case TS_SET_RATE:       /* TODO: what is this? */
		break;
	case TS_GET_CAL:
		copy_to_user((char *)arg, (char *)&ts->cal, sizeof(TS_CAL));
		break;
	case TS_SET_CAL:
		copy_from_user((char *)&ts->cal, (char *)arg, sizeof(TS_CAL));
		break;
	default:
		err("unknown cmd %04x", cmd);
		return -EINVAL;
	}

	info("MQ9000ts_ioctl() done=======>");
	return 0;
}

static unsigned int
MQ9000_poll(struct file * filp, poll_table * wait)
{
	MQ9000_ts_t* ts = (MQ9000_ts_t*)filp->private_data;
//	info("MQ9000ts_poll() =======>");

	poll_wait(filp, &ts->wait, wait);
	if (ts->event_count)
	{
		info1("M_p() event_count=%d",ts->event_count);
		return POLLIN | POLLRDNORM;
	}

//	info("MQ9000ts_poll() Done =======>");
	return 0;
}

#if defined(A300_QTOPIA)
static ssize_t
MQ9000_read(struct file * filp, char * buf, size_t count, loff_t * l)
{
	MQ9000_ts_t* ts = (MQ9000_ts_t*)filp->private_data;
	unsigned long flags;
	TS_EVENT event, t;
	int i;

	info("MQ9000ts_read() =======> input count=%d",count);
	info1("M_r() => i c=%d, e_c=%d,s=%d,cs=%d",count,ts->event_count,sizeof(TS_EVENT),sizeof(C_TS_EVENT));

	if (ts->event_count == 0)
	{
		info1("1");
		if (filp->f_flags & O_NONBLOCK)
		{
			info1("2");
			return -EAGAIN;
		}
		interruptible_sleep_on(&ts->wait);
		if (signal_pending(current))
		{
			info1("3");
			return -ERESTARTSYS;
		}
	}

	info1("4--");
	for (i = count; i >= sizeof(TS_EVENT);
			i -= sizeof(TS_EVENT), buf += sizeof(TS_EVENT))
	{
		if(ts->event_count == 0)
			break;

		spin_lock_irqsave(&ts->lock, flags);
		event = ts->event_buf[ts->nextOut++];

		if (xyswap) {
			short tmp = event.x;
			event.x = event.y;
			event.y = tmp;
		}
		if (cal_ok) {
			t.x = (x_rev) ? ((raw_max_x - event.x) * res_x) / (raw_max_x - raw_min_x)
		 		: ((event.x - raw_min_x) * res_x) / (raw_max_x - raw_min_x);
			t.y = (y_rev) ? ((raw_max_y - event.y) * res_y) / (raw_max_y - raw_min_y)
				: ((event.y - raw_min_y) * res_y) / (raw_max_y - raw_min_y);
		} else {
			t.x = event.x;
			t.y = event.y;
		}

		t.pressure = event.pressure;
		t.millisecs = event.millisecs;

		if (ts->nextOut == EVENT_BUFSIZE)
			ts->nextOut = 0;
		if (ts->event_count)
			ts->event_count--;
		spin_unlock_irqrestore(&ts->lock, flags);
		copy_to_user(buf, &t, sizeof(TS_EVENT));
	}

	info1("M_r() => rc=%d",count-i);
	info("MQ9000ts_read() Done =======>");
	return count - i;
}
#else
static ssize_t
MQ9000_read(struct file * filp, char * buf, size_t count, loff_t * l)
{
	MQ9000_ts_t* ts = (MQ9000_ts_t*)filp->private_data;
	unsigned char data[5];
	unsigned long flags;
	TS_EVENT event;
	int i;

	info("MQ9000ts_read() =======> input count=%d",count);
	info1("M_r() => i c=%d, e_c=%d,s=%d,cs=%d",count,ts->event_count,sizeof(TS_EVENT),sizeof(C_TS_EVENT));

	if (ts->event_count == 0)
	{
		info1("1");
		if (filp->f_flags & O_NONBLOCK)
		{
			info1("2");
			return -EAGAIN;
		}
		interruptible_sleep_on(&ts->wait);
		if (signal_pending(current))
		{
			info1("3");
			return -ERESTARTSYS;
		}
	}

	info1("4--");
#if 1 // TZYYWE hack for QT CUTOMERTOUCHPANEL
	// for now, the input event size is equal to 5
	// data[0] = status = 0x40, left bottom push
	// data[1],data[2] = x
	// data[3], data[4] = y
	for (i = count;
	     i >= 5;
	     i -= 5, buf += 5)
	{
		info1("5");
		if(ts->event_count == 0)
		{
			info1("6");
			break;
		}
		spin_lock_irqsave(&ts->lock, flags);
		event = ts->event_buf[ts->nextOut++];
		data[0]	= (unsigned char)(event.pressure); // 0x40; // hardcode for now, assume every data is LBUTTON down, TZYYWEI
		data[1]	= (unsigned char)(event.x >> 8);
		data[2]	= (unsigned char)(event.x & 0x00FF);
		data[3]	= (unsigned char)(event.y >> 8);
		data[4]	= (unsigned char)(event.y & 0x00FF);

		info1("MQ9000ts_read() x=%d,y=%d",event.x,event.y);

		if (ts->nextOut == EVENT_BUFSIZE)
			ts->nextOut = 0;
		if (ts->event_count)
		{
			ts->event_count--;
			info("MQ9000ts_read() ts->event_count=%d",ts->event_count);
		}
		spin_unlock_irqrestore(&ts->lock, flags);
		info1("7");
		copy_to_user(buf, data, 5);
	}
#else
	for (i = count;
	     i >= sizeof(TS_EVENT);
	     i -= sizeof(TS_EVENT), buf += sizeof(TS_EVENT))
	{
		info1("5");
		if (ts->event_count == 0)
		{
			info1("6");
			break;
		}
		spin_lock_irqsave(&ts->lock, flags);
		event = ts->event_buf[ts->nextOut++];
		if (ts->nextOut == EVENT_BUFSIZE)
			ts->nextOut = 0;
		if (ts->event_count)
		{
			ts->event_count--;
			info("MQ9000ts_read() ts->event_count=%d",ts->event_count);
		}
		spin_unlock_irqrestore(&ts->lock, flags);
		info1("7");
		copy_to_user(buf, &event, sizeof(TS_EVENT));
	}
#endif

	info1("M_r() => rc=%d",count-i);
	info("MQ9000ts_read() Done =======>");
	return count - i;
}
#endif // A300_QTOPIA

static int
MQ9000_open(struct inode * inode, struct file * filp)
{
	MQ9000_ts_t* ts;
	unsigned long flags;

	info("MQ9000ts_open() =======>");

	filp->private_data = ts = &MQ9000_ts;

	spin_lock_irqsave(&ts->lock, flags);

	//Make sure that the SPI (or GPIO) block is ready for Touch Panel input
	sp_regs();
	SetupSPI();
	ReadADS7846E(MEASURE_INIT);

	sp_regs();

	/*
	 * init bh handler that chugs the raw data (calibrates and
	 * calculates touch pressure).
	 */
	ts->chug_tq.routine = chug_raw_data;
	ts->chug_tq.data = ts;
	ts->pendown = 0; // pen up

	ts->ts_pendown = 0; // no pen touch
	ts->pendown_sent = 0; // no pen touch
	
	// flush event queue
	ts->nextIn = ts->nextOut = ts->event_count = 0;
	
	// Start acquisition timer function
	init_timer(&ts->acq_timer);
	ts->acq_timer.function = MQ9000_acq_timer;
	ts->acq_count = 0;

#if 1
	ts->acq_timer.data = (unsigned long)ts;
//	ts->acq_timer.expires = jiffies + HZ / 100;
	ts->acq_timer.expires = jiffies + HZ / 100;
	enable_irq(IRQ_KATANATS);
#endif
	spin_unlock_irqrestore(&ts->lock, flags);
	MOD_INC_USE_COUNT;
	info("MQ9000ts_open() Done =======>");
	return 0;
}

static int
MQ9000_release(struct inode * inode, struct file * filp)
{
	MQ9000_ts_t* ts = (MQ9000_ts_t*)filp->private_data;
	unsigned long flags;
	
	info("MQ9000ts_release() =======>");

	MQ9000_fasync(-1, filp, 0);

	del_timer_sync(&ts->acq_timer);

	spin_lock_irqsave(&ts->lock, flags);

	// disable TS interrupts from GPIO input side
	disable_irq(IRQ_KATANATS);

	spin_unlock_irqrestore(&ts->lock, flags);

	MOD_DEC_USE_COUNT;

	info("MQ9000ts_release() Done =======>");
	return 0;
}


static struct file_operations ts_fops = {
	read:       MQ9000_read,
	poll:       MQ9000_poll,
	ioctl:	    MQ9000_ioctl,
	fasync:     MQ9000_fasync,
	open:	    MQ9000_open,
	release:    MQ9000_release,
};
/* +++++++++++++ End File operations ++++++++++++++*/


int __init
MQ9000ts_init_module(void)
{
	MQ9000_ts_t* ts = &MQ9000_ts;
	int ret;

	info("MQ9000ts_init_module() =======>");

	/* register our character device */
	if ((ret = register_chrdev(TS_MAJOR, TS_NAME, &ts_fops)) < 0) {
		err("can't get major number");
		return ret;
	}
	info("registered");

	memset(ts, 0, sizeof(MQ9000_ts_t));
	init_waitqueue_head(&ts->wait);
	spin_lock_init(&ts->lock);

#if defined(A300_QTOPIA)
	raw_max_x = X_AXIS_MAX;
	raw_max_y = Y_AXIS_MAX;
	raw_min_x = X_AXIS_MIN;
	raw_min_y = Y_AXIS_MIN;
	res_x = 240;
	res_y = 320;
	cal_ok = 1;
	x_rev = 0;
	y_rev = 0;
	xyswap = 0;
#endif
	// TBD, TZYYWEI
	// initial calibration values
	ts->cal.xscale = -93;
	ts->cal.xtrans = 346;
	ts->cal.yscale = -64;
	ts->cal.ytrans = 251;

	// init pen up/down hysteresis points
	ts->pendown_thresh_ohms = DEFAULT_PENDOWN_THRESH_OHMS;
	ts->penup_thresh_ohms = DEFAULT_PENUP_THRESH_OHMS;
	ts->pressure_eqn = PRESSURE_EQN_2;
	// init X and Y plate resistances
	ts->x_plate_ohms = DEFAULT_X_PLATE_OHMS;
	ts->y_plate_ohms = DEFAULT_Y_PLATE_OHMS;

	// Set up GPIO to SPI mode
	sp_regs();
	SetupSPI();
	sp_regs();
	
	if ((ret = request_irq(IRQ_KATANATS, mqts_interrupt,
			       SA_INTERRUPT, TS_NAME, ts))) {
		err("could not get IRQ");
		return ret;
	}

	// FIXME: is this a working baudrate?
	ts->clkdiv = 0;

	info("baudrate = %d Hz", ts->baudrate);
	
	info("MQ9000ts_init_module() Done=======>");
	printk("KATANA Touch Panel driver initialized\n");
	return 0;
}

void
MQ9000ts_cleanup_module(void)
{
	info("MQ9000ts_cleanup_module() =======>");
	// disable GPIO119 and hold in reset
	free_irq(IRQ_KATANATS, &MQ9000_ts);
	unregister_chrdev(TS_MAJOR, TS_NAME);
	info("MQ9000ts_cleanup_module() Done=======>");
}

/* Module information */
MODULE_AUTHOR("Tzyywei Hwang, tzyywei@mediaq.com, www.mediaq.com");
MODULE_DESCRIPTION("MQ9000/ADS7846 Touch Screen Driver");

module_init(MQ9000ts_init_module);
module_exit(MQ9000ts_cleanup_module);
