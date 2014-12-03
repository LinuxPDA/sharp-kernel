/* 
 * Copyright (C) 2001 Jeff Dike (jdike@karaya.com)
 * Licensed under the GPL
 */

#include <stdio.h>
#include <stddef.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/uio.h>
#include <sys/wait.h>
#include <net/if.h>
#include "net_user.h"
#include "tuntap.h"
#include "user.h"
#include "kern_util.h"

#define MAX_PACKET ETH_MAX_PACKET

void tuntap_user_init(void *data, void *dev)
{
	struct tuntap_data *pri = data;

	pri->dev = dev;
}

struct tuntap_open_data {
	char *name;
	char *gate;
	int data_fd;
	int remote;
	int me;
	int err;
	char *buffer;
	int len;
	int used;
};

static void tuntap_open_tramp(void *arg)
{
	struct tuntap_open_data *data = arg;
	char version_buf[sizeof("nnnnn\0")];
	char fd_buf[sizeof("nnnnnn\0")];
	char *args[] = { "uml_net", version_buf, "tuntap", "up", "", fd_buf,
			 data->gate, NULL };
	char buf[CMSG_SPACE(sizeof(data->data_fd))];
	struct msghdr msg;
	struct cmsghdr *cmsg;
	struct iovec iov;
	int pid, n;

	sprintf(version_buf, "%d", UML_NET_VERSION);
	sprintf(fd_buf, "%d", data->remote);
	data->err = 0;
	if((pid = fork()) == 0){
		close(data->me);
		execvp(args[0], args);
		printk("Exec of '%s' failed - errno = %d\n", args[0], errno);
		exit(1);		
	}	
	else if(pid < 0) data->err = errno;
	close(data->remote);

	msg.msg_name = NULL;
	msg.msg_namelen = 0;
	if(data->buffer != NULL){
		iov = ((struct iovec) { data->buffer, data->len });
		msg.msg_iov = &iov;
		msg.msg_iovlen = 1;
	}
	else {
		msg.msg_iov = NULL;
		msg.msg_iovlen = 0;
	}
	msg.msg_control = buf;
	msg.msg_controllen = sizeof(buf);
	msg.msg_flags = 0;
	n = recvmsg(data->me, &msg, 0);
	data->used = n;
	if(n < 0){
		printk("tuntap_open_tramp : recvmsg failed - errno = %d\n", 
		       errno);
		data->err = errno;
		return;
	}
	waitpid(pid, NULL, 0);

	cmsg = CMSG_FIRSTHDR(&msg);
	if(cmsg == NULL){
		printk("tuntap_open_tramp : didn't receive a message\n");
		data->err = EINVAL;
		return;
	}
	if((cmsg->cmsg_level != SOL_SOCKET) || 
	   (cmsg->cmsg_type != SCM_RIGHTS)){
		printk("tuntap_open_tramp : didn't receive a descriptor\n");
		data->err = EINVAL;
		return;
	}
	data->data_fd = ((int *) CMSG_DATA(cmsg))[0];
}

struct tuntap_change_data {
	char *dev;
	char *what;
	char *address;
};

static void tuntap_change_tramp(void *arg)
{
	int pid;
	struct tuntap_change_data *data = arg;
	char version[sizeof("nnnnn\0")];
	char *argv[] = { "uml_net", version, "tuntap", data->what, data->dev, 
			 data->address, NULL };

	sprintf(version, "%d", UML_NET_VERSION);
	if((pid = fork()) == 0){
		execvp(argv[0], argv);
		printk("Exec of '%s' failed - errno = %d\n", argv[0], errno);
		exit(1);		
	}	
	waitpid(pid, NULL, 0);
}

static void tuntap_change(char *dev, char *what, unsigned char *addr)
{
	char addr_buf[sizeof("255.255.255.255\0")];
	struct tuntap_change_data data;

	data.dev = dev;
	data.what = what;
	sprintf(addr_buf, "%d.%d.%d.%d", addr[0], addr[1], addr[2], addr[3]);
	data.address = addr_buf;
	tracing_cb(tuntap_change_tramp, &data);	
}

static void tuntap_open_addr(unsigned char *addr, void *arg)
{
	char *dev = arg;

	tuntap_change(dev, "add", addr);
}

static void tuntap_close_addr(unsigned char *addr, void *arg)
{
	char *dev = arg;

	tuntap_change(dev, "del", addr);
}

static void tuntap_add_addr(unsigned char *addr, void *data)
{
	struct tuntap_data *pri = data;

	if(pri->fd == -1) return;
	tuntap_open_addr(addr, pri->dev_name);
}

static void tuntap_del_addr(unsigned char *addr, void *data)
{
	struct tuntap_data *pri = data;

	if(pri->fd == -1) return;
	tuntap_close_addr(addr, pri->dev_name);
}

static int tuntap_open(void *data)
{
	struct tuntap_data *pri = data;
	struct tuntap_open_data tap_data;
	int err, fds[2];

	err = tap_open_common(pri->dev, pri->hw_setup, pri->gate_addr);
	if(err) return(err);

	if(socketpair(PF_UNIX, SOCK_DGRAM, 0, fds) < 0){
		printk("data socketpair failed - errno = %d\n", errno);
		return(-errno);
	}

	tap_data.me = fds[0];
	tap_data.remote = fds[1];
	tap_data.data_fd = -1;
	tap_data.gate = pri->gate_addr;
	tap_data.buffer = get_output_buffer(&tap_data.len);
	if(tap_data.buffer != NULL) tap_data.len--;
	tap_data.used = 0;

	tracing_cb(tuntap_open_tramp, &tap_data);
	if(tap_data.buffer != NULL){
		pri->dev_name = uml_strdup(tap_data.buffer);
		tap_data.buffer[tap_data.used] = '\0';
		printk(tap_data.buffer + IFNAMSIZ);
		free_output_buffer(tap_data.buffer);
	}
	if(tap_data.err != 0){
		printk("tuntap_open_tramp failed - errno = %d\n", 
		       tap_data.err);
		return(-tap_data.err);
	}
	close(fds[0]);

	pri->fd = tap_data.data_fd;
	iter_addresses(pri->dev, tuntap_open_addr, pri->dev_name);
	return(tap_data.data_fd);	
}

static void tuntap_close(int fd, void *data)
{
	struct tuntap_data *pri = data;

	iter_addresses(pri->dev, tuntap_close_addr, pri->dev_name);
	close(fd);
}

int tuntap_user_read(int fd, void *buf, int len, struct tuntap_data *pri)
{
	int n;

	while(((n = read(fd,  buf,  len)) < 0) && (errno == EINTR)) ;
	if(errno == EAGAIN) return(0);
	if(n < 0) return(-errno);
	return(n);
}

int tuntap_user_write(int fd, void *buf, int len, struct tuntap_data *pri)
{
	int n;

	while(((n = write(fd, buf, len)) < 0) && (errno == EINTR)) ;
	if(errno == EAGAIN) n = 0;
	if(n < 0) return(-errno);
	return(n);
}

static int tuntap_set_mtu(int mtu, void *data)
{
	return(mtu);
}

struct net_user_info tuntap_user_info = {
	init:		tuntap_user_init,
	open:		tuntap_open,
	close:	 	tuntap_close,
	set_mtu:	tuntap_set_mtu,
	add_address:	tuntap_add_addr,
	delete_address: tuntap_del_addr,
	max_packet:	MAX_PACKET
};

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
