/*
 * linux/include/asm-arm/arch-brutus/uncompress.h
 *
 * (C) 1999 Nicolas Pitre <nico@cam.org>
 *
 * Reorganised to be machine independent.
 *
 * ChangLog:
 *	30-Jul-2002 Lineo Japan, Inc.  for 2.4.18
 *      29-Jan-2003 Sharp Corporation
 */

#include "hardware.h"

/* sa1100_setup() will perform any special initialization for UART, etc. */
extern void sa1100_setup( int arch_id );
#define arch_decomp_setup()	sa1100_setup(arch_id)

/*
 * The following code assumes the serial port has already been
 * initialized by the bootloader.  We search for the first enabled
 * port in the most probable order.  If you didn't setup a port in
 * your bootloader then nothing will appear (which might be desired).
 */

#define UART(x)		(*(volatile unsigned long *)(serial_port + (x)))

static void puts( const char *s )
{
	unsigned long serial_port;


#if defined(CONFIG_SA1100_COLLIE)
  return;
#endif

	do {
		serial_port = _Ser3UTCR0;
		if (UART(UTCR3) & UTCR3_TXE) break;
		serial_port = _Ser1UTCR0;
		if (UART(UTCR3) & UTCR3_TXE) break;
		serial_port = _Ser2UTCR0;
		if (UART(UTCR3) & UTCR3_TXE) break;
		return;
	} while (0);

	for (; *s; s++) {
		/* wait for space in the UART's transmiter */
		while (!(UART(UTSR1) & UTSR1_TNF))
			barrier();

		/* send the character out. */
		UART(UTDR) = *s;

		/* if a LF, also do CR... */
		if (*s == 10) {
			while (!(UART(UTSR1) & UTSR1_TNF))
				barrier();
			UART(UTDR) = 13;
		}
	}
}

/*
 * Nothing to do for these
 */
#ifndef CONFIG_SA1100_COLLIE
#define arch_decomp_setup()
#endif
#define arch_decomp_wdog()
