/*
 * Copyright (C) 2001 Lennert Buytenhek (buytenh@gnu.org)
 * Licensed under the GPL
 */

#ifndef __MCONSOLE_H__
#define __MCONSOLE_H__

struct mconsole_request
{
	int has_correct_credentials;
	int len;
	char buf[512];

	int originating_fd;
	int originlen;
	unsigned char origin[128];			/* sockaddr_un */
};

struct mconsole_command
{
	char *command;
	void (*handler)(struct mconsole_request *req);
};

extern char socket_name[];

extern int unlink_socket(void);
extern int mconsole_reply(struct mconsole_request *req, char *reply);
extern void mconsole_version(struct mconsole_request *req);
extern void mconsole_help(struct mconsole_request *req);
extern void mconsole_halt(struct mconsole_request *req);
extern void mconsole_reboot(struct mconsole_request *req);
extern void mconsole_config(struct mconsole_request *req);
extern void mconsole_remove(struct mconsole_request *req);
extern void mconsole_sysrq(struct mconsole_request *req);

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
