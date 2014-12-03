/* 
 * Copyright (C) 2000 Jeff Dike (jdike@karaya.com)
 * Licensed under the GPL
 */

#ifndef __TTY_H__
#define __TTY_H__

struct chan {
	enum { XTERM, PTY, SOCKET, FD, TTY, PTS, FILE_CHAN, COPY } type;
	int fd;
	int hung_up;
	int opened;
	int init_pri;
	union {
		struct {
			int pid;
			int line;
			char *title;
			int raw;
		} xterm;
		struct {
			void (*announce)(char *dev_name, int dev);
			int dev;
			int raw;
		} pty, pts;
		struct {
			char *dev;
			int raw;
			void *tty_state;
		} tty;
		struct {
			int port;
			int connected;
			int pty;
		} sock;
	} data;
};

struct io_chan {
	struct chan in;
	struct chan out;
};

#define INIT_STATIC (0)
#define INIT_ALL (1)
#define INIT_ONE (2)

#define PTY_CHAN_INIT(announce, dev, raw, pri) \
        { PTY, -1, 1, 0, pri, { pty: { announce, dev, raw } } }

#define PTS_CHAN_INIT(announce, dev, raw, pri) \
        { PTS, -1, 0, 0, pri, { pts: { announce, dev, raw } } }

#define XTERM_CHAN_INIT(n, title, raw, pri) \
        { XTERM, -1, 0, 0, pri, { xterm: { -1, n, title, raw } } }

#define TTY_CHAN_INIT(dev, raw, pri) \
        { TTY, -1, 0, 0, pri, { tty: { dev, raw } } }

#define SOCK_CHAN_INIT(port, pri) \
        { SOCKET, -1, 0, 0, pri, { sock: { port, 0, -1 } } }

#define FD_CHAN_INIT(fd, pri) { FD, fd, 0, 0, pri }

#define COPY_CHAN_INIT() { COPY }

#define PTY_IO_CHAN_INIT(announce, data, raw, pri) \
	{ PTY_CHAN_INIT(announce, data, raw, pri), COPY_CHAN_INIT() }

#define PTS_IO_CHAN_INIT(announce, data, raw, pri) \
	{ PTS_CHAN_INIT(announce, data, raw, pri), COPY_CHAN_INIT() }

#define XTERM_IO_CHAN_INIT(n, title, raw, pri) \
        { XTERM_CHAN_INIT(n, title, raw, pri), COPY_CHAN_INIT() }

#define STDIO_IO_CHAN_INIT(pri) \
        { { FD, 0, 0, 0, pri, { } }, { FD, 1, 0, 0, pri, { } } }

#define TTY_IO_CHAN_INIT(dev, raw, pri) \
	{ TTY_CHAN_INIT(dev, raw, pri), COPY_CHAN_INIT() }

#define SOCK_IO_CHAN_INIT(port, pri) \
	{ SOCK_CHAN_INIT(port, pri), COPY_CHAN_INIT() }

#define FD_IO_CHAN_INIT(in, out, pri) \
        { FD_CHAN_INIT(in, pri), FD_CHAN_INIT(out, pri) }

#define CHAN_IN_FD(chan) ((chan).in.fd)
#define CHAN_OUT_FD(chan) (((chan).out.type == COPY) ? ((chan).in.fd) : \
			                               ((chan).out.fd))

struct chan_opts {
	void (*announce)(char *dev_name, int dev);
	int dev;
	char *xterm_title;
	int raw_pty;
};

extern int tty_read(struct chan *chan, void *tty);
extern void tty_eof(void *tty_ptr, int *hung_up);
extern void tty_interrupt(struct io_chan *chan, void *tty_ptr, int count);
extern void tty_receive_char(void *tty_ptr, char ch);
extern int open_chan_pair(struct io_chan *chan, 
			  int (*irq_setup)(int fd, void *data), void *data);
extern int open_fd(struct chan *chan);
extern int open_xterm(struct chan *chan);
extern int open_pty(struct chan *chan);
extern int open_tty(struct chan *chan);
extern int open_pts(struct chan *chan);
extern int open_socket(struct chan *chan);
extern int write_chan(struct io_chan *chan, const char *buf, int len);
extern int parse_chan_pair(char *str, int device, struct io_chan *chan, 
			   int pri, struct chan_opts *opts);
extern void accept_connection(struct chan *chan);
extern void close_chan_pair(struct io_chan *chan);

#endif

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
