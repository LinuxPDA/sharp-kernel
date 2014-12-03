/* 
 * Copyright (C) 2001 Jeff Dike (jdike@karaya.com)
 * Licensed under the GPL
 */

#ifndef __UM_TUNTAP_H
#define __UM_TUNTAP_H

#include "net_user.h"

struct tuntap_data {
	char *dev_name;
	char *gate_addr;
	int fd;
	void *dev;
	unsigned char hw_addr[ETH_ADDR_LEN];
	int hw_setup;
};

extern struct net_user_info tuntap_user_info;

extern int tuntap_user_write(int fd, void *buf, int len, 
			     struct tuntap_data *pri);
extern int tuntap_user_read(int fd, void *buf, int len, 
			    struct tuntap_data *pri);

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
