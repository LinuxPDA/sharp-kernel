/* 
 * Copyright (C) 2000 Jeff Dike (jdike@karaya.com)
 * Licensed under the GPL
 */

#include "linux/posix_types.h"
#include "linux/tty.h"
#include "linux/tty_flip.h"
#include "linux/types.h"
#include "linux/major.h"
#include "linux/kdev_t.h"
#include "linux/console.h"
#include "linux/string.h"
#include "linux/sched.h"
#include "linux/init.h"
#include "linux/interrupt.h"
#include "asm/current.h"
#include "asm/softirq.h"
#include "asm/hardirq.h"
#include "stdio_console.h"
#include "chan.h"
#include "user_util.h"
#include "kern_util.h"

#define MAX_TTYS (8)

static struct tty_driver console_driver;
static struct tty_struct *console_table[MAX_TTYS];
static struct termios *console_termios[MAX_TTYS];
static struct termios *console_termios_locked[MAX_TTYS];
static int console_refcount;

#define INIT_VT \
        { XTERM_IO_CHAN_INIT(-1, "Virtual Console #%d", 1, INIT_STATIC), 0, \
          NULL }

static struct vt {
	struct io_chan chan;
	int count;
	struct tty_struct *tty;
} vts[MAX_TTYS] = { { STDIO_IO_CHAN_INIT(INIT_STATIC), 0, NULL }, 
		    [ 1 ... MAX_TTYS - 1 ] = INIT_VT };

static void console_interrupt(int irq, void *dev, struct pt_regs *unused)
{
	struct vt *term = dev;

	tty_interrupt(&term->chan, term->tty, term->count);
}

DECLARE_MUTEX(stdio_sem);

static int setup_console_irq(int fd, void *data)
{
	return(um_request_irq(CONSOLE_IRQ, fd, console_interrupt, 
			      SA_INTERRUPT | SA_SHIRQ, "console", data));
}

static int open_console(int line, struct tty_struct *tty)
{
	int err;

	down(&stdio_sem);
	err = open_chan_pair(&vts[line].chan, setup_console_irq, &vts[line]);
	if(err < 0){
		printk("Failed to open virtual console %d, errno = %d\n",
		       line, err);
		up(&stdio_sem);
		return(err);
	}
	vts[line].count++;
	vts[line].tty = tty;
	up(&stdio_sem);
	return(0);
}

static int con_open(struct tty_struct * tty, struct file * filp)
{
	int line, ret;

	line = MINOR(tty->device) - tty->driver.minor_start;
	ret = open_console(line, tty);
	update_console_size(CHAN_IN_FD(vts[line].chan), &tty->winsize.ws_row, 
			    &tty->winsize.ws_col);	
	return(ret);
}

static void con_close(struct tty_struct * tty, struct file * filp)
{
	int line;

	line = MINOR(tty->device) - tty->driver.minor_start;
	vts[line].count--;
}

static int con_write(struct tty_struct * tty, int from_user, 
		     const unsigned char *buf, int count)
{
	int line;

	if (in_interrupt() && tty->stopped)
		return 0;
	while (tty->stopped) {
		schedule();
	}	
	line = MINOR(tty->device) - tty->driver.minor_start;
	return(write_chan(&vts[line].chan, buf, count));
}

static int write_room(struct tty_struct * tty)
{
	return(1024);
}

static void set_termios(struct tty_struct *tty, struct termios * old)
{
}

static int chars_in_buffer(struct tty_struct *tty)
{
	return(0);
}

extern void tty_register_devfs(struct tty_driver *driver, unsigned int flags,
			       unsigned int minor);
extern void tty_unregister_devfs (struct tty_driver *driver, unsigned minor);

int stdio_init(void)
{
	int i;

	for(i=0;i<sizeof(vts)/sizeof(vts[0]);i++){
		if((vts[i].chan.in.type == XTERM) && 
		   (vts[i].chan.in.init_pri == INIT_STATIC))
			vts[i].chan = 
				((struct io_chan) 
				 XTERM_IO_CHAN_INIT(i, "Virtual Console #%d",
						    1, INIT_STATIC));
	}

	printk(KERN_INFO "Initializing stdio console driver\n");
	memset(&console_driver, 0, sizeof(struct tty_driver));
	console_driver.magic = TTY_DRIVER_MAGIC;
	console_driver.driver_name = "stdio console";
	console_driver.name = "ttys/%d";
	console_driver.major = TTY_MAJOR;
	console_driver.minor_start = 0;
	console_driver.num = 8;
	console_driver.type = TTY_DRIVER_TYPE_CONSOLE;
	console_driver.subtype = SYSTEM_TYPE_CONSOLE;
	console_driver.init_termios = tty_std_termios;
	console_driver.flags = TTY_DRIVER_REAL_RAW | TTY_DRIVER_NO_DEVFS;
	console_driver.refcount = &console_refcount;
	console_driver.table = console_table;
	console_driver.termios = console_termios;
	console_driver.termios_locked = console_termios_locked;

	console_driver.open = con_open;
	console_driver.close = con_close;
	console_driver.write = con_write;
	console_driver.put_char = NULL;
	console_driver.flush_chars = NULL;
	console_driver.write_room = write_room;
	console_driver.chars_in_buffer = chars_in_buffer;
	console_driver.flush_buffer = NULL;
	console_driver.ioctl = NULL;
	console_driver.throttle = NULL;
	console_driver.unthrottle = NULL;
	console_driver.send_xchar = NULL;
	console_driver.set_termios = set_termios;
	console_driver.stop = NULL;
	console_driver.start = NULL;
	console_driver.hangup = NULL;
	console_driver.break_ctl = NULL;
	console_driver.wait_until_sent = NULL;
	console_driver.read_proc = NULL;
	if (tty_register_driver(&console_driver))
		panic("Couldn't register console driver\n");
	for(i=0;i<MAX_TTYS;i++){
		tty_register_devfs(&console_driver, 0, i);
	}
	open_console(0, NULL);
	return(0);
}

__initcall(stdio_init);

static void console_write(struct console *console, const char *string, 
			  unsigned len)
{
	cooked(CHAN_OUT_FD(vts[console->index].chan));
	write_chan(&vts[console->index].chan, string, len);
	raw(CHAN_OUT_FD(vts[console->index].chan), 0);
}

static kdev_t console_device(struct console *c)
{
	return MKDEV(TTY_MAJOR, c->index);
}

static int console_setup(struct console *co, char *options)
{
	return(0);
}

static struct console stdiocons = {
	"tty",
	console_write,
	NULL,
	console_device,
	NULL,
	NULL,
	console_setup,
	CON_PRINTBUFFER,
	-1,
	0,
	NULL
};

void stdio_console_init(void)
{
	save_console_flags();
	register_console(&stdiocons);
}

void stdio_announce(char *dev_name, int dev)
{
	printk("Virtual console %d assigned device '%s'\n", dev, dev_name);
}

static struct chan_opts opts = {
	announce: 	stdio_announce,
	dev:		-1,
	xterm_title:	"Virtual Console #%d",
	raw_pty:	1
};

static int console_chan_setup(char *str)
{
	int i, n;
	char *end;

	if(*str == '=') n = -1;
	else {
		n = simple_strtoul(str, &end, 0);
		if(*end != '='){
			printk("console_chan_setup failed to parse \"%s\"\n", 
			       str);
			return(1);
		}
		str = end;
	}
	str++;
	if(n == -1){
		for(i=0;i<sizeof(vts)/sizeof(vts[0]);i++){
			opts.dev = i;
			if(parse_chan_pair(str, i, &vts[i].chan, INIT_ALL,
					   &opts)) 
				return(1);
		}
	}
	else {
		opts.dev = n;
		parse_chan_pair(str, n, &vts[n].chan, INIT_ONE, &opts);
	}
	return(1);
}

__setup("con", console_chan_setup);

static void console_exit(void)
{
	int i;

	for(i=0;i<sizeof(vts)/sizeof(vts[0]);i++){
		close_chan_pair(&vts[i].chan);
	}
}

__exitcall(console_exit);

/*
 * Overrides for Emacs so that we follow Linus's tabbing style.
 * Emacs will notice this stuff at the end of the file and automatically
 * adjust the settings for this buffer only.  This must remain at the end
 * of the file.
 * ---------------------------------------------------------------------------
 * Local variables:
 * c-file-style: "linux"
 * End:
 */
