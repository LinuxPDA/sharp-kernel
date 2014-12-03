/*
 * Copyright (C) 1996 Paul Mackerras.
 */
#include <linux/config.h>
#include <linux/string.h>
#include <asm/machdep.h>
#include <asm/io.h>
#include <asm/page.h>
#include <linux/adb.h>
#include <linux/pmu.h>
#include <linux/cuda.h>
#include <linux/kernel.h>
#include <asm/prom.h>
#include <asm/feature.h>
#include <asm/processor.h>
#include <asm/delay.h>
#ifdef CONFIG_SMP
#include <asm/bitops.h>
#endif

#define TXRDY 0x20
#define RXRDY 0x01

static volatile unsigned char *sccc, * volatile sccd;
extern void xmon_printf(const char *fmt, ...);


extern void *comport1;
void
xmon_map_scc(void)
{
	/* should already be mapped by the kernel boot */
	sccd = (volatile unsigned char *) (((unsigned long)comport1));
	sccc = (volatile unsigned char *) (((unsigned long)comport1)+5);
}

static int scc_initialized = 0;

void xmon_init_scc(void);

int
xmon_write(void *handle, void *ptr, int nb)
{
	char *p = ptr;
	int i, c, ct;

#ifdef CONFIG_SMP
	static unsigned long xmon_write_lock;
	int lock_wait = 1000000;
	int locked;

	while ((locked = test_and_set_bit(0, &xmon_write_lock)) != 0)
		if (--lock_wait == 0)
			break;
#endif

	if (!scc_initialized)
		xmon_init_scc();
	ct = 0;
	for (i = 0; i < nb; ++i) {
		while ((*sccc & TXRDY) == 0)
			/* spin for ready */;
		c = p[i];
		if (c == '\n' && !ct) {
			c = '\r';
			ct = 1;
			--i;
		} else {
			ct = 0;
		}
		sccd[3] &= ~0x80;	/* reset DLAB */
		eieio();
		*sccd = c;
		eieio();
	}

#ifdef CONFIG_SMP
	if (!locked)
		clear_bit(0, &xmon_write_lock);
#endif
	return nb;
}


int
xmon_read(void *handle, void *ptr, int nb)
{
    char *p = ptr;
    int i;

    if (!scc_initialized)
	xmon_init_scc();
    for (i = 0; i < nb; ++i) {
	while ((*sccc & RXRDY) == 0)
	    /* spin for ready */;
	sccd[3] &= ~0x80;	/* reset DLAB */
	eieio();
	*p++ = *sccd;
    }
    return i;
}

int
xmon_read_poll(void)
{
	if ((*sccc & RXRDY) == 0)
		return -1;
	sccd[3] &= ~0x80;	/* reset DLAB */
	eieio();
	return *sccd;
}

void
xmon_init_scc()
{
	sccd[3] = 0x83; eieio();	/* LCR = 8N1 + DLAB */
	sccd[0] = 12; eieio();		/* DLL = 9600 baud */
	sccd[1] = 0; eieio();
	sccd[2] = 0; eieio();		/* FCR = 0 */
	sccd[3] = 3; eieio();		/* LCR = 8N1 */
	sccd[1] = 0; eieio();		/* IER = 0 */
	scc_initialized = 1;
}


void *xmon_stdin;
void *xmon_stdout;
void *xmon_stderr;

void
xmon_init(void)
{
}

int
xmon_putc(int c, void *f)
{
    char ch = c;

    if (c == '\n')
	xmon_putc('\r', f);
    return xmon_write(f, &ch, 1) == 1? c: -1;
}

int
xmon_putchar(int c)
{
    return xmon_putc(c, xmon_stdout);
}

int
xmon_fputs(char *str, void *f)
{
    int n = strlen(str);

    return xmon_write(f, str, n) == n? 0: -1;
}

int
xmon_readchar(void)
{
    char ch;

    for (;;) {
	switch (xmon_read(xmon_stdin, &ch, 1)) {
	case 1:
	    return ch;
	case -1:
	    xmon_printf("read(stdin) returned -1\r\n", 0, 0);
	    return -1;
	}
    }
}

static char line[256];
static char *lineptr;
static int lineleft;


int
xmon_getchar(void)
{
    int c;

    if (lineleft == 0) {
	lineptr = line;
	for (;;) {
	    c = xmon_readchar();
	    if (c == -1 || c == 4)
		break;
	    if (c == '\r' || c == '\n') {
		*lineptr++ = '\n';
		xmon_putchar('\n');
		break;
	    }
	    switch (c) {
	    case 0177:
	    case '\b':
		if (lineptr > line) {
		    xmon_putchar('\b');
		    xmon_putchar(' ');
		    xmon_putchar('\b');
		    --lineptr;
		}
		break;
	    case 'U' & 0x1F:
		while (lineptr > line) {
		    xmon_putchar('\b');
		    xmon_putchar(' ');
		    xmon_putchar('\b');
		    --lineptr;
		}
		break;
	    default:
		if (lineptr >= &line[sizeof(line) - 1])
		    xmon_putchar('\a');
		else {
		    xmon_putchar(c);
		    *lineptr++ = c;
		}
	    }
	}
	lineleft = lineptr - line;
	lineptr = line;
    }
    if (lineleft == 0)
	return -1;
    --lineleft;
    return *lineptr++;
}

char *
xmon_fgets(char *str, int nb, void *f)
{
    char *p;
    int c;

    for (p = str; p < str + nb - 1; ) {
	c = xmon_getchar();
	if (c == -1) {
	    if (p == str)
		return 0;
	    break;
	}
	*p++ = c;
	if (c == '\n')
	    break;
    }
    *p = 0;
    return str;
}

void
xmon_enter(void)
{
#ifdef CONFIG_ADB_PMU
	pmu_suspend();
#endif
}

void
xmon_leave(void)
{
#ifdef CONFIG_ADB_PMU
	pmu_resume();
#endif
}
