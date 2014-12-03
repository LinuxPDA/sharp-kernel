/*
 * debug-serial.c: for porting bootstrap-phase
 *
 * Time-stamp: <02/07/30 20:47:43 imai>
 *
 */
#include <asm/types.h>

#define BASE	0xaa00c040
#define TH	(*(volatile u8 *)(BASE+0))
#define LS	(*(volatile u8 *)(BASE+5))

static inline void serial_putc ( char c )
{
	while ( (LS & (1<<5)) == 0 )
		;
	TH = c;
}

void debug_puts ( char *str )
{
	while ( *str != '\0' ) {
		if ( *str == '\n' )
			serial_putc ( '\r' );
		serial_putc ( *(str++) );
	}
}
