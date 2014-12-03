/* 
 * Copyright (C) 2000 Jeff Dike (jdike@karaya.com)
 * Licensed under the GPL
 */

#include "linux/fs.h"
#include "linux/tty.h"
#include "linux/tty_driver.h"
#include "linux/major.h"
#include "linux/mm.h"
#include "linux/init.h"
#include "asm/termbits.h"
#include "asm/irq.h"
#include "ssl.h"
#include "chan.h"
#include "user_util.h"
#include "kern_util.h"
#include "kern.h"

static int ssl_version = 1;

static struct tty_driver ssl_driver;

static int ssl_refcount = 0;

#define NR_PORTS 64

static struct tty_struct *ssl_table[NR_PORTS];
static struct termios *ssl_termios[NR_PORTS];
static struct termios *ssl_termios_locked[NR_PORTS];

static struct ssl {
	struct io_chan chan;
	int count;
	struct tty_struct *tty;
} private[NR_PORTS] = { [0 ... NR_PORTS - 1] = 
			{ PTY_IO_CHAN_INIT(NULL, 0, 1, INIT_STATIC),
			  0, NULL } };

static void ssl_interrupt(int irq, void *dev, struct pt_regs *unused)
{
	struct ssl *line = dev;

	tty_interrupt(&line->chan, line->tty, line->count);
}

static int setup_ssl_irq(int fd, void *data)
{
	return(um_request_irq(SSL_IRQ, fd, ssl_interrupt, 
			      SA_INTERRUPT | SA_SHIRQ, "ssl", data));
}

void ssl_announce(char *dev_name, int dev)
{
	printk("Serial line %d assigned device '%s'\n", dev, dev_name);
}

DECLARE_MUTEX(ssl_sem);

int ssl_open(struct tty_struct *tty, struct file *filp)
{
	int line, err;

	line = MINOR(tty->device) - tty->driver.minor_start;
	if ((line < 0) || (line >= NR_PORTS))
		return -ENODEV;
	down(&ssl_sem);
	if(tty == NULL) panic("NULL tty in ssl_open");
	private[line].tty = tty;
	tty->driver_data = &private[line];
	err = open_chan_pair(&private[line].chan, setup_ssl_irq, 
			     &private[line]);
	up(&ssl_sem);	
	if(err){
		printk("Couldn't open serial line %d - errno = %d\n", line,
		       -err);
		up(&ssl_sem);	
		return(err);
	}
	private[line].count++;
	up(&ssl_sem);	
	return(0);
}

static void ssl_close(struct tty_struct *tty, struct file * filp)
{
	int line;

	line = MINOR(tty->device) - tty->driver.minor_start;
	private[line].count--;
	if(private[line].count == 0) private[line].tty = NULL;
}

static int ssl_write(struct tty_struct * tty, int from_user,
		     const unsigned char *buf, int count)
{
	int line;

	line = MINOR(tty->device) - tty->driver.minor_start;
	if ((line < 0) || (line >= NR_PORTS))
		panic("Bad tty in ssl_put_char");
	return(write_chan(&private[line].chan, buf, count));
}

static void ssl_put_char(struct tty_struct *tty, unsigned char ch)
{
	int line;

	line = MINOR(tty->device) - tty->driver.minor_start;
	if ((line < 0) || (line >= NR_PORTS))
		panic("Bad tty in ssl_put_char");
	write_chan(&private[line].chan, &ch, sizeof(ch));
}

static void ssl_flush_chars(struct tty_struct *tty)
{
	return;
}

static int ssl_write_room(struct tty_struct *tty)
{
	return(16384);
}

static int ssl_chars_in_buffer(struct tty_struct *tty)
{
	return(0);
}

static void ssl_flush_buffer(struct tty_struct *tty)
{
	return;
}

static int ssl_ioctl(struct tty_struct *tty, struct file * file,
		     unsigned int cmd, unsigned long arg)
{
	int ret;

	ret = 0;
	switch(cmd){
	case TCGETS:
	case TCSETS:
	case TCFLSH:
	case TCSETSF:
	case TCSETSW:
	case TCGETA:
		ret = -ENOIOCTLCMD;
		break;
	default:
		printk("Unimplemented ioctl in ssl_ioctl : 0x%x\n", cmd);
		ret = -ENOIOCTLCMD;
		break;
	}
	return(ret);
}

static void ssl_throttle(struct tty_struct * tty)
{
	printk("Someone should implement ssl_throttle\n");
}

static void ssl_unthrottle(struct tty_struct * tty)
{
	printk("Someone should implement ssl_unthrottle\n");
}

static void ssl_set_termios(struct tty_struct *tty, 
			    struct termios *old_termios)
{
}

static void ssl_stop(struct tty_struct *tty)
{
	printk("Someone should implement ssl_stop\n");
}

static void ssl_start(struct tty_struct *tty)
{
	printk("Someone should implement ssl_start\n");
}

void ssl_hangup(struct tty_struct *tty)
{
}

extern void tty_register_devfs  (struct tty_driver *driver, unsigned int flags,
				 unsigned int minor);
extern void tty_unregister_devfs (struct tty_driver *driver, unsigned minor);

int ssl_init(void)
{
	int i;

	for(i=0;i<sizeof(private)/sizeof(private[0]);i++){
		if(private[i].chan.in.init_pri == INIT_STATIC)
			private[i].chan = ((struct io_chan) 
					   PTY_IO_CHAN_INIT(ssl_announce, 
							    i, 1,
							    INIT_STATIC));
	}

	printk(KERN_INFO "Initializing software serial port version %d\n", 
	       ssl_version);
  
	/* Initialize the tty_driver structure */
	
	memset(&ssl_driver, 0, sizeof(struct tty_driver));
	ssl_driver.magic = TTY_DRIVER_MAGIC;
	ssl_driver.name = "serial/%d";
	ssl_driver.major = TTYAUX_MAJOR;
	ssl_driver.minor_start = 64;
	ssl_driver.num = NR_PORTS;
	ssl_driver.type = TTY_DRIVER_TYPE_SERIAL;
	ssl_driver.subtype = 0;
	ssl_driver.init_termios = tty_std_termios;
	ssl_driver.init_termios.c_cflag =
		B9600 | CS8 | CREAD | HUPCL | CLOCAL;
	ssl_driver.flags = TTY_DRIVER_REAL_RAW;
	ssl_driver.refcount = &ssl_refcount;
	ssl_driver.table = ssl_table;
	ssl_driver.termios = ssl_termios;
	ssl_driver.termios_locked = ssl_termios_locked;

	ssl_driver.open = ssl_open;
	ssl_driver.close = ssl_close;
	ssl_driver.write = ssl_write;
	ssl_driver.put_char = ssl_put_char;
	ssl_driver.flush_chars = ssl_flush_chars;
	ssl_driver.write_room = ssl_write_room;
	ssl_driver.chars_in_buffer = ssl_chars_in_buffer;
	ssl_driver.flush_buffer = ssl_flush_buffer;
	ssl_driver.ioctl = ssl_ioctl;
	ssl_driver.throttle = ssl_throttle;
	ssl_driver.unthrottle = ssl_unthrottle;
	ssl_driver.set_termios = ssl_set_termios;
	ssl_driver.stop = ssl_stop;
	ssl_driver.start = ssl_start;
	ssl_driver.hangup = ssl_hangup;
	if (tty_register_driver(&ssl_driver))
		panic("Couldn't register ssl driver\n");
	return(0);
}

__initcall(ssl_init);

static struct chan_opts opts = {
	announce: 	ssl_announce,
	dev:		-1,
	xterm_title:	"Serial Line #%d",
	raw_pty:	1
};

static int ssl_chan_setup(char *str)
{
	int i, n;
	char *end;

	if(*str == '=') n = -1;
	else {
		n = simple_strtoul(str, &end, 0);
		if(*end != '='){
			printk("ssl_chan_setup failed to parse \"%s\"\n", 
			       str);
			return(1);
		}
		str = end;
	}
	str++;
	if(n == -1){
		for(i=0;i<sizeof(private)/sizeof(private[0]);i++){
			opts.dev = i;
			if(parse_chan_pair(str, i, &private[i].chan, INIT_ALL,
				      &opts)) 
				return(1);
		}
	}
	else {
		opts.dev = n;
		parse_chan_pair(str, n, &private[n].chan, INIT_ONE, &opts);
	}
	return(1);
}

__setup("ssl", ssl_chan_setup);

static void ssl_exit(void)
{
	int i;

	for(i=0;i<sizeof(private)/sizeof(private[0]);i++){
		close_chan_pair(&private[i].chan);
	}
}

__exitcall(ssl_exit);

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
