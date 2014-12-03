/*
 * Copyright (C) 2001 Lennert Buytenhek (buytenh@gnu.org) and 
 * James Leu (jleu@mindspring.net).
 * Copyright (C) 2001 by various other people who didn't put their name here.
 * Licensed under the GPL.
 */

#include <errno.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/time.h>
#include "net_user.h"
#include "daemon.h"
#include "kern_util.h"
#include "user_util.h"
#include "user.h"

#define MAX_PACKET (ETH_MAX_PACKET + ETH_HEADER_OTHER)

enum request_type { REQ_NEW_CONTROL };

struct request {
  enum request_type type;
  union {
    struct {
	    unsigned char addr[ETH_ADDR_LEN];
	    struct sockaddr_un name;
    } new_control;
    struct {
	    unsigned long cookie;
    } new_data;
  } u;
};

static struct sockaddr_un *new_addr(void *name, int len)
{
	struct sockaddr_un *sun;

	sun = um_kmalloc(sizeof(struct sockaddr_un));
	if(sun == NULL){
		printk("new_addr: allocation of sockaddr_un failed\n");
		return(NULL);
	}
	sun->sun_family = AF_UNIX;
	memcpy(sun->sun_path, name, len);
	return(sun);
}

static void daemon_user_init(void *data, void *dev)
{
	struct daemon_data *pri = data;
	struct timeval tv;
	struct {
		char zero;
		int pid;
		int usecs;
	} name;

	if(!strcmp(pri->sock_type, "unix")){
		pri->ctl_addr = new_addr(pri->ctl_sock, 
					 strlen(pri->ctl_sock) + 1);
		pri->data_addr = new_addr(pri->data_sock, 
					  strlen(pri->data_sock) + 1);
	}
	name.zero = 0;
	name.pid = getpid();
	gettimeofday(&tv, NULL);
	name.usecs = tv.tv_usec;
	pri->local_addr = new_addr(&name, sizeof(name));
	pri->dev = dev;
}

static int daemon_open(void *data)
{
	struct daemon_data *pri = data;
	struct sockaddr_un *ctl_addr = pri->ctl_addr;
	struct sockaddr_un *local_addr = pri->local_addr;
	struct request req;
	char addr[sizeof("255.255.255.255\0")];
	int fd, n, err;

	if(!pri->hw_setup){
		pri->hwaddr[0] = 0xfe;
		pri->hwaddr[1] = 0xfd;
		pri->hwaddr[2] = 0x0;
		pri->hwaddr[3] = 0x0;
		pri->hwaddr[4] = 0x0;
		pri->hwaddr[5] = 0x0;
		dev_ip_addr(pri->dev, addr, &pri->hwaddr[2]);
		set_ether_mac(pri->dev, pri->hwaddr);
	}
	if((ctl_addr == NULL) || (pri->data_addr == NULL) || 
	   (pri->local_addr == NULL))
		return(-EINVAL);

	if((pri->control = socket(AF_UNIX, SOCK_STREAM, 0)) < 0){
		printk("daemon_open : control socket failed, errno = %d\n", 
		       errno);		
		return(-ENOMEM);
	}

	if(connect(pri->control, (struct sockaddr *) ctl_addr, 
		   sizeof(*ctl_addr)) < 0){
		printk("daemon_open : control connect failed, errno = %d\n", 
		       errno);
		err = -ENOTCONN;
		goto out;
	}

	req.type = REQ_NEW_CONTROL;
	memcpy(req.u.new_control.addr, pri->hwaddr, 
	       sizeof(req.u.new_control.addr));
	req.u.new_control.name = *local_addr;
	n = write(pri->control, &req, sizeof(req));
	if(n != sizeof(req)){
		printk("daemon_open : control setup request returned %d, "
		       "errno = %d\n", n, errno);
		err = -ENOTCONN;
		goto out;		
	}

	if((fd = socket(AF_UNIX, SOCK_DGRAM, 0)) < 0){
		printk("daemon_open : data socket failed, errno = %d\n", 
		       errno);
		err = -ENOMEM;
		goto out;
	}
	if(bind(fd, (struct sockaddr *) local_addr, sizeof(*local_addr)) < 0){
		printk("daemon_open : data bind failed, errno = %d\n", 
		       errno);
		close(fd);
		err = -EINVAL;
		goto out;
	}		
	
	return(fd);
 out:
	close(pri->control);
	return(err);
}

static void daemon_close(int fd, void *data)
{
	struct daemon_data *pri = data;

	close(fd);
	close(pri->control);
}

int daemon_user_read(int fd, void *buf, int len, struct daemon_data *data)
{
	int n;

	while(((n = recvfrom(fd, buf, len, 0, NULL, NULL)) < 0) && 
	      (errno == EINTR)) ;
	if(n < 0){
		if(errno == EAGAIN) return(0);
		return(-errno);
	}
	else if(n == 0) return(-ENOTCONN);
	return(n);
}

int daemon_user_write(int fd, void *buf, int len, struct daemon_data *pri)
{
	int n;
	struct sockaddr_un *data_addr = pri->data_addr;

	n = sendto(fd, buf, len, 0, (struct sockaddr *) data_addr, 
		   sizeof(*data_addr));
	if(n < 0){
		if(errno == EAGAIN) return(0);
		return(-errno);
	}
	return(n);
}

static int daemon_set_mtu(int mtu, void *data)
{
	return(mtu);
}

int daemon_user_set_mac(struct daemon_data *pri, unsigned char *hwaddr,
			 int len)
{
	memcpy(pri->hwaddr, hwaddr, len);
	return(0);
}

struct net_user_info daemon_user_info = {
	init:		daemon_user_init,
	open:		daemon_open,
	close:	 	daemon_close,
	set_mtu:	daemon_set_mtu,
	add_address:	NULL,
	delete_address: NULL,
	max_packet:	MAX_PACKET - ETH_HEADER_OTHER
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
