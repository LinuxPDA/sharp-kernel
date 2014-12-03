/*
 * linux/drivers/usbd/sa1100_bi/sa1100.h 
 *
 * Copyright (c) 2000, 2001 Lineo
 * Copyright (c) 2001 Hewlett Packard
 *
 * Adapted from: Copyright (C)  Compaq Computer Corporation, 1998, 1999
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

#if defined(CONFIG_SA1100_ASSABET) || defined(CONFIG_SA1100_BITSY)
#define CONFIG_SA1100_USBCABLE_GPIO 23
#define CONFIG_SA1100_USBCABLE_ACTIVE_HIGH 1
#undef  CONFIG_SA1100_CONNECT_GPIO
#undef  CONFIG_SA1100_CONNECT_ACTIVE_HIGH 
#endif

#if defined(CONFIG_SA1110_CALYPSO)
#define CONFIG_SA1100_USBCABLE_GPIO 12
#undef  CONFIG_SA1100_USBCABLE_ACTIVE_HIGH
#define CONFIG_SA1100_CONNECT_GPIO 27
#define CONFIG_SA1100_CONNECT_ACTIVE_HIGH 1        
#endif



/* struct sa1100_bi_data
 *
 * private data structure for this bus interface driver
 */
struct sa1100_bi_data {
    int num;
    //struct tq_struct    sent_bh;
    struct tq_struct    cradle_bh;
};


int ep0_reset(void);

void ep0_int_hndlr(unsigned int);
void ep1_int_hndlr(unsigned int);
void ep2_int_hndlr(unsigned int, int);

int ep1_restart(void);

void ep1_reset(void);
void ep2_reset(void);

int ep0_enable(struct usb_device_instance *device, struct usb_endpoint_instance *endpoint);
void ep1_enable(struct usb_device_instance *, struct usb_endpoint_instance *, int);
int ep2_enable(struct usb_device_instance *device, struct usb_endpoint_instance *, int);

void ep0_disable(void);
void ep1_disable(void);
void ep2_disable(void);

void ep2_send(void);

#if 0
void udc_connect(void);
void udc_disconnect(void);
#endif

volatile int udc(volatile unsigned int *);

void sa1100_poke(void);
void sa1100_cradle_event(void);

extern unsigned int udc_interrupts;
extern unsigned int ep0_interrupts;
extern unsigned int tx_interrupts;
extern unsigned int rx_interrupts;
extern unsigned int udc_address_errors;
extern unsigned int udc_rpe_errors;
extern unsigned int udc_fcs_errors;
extern unsigned int udc_ep1_errors;
extern unsigned int udc_ep2_errors;
extern unsigned int udc_ep2_tpe;
extern unsigned int udc_ep2_tur;
extern unsigned int udc_ep2_sst;
extern unsigned int udc_ep2_fst;

extern unsigned int udc_address;

void sa1100_kickoff(void);
void sa1100_killoff(void);

int sa1100_bi_receive_data(int , void *, int , int );

void sa1100_bi_do_event(int , usb_device_event_t );

#define SET_AND_TEST(s,t,c) for (c=20; ((s), (t)) && c--;udelay(1))
#define IOIOIO(reg,val,set,test,rc) do { SET_AND_TEST(set,test,rc); } while(0)


extern int udc_connected(void);
extern void sa1100_cable_event(void);

extern struct usb_device_instance *device_array[MAX_DEVICES];;

extern int usbd_tx_dma;
extern int usbd_rx_dma;

extern int dbgflg_usbdbi_init;
extern int dbgflg_usbdbi_intr;
extern int dbgflg_usbdbi_udc;
extern int dbgflg_usbdbi_tick;
extern int dbgflg_usbdbi_usbe;
extern int dbgflg_usbdbi_rx;
extern int dbgflg_usbdbi_tx;
extern int dbgflg_usbdbi_ep0;

#define dbg_init(lvl,fmt,args...)  dbgPRINT(dbgflg_usbdbi_init,lvl,fmt,##args)
#define dbg_intr(lvl,fmt,args...)  dbgPRINT(dbgflg_usbdbi_intr,lvl,fmt,##args)
#define dbg_udc(lvl,fmt,args...)   dbgPRINT(dbgflg_usbdbi_udc,lvl,fmt,##args)
#define dbg_tick(lvl,fmt,args...)  dbgPRINT(dbgflg_usbdbi_tick,lvl,fmt,##args)
#define dbg_usbe(lvl,fmt,args...)  dbgPRINT(dbgflg_usbdbi_usbe,lvl,fmt,##args)
#define dbg_rx(lvl,fmt,args...)    dbgPRINT(dbgflg_usbdbi_rx,lvl,fmt,##args)
#define dbg_tx(lvl,fmt,args...)    dbgPRINT(dbgflg_usbdbi_tx,lvl,fmt,##args)
#define dbg_ep0(lvl,fmt,args...)   dbgPRINT(dbgflg_usbdbi_ep0,lvl,fmt,##args)

#define EP1_RX_LOGICAL  1
#define EP2_TX_LOGICAL  2

#define EP1_RX_PHYSICAL  0
#define EP2_TX_PHYSICAL  1

#if 1
static __inline__ volatile int _udc(volatile unsigned int *regaddr)
{
    volatile unsigned int value;
    int ok;
    for(ok = 1000, value = *(regaddr); value != *(regaddr) && ok--; value = *(regaddr));
    if (!ok) {
        printk(KERN_ERR"NOT OK: %p %x\n", regaddr, value);
    }
    return value;
}
#endif



// Ser0UDCCR
#define UDCCR  ((volatile unsigned int *)io_p2v(0x80000000))
#define UDCCR_INTS (UDCCR_EIM | UDCCR_RIM | UDCCR_TIM | UDCCR_SRM | UDCCR_REM)
#define UDCCR_ERR29 0x80        // enable ERRATA 29 fix for B5 stepping

#define UDCAR ((volatile unsigned int *)io_p2v(0x80000004))
#define UDCOMP ((volatile unsigned int *)io_p2v(0x80000008))
#define UDCIMP ((volatile unsigned int *)io_p2v(0x8000000C))

// Ser0UDCCS0
#define UDCCS0 ((volatile unsigned int *)io_p2v(0x80000010))

#define UDCD0  ((volatile unsigned int *)io_p2v(0x8000001c))
#define UDCDR  ((volatile unsigned int *)io_p2v(0x80000028))
#define UDCWC  ((volatile unsigned int *)io_p2v(0x80000020))
#define UDCSR  ((volatile unsigned int *)io_p2v(0x80000030))

// Ser0UDCCS1
#define UDCCS1  ((volatile unsigned int *)io_p2v(0x80000014))

// Ser0UDCCS2
#define UDCCS2  ((volatile unsigned int *)io_p2v(0x80000018))

#define VENDOR_SPECIFIC_CLASS       0xff
#define VENDOR_SPECIFIC_SUBCLASS    0xff
#define VENDOR_SPECIFIC_PROTOCOL    0xff

#define DDAR  ((volatile unsigned int *)io_p2v(0xb0000000))
#define DDCS  ((volatile unsigned int *)io_p2v(0xb000000c))
#define DDBA  ((volatile unsigned int *)io_p2v(0xb0000010))
#define DDTA  ((volatile unsigned int *)io_p2v(0xb0000014))
#define DDBB  ((volatile unsigned int *)io_p2v(0xb0000018))
#define DDTB  ((volatile unsigned int *)io_p2v(0xb000001c))

#define DMA_UDC_TRANSMIT                0x80000A04
#define DMA_UDC_RECEIVE                 0x80000A15

