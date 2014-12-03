/* -------------------------------------------------------------------- */
/* i2c-voyagergx.h: 							*/
/* -------------------------------------------------------------------- */
/*  This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.	
	
    Copyright 2003 (c) Lineo uSolutions,Inc.
*/
/* --------------------------------------------------------------------	*/

#ifndef I2C_VOYAGERGX_H
#define I2C_VOYAGERGX_H 1

#include <asm/voyagergx_reg.h>

//#define TC56XX
#define OV7640
//#define OV7141

#if defined(TC56XX)
#define PACKET_SIZE		4
#define SERIAL_WRITE_ADDR	((0x56 << 1) + 0)	//0xAC
#define SERIAL_READ_ADDR	((0x56 << 1) + 1)	//0xAD
//#define SERIAL_WRITE_ADDR	((0x57 << 1) + 0)       //0xAE
//#define SERIAL_READ_ADDR	((0x57 << 1) + 0)       //0xAF

#elif defined(OV7640)
#define PACKET_SIZE		2
//#define SERIAL_WRITE_ADDR	((0x42 << 1) + 0)
//#define SERIAL_READ_ADDR	((0x42 << 1) + 1)
#define SERIAL_WRITE_ADDR	(0x42)
#define SERIAL_READ_ADDR	(0x43)

#elif defined(OV7141)
#define PACKET_SIZE		2
#define SERIAL_WRITE_ADDR	((0x42 << 1) + 0)
#define SERIAL_READ_ADDR	((0x42 << 1) + 1)
//#define SERIAL_WRITE_ADDR	((0x43 << 1) + 0)
//#define SERIAL_READ_ADDR	((0x43 << 1) + 1)

#else
#define PACKET_SIZE		16
#endif

/* -------------------------------------------------------------------- */

static unsigned char register_ov7640_7141[] = 
{
0x00, 0x00,  /* AGC Gain control gain setting,Range: [00] to [FF] */
#if !defined(OV7141)
0x01, 0x80,  /* Blue channel gain setting,Range: [00] to [FF] */
0x02, 0x80,  /* Red channel gain setting,Range: [00] to [FF] */
0x03, 0x84,  /* Image Format Color saturation value, 
		Bit[7:4]: Saturation value Range: [0] to [F] 
		Bit[3:0]: Reserved */
0x04, 0x34,  /* Image Format Color hue control 
		Bit[7:6]: Reserved 
		Bit[5]: Hue Enable 
		Bit[4:0]: Hue setting */
0x05, 0x3E,  /* Red/Blue Pre-Amplifier gain setting
		Bit[7:4]: Red channel pre-amplifier gain setting, Range: [0] to [F]
		Bit[3:0]: Blue channel pre-amplifier gain setting,Range: [0] to [F] */
#endif
0x06, 0x80,   /* Brightness setting, Range: [00] to [FF] */
/* 0x07-0x09 Reserved */
/* 0x0A, 0x76, Product ID number (Read only) */
/* 0x0B, 0x48, Product version number (Read only) */
/* 0x0C-0x0F Reserved */
0x10, 0x41,  /* Exposure Value */
0x11, 0x00,  /* Data Format and Internal Clock
		Bit[7:6]: Data Format HSYNC/VSYNC Polarity
			00: HSYNC = NEG VSYNC = POS
			01: HSYNC = NEG VSYNC = NEG
			10: HSYNC = POS VSYNC = POS
			11: HSYNC = POS VSYNC = POS
		Bit[5:0]: Internal Clock Pre-Scalar
			Range: [0 0000] to [F FFFF] */
0x12, 0x14,  /* Common Control A
		Bit[7]: SCCB Register Reset
			0: No change
			1: Reset all registers to default values
		Bit[6]: Output Format Mirror Image Enable
		Bit[5]: Reserved
		Bit[4]: Data Format YUV formatting
			0: Y U Y V Y U Y V
			1: U Y V Y U Y V Y (default)
		Bit[3]: Output Format Output Channel Select A
			0: YUV/YCbCr
			1: RGB/Raw RGB
		Bit[2]: AWB Enable
		Bit[1:0]: Reserved */
#if !defined(OV7141)
0x13, 0xA3,  /* Common Control B
		Bit[7:5]: Reserved
		Bit[4]: Data Format ITU-656 Format Enable
		Bit[3]: Reserved
		Bit[2]: SCCB Tri-State Enable Y[7:0]
		Bit[1]: AGC Enable
		Bit[0]: AEC Enable */
#endif
0x14, 0x04,  /* Common Control C
		Bit[7:6]: Reserved
		Bit[5]: Output Format Resolution
			0: VGA (640x480)
			1: QVGA (320x240)
		Bit[4]: Reserved
		Bit[3]: Data Format HREF Polarity
			0: HREF Positive
			1: HREF Negative
		Bit[2:0]: Reserved */
0x15, 0x00,   /* Common Control D
		Bit[7]: Data Format Output Flag Bit Disable
			0: Frame = 254 data bits (00/FF = Reserved flag bits)
			1: Frame = 256 data bits
		Bit[6]: Data Format Y[7:0]-PCLK Reference Edge
			0: Y[7:0] data out on PCLK falling edge
			1: Y[7:0] data out on PCLK rising edge
		Bit[5:1]: Reserved
		Bit[0]: Data Format UV Sequence Exchange
			0: V Y U Y V Y U Y
			1: U Y V Y U Y V Y */
/* 0x16 Reserved */
0x17, 0x1A,  /* Horizontal Frame (HREF Column) Start */
0x18, 0xBA,  /* Horizontal Frame (HREF Column) Stop */
0x19, 0x03,  /* Vertical Frame (Row) Start */
0x1A, 0xF3,  /* Vertical Frame (Row) Stop */
0x1B, 0x00,  /* Data Format Pixel Delay Select
	(Delays timing of the Y[7:0] data relative to HREF in pixel units)
	Range: [00] (No delay) to [FF] (256 pixel delay) */
/* 0x1C, 0x7F, Manufacturer ID Byte High (Read only = 0x7F) */
/* 0x1D, 0xA2, Manufacturer ID Byte Low (Read only = 0xA2) */
/* 0x1E Reserved */
0x1F, 0x01,  /* Output Format C Format Control
		Bit[7:5]: Reserved
		Bit[4]: Output Format ¨C RGB:565 Enable
		Bit[3]: Reserved
		Bit[2]: Output Format ¨C RGB:555 Enable
		Bit[1:0]: Reserved */
0x20, 0xC0,  /* Common Control E
		Bit[7]: Reserved
		Bit[6]: AEC C Digital Averaging Enable
		Bit[5]: Reserved
		Bit[4]: Image Quality C Edge Enhancement Enable
		Bit[3:1]: Reserved
		Bit[0]: Y[7:0] 2X IOL / IOH Enable */
/* 0x21-0x23 Reserved */
0x24, 0x10,  /* AGC/AEC C Stable Operating Region C Upper Limit */
0x25, 0x8A,  /* AGC/AEC C Stable Operating Region C Lower Limit */
0x26, 0xA2,  /* Common Control F
		Bit[7:3]: Reserved
		Bit[2]: Data Format C Output Data MSB/LSB Swap Enable
		(LSB MSB (Y[7]) and MSB LSB (Y[0])
		Bit[1:0]: Reserved */
0x27, 0xE2,  /* Common Control G
		Bit[7:5]: Reserved
		Bit[4]: Color Matrix C RGB Crosstalk Compensation Enable
		(Used to increase each color filter efficiency)
		Bit[3:2]: Reserved
		Bit[1]: Data Format C Output Full Range Enable
			0: Output Range = [10] to [F0] (224 bits)
			1: Output Range = [01] to [FE] (254/256 bits)
		Bit[0]: Reserved */
0x28, 0x20,  /* Common Control H
		Bit[7]: Output Format C RGB Output Select
			0: RGB
			1: Raw RGB
		Bit[6]: Device Select
			0: OV7640
			1: OV7141
		Bit[5]: Output Format C Scan Select
			0: Interlaced
			1: Progressive
		Bit[4:0]: Reserved */
/* 0x29, 0x00,  Common Control I
		Bit[7:2]: Reserved
		Bit[1:0]: Device Version (Read-only) */
0x2A, 0x00,  /* Output Format Frame Rate Adjust High
		Bit[7]: Data Format Frame Rate Adjust Enable
		Bit[6:5]: Data Format Frame Rate Adjust Setting MSB
		FRA[9:0] = MSB + LSB = FRARH[6:5] + FRARL[7:0]
		Bit[4]: A/D UV Channel  Pixel DelayEnable
		Bit[3:0]: Reserved */
0x2B, 0x00,  /* Data Format Frame Rate Adjust Setting LSB
		FRA[9:0] = MSB + LSB = FRARH[6:5] + FRARL[7:0] */
/* 0x2C Reserved */
0x2D, 0x81,  /* Common Control J
		Bit[7:3]: Reserved
		Bit[2]: AEC Band Filter Enable
		Bit[1:0]: Reserved */
/* 0x2E-0x5F Reserved */
0x60, 0x06,  /* Signal Process Control B
		Bit[7]: AGC 1.5x Multiplier (Pre-amplifier) Enable
		Bit[6:0]: Reserved */
/* 0x61-0x6B Reserved */
#if !defined(OV7141)
0x6C, 0x11,  /* Color Matrix RGB Crosstalk Compensation R Channel */
0x6D, 0x01,  /* Color Matrix RGB Crosstalk Compensation G Channel */
0x6E, 0x06,  /* Color Matrix RGB Crosstalk Compensation B Channel */
#endif
/* 0x6F-0x70 Reserved */
0x71, 0x00,  /* Common Mode Control L
		Bit[7]: Reserved
		Bit[6]: Data Format PCLK output gated by HREF Enable
		Bit[5]: Data Format Output HSYNC on HREF Pin Enable
		Bit[4]: Reserved
		Bit[3:2]: Data Format HSYNC Rising Edge Delay MSB
		Bit[1:0]: Data Format HSYNC Falling Edge Delay MSB */
0x72, 0x10,  /* Data Format HSYNC Rising Edge Delay LSB
		HSYNCR[9:0] = MSB + LSB = COML[3:2] + HSDYR[7:0]
		Range 000 to 762 pixel delays */ 
0x73, 0x50,   /* Data Format HSYNC Falling Edge Delay LSB
		HSYNCF[9:0] = MSB + LSB = COML[1:0] + HSDYF[7:0]
		. Range 000 to 762 pixel delays */
0x74, 0x20,  /* Common Mode Control M
		Bit[7]: Reserved
		Bit[6:5]: AGC Maximum Gain Select
			00: +6 dB
			01: +12 dB
			10: +6 dB
			11: +18 dB
		Bit[4:0]: Reserved */
0x75, 0x02,  /* Common Mode Control N
		Bit[7]: Output Format Vertical Flip Enable
		Bit[6:0]: Reserved */
0x76, 0x00,  /* Common Mode Control O
		Bit[7:6]: Reserved
		Bit[5]: Standby Mode Enable
		Bit[4:3]: Reserved
		Bit[2]: SCCB Tri-State Enable VSYNC, HREF and PCLK
		Bit[1:0]: Reserved */
/* 0x77-0x7D Reserved */ 
0x7E, 0x00,  /* AEC Digital Y/G Channel Average
		(Automatically updated by AGC/AEC, user can only read the values) */
#if !defined(OV7141)
0x7F, 0x00,  /* AEC Digital R/V Channel Average
		(Automatically updated by AGC/AEC, user can only read the values) */
0x80, 0x00,  /* AEC Digital B/U Channel Average
		(Automatically updated by AGC/AEC, user can only read the values) */
#endif
0xFF, 0xFF   /* Terminate Code */
};

#endif /* I2C_VOYAGERGX_H */
