/*
 * Copyright (C) 2001 Lennert Buytenhek (buytenh@gnu.org)
 * Licensed under the GPL
 */

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <signal.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/uio.h>
#include <sys/un.h>
#include <unistd.h>
#include "user.h"
#include "mconsole.h"
#include "umid.h"

static struct mconsole_command commands[] = {
	{ "version", mconsole_version },
	{ "halt", mconsole_halt },
	{ "reboot", mconsole_reboot },
	{ "config", mconsole_config },
	{ "remove", mconsole_remove },
	{ "sysrq", mconsole_sysrq },
	{ "help", mconsole_help },
};

char socket_name[256];

static int has_correct_credentials(struct msghdr *msg)
{
	struct cmsghdr *cmsg;

	cmsg = CMSG_FIRSTHDR(msg);
	while (cmsg != NULL) {
		if (cmsg->cmsg_level == SOL_SOCKET &&
		    cmsg->cmsg_type == SCM_CREDENTIALS) {
			struct ucred *cred;

			cred = (struct ucred *)CMSG_DATA(cmsg);
			if (cred->uid == getuid())
				return 1;
		}
		cmsg = CMSG_NXTHDR(msg, cmsg);
	}

	return 0;
}

int get_request(int fd, struct mconsole_request *req)
{
	char anc[64];
	struct iovec iov;
	struct msghdr msg;

	iov.iov_base = req->buf;
	iov.iov_len = sizeof(req->buf) - 1;

	msg.msg_name = &(req->origin);
	msg.msg_namelen = sizeof(req->origin);
	msg.msg_iov = &iov;
	msg.msg_iovlen = 1;
	msg.msg_control = anc;
	msg.msg_controllen = sizeof(anc);
	msg.msg_flags = 0;

	req->len = recvmsg(fd, &msg, 0);
	if (req->len < 0)
		return 0;

	if (req->len < sizeof(req->buf))
		req->buf[req->len] = 0;

	req->originlen = msg.msg_namelen;
	req->originating_fd = fd;
	req->has_correct_credentials = has_correct_credentials(&msg);

	return 1;
}

int mconsole_reply(struct mconsole_request *req, char *reply)
{
	struct iovec iov;
	struct msghdr msg;

	iov.iov_base = reply;
	iov.iov_len = strlen(reply);

	msg.msg_name = &(req->origin);
	msg.msg_namelen = req->originlen;
	msg.msg_iov = &iov;
	msg.msg_iovlen = 1;
	msg.msg_control = NULL;
	msg.msg_controllen = 0;
	msg.msg_flags = 0;

	return sendmsg(req->originating_fd, &msg, 0);
}

int unlink_socket(void)
{
	unlink(socket_name);
	return 0;
}

int create_listening_socket(void)
{
	struct sockaddr_un addr;
	char file[256];
	int sock, err, yes = 1;

	sock = socket(PF_UNIX, SOCK_DGRAM, 0);
	if (sock < 0) {
		printk("create_listening_socket - socket failed, errno = %d\n",
		       errno);
		return(-1);
	}

	addr.sun_family = AF_UNIX;

	if(umid_file_name("mconsole", file, sizeof(file))) return(-1);

	strcpy(socket_name, file);
	strcpy(addr.sun_path, file);

	err = bind(sock, (struct sockaddr *) &addr, sizeof(addr));
	if (err < 0) {
		if (errno != EADDRINUSE) {
			printk("create_listening_socket - bind failed, "
			       "errno = %d\n", errno);
			return(-1);
		}
	}

	setsockopt(sock, SOL_SOCKET, SO_PASSCRED, &yes, sizeof(yes));

	return sock;
}

void handle_request(struct mconsole_request *req)
{
	struct mconsole_command *cmd;
	int i;

	if (!req->has_correct_credentials)
		return;

	for(i=0;i<sizeof(commands)/sizeof(commands[0]);i++){
		cmd = &commands[i];
		if (!strncmp(req->buf, cmd->command, strlen(cmd->command))) {
			cmd->handler(req);
			return;
		}
	}
	mconsole_reply(req, "ERR unknown command");
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
