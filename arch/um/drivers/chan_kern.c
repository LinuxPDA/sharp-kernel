/* 
 * Copyright (C) 2000 Jeff Dike (jdike@karaya.com)
 * Licensed under the GPL
 */

#include <linux/stddef.h>
#include <linux/tty.h>
#include <linux/tty_flip.h>
#include <asm/irq.h>
#include "chan.h"
#include "user_util.h"
#include "kern.h"

void tty_interrupt(struct io_chan *chan, void *tty_ptr, int count)
{
	struct tty_struct *tty = tty_ptr;
	int n;

	if(count == 0) return;
	n = tty_read(&chan->in, tty);
	if(tty) tty_flip_buffer_push(tty);
	if(n > 0){
		chan->in.hung_up = 0;
		reactivate_fd(chan->in.fd);
	}
}

void tty_eof(void *tty_ptr, int *hung_up)
{
	struct tty_struct *tty = tty_ptr;

	if(tty == NULL) return;
	if(!*hung_up) tty_hangup(tty);
	*hung_up = 1;
}

void tty_receive_char(void *tty_ptr, char ch)
{
	struct tty_struct *tty = tty_ptr;

	if(tty == NULL) return;

	if (I_IXON(tty) && !I_IXOFF(tty) && !tty->raw) {
		if (ch == STOP_CHAR(tty))
			stop_tty(tty);
		else if (ch == START_CHAR(tty))
			start_tty(tty);
	}

	if((tty->flip.flag_buf_ptr == NULL) || 
	   (tty->flip.char_buf_ptr == NULL))
		return;
	tty_insert_flip_char(tty, ch, TTY_NORMAL);
}

static void accept_interrupt(int irq, void *dev, struct pt_regs *unused)
{
	accept_connection(dev);
}

static int setup_accept_irq(int fd, void *data)
{
	return(um_request_irq(ACCEPT_IRQ, fd, accept_interrupt, 
			      SA_INTERRUPT | SA_SHIRQ, "socket", data));
}

static int open_chan(struct chan *chan, int (*irq_setup)(int fd, void *data),
		     int input, void *data)
{
	struct io_chan subchan;
	int err;

	if(chan->opened) return(0);
	switch(chan->type){
	case XTERM:
		err = open_xterm(chan);
		break;
	case PTY:
		err = open_pty(chan);
		break;
	case TTY:
		err = open_tty(chan);
		break;
	case PTS:
		err = open_pts(chan);
		break;
	case FD:
		err = open_fd(chan);
		break;
	case SOCKET:
		subchan = ((struct io_chan) 
			   PTS_IO_CHAN_INIT(NULL, -1, 1, INIT_STATIC));
		err = open_chan_pair(&subchan, irq_setup, data);
		if(err) return(err);
		chan->data.sock.pty = subchan.in.fd;
		err = open_socket(chan);
		irq_setup = setup_accept_irq;
		data = chan;
		break;
	default:
		printk("open_chan : Unknown channel type - %d\n", chan->type);
		err = -ENODEV;
		break;
	}
	if(err) return(err);
	if(input && irq_setup) err = (*irq_setup)(chan->fd, data);
	if(err) return(err);
	chan->opened = 1;
	return(0);
}

int open_chan_pair(struct io_chan *chan, int (*irq_setup)(int fd, void *data),
		   void *data)
{
	int err;

	err = open_chan(&chan->in, irq_setup, 1, data);
	if(err) return(err);
	if(chan->out.type != COPY) 
		err = open_chan(&chan->out, irq_setup, 0, data);
	return(err);
}

static void close_chan(struct chan *chan)
{
	switch(chan->type){
	case XTERM:
		if(chan->data.xterm.pid != -1)
			kill(chan->data.xterm.pid, SIGKILL);
		break;
	default:
		break;
	}
}

void close_chan_pair(struct io_chan *chan)
{
	close_chan(&chan->in);
	if(chan->out.type != COPY)
		close_chan(&chan->out);
}

int write_chan(struct io_chan *chan, const char *buf, int len)
{
	struct chan *c;
	int n;

	if(chan->out.type == COPY) c = &chan->in;
	else c = &chan->out;
	if(c->fd == -1) return(-ENODEV);
	n = write(c->fd, buf, len);
	if(errno == EAGAIN) n = 0;
	if(n < 0) return(-errno);
	return(n);
}

static int parse_chan(char *str, int device, struct chan *chan, int pri, 
		      struct chan_opts *opts)
{
	if(pri <= chan->init_pri) return(0);
	if(!strncmp(str, "pty", strlen("pty"))){
		*chan = ((struct chan) PTY_CHAN_INIT(opts->announce, 
						     opts->dev, opts->raw_pty,
						     pri));
	}
	else if(!strncmp(str, "tty", strlen("tty"))){
		char *tty_name;

		tty_name = &str[strlen("tty")];
		if(*tty_name != ':'){
			printk("parse_chan : channel type 'tty' must "
			       "specify a device\n");
			return(-1);
		}
		tty_name++;
		*chan = ((struct chan) TTY_CHAN_INIT(tty_name, opts->raw_pty,
						     pri));
	}
	else if(!strncmp(str, "pts", strlen("pts"))){
		*chan = ((struct chan) PTS_CHAN_INIT(opts->announce, 
						     opts->dev, opts->raw_pty,
						     pri));
	}	
	else if(!strncmp(str, "xterm", strlen("xterm"))){
		*chan = ((struct chan) 
			 XTERM_CHAN_INIT(device, opts->xterm_title,
					 opts->raw_pty, pri));
	}
	else if(!strncmp(str, "socket", strlen("socket"))){
		char *sock;
		int n;

		sock = &str[strlen("socket")];
		if(*sock != ':'){
			printk("parse_chan : channel type 'socket' must "
			       "specify a socket\n");
			return(-1);
		}
		sock++;
		n = simple_strtoul(sock, NULL, 0);
		if(n == 0){
			printk("parse_chan : invalid socket number - '%s'\n",
			       sock);
			return(-1);
		}
		*chan = ((struct chan) SOCK_CHAN_INIT(n, pri));
	}
	else if(!strncmp(str, "fd", strlen("fd"))){
		char *fd, *end;
		int n;

		fd = &str[strlen("fd")];
		if(*fd != ':'){
			printk("parse_chan : channel type 'fd' must "
			       "specify a file descriptor\n");
			return(-1);
		}
		fd++;
		n = simple_strtoul(fd, &end, 0);
		if(*end != '\0'){
			printk("parse_chan : couldn't parse file descriptor "
			       "'%s'", fd);
			return(-1);
		}
		*chan = ((struct chan) FD_CHAN_INIT(n, pri));
		return(0);
	}
	else {
		printk("parse_chan couldn't parse \"%s\"\n", str);
		return(-1);
	}
	chan->init_pri = pri;
	return(0);
}

int parse_chan_pair(char *str, int device, struct io_chan *chan, int pri, 
		    struct chan_opts *opts)
{
	char *in, *out;
	int err;

	if((out = strchr(str, ',')) != NULL){
		in = str;
		*out = '\0';
		out++;
		err = parse_chan(in, device, &chan->in, pri, opts);
		if(err) return(err);
		err = parse_chan(out, device, &chan->out, pri, opts);
		return(err);
	}
	else {
		err = parse_chan(str, device, &chan->in, pri, opts);
		if(err) return(err);
		chan->out.type = COPY;
		return(0);
	}
}


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
