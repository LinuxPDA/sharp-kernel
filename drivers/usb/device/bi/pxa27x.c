/*
 * linux/drivers/usb/device/bi/pxa_27x.c -- Xscale USB Device Controller driver. 
 *
 * Copyright (c) 2004 Lineo Solutions, Inc.
 *
 * Written by Kana Paku
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
 * Based on
 *
 * linux/drivers/usb/device/bi/pxa.c -- Xscale USB Device Controller driver. 
 *
 * Copyright (c) 2000, 2001, 2002 Lineo
 *
 * By: 
 *      Stuart Lynne <sl@lineo.com>, 
 *      Tom Rushworth <tbr@lineo.com>, 
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
 * Change Log
 *      28-Nov-2003 Lineo Solutions, Inc.   Add module parameter. 'shortpacket'
 *                                          and 'recvpacket'.
 */


/*
 * Testing notes
 *
 * This code was developed on the Intel Lubbock with Cotulla PXA250 processor.
 *
 * The default 
 *
 */

/*****************************************************************************/

#include <linux/config.h>
#include <linux/module.h>

#include "../usbd-export.h"
#include "../usbd-build.h"
#include "../usbd-module.h"

MODULE_AUTHOR ("sl@lineo.com, tbr@lineo.com");
MODULE_DESCRIPTION ("Xscale USB Device Bus Interface");

USBD_MODULE_INFO ("pxa27x_bi 0.1-alpha");

#include <linux/kernel.h>
#include <linux/malloc.h>
#include <linux/interrupt.h>
#include <linux/pci.h>
#include <linux/init.h>

#include <asm/atomic.h>
#include <asm/io.h>

#include <linux/proc_fs.h>
#include <linux/vmalloc.h>

#include <linux/netdevice.h>

#include <asm/irq.h>
#include <asm/system.h>

#include <asm/types.h>

#include <asm/uaccess.h>
#include <asm/io.h>
#include <linux/delay.h>
#include <asm/hardware.h>
#include <asm/dma.h>
#if defined(CONFIG_ARCH_SHARP_SL) && defined(CONFIG_APM_CPU_IDLE)
#include <asm/sharp_apm.h>
#endif

#include "../usbd.h"
#include "../usbd-func.h"
#include "../usbd-bus.h"
#include "../usbd-inline.h"
#include "usbd-bi.h"

#include "pxa27x.h"


static int  shortpacket = 0;
MODULE_PARM(shortpacket, "i");

static int  recvpacket = 5;
MODULE_PARM(recvpacket, "i");


#undef USE_DMA_OUT
#undef USE_DMA_IN

unsigned int int_oscr;

#undef PXA_TRACE
#ifdef PXA_TRACE
typedef enum pxa_trace_type {
        pxa_regs, pxa_setup, pxa_xmit, pxa_ccr, pxa_iro, pxa_in, pxa_out
} pxa_trace_type_t;


typedef struct pxa_regs {
        u32     cs0;
        char *  msg;
} pxa_regs_t;

typedef struct pxa_xmit {
        u32     size;
} pxa_xmit_t;

typedef struct pxa_inout {
        u32     size;
        u32     crs;
} pxa_inout_t;

typedef struct pxa_ccr {
        u32     ccr;
        char *  msg;
} pxa_ccr_t;

typedef struct pxa_iro {
        u32     iro;
        char *  msg;
} pxa_iro_t;

typedef struct pxa_trace {
        pxa_trace_type_t        trace_type;
        u32     interrupts;
        u32     ocsr;
        u64     jiffies;
        union {
                pxa_regs_t       regs;
                pxa_xmit_t       xmit;
                pxa_ccr_t        ccr;
                pxa_iro_t        iro;
                pxa_inout_t      inout;
                struct usb_device_request       setup;
        } trace;
} pxa_trace_t;

#define TRACE_MAX       10000

int trace_next;
pxa_trace_t *pxa_traces;

static __inline__ void PXA_REGS(u32 cs0, char *msg)
{
        if ((trace_next < TRACE_MAX) && pxa_traces) {
                pxa_trace_t *p = pxa_traces + trace_next++;
                p->ocsr = OSCR;
                p->jiffies = jiffies;
                p->interrupts = udc_interrupts;
                p->trace_type = pxa_regs;
                p->trace.regs.cs0 = cs0;
                p->trace.regs.msg = msg;
        }
}

static __inline__ void PXA_SETUP(struct usb_device_request *setup)
{
        if ((trace_next < TRACE_MAX) && pxa_traces) {
                pxa_trace_t *p = pxa_traces + trace_next++;
                p->ocsr = OSCR;
                p->jiffies = jiffies;
                p->interrupts = udc_interrupts;
                p->trace_type = pxa_setup;
                memcpy(&p->trace.setup, setup, sizeof(struct usb_device_request));
        }
}

static __inline__ void PXA_XMIT(u32 size)
{
        if ((trace_next < TRACE_MAX) && pxa_traces) {
                pxa_trace_t *p = pxa_traces + trace_next++;
                p->ocsr = OSCR;
                p->jiffies = jiffies;
                p->interrupts = udc_interrupts;
                p->trace_type = pxa_xmit;
                p->trace.xmit.size = size;
        }
}

static __inline__ void PXA_IN(u32 size, u32 crs)
{
        if ((trace_next < TRACE_MAX) && pxa_traces) {
                pxa_trace_t *p = pxa_traces + trace_next++;
                p->ocsr = OSCR;
                p->jiffies = jiffies;
                p->interrupts = udc_interrupts;
                p->trace_type = pxa_in;
                p->trace.inout.size = size;
                p->trace.inout.crs = crs;
        }
}

static __inline__ void PXA_OUT(u32 size, u32 crs)
{
        if ((trace_next < TRACE_MAX) && pxa_traces) {
                pxa_trace_t *p = pxa_traces + trace_next++;
                p->ocsr = OSCR;
                p->jiffies = jiffies;
                p->interrupts = udc_interrupts;
                p->trace_type = pxa_out;
                p->trace.inout.size = size;
                p->trace.inout.crs = crs;
        }
}

static __inline__ void PXA_CCR(u32 ccr, char *msg)
{
        if ((trace_next < TRACE_MAX) && pxa_traces) {
                pxa_trace_t *p = pxa_traces + trace_next++;
                p->ocsr = OSCR;
                p->jiffies = jiffies;
                p->interrupts = udc_interrupts;
                p->trace_type = pxa_ccr;
                p->trace.ccr.ccr = ccr;
                p->trace.ccr.msg = msg;
        }
}

static __inline__ void PXA_IRO(u32 iro, char *msg)
{
        if ((trace_next < TRACE_MAX) && pxa_traces) {
                pxa_trace_t *p = pxa_traces + trace_next++;
                p->ocsr = OSCR;
                p->jiffies = jiffies;
                p->interrupts = udc_interrupts;
                p->trace_type = pxa_iro;
                p->trace.iro.iro = iro;
                p->trace.iro.msg = msg;
        }
}
#else
static __inline__ void PXA_REGS(u32 cs0, char *msg)
{
}

static __inline__ void PXA_SETUP(struct usb_device_request *setup)
{
}

static __inline__ void PXA_XMIT(u32 size)
{
}

static __inline__ void PXA_CCR(u32 ccr, char *msg)
{
}

static __inline__ void PXA_IRO(u32 iro, char *msg)
{
}

static __inline__ void PXA_IN(u32 size, u32 crs)
{
}

static __inline__ void PXA_OUT(u32 size, u32 crs)
{
}

#endif


#if defined(USE_DMA_IN) || defined(USE_DMA_OUT)
unsigned int dma_interrupts;

void pxa_kill(void)
{
        if ((udc_interrupts > 1000) || (dma_interrupts > 100)) {

                if ((udc_interrupts%100) == 0) {

                        printk(KERN_INFO
                                        "int[%d] %d disabling USIR[%02x %02x] "
                                        "CCR[%02x] UICR[%02x %02x] UFNHR[%02x %02x]\n",
                                        udc_interrupts, dma_interrupts, USIR1, USIR0, UDCCR, UICR1, UICR0, UFNHR, UFNLR);

                }

                UDCCR &= ~( UDCCR_UDE | UDCCR_REM );
                UICR1 = UICR0 = 0xff;
                UFNHR |= UFNHR_SIM;
                USIR1 = USIR1;
                USIR0 = USIR0;
                UDCCS0 = UDCCS0_FTF | UDCCS0_FST | UDCCS0_SA | UDCCS0_OPR;
                CKEN &= ~CKEN11_USB;
                return;

        }

}
#endif /* defined(USE_DMA_IN) || defined(USE_DMA_OUT) */

static int udc_suspended;
static struct usb_device_instance *udc_device;	// required for the interrupt handler

/*
 * ep_endpoints - map physical endpoints to logical endpoints
 */
static struct usb_endpoint_instance *ep_endpoints[UDC_MAX_ENDPOINTS];

static struct urb *ep0_urb;

extern unsigned int udc_interrupts;

#if 0
int jifs(void)
{
        static unsigned long jiffies_last;
        int elapsed = jiffies - jiffies_last;

        jiffies_last = jiffies;
        return elapsed;
}
#else
int jifs(void)
{
        static unsigned long jiffies_last;
        int elapsed = OSCR - jiffies_last;

        jiffies_last = OSCR;
        return elapsed;
}
#endif



/* ********************************************************************************************* */
/* IO
 */

/*
 * Map logical to physical
 */

typedef enum ep {
        ep_control, ep_bulk_in, ep_bulk_out, ep_iso_in, ep_iso_out, ep_interrupt
} ep_t;

/*
 * PXA has lots of endpoints, but they have fixed address and type
 * so logical to physical map is limited to masking top bits so we
 * can find appropriate info.
 */


u32 _UDCDRN[16] = {
        0x40600300, 0x40600304, 0x40600308, 0x4060030C,
        0x40600310, 0x40600314, 0x40600318, 0x4060031C,
        0x40600320, 0x40600324, 0x40600328, 0x4060032C, 
        0x40600330, 0x40600334, 0x40600338, 0x4060033C, 
};

u32 _UDCBCRN[16] = {
        0x40600200,          0,          0x40600208, 0,
        0x40600210, 0,          0,          0x4060021C,
        0,          0x40600224, 0,          0,
        0x40600230, 0,          0x40600238, 0, 
};

#define UDCCSRN(x)        __REG2(0x40600100, (x) << 2)


#define UDCDRN(x)        __REG(_UDCDRN[x])
#define UDCBCRN(x)        __REG(_UDCBCRN[x])

u32 _UDCCRN[16] = {
        0,          0x40600404, 0x40600408, 0x4060040C,
        0x40600410, 0x40600414, 0x40600418, 0x4060041C,
        0x40600420, 0x40600424, 0x40600428, 0x4060042C,
        0x40600430, 0x40600434, 0x40600438, 0x4060043C,
};

#define UDCCRN(x)        __REG(_UDCCRN[x])

#if defined(USE_DMA_IN) || defined(USE_DMA_OUT)

struct ep_map {
        int logical;
        ep_t eptype;
        int size;
        int dma_chan;
        volatile u32 *drcmr;
        u_long dev_addr;
};

#ifdef USE_DMA_OUT
static usb_stream_t usb_stream_out = {
        name:           "PXA USB out",
        dcmd:           (DCMD_INCTRGADDR|DCMD_FLOWSRC|DCMD_BURST32|DCMD_WIDTH1),
        dma_ch:         -1,
        //drcmr:        // copy from ep_maps
        //dev_addr:     // get from UUDRN()
};
#endif /* USE_DMA_OUT */

#ifdef USE_DMA_IN
static usb_stream_t usb_stream_in = {
        name:           "PXA USB in",
        dcmd:           ((DCMD_INCSRCADDR|DCMD_FLOWTRG|DCMD_BURST32|DCMD_WIDTH1)),
        dma_ch:         -1,
        //drcmr:        // copy from ep_maps
        //dev_addr:     // get from UUDRN()
};
#endif /* USE_DMA_IN */

static struct ep_map ep_maps[16] = {
        { logical: 0, eptype: ep_control,   size: 16, },

        { logical: 1, eptype: ep_bulk_in,   size: 64, drcmr: &DRCMR25 },
        { logical: 2, eptype: ep_bulk_out,  size: 64, drcmr: &DRCMR26 },

        { logical: 3, eptype: ep_iso_in,   size: 256, drcmr: &DRCMR27 },
        { logical: 4, eptype: ep_iso_out,  size: 256, drcmr: &DRCMR28 },

        { logical: 5, eptype: ep_interrupt , size: 8, },

        { logical: 6, eptype: ep_bulk_in,   size: 64, drcmr: &DRCMR30 },
        { logical: 7, eptype: ep_bulk_out,  size: 64, drcmr: &DRCMR31 },
                                                      
        { logical: 8, eptype: ep_iso_in,   size: 256, drcmr: &DRCMR33 },
        { logical: 9, eptype: ep_iso_out,  size: 256, drcmr: &DRCMR34 },
                                                      
        { logical: 10, eptype: ep_interrupt, size: 8, },
                                                      
        { logical: 11, eptype: ep_bulk_in,  size: 64, drcmr: &DRCMR35 },
        { logical: 12, eptype: ep_bulk_out, size: 64, drcmr: &DRCMR36 },
                                                      
        { logical: 13, eptype: ep_iso_in,  size: 256, drcmr: &DRCMR37 },
        { logical: 14, eptype: ep_iso_out, size: 256, drcmr: &DRCMR38 },
                                                      
        { logical: 15, eptype: ep_interrupt, size: 8, },
};
#else /* defined(USE_DMA_IN) || defined(USE_DMA_OUT) */

struct ep_map {
        int             logical;
        ep_t            eptype;
        int             size;
        int             dma_chan;
};

static struct ep_map ep_maps[16] = {
        { logical: 0, eptype: ep_control,   size: 16, },
        { logical: 1, eptype: ep_bulk_in,   size: 64, },

        { logical: 2, eptype: ep_bulk_out,  size: 64, },

        { logical: 3, eptype: ep_iso_in,   size: 256, },
        { logical: 4, eptype: ep_iso_out,  size: 256, },

        { logical: 5, eptype: ep_interrupt , size: 8, },

        { logical: 6, eptype: ep_bulk_in,   size: 64, },
        { logical: 7, eptype: ep_bulk_out,  size: 64, },
                                                      
        { logical: 8, eptype: ep_iso_in,   size: 256, },
        { logical: 9, eptype: ep_iso_out,  size: 256, },
                                                      
        { logical: 10, eptype: ep_interrupt, size: 8, },
                                                      
        { logical: 11, eptype: ep_bulk_in,  size: 64, },
        { logical: 12, eptype: ep_bulk_out, size: 64, },
                                                      
        { logical: 13, eptype: ep_iso_in,  size: 256, },
        { logical: 14, eptype: ep_iso_out, size: 256, },
                                                      
        { logical: 15, eptype: ep_interrupt, size: 8, },
};

#endif /* defined(USE_DMA_IN) || defined(USE_DMA_OUT) */


struct udccr_cfg {
	u32			     ee:1;		/* Endpoint Enable */
	u32			     de:1;		/* Double-Buffering Enable */
	u32			     mps:10;	/* Maximum Packet Size */
	u32			     ed:1;		/* USB Endpoint Direction */
	u32			     et:2;		/* USB Endpoint Type */
	u32			     en:4;		/* Endpoint Number */
	u32			     aisn:3;	/* Alternate Interface Number */
	u32			     in:3;		/* Interface Number */
	u32			     cn:2;		/* Configuration Number */
	u32			     resv1:5;	/* Reserved */
};

static struct udccr_cfg udccr_cfgs[] = {
        { ee: 1, de: 1, mps: 64,  ed: 1, et: 2, en: 1,  aisn: 0, in: 0, cn: 1, },
        { ee: 1, de: 1, mps: 64,  ed: 0, et: 2, en: 2,  aisn: 0, in: 0, cn: 1, },
        { ee: 1, de: 1, mps: 256, ed: 1, et: 1, en: 3,  aisn: 0, in: 0, cn: 1, },
        { ee: 1, de: 1, mps: 256, ed: 0, et: 1, en: 4,  aisn: 0, in: 0, cn: 1, },
        { ee: 1, de: 0, mps: 8,   ed: 1, et: 3, en: 5,  aisn: 0, in: 0, cn: 1, },

        { ee: 0},
        { ee: 0},
        { ee: 0},
        { ee: 0},
        { ee: 0},

        { ee: 0},
        { ee: 0},
        { ee: 0},
        { ee: 0},
        { ee: 0},
};

/* indices for ... */
#define FCS_DEFAULT				-1
int lockFCS_status = FCS_DEFAULT;
static void pxa_lockFCS (unsigned long lock_index, int action);

static __inline__ void pxa_enable_ep_interrupt(int ep)
{
        ep &= 0xf;
        UDCICR0 |= 0x3<<(ep*2);
}

static __inline__ void pxa_disable_ep_interrupt(int ep)
{
        ep &= 0xf;
        UDCICR0 &= ~(0x3<<(ep*2));
}

static __inline__ void pxa_ep_reset_irs(int ep)
{
        ep &= 0xf;
        UDCISR0 |= 0x1<<(ep*2);
}



/* ********************************************************************************************* */
/* Bulk OUT (recv)
 */

static void pxa_out_flush(int ep) {

        unsigned long c;

        while (UDCCSRN(ep) & UDCCSR0_RNE) {
                c = UDCDRN(ep);
        }
}

#ifndef USE_DMA_OUT
static __inline__ void pxa_out_n(int ep, struct usb_endpoint_instance *endpoint)
{
        if (UDCCSRN(ep) & UDCCS_PC) {

                if (endpoint) {
                        if (!endpoint->rcv_urb) {
                                endpoint->rcv_urb = first_urb_detached (&endpoint->rdy);
                        }
                        if (endpoint->rcv_urb) {

                            int len = 0;
                            unsigned char *cp = (unsigned char *)(endpoint->rcv_urb->buffer + endpoint->rcv_urb->actual_length);
                            unsigned long dwbuf;

                                if (cp) {
                                    // read available bytes (max packetsize) into urb buffer
                                    int count = MIN(UDCBCRN(ep), endpoint->rcv_packetSize);

                                    PXA_OUT(count, UDCCSRN(ep));

                                    for(; count >= 4; count -= 4, cp += 4, len += 4){
                                        dwbuf = UDCDRN(ep);
                                        memcpy(cp, &dwbuf, 4);
                                    }
                                    if(count){
                                        dwbuf = UDCDRN(ep);
                                        memcpy(cp, &dwbuf, count);
                                        cp += count;
                                        len += count;
                                        count -= count;
                                    }
                                // Clear PC / Error Bit and Update UDCBCR with the State of Second Buffer
                                UDCCSRN(ep) = UDCCS_PC | UDCCS_TRN | UDCCS_SST;
                                }
                                else {
                                    printk(KERN_INFO"read[%d:%d] bad arguements\n", udc_interrupts, jifs());
                                    pxa_out_flush(ep);
                                    // Clear PC / Error Bit
                                    UDCCSRN(ep) = UDCCS_PC | UDCCS_TRN | UDCCS_SST;
                                }
                                // fall through if error of any type, len = 0
                                usbd_rcv_complete_irq (endpoint, len, 0);
                        }
                        else {
                            // Clear PC / Error Bit and Interrupu
                            UDCCSRN(ep) = UDCCS_PC | UDCCS_TRN | UDCCS_SST;
                            pxa_out_flush(ep);
                        }
                }
            pxa_ep_reset_irs(ep);
        }
        else {
            // clear interrupt
            pxa_ep_reset_irs(ep);
        }

}
#else /* !defined(USE_DMA_OUT) */

static void pxa_usb_out_dma_irq(int ch, void *dev_id, struct pt_regs *regs)
{
        usb_stream_t *usb_stream = dev_id;
        u_int dcsr;

        dma_interrupts++;

        pxa_kill();

        printk(KERN_INFO"DMA[%d:%d]\n", udc_interrupts, dma_interrupts);

        // Stop DMA
        dcsr = DCSR(ch);
        DCSR(ch) = dcsr & ~DCSR_STOPIRQEN;

        if (!usb_stream->buffers) {
                printk(KERN_INFO"pxa_usb_out_dma: no buffers\n");
                return;
        }

        if (dcsr & DCSR_BUSERR) {
                printk(KERN_INFO"pxa_usb_out_dma: bus error %d\n",ch);
                return;
        }
#if 0
        if (dcsr & DCSR_ENDINTR) {

                u_long cur_dma_desc;

                // derive DMA descriptor, DDADR points to next descriptor, not current

                cur_dma_desc = DDADR(ch) - usb_stream->dma_desc_phys - sizeof(pxa_dma_desc);
        }
#endif
}

void pxa_start_out_dma(int ep, struct usb_endpoint_instance *endpoint, usb_stream_t *usb_stream) 
{

        DCSR(usb_stream->dma_ch) = DCSR_RUN;

        *usb_stream->drcmr = usb_stream->dma_ch | DRCMR_MAPVLD;

        printk(KERN_INFO"start out dma [%d:%d] dma_ch: %d drcmr: %p %x DCSR: %x DDADR: %x DSADR: %x DTADR: %x DCMD: %x\n", 
                        udc_interrupts, dma_interrupts, usb_stream->dma_ch,
                        usb_stream->drcmr, *usb_stream->drcmr, 
                        DCSR(usb_stream->dma_ch),
                        DDADR(usb_stream->dma_ch),
                        DSADR(usb_stream->dma_ch),
                        DTADR(usb_stream->dma_ch),
                        DCMD(usb_stream->dma_ch));
}

#endif /* !defined(USE_DMA_OUT) */

/* ********************************************************************************************* */
/* Bulk IN (tx)
 */

static void __inline__ pxa_start_n (unsigned int ep, struct usb_endpoint_instance *endpoint)
{
        if (endpoint->tx_urb) {
                int last;
                struct urb *urb = endpoint->tx_urb;

                if (( last = MIN (urb->actual_length - (endpoint->sent + endpoint->last), endpoint->tx_packetSize))) 
                {
                        int size = last;
                        unsigned char *cp = (unsigned char *)(urb->buffer + endpoint->sent + endpoint->last);
                        unsigned long dwbuf;
                        unsigned short wbuf;

                        PXA_IN(size, UDCCSRN(ep));

                        for(; size >= 4; size -= 4, cp += 4){
                            memcpy(&dwbuf, cp, 4);
                            UDCDRN(ep) = dwbuf;
                        }
                        if(size >= 2){
                            memcpy(&wbuf, cp, 2);
                            *((unsigned short*)(&(UDCDRN(ep)))) = wbuf;
                            size -= 2;
                            cp += 2;
                        }
                        if(size >= 1){
                            *((unsigned char*)(&(UDCDRN(ep)))) = *cp;
                            size -= 1;
                            cp += 1;
                        }

                        if (( last < endpoint->tx_packetSize ) ||
                            ( shortpacket == 0 && (endpoint->tx_urb->actual_length - endpoint->sent ) == last )) 
                        {
                                UDCCSRN(ep) = UDCCS_SP;
                        }
                        endpoint->last += last;
                }
        }
}

static void __inline__ pxa_in_n (unsigned int ep, struct usb_endpoint_instance *endpoint)
{
        int udccsn;

        pxa_ep_reset_irs(ep);
        udccsn = UDCCSRN(ep);

        // if TPC update tx urb and clear TPC
        if (udccsn & UDCCS_PC) {
                UDCCSRN(ep) = UDCCS_PC;
                usbd_tx_complete_irq(endpoint, 0);
        }

        // clear underrun, not much we can do about it
        if (udccsn & UDCCS_TRN) {
                UDCCSRN(ep) = UDCCS_TRN;
        }

        udccsn = UDCCSRN(ep);

        if (udccsn & UDCCS_FS) {
                pxa_start_n(ep, endpoint);
        }

}


#ifdef USE_DMA_IN
static void pxa_usb_in_dma_irq(int ch, void *dev_id, struct pt_regs *regs)
{
        u_int dcsr;
        dcsr = DCSR(ch);
        DCSR(ch) = dcsr & ~DCSR_STOPIRQEN;
}
#endif /* USE_DMA_IN */


/* ********************************************************************************************* */
/* Control (endpoint zero)
 */


void pxa_ep0xmit(struct usb_endpoint_instance *endpoint, volatile u32 udccs0)
{
	int short_packet;
	int size;
	struct urb *urb = endpoint->tx_urb;

        PXA_REGS(udccs0, "          --> xmit");

        //printk(KERN_INFO"tx[%d:%d] CS0[%02x]\n", udc_interrupts, jifs(), UDCCS0);

        // check for premature status stage - host abandoned previous IN
        if ((udccs0 & UDCCSR0_OPC) && !(UDCCSR0 & UDCCSR0_SA) ) {

                // clear tx fifo and opr
                UDCCSR0 = UDCCSR0_FTF | UDCCSR0_OPC;
                endpoint->state = WAIT_FOR_SETUP;
                endpoint->tx_urb = NULL;
                PXA_REGS(UDCCSR0, "          <-- xmit premature status");
                return;
        }

        // check for stall
        if (udccs0 & UDCCSR0_SST) {
                // clear stall and tx fifo
                UDCCSR0 = UDCCSR0_SST | UDCCSR0_FTF;
                endpoint->state = WAIT_FOR_SETUP;
                endpoint->tx_urb = NULL;
                PXA_REGS(UDCCSR0, "          <-- xmit stall");
                return;
        }

	/* How much are we sending this time? (May be zero!)
           (Note that later call of tx_complete() will add last to sent.) */
	if (NULL == urb) {
		size = 0;
	} else {
		endpoint->last = size = MIN (urb->actual_length - endpoint->sent, endpoint->tx_packetSize);
	}

	/* Will this be a short packet?
          (It may be the last, but still be full size, in which case we will need a ZLP later.) */
        short_packet = (size < endpoint->tx_packetSize);

        PXA_XMIT(size);

	if (size > 0 && urb->buffer) {
		// Stuff the FIFO
        unsigned char *cp = (unsigned char *)(urb->buffer + endpoint->sent);
        unsigned long dwbuf;
        unsigned short wbuf;

        for(; size >= 4; size -= 4, cp += 4){
            memcpy(&dwbuf, cp, 4);
			UDCDRN(0) = dwbuf;
		}
		if(size >= 2){
			memcpy(&wbuf, cp, 2);
            *((unsigned short*)(&(UDCDRN(0)))) = wbuf;
            size -= 2;
            cp += 2;
		}
		if(size >= 1){
            *((unsigned char*)(&(UDCDRN(0)))) = *cp;
            size -= 1;
            cp += 1;
		}
    }

        // Is this the end of the data state? (We've sent all the data, plus any required ZLP.)
        if (!endpoint->tx_urb || (endpoint->last < endpoint->tx_packetSize)) {
		// Tell the UDC we are at the end of the packet.
		UDCCSR0 = UDCCSR0_IPR;
                endpoint->state = WAIT_FOR_OUT_STATUS;
                PXA_REGS(UDCCSR0, "          <-- xmit wait for status");
	}

        else if ((endpoint->last == endpoint->tx_packetSize) && 
                        ((endpoint->last + endpoint->sent) == ep0_urb->actual_length)  &&
                        ((ep0_urb->actual_length) < le16_to_cpu(ep0_urb->device_request.wLength)) 
                ) 
        {
		// Tell the UDC we are at the end of the packet.
                endpoint->state = DATA_STATE_NEED_ZLP;
                PXA_REGS(UDCCSR0, "          <-- xmit need zlp");
	}
        else {
                PXA_REGS(UDCCSR0, "          <-- xmit not finished");
        }
}

void __inline__ pxa_ep0setup(struct usb_endpoint_instance *endpoint, volatile u32 udccs0)
{
        if ((udccs0 & (UDCCSR0_SA | UDCCSR0_OPC | UDCCSR0_RNE)) == (UDCCSR0_SA | UDCCSR0_OPC | UDCCSR0_RNE)) {

                int len = 0;
                int max = 8;
                unsigned long dwbuf;
                unsigned char *cp = (unsigned char *)&ep0_urb->device_request;

                PXA_REGS(udccs0, "      --> setup");

                //memset(cp, 0, max);

                for(; max; max -= 4, cp += 4, len += 4){
                        dwbuf = UDCDR0;
                        memcpy(cp, &dwbuf, 4);
                }
                PXA_SETUP(&ep0_urb->device_request);

                // process setup packet
                if (usbd_recv_setup(ep0_urb)) {
                        // setup processing failed 
                        UDCCSR0 = UDCCSR0_FST;
                        endpoint->state = WAIT_FOR_SETUP;
                        PXA_REGS(udccs0, "      --> bad setup FST");
                        return;
                }

                // check direction
                if ((ep0_urb->device_request.bmRequestType & USB_REQ_DIRECTION_MASK) == USB_REQ_HOST2DEVICE) 
                {
                        // Control Write - we are receiving more data from the host

                        // should we setup to receive data
                        if (le16_to_cpu (ep0_urb->device_request.wLength)) {
                                //printk(KERN_INFO"sl11_ep0: read data %d\n",
                                //      le16_to_cpu(ep0_urb->device_request.wLength));
                                endpoint->rcv_urb = ep0_urb;
                                endpoint->rcv_urb->actual_length = 0;
                                // XXX this has not been tested
                                // pxa_out_0 (0, endpoint);
                                PXA_REGS(UDCCSR0,"      <-- setup no response");
                                return;
                        }

                        // allow for premature IN (c.f. 12.5.3 #8)
                        UDCCSR0 = UDCCSR0_IPR;
                        PXA_REGS(UDCCSR0,"      <-- setup nodata");
                }
                else {
                        // Control Read - we are sending data to the host

                        // verify that we have non-zero request length
                        if (!le16_to_cpu (ep0_urb->device_request.wLength)) {
                                udc_stall_ep (0);
                                PXA_REGS(UDCCSR0,"      <-- setup stalling zero wLength");
                                return;
                        }
                        // verify that we have non-zero length response
                        if (!ep0_urb->actual_length) {
                                udc_stall_ep (0);
                                PXA_REGS(UDCCSR0,"      <-- setup stalling zero response");
                                return;
                        }

                        // start sending
                        endpoint->tx_urb = ep0_urb;
                        endpoint->sent = 0;
                        endpoint->last = 0;
                        endpoint->state = DATA_STATE_XMIT;
			pxa_ep0xmit(endpoint, UDCCSR0);

                        // Clear SA and OPR bits
                        UDCCSR0 = UDCCSR0_SA | UDCCSR0_OPC;
                        PXA_REGS(UDCCSR0,"      <-- setup data");
                }

        }

}


static __inline__ void pxa_ep0(struct usb_endpoint_instance *endpoint, volatile u32 udccs0)
{
        int j = 0;

        PXA_REGS(udccs0,"  --> ep0");

        if (udccs0 & UDCCSR0_SST) {
                pxa_ep_reset_irs(0);
                UDCCSR0 = UDCCSR0_SST;
                PXA_REGS(udccs0,"  --> ep0 clear SST");
                if (endpoint) {
                        endpoint->state = WAIT_FOR_SETUP;
                        endpoint->tx_urb = NULL;
                }
                return;
        }


        if (!endpoint) {
                printk(KERN_INFO"ep0[%d:%d] endpoint Zero is NULL CS0[%02x]\n", udc_interrupts, jifs(), UDCCSR0);
                pxa_ep_reset_irs(0);
                UDCCSR0 = UDCCSR0_IPR | UDCCSR0_OPC | UDCCSR0_SA;
                PXA_REGS(UDCCSR0,"  ep0 NULL");
                return;
        }

        if (endpoint->tx_urb) {
                usbd_tx_complete_irq (endpoint, 0);
                if (!endpoint->tx_urb) {
                        if (endpoint->state != DATA_STATE_NEED_ZLP) {
                             endpoint->state = WAIT_FOR_OUT_STATUS;
                        }
                }
        }

        if ((endpoint->state != WAIT_FOR_SETUP) && (udccs0 & UDCCSR0_SA)) {
                PXA_REGS(udccs0,"  --> ep0 early SA");
                endpoint->state = WAIT_FOR_SETUP;
                endpoint->tx_urb = NULL;
        }

        switch (endpoint->state) {
        case DATA_STATE_NEED_ZLP:
                UDCCSR0 = UDCCSR0_IPR;
                endpoint->state = WAIT_FOR_OUT_STATUS;
                break;

        case WAIT_FOR_OUT_STATUS:
                if ((udccs0 & (UDCCSR0_OPC | UDCCSR0_SA)) == (UDCCSR0_OPC)) {
                        UDCCSR0 = UDCCSR0_OPC;
                }
                PXA_REGS(UDCCSR0,"  --> ep0 WAIT for STATUS");
                endpoint->state = WAIT_FOR_SETUP;

        case WAIT_FOR_SETUP:
                do {
                        pxa_ep0setup(endpoint, UDCCSR0);

                        if (udccs0 & UDCCSR0_SST) {
                                pxa_ep_reset_irs(0);
                                UDCCSR0 = UDCCSR0_SST | UDCCSR0_OPC | UDCCSR0_SA;
                                PXA_REGS(udccs0,"  --> ep0 clear SST");
                                if (endpoint) {
                                        endpoint->state = WAIT_FOR_SETUP;
                                        endpoint->tx_urb = NULL;
                                }
                                return;
                        }

                        if (j++ > 2) {
                                u32 udccs0 = UDCCSR0;
                                PXA_REGS(udccs0,"  ep0 wait");
                                if ((udccs0 & (UDCCSR0_OPC | UDCCSR0_SA | UDCCSR0_RNE)) == (UDCCSR0_OPC | UDCCSR0_SA)) {
                                        UDCCSR0 = UDCCSR0_OPC | UDCCSR0_SA;
                                        PXA_REGS(UDCCSR0,"  ep0 force");
                                }
                                else {
                                        UDCCSR0 = UDCCSR0_OPC | UDCCSR0_SA;
                                        PXA_REGS(UDCCSR0,"  ep0 force and return");
                                        break;
                                }
                        }

                } while (UDCCSR0 & (UDCCSR0_OPC | UDCCSR0_RNE));
                break;

        case DATA_STATE_XMIT:
                pxa_ep0xmit(endpoint, UDCCSR0);
                break;

        }

        pxa_ep_reset_irs(0);
        PXA_REGS(UDCCSR0,"  <-- ep0");
}

/* ********************************************************************************************* */
/* Interrupt Handler
 */

/**
 * int_hndlr - interrupt handler
 *
 */
static void int_hndlr (int irq, void *dev_id, struct pt_regs *regs)
{
        int usiro;
        int udccr;
        int ep;

        int_oscr = OSCR;
	udc_interrupts++;

#if defined(USE_DMA_IN) || defined(USE_DMA_OUT)
        pxa_kill();
#endif /* defined(USE_DMA_IN) || defined(USE_DMA_OUT) */

        // check for common, high priority interrupts first, i.e. per endpoint service requests
        // XXX if ISO supported it might be necessary to give the ISO endpoints priority
        
        while ((usiro = UDCISR0)) {
                u32 udccs0 = UDCCSR0;
                PXA_IRO(usiro, "------------------------> Interrupt");
                usiro |= (0x3 << 2);
                for (ep = 0; usiro; usiro >>= 2, ep++) {
                        if (usiro & 3) {
                                switch (ep_maps[ep].eptype) {
                                case ep_control:
                                        pxa_ep0(ep_endpoints[0], udccs0);
                                        //PXA_IRO(USIRO, "<-- Interrupt");
                                        break;

                                case ep_bulk_in:
                                case ep_interrupt:
                                        pxa_in_n(ep, ep_endpoints[ep]);
                                        break;

                                case ep_bulk_out:
#ifndef USE_DMA_OUT                     
                                        pxa_out_n(ep, ep_endpoints[ep]);
                                        /* Try to Read from the Second Buffer */
                                        if(UDCCSRN(ep) & UDCCS_FS) {
                                            pxa_out_n(ep, ep_endpoints[ep]);
                                        }
                                        break;
#endif /* USE_DMA_OUT */
                                case ep_iso_in:
                                case ep_iso_out:
                                        pxa_ep_reset_irs(ep);
                                        break;
                                }
                        }
                }
        }
        // sof interrupt
        if (UDCISR1 & UDCISR1_IRSOF) {
                UDCISR1 = UDCISR1_IRSOF;
        }

        // uncommon interrupts
        if ((udccr = UDCISR1) & (UDCISR1_IRRS | UDCISR1_IRRU | UDCISR1_IRSU | UDCISR1_IRCC)) {
        char config_num = 0;
                // Configuration Change
                if (udccr & UDCISR1_IRCC) {
                    unsigned char *cp = (unsigned char *)&ep0_urb->device_request;
                    unsigned char set_config[8] = {0x00, 0x09, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};

#if defined(CONFIG_ARCH_SHARP_SL) && defined(CONFIG_APM_CPU_IDLE)
                    pxa_lockFCS(LOCK_FCS_UDC, 1);
#endif
                    /* Switch Emdpoint Memory to Active Configuration */
                    UDCCR = (UDCCR & ~0x08) | 0x10;
                    /* Clear Interrupt Status Register */
                    UDCISR1 |= UDCISR1_IRCC;
                    /* Read Active UDC Configuration Number */
                    config_num = ((UDCCR >> 11) & 0x3);
                    /* Set Configuration Number to Device Request */
                    set_config[2] = config_num;
                    memcpy(cp, set_config, sizeof(set_config));
                    usbd_device_event (udc_device, DEVICE_BUS_ACTIVITY, 0);
                    // process setup packet
                    usbd_recv_setup(ep0_urb);
                }

                // UDC Reset
                if (udccr & UDCISR1_IRRS) {
                        PXA_CCR(udccr, "------------------------> Reset");
                        //printk(KERN_INFO"int_hndlr[%d:%d] Reset\n", udc_interrupts, jifs());
                                        
                        udc_suspended = 0;
                        usbd_device_event (udc_device, DEVICE_RESET, 0);
                        usbd_device_event (udc_device, DEVICE_ADDRESS_ASSIGNED, 0);	
                        UDCISR1 |= UDCISR1_IRRS;
                }

                // UDC Resume
                if (udccr & UDCISR1_IRRU) {
                        PXA_CCR(udccr, "------------------------> Resume");
                        if (udc_suspended) {
                                udc_suspended = 0;
                                usbd_device_event (udc_device, DEVICE_BUS_ACTIVITY, 0);
                        }
                        UDCISR1 |= UDCISR1_IRRU;
                }

                // UDC Suspend
                if (udccr & UDCISR1_IRSU) {
                        PXA_CCR(udccr, "------------------------> Suspend");
                        if (!udc_suspended) {
#if defined(CONFIG_ARCH_SHARP_SL) && defined(CONFIG_APM_CPU_IDLE)
                    pxa_lockFCS(LOCK_FCS_UDC, 0);
#endif
                                udc_suspended = 1;
                                usbd_device_event (udc_device, DEVICE_BUS_INACTIVE, 0);
                        }
                        UDCISR1 |= UDCISR1_IRSU;
                }
        }
        PXA_CCR(UDCCR, "<-- Interrupt");
}


/* ********************************************************************************************* */


/* ********************************************************************************************* */
/*
 * Start of public functions.
 */

/**
 * udc_start_in_irq - start transmit
 * @eendpoint: endpoint instance
 *
 * Called by bus interface driver to see if we need to start a data transmission.
 */
void udc_start_in_irq (struct usb_endpoint_instance *endpoint)
{
        if (UDCCSRN(endpoint->endpoint_address & 0xf) & UDCCS_FS) {
                pxa_start_n (endpoint->endpoint_address & 0xf, endpoint);
        }
}

/**
 * udc_init - initialize
 *
 * Return non-zero if we cannot see device.
 **/
int udc_init (void)
{
    int i;
    struct udccr_cfg* p;

    udc_disable_interrupts (NULL);

    /* Set Endpoint Configuration Regiser */
    p = udccr_cfgs;
    for(i=1; i<=sizeof(udccr_cfgs) / sizeof(struct udccr_cfg); i++){
         UDCCRN(i) = *(u32 *)p;
         p++;
    }
	return 0;
}


/**
 * udc_start_in - start transmit
 * @eendpoint: endpoint instance
 *
 * Called by bus interface driver to see if we need to start a data transmission.
 */
void udc_start_in (struct usb_endpoint_instance *endpoint)
{
	if (endpoint) {
		unsigned long flags;
		local_irq_save (flags);
                udc_start_in_irq(endpoint);
		local_irq_restore (flags);
	}
}


/**
 * udc_stall_ep - stall endpoint
 * @ep: physical endpoint
 *
 * Stall the endpoint.
 */
void udc_stall_ep (unsigned int ep)
{
	if (ep < UDC_MAX_ENDPOINTS) {
		// stall
	}
}


/**
 * udc_reset_ep - reset endpoint
 * @ep: physical endpoint
 * reset the endpoint.
 *
 * returns : 0 if ok, -1 otherwise
 */
void udc_reset_ep (unsigned int ep)
{
	if (ep < UDC_MAX_ENDPOINTS) {
	}
}


/**
 * udc_endpoint_halted - is endpoint halted
 * @ep:
 *
 * Return non-zero if endpoint is halted
 */
int udc_endpoint_halted (unsigned int ep)
{
	return 0;
}


/**
 * udc_set_address - set the USB address for this device
 * @address:
 *
 * Called from control endpoint function after it decodes a set address setup packet.
 */
void udc_set_address (unsigned char address)
{
}

/**
 * udc_serial_init - set a serial number if available
 */
int __init udc_serial_init (struct usb_bus_instance *bus)
{
	return -EINVAL;
}

/* ********************************************************************************************* */

/**
 * udc_max_endpoints - max physical endpoints 
 *
 * Return number of physical endpoints.
 */
int udc_max_endpoints (void)
{
	return UDC_MAX_ENDPOINTS;
}


/**
 * udc_check_ep - check logical endpoint 
 * @lep:
 *
 * Return physical endpoint number to use for this logical endpoint or zero if not valid.
 */
int udc_check_ep (int logical_endpoint, int packetsize)
{
        // XXX check ep table

        return ( ((logical_endpoint & 0xf) >= UDC_MAX_ENDPOINTS) || (packetsize > 64)) 
                ?  0 : (logical_endpoint & 0xf);
}


/**
 * udc_set_ep - setup endpoint 
 * @ep:
 * @endpoint:
 *
 * Associate a physical endpoint with endpoint_instance
 */
void udc_setup_ep (struct usb_device_instance *device, unsigned int ep,
		   struct usb_endpoint_instance *endpoint)
{
	if (ep < UDC_MAX_ENDPOINTS) {

		ep_endpoints[ep] = endpoint;

		// ep0
		if (ep == 0) {
		}
		// IN
		else if (endpoint->endpoint_address & 0x80) {
#ifdef USE_DMA_IN
                        usb_stream_in.ep = ep;
                        usb_stream_in.endpoint = endpoint;
                        usb_stream_in.drcmr = ep_maps[ep].drcmr;
                        usb_stream_in.dev_addr = UDCDRN(ep);
                        for (i = 0; i < usb_stream_in.num; i++) {
                                usb_stream_in.dma_desc[i].dsadr = usb_stream_in.dma_buf_phys + (i * usb_stream_in.size);
                                usb_stream_in.dma_desc[i].dtadr = ep_maps[ep].dev_addr;
                                usb_stream_in.dma_desc[i].dcmd = usb_stream_in.size;
                        }
#endif /* USE_DMA_IN */
		}
		// OUT
		else if (endpoint->endpoint_address) {


			usbd_fill_rcv (device, endpoint, recvpacket);
			endpoint->rcv_urb = first_urb_detached (&endpoint->rdy);
#ifdef USE_DMA_OUT
                        //printk(KERN_INFO"udc_setup_ep: OUT alloc urbs ep: %d addr: %d num: %d size: %d dma_ch: %d\n",
                        //                ep, endpoint->endpoint_address, usb_stream_out.num, 
                        //                usb_stream_out.size, usb_stream_out.dma_ch);
                        usb_stream_out.ep = ep;
                        usb_stream_out.endpoint = endpoint;
                        usb_stream_out.drcmr = ep_maps[ep].drcmr;
                        usb_stream_out.dev_addr = _UDCDRN[ep];
                        for (i = 0; i < usb_stream_out.num; i++) {

                                usb_stream_out.dma_desc[i].ddadr = usb_stream_out.dma_desc_phys + 
                                        ((i < (usb_stream_out.num - 1)) ? (i + 1) * sizeof(pxa_dma_desc) : 0);

                                usb_stream_out.dma_desc[i].dsadr = usb_stream_out.dev_addr;
                                usb_stream_out.dma_desc[i].dtadr = usb_stream_out.dma_buf_phys + (i * usb_stream_out.size);
                                usb_stream_out.dma_desc[i].dcmd = usb_stream_out.size | usb_stream_out.dcmd;

                                //printk(KERN_INFO"udc_setup_ep: dma_desc[%d] ddadr: %x dsadr: %x dtadr: %x dcmd: %x\n",
                                //                i,
                                //                usb_stream_out.dma_desc[i].ddadr,
                                //                usb_stream_out.dma_desc[i].dsadr,
                                //                usb_stream_out.dma_desc[i].dtadr,
                                //                usb_stream_out.dma_desc[i].dcmd
                                //                );
                        }
#if 1
                        DDADR(usb_stream_out.dma_ch) = usb_stream_out.buffers[0].dma_desc->ddadr;
                        //printk(KERN_INFO"udc_setup_ep: DDAR: %p %p\n", 
                        //                DDADR(usb_stream_out.dma_ch), usb_stream_out.buffers[0].dma_desc->ddadr);
                        pxa_start_out_dma(ep, endpoint, &usb_stream_out);
#endif
#endif /* USE_DMA_OUT */
		}
                pxa_enable_ep_interrupt(ep_endpoints[ep]->endpoint_address);
	}
}

/**
 * udc_disable_ep - disable endpoint
 * @ep:
 *
 * Disable specified endpoint 
 */
void udc_disable_ep (unsigned int ep)
{
	if (ep < UDC_MAX_ENDPOINTS) {
		struct usb_endpoint_instance *endpoint;

		if ((endpoint = ep_endpoints[ep])) {
			ep_endpoints[ep] = NULL;
			usbd_flush_ep (endpoint);
		}
		if (ep == 0) {
                        //printk(KERN_INFO"udc_setup_ep: 0 do nothing\n");
		}
		// IN
		else if (endpoint->endpoint_address & 0x80) {
#ifdef USE_DMA_IN
                        usb_stream_in.ep = 0;
                        usb_stream_in.endpoint = NULL;
#endif /* USE_DMA_IN */
		}
		// OUT
		else if (endpoint->endpoint_address) {
#if 0
#ifdef USE_DMA_OUT
                        usb_stream_out.ep = 0;
                        usb_stream_out.endpoint = NULL;
#endif
#endif
		}
	}
}

/* ********************************************************************************************* */

/**
 * udc_connected - is the USB cable connected
 *
 * Return non-zeron if cable is connected.
 */
int udc_connected ()
{
int rc = 0;

    udelay(100);
    /* Host Cable not connected ? Host is not active ? Device is active ? */
    if ( (GPLR1 & GPIO_bit(USB_CONNECT_GPIO)) &&
         (GPLR1 & GPIO_bit(USBD_VBUS_GPIO_DEVICE)) && 
         !(GPLR1 & GPIO_bit(USBD_VBUS_GPIO_HOST)) ) {
        rc = 1;
    } else {                      
        rc = 0;
    }
    return rc;
}

/**
 * udc_connect - enable pullup resistor
 *
 * Turn on the USB connection by enabling the pullup resistor.
 */
void udc_connect (void)
{
    /* Host Cable not connected ? */
    if ( !(GPLR1 & GPIO_bit(USB_CONNECT_GPIO))) {
        printk("udc_connect: host cable connected. \n");
        return;
    }
    /* Host is not active ? */
    if ( (GPLR1 & GPIO_bit(USBD_VBUS_GPIO_HOST)) ) {
        printk("udc_connect: usb-host active. \n");
        return;
    }

    /* Device is active ? */
    if ( !(GPLR1 & GPIO_bit(USBD_VBUS_GPIO_DEVICE)) ) {
        printk("udc_connect: usb-device is not active. \n");
        return;
    }

    // Device Clock Enable Register
    CKEN |= CKEN11_USB;

    /* Init USB Port 2 Output Control Register */
    UP2OCR = 0x00;

    /* Host Port 2 Transceiver Output Enable */
    UP2OCR |= UP2OCR_HXOE;

    /* D+ Pull Up SW1 ON */
    UP2OCR |= UP2OCR_DPPUE;

    /* Clear PTG Peripheral Control Hold */
    if( PSSR & PSSR_OTGPH ){
        PSSR |= PSSR_OTGPH;
    }
}

/**
 * udc_disconnect - disable pullup resistor
 *
 * Turn off the USB connection by disabling the pullup resistor.
 */
void udc_disconnect (void)
{
	// c.f. 3.6.2 Clock Enable Register
	CKEN &= ~CKEN11_USB;

    if ( (GPLR1 & GPIO_bit(USB_CONNECT_GPIO)) &&
         !(GPLR1 & GPIO_bit(USBD_VBUS_GPIO_HOST)) ) {
        /* Init USB Port 2 Output Control Register */
        UP2OCR = 0x00;
    }

#if defined(CONFIG_ARCH_SHARP_SL) && defined(CONFIG_APM_CPU_IDLE)
    pxa_lockFCS(LOCK_FCS_UDC, 0);
#endif

}

/**
 * udc_int_hndlr_cable - interrupt handler for cable
 */
static void udc_int_hndlr_cable (int irq, void *dev_id, struct pt_regs *regs)
{
/* USBD? */
    if(GPLR1 & GPIO_bit(USB_CONNECT_GPIO)) {        
		// GPIOn interrupt
		//dbg_udc("udc_cradle_interrupt: ->%08x\n",GPLR(USBD_CABLE_GPIO));
	  if (udc_connected()) {
	    udc_connect();
	    usbd_device_event(udc_device, DEVICE_HUB_RESET, 0);
	    udc_cable_event ();
	  } else {
	    udc_disconnect();
	  }
    }
}

/* ********************************************************************************************* */

/**
 * udc_enable_interrupts - enable interrupts
 *
 * Switch on UDC interrupts.
 *
 */
void udc_all_interrupts (struct usb_device_instance *device)
{
        int i;

        UDCICR1 |= UDCICR1_IECC;    // enable configuration change interrupt
        UDCICR1 |= UDCICR1_IESU;    // enable suspend interrupt
        UDCICR1 |= UDCICR1_IERU;    // enable resume interrupt
        UDCICR1 &= ~UDCICR1_IERS;     // disable reset interrupt

        UDCISR1 = 0xffffffff;

        // XXX SOF UFNHR &= ~UFNHR_SIM;

        // always enable control endpoint
        pxa_enable_ep_interrupt(0);

        for (i = 1; i < UDC_MAX_ENDPOINTS; i++) {
                if (ep_endpoints[i] && ep_endpoints[i]->endpoint_address) {
                        pxa_enable_ep_interrupt(ep_endpoints[i]->endpoint_address);
                }
        }
}


/**
 * udc_suspended_interrupts - enable suspended interrupts
 *
 * Switch on only UDC resume interrupt.
 *
 */
void udc_suspended_interrupts (struct usb_device_instance *device)
{

        UDCICR1 |= UDCICR1_IERS;     // enable reset interrupt
        UDCICR1 &= ~UDCICR1_IESU;    // disable suspend interrupt
        UDCICR1 &= ~UDCICR1_IERU;    // disable resume interrupt
        UDCICR1 &= ~UDCICR1_IECC;    // disable configuration change interrupt

}


/**
 * udc_disable_interrupts - disable interrupts.
 *
 * switch off interrupts
 */
void udc_disable_interrupts (struct usb_device_instance *device)
{
        UDCICR0 = UDCICR1 = 0;                 // disable endpoint interrupts
}

/* ********************************************************************************************* */

/**
 * udc_ep0_packetsize - return ep0 packetsize
 */
int udc_ep0_packetsize (void)
{
	return EP0_PACKETSIZE;
}

/**
 * udc_enable - enable the UDC
 *
 * Switch on the UDC
 */
void udc_enable (struct usb_device_instance *device)
{
	// save the device structure pointer
	udc_device = device;

	// ep0 urb
	if (!ep0_urb) {
		if (!(ep0_urb = usbd_alloc_urb (device, device->function_instance_array, 0, 512))) {
			printk (KERN_ERR "udc_enable: usbd_alloc_urb failed\n");
		}
	} 
        else {
		printk (KERN_ERR "udc_enable: ep0_urb already allocated\n");
	}


	// enable UDC

#if 0
	// c.f. 3.6.2 Clock Enable Register
	CKEN |= CKEN11_USB;
#endif

        // c.f. 12.4.11 GPIOn and GPIOx
        // enable cable interrupt
        

        // c.f. 12.5 UDC Operation - after reset on EP0 interrupt is enabled.

        //UDC Enable
        UDCCR |= UDCCR_UDE;
        // Rest Interrupt disable
        UDCICR1 &= ~UDCICR1_IERS;
        
}


/**
 * udc_disable - disable the UDC
 *
 * Switch off the UDC
 */
void udc_disable (void)
{
        //UDC disnable
        UDCCR &= ~UDCCR_UDE;
        // Rest Interrupt eable
        UDCICR1 |= UDCICR1_IERS;

	// c.f. 3.6.2 Clock Enable Register
	CKEN &= ~CKEN11_USB;

    if (!(GPLR1 & GPIO_bit(USBD_VBUS_GPIO_HOST)) ) {
        /* Init USB Port 2 Output Control Register */
        UP2OCR = 0x00;
    }

        // disable cable interrupt
        
	// reset device pointer
	udc_device = NULL;

	// ep0 urb

	if (ep0_urb) {
		usbd_dealloc_urb (ep0_urb);
		ep0_urb = 0;
	} else {
		printk (KERN_ERR "udc_disable: ep0_urb already NULL\n");
	}

#if defined(CONFIG_ARCH_SHARP_SL) && defined(CONFIG_APM_CPU_IDLE)
    pxa_lockFCS(LOCK_FCS_UDC, 0);
#endif
}


/**
 * udc_startup - allow udc code to do any additional startup
 */
void udc_startup_events (struct usb_device_instance *device)
{
	usbd_device_event (device, DEVICE_INIT, 0);
	usbd_device_event (device, DEVICE_CREATE, 0);
	usbd_device_event (device, DEVICE_HUB_CONFIGURED, 0);
	usbd_device_event (device, DEVICE_RESET, 0);
}


/* ********************************************************************************************* */
#ifdef PXA_TRACE
#ifdef CONFIG_USBD_PROCFS
/* Proc Filesystem *************************************************************************** */
        
/* *    
 * pxa_proc_read - implement proc file system read.
 * @file        
 * @buf         
 * @count
 * @pos 
 *      
 * Standard proc file system read function.
 */         
static ssize_t pxa_proc_read (struct file *file, char *buf, size_t count, loff_t * pos)
{                                  
        unsigned long page;
        int len = 0;
        int index;

        MOD_INC_USE_COUNT;
        // get a page, max 4095 bytes of data...
        if (!(page = get_free_page (GFP_KERNEL))) {
                MOD_DEC_USE_COUNT;
                return -ENOMEM;
        }

        len = 0;
        index = (*pos)++;

        if (index == 0) {
                len += sprintf ((char *) page + len, "   Index     Ints     Jifs    Ticks\n");
        }       
                         

        if (index < trace_next) {

                u64 jifs = 1111;
                u32 ticks = 2222;
                pxa_trace_t *p = pxa_traces + index;
                unsigned char *cp;

                if (index > 0) {
                        u32 ocsr = pxa_traces[index-1].ocsr;
                        ticks = (p->ocsr > ocsr) ? (p->ocsr - ocsr) : (ocsr - p->ocsr) ;
                        jifs = p->jiffies - pxa_traces[index-1].jiffies;
                }
                
                len += sprintf ((char *) page + len, "%8d %8d %8lu ", index, p->interrupts, jifs);

                if (ticks > 1024*1024) {
                        len += sprintf ((char *) page + len, "%8dM ", ticks>>20);
                }
                else {
                        len += sprintf ((char *) page + len, "%8d  ", ticks);
                }
                switch (p->trace_type) {
                case pxa_regs:
                        len += sprintf ((char *) page + len, "CSR0[%02x] %s\n", p->trace.regs.cs0, p->trace.regs.msg);
                        break;
                case pxa_setup:
                        cp = (unsigned char *)&p->trace.setup;
                        len += sprintf ((char *) page + len, 
                                        " --               request [%02x %02x %02x %02x %02x %02x %02x %02x]\n", 
                                        cp[0], cp[1], cp[2], cp[3], cp[4], cp[5], cp[6], cp[7]);
                        break;
                case pxa_xmit:
                        len += sprintf ((char *) page + len, 
                                        " --                   sending [%02x]\n", p->trace.xmit.size);
                        break;
                case pxa_in:
                        len += sprintf ((char *) page + len,                 
                                        "CRS[%03x]             in [%02x]\n", p->trace.inout.crs, p->trace.inout.size);
                        break;
                case pxa_out:
                        len += sprintf ((char *) page + len, 
                                        "CRS[%03x]             out [%02x]\n", p->trace.inout.crs, p->trace.inout.size);
                        break;
                case pxa_ccr:
                        len += sprintf ((char *) page + len, 
                                        "CR  [%02x] %s\n", p->trace.ccr.ccr, p->trace.ccr.msg);
                        break;
                case pxa_iro:
                        len += sprintf ((char *) page + len, 
                                        "ISR0[%02x] %s\n", p->trace.iro.iro, p->trace.iro.msg);
                        break;
                }
        }
        if (len > count) {
                len = -EINVAL;
        } else if (len > 0 && copy_to_user (buf, (char *) page, len)) {
                len = -EFAULT;
        }
        free_page (page);
        MOD_DEC_USE_COUNT;
        return len;
}
/* *
 * pxa_proc_write - implement proc file system write.
 * @file
 * @buf
 * @count
 * @pos
 *
 * Proc file system write function, used to signal monitor actions complete.
 * (Hotplug script (or whatever) writes to the file to signal the completion
 * of the script.)  An ugly hack.
 */
static ssize_t pxa_proc_write (struct file *file, const char *buf, size_t count, loff_t * pos)
{
        return count;
}

static struct file_operations pxa_proc_operations_functions = {
        read:pxa_proc_read,
        write:pxa_proc_write,
};

#endif
#endif


/* ********************************************************************************************* */
/**
 * udc_name - return name of USB Device Controller
 */
char *udc_name (void)
{
	return UDC_NAME;
}

/**
 * udc_request_udc_irq - request UDC interrupt
 *
 * Return non-zero if not successful.
 */
int udc_request_udc_irq ()
{
	// request IRQ  and IO region
	if (request_irq (IRQ_USB, int_hndlr, SA_INTERRUPT | SA_SAMPLE_RANDOM,
	     UDC_NAME " USBD Bus Interface", NULL) != 0) 
        {
		printk (KERN_INFO "usb_ctl: Couldn't request USB irq\n");
		return -EINVAL;
	}
	return 0;
}

/**
 * udc_request_cable_irq - request Cable interrupt
 *
 * Return non-zero if not successful.
 */
int udc_request_cable_irq ()
{
	set_GPIO_mode(USBD_VBUS_GPIO_DEVICE | GPIO_IN);
	set_GPIO_IRQ_edge(USBD_VBUS_GPIO_DEVICE, GPIO_BOTH_EDGES);
	// request IRQ  and IO region
	if (request_irq (IRQ_GPIO(USBD_VBUS_GPIO_DEVICE), udc_int_hndlr_cable,
		SA_INTERRUPT | SA_SAMPLE_RANDOM, UDC_NAME " Cable", NULL) != 0)
	{
		printk (KERN_INFO "usb_ctl: Couldn't request USB cable irq\n");
		return -EINVAL;
	}
	return 0;
}

#if defined(USE_DMA_IN) || defined(USE_DMA_OUT)


void pxa_free_dma_stream(usb_stream_t *usb_stream)
{
        printk(KERN_INFO"pxa_free_dma_stream:\n");
        if (usb_stream->dma_ch >= 0) {
                printk(KERN_INFO"pxa_free_dma_stream: pxa_free_dma\n");
                pxa_free_dma(usb_stream->dma_ch);
                usb_stream->dma_ch = -1;
        }
#if 1
        printk(KERN_INFO"pxa_free_dma_stream: dma_buf: %p\n", usb_stream->dma_buf);
        if (usb_stream->dma_buf) {
                printk(KERN_INFO"pxa_free_dma_stream: consistent_free dma_buf\n");
                consistent_free(usb_stream->dma_buf, usb_stream->num * usb_stream->size, usb_stream->dma_buf_phys);
                usb_stream->dma_buf = NULL;
        }

        printk(KERN_INFO"pxa_free_dma_stream: dma_desc: %p\n", usb_stream->dma_desc);
        if (usb_stream->dma_desc) {
                printk(KERN_INFO"pxa_free_dma_stream: consistent_free dma_desc\n");
                consistent_free(usb_stream->dma_desc, usb_stream->num * sizeof(pxa_dma_desc), usb_stream->dma_desc_phys);
                usb_stream->dma_desc = NULL;
        }

        printk(KERN_INFO"pxa_free_dma_stream: buffers: %p\n", usb_stream->buffers);
        if (usb_stream->buffers) {
                printk(KERN_INFO"pxa_free_dma_stream: kfree buffers\n");
                kfree(usb_stream->buffers);
                usb_stream->buffers = NULL;
        }
#endif
        printk(KERN_INFO"pxa_free_dma_stream: done\n");
}


int pxa_setup_dma(/*int ep, struct usb_endpoint_instance *endpoint,*/ usb_stream_t *usb_stream)
{
	usb_buf_t *buffers;

        pxa_dma_desc *dma_desc = NULL;
        dma_addr_t dma_desc_phys = 0;

        unsigned char *dma_buf = NULL;
        dma_addr_t dma_buf_phys = 0;

        int i;
        int num;
        int size;

        usb_stream->num = num = 4;
        usb_stream->size = size = 64;

        // DMA buffer structure array
        if (!(buffers = kmalloc(sizeof(usb_buf_t) * usb_stream->num, GFP_KERNEL))) {
                goto err;
        }
        memset(buffers, 0, sizeof(usb_buf_t) * usb_stream->num);
        usb_stream->buffers = buffers;


        // DMA Buffer
        if (!(dma_buf = consistent_alloc(GFP_KERNEL, num * size, &dma_buf_phys))) {
                goto err;
        }
        usb_stream->dma_buf = dma_buf;
        usb_stream->dma_buf_phys = dma_buf_phys;


        // DMA descriptor array
        if (!(dma_desc = consistent_alloc(GFP_KERNEL, num * sizeof(pxa_dma_desc), &dma_desc_phys))) {
                goto err;
        }
        usb_stream->dma_desc = dma_desc;
        usb_stream->dma_desc_phys = dma_desc_phys;

        printk(KERN_INFO"dma stream dma_buf: %p dma_buf_phys: %p dma_desc: %p dma_desc_phys: %p\n",
                        usb_stream->dma_buf,
                        usb_stream->dma_buf_phys,
                        usb_stream->dma_desc,
                        usb_stream->dma_desc_phys
              );
        // setup arrays
        //
        for (i = 0; i < num; i++) {

                // usb_stream->dma_desc[i]

                //dma_desc[i].ddadr = dma_desc_phys + (i < (num - 1)) ?  (i + 1) * sizeof(pxa_dma_desc) : 0;

                // usb_stream->buffers[i]
                buffers[i].dma_buf = dma_buf + (i * size);
                buffers[i].dma_buf_phys = dma_buf_phys + (i * size);
                buffers[i].dma_desc = dma_desc + (i * sizeof(pxa_dma_desc));
                buffers[i].dma_desc_phys = dma_desc_phys + (i * sizeof(pxa_dma_desc));

                printk(KERN_INFO"dma buf[%d] dma_buf: %p dma_buf_phys: %p dma_desc: %p dma_desc_phys: %p\n",
                                i,
                                buffers[i].dma_buf,
                                buffers[i].dma_buf_phys,
                                buffers[i].dma_desc,
                                buffers[i].dma_desc_phys
                                );
        }

        // prime DMA

        //DDADR(usb_stream->dma_ch) = buffers[0].dma_desc->ddadr;

        return 0;
err:
        return -ENOMEM;
}
/**
 * udc_request_udc_io - request UDC io region
 *
 * Return non-zero if not successful.
 */
int udc_request_io ()
{
#ifdef USE_DMA_IN
        // XXX verify prio
        if ((usb_stream_in.dma_ch = pxa_request_dma(usb_stream_in.name, 
                                        DMA_PRIO_HIGH, pxa_usb_in_dma_irq, &usb_stream_in)) < 0) 
        {
                printk(KERN_INFO"pxa: failed to allocate IN dma channel\n");
                return -EINVAL;
        }
        if (!pxa_setup_dma(&usb_stream_in)) {
                pxa_free_dma_stream(&usb_stream_in);
        }
        printk(KERN_INFO"pxa: DMA channels in: %d\n", usb_stream_in.dma_ch);
#endif /* USE_DMA_IN */

#ifdef USE_DMA_OUT
        if ((usb_stream_out.dma_ch = pxa_request_dma(usb_stream_out.name, 
                                        DMA_PRIO_HIGH, pxa_usb_out_dma_irq, &usb_stream_out)) < 0) 
        {
                printk(KERN_INFO"pxa: failed to allocate OUT dma channel\n");
#ifdef USE_DMA_IN
                pxa_free_dma_stream(&usb_stream_in);
#endif /* USE_DMA_IN */
                return -EINVAL;
        }
        if (!pxa_setup_dma(&usb_stream_out)) {
#ifdef USE_DMA_IN
                pxa_free_dma_stream(&usb_stream_in);
#endif /* USE_DMA_IN */
                pxa_free_dma_stream(&usb_stream_out);
                return -EINVAL;
        }

        printk(KERN_INFO"pxa: DMA channels out: %d\n", usb_stream_out.dma_ch);
#endif /* USE_DMA_OUT */

	return 0;
}

/**
 * udc_release_release_io - release UDC io region
 */
void udc_release_io ()
{

#ifdef USE_DMA_IN
        pxa_free_dma_stream(&usb_stream_in);
#endif /* USE_DMA_IN */
#ifdef USE_DMA_OUT
        pxa_free_dma_stream(&usb_stream_out);
#endif /* USE_DMA_OUT */
}

#else /* defined(USE_DMA_IN) || defined(USE_DMA_OUT) */

/**
 * udc_request_udc_io - request UDC io region
 *
 * Return non-zero if not successful.
 */
int udc_request_io ()
{
#ifdef PXA_TRACE
#ifdef CONFIG_USBD_PROCFS
        if (!(pxa_traces = vmalloc(sizeof(pxa_trace_t) * TRACE_MAX))) {
                printk(KERN_ERR"PXA_TRACE malloc failed %p %d\n", pxa_traces, sizeof(pxa_trace_t) * TRACE_MAX);
        }
        else {
                printk(KERN_ERR"PXA_TRACE malloc ok %p\n", pxa_traces);
        }
        PXA_REGS(0,"init");
        PXA_REGS(0,"test");
        {
                struct proc_dir_entry *p;

                // create proc filesystem entries
                if ((p = create_proc_entry ("pxa", 0, 0)) == NULL) {
                        printk(KERN_INFO"PXA PROC FS failed\n");
                }
                else {
                        printk(KERN_INFO"PXA PROC FS Created\n");
                        p->proc_fops = &pxa_proc_operations_functions;
                }
        }
#endif
#endif
	return 0;
}

/**
 * udc_release_release_io - release UDC io region
 */
void udc_release_io ()
{
#ifdef PXA_TRACE
#ifdef CONFIG_USBD_PROCFS
        unsigned long flags;
        save_flags_cli (flags);
        remove_proc_entry ("pxa", NULL);
        if (pxa_traces) {
                pxa_trace_t *p = pxa_traces;
                pxa_traces = 0;
                vfree(p);
        }
        restore_flags (flags);
#endif
#endif

}

#endif /* defined(USE_DMA_IN) || defined(USE_DMA_OUT) */


/**
 * udc_release_udc_irq - release UDC irq
 */
void udc_release_udc_irq ()
{
	free_irq (IRQ_USB, NULL);
}

/**
 * udc_release_cable_irq - release Cable irq
 */
void udc_release_cable_irq ()
{
#ifdef XXXX_IRQ
	free_irq (XXXX_CABLE_IRQ, NULL);
#endif
	free_irq (IRQ_GPIO(USBD_VBUS_GPIO_DEVICE), NULL);
}


/**
 * udc_regs - dump registers
 *
 * Dump registers with printk
 */
void udc_regs (void)
{
	printk ("[%d:%d] CCR[%02x] UICR[%02x %02x] UFNH[%02x] UDCCS[%02x %02x %02x %02x]\n", 

                        udc_interrupts, jifs(), UDCCR, UDCICR1, UDCICR0, UDCFNR, UDCCSR0, UDCCSR_A, UDCCSR_B, UDCCSR_E);
}

/**
 * monitor_lockFCS - 
 *
 * 
 */
static void pxa_lockFCS (unsigned long lock_index, int action)
{

        if( action == lockFCS_status && action){
		/* No need to do it again... */
		return;
	}

	lockFCS_status = action;
	lock_FCS(LOCK_FCS_UDC, action);
	return;
}
