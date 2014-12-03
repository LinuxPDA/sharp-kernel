/*
 * Kernel Debugger Console I/O handler
 *
 * Copyright (C) 1999 Silicon Graphics, Inc.
 * Copyright (C) Scott Lurndal (slurn@engr.sgi.com)
 * Copyright (C) Scott Foehner (sfoehner@engr.sgi.com)
 * Copyright (C) Srinivasa Thirumalachar (sprasad@engr.sgi.com)
 *
 * See the file LIA-COPYRIGHT for additional information.
 *
 * Written March 1999 by Scott Lurndal at Silicon Graphics, Inc.
 *
 * Modifications from:
 *	Chuck Fleckenstein		1999/07/20
 *		Move kdb_info struct declaration to this file
 *		for cases where serial support is not compiled into
 *		the kernel.
 *
 *	Masahiro Adegawa		1999/07/20
 *		Handle some peculiarities of japanese 86/106
 *		keyboards.
 *
 *	marc@mucom.co.il		1999/07/20
 *		Catch buffer overflow for serial input.
 *
 *      Scott Foehner
 *              Port to ia64
 *
 *	Scott Lurndal			2000/01/03
 *		Restructure for v1.0
 *
 *	Keith Owens			2000/05/23
 *		KDB v1.2
 *
 *	Andi Kleen			2000/03/19
 *		Support simultaneous input from serial line and keyboard.
 */

#include <linux/kernel.h>
#include <asm/io.h>
#include <linux/wait.h>
#include <linux/delay.h>
#include <linux/pc_keyb.h>
#include <linux/console.h>
#include <linux/ctype.h>
#include <linux/keyboard.h>
#include <linux/serial_reg.h>

#include <linux/kdb.h>
#include <linux/kdbprivate.h>
#undef FILE
#include "nonstdio.h"

int kdb_port;

/*
 * This module contains code to read characters from the keyboard or a serial
 * port.
 *
 * It is used by the kernel debugger, and is polled, not interrupt driven.
 *
 */


void
kdb_resetkeyboard(void)
{
#if 0
	kdb_kbdsend(KBD_CMD_ENABLE);
#endif
}

int inchar(void);

#if defined(CONFIG_VT)
/*
 * Check if the keyboard controller has a keypress for us.
 * Some parts (Enter Release, LED change) are still blocking polled here,
 * but hopefully they are all short.
 */
static int get_kbd_char(void)
{
	int keychar;
	keychar = inchar();
	if (keychar == '\n')
	{
	    kdb_printf("\n");
	}
	/*
	 * echo the character.
	 */
	kdb_printf("%c", keychar);

	return keychar ;
}
#endif /* CONFIG_VT */


typedef int (*get_char_func)(void);

static get_char_func poll_funcs[] = {
#if defined(CONFIG_VT)
	get_kbd_char,
#endif
	NULL
};
void flush_input(void);
char *
kdba_read(char *buffer, size_t bufsize)
{
	char	*cp = buffer;
	char	*bufend = buffer+bufsize-2;	/* Reserve space for newline and null byte */
	flush_input();
	for (;;) {
		int key;
		get_char_func *f;
		for (f = &poll_funcs[0]; ; ++f) {
			if (*f == NULL)
				f = &poll_funcs[0];
			key = (*f)();
			if (key != -1)
				break;
		}

		/* Echo is done in the low level functions */
		switch (key) {
		case '\b': /* backspace */
			if (cp > buffer) {
				xmon_printf("\b \b");
				--cp;
			}
			break;
		case '\n': /* enter */
			*cp++ = '\n';
			*cp++ = '\0';
			return buffer;
		default:
			if (cp < bufend)
				*cp++ = key;
			break;
		}
	}
}
static char line[256];
static char *lineptr;
void flush_input(void)
{
	lineptr = NULL;
}
int inchar(void)
{
	if (lineptr == NULL || *lineptr == 0)
	{
		if (xmon_fgets(line, sizeof(line), xmon_stdin) == NULL)
		{
			lineptr = NULL;
			return EOF;
		}
		lineptr = line;
	}
	return *lineptr++;
}
