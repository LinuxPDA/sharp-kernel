/*
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
 * Copyright (C) 2003 Motorola Inc Ltd
 *
 */

#include <linux/config.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/sched.h>
#include <linux/init.h>
#include <asm/uaccess.h> 
#include <linux/pm.h>
#include <linux/delay.h>

#include <linux/i2c.h>
#include <linux/i2c-algo-bit.h>
#include <linux/i2c-id.h>

#include <asm/dma.h>

#include "sound_config.h"  /*include many macro definiation*/
#include "dev_table.h"     /*sound_install_audiodrv()...*/
#include "dbmx-mw8731-audio.h"

#include <linux/soundcard.h>

//#define DEBUG
#ifdef DEBUG
# define DPRINTK(fmt, args...) printk("%s: " fmt, __FUNCTION__, ## args)
# define FUNC_START	DPRINTK(KERN_ERR"start\n");
# define FUNC_END	DPRINTK(KERN_ERR"end\n");
#else
# define DPRINTK(fmt, args...)
# define FUNC_START
# define FUNC_END
#endif

#define RX_SSI	1
#define TX_SSI	1

/***********************************************************************************
 *This to store the control word for codec DAC3550a
 * ********************************************************************************/
static uint8_t mesgbuf[4];
static uint8_t judge[3];

/***********************************************************************************
 *  Global var
 * ********************************************************************************/
static struct ssi_aud 	i2s_ssi;

static const uint32_t ssi_bits = 16;
static uint32_t ssi_channel;
static uint32_t ssi_speed;
static uint32_t ssi_busy;
static uint32_t open_mode;

static uint8_t gVolumeL = 80, gVolumeR = 80;
static uint8_t gGainL = 60, gGainR = 60;
static uint32_t gActiveRecordChannel = SOUND_MASK_MIC;

/***********************************************************************************
 *  Register Contents (sets to default value on power up)
 * ********************************************************************************/


static uint16_t regMWLeftLineIn 					= KLLIN;
static uint16_t regMWRightLineIn 					= KRLIN;
static uint16_t regMWLeftHeadphoneOut 				= KLHPOUT;
static uint16_t regMWRightHeadphoneOut 				= KRHPOUT;
static uint16_t regMWAnalogAudioPathControl 		= KAAPC;
static uint16_t regMWDigitalAudioPathControl		= KDAPC;
static uint16_t regMWPowerDownControl				= KPDC;
static uint16_t regMWDigitalAudioInterfaceFormat 	= KDAIF;
static uint16_t regMWSamplingControl				= KSC;
static uint16_t regMWActiveControl					= KAC;


static int  dbmx_audiodev, dbmx_mixerdev;
static struct pm_dev *pmdev;

/***********************************************************************************
 *  Functions to be used
 * ********************************************************************************/
extern void 	DMAbuf_outputintr	(int dev, int notify_only);
extern void 	DMAbuf_inputintr	(int dev);
extern int16_t 	i2c_ssi_write 		(uint8_t * buf, size_t count, int16_t ssi_reg );
extern int16_t 	i2c_ssi_read 		(uint8_t * buf, size_t count, int16_t ssi_reg ); 

static int 	dbmx_audio_prepare_for_io		(int dev, int bsize, int bcount);
static void dbmx_audio_trigger				(int dev, int state);
static void dbmx_audio_output_block			(int dev, unsigned long buf, int count, int intrflag);
static void dbmx_audio_close				(int dev);
static int 	dbmx_audio_open					(int dev, int mode);
static void dbmx_audio_reset				(int dev);
static int 	dbmx_audio_ioctl				(int dev, unsigned int cmd, caddr_t arg);
static int 	dbmx_audio_setspeed				(int dev, int speed);
static signed short dbmx_audio_setchannel	(int dev, signed short channels);
static unsigned int dbmx_audio_setbits		(int dev, unsigned int bits);
static int 	dbmx_pm_callback				(struct pm_dev *pmdev, pm_request_t rqst, void *data);   


void ssi_DMA_Tx_Handler(void*);
void ssi_DMA_Rx_Handler(void*);

/* -------------------------------------------------------------------------------
 * Codec register control functions
 * -------------------------------------------------------------------------------
 */
static int16_t mw8731SetLineInVolume(leftRightDirection_t dir, uint8_t volume)
{	
	uint16_t temp;
	
	FUNC_START
	
	// max is 100
	if (volume > 100)
		volume = 100;
	
	// convert percentage volume to HEX
	temp = (volume * (KMaxLineInVolume - KMinLineInVolume) / 100) + KMinLineInVolume;
	
	switch(dir)
	{
	case KLeft:
		regMWLeftLineIn &= ~KLRINBOTH;
		regMWLeftLineIn &= ~KLINVOL;
		regMWLeftLineIn |= (temp & KLINVOL);
		temp = regMWLeftLineIn;
		break;
	case KRight:
		regMWRightLineIn &= ~KRLINBOTH;
		regMWRightLineIn &= ~KRINVOL;
		regMWRightLineIn |= (temp & KRINVOL);
		temp = regMWRightLineIn;
		break;
	case KBoth:
	default:
		regMWLeftLineIn |= KLRINBOTH;
		regMWLeftLineIn &= ~KLINVOL;
		regMWLeftLineIn |= (temp & KLINVOL);
		temp = regMWLeftLineIn;
		break;	
	}

	mesgbuf[0] = temp >> 8;
	mesgbuf[1] = temp & 0x00FF;

	FUNC_END

	return i2c_ssi_write(mesgbuf, 2, 0);
}

static int16_t mw8731MuteLineIn(leftRightDirection_t dir)
{	
	uint16_t temp;
	
	FUNC_START

	switch(dir)
	{
	case KLeft:
		regMWLeftLineIn &= ~KLRINBOTH;
		regMWLeftLineIn |= KLINMUTE;
		temp = regMWLeftLineIn;
		break;
	case KRight:
		regMWRightLineIn &= ~KRLINBOTH;
		regMWRightLineIn |= KRINMUTE;
		temp = regMWRightLineIn;
		break;
	case KBoth:
	default:
		regMWLeftLineIn |= KLRINBOTH;
		regMWLeftLineIn |= KLINMUTE;
		temp = regMWLeftLineIn;
		break;
	}

	mesgbuf[0] = temp >> 8;
	mesgbuf[1] = temp & 0x00FF;
	
	FUNC_END
	
	return i2c_ssi_write(mesgbuf, 2, 0);
}

static int16_t mw8731UnmuteLineIn(leftRightDirection_t dir)
{	
	uint16_t temp;
	
	FUNC_START

	switch(dir)
	{
	case KLeft:
		regMWLeftLineIn &= ~KLRINBOTH;
		regMWLeftLineIn &= ~KLINMUTE;
		temp = regMWLeftLineIn;
		break;
	case KRight:
		regMWRightLineIn &= ~KRLINBOTH;
		regMWRightLineIn &= ~KRINMUTE;
		temp = regMWRightLineIn;
		break;
	case KBoth:
	default:
		regMWLeftLineIn |= KLRINBOTH;
		regMWLeftLineIn &= ~KLINMUTE;
		temp = regMWLeftLineIn;
		break;
	}

	mesgbuf[0] = temp >> 8;
	mesgbuf[1] = temp & 0x00FF;
	
	FUNC_END
	
	return i2c_ssi_write(mesgbuf, 2, 0);
}

static int16_t mw8731MuteMicIn(void)
{
	FUNC_START
	
	regMWAnalogAudioPathControl |= KMUTEMIC;
	
	mesgbuf[0] = regMWAnalogAudioPathControl >> 8;
	mesgbuf[1] = regMWAnalogAudioPathControl & 0x00FF;
	
	FUNC_END
	
	return i2c_ssi_write(mesgbuf, 2, 0);
}

static int16_t mw8731UnmuteMicIn(void)
{	
	FUNC_START
	
	regMWAnalogAudioPathControl &= ~KMUTEMIC;
	
	mesgbuf[0] = regMWAnalogAudioPathControl >> 8;
	mesgbuf[1] = regMWAnalogAudioPathControl & 0x00FF;
	
	FUNC_END
	
	return i2c_ssi_write(mesgbuf, 2, 0);
}


static int16_t mw8731SetHeadPhoneVolume(leftRightDirection_t dir, uint8_t volume)
{	
	uint16_t temp;
	
	// max is 100
	if (volume > 100)
		volume = 100;
	
	// convert percentage volume to HEX
	temp = (volume * (KMaxOutVolume - KMinOutVolume) / 100) + KMinOutVolume;
	
	switch(dir)
	{
	case KLeft:
		regMWLeftHeadphoneOut &= ~KLRHPBOTH;
		regMWLeftHeadphoneOut &= ~KLHPVOL;
		regMWLeftHeadphoneOut |= (temp & KLHPVOL);
		temp = regMWLeftHeadphoneOut;
		break;
	case KRight:
		regMWRightHeadphoneOut &= ~KLRHPBOTH;
		regMWRightHeadphoneOut &= ~KLHPVOL;
		regMWRightHeadphoneOut |= (temp & KLHPVOL);
		temp = regMWRightHeadphoneOut;
		break;
	case KBoth:
	default:
		regMWLeftHeadphoneOut |= KLRHPBOTH;
		regMWLeftHeadphoneOut &= ~KLHPVOL;
		regMWLeftHeadphoneOut |= (temp & KLHPVOL);
		temp = regMWLeftHeadphoneOut;
		break;
	}

	mesgbuf[0] = temp >> 8;
	mesgbuf[1] = temp & 0x00FF;

	FUNC_END

	return i2c_ssi_write(mesgbuf, 2, 0);
}

static int16_t mw8731SelectMicIn(void)
{
	FUNC_START

	regMWAnalogAudioPathControl |= KINSEL;	// select MIC
	mesgbuf[0] = regMWAnalogAudioPathControl >> 8;
	mesgbuf[1] = regMWAnalogAudioPathControl & 0x00FF;

	gActiveRecordChannel = SOUND_MASK_MIC;

	FUNC_END

	return i2c_ssi_write(mesgbuf, 2, 0);
}

static int16_t mw8731SelectLineIn(void)
{
	FUNC_START

	regMWAnalogAudioPathControl &= ~KINSEL;	// select line in
	mesgbuf[0] = regMWAnalogAudioPathControl >> 8;
	mesgbuf[1] = regMWAnalogAudioPathControl & 0x00FF;

	gActiveRecordChannel = SOUND_MASK_LINE;

	FUNC_END

	return i2c_ssi_write(mesgbuf, 2, 0);
}

static int16_t mw8731EnableBypass(void)
{
	FUNC_START

	regMWAnalogAudioPathControl |= KBYPASS;
	mesgbuf[0] = regMWAnalogAudioPathControl >> 8;
	mesgbuf[1] = regMWAnalogAudioPathControl & 0x00FF;

	FUNC_END

	return i2c_ssi_write(mesgbuf, 2, 0);
}

static int16_t mw8731DisableBypass(void)
{
	FUNC_START

	regMWAnalogAudioPathControl &= ~KBYPASS;
	mesgbuf[0] = regMWAnalogAudioPathControl >> 8;
	mesgbuf[1] = regMWAnalogAudioPathControl & 0x00FF;

	FUNC_END

	return i2c_ssi_write(mesgbuf, 2, 0);
}

static int16_t mw8731EnableMicBoost(void)
{
	FUNC_START

	regMWAnalogAudioPathControl |= KMICBOOST;
	mesgbuf[0] = regMWAnalogAudioPathControl >> 8;
	mesgbuf[1] = regMWAnalogAudioPathControl & 0x00FF;

	FUNC_END

	return i2c_ssi_write(mesgbuf, 2, 0);
}

static int16_t mw8731DisableMicBoost(void)
{
	FUNC_START

	regMWAnalogAudioPathControl &= ~KMICBOOST;
	mesgbuf[0] = regMWAnalogAudioPathControl >> 8;
	mesgbuf[1] = regMWAnalogAudioPathControl & 0x00FF;

	FUNC_END

	return i2c_ssi_write(mesgbuf, 2, 0);
}

static int16_t mw8731EnableSideTone(void)
{
	FUNC_START

	regMWAnalogAudioPathControl |= KSIDETONE;
	mesgbuf[0] = regMWAnalogAudioPathControl >> 8;
	mesgbuf[1] = regMWAnalogAudioPathControl & 0x00FF;

	FUNC_END

	return i2c_ssi_write(mesgbuf, 2, 0);
}

static int16_t mw8731DisableSideTone(void)
{
	FUNC_START

	regMWAnalogAudioPathControl &= ~KSIDETONE;
	mesgbuf[0] = regMWAnalogAudioPathControl >> 8;
	mesgbuf[1] = regMWAnalogAudioPathControl & 0x00FF;

	FUNC_END

	return i2c_ssi_write(mesgbuf, 2, 0);
}

static int16_t mw8731SelectDAC(void)
{
	FUNC_START

	regMWAnalogAudioPathControl |= KDACSEL;
	mesgbuf[0] = regMWAnalogAudioPathControl >> 8;
	mesgbuf[1] = regMWAnalogAudioPathControl & 0x00FF;

	FUNC_END

	return i2c_ssi_write(mesgbuf, 2, 0);
}

static int16_t mw8731DeselectDAC(void)
{
	FUNC_START

	regMWAnalogAudioPathControl &= ~KDACSEL;
	mesgbuf[0] = regMWAnalogAudioPathControl >> 8;
	mesgbuf[1] = regMWAnalogAudioPathControl & 0x00FF;

	FUNC_END

	return i2c_ssi_write(mesgbuf, 2, 0);
}

static int16_t mw8731SetAnalogPathCtrl(uint32_t aCtrl)
{
	FUNC_START
	
	regMWAnalogAudioPathControl = (regMWAnalogAudioPathControl & ~0xFF) | (aCtrl & 0xFF);
	
	mesgbuf[0] = regMWAnalogAudioPathControl >> 8;
	mesgbuf[1] = regMWAnalogAudioPathControl & 0x00FF;
	
	FUNC_END
	
	return i2c_ssi_write(mesgbuf, 2, 0);
}

static int16_t mw8731SetDigitalPathCtrl(uint32_t aCtrl)
{
	FUNC_START
	
	regMWDigitalAudioPathControl = (regMWDigitalAudioPathControl & ~0x1F) | (aCtrl & 0x1F);
	
	mesgbuf[0] = regMWDigitalAudioPathControl >> 8;
	mesgbuf[1] = regMWDigitalAudioPathControl & 0x00FF;
	
	FUNC_END
	
	return i2c_ssi_write(mesgbuf, 2, 0);
}

static int16_t mw8731UnmuteDAC(void)
{
	FUNC_START
	
	regMWDigitalAudioPathControl &= ~KDACMU;
	
	mesgbuf[0] = regMWDigitalAudioPathControl >> 8;
	mesgbuf[1] = regMWDigitalAudioPathControl & 0x00FF;
	
	FUNC_END
	
	return i2c_ssi_write(mesgbuf, 2, 0);
}

static int16_t mw8731MuteDAC(void)
{
	FUNC_START
	
	regMWDigitalAudioPathControl |= KDACMU;
	
	mesgbuf[0] = regMWDigitalAudioPathControl >> 8;
	mesgbuf[1] = regMWDigitalAudioPathControl & 0x00FF;
	
	FUNC_END
	
	return i2c_ssi_write(mesgbuf, 2, 0);
}

static int16_t mw8731DisableDeemphasis(void)
{	
	FUNC_START
	
	regMWDigitalAudioPathControl &= ~KDEEMP;
	
	mesgbuf[0] = regMWDigitalAudioPathControl >> 8;
	mesgbuf[1] = regMWDigitalAudioPathControl & 0x00FF;
	
	FUNC_END
	
	return i2c_ssi_write(mesgbuf, 2, 0);
}

static int16_t mw8731SetPowerDown(void)
{
	FUNC_START

	regMWPowerDownControl |= KAllPowerDown;

	mesgbuf[0] = regMWPowerDownControl >> 8;
	mesgbuf[1] = regMWPowerDownControl & 0x00FF;
	
	FUNC_END
	
	return i2c_ssi_write(mesgbuf, 2, 0);
}

static int16_t mw8731SetPowerUp(void)
{
	FUNC_START

	regMWPowerDownControl &= ~KAllPowerDown;

	mesgbuf[0] = regMWPowerDownControl >> 8;
	mesgbuf[1] = regMWPowerDownControl & 0x00FF;
	
	FUNC_END
	
	return i2c_ssi_write(mesgbuf, 2, 0);
}

static int16_t mw8731SetDigitalIFFormat(uint32_t aFormat)
{
	FUNC_START
	
	regMWDigitalAudioInterfaceFormat = (regMWDigitalAudioInterfaceFormat & ~0xFF) | (aFormat & 0xFF);
	
	mesgbuf[0] = regMWDigitalAudioInterfaceFormat >> 8;
	mesgbuf[1] = regMWDigitalAudioInterfaceFormat & 0x00FF;
	
	FUNC_END
	
	return i2c_ssi_write(mesgbuf, 2, 0);
}

static int16_t mw8731SetSamplingCtrl(uint32_t rate)
{	
	FUNC_START

	regMWSamplingControl &= ~0x3C;
	
	// MCLK = 11.2896MHz
	if(rate == 8000)
	{
		regMWSamplingControl |= 0x2C;
	}
	else //default is 44100
	{
		regMWSamplingControl |= 0x20;
	}
	
	mesgbuf[0] = regMWSamplingControl >> 8;
	mesgbuf[1] = regMWSamplingControl & 0x00FF;

	FUNC_END
	
	return i2c_ssi_write(mesgbuf, 2, 0);
}

static int16_t mw8731SetActive(void)
{
	FUNC_START

	regMWActiveControl |= KACTIVE;
    	
	mesgbuf[0] = regMWActiveControl >> 8;
	mesgbuf[1] = regMWActiveControl & 0x00FF;
	
	FUNC_END
	
	return i2c_ssi_write(mesgbuf, 2, 0);
}

static int16_t mw8731SetInactive(void)
{
	FUNC_START

	regMWActiveControl &= ~KACTIVE;
    	
	mesgbuf[0] = regMWActiveControl >> 8;
	mesgbuf[1] = regMWActiveControl & 0x00FF;
	
	FUNC_END
	
	return i2c_ssi_write(mesgbuf, 2, 0);
}

static int16_t mw8731Reset(void)
{
	uint16_t temp = (KR15<<KAddrShift);

	FUNC_START

	// restore default hardware value
	regMWLeftLineIn 					= KLLIN;
 	regMWRightLineIn 					= KRLIN;
	regMWLeftHeadphoneOut 				= KLHPOUT;
	regMWRightHeadphoneOut 				= KRHPOUT;
	regMWAnalogAudioPathControl 		= KAAPC;
	regMWDigitalAudioPathControl		= KDAPC;
	regMWPowerDownControl				= KPDC;
	regMWDigitalAudioInterfaceFormat 	= KDAIF;
	regMWSamplingControl				= KSC;
	regMWActiveControl					= KAC;

	mesgbuf[0] = temp >> 8;
	mesgbuf[1] = temp & 0x00FF;
	
	FUNC_END
	
	return i2c_ssi_write(mesgbuf, 2, 0);
}

////////////////////////////////////////////////////////////////////

void codec_init(void)
{
	FUNC_START
	
	mw8731Reset();

	mw8731SetPowerUp();	
	mw8731SetInactive();
	
	// audio interface control
	regMWDigitalAudioInterfaceFormat &= ~0x4F;	// 16-bit
	regMWDigitalAudioInterfaceFormat |= 0x40;	// master mode
	regMWDigitalAudioInterfaceFormat |= 0x01;	// MSB-first, left justified
	mw8731SetDigitalIFFormat(regMWDigitalAudioInterfaceFormat);
	
	mw8731SetActive();
	
	// input/output path
	mw8731DisableBypass();
	mw8731DisableSideTone();
	
	if (open_mode & OPEN_READ)
	{
		// default is mic in
		mw8731SelectMicIn();
		mw8731UnmuteMicIn();
		mw8731EnableMicBoost();
//		mw8731MuteLineIn(KBoth);
	}
	
	if (open_mode & OPEN_WRITE)
	{
		mw8731SelectDAC();
		mw8731DisableDeemphasis();
		mw8731UnmuteDAC();
		mw8731SetHeadPhoneVolume(KBoth, gVolumeL);	
	}
	
	FUNC_END
}

// Sets up the I/O ports(ADS board), configure SSI, and init CODEC via I2C
void ssiHw_init(void)
{
	unsigned int temp;

	FUNC_START	
	
	// *****************
	// set up CODEC
	// *****************
	codec_init();
	
	// *****************
	// set up audio mux
	// *****************
	_reg_AUDMUX_HPCR1 = 	(1	<<	31) |		// TFSDIR 
							(1 	<<	30)	|		// TCLKDIR
							(4	<<	26)	|		// TFCSEL	select TX frame syn. from port5
							(1	<<	25)	|		// RSFDIR
							(1	<<	24)	|		// RCLKDIR
							(3	<<	20)	|		// RFCSEL	select RX frame syn. from port 4 !!!! diff in codec standalone!!!
							(3 	<<	13)	|		// RXDSEL	select RXD from port 4
							(0	<<	12) |		// SYN		
							(0	<<	10)	|		// TXRXEN
							(0	<< 	8)	|		// INMEN
							(0	<<	0);			// INMASK
	// Pinout SSI1 (Receive)
	_reg_AUDMUX_PPCR1 = 	(0	<<	31) |		// TFSDIR 		
							(0 	<<	30)	|		// TCLKDIR
							(0	<<	26)	|		// TFCSEL
							(0	<<	25)	|		// RSFDIR
							(0	<<	24)	|		// RCLKDIR
							(0	<<	20)	|		// RFCSEL
							(0	<<	13)	|		// RXDSEL
							(0	<<	12) |		// SYN
							(0	<<	10)	|		// TXRXEN
							(0	<< 	8)	|		// INMEN
							(0	<<	0);			// INMASK	
	// Pinout SSI2 (Transmit)
	_reg_AUDMUX_PPCR2 = 	(0	<<	31) |		// TFSDIR 
							(0 	<<	30)	|		// TCLKDIR
							(0	<<	26)	|		// TFCSEL
							(0	<<	25)	|		// RSFDIR
							(0	<<	24)	|		// RCLKDIR
							(0	<<	20)	|		// RFCSEL
							(0	<<	13)	|		// RXDSEL
							(0	<<	12) |		// SYN
							(0	<<	10)	|		// TXRXEN
							(0	<< 	8)	|		// INMEN
							(0	<<	0);			// INMASK
	
	// ***************
	// configure GPIO
	// ***************
	// set Port C SSI pins as primary functions 
	_reg_GPIO_GIUS(GPIOC) &= ~0x0FF00000;
	_reg_GPIO_GPR(GPIOC) &= ~0x0FF00000;

	// ***************
	// initialise SSI
	// ***************
	// enable ipg clock input to SSI1
	_reg_CRM_PCCR0 |= 0x00020040;

	// reset SSI
	_reg_SSI_SCR(TX_SSI) = 0;	// disable SSI and reset all bits in SCR
	_reg_SSI_SCR(TX_SSI) |= 1;	// enable SSI

	_reg_SSI_STCR(TX_SSI) =	(1	<<	9)	|		// TXBIT0
							(1	<<	7)	|		// TFEN0
							(0	<<	6)	|		// TFDIR
							(0	<< 	5)	|		// TXDIR
							(0	<<	4)	|		// TSHFD
							(1	<<	3)	;		// TSCKP

	_reg_SSI_SRCR(RX_SSI) =	(1	<<	9)	|		// RXBIT0
							(1	<<	7)	|		// RFEN0
							(0	<<	6)	|		// RFDIR
							(0	<< 	5)	|		// RXDIR
							(0	<<	4)	|		// RSHFD
							(1	<<	3)	;		// RSCKP

	// Set Tx FIFO water mark to be 4; Rx FIFO water mark to 6
	temp = _reg_SSI_SFCSR(TX_SSI);
	_reg_SSI_SFCSR(TX_SSI) = (temp & ~0x00FF) | 0x0064;
	
	// enable SSI TxFIFO 0 request DMA
	_reg_SSI_SIER(TX_SSI) |= 1 << 20;
	// enable SSI RxFIFO 0 request DMA
	_reg_SSI_SIER(RX_SSI) |= 1 << 22;
	
	FUNC_END
}


static int32_t dbmx_allocate_dma(int8_t *name)
{
	int32_t dmaChannel;

	FUNC_START
	
	// Try from Channel 0
	for (dmaChannel = 0; dmaChannel < 16; dmaChannel++)
	{
		if (!sound_alloc_dma(dmaChannel, name))
		{
			printk("%s: DMA channel %d acquired (%s)\n", __FUNCTION__, dmaChannel, name);
			return dmaChannel;
		}	
	}

	FUNC_END
	return -1;
}

static void dbmx_pm_setup(void)
{
	FUNC_START

	pmdev = pm_register(PM_UNKNOWN_DEV, dbmx_audiodev, dbmx_pm_callback);
	
	FUNC_END
}




/******************************************************************************************************
* Input:             
*       dev:         The major number of the device.
*       bits:        The format of the sound data.
* Value Returned:    The format of the sound data. 	
*
* Description: This routine will be used to set the format of the sound data.
*
*******************************************************************************************************/

static unsigned int dbmx_audio_setbits(int dev, unsigned int bits)
{
	FUNC_START
	FUNC_END
	return AFMT_S16_LE;
}
/****************************************************************************************************** 
* Input:             
*       dev:         The major number of the device.
*       channels:    The number of the channel be required.
* Value Returned:    The number of the channel be set. 	
*
* Description: This routine will be used to set the number of the channel.
*
*******************************************************************************************************/
static signed short dbmx_audio_setchannel(int dev, signed short channels)
{
	FUNC_START

	switch(channels)
	{
		case 1:
			ssi_channel = 1;
			break;
		case 2:
			ssi_channel = 2;
			break;
		default:
			break;
	}
	
	DPRINTK("ssi_channel == %d\n", ssi_channel);
	FUNC_END
	return ssi_channel;
}
/****************************************************************************************************** 
* Input:             
*       dev:         The major number of the device.
*       speed:       The sample rate.
* Value Returned:    The sample rate be set. 	
*
* Description: This routine will be used to set the sample rate.
*
*******************************************************************************************************/
static int dbmx_audio_setspeed(int dev, int speed)
{
	FUNC_START
	
	switch(speed)
	{
		case 44100:
			ssi_speed = speed;
		case 8000:
			ssi_speed = speed;
		default:
			break;
	}

	DPRINTK("speed == %d\n", ssi_speed);	
	FUNC_END
	return ssi_speed;
}
/****************************************************************************************************** 
* Input:             
*       dev:         The major number of the device.
*       cmd:         The command to control the codec.
* Value Returned:    1       success.
*                    -1      falure. 	
*
* Description: This routine will be used to control the codec.
*
*******************************************************************************************************/
static int dbmx_mixer_ioctl(int dev, unsigned int cmd, caddr_t arg)
{   
    int ret=0;
    int wflag = 0;
    int usrVol;
    
    FUNC_START
    
    /* Only accepts mixer (type 'M') ioctls */
    if (_IOC_TYPE(cmd) != 'M')
    {
    	FUNC_END
		return -EINVAL;
	}

    switch (cmd)
    {
	case SOUND_MIXER_READ_DEVMASK:
		// Mask available channels
		ret = SOUND_MASK_VOLUME | SOUND_MASK_LINE | SOUND_MASK_MIC;
		DPRINTK("SOUND_MIXER_READ_DEVMASK 0x%.4x\n", ret);
		break;
	case SOUND_MIXER_READ_RECMASK:
		// Mask available recording channels
		ret = SOUND_MASK_LINE | SOUND_MASK_MIC;
		DPRINTK("SOUND_MIXER_READ_RECMASK 0x%.4x\n", ret);
		break;
	case SOUND_MIXER_READ_STEREODEVS:
		// Mask stereo capable channels
		ret = SOUND_MASK_VOLUME | SOUND_MASK_LINE;
		DPRINTK("SOUND_MIXER_READ_STEREODEVS 0x%.4x\n", ret);
		break;
	case SOUND_MIXER_READ_CAPS:
		// Only one recording source at a time
		ret = SOUND_CAP_EXCL_INPUT;
		DPRINTK("SOUND_MIXER_READ_CAPS 0x%.4x\n", ret);
		break;
	case SOUND_MIXER_READ_VOLUME:
		// Return volume
		ret = gVolumeL | (gVolumeR << 8);
		DPRINTK("SOUND_MIXER_READ_VOLUME 0x%.4x\n", ret);
		break;
	case SOUND_MIXER_WRITE_VOLUME:
		// Set and return new volume
		if (get_user(usrVol, (int *)arg))
		{
			FUNC_END
			return -EFAULT;
		}
		gVolumeL = usrVol & 0xff;
		gVolumeR = (usrVol >> 8) & 0xff;
		DPRINTK("SOUND_MIXER_WRITE_VOLUME Left=%d%% Right=%d%%\n", gVolumeL, gVolumeR);
		if (gVolumeL == gVolumeR)
		{
			mw8731SetHeadPhoneVolume(KBoth, gVolumeL);
		}
		else
		{
			mw8731SetHeadPhoneVolume(KLeft, gVolumeL);
			mw8731SetHeadPhoneVolume(KRight, gVolumeR);
		}
		ret = usrVol;
		break;
	case SOUND_MIXER_READ_LINE:
		// Return input volume
		ret = gGainL | (gGainR << 8);
		DPRINTK("SOUND_MIXER_READ_LINE 0x%.4x\n", ret);
		break;
	case SOUND_MIXER_WRITE_LINE:
		// Set and return new volume
		if (get_user(usrVol, (int *)arg))
		{
			FUNC_END
			return -EFAULT;
		}
		gGainL = usrVol & 0xff;
		gGainR = (usrVol >> 8) & 0xff;
		DPRINTK("SOUND_MIXER_WRITE_LINE Left=%d%% Right=%d%%\n", gGainL, gGainR);
		if (gGainL == gGainR)
		{
			mw8731SetLineInVolume(KBoth, gGainL);
		}
		else
		{
			mw8731SetLineInVolume(KLeft, gGainL);
			mw8731SetLineInVolume(KRight, gGainR);
		}
		ret = usrVol;
		break;
	case SOUND_MIXER_READ_RECSRC:
		// Mask currently active recording device
		ret = gActiveRecordChannel;
		DPRINTK("SOUND_MIXER_READ_RECSRC 0x%.4x\n", ret);
		break;
	case SOUND_MIXER_WRITE_RECSRC:
		if (get_user(ret, (int *)arg))
		{
			FUNC_END
			return -EFAULT;
		}
		if (ret == 0 || (ret & SOUND_MASK_MIC))
		{
			// no channel selected or MIC is selected --> use MIC
			DPRINTK("SOUND_MIXER_WRITE_RECSRC 0x%.4x\n", ret);
			gActiveRecordChannel = SOUND_MASK_MIC;
			// Configure CODEC
			mw8731SelectMicIn();
		}
		else if (ret & SOUND_MASK_LINE)
		{
			// otherwise --> use LINE in
			DPRINTK("SOUND_MIXER_WRITE_RECSRC 0x%.4x\n", ret);
			gActiveRecordChannel = SOUND_MASK_LINE;
			// configure CODEC
			mw8731SelectLineIn();
		}
		else // invalid channel selection!
		{
			DPRINTK("SOUND_MIXER_WRITE_RECSRC invalid channel!\n");
			FUNC_END
			return -EINVAL;
		}
		break;

	default:
		DPRINTK("Unknown or unsupported cmd\n");
		FUNC_END
		return -EINVAL;
    }

	FUNC_END
    
    if (wflag == 0)
    	return put_user(ret, (int *)arg);
    	
    return 1;
}

static int dbmx_audio_ioctl(int dev, unsigned int cmd, caddr_t arg)
{   
	int val,ret=0;
	int wflag = 0;
	
	FUNC_START	
	
	switch (cmd) 
	{
	case SOUND_PCM_WRITE_RATE:
		if (get_user(val, (int *)arg)) 
		{
			FUNC_END
			return -EFAULT;
		}
		ret = dbmx_audio_setspeed(dev,val);
		break;
	case SOUND_PCM_READ_RATE:
		ret = ssi_speed;
		break;
	case SOUND_PCM_READ_CHANNELS:
		ret = ssi_channel;
		break;
	case SNDCTL_DSP_SETFMT:
		if (get_user(val, (int *)arg))
		{
			FUNC_END
			return -EFAULT;
		}
		ret = dbmx_audio_setbits(dev,val);
		break;
	case SNDCTL_DSP_GETFMTS:
		ret = AFMT_S16_LE;
		break;
	case SOUND_PCM_READ_BITS:
		ret = ssi_bits;
		break;
	default:
		FUNC_END
		return -EINVAL;
	}
	if(wflag == 0)
	{
		FUNC_END
		return put_user(ret, (int *)arg);
	}
	
	FUNC_END
	return 1;
}

/****************************************************************************************************** 
* Input:             
*       dev:         The major number of the device.
* Value Returned:    None.

* Description: This routine will reset device.
*
*******************************************************************************************************/
static void dbmx_audio_reset(int dev)
{
	uint32_t flags;
	
	FUNC_START
	
	save_flags(flags);
	// disable
	mw8731MuteDAC();	// first mute
	mw8731SetPowerDown();
	
	//disable Tx and Rx FIFO 0
	_reg_SSI_STCR(TX_SSI) &= ~(1 << 7);
	_reg_SSI_SRCR(RX_SSI) &= ~(1 << 7);
	
	// Disable transfer
	_reg_SSI_SCR(TX_SSI) &= ~0x02;
	judge[0] = 0;
	// Disable receive
	_reg_SSI_SCR(RX_SSI) &= ~0x04;
	judge[1] = 0;
	
	mw8731Reset();
	
	restore_flags(flags);
	FUNC_END
}

/****************************************************************************************************** 
* Input:             
*       dev:          The major number of the device.
*       mode:         PCM_ENABLE_OUTPUT   or  PCM_ENABLE_INPUT.
* Value Returned:    0           success.
                     -EBUSY      falure. 	
*
* Description: This routine will be used to open the device.
*
*******************************************************************************************************/
static int  dbmx_audio_open(int dev, int  mode)
{
	uint32_t flags;
	save_flags(flags);

	FUNC_START
	
	cli();
	if (ssi_busy)
	{
	    restore_flags(flags);
		FUNC_END
	    return -EBUSY;
	}
	ssi_busy = 1;
	restore_flags(flags);
	if (! ((mode & OPEN_READ) || (mode & OPEN_WRITE) || (mode & OPEN_READWRITE)))
	{
		FUNC_END
		return -EIO;
	}

	judge[0] = 0;
	judge[1] = 0;
	open_mode = mode;
	ssiHw_init();

	FUNC_END
   	return 0;
}
/****************************************************************************************************** 
* Input:             
*       dev:         The major number of the device.

* Value Returned:    None.

* Description: This routine will be used to close the device.
*******************************************************************************************************/
static void dbmx_audio_close(int dev)
{
	uint32_t   flags;
	
	FUNC_START
	
	save_flags(flags);
	cli();
	judge[2] = 0;
	dbmx_audio_reset(dev);
	ssi_busy = 0;
	restore_flags(flags);

	FUNC_END
}
/****************************************************************************************************** 
* Input:             
*       dev:         The major number of the device.
*       buf:         The pointer of the data buffer.
*       count:       The size of the buffer.
*       intrflag:    The flag bit.
* Value Returned:    None.

* Description: 
*
*******************************************************************************************************/
static void dbmx_audio_output_block(int dev, unsigned long buf, int count, int intrflag)
{
	unsigned long flags;
	
	FUNC_START
	if (judge[0] != 1)
	{
		judge[0] = 1;
		FUNC_END
		return;
	}
	DPRINTK("Re-enabling dma_channel_w(%d)\n", i2s_ssi.dma_channel_w);
	flags = claim_dma_lock();
	disable_dma(i2s_ssi.dma_channel_w);
	clear_dma_ff(i2s_ssi.dma_channel_w);
	set_dma_addr(i2s_ssi.dma_channel_w, buf);
	set_dma_count(i2s_ssi.dma_channel_w, count);
	enable_dma(i2s_ssi.dma_channel_w);
	release_dma_lock(flags);
	
	FUNC_END

}


static void dbmx_audio_start_input(int dev, unsigned long buf, int count, int intrflag)
{		
	unsigned long flags;
	
	FUNC_START
	
	if (judge[1] != 1)
	{
		judge[1] = 1;
		FUNC_END
		return;
	}
	DPRINTK("Re-enabling dma_channel_r(%d)\n", i2s_ssi.dma_channel_r);
	flags = claim_dma_lock();
	disable_dma(i2s_ssi.dma_channel_r);
	clear_dma_ff(i2s_ssi.dma_channel_r);
	set_dma_addr(i2s_ssi.dma_channel_r, buf);	// get data from SSI FIFO to buf
	set_dma_count(i2s_ssi.dma_channel_r, count);
	enable_dma(i2s_ssi.dma_channel_r);
	release_dma_lock(flags);
	
	FUNC_END

}

/****************************************************************************************************** 
* Input:             
*       dev:           The major number of the device.
*       state:         the result of (audio_operation->go)*(audio_operation->enable_bits).
*       
* Value Returned:    None.

* Description: This routine will be used to start the transfer.
*
*******************************************************************************************************/
static void dbmx_audio_trigger(int dev, int state)
{
	uint32_t flags;
	
	FUNC_START
	
	save_flags(flags);
	cli();
	state &= open_mode;

	if (state & PCM_ENABLE_OUTPUT)
	{
		if (judge[0] == 1)
		{
			DPRINTK("Enabling SSI transfer (TX)...\n");
			// Enable transmit
			_reg_SSI_SCR(TX_SSI) |= 0x02;
		}
	}
	
	if(state & PCM_ENABLE_INPUT)
	{
		if (judge[1] == 1)
		{
			DPRINTK("Enabling SSI transfer (RX)...\n");
			// Enable receive
			_reg_SSI_SCR(RX_SSI) |= 0x04;
		}
	}

	restore_flags(flags);
	
	FUNC_END
}
/****************************************************************************************************** 
* Input:             
*       dev:         The major number of the device.
*       bsize:       
*       bcount:      
*
* Value Returned:    0      success.
*                    -1     falure.
*
* Description: This routine will be used to check the key registers.
*
*******************************************************************************************************/
static int dbmx_audio_prepare_for_io(int dev, int bsize, int bcount)
{	
	FUNC_START
	
	mw8731SetSamplingCtrl(ssi_speed);

	switch(ssi_speed)
	{
	case 44100:
		//16-bit, 4 word/frame (left = slot 0, right = slot 2)
		_reg_SSI_STCCR(TX_SSI) = _reg_SSI_SRCCR(RX_SSI) = 0x0000E300;
		if (ssi_channel == 2)
		{	
			printk("16 bit/sample, 44.1 kHz, stereo\n");
			
			_reg_SSI_SCR(TX_SSI) |= 1<<3; // network mode
			
			_reg_SSI_STMSK(TX_SSI) = _reg_SSI_SRMSK(RX_SSI) = ~(1<<0 | 1<<2);
		}
		else // (ssi_channel == 1)
		{
			printk("16 bit/sample, 44.1 kHz, mono\n");
			
			_reg_SSI_SCR(TX_SSI) |= 1<<3; // network mode
				
			_reg_SSI_STMSK(TX_SSI) = _reg_SSI_SRMSK(RX_SSI) = ~(1<<0);
		}
		break;
	case 8000:
		//16-bit, 22 word/frame (left = slot 0, right = slot 11)
		_reg_SSI_STCCR(TX_SSI) = _reg_SSI_SRCCR(RX_SSI) = 0x0000F500;
		if (ssi_channel == 2)
		{
			printk("16 bit/sample, 8 kHz, stereo\n");
			
			_reg_SSI_SCR(TX_SSI) |= 1<<3; // network mode
			
			_reg_SSI_STMSK(TX_SSI) = _reg_SSI_SRMSK(RX_SSI) = ~(1<<0 | 1<<11);
		}
		else // (ssi_channel == 1)
		{
			printk("16 bit/sample, 8 kHz, mono\n");
			
			_reg_SSI_SCR(TX_SSI) |= 1<<3; // network mode
			
			_reg_SSI_STMSK(TX_SSI) = _reg_SSI_SRMSK(RX_SSI) = ~(1<<0);
		}
		break;
	}

	FUNC_END
	return 0;
}

static struct audio_driver dbmx_audio_driver =
{
		owner:				THIS_MODULE,
		open:				dbmx_audio_open,
		close:				dbmx_audio_close,
		output_block:		dbmx_audio_output_block,
		start_input:		dbmx_audio_start_input,
		ioctl:				dbmx_audio_ioctl,
		prepare_for_input:	dbmx_audio_prepare_for_io,
		prepare_for_output:	dbmx_audio_prepare_for_io,
		halt_io:			dbmx_audio_reset,
		trigger:			dbmx_audio_trigger,
		set_speed:			dbmx_audio_setspeed,
		set_bits:			dbmx_audio_setbits,
		set_channels:		dbmx_audio_setchannel,
};

static struct mixer_operations dbmx_mixer_operations =
{
	owner:		THIS_MODULE,
	id:			"DBMX",
	name:		"dbmx-mw8731-audio Mixer Driver",
	ioctl:		dbmx_mixer_ioctl
};


static int dbmx_pm_callback(struct pm_dev *pmdev, pm_request_t rqst, void *data)
{
	FUNC_START

	switch (rqst)
	{
	case PM_SUSPEND:
		if (ssi_busy == 1) 
		{
			judge[2] = 0;
			dbmx_audio_reset(dbmx_audiodev);
		}
		break;
	case PM_RESUME:
		if (ssi_busy == 1)
		{
			judge[2] = 1;
			dbmx_audio_reset(dbmx_audiodev);
		}
		break;
	}
	
	FUNC_END
	return 0;
}

/*
 *DMA Interrupt Handler is designed here.
 */
void ssi_DMA_Tx_Handler(void* arg)
{
	uint32_t flags;
	uint32_t status = *(uint32_t *)arg;
	FUNC_START

	save_flags(flags);

	if (status & DMA_DONE)
	{
		if (_reg_SSI_SISR(TX_SSI) & (1<<8))
		{
			printk("!!!!! TX FIFO Underrun!\n");
		}
		DMAbuf_outputintr(dbmx_audiodev, 1);
	}
	else
	{
		if (status & DMA_BURST_TIMEOUT)
			printk("%s: DMA_BURST_TIMEOUT\n", __FUNCTION__);
		if (status & DMA_TRANSFER_ERROR)
			printk("%s: DMA_TRANSFER_ERROR\n", __FUNCTION__);
		if (status & DMA_BUFFER_OVERFLOW)
			printk("%s: DMA_BUFFER_OVERFLOW\n", __FUNCTION__);
		if (status & DMA_REQUEST_TIMEOUT)
			printk("%s: DMA_REQUEST_TIMEOUT\n", __FUNCTION__);
			
		dbmx2_dump_dma_register(i2s_ssi.dma_channel_w);
	}
	
	restore_flags(flags);
	
	FUNC_END
}

void ssi_DMA_Rx_Handler(void* arg)
{
	uint32_t flags;
	uint32_t status = *(uint32_t *)arg;	
	FUNC_START

	save_flags(flags);	

	if(status & DMA_DONE)
	{
		if (_reg_SSI_SISR(RX_SSI) & (1<<10))
		{
			printk("!!!!! RX FIFO Overrrun!\n");
		}
		DMAbuf_inputintr(dbmx_audiodev);
	}
	else
	{
		if (status & DMA_BURST_TIMEOUT)
			printk("%s: DMA_BURST_TIMEOUT\n", __FUNCTION__);
		if (status & DMA_TRANSFER_ERROR)
			printk("%s: DMA_TRANSFER_ERROR\n", __FUNCTION__);
		if (status & DMA_BUFFER_OVERFLOW)
			printk("%s: DMA_BUFFER_OVERFLOW\n", __FUNCTION__);
		if (status & DMA_REQUEST_TIMEOUT)
			printk("%s: DMA_REQUEST_TIMEOUT\n", __FUNCTION__);
			
		dbmx2_dump_dma_register(i2s_ssi.dma_channel_r);
	}
	
	restore_flags(flags);
	
	FUNC_END
}


/****************************************************************************************************** 
* Input:            None             
*
* Value Returned:   0 if success; anything else if otherwise
*
* Description:		Module entry function
*
*******************************************************************************************************/
int __init init_dbmx_audio(void)
{
	dma_request_t	ssi_dma_req = {0};

	FUNC_START

	//**********************
	// Get TX DMA
	//**********************
	if (-1 == (i2s_ssi.dma_channel_w = dbmx_allocate_dma("DBMX SSI TX")))
	{
		printk("%s: Failed to acquire TX DMA\n", __FUNCTION__);
		return -1;
	}
	// Configure the dma request members that do not change
	ssi_dma_req.burstLength = 8;
	// DMA interrupt config
	ssi_dma_req.callback = (dma_callback_t)ssi_DMA_Tx_Handler;
	// source config
	ssi_dma_req.sourceType = DMA_TYPE_LINEAR;
	ssi_dma_req.sourcePort = DMA_MEM_SIZE_32; 
	// dest config
	ssi_dma_req.destType = DMA_TYPE_FIFO;
	ssi_dma_req.destAddr = (uint32_t *)0x10010000;	//SSI1 Tx data register 0
	ssi_dma_req.destPort = DMA_MEM_SIZE_16;
	// request config
	ssi_dma_req.request = DMA_REQ_SSI1_TX0; 	// request source: SSI1 TX0 FIFO
	_reg_DMA_CCR(i2s_ssi.dma_channel_w) |= 1<<3;
	_reg_DMA_RTOR(i2s_ssi.dma_channel_w) = 0x500;
	
	
	dbmx2_dma_set_config(i2s_ssi.dma_channel_w, &ssi_dma_req);

	//**********************
	// Get RX DMA
	//**********************
	if (-1 == (i2s_ssi.dma_channel_r = dbmx_allocate_dma("DBMX SSI RX")))
	{
		printk("%s: Failed to acquire RX DMA\n", __FUNCTION__);
		sound_free_dma(i2s_ssi.dma_channel_w);
		return -1;
	}
	// Configure the dma request members that do not change
	ssi_dma_req.burstLength = 12;
	// DMA interrupt config
	ssi_dma_req.callback = (dma_callback_t)ssi_DMA_Rx_Handler;
	// source config
	ssi_dma_req.sourceType = DMA_TYPE_FIFO;
	ssi_dma_req.sourceAddr = (uint32_t *)0x10010008;	//SSI1 Rx data register 0
	ssi_dma_req.sourcePort = DMA_MEM_SIZE_16; 
	// dest config
	ssi_dma_req.destType = DMA_TYPE_LINEAR;
	ssi_dma_req.destPort = DMA_MEM_SIZE_32;
	// request config
	ssi_dma_req.request = DMA_REQ_SSI1_RX0; 	// request source: SSI1 RX0 FIFO
	_reg_DMA_CCR(i2s_ssi.dma_channel_r) |= 1<<3;
	_reg_DMA_RTOR(i2s_ssi.dma_channel_r) = 0x500;

	dbmx2_dma_set_config(i2s_ssi.dma_channel_r, &ssi_dma_req);

	//**********************
	// Install audio driver
	//**********************
	if ((dbmx_audiodev = sound_install_audiodrv(AUDIO_DRIVER_VERSION,
					"dbmx audio (ssi-mw8731) driver",
					&dbmx_audio_driver,
					sizeof(struct audio_driver),
					DMA_DUPLEX,
					AFMT_S16_LE,
					NULL,
					i2s_ssi.dma_channel_w,
					i2s_ssi.dma_channel_r)) < 0)
	{
		printk("Too many audio devices in the system!\n");
		sound_free_dma(i2s_ssi.dma_channel_w);
		sound_free_dma(i2s_ssi.dma_channel_r);
		return -1;
	}

	//**********************
	// Install mixer driver
	//**********************
   	if ((dbmx_mixerdev = sound_install_mixer(MIXER_DRIVER_VERSION,
					dbmx_mixer_operations.name,
					&dbmx_mixer_operations,
					sizeof(struct mixer_operations),
					NULL)) < 0)
	{
		printk("Too many mixer devices in the system!\n");
		sound_free_dma(i2s_ssi.dma_channel_w);
		sound_free_dma(i2s_ssi.dma_channel_r);
		sound_unload_mixerdev(dbmx_mixerdev);
		return -1;
	}

	// pm setup
//	dbmx_pm_setup();
		
	FUNC_END
	return 0;
	
}

/****************************************************************************************************** 
* Input:            None             
*
* Value Returned:   None   
*
* Description: 		Module release function
*
*******************************************************************************************************/
void __exit cleanup_dbmx_audio(void)
{
	FUNC_START

	if (i2s_ssi.dma_channel_w >= 0)
	{
		sound_free_dma(i2s_ssi.dma_channel_w);
	}
	
	if (i2s_ssi.dma_channel_r >= 0)
	{
		sound_free_dma(i2s_ssi.dma_channel_r);
	}
	
	if(dbmx_audiodev != -1)
		sound_unload_audiodev(dbmx_audiodev);
	
	if(dbmx_mixerdev != -1)	
		sound_unload_mixerdev(dbmx_mixerdev);

//	pm_unregister(pmdev);

	FUNC_END
}



module_init(init_dbmx_audio);
module_exit(cleanup_dbmx_audio);


MODULE_AUTHOR("Motorola");
MODULE_DESCRIPTION("Audio Device Driver for DBMX");
MODULE_LICENSE("GPL");
