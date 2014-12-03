/* 
 * Copyright (C) 2001 Jeff Dike (jdike@karaya.com)
 * Licensed under the GPL
 */

#include <stddef.h>
#include <stdio.h>
#include <errno.h>
#include "user.h"
#include "net_user.h"

int tap_open_common(void *dev, int hw_setup, char *gate_addr)
{
	char addr[sizeof("255.255.255.255\0")];
	char ether[ETH_ADDR_LEN];

	if((gate_addr != NULL) || !hw_setup){
		ether[0] = 0xfe;
		ether[1] = 0xfd;
		ether[2] = 0x0;
		ether[3] = 0x0;
		ether[4] = 0x0;
		ether[5] = 0x0;
		dev_ip_addr(dev, addr, &ether[2]);
	}
	if(gate_addr != NULL){
		int uml_addr[4], tap_addr[4];
		if(sscanf(addr, "%d.%d.%d.%d", &uml_addr[0], &uml_addr[1], 
			  &uml_addr[2], &uml_addr[3]) != 4){
			printk("Invalid UML IP address - '%s'\n", addr);
			return(-EINVAL);
		}
		if(sscanf(gate_addr, "%d.%d.%d.%d", &tap_addr[0], 
			  &tap_addr[1], &tap_addr[2], &tap_addr[3]) != 4){
			printk("Invalid tap IP address - '%s'\n", 
			       gate_addr);
			return(-EINVAL);
		}
		if((uml_addr[0] == tap_addr[0]) && 
		   (uml_addr[1] == tap_addr[1]) && 
		   (uml_addr[2] == tap_addr[2]) && 
		   (uml_addr[3] == tap_addr[3])){
			printk("The tap IP address and the UML eth IP address"
			       " must be different\n");
			return(-EINVAL);
		}
	}
	if(!hw_setup){
		ether[0] = 0xfe;
		ether[1] = 0xfd;
		set_ether_mac(dev, ether);
	}
	return(0);
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
