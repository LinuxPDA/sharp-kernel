/*
 * Copyright (C) 2001 Lennert Buytenhek (buytenh@gnu.org) and 
 * James Leu (jleu@mindspring.net).
 * Copyright (C) 2001 by various other people who didn't put their name here.
 * Licensed under the GPL.
 */

#include <stdio.h>
#include <unistd.h>
#include <stddef.h>
#include <fcntl.h>
#include <stdlib.h>
#include <sys/errno.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <sys/un.h>
#include <net/if.h>
#include "user.h"
#include "kern_util.h"
#include "net_user.h"
#include "etap.h"

#define MAX_PACKET ETH_MAX_PACKET

void etap_user_init(void *data, void *dev)
{
	struct ethertap_data *pri = data;

	pri->dev = dev;
}

struct etap_open_data {
	char *name;
	char *gate;
	int data_remote;
	int data_me;
	int control_remote;
	int control_me;
	int err;
};

struct addr_change {
	enum { ADD_ADDR, DEL_ADDR } what;
	unsigned char addr[4];
};

static void etap_open_addr(unsigned char *addr, void *arg)
{
	int fd = *((int *) arg);
	struct addr_change change;

	change.what = ADD_ADDR;
	memcpy(change.addr, addr, sizeof(change.addr));
	if(write(fd, &change, sizeof(change)) != sizeof(change))
		printk("etap_add_addr - request failed, errno = %d\n",
		       errno);
}

static void etap_close_addr(unsigned char *addr, void *arg)
{
	int fd = *((int *) arg);
	struct addr_change change;

	change.what = DEL_ADDR;
	memcpy(change.addr, addr, sizeof(change.addr));
	if(write(fd, &change, sizeof(change)) != sizeof(change))
		printk("etap_close_addr - request failed, errno = %d\n",
		       errno);
}

static void etap_tramp(void *arg)
{
	struct etap_open_data *data = arg;
	int pid, status, n;
	char version_buf[sizeof("nnnnn\0")];
	char data_fd_buf[sizeof("nnnnnn\0")];
	char control_fd_buf[sizeof("nnnnnn\0")];
	char gate_buf[sizeof("nnn.nnn.nnn.nnn\0")], c;
	char *setup_args[] = { "uml_net", version_buf, "ethertap", data->name,
			       data_fd_buf, control_fd_buf, gate_buf, NULL };
	char *nosetup_args[] = { "uml_net", version_buf, "ethertap", 
				 data->name, data_fd_buf, control_fd_buf, 
				 NULL };
	char **args;

	sprintf(data_fd_buf, "%d", data->data_remote);
	sprintf(control_fd_buf, "%d", data->control_remote);
	sprintf(version_buf, "%d", UML_NET_VERSION);
	if(data->gate != NULL){
		strcpy(gate_buf, data->gate);
		args = setup_args;
	}
	else args = nosetup_args;
	data->err = 0;
	if((pid = fork()) == 0){
		char zero = 0; /* XXX Not necessary */

		close(data->data_me);
		close(data->control_me);
		execvp(args[0], args);
		printk("Exec of '%s' failed - errno = %d\n", args[0], errno);
		write(data->control_remote, &zero, sizeof(zero));
		exit(errno);
	}
	else if(pid < 0) data->err = errno;
	close(data->data_remote);
	close(data->control_remote);
	n = read(data->control_me, &c, sizeof(c));
	if(n != sizeof(c)){
		printk("etap_open - failed to read response from helper : "
		       "return = %d, errno = %d\n", n, errno);
		if(waitpid(pid, &status, 0) < 0) data->err = errno;
		else if(!WIFEXITED(status) || (WEXITSTATUS(status) != 1)){
			printk("uml_net didn't exit with status 1\n");
			data->err = EINVAL;
		}
		else data->err = EINVAL;
	}
	else if(c != 1) data->err = EINVAL;
}

static int etap_open(void *data)
{
	struct ethertap_data *pri = data;
	struct etap_open_data tap_data;
	int data_fds[2], control_fds[2], err;

	err = tap_open_common(pri->dev, pri->hw_setup, pri->gate_addr);
	if(err) return(err);

	tap_data.name = pri->dev_name;

	if(socketpair(PF_UNIX, SOCK_DGRAM, 0, data_fds) < 0){
		printk("data socketpair failed - errno = %d\n", errno);
		return(-errno);
	}
	tap_data.data_remote = data_fds[1];
	tap_data.data_me = data_fds[0];

	if(socketpair(PF_UNIX, SOCK_STREAM, 0, control_fds) < 0){
		printk("data socketpair failed - errno = %d\n", errno);
		return(-errno);
	}
	tap_data.control_remote = control_fds[1];
	tap_data.control_me = control_fds[0];
	
	tap_data.gate = pri->gate_addr;
	tracing_cb(etap_tramp, &tap_data);
	if(tap_data.err != 0){
		printk("etap_tramp failed - errno = %d\n", tap_data.err);
		return(-tap_data.err);
	}
	pri->data_fd = data_fds[0];
	pri->control_fd = control_fds[0];
	iter_addresses(pri->dev, etap_open_addr, &pri->control_fd);
	return(data_fds[0]);
}

static void etap_close(int fd, void *data)
{
	struct ethertap_data *pri = data;

	iter_addresses(pri->dev, etap_close_addr, &pri->control_fd);
	close(fd);
	shutdown(pri->data_fd, SHUT_RDWR);
	close(pri->data_fd);
	close(pri->control_fd);
}

int etap_user_read(int fd, void *buf, int len, struct ethertap_data *pri)
{
	int n;

	while(((n = recvfrom(fd,  buf,  len, 0, NULL, NULL)) < 0) && 
	      (errno == EINTR)) ;
	if(errno == EAGAIN) return(0);
	if(n < 0) return(-errno);
	return(n);
}

int etap_user_write(int fd, void *buf, int len, struct ethertap_data *pri)
{
	int n;

	while(((n = send(fd, buf, len, 0)) < 0) && (errno == EINTR)) ;
	if(errno == EAGAIN) n = 0;
	if(n < 0) return(-errno);
	return(n);
}

static int etap_set_mtu(int mtu, void *data)
{
	return(mtu);
}

static void etap_add_addr(unsigned char *addr, void *data)
{
	struct ethertap_data *pri = data;

	if(pri->control_fd == -1) return;
	etap_open_addr(addr, &pri->control_fd);
}

static void etap_del_addr(unsigned char *addr, void *data)
{
	struct ethertap_data *pri = data;

	if(pri->control_fd == -1) return;
	etap_close_addr(addr, &pri->control_fd);
}

struct net_user_info ethertap_user_info = {
	init:		etap_user_init,
	open:		etap_open,
	close:	 	etap_close,
	set_mtu:	etap_set_mtu,
	add_address:	etap_add_addr,
	delete_address: etap_del_addr,
        max_packet:     MAX_PACKET - ETH_HEADER_ETHERTAP
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
