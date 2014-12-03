/*
 * user-mode-linux networking multicast transport
 * Copyright (C) 2001 by Harald Welte <laforge@gnumonks.org>
 *
 * based on the existing uml-networking code, which is
 * Copyright (C) 2001 Lennert Buytenhek (buytenh@gnu.org) and 
 * James Leu (jleu@mindspring.net).
 * Copyright (C) 2001 by various other people who didn't put their name here.
 *
 * Licensed under the GPL.
 *
 */

#include <errno.h>
#include <unistd.h>
#include <linux/inet.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/time.h>
#include <netinet/in.h>
#include "net_user.h"
#include "mcast.h"
#include "kern_util.h"
#include "user_util.h"
#include "user.h"

#define MAX_PACKET (ETH_MAX_PACKET + ETH_HEADER_OTHER)

static struct sockaddr_in *new_addr(char *addr, unsigned short port)
{
	struct sockaddr_in *sin;

	sin = um_kmalloc(sizeof(struct sockaddr_in));
	if(sin == NULL){
		printk("new_addr: allocation of sockaddr_in failed\n");
		return(NULL);
	}
	sin->sin_addr.s_addr = in_aton(addr);
	sin->sin_port = port;
	return(sin);
}

static void mcast_user_init(void *data, void *dev)
{
	struct mcast_data *pri = data;

	pri->mcast_addr = new_addr(pri->addr, pri->port);
	pri->dev = dev;
}

static int mcast_open(void *data)
{
	struct mcast_data *pri = data;
	struct sockaddr_in *sin = pri->mcast_addr;
	struct ip_mreq mreq;
	char addr[sizeof("255.255.255.255\0")];
	int fd, err, yes = 1;


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

	if ((sin->sin_addr.s_addr == 0) || (sin->sin_port == 0)) {
		err = -EINVAL;
		goto out;
	}

	if ((fd = socket(AF_INET, SOCK_DGRAM, 0)) < 0){
		printk("mcast_open : data socket failed, errno = %d\n", 
		       errno);
		err = -ENOMEM;
		goto out;
	}

	if (setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes)) < 0) {
		printk("mcast_open: SO_REUSEADDR failed, errno = %d\n",
			errno);
		close(fd);
		err = -EINVAL;
		goto out;
	}

	/* set ttl according to config */
	if (setsockopt(fd, SOL_IP, IP_MULTICAST_TTL, &pri->ttl,
		       sizeof(pri->ttl)) < 0) {
		printk("mcast_open: IP_MULTICAST_TTL failed, error = %d\n",
			errno);
		close(fd);
		err = -EINVAL;
		goto out;
	}

	/* set LOOP, so data does get fed back to local sockets */
	if (setsockopt(fd, SOL_IP, IP_MULTICAST_LOOP, &yes, sizeof(yes)) < 0) {
		printk("mcast_open: IP_MULTICAST_LOOP failed, error = %d\n",
			errno);
		close(fd);
		err = -EINVAL;
		goto out;
	}

	/* bind socket to mcast address */
	if (bind(fd, (struct sockaddr *) sin, sizeof(*sin)) < 0) {
		printk("mcast_open : data bind failed, errno = %d\n", errno);
		close(fd);
		err = -EINVAL;
		goto out;
	}		
	
	/* subscribe to the multicast group */
	mreq.imr_multiaddr.s_addr = sin->sin_addr.s_addr;
	mreq.imr_interface.s_addr = 0;
	if (setsockopt(fd, SOL_IP, IP_ADD_MEMBERSHIP, 
		       &mreq, sizeof(mreq)) < 0) {
		printk("mcast_open: IP_ADD_MEMBERSHIP failed, error = %d\n",
			errno);
		close(fd);
		err = -EINVAL;
		goto out;
	}

	return(fd);
 out:
	return(err);
}

static void mcast_close(int fd, void *data)
{
	struct ip_mreq mreq;
	struct mcast_data *pri = data;
	struct sockaddr_in *sin = pri->mcast_addr;

	mreq.imr_multiaddr.s_addr = sin->sin_addr.s_addr;
	mreq.imr_interface.s_addr = 0;
	if (setsockopt(fd, SOL_IP, IP_DROP_MEMBERSHIP,
		       &mreq, sizeof(mreq)) < 0) {
		printk("mcast_open: IP_DROP_MEMBERSHIP failed, error = %d\n",
			errno);
	}

	close(fd);
}

int mcast_user_read(int fd, void *buf, int len, struct mcast_data *data)
{
	int n;

	while (((n = recvfrom(fd, buf, len, 0, NULL, NULL)) < 0) && 
	       (errno == EINTR)) ;

	if (n < 0) {
		if (errno == EAGAIN) 
			return 0;
		return -errno;
	} else if (n == 0) 
		return -ENOTCONN;

	return n;
}

int mcast_user_write(int fd, void *buf, int len, struct mcast_data *pri)
{
	int n;
	struct sockaddr_in *data_addr = pri->mcast_addr;

	n = sendto(fd, buf, len, 0, (struct sockaddr *) data_addr, 
		   sizeof(*data_addr));

	if (n < 0) 
		return(-errno);

	return(n);
}

static int mcast_set_mtu(int mtu, void *data)
{
	return(mtu);
}

int mcast_user_set_mac(struct mcast_data *pri, unsigned char *hwaddr,
			 int len)
{
	memcpy(pri->hwaddr, hwaddr, len);
	return 0;
}

struct net_user_info mcast_user_info = {
	init:		mcast_user_init,
	open:		mcast_open,
	close:	 	mcast_close,
	set_mtu:	mcast_set_mtu,
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
