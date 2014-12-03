/*
 * linux/drivers/usbd/l7205_bi/l7205.h -- L7205 USB controller driver. 
 *
 * Copyright (c) 2000, 2001 Lineo
 *
 * By: 
 *      Stuart Lynne <sl@lineo.com>, 
 *      Tom Rushworth <tbr@lineo.com>, 
 *      Bruce Balden <balden@lineo.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 */


#define MAX_DEVICES 1
#undef USE_DMA

//#define DOLOOPBACK 1
//#define DOLOOPDUMP 1

/* struct l7205_bi_data
 *
 * private data structure for this bus interface driver
 */
struct l7205_bi_data {
    int num;
    //struct tq_struct    sent_bh;
    struct tq_struct    cradle_bh;
};



int l7205_bi_send_urb(struct urb *);

int udc_init(void);
void udc_cleanup(void);
void udc_irq_enable(void);

//int udc_stall_ep(int);
//int udc_reset_ep(int);
void udc_dma_init(void);
void udc_disable(void);
void udc_enable(struct usb_device_instance *device);
//void udc_enable_interrupts(int);
//void udc_disable_interrupts(int);
//int udc_endpoint_halted(int);
void udc_set_address(unsigned char);
void udc_toggle(volatile unsigned long , unsigned int );

void udc_dump_fifos(char *, int ); 

int ep0_init(struct usb_device_instance *);
int ep1_enable(struct usb_device_instance *, struct usb_endpoint_instance *);
int ep2_enable(struct usb_device_instance *, struct usb_endpoint_instance *);

void ep0_int_hndlr(unsigned int);
void ep1_int_hndlr(unsigned int, unsigned int, int, int);
void ep2_int_hndlr(unsigned int, unsigned int, int, int, int, int);

int ep1_restart(void);
void ep1_clear(int);

int ep0_reset(void);
void ep1_reset(void);
void ep2_reset(void);

int ep0_enable(struct usb_device_instance *);


void ep1_disable(void);
void ep2_disable(void);

void ep2_start_tx(unsigned int ep, struct usb_endpoint_instance *endpoint, int restart);

//void ep2_send(void);
void udc_connect(void);
void udc_disconnect(void);

volatile int udc(volatile unsigned int *);

void l7205_poke(void);
void l7205_cradle_event(void);

extern unsigned int udc_interrupts;
extern unsigned int ep0_interrupts;
extern unsigned int tx_interrupts;
extern unsigned int rx_interrupts;
extern unsigned int udc_address_errors;
extern unsigned int f1rq_errors;
extern unsigned int f1err_count;
extern unsigned int udc_rpe_errors;
extern unsigned int udc_fcs_errors;
extern unsigned int udc_ep1_errors;
extern unsigned int udc_ep2_errors;

extern int udc_address;

void l7205_kickoff(void);
void l7205_killoff(void);

int l7205_bi_receive_data(int , void *, int , int );

void l7205_bi_do_event(int , usb_device_event_t );

#define SET_AND_TEST(s,t,c) for (c=10; ((s), (t)) && c--;)

extern int udc_incradle(void);

#if 1
static __inline__ volatile int _udc(volatile unsigned int *regaddr)
{
    volatile unsigned int value;
    int ok;
    for(ok = 1000, value = *(regaddr); value != *(regaddr) && ok--; value = *(regaddr));
    if (!ok) {
        printk(KERN_DEBUG "udc: NOT OK: %p %x\n", regaddr, value);
    }
    return value;
}
#endif

extern struct usb_device_instance *device_array[MAX_DEVICES];;

extern int usbd_tx_dma;
extern int usbd_rx_dma;

extern int dbgflg_usbdbi_init;
extern int dbgflg_usbdbi_intr;
extern int dbgflg_usbdbi_tick;
extern int dbgflg_usbdbi_usbe;
extern int dbgflg_usbdbi_rx;
extern int dbgflg_usbdbi_tx;
extern int dbgflg_usbdbi_setup;
extern int dbgflg_usbdbi_ep0;

#define dbg_init(lvl,fmt,args...)  dbgPRINT(dbgflg_usbdbi_init,lvl,fmt,##args)
#define dbg_intr(lvl,fmt,args...)  dbgPRINT(dbgflg_usbdbi_intr,lvl,fmt,##args)
#define dbg_tick(lvl,fmt,args...)  dbgPRINT(dbgflg_usbdbi_tick,lvl,fmt,##args)
#define dbg_usbe(lvl,fmt,args...)  dbgPRINT(dbgflg_usbdbi_usbe,lvl,fmt,##args)
#define dbg_rx(lvl,fmt,args...)    dbgPRINT(dbgflg_usbdbi_rx,lvl,fmt,##args)
#define dbg_tx(lvl,fmt,args...)    dbgPRINT(dbgflg_usbdbi_tx,lvl,fmt,##args)
#define dbg_setup(lvl,fmt,args...) dbgPRINT(dbgflg_usbdbi_setup,lvl,fmt,##args)
#define dbg_ep0(lvl,fmt,args...)   dbgPRINT(dbgflg_usbdbi_ep0,lvl,fmt,##args)

