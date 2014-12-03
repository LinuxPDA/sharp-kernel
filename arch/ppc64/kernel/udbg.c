/*
 * NS16550 Serial Port (uart) debugging stuff.
 *
 * c 2001 PPC 64 Team, IBM Corp
 *
 * NOTE: I am trying to make this code avoid any static data references to
 *  simplify debugging early boot.  We'll see how that goes...
 *
 * To use this call udbg_init() first.  It will init the uart to 9600 8N1.
 * You may need to update the COM1 define if your uart is at a different addr.
 *
 *      This program is free software; you can redistribute it and/or
 *      modify it under the terms of the GNU General Public License
 *      as published by the Free Software Foundation; either version
 *      2 of the License, or (at your option) any later version.
 */

#include <stdarg.h>
#include <asm/processor.h>
#include <asm/Naca.h>
#define WANT_PPCDBG_TAB /* Only defined here */
#include <asm/uaccess.h>
#include <asm/ppcdebug.h>

extern struct Naca *naca;
extern int _machine;

struct NS16550 {
    /* this struct must be packed */
    unsigned char rbr;  /* 0 */
    unsigned char ier;  /* 1 */
    unsigned char fcr;  /* 2 */
    unsigned char lcr;  /* 3 */
    unsigned char mcr;  /* 4 */
    unsigned char lsr;  /* 5 */
    unsigned char msr;  /* 6 */
    unsigned char scr;  /* 7 */
};

#define thr rbr
#define iir fcr
#define dll rbr
#define dlm ier
#define dlab lcr

#define LSR_DR   0x01  /* Data ready */
#define LSR_OE   0x02  /* Overrun */
#define LSR_PE   0x04  /* Parity error */
#define LSR_FE   0x08  /* Framing error */
#define LSR_BI   0x10  /* Break */
#define LSR_THRE 0x20  /* Xmit holding register empty */
#define LSR_TEMT 0x40  /* Xmitter empty */
#define LSR_ERR  0x80  /* Error */

void *comport1;

static inline void eieio(void) { asm volatile ("eieio" : :); }

void
udbg_init(void)
{
	if ( _machine != _MACH_iSeries ) {
		volatile struct NS16550 *port = (struct NS16550 *)comport1;
		port->lcr = 0x00; eieio();
		port->ier = 0xFF; eieio();
		port->ier = 0x00; eieio();
		port->lcr = 0x80; eieio();	/* Access baud rate */
		port->dll = 12;   eieio();	/* 1 = 115200,  2 = 57600, 3 = 38400, 12 = 9600 baud */
		port->dlm = 0;    eieio();	/* dll >> 8 which should be zero for fast rates; */
		port->lcr = 0x03; eieio();	/* 8 data, 1 stop, no parity */
		port->mcr = 0x03; eieio();	/* RTS/DTR */
		port->fcr = 0x07; eieio();	/* Clear & enable FIFOs */
	}
}


void
udbg_putc(unsigned char c)
{
  if ( _machine != _MACH_iSeries ) {
    volatile struct NS16550 *port = (struct NS16550 *)comport1;
    while ((port->lsr & LSR_THRE) == 0)
      /* wait for idle */;
    port->thr = c; eieio();
    if (c == '\n') {
      /* Also put a CR.  This is for convenience. */
      while ((port->lsr & LSR_THRE) == 0)
	/* wait for idle */;
      port->thr = '\r'; eieio();
    }
  }
  else {
    printk("%c", c);
  }

}

unsigned char
udbg_getc(void)
{
	if ( _machine != _MACH_iSeries ) {
		volatile struct NS16550 *port = (struct NS16550 *)comport1;
		while ((port->lsr & LSR_DR) == 0)
			/* wait for char */;
		return port->rbr;
	}
	return 0;
}

void
udbg_puts(const char *s)
{
    if ( _machine != _MACH_iSeries ) {
	char c;
        if ( s && *s != '\0' )
		while ( ( c = *s++ ) != '\0' )
	        	udbg_putc(c);
        else
	        udbg_puts("NULL");
    }
    else
	printk("%s", s);
}

void
udbg_puthex(unsigned long val)
{
	int i, nibbles = sizeof(val)*2;
	unsigned char buf[sizeof(val)*2+1];
	for (i = nibbles-1;  i >= 0;  i--) {
		buf[i] = (val & 0xf) + '0';
		if (buf[i] > '9')
		    buf[i] += ('a'-'0'-10);
		val >>= 4;
	}
	buf[nibbles] = '\0';
	udbg_puts(buf);
}

void
udbg_printSP(const char *s)
{
    if ( _machine != _MACH_iSeries ) {
	unsigned long sp;
	asm("mr %0,1" : "=r" (sp) :);
	if (s)
	    udbg_puts(s);
	udbg_puthex(sp);
    }
}

void
udbg_printf(const char *fmt, ...)
{
        unsigned char buf[256];

        va_list args;
        va_start(args, fmt);

        vsprintf(buf, fmt, args);
        udbg_puts(buf);

        va_end(args);
}

/* Special print used by PPCDBG() macro */
void
udbg_ppcdbg(unsigned long flags, const char *fmt, ...)
{
		unsigned long active_debugs = flags & naca->debug_switch;
		if ( active_debugs ) {
			va_list ap;
			unsigned char buf[256];
			unsigned long i, len = 0;

			for(i=0; i < PPCDBG_NUM_FLAGS ;i++) {
				if (((1U << i) & active_debugs) && 
				    trace_names[i]) {
					len += strlen(trace_names[i]); 
					udbg_puts(trace_names[i]);
					break;
				}
			}
			sprintf(buf, " [%s]: ", current->comm);
			len += strlen(buf); 
			udbg_puts(buf);

			while(len < 18) {
				udbg_putc(' ');
				len++;
			}

			va_start(ap, fmt);
			vsprintf(buf, fmt, ap);
			udbg_puts(buf);
			va_end(ap);
		}
}

unsigned long
udbg_ifdebug(unsigned long flags)
{
    return (flags & naca->debug_switch);
}

/* Print out an area of memory.
 * The msg will be printed, followed by a newline, followed by the
 * memory dump.
 */
void
udbg_dump(char *msg, void *addr, unsigned len)
{
    if ( _machine != _MACH_iSeries ) {
    int i;
    int j;
    int numrows;
    unsigned char c;
    unsigned char tc;
    char *buff = addr;

    if (msg) {
        udbg_puts(msg);
        udbg_puts("\n");
    }

    if (!buff) {
        udbg_puts("  Danger, Will Robinson, Null Pointer\n");
        return;
    }
    numrows = (len + 15) / 16;

    for (i = 0; i < numrows; i++) {
        udbg_printf("%016lx    ", &buff[i*16]);
        for (j = 0; j < 16; j++) {
            if (j + i * 16 >= len) {
                udbg_puts("  ");
            } else {
                udbg_printf("%02x", (unsigned char)buff[j + i * 16]);
            } /* endif */
            if (j % 8 == 7) {
                udbg_puts(" ");
            } /* endif */
        } /* endfor */

        udbg_puts(" a|");

        for (j = 0; j < 16; j++) {
            if (j + i * 16 >= len) {
                udbg_putc(' ');
            } else {
                c = (unsigned char)buff[j + i * 16];
                tc = c;
                if (tc >= 32 && tc < 127 && tc != ' ') {
                    udbg_putc(tc);
                } else {
                    udbg_putc('.');
                } /* endif */
            } /* endif */
        } /* endfor */
        udbg_putc('|');
        udbg_putc('\n');
    } /* endfor */
    }
}

