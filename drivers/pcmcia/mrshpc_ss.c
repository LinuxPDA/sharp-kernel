/*
 * linux/drivers/pcmcia/mrshpc_ss.c
 *
 * Copyright (c) 2003 Lineo uSolutions, Inc.
 * Copyright (c) 2001, 2002 Lineo Japan, Inc.
 *
 * Based on sa1100_generic.c
 *
 * May be copied or modified under the terms of the GNU General Public
 * License.  See linux/COPYING for more information.
 *
 * MR-SHPC-01 socket serivce driver
 *
 */
#include <linux/config.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/delay.h>
#include <linux/ioport.h>
#include <linux/kernel.h>
#include <linux/tqueue.h>
#include <linux/timer.h>
#include <linux/mm.h>
#include <linux/notifier.h>
#include <linux/proc_fs.h>
#include <linux/version.h>

#include <asm/mrshpc.h>
#include <asm/io.h>
#include <asm/irq.h>
#include <asm/system.h>

#include <pcmcia/version.h>
#include <pcmcia/cs_types.h>
#include <pcmcia/cs.h>
#include <pcmcia/ss.h>
#include <pcmcia/bus_ops.h>
#include <pcmcia/bulkmem.h>
#include <pcmcia/cistpl.h>
#include "cs_internal.h"

#define MRSHCP_POLL_PERIOD (1*HZ)
#define MRSHCP_MAX_SOCKET  1

struct pcmcia_state {
	unsigned detect  : 1,
	         ready   : 1,
		 bvd1    : 1,
		 bvd2    : 1,
		 wrprot  : 1,
		 vs_3v   : 1,
	  	 vs_Xv   : 1;
};

struct pcmcia_state_array {
	unsigned int size;
	struct pcmcia_state* state;
};

struct pcmcia_configure {
	unsigned sock    : 8,
		 vcc     : 8,
		 vpp     : 8,
	         output  : 1,
		 speaker : 1,
	         reset   : 1;
};

struct mrshpc_socket {
	socket_state_t      cs_state;
	struct pcmcia_state k_state;
	unsigned int        irq;
	void                (*handler)(void*, unsigned int);
	void*               handler_info;
	pccard_io_map       io_map[MAX_IO_WIN];
	pccard_mem_map      mem_map[MAX_WIN];
};
static struct mrshpc_socket mrshpc_ss_socket[MRSHCP_MAX_SOCKET];

static struct timer_list poll_timer;
static struct tq_struct mrshpc_ss_task;

static int  mrshpc_ss_driver_init(void);
static void mrshpc_ss_task_handler(void*);
static void mrshpc_ss_poll_event(unsigned long);

static int  mrshpc_ss_init(unsigned int);
static int  mrshpc_ss_suspend(unsigned int);
static int  mrshpc_ss_register_callback(unsigned int,
					void (*)(void*, unsigned int), void*);
static int  mrshpc_ss_inquire_socket(unsigned int, socket_cap_t*);
static int  mrshpc_ss_get_status(unsigned int, u_int*);
static int  mrshpc_ss_get_socket(unsigned int, socket_state_t*);
static int  mrshpc_ss_set_socket(unsigned int, socket_state_t*);
static int  mrshpc_ss_get_io_map(unsigned int, struct pccard_io_map*);
static int  mrshpc_ss_set_io_map(unsigned int, struct pccard_io_map*);
static int  mrshpc_ss_get_mem_map(unsigned int, struct pccard_mem_map*);
static int  mrshpc_ss_set_mem_map(unsigned int, struct pccard_mem_map*);
#ifdef CONFIG_PROC_FS
static void mrshpc_ss_proc_setup(unsigned int, struct proc_dir_entry*);
static int  mrshpc_ss_proc_status(char*, char**, off_t, int, int*, void*);
#endif

static struct pccard_operations mrshpc_ss_operations = {
	mrshpc_ss_init,
	mrshpc_ss_suspend,
	mrshpc_ss_register_callback,
	mrshpc_ss_inquire_socket,
	mrshpc_ss_get_status,
	mrshpc_ss_get_socket,
	mrshpc_ss_set_socket,
	mrshpc_ss_get_io_map,
	mrshpc_ss_set_io_map,
	mrshpc_ss_get_mem_map,
	mrshpc_ss_set_mem_map,
#ifdef CONFIG_PROC_FS
	mrshpc_ss_proc_setup
#endif
};

static int mrshpc_ss_init(unsigned int sock)
{
	return 0;
}

static int mrshpc_ss_socket_state(struct pcmcia_state_array* state_array)
{
	unsigned int csr;

	memset(state_array->state, 0,
	       (state_array->size) * sizeof(struct pcmcia_state));

	csr = ctrl_inw(MRSHPC_CSR);

	state_array->state[0].detect =
		((csr & (MRSHPC_CSR_CD1|MRSHPC_CSR_CD2)) ? 0: 1);
	state_array->state[0].ready  = ((csr & MRSHPC_CSR_RDY_BSY) ? 1 : 0);
	state_array->state[0].bvd1   = ((csr & MRSHPC_CSR_BVD1)    ? 1 : 0);
	state_array->state[0].bvd2   = ((csr & MRSHPC_CSR_BVD2)    ? 1 : 0);
	state_array->state[0].wrprot = ((csr & MRSHPC_CSR_WPS)     ? 1 : 0);
	state_array->state[0].vs_3v  = ((csr & MRSHPC_CSR_VS1)     ? 0 : 1);
	state_array->state[0].vs_Xv  = ((csr & MRSHPC_CSR_VS2)     ? 0 : 1);

	return 0;
}

static int mrshpc_ss_configure_socket(struct pcmcia_configure* config)
{
#if 0
	printk(KERN_ERR "%s: vcc %d, csr=%04x\n",
	       __FUNCTION__, config->vcc, ctrl_inw(MRSHPC_CSR));
#endif

#if defined(CONFIG_SH_7760_SOLUTION_ENGINE)
	ctrl_outw(0x0009, 0xa4000000);
	udelay(20000);
	udelay(20000);
	ctrl_outw(0x0001, 0xa4000000);

	udelay(20000);

	ctrl_outw(0x0030, MRSHPC_MODE);
	ctrl_outw(0x0003, MRSHPC_OPTION);
#endif

#if 0
#if defined(CONFIG_SH_MS7727RP)
	ctrl_outw(ctrl_inw(MRSHPC_MODE) | MRSHPC_MODE_MODE66, MRSHPC_MODE);
	udelay(2000);
#endif
#endif

	switch (config->vcc) {
	case 0:	
#if 0
		ctrl_outw(MRSHPC_CPWCR_CARD_POW_MASK|
			  MRSHPC_CPWCR_CARD_RESET|
			  MRSHPC_CPWCR_CARD_ENABLE|
			  MRSHPC_CPWCR_AUTO_POWER, MRSHPC_CPWCR);
#endif
		return 0;
	case 50:
		ctrl_outw(MRSHPC_CPWCR_CARD_POW_MASK|
			  MRSHPC_CPWCR_CARD_RESET|
			  MRSHPC_CPWCR_CARD_ENABLE|
			  MRSHPC_CPWCR_AUTO_POWER|
			  MRSHPC_CPWCR_VCC_POWER|
			  MRSHPC_CPWCR_VCC5V, MRSHPC_CPWCR);
		break;
	case 33:
		ctrl_outw(MRSHPC_CPWCR_CARD_POW_MASK|
			  MRSHPC_CPWCR_CARD_RESET|
			  MRSHPC_CPWCR_CARD_ENABLE|
			  MRSHPC_CPWCR_AUTO_POWER|
			  MRSHPC_CPWCR_VCC_POWER|
			  MRSHPC_CPWCR_VCC3V, MRSHPC_CPWCR);
		break;
	default:
		printk(KERN_ERR "%s: unrecognized vcc %u\n",
		       __FUNCTION__, config->vcc);
		return -1;
	}
	udelay(20000);
	udelay(20000);
	udelay(20000);
	udelay(20000);
	udelay(20000);

	ctrl_outw(0x8a84, MRSHPC_MW0CR1);
	if (ctrl_inw(MRSHPC_CSR) & 0x4000) {
		ctrl_outw(0x0b00, MRSHPC_MW0CR2);
	}
	else {
		ctrl_outw(0x0300, MRSHPC_MW0CR2); 
	}
	ctrl_outw(0x8a85, MRSHPC_MW1CR1);
	if (ctrl_inw(MRSHPC_CSR) & 0x4000) {
		ctrl_outw(0x0a00, MRSHPC_MW1CR2);
	}
	else {
		ctrl_outw(0x0200, MRSHPC_MW1CR2);
	}
	ctrl_outw(0x8a86, MRSHPC_IOWCR1);
	ctrl_outw(0x0008, MRSHPC_CDCR);
	if (ctrl_inw(MRSHPC_CSR) & 0x4000) {
		ctrl_outw(0x0a00, MRSHPC_IOWCR2);
	}
	else {
		ctrl_outw(0x0200, MRSHPC_IOWCR2);
	}
#if defined(CONFIG_SH_MS7710SE)
	ctrl_outw(0x2800, MRSHPC_ICR); // SIR1
#else
	ctrl_outw(0x2000, MRSHPC_ICR);
#endif

	return 0;
}

static int mrshpc_ss_suspend(unsigned int sock)
{
	struct pcmcia_configure config;
	int ret;

	config.sock    = sock;
	config.vcc     = 0;
	config.vpp     = 0;
	config.output  = 0;
	config.speaker = 0;
	config.reset   = 1;

	ret = mrshpc_ss_configure_socket(&config);
	if (ret == 0) {
		mrshpc_ss_socket[sock].cs_state = dead_socket;
	}

	return ret;
}

static unsigned int mrshpc_ss_events(struct pcmcia_state* state,
				     struct pcmcia_state* prev_state,
				     unsigned int mask,
				     unsigned int flags)
{
	unsigned int events = 0;

	if (state->detect != prev_state->detect) {
		events |= mask & SS_DETECT;
	}
	if (state->ready != prev_state->ready) {
		events |= mask & (flags & SS_IOCARD ? 0 : SS_READY);
	}
	if (state->bvd1 != prev_state->bvd1) {
		events |= mask & (flags & SS_IOCARD ? SS_STSCHG : SS_BATDEAD);
	}
	if (state->bvd2 != prev_state->bvd2) {
		events |= mask & (flags & SS_IOCARD ? 0 : SS_BATWARN);
	}

	*prev_state = *state;

	return events;
}

static void mrshpc_ss_task_handler(void* data)
{
	struct pcmcia_state state[MRSHCP_MAX_SOCKET];
	struct pcmcia_state_array state_array;
	int i, events, all_events;

	state_array.size  = 1;
	state_array.state = state;

	do {
		all_events = 0;

		mrshpc_ss_socket_state(&state_array);
		
		for (i = 0; i < state_array.size; i++) {
			events = mrshpc_ss_events(&state[i],
					&mrshpc_ss_socket[i].k_state,
					mrshpc_ss_socket[i].cs_state.csc_mask,
					mrshpc_ss_socket[i].cs_state.flags);
			if (event && mrshpc_ss_socket[i].handler) {
				mrshpc_ss_socket[i].handler(mrshpc_ss_socket[i].handler_info, events);
			}
			all_events |= events;
		}
	} while(all_events);
}

static struct tq_struct mrshpc_ss_task = {
	routine: mrshpc_ss_task_handler
};

static void mrshpc_ss_poll_event(unsigned long dummy)
{
	poll_timer.function = mrshpc_ss_poll_event;
	poll_timer.expires  = jiffies + MRSHCP_POLL_PERIOD;

	add_timer(&poll_timer);

	schedule_task(&mrshpc_ss_task);
}

static int mrshpc_ss_register_callback(unsigned int sock,
				       void (*handler)(void*, unsigned int),
				       void* info)
{
	if (handler) {
		MOD_INC_USE_COUNT;
		mrshpc_ss_socket[sock].handler = handler;
		mrshpc_ss_socket[sock].handler_info = info;
	}
	else {
		mrshpc_ss_socket[sock].handler = NULL;
		MOD_DEC_USE_COUNT;
	}
	return 0;
}

static int mrshpc_ss_inquire_socket(unsigned int sock, socket_cap_t* cap)
{
	cap->features = (SS_CAP_PAGE_REGS | SS_CAP_STATIC_MAP | SS_CAP_PCCARD);

	cap->irq_mask  = 0;
	cap->map_size  = PAGE_SIZE;
	cap->pci_irq   = MRSHPC_IRQ;
	cap->io_offset = MRSHPC_IO_BASE;

	return 0;
}

static int mrshpc_ss_get_status(unsigned int sock, unsigned int* status)
{
	struct pcmcia_state state[MRSHCP_MAX_SOCKET];
	struct pcmcia_state_array state_array;

	state_array.size  = 1;
	state_array.state = state;
	mrshpc_ss_socket_state(&state_array);

	mrshpc_ss_socket[sock].k_state = state[sock];

	*status  = (state[sock].detect ? SS_DETECT : 0);
	*status |= (state[sock].ready  ? SS_READY  : 0);
	*status |= (mrshpc_ss_socket[sock].cs_state.Vcc ? SS_POWERON : 0);

	if (mrshpc_ss_socket[sock].cs_state.flags & SS_IOCARD) {
		*status |= state[sock].bvd1 ? SS_STSCHG : 0;
	}
	else {
		*status |= (state[sock].bvd1 ? 0 : SS_BATDEAD);
		*status |= (state[sock].bvd2 ? 0 : SS_BATWARN);
	}

	*status |= (state[sock].vs_3v ? SS_3VCARD : 0);
	*status |= (state[sock].vs_Xv ? SS_XVCARD : 0);

	return 0;
}

static int mrshpc_ss_get_socket(unsigned int sock, socket_state_t* state)
{
	*state = mrshpc_ss_socket[sock].cs_state;

	return 0;
}

static int mrshpc_ss_set_socket(unsigned int sock, socket_state_t* state)
{
	struct pcmcia_configure configure;

	configure.sock    = sock;
	configure.vcc     = state->Vcc;
	configure.vpp     = state->Vpp;
	configure.output  = (state->flags & SS_OUTPUT_ENA ? 1 : 0);
	configure.speaker = (state->flags & SS_SPKR_ENA   ? 1 : 0);
	configure.reset   = (state->flags & SS_RESET      ? 1 : 0);

	if (mrshpc_ss_configure_socket(&configure) < 0)
		return -1;

	mrshpc_ss_socket[sock].cs_state = *state;
  
	return 0;
}

static int mrshpc_ss_get_io_map(unsigned int sock, struct pccard_io_map* map)
{
	*map = mrshpc_ss_socket[sock].io_map[map->map];

	return 0;
}

static int mrshpc_ss_set_io_map(unsigned int sock, struct pccard_io_map* map)
{
	unsigned long start;

	start = map->start;

	if (map->stop == 1) {
		map->stop = PAGE_SIZE - 1;
	}

	map->start = MRSHPC_IO_BASE;
	map->stop  = map->start + (map->stop - start);

	mrshpc_ss_socket[sock].io_map[map->map] = *map;

	return 0;
}

static int mrshpc_ss_get_mem_map(unsigned int sock, struct pccard_mem_map* map)
{
	*map = mrshpc_ss_socket[sock].mem_map[map->map];

	return 0;
}

static int mrshpc_ss_set_mem_map(unsigned int sock, struct pccard_mem_map* map)
{
	unsigned long start;

	start = map->sys_start;

	if(map->sys_stop == 0) {
		map->sys_stop = PAGE_SIZE - 1;
	}

	if (map->flags & MAP_ATTRIB) {
		map->sys_start = MRSHPC_ATTR_BASE;
	}
	else {	
		map->sys_start = MRSHPC_MEM_BASE;
	}

	map->sys_stop = map->sys_start + (map->sys_stop - start);

	mrshpc_ss_socket[sock].mem_map[map->map] = *map;

	return 0;
}

#if defined(CONFIG_PROC_FS)

static void mrshpc_ss_proc_setup(unsigned int sock,
				 struct proc_dir_entry* base)
{
	struct proc_dir_entry* entry;

	entry = create_proc_entry("status", 0, base);
	if (!entry) {
		printk(KERN_ERR
		       "Unable to install \"status\" procfs entry.\n");
		return;
	}

	entry->read_proc = mrshpc_ss_proc_status;
	entry->data = (void*)sock;
}

static int mrshpc_ss_proc_status(char* buf, char** start, off_t pos,
				 int count, int* eof, void* data)
{
	unsigned int sock = (unsigned int)data;
	char* p = buf;

	p += sprintf(p, "k_flags  : %s%s%s%s%s%s%s\n", 
		     mrshpc_ss_socket[sock].k_state.detect ? "detect " : "",
		     mrshpc_ss_socket[sock].k_state.ready  ? "ready "  : "",
		     mrshpc_ss_socket[sock].k_state.bvd1   ? "bvd1 "   : "",
		     mrshpc_ss_socket[sock].k_state.bvd2   ? "bvd2 "   : "",
		     mrshpc_ss_socket[sock].k_state.wrprot ? "wrprot " : "",
		     mrshpc_ss_socket[sock].k_state.vs_3v  ? "vs_3v "  : "",
		     mrshpc_ss_socket[sock].k_state.vs_Xv  ? "vs_Xv "  : "");

	p += sprintf(p, "status   : %s%s%s%s%s%s%s%s%s\n",
		     mrshpc_ss_socket[sock].k_state.detect ? \
		     "SS_DETECT "  : "",
		     mrshpc_ss_socket[sock].k_state.ready ?
		     "SS_READY "   : "",
		     mrshpc_ss_socket[sock].cs_state.Vcc ?
		     "SS_POWERON " : "",
		     mrshpc_ss_socket[sock].cs_state.flags & SS_IOCARD ? \
		     "SS_IOCARD " : "",
		     ((mrshpc_ss_socket[sock].cs_state.flags & SS_IOCARD) &&
		      (mrshpc_ss_socket[sock].k_state.bvd1)) ? \
		     "SS_STSCHG " : "",
		     (!(mrshpc_ss_socket[sock].cs_state.flags & SS_IOCARD) &&
		      (mrshpc_ss_socket[sock].k_state.bvd1 == 0)) ? \
		     "SS_BATDEAD " : "",
		     (!(mrshpc_ss_socket[sock].cs_state.flags & SS_IOCARD) &&
		      (mrshpc_ss_socket[sock].k_state.bvd2 == 0)) ? \
		     "SS_BATWARN " : "",
		     mrshpc_ss_socket[sock].k_state.vs_3v ? "SS_3VCARD " : "",
		     mrshpc_ss_socket[sock].k_state.vs_Xv ? "SS_XVCARD " : "");
	
	p += sprintf(p, "mask     : %s%s%s%s%s\n",
		     (mrshpc_ss_socket[sock].cs_state.csc_mask & SS_DETECT) ? \
		     "SS_DETECT " : "",
		     (mrshpc_ss_socket[sock].cs_state.csc_mask & SS_READY) ? \
		     "SS_READY " : "",
		     (mrshpc_ss_socket[sock].cs_state.csc_mask & SS_BATDEAD) ?\
		     "SS_BATDEAD " : "",
		     (mrshpc_ss_socket[sock].cs_state.csc_mask & SS_BATWARN) ?\
		     "SS_BATWARN " : "",
		     (mrshpc_ss_socket[sock].cs_state.csc_mask & SS_STSCHG) ? \
		     "SS_STSCHG " : "");

	p += sprintf(p, "cs_flags : %s%s%s%s%s\n",
		     (mrshpc_ss_socket[sock].cs_state.flags & SS_PWR_AUTO) ? \
		     "SS_PWR_AUTO " : "",
		     (mrshpc_ss_socket[sock].cs_state.flags & SS_IOCARD) ? \
		     "SS_IOCARD " : "",
		     (mrshpc_ss_socket[sock].cs_state.flags & SS_RESET) ? \
		     "SS_RESET " : "",
		     (mrshpc_ss_socket[sock].cs_state.flags & SS_SPKR_ENA) ? \
		     "SS_SPKR_ENA " : "",
		     (mrshpc_ss_socket[sock].cs_state.flags & SS_OUTPUT_ENA) ?\
		     "SS_OUTPUT_ENA " : "");

	p += sprintf(p, "Vcc      : %d\n",
		     mrshpc_ss_socket[sock].cs_state.Vcc);
	p += sprintf(p, "Vpp      : %d\n",
		     mrshpc_ss_socket[sock].cs_state.Vpp);
	p += sprintf(p, "irq      : %d\n",
		     mrshpc_ss_socket[sock].cs_state.io_irq);

	return (p - buf);
}

#endif

#if 0 // old function
static int __init mrshpc_ss_driver_init(void)
{
	servinfo_t info;
	struct pcmcia_state state[MRSHCP_MAX_SOCKET];
	struct pcmcia_state_array state_array;

	printk(KERN_INFO "MRSHPC PCMCIA (CS release %s)\n", CS_RELEASE);
	CardServices(GetCardServicesInfo, &info);

	pcmcia_get_card_services_info(&info);
	if (info.Revision != CS_RELEASE_CODE) {
		printk(KERN_ERR "Card Services release codes do not match.\n");
		return -ENODEV;
	}

        ctrl_outw(0x0030, MRSHPC_MODE);
        ctrl_outw(0x0003, MRSHPC_OPTION);

	ctrl_outw(MRSHPC_CPWCR_CARD_POW_MASK|
		  MRSHPC_CPWCR_CARD_RESET|
		  MRSHPC_CPWCR_CARD_ENABLE|
		  MRSHPC_CPWCR_AUTO_POWER, MRSHPC_CPWCR);

	state_array.size  = 1;
	state_array.state = state;

	mrshpc_ss_socket_state(&state_array);
	mrshpc_ss_socket[0].k_state = state[0];

	mrshpc_ss_socket[0].cs_state.csc_mask = SS_DETECT;

	if (register_ss_entry(1, &mrshpc_ss_operations) < 0) {
		printk(KERN_ERR
		       "Unable to register socket service routine.\n");
		return -ENXIO;
	}

	mrshpc_ss_poll_event(0);

	return 0;
}
#endif  // old function
static int __init mrshpc_ss_driver_init(void)
{
	servinfo_t info;
	struct pcmcia_state state[MRSHCP_MAX_SOCKET];
	struct pcmcia_state_array state_array;

	printk(KERN_INFO "MR-SHPC-01 PCMCIA (CS release %s)\n", CS_RELEASE);
	CardServices(GetCardServicesInfo, &info);

	pcmcia_get_card_services_info(&info);
	if (info.Revision != CS_RELEASE_CODE) {
	printk(KERN_ERR "Card Services release codes do not match.\n");
		return -ENODEV;
	}

	if ((ctrl_inw(MRSHPC_CSR) & 0x000c) == 0){

		if ((ctrl_inw(MRSHPC_CSR) & 0x0080) == 0) {
			ctrl_outw(0x0674, MRSHPC_CPWCR); /* Card Vcc is 3.3v? */
		} else {
			ctrl_outw(0x0678, MRSHPC_CPWCR); /* Card Vcc is 5V */
		}
		udelay(20000);
#if 0
		udelay(20000);
		udelay(20000);
		udelay(20000);
		udelay(20000);
#endif
		/*
	 	*  PC-Card window open 
	 	*  flag == COMMON/ATTRIBUTE/IO
	 	*/
		/* common window open */
		ctrl_outw(0x8a84, MRSHPC_MW0CR1);
		if (ctrl_inw(MRSHPC_CSR) & 0x4000) {
			ctrl_outw(0x0b00, MRSHPC_MW0CR2);
		}
		else {
			ctrl_outw(0x0300, MRSHPC_MW0CR2); 
		}
		ctrl_outw(0x8a85, MRSHPC_MW1CR1);
		if (ctrl_inw(MRSHPC_CSR) & 0x4000) {
			ctrl_outw(0x0a00, MRSHPC_MW1CR2);
		}
		else {
			ctrl_outw(0x0200, MRSHPC_MW1CR2);
		}
		ctrl_outw(0x8a86, MRSHPC_IOWCR1);
		ctrl_outw(0x0008, MRSHPC_CDCR);
		if (ctrl_inw(MRSHPC_CSR) & 0x4000) {
			ctrl_outw(0x0a00, MRSHPC_IOWCR2);
		}
		else {
			ctrl_outw(0x0200, MRSHPC_IOWCR2);
		}
		ctrl_outw(0x2000, MRSHPC_ICR);
	
		udelay(20000);
		udelay(20000);

	}else{
		ctrl_outw(MRSHPC_CPWCR_CARD_POW_MASK|
		  	 MRSHPC_CPWCR_CARD_RESET|
			 MRSHPC_CPWCR_CARD_ENABLE|
			 MRSHPC_CPWCR_AUTO_POWER, MRSHPC_CPWCR);

		udelay(20000);

	}

	state_array.size  = 1;
	state_array.state = state;

	mrshpc_ss_socket_state(&state_array);
	mrshpc_ss_socket[0].k_state = state[0];

	mrshpc_ss_socket[0].cs_state.csc_mask = SS_DETECT;

	if (register_ss_entry(1, &mrshpc_ss_operations) < 0) {
		printk(KERN_ERR
		       "Unable to register socket service routine.\n");
		return -ENXIO;
	}

	mrshpc_ss_poll_event(0);

	return 0;
}

#ifdef MODULE
static void __exit mrshpc_ss_driver_exit(void)
{
	del_timer_sync(&poll_timer);

	unregister_ss_entry(&mrshpc_ss_operations);

	flush_scheduled_tasks();
}
#endif

module_init(mrshpc_ss_driver_init);
#ifdef MODULE
module_exit(mrshpc_ss_driver_exit);
#endif

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Lineo Solutions, Inc. http://www.lineo.co.jp/");
MODULE_DESCRIPTION("MR-SHPC-01 socket serivce driver");
