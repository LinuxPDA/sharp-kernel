/*
 * Sharp PDA Driver Header File
 * Copyright (c) 2001 SHARP
 *
 * based on
 * Collie APM Driver Header File
 * Copyright (c) 2001 Lineo Inc.
 *
 * Change Log
 *	26-Jun-2002 SHARP	Add `APM_IOC_GET_BACKPACK_STATE`
 *	12-Dec-2002 Sharp Corporation for Poodle and Corgi
 *	24-Sep-2004 Lineo Solutions, Inc.  for Spitz
 *      20-Dec-2004 Sharp Corporation  for Spitz
 */

#ifndef _SHARP_APM_H
#define _SHARP_APM_H

/*
 *   /proc/apm battery status
 */
#define APM_BATTERY_STATUS_VERY_LOW 0x7f



#define APM_IOC_MAGIC 'A'

#define APM_IOC_GET_AC_STATUS		_IO(APM_IOC_MAGIC, 10)
#define APM_IOC_AUTO_POWER_CANCEL	_IO(APM_IOC_MAGIC, 11)
#define APM_IOC_GET_AUTO_POWER_CANCEL	_IO(APM_IOC_MAGIC, 12)
#define APM_IOC_AUTO_LIGHT_CANCEL	_IO(APM_IOC_MAGIC, 13)
#define APM_IOC_GET_AUTO_LIGHT_CANCEL	_IO(APM_IOC_MAGIC, 14)
#define APM_IOC_GET_AUTO_POWER_TIME	_IO(APM_IOC_MAGIC, 15)
#define APM_IOC_SET_AUTO_POWER_TIME	_IO(APM_IOC_MAGIC, 16)
#define APM_IOC_GET_AUTO_LIGHT_TIME	_IO(APM_IOC_MAGIC, 17)
#define APM_IOC_SET_AUTO_LIGHT_TIME	_IO(APM_IOC_MAGIC, 18)

#define APM_IOC_SET_DAEMON_MODE		_IO(APM_IOC_MAGIC, 20)
#define APM_IOC_RESET_DAEMON_MODE	_IO(APM_IOC_MAGIC, 21)
#define APM_IOC_SFREQ			_IO(APM_IOC_MAGIC, 25)
#define APM_IOC_GFREQ			_IO(APM_IOC_MAGIC, 26)
#define APM_IOC_RESET_PM		_IO(APM_IOC_MAGIC, 28)


#define APM_IOC_BATTERY_MAIN_MODE	_IO(APM_IOC_MAGIC, 30)
#define APM_IOC_BATTERY_BACK_MODE	_IO(APM_IOC_MAGIC, 31)
#define APM_IOC_BATTERY_BACK_CHK	_IO(APM_IOC_MAGIC, 32)
#define APM_IOC_BATTERY_MAIN_CHK	_IO(APM_IOC_MAGIC, 33)
#define APM_IOC_GET_BACKPACK_STATE	_IO(APM_IOC_MAGIC, 34)

#define APM_IOC_GET_ON_MODE		_IO(APM_IOC_MAGIC, 40)
#define APM_IOC_GET_ON_MODE_CLEAR	_IO(APM_IOC_MAGIC, 41)
#define APM_IOC_GET_REGISTER		_IO(APM_IOC_MAGIC, 45)

#define APM_IOC_RESET			_IO(APM_IOC_MAGIC, 50)

#define APM_IOC_GET_HINGE_STATE	_IO(APM_IOC_MAGIC, 60)	/* for C3 */

#define APM_IOC_SETRTC			_IO(APM_IOC_MAGIC, 70)
#define APM_IOC_GETRTC			_IO(APM_IOC_MAGIC, 71)

#define APM_IOC_KICK_BATTERY_CHECK	_IO(APM_IOC_MAGIC, 80)

#define APM_IOC_GET_JACKET_STATE	_IO(APM_IOC_MAGIC, 90)	/* for tosa */
#define APM_IOC_BATTERY_JACKET_CHK	_IO(APM_IOC_MAGIC, 91)
#define APM_IOC_GET_CARDSLOT_ERROR	_IO(APM_IOC_MAGIC, 92)


#define APM_IOCGWUPSRC  _IOR (APM_IOC_MAGIC, 200, int) /* 0x800441c8 */
#define APM_IOCSWUPSRC  _IOWR(APM_IOC_MAGIC, 201, int) /* 0xc00441c9 */
#define APM_IOCGWUPFACT _IOR (APM_IOC_MAGIC, 202, int) /* 0x800441ca */
#define APM_IOCGEVTSRC  _IOR (APM_IOC_MAGIC, 203, int) /* 0x800441cb */
#define APM_IOCSEVTSRC  _IOWR(APM_IOC_MAGIC, 204, int) /* 0xc00441cc */

/* COLLIE_APM_IOCGEVSRC, COLLIE_APM_IOCEEVSRC */
#define APM_EVT_POWER_BUTTON	(1 << 0)
#define APM_EVT_BATTERY_FAULT	(1 << 1)
#define APM_EVT_BATTERY_STATUS	(1 << 2)
#define APM_EVT_LIGHT_CHANGE	(1 << 3)

#define WAKEUP_RTC		0x80000000
#define WAKEUP_HOME_KEY		0x40000000
#define WAKEUP_OK_KEY		0x20000000
#define WAKEUP_MENU_KEY		0x10000000
#define WAKEUP_SYNC_KEY		0x01000000
#define WAKEUP_POWER_KEY	0x02000000
#define WAKEUP_PHS			0x04000000

#define LOCK_FCS_MMC		0x00000001
#define LOCK_FCS_FFUART		0x00000002
#define LOCK_FCS_STUART		0x00000004
#define LOCK_FCS_BTUART		0x00000008
#define LOCK_FCS_IRDA		0x00000010
#define LOCK_FCS_SSP		0x00000020
#define LOCK_FCS_UDC		0x00000040
#define LOCK_FCS_AC97		0x00000080
#define LOCK_FCS_PCMCIA		0x00000100
#define LOCK_FCS_SOUND		0x00000200
#define LOCK_FCS_KEY		0x00000400
#define LOCK_FCS_TOUCH		0x00000800
#define LOCK_FCS_BATTERY	0x00001000
#define LOCK_FCS_AC97_SUB	0x00002000
#define LOCK_FCS_PCMCIA2	0x00004000
#define LOCK_FCS_USBH       	0x00008000
#define LOCK_FCS_USR0		0x01000000
#define LOCK_FCS_USR1		0x02000000
#define LOCK_FCS_USR2		0x04000000
#define LOCK_FCS_USR3		0x08000000
#define LOCK_FCS_USR4		0x10000000
#define LOCK_FCS_USR5		0x20000000
#define LOCK_FCS_USR6		0x40000000
#define LOCK_FCS_USR7		0x80000000

#define LOCK_FCS_SYNC_MSK	(LOCK_FCS_FFUART|LOCK_FCS_UDC)

#define POWER_MODE_MMC		0x00000001
#define POWER_MODE_FFUART	0x00000002
#define POWER_MODE_STUART	0x00000004
#define POWER_MODE_BTUART	0x00000008
#define POWER_MODE_IRDA		0x00000010
#define POWER_MODE_SSP		0x00000020
#define POWER_MODE_UDC		0x00000040
#define POWER_MODE_AC97		0x00000080
#define POWER_MODE_PCMCIA	0x00000100
#define POWER_MODE_SOUND	0x00000200
#define POWER_MODE_KEY		0x00000400
#define POWER_MODE_TOUCH	0x00000800
#define POWER_MODE_BATTERY	0x00001000
#define POWER_MODE_AC97_SUB	0x00002000
#define POWER_MODE_PCMCIA2	0x00004000
#define POWER_MODE_USBH   	0x00008000

#define POWER_MODE_CAUTION_MASK	(POWER_MODE_MMC|POWER_MODE_STUART|POWER_MODE_UDC|POWER_MODE_PCMCIA|POWER_MODE_USBH)

#if defined(CONFIG_ARCH_PXA_TOSA)
#define LOCK_FCS_POWER_MODE_CORRESPOND_MASK	(POWER_MODE_FFUART|POWER_MODE_STUART|POWER_MODE_BTUART|POWER_MODE_IRDA|POWER_MODE_SSP|POWER_MODE_UDC|POWER_MODE_AC97|POWER_MODE_PCMCIA|POWER_MODE_SOUND|POWER_MODE_KEY|POWER_MODE_TOUCH|POWER_MODE_BATTERY|POWER_MODE_AC97_SUB|POWER_MODE_PCMCIA2)
#elif defined(CONFIG_ARCH_PXA_SPITZ)
#define LOCK_FCS_POWER_MODE_CORRESPOND_MASK	(POWER_MODE_FFUART|POWER_MODE_STUART|POWER_MODE_BTUART|POWER_MODE_IRDA|POWER_MODE_SSP|POWER_MODE_UDC|POWER_MODE_AC97|POWER_MODE_SOUND|POWER_MODE_USBH|POWER_MODE_PCMCIA)
#else
#define LOCK_FCS_POWER_MODE_CORRESPOND_MASK	(POWER_MODE_FFUART|POWER_MODE_STUART|POWER_MODE_BTUART|POWER_MODE_IRDA|POWER_MODE_SSP|POWER_MODE_UDC|POWER_MODE_AC97|POWER_MODE_PCMCIA|POWER_MODE_SOUND)
#endif

int change_power_mode(unsigned long, int);
int lock_FCS(unsigned long, int);

#ifdef CONFIG_COLLIE
#include <asm/arch/collie_apm.h>
#endif

#ifdef CONFIG_IRIS
#include <asm/arch/iris_apm.h>
#endif

#endif /* _SHARP_APM_H */
