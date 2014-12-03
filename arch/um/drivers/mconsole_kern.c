/*
 * Copyright (C) 2001 Lennert Buytenhek (buytenh@gnu.org)
 * Licensed under the GPL
 */

#include "linux/kernel.h"
#include "linux/slab.h"
#include "linux/init.h"
#include "linux/notifier.h"
#include "linux/reboot.h"
#include "linux/utsname.h"
#include "linux/ctype.h"
#include "linux/interrupt.h"
#include "linux/sysrq.h"
#include "asm/irq.h"
#include "user_util.h"
#include "kern_util.h"
#include "kern.h"
#include "mconsole.h"
#include "mconsole_kern.h"

extern int create_listening_socket(void);
extern int get_request(int fd, struct mconsole_request *req);
extern void handle_request(struct mconsole_request *req);

static int do_unlink_socket(struct notifier_block *, unsigned long, void *);

static struct notifier_block reboot_notifier = {
	notifier_call:		do_unlink_socket,
	priority:		0,
};

int do_unlink_socket(struct notifier_block *notifier, unsigned long what, 
		     void *data)
{
	return(unlink_socket());
}

LIST_HEAD(mc_requests);

void mc_task_proc(void *unused)
{
	struct mconsole_entry *req;
	unsigned long flags;
	int done;

	do {
		save_flags(flags);
		req = list_entry(mc_requests.next, struct mconsole_entry, 
				 list);
		list_del(&req->list);
		done = list_empty(&mc_requests);
		restore_flags(flags);
		handle_request(&req->request);
		kfree(req);
	} while(!done);
}

struct tq_struct mconsole_task = {
	routine:	mc_task_proc,
	data: 		NULL
};

void mconsole_interrupt(int irq, void *dev_id, struct pt_regs *regs)
{
	int fd;
	struct mconsole_entry *new;
	struct mconsole_request req;

	fd = (int) dev_id;
	while (get_request(fd, &req)) {
		new = kmalloc(sizeof(req), GFP_ATOMIC);
		if(new == NULL)
			mconsole_reply(&req, "ERR Out of memory");
		else {
			new->request = req;
			list_add(&new->list, &mc_requests);
		}
	}
	schedule_task(&mconsole_task);
	reactivate_fd(fd);
}

void mconsole_version(struct mconsole_request *req)
{
	char version[256];

	sprintf(version, "OK %s %s %s %s %s", system_utsname.sysname, 
		system_utsname.nodename, system_utsname.release, 
		system_utsname.version, system_utsname.machine);
	mconsole_reply(req, version);
}

#define UML_MCONSOLE_HELPTEXT "OK
Commands:
    version - Get kernel version
    help - Print this message
    halt - Halt UML
    reboot - Reboot UML
    config <dev>=<config> - Add a new device to UML; 
	same syntax as command line
    remove <dev> - Remove a device from the client
"

void mconsole_help(struct mconsole_request *req)
{
	mconsole_reply(req, UML_MCONSOLE_HELPTEXT) ;
}

void mconsole_halt(struct mconsole_request *req)
{
	mconsole_reply(req, "OK");
	machine_halt();
}

void mconsole_reboot(struct mconsole_request *req)
{
	mconsole_reply(req, "OK");
	machine_restart(NULL);
}

LIST_HEAD(mconsole_devices);

void mconsole_register_dev(struct mc_device *new)
{
	list_add(&new->list, &mconsole_devices);
}

static struct mc_device *mconsole_find_dev(char *name)
{
	struct list_head *ele;
	struct mc_device *dev;

	list_for_each(ele, &mconsole_devices){
		dev = list_entry(ele, struct mc_device, list);
		if(!strncmp(name, dev->name, strlen(dev->name)))
			return(dev);
	}
	return(NULL);
}

void mconsole_config(struct mconsole_request *req)
{
	struct mc_device *dev;
	char *ptr = req->buf, *ok;
	int err;

	ptr += strlen("config");
	while(isspace(*ptr)) ptr++;
	dev = mconsole_find_dev(ptr);
	if(dev == NULL){
		mconsole_reply(req, "ERR Bad configuration option");
		return;
	}
	err = (*dev->config)(&ptr[strlen(dev->name)]);
	if(err) ok = "ERR";
	else ok = "OK";
	mconsole_reply(req, ok);
}

void mconsole_remove(struct mconsole_request *req)
{
	struct mc_device *dev;	
	char *ptr = req->buf, *ok;
	int err;

	ptr += strlen("remove");
	while(isspace(*ptr)) ptr++;
	dev = mconsole_find_dev(ptr);
	if(dev == NULL){
		mconsole_reply(req, "ERR Bad remove option");
		return;
	}
	err = (*dev->remove)(&ptr[strlen(dev->name)]);
	if(err) ok = "ERR";
	else ok = "OK";
	mconsole_reply(req, ok);
}

#ifdef CONFIG_MAGIC_SYSRQ
void mconsole_sysrq(struct mconsole_request *req)
{
	char *ptr = req->buf;

	ptr += strlen("sysrq");
	while(isspace(*ptr)) ptr++;

	handle_sysrq(*ptr, (struct pt_regs *) &current->thread.process_regs,
		     NULL, NULL);
	mconsole_reply(req, "OK");
}
#else
void mconsole_sysrq(struct mconsole_request *req)
{
	mconsole_reply(req, "ERR sysrq not compiled in");
}
#endif

int mconsole_init(void)
{
	int err;
	int sock;

	sock = create_listening_socket();
	if (sock < 0) {
		printk("Failed to initialize management console\n");
		return 1;
	}

	register_reboot_notifier(&reboot_notifier);

	err = um_request_irq(MCONSOLE_IRQ, sock, mconsole_interrupt,
			     SA_INTERRUPT | SA_SHIRQ, "mconsole",
			     (void *)sock);
	if (err) {
		printk("Failed to get IRQ for management console\n");
		return 1;
	}
	
	printk("mconsole initialized on %s\n", socket_name);
	return 0;
}

__initcall(mconsole_init);

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
