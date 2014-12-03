/* 
 * Copyright (C) 2000 Jeff Dike (jdike@karaya.com)
 * Licensed under the GPL
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <termios.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <netinet/in.h>
#include "ssl.h"
#include "chan.h"
#include "user.h"
#include "user_util.h"
#include "kern_util.h"

int tty_read(struct chan *chan, void *tty)
{
	int n;
	char ch;

	while((n = read(chan->fd, &ch, sizeof(ch))) == sizeof(ch))
		tty_receive_char(tty, ch);
	if(n < 0){
		if(errno == EIO){
			tty_eof(tty, &chan->hung_up);
			return(0);
		}  
		else if(errno != EAGAIN){
			 printk("tty_read read failed, errno = %d\n", errno);
			 return(-1);
		}
	}
	else if(n == 0) return(0);
	return(1);
}

int open_fd(struct chan *chan)
{
	raw(chan->fd, 0);
	return(0);
}

struct xterm_info {
	char tty[2];
	int fd;
	int console_num;
	int *pid_out;
	char *title;
};

static void xterm_tramp(void *arg)
{
	struct xterm_info *info;
	int pid;
	char title[256], flag[sizeof("Sxxnn\0")];

	info = arg;
	sprintf(flag, "-S%c%c%d", info->tty[0], info->tty[1], info->fd);
	sprintf(title, info->title, info->console_num);
	if((pid = fork()) != 0) *info->pid_out = pid;
	else {
		execlp("xterm", "xterm", flag, "-T", title, NULL);
		printk("execlp of xterm failed - errno = %d\n", errno);
		close(info->fd);
		exit(1);
	}
}

static int getmaster(char *line, int raw)
{
        struct termios tt;
	struct stat stb;
	char *pty, *bank, *cp;
	int master;

	pty = &line[strlen("/dev/ptyp")];
	for (bank = "pqrs"; *bank; bank++) {
		line[strlen("/dev/pty")] = *bank;
		*pty = '0';
		if (stat(line, &stb) < 0)
			break;
		for (cp = "0123456789abcdef"; *cp; cp++) {
			*pty = *cp;
			master = open(line, O_RDWR);
			if (master >= 0) {
				char *tp = &line[strlen("/dev/")];
				int ok;

				/* verify slave side is usable */
				*tp = 't';
				ok = access(line, R_OK|W_OK) == 0;
				*tp = 'p';
				if (ok) {
					if(raw){
						tcgetattr(master, &tt);
						cfmakeraw(&tt);
						tcsetattr(master, TCSADRAIN, 
							  &tt);
					}
					return(master);
				}
				(void) close(master);
			}
		}
	}
	return(-1);
}

int open_xterm(struct chan *chan)
{
	struct xterm_info info;
	int master, slave;
	char dev[] = "/dev/ptyXX", c;

	master = getmaster(dev, chan->data.xterm.raw);
	if(master == -1){
		printk("No unused host ptys found\n");
		return(-ENODEV);
	}
	dev[strlen("/dev/")] = 't';
	slave = open(dev, O_RDWR);
	if(slave == -1) return(-errno);
	info.tty[0] = dev[strlen("/dev/pty")];
	info.tty[1] = dev[strlen("/dev/ptyX")];
	info.fd = master;
	info.console_num = chan->data.xterm.line;
	info.pid_out = &chan->data.xterm.pid;
	info.title = chan->data.xterm.title;
	tracing_cb(xterm_tramp, &info);
	close(master);
	while((read(slave, &c, sizeof(c)) == sizeof(c)) && (c != '\n')) ;
	if(chan->data.xterm.raw) raw(slave, 1);
	chan->fd = slave;
	return(0);
}

int open_pty(struct chan *chan)
{
	char dev[sizeof("/dev/ptyxx\0")] = "/dev/ptyxx";

	chan->fd = getmaster(dev, chan->data.pty.raw);
	if(chan->fd < 0) return(-errno);
	if(chan->data.pty.announce) 
		(*chan->data.pty.announce)(dev, chan->data.pty.dev);
	return(0);
}

int open_tty(struct chan *chan)
{
	int fd;

	fd = open(chan->data.tty.dev, O_RDWR);
	if(fd < 0) return(-errno);
	chan->fd = fd;
	if(chan->data.tty.raw) raw(fd, 1);
	return(0);
}

int open_pts(struct chan *chan)
{
	int fd;

	if((fd = get_pty()) < 0){
		printk("open_pts : Failed to open pts\n");
		return(-errno);
	}
	chan->fd = fd;
	if(chan->data.tty.raw) raw(fd, 1);
	if(chan->data.pts.announce)
		(*chan->data.pts.announce)(ptsname(fd), chan->data.pts.dev);
	return(0);
}

int open_socket(struct chan *chan)
{
	struct sockaddr_in addr;
	int sock;

	sock = socket(PF_INET, SOCK_STREAM, 0);
	if(sock < 0) return(-errno);
	addr.sin_family = AF_INET;
	addr.sin_port = htons(chan->data.sock.port);
	addr.sin_addr.s_addr = htonl(INADDR_ANY);
	if(bind(sock, (struct sockaddr *) &addr, sizeof(addr)) < 0)
		return(-errno);
	if(listen(sock, 1) < 0)	return(-errno);
	chan->data.sock.connected = 0;
	chan->fd = sock;
	return(0);
}

struct telnetd_args {
	int fd;
	char helper[sizeof("/tmp/uml_dev/nnnn\0")];
};

static void telnetd_tramp(void *arg)
{
	struct telnetd_args info = *((struct telnetd_args *) arg);

	if(fork() == 0){
		dup2(info.fd, 0);
		dup2(info.fd, 1);
		dup2(info.fd, 2);
		close(info.fd);
		execlp("in.telnetd", "in.telnetd", "-L", info.helper, NULL);
		printk("execlp of telnetd failed - errno = %d\n", errno);
		exit(1);
	}
}

void accept_connection(struct chan *chan)
{
	struct telnetd_args info;
	char *name;

	if((info.fd = accept(chan->fd, NULL, 0)) < 0){
		printk("tty_read: accept failed - errno = %d\n", 
		       errno);
		return;
	}
	mkdir("/tmp/uml_dev", 0777);
	name = ptsname(chan->data.sock.pty);
	name = rindex(name, '/');
	sprintf(info.helper, "/tmp/uml_dev%s", name);
	if(symlink("/home/jdike/bin/telnet_helper", info.helper)){
		printk("symlink from '%s' failed, errno = %d\n", info.helper,
		       errno);
		return;
	}
	tracing_cb(telnetd_tramp, &info);
	chan->data.sock.connected = 1;
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
