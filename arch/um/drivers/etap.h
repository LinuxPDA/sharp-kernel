/* 
 * Copyright (C) 2001 Jeff Dike (jdike@karaya.com)
 * Licensed under the GPL
 */

#include "net_user.h"

struct ethertap_data {
	char *dev_name;
	char *gate_addr;
	int data_fd;
	int control_fd;
	void *dev;
       unsigned char hw_addr[ETH_ADDR_LEN];
	int hw_setup;
};

extern struct net_user_info ethertap_user_info;
extern int etap_user_write(int fd, void *buf, int len, 
			   struct ethertap_data *pri);
extern int etap_user_read(int fd, void *buf, int len, 
			  struct ethertap_data *pri);

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
