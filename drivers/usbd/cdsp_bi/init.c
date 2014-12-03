/*
 * linux/drivers/usbd/cdsp_bi/init.c - USB device CDSP bus interface
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

#ifdef MODULE
#include <linux/module.h>
#else
#error MODULE not defined
#endif
#include <linux/config.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/list.h>
#include <asm/uaccess.h>
#include <asm/atomic.h>
#include <linux/netdevice.h>
#include <linux/skbuff.h>
#include <linux/etherdevice.h>
#include <net/arp.h>
#include <linux/rtnetlink.h>
#include <linux/smp_lock.h>
#include <linux/ctype.h>
#include <linux/timer.h>
#include <linux/string.h>
#include <linux/atmdev.h>
#include <linux/pkt_sched.h>
#include <linux/delay.h>
#include <asm/sead/sead.h>
#include <asm/sead/seadint.h>
#include <asm/avalanche/avalanche_regs.h>

#define VERSION "2001-05-04"

#include "../usbd-debug.h"
#include "../usbd.h"
#include "../usbd-func.h"
#include "../usbd-bus.h"
#include "../usbd-inline.h"
#include "cdsp.h"
#include "cdsp_u2v.h"
#include "cdsp_proc.h"

#define MAX_DEVICES 2

#define          AV_PRCR           0x0
#define              PRCR_USB         (0x1<<8)
#define          AV_SWRCR          0x4
#define          AV_RSR            0x8

#define  AV_INTR_BASE SEAD_ICTRL_REGS_BASE
#define          AVI_INTSR1       0x00
#define          AVI_INTSR2       0x04
#define          AVI_INTCR1       0x10
#define          AVI_INTCR2       0x14
#define          AVI_INTER1       0x20
#define          AVI_INTER2       0x24


/* Module Parameters ************************************************************************* */

char *dbg = NULL;
int rstie = 0;
int frste = 1;

MODULE_AUTHOR("sl@lineo.com, tbr@lineo.com");
MODULE_DESCRIPTION("USB device CDSP bus interface");
MODULE_PARM(dbg, "s");
MODULE_PARM(rstie, "i"); // USB RSTR interrupt enabled if > 0 (problematic on SEAD)
MODULE_PARM(frste, "i"); // (USB reset => CDSP module reset) enabled 4.1.5 of USB_CDSP Module doc

/* Debug switches (module parameter "dbg=...") *********************************************** */

int      dbgflg_init = 0;
int      dbgflg_intr = 0;
int      dbgflg_tick = 0;
int      dbgflg_usbe = 0;
int      dbgflg_rstr = 0;
int      dbgflg_setup = 0;

static debug_option dbg_table[] = {
    {&dbgflg_init,NULL,"init","initialization and termination"},
    {&dbgflg_intr,NULL,"intr","interrupt handling"},
    {&dbgflg_tick,NULL,"tick","interrupt status monitoring on clock tick"},
    {&dbgflg_usbe,NULL,"usbe","USB events"},
    {&dbgflg_rstr,NULL,"rstr","USB RSTR interrupt handling"},
    {&dbgflg_setup,NULL,"setup","Setup packet handling"},
    {NULL,NULL,NULL,NULL}
};

    /* Debug stuff */
    /* status monitor on clock tick... */
    // struct tasklet_struct clk_task;
static struct tq_struct clk_tsk;
static u32 old_USB_status = 0;
static u32 old_AVA_status = 0;
// static u32 old_USB_vect = 0;
static u32 tick_count = 0;
static u32 repeat_count = 0;
static u32 interrupt_count = 0;

static u32 st_count[8];
static u32 st_freq[8] = { 1, 1, 1, 1, 1, 1, 1, 1};

/* Globals *********************************************************************************** */

// struct usb_bus_instance *bus;   // this should be a list
struct usb_device_instance *device_array[MAX_DEVICES];

LIST_HEAD(urbs);

struct urb *urb_list;

/* VBus register I/O ************************************************************************* */

static void reg32write(u32 addr, u32 data)
{
    *((u32 *)(void *)(addr)) = data;
}

static u32 reg32read(u32 addr)
{
    return(*((u32 *)(void *)(addr)));
}


/* usb-busintf.c ***************************************************************************** */

#define DMA_FIFO_SIZE 2048
#define USB_BUF_SIZE    64

typedef struct host_intr_info {
    u32 word0;
    u32 word1;
} host_intr_info;

typedef struct dma_fifo {
    u32 pointers;
    u8 octet[DMA_FIFO_SIZE];
} dma_fifo;

typedef struct bi_data {
    struct usb_bus_instance *bus;
    u32 mcu_base;
    u32 usb_base;
    u32 desired_ui_mask;
    struct sead_ictrl_regs *sead_hw0_icregs;
    host_intr_info hx_intr_data;
    dma_fifo tx_fifo;
    dma_fifo rx_fifo;
} bi_data;

enum intf_intr {
    usb_attach,
    usb_power,
    usb_setup,
    usb_reset,
    usb_suspend,
    usb_resume,
    usb_sof,
    usb_rx,
    usb_tx
};

enum transfer_result {
    TRANSFER_ACK_NODATA,
    TRANSFER_ACK_DATA,
    TRANSFER_STALL,
    TRANSFER_NAK
};

/* Low level chip operations ***************************************************************** */

/**
 * cdsp_connect - enable upstream port
 * @bus - pointer to usb_bus_instance
 */
void cdsp_connect(struct usb_bus_instance *bus)
{
    /* This "turns on" the current draw that enables an
       upstream port to sense when the device is plugged
       in.  As of 20010108, the SEAD board had this hard
       wired "on". */
    u32 oval,nval;
    struct bi_data *data;
    if (!bus || NULL == (data = bus->privdata)) {
        return;
    }
    oval = reg32read(data->mcu_base+USB_Control);
    nval = (oval & ~UC_Rmt_Wkup) | UC_Connect;
    reg32write(data->mcu_base+USB_Control,nval);
    if (dbgflg_init >= 1) {
        nval = reg32read(data->mcu_base+USB_Control);
        dbgPRINT(dbgflg_init,1,"USB_Control#%02x->#%02x(-#%02x+#%02x)",
                 oval,nval,UC_Rmt_Wkup,UC_Connect);
    }
}

/**
 * cdsp_disconnect - disable upstream port
 * @bus - pointer to usb_bus_instance
 */
void cdsp_disconnect(struct usb_bus_instance *bus)
{
    u32 oval,nval;
    struct bi_data *data;
    if (!bus || NULL == (data = bus->privdata)) {
        return;
    }
    oval = reg32read(data->mcu_base+USB_Control);
    nval = oval & ~(UC_Rmt_Wkup | UC_Connect);
    reg32write(data->mcu_base+USB_Control,nval);
    nval = reg32read(data->mcu_base+USB_Control);
    if (dbgflg_init >= 1) {
        nval = reg32read(data->mcu_base+USB_Control);
        dbgPRINT(dbgflg_init,1,"USB_Control#%02x->#%02x(-#%02x)",
                 oval,nval,(UC_Rmt_Wkup|UC_Connect));
    }
}

/**
 * cdsp_enable_usb_interrupts - enable CDSP USB interrupts
 * @bus - pointer to usb_bus_instance
 * @allow_reset - leave RSTR disabled if FALSE
 * Note: this does not enable/disable the Avalanche/SEAD interrupts,
 *       it operates _only_ on the CDSP registers.
 */
void cdsp_enable_usb_interrupts(
        struct usb_bus_instance *bus,
        int allow_reset)
{
    u32 oval,nval,dval;
    struct bi_data *bd;
    if (!bus || NULL == (bd = bus->privdata)) {
        return;
    }
    /* Enable USB interrupts. */
    // oval = reg32read(bd->mcu_base+USB_Interrupt_Mask);
    oval = bd->desired_ui_mask;
    dval = UI_STPOW_Enable | UI_Setup_Enable |
           UI_RESR_Enable | UI_SUSR_Enable;
    if (allow_reset) {
        dval |= UI_RSTR_Enable;
    }
    reg32write(bd->mcu_base+USB_Interrupt_Mask,(oval|dval));
    bd->desired_ui_mask = oval | dval;
    if (dbgflg_intr >= 1 /* && repeat_count <= 2 */) {
        /* Don't chatter in an interrupt repeat loop. */
        nval = reg32read(bd->mcu_base+USB_Interrupt_Mask);
        dbgPRINT(dbgflg_intr,1,"UI_Mask#%02x->#%02x(+#%02x)",
                 oval,nval,dval);
    }
}

/**
 * cdsp_disable_usb_interrupts - disable CDSP USB interrupts
 * @bus - pointer to usb_bus_instance
 * @allow_resume - leave RESR enabled if TRUE
 * Note: this does not enable/disable the Avalanche/SEAD interrupts,
 *       it operates _only_ on the CDSP registers.
 */
void cdsp_disable_usb_interrupts(
        struct usb_bus_instance *bus,
        int allow_resume)
{
    u32 oval,nval,dval,cval;
    struct bi_data *bd;
    if (!bus || NULL == (bd = bus->privdata)) {
        return;
    }
    /* Disable USB interrupts. */
    // oval = reg32read(bd->mcu_base+USB_Interrupt_Mask);
    oval = bd->desired_ui_mask;
    dval = UI_STPOW_Enable | UI_Setup_Enable |
           UI_SUSR_Enable | UI_RSTR_Enable;
    if (allow_resume) {
        /* Make sure RESR _is_ enabled. */
        cval = UI_RESR_Enable;
    } else {
        dval |= UI_RESR_Enable;
        cval = 0;
    }
    nval = (oval & ~dval) | cval;
    reg32write(bd->mcu_base+USB_Interrupt_Mask,nval);
    bd->desired_ui_mask = nval;
    if (dbgflg_intr >= 1 /* && repeat_count <= 2 */) {
        nval = reg32read(bd->mcu_base+USB_Interrupt_Mask);
        dbgPRINT(dbgflg_intr,1,"UI_Mask#%02x->#%02x(-#%02x+#%02x)",
                 oval,nval,dval,cval);
    }
}

/**
 * cdsp_enable_usb_reset - enable CDSP USB internal function reset
 * @bus - pointer to usb_bus_instance
 * Note: this allows a (bus generated) function reset to
 *       reset (most of) the CDSP module.
 */
void cdsp_enable_usb_reset(struct usb_bus_instance *bus)
{
    u32 oval,nval;
    struct bi_data *data;
    if (!bus || NULL == (data = bus->privdata) || !frste) {
        return;
    }
    oval = reg32read(data->mcu_base+USB_Control);
    nval = oval | UC_Fn_Reset_Conn;
    reg32write(data->mcu_base+USB_Control,nval);
    if (dbgflg_rstr >= 1) {
        nval = reg32read(data->mcu_base+USB_Control);
        dbgPRINT(dbgflg_rstr,1,"USB_Control#%02x->#%02x(+#%02x)",
                 oval,nval,UC_Fn_Reset_Conn);
    }
}

/**
 * cdsp_disable_usb_reset - disable CDSP USB internal function reset
 * @bus - pointer to usb_bus_instance
 * Note: this stops a (bus generated) function reset from
 *       reseting (most of) the CDSP module.
 */
void cdsp_disable_usb_reset(struct usb_bus_instance *bus)
{
    u32 oval,nval;
    struct bi_data *data;
    if (!bus || NULL == (data = bus->privdata)) {
        return;
    }
    oval = reg32read(data->mcu_base+USB_Control);
    nval = oval & ~UC_Fn_Reset_Conn;
    reg32write(data->mcu_base+USB_Control,nval);
    if (dbgflg_rstr >= 1) {
        nval = reg32read(data->mcu_base+USB_Control);
        dbgPRINT(dbgflg_rstr,1,"USB_Control#%02x->#%02x(-#%02x)",
                 oval,nval,UC_Fn_Reset_Conn);
    }
}

static void cdsp_endpoint_disable(cdsp_endpoint_descriptor *ep)
{
    ep->config = 0;
    ep->x_buffer_base = 0;
    ep->x_buffer_byte_count = 0;
    ep->y_buffer_base = 0;
    ep->y_buffer_byte_count = 0;
    ep->xy_buffer_size = 0;
}

int cdsp_do_reset(bi_data *bd)
{
    /* Try to reset the CDSP module. */
    u32 oval,nval;
    if (5 <= dbgflg_rstr) {
        /* Have a look at the Reset Status Register */
        oval = reg32read(RESET_CTRL_BASE+AV_RSR);
        dbgPRINT(dbgflg_rstr,5,"RSR#%08x",oval);
    }
    /* Have a look at the Peripheral Reset Control Register */
    oval = reg32read(RESET_CTRL_BASE+AV_PRCR);
    /* Reset USB */
    nval = oval & ~PRCR_USB;
    reg32write(RESET_CTRL_BASE+AV_PRCR,nval);
    udelay(1);
    reg32write(RESET_CTRL_BASE+AV_PRCR,PRCR_USB);
    nval = reg32read(RESET_CTRL_BASE+AV_PRCR);
    dbgPRINT(dbgflg_rstr,3,"RESET PRCR#%08x->#%08x",oval,nval);
    if ((nval & PRCR_USB) != PRCR_USB) {
        printk(KERN_ERR "cdsp_bi: could not reset CDSP.\n");
        return(-ENODEV);
    }
    return(0);
}

void cdsp_set_ep_defaults(bi_data *bd)
{
    int n;
    u32 val;
    cdsp_endpoint_descriptor *ep;
    /* Configure EndPoint 0 for In and Out */
    val = EC_USB_Intr_Enable | EC_UBM_Enable;
    reg32write(bd->mcu_base+InEP0_Config,val);
    reg32write(bd->mcu_base+OutEP0_Config,val);
    val = 0;
    reg32write(bd->mcu_base+InEP0_Byte_Count,val);
    reg32write(bd->mcu_base+OutEP0_Byte_Count,val);
    /* Configure EndPoints 1-7 */
    /* EP1-7 are disabled */
    for (n = 1; n <= 7; n++) {
        ep = (cdsp_endpoint_descriptor *)(void *)(bd->mcu_base+InEP_Descrip(n));
        cdsp_endpoint_disable(ep);
        ep = (cdsp_endpoint_descriptor *)(void *)(bd->mcu_base+OutEP_Descrip(n));
        cdsp_endpoint_disable(ep);
    }
}

/* Bus Interface Callback Functions ********************************************************** */

int cdsp_submit_urb(struct urb *tx_urb)
{
    DBGmark;
    list_add_tail(&tx_urb->urbs, &urbs);
    return 0;
}

int cdsp_cancel_urb(struct urb *tx_urb)
{
    DBGmark;
    return 0;
}

/**
 * cdsp_endpoint_halted - check if endpoint halted
 * @device: device
 * @endpoint: endpoint to check
 *
 * Used by the USB Device Core to check endpoint halt status.
 */
int cdsp_endpoint_halted(struct usb_device_instance *device, int endpoint)
{
    DBGmark;
    return 0;
}


/**
 * cdsp_device_feature - handle set/clear feature requests
 * @device: device
 * @endpoint: endpoint to operate on
 * @flag: stall (TRUE) or reset (FALSE)
 *
 * Used by the USB Device Core to stall or reset an endpoint.
 */
int cdsp_device_feature(struct usb_device_instance *device, int endpoint, int flag)
{
    dbgPRINT(dbgflg_init,1,"endpoint %d flag %d",endpoint,flag);
    if (flag) {
         // udc_stall_ep(endpoint);
    }
    else {
         // udc_reset_ep(endpoint);
    }
    return 0;
}


/**
 * cdsp_device_event - handle generic bus event
 * @device: device pointer
 * @num: interrupt event
 *
 * Called by usb core layer to inform bus of an event.
 */
int cdsp_device_event(struct usb_device_instance *device, usb_device_event_t event) 
{

    if (!device) {
        return 0;
    }
    switch (event) {
    case DEVICE_CREATE:
        dbgPRINT(dbgflg_usbe,1,"CREATE");
        // enable upstream port
        cdsp_enable_usb_interrupts(device->bus,rstie);
        cdsp_connect(device->bus);
        break;
    case DEVICE_DESTROY:
        dbgPRINT(dbgflg_usbe,1,"DESTROY");
        // disable upstream port
        cdsp_disconnect(device->bus);
        cdsp_disable_usb_interrupts(device->bus,0);
        break;
    case DEVICE_HUB_CONFIGURED:
        dbgPRINT(dbgflg_usbe,1,"HUB_CONFIGURED");
        break;
    case DEVICE_HUB_RESET:
        dbgPRINT(dbgflg_usbe,1,"HUB_RESET");
        break;
    case DEVICE_RESET:
        dbgPRINT(dbgflg_usbe,1,"DEVICE_RESET");
        break;
    case DEVICE_POWER_INTERRUPTION:
        dbgPRINT(dbgflg_usbe,1,"DEVICE_POWER_INTERRUPTION");
        break;

    case DEVICE_ADDRESS_ASSIGNED:
        dbgPRINT(dbgflg_usbe,1,"ADDRESS");
        // udc_set_address(device->address);
        break;

    case DEVICE_CONFIGURED:
        dbgPRINT(dbgflg_usbe,1,"DEVICE_CONFIGURED");
        break;
    case DEVICE_DE_CONFIGURED:
        dbgPRINT(dbgflg_usbe,1,"DEVICE_DE_CONFIGURED");
        break;

    case DEVICE_BUS_INACTIVE:
        dbgPRINT(dbgflg_usbe,1,"DEVICE_BUS_INACTIVE");
        cdsp_disable_usb_interrupts(device->bus,1);
        // udc_disable_interrupts(1);  1 ===> suspend interr only? QQQ
        break;

    case DEVICE_BUS_ACTIVITY:
        dbgPRINT(dbgflg_usbe,1,"DEVICE_BUS_ACTIVITY");
        cdsp_enable_usb_interrupts(device->bus,rstie);
        // udc_enable_interrupts(1);  1 ===> suspend interr only? QQQ
        break;

    case DEVICE_SET_INTERFACE:
        dbgPRINT(dbgflg_usbe,1,"DEVICE_SET_INTERFACE");
        break;
    case DEVICE_SET_FEATURE:
        dbgPRINT(dbgflg_usbe,1,"DEVICE_SET_FEATURE");
        break;
    case DEVICE_CLEAR_FEATURE:
        dbgPRINT(dbgflg_usbe,1,"DEVICE_CLEAR_FEATURE");
        break;
    default:
        dbgPRINT(dbgflg_init,1,"unknown event %d",event);
    }

    return 0;
}


struct usb_bus_operations cdsp_bi_ops = {
    send_urb: cdsp_submit_urb,
    cancel_urb: cdsp_cancel_urb,
    endpoint_halted: cdsp_endpoint_halted,
    device_feature: cdsp_device_feature,
    device_event: cdsp_device_event
};

struct usb_bus_driver bi_driver = {
    name: "CDSP Bus Driver cdsp0",
    max_endpoints: 4,
    maxpacketsize: 8,
    ops: &cdsp_bi_ops
};

/* Bus Interface Support Functions *********************************************************** */



/**
 * cdsp_do_event - generate a usb device event
 * @device: device number
 * @event: event to generate
 */
void cdsp_do_event(int device, usb_device_event_t event)
{
    if (device < 0 || device >= MAX_DEVICES || NULL == device_array[device]) {
        return;
    }
    usb_device_event(device_array[device], event);

}

/* usb-busintf.c ***************************************************************************** */

struct bi_data * bi_alloc(char *name)
{
    struct bi_data *data;
    // allocate a bi private data structure
    if ((data = kmalloc(sizeof(struct bi_data),GFP_KERNEL)) == NULL) {
        return NULL;
    }
    memset(data, 0, sizeof(struct bi_data));
    return data;
}

void bi_dealloc(struct bi_data *data)
{
    kfree(data);
}

/* usb-busintf.c ***************************************************************************** */

/* bi_power_on - respond to power on event
 * @bus - pointer to usb_bus_instance
 */
void bi_power_on(struct usb_bus_instance *bus)
{
    if (!bus) {
        return;
    }
    // create a device
    if ((bus->device = usb_register_device(NULL, bus, 8))==NULL) {
        // failed - disconnect
        cdsp_disconnect(bus);
    }
}

/* bi_power_off - respond to power off event
 * @bus - pointer to usb_bus_instance
 */
void bi_power_off(struct usb_bus_instance *bus)
{
    if (!bus) {
        return;
    }
    // destroy the device
    if (bus->device) {
        struct usb_device_instance *device = bus->device;
        bus->device = NULL;
        usb_deregister_device(device);
    }
}

/* bi_stall - put endpoint into stall
 * @bus - pointer to usb_bus_instance
 * @urb
 */
void bi_stall(struct usb_bus_instance *bus, struct urb *urb)
{
    dbgPRINT(dbgflg_usbe,1," ");
    if (!bus) {
        return;
    }
}

/* bi_ack - send an ack
 * @bus - pointer to usb_bus_instance
 * @urb
 */
void bi_ack(struct usb_bus_instance *bus, struct urb *urb)
{
    dbgPRINT(dbgflg_usbe,1," ");
    if (!bus) {
        return;
    }
}

/* bi_result - send results and ack
 * @bus - pointer to usb_bus_instance
 * @urb
 */
void bi_result(struct usb_bus_instance *bus, struct urb *urb)
{
    dbgPRINT(dbgflg_usbe,1," ");
    if (!bus) {
        return;
    }
    //bi_submit_urb(urb);
    return;
}

/* CDSP specific stuff *********************************************************************** */

static char *dbg_iname[48] = {
    "DONE", "v#02", "v#04", "v#06", "v#08", "v#0A", "v#0C", "v#0E",
    "v#10", "OEP1", "OEP2", "OEP3", "OEP4", "OEP5", "OEP6", "OEP7",
    "v#20", "IEP1", "IEP2", "IEP3", "IEP4", "IEP5", "IEP6", "IEP7",
    "SUOW", "SETP", "v#34", "v#36", "RESR", "SUSR", "RSTR", "v#3E",
    "v#40", "v#42", "IEP0", "OEP0", "v#48", "v#4A", "v#4C", "v#4E",
    "TxDM", "RxDM", "HxDM", "v#56", "v#58", "v#5A", "rstr", "unkn"
};
#define VI_rstr 0x5C
#define VI_unkn 0x5E
static char tox[] = {"0123456789ABCDEF"};

static u32 dbg_uicount[48],
           dbg_uitotal[48];
static char imsg_buff[256];

static u32 stuck_disabled = 0;
static u32 prev_USB_status = 0;

static char setup[8];
#ifdef MAYBE
static char iep0[8];
static char oep0[8];
#endif

static void do_setup_pkt(bi_data *bd)
{
   u32 *up;
   char *cp,*ce;
   up = (u32 *) (void *) (bd->mcu_base + Setup_Packet);
   for (ce = 8 + (cp = setup); cp < ce;) {
       *cp++ = 0xff & *up++;
   }
   dbgPRINTmem(dbgflg_setup,1,setup,8);
#ifdef MAYBE
   up = (u32 *) (void *) (bd->mcu_base + InEP0_Buffer);
   for (ce = 8 + (cp = iep0); cp < ce;) {
       *cp++ = 0xff & *up++;
   }
   dbgPRINTmem(dbgflg_setup,1,iep0,8);
   up = (u32 *) (void *) (bd->mcu_base + OutEP0_Buffer);
   for (ce = 8 + (cp = oep0); cp < ce;) {
       *cp++ = 0xff & *up++;
   }
   dbgPRINTmem(dbgflg_setup,1,oep0,8);
#endif
}

static void cdsp_interrupt(int x, void *vp, struct pt_regs *regs)
{
    bi_data *bd = (bi_data *) vp;
    u32 ust,ost,uim,sreg,ireg,val,icount,oval,tcount,
        bailing_out,repeating;
    // struct sead_ictrl_regs *sead_hw0_icregs = (struct sead_ictrl_regs *) AV_INTR_BASE;
    // dbgPRINT(dbgflg_init,1,"x=%d vp#%08x",x,(u32)vp);
    // /* Read the Avalanche interrupt status register. */
    // val = reg32read(AV_INTR_BASE+AVI_INTSR1);
    // dbgPRINT(dbgflg_init,1,"x=%d vp#%08x val#%08x",x,(u32)vp,val);
    // Dispatch routine may have already cleared this...
    // reg32write(AV_INTR_BASE+AVI_INTCR1,val);
    /* Read the queued interrupts from the priority vector. */
    ireg = bd->mcu_base + Vector_Intr;
    sreg = bd->mcu_base + USB_Status;
    bailing_out = repeating = 0;
    interrupt_count += 1;
    ost = ust = reg32read(sreg);
    if (prev_USB_status != ost) {
        repeating = 0;
        repeat_count = 0;
    } else {
        /* We may be getting hit over and over with a single
           interrupt (e.g. RSTR), disable it if we are. */
        switch (ost) {
        case US_RSTR:
            uim = reg32read(bd->mcu_base+USB_Interrupt_Mask) & ~UI_RSTR_Enable;
            reg32write(bd->mcu_base+USB_Interrupt_Mask,uim);
            bd->desired_ui_mask &= ~UI_RSTR_Enable;
            stuck_disabled |= UI_RSTR_Enable;
            repeating = 1;
            dbgPRINT(dbgflg_intr,1,"repeating RSTR => UI_Mask#%02x",
                     reg32read(bd->mcu_base+USB_Interrupt_Mask));
            break;
        case US_SUSR:
            uim = reg32read(bd->mcu_base+USB_Interrupt_Mask) & ~UI_SUSR_Enable;
            reg32write(bd->mcu_base+USB_Interrupt_Mask,uim);
            bd->desired_ui_mask &= ~UI_SUSR_Enable;
            stuck_disabled |= UI_SUSR_Enable;
            repeating = 1;
            dbgPRINT(dbgflg_intr,1,"repeating SUSR => UI_Mask#%02x",
                     reg32read(bd->mcu_base+USB_Interrupt_Mask));
            break;
        case US_RESR:
            uim = reg32read(bd->mcu_base+USB_Interrupt_Mask) & ~UI_RESR_Enable;
            reg32write(bd->mcu_base+USB_Interrupt_Mask,uim);
            bd->desired_ui_mask &= ~UI_RESR_Enable;
            stuck_disabled |= UI_RESR_Enable;
            repeating = 1;
            dbgPRINT(dbgflg_intr,1,"repeating RESR => UI_Mask#%02x",
                     reg32read(bd->mcu_base+USB_Interrupt_Mask));
            break;
        // Multiple Setups in a row seems reasonable....
        //case US_Setup_Rcvd:
        //case US_STPOW:
        default:
            repeating = 0;
            repeat_count = 0;
        }
        if (1000 <= (repeat_count += repeating)) {
            repeat_count = 1;
        }
    }
    if (0 == stuck_disabled) {
        oval = VI_Done;
    } else if (stuck_disabled & UI_RSTR_Enable) {
        oval = VI_RSTR;
    } else if (stuck_disabled & UI_SUSR_Enable) {
        oval = VI_SUSR;
    } else if (stuck_disabled & UI_RESR_Enable) {
        oval = VI_RESR;
    } else if (stuck_disabled & UI_Setup_Enable) {
        oval = VI_SETUP;
    } else if (stuck_disabled & UI_STPOW_Enable) {
        oval = VI_STPOW;
    } else {
        oval = VI_Done;
    }
    icount = 0;
    do {
        tcount = 1;
        if (bailing_out) {
            /* Looks like a stuck interrupt, pick values from
               the status registers. */
            if (ust & US_RSTR) {
                val = VI_RSTR;
            } else if (ust & US_SUSR) {
                val = VI_SUSR;
            } else if (ust & US_RESR) {
                val = VI_RESR;
            } else if (ust & US_Setup_Rcvd) {
                val = VI_SETUP;
            } else if (ust & US_STPOW) {
                val = VI_STPOW;
            } else {
                // FUTURE: examine Endpoint status registers...
                val = VI_Done;
                continue;
            }
        } else {
            /* Read next value from the Vector_Intr register. */
            if (VI_Done == (val = reg32read(ireg))) {
                /* Register is empty, we're done. */
                continue;
            }
            ust |= reg32read(sreg);
            /* Look for values "stuck" in the Vector_Intr register. */
            if (val != oval) {
                /* A new interrupt, the way it's sposed to be. */
                /* Clear the interrupt in the priority vector. */
                reg32write(ireg,0xff);
                /* Enable any "stuck" interrupts disabled. */
                if (0 != stuck_disabled && !repeating) {
                   uim = bd->desired_ui_mask | stuck_disabled;
                   //uim = reg32read(bd->mcu_base+USB_Interrupt_Mask) | stuck_disabled;
                   reg32write(bd->mcu_base+USB_Interrupt_Mask,uim);
                   bd->desired_ui_mask = uim;
                   stuck_disabled = 0;
                }
            } else {
                /* A repeated interrupt, try harder to clear it. */
                /* Disable if possible. */
                switch (val) {
                case VI_STPOW:
                    //uim = reg32read(bd->mcu_base+USB_Interrupt_Mask) & ~UI_STPOW_Enable;
                    uim = bd->desired_ui_mask & ~UI_STPOW_Enable;
                    reg32write(bd->mcu_base+USB_Interrupt_Mask,uim);
                    bd->desired_ui_mask = uim;
                    stuck_disabled |= UI_STPOW_Enable;
                    dbgPRINT(dbgflg_intr,1,"stuck STPOW => UI_Mask#%02x",
                             reg32read(bd->mcu_base+USB_Interrupt_Mask));
                    break;
                case VI_SETUP:
                    //uim = reg32read(bd->mcu_base+USB_Interrupt_Mask) & ~UI_Setup_Enable;
                    uim = bd->desired_ui_mask & ~UI_Setup_Enable;
                    reg32write(bd->mcu_base+USB_Interrupt_Mask,uim);
                    bd->desired_ui_mask = uim;
                    stuck_disabled |= UI_Setup_Enable;
                    dbgPRINT(dbgflg_intr,1,"stuck SETUP => UI_Mask#%02x",
                             reg32read(bd->mcu_base+USB_Interrupt_Mask));
                    break;
                case VI_RESR:
                    //uim = reg32read(bd->mcu_base+USB_Interrupt_Mask) & ~UI_RESR_Enable;
                    uim = bd->desired_ui_mask & ~UI_RESR_Enable;
                    reg32write(bd->mcu_base+USB_Interrupt_Mask,uim);
                    bd->desired_ui_mask = uim;
                    stuck_disabled |= UI_RESR_Enable;
                    dbgPRINT(dbgflg_intr,1,"stuck RESR => UI_Mask#%02x",
                             reg32read(bd->mcu_base+USB_Interrupt_Mask));
                    break;
                case VI_SUSR:
                    //uim = reg32read(bd->mcu_base+USB_Interrupt_Mask) & ~UI_SUSR_Enable;
                    uim = bd->desired_ui_mask & ~UI_SUSR_Enable;
                    reg32write(bd->mcu_base+USB_Interrupt_Mask,uim);
                    bd->desired_ui_mask = uim;
                    stuck_disabled |= UI_SUSR_Enable;
                    dbgPRINT(dbgflg_intr,1,"stuck SUSR => UI_Mask#%02x",
                             reg32read(bd->mcu_base+USB_Interrupt_Mask));
                    break;
                case VI_RSTR:
                    //uim = reg32read(bd->mcu_base+USB_Interrupt_Mask) & ~UI_RSTR_Enable;
                    uim = bd->desired_ui_mask & ~UI_RSTR_Enable;
                    reg32write(bd->mcu_base+USB_Interrupt_Mask,uim);
                    bd->desired_ui_mask = uim;
                    stuck_disabled |= UI_RSTR_Enable;
                    dbgPRINT(dbgflg_intr,1,"stuck RSTR => UI_Mask#%02x",
                             reg32read(bd->mcu_base+USB_Interrupt_Mask));
                    break;
                default:
                    // Yet another kinda stuck interrupt?
                    dbgPRINT(dbgflg_intr,1,"Stuck on #%02x!",val);
                }
                /* Read and clear 'til we're blue in the face... */
                do {
                    reg32write(ireg,0xff);
                    udelay(1);
                } while (++tcount < 1024 && oval == (val = reg32read(ireg)));
                /* Ok, we're blue, did the interrupt clear? */
                if (VI_Done == val) {
                    /* Got it. */
                    continue;
                }
                if (oval == val) {
                    /* Gah.  Really stuck.  Service anything in the status
                       registers, then bail. */
                    bailing_out = 1;
                    continue;
                }
            }
        }
        oval = val;
        icount += 1;
        /* See what we have. */
        switch (val) {
        case VI_OE1:
            dbgPRINT(dbgflg_intr,5,"OE1");
            break;
        case VI_OE2:
            dbgPRINT(dbgflg_intr,5,"OE2");
            break;
        case VI_OE3:
            dbgPRINT(dbgflg_intr,5,"OE3");
            break;
        case VI_OE4:
            dbgPRINT(dbgflg_intr,5,"OE4");
            break;
        case VI_OE5:
            dbgPRINT(dbgflg_intr,5,"OE5");
            break;
        case VI_OE6:
            dbgPRINT(dbgflg_intr,5,"OE6");
            break;
        case VI_OE7:
            dbgPRINT(dbgflg_intr,5,"OE7");
            break;
        case VI_IE1:
            dbgPRINT(dbgflg_intr,5,"IE1");
            break;
        case VI_IE2:
            dbgPRINT(dbgflg_intr,5,"IE2");
            break;
        case VI_IE3:
            dbgPRINT(dbgflg_intr,5,"IE3");
            break;
        case VI_IE4:
            dbgPRINT(dbgflg_intr,5,"IE4");
            break;
        case VI_IE5:
            dbgPRINT(dbgflg_intr,5,"IE5");
            break;
        case VI_IE6:
            dbgPRINT(dbgflg_intr,5,"IE6");
            break;
        case VI_IE7:
            dbgPRINT(dbgflg_intr,5,"IE7");
            break;
        case VI_STPOW:
            dbgPRINT(dbgflg_intr,5,"STPOW");
            /* Clear status bit. */
            reg32write(bd->mcu_base+USB_Status,US_STPOW);
            /* Remove cleared bit from status mask. */
            ust &= ~US_STPOW;
            break;
        case VI_SETUP:
            dbgPRINT(dbgflg_intr,5,"SETUP");
            /* Clear status bit. */
            reg32write(bd->mcu_base+USB_Status,US_Setup_Rcvd);
            /* Remove cleared bit from status mask. */
            ust &= ~US_Setup_Rcvd;
            /* Handle setup packet. */
            do_setup_pkt(bd);
            break;
        case VI_RESR:
            dbgPRINT(dbgflg_intr,5,"RESR");
            /* Clear status bit. */
            reg32write(bd->mcu_base+USB_Status,US_RESR);
            /* Remove cleared bit from status mask. */
            ust &= ~US_RESR;
            break;
        case VI_SUSR:
            dbgPRINT(dbgflg_intr,5,"SUSR");
            /* Clear status bit. */
            reg32write(bd->mcu_base+USB_Status,US_SUSR);
            /* Remove cleared bit from status mask. */
            ust &= ~US_SUSR;
            break;
        case VI_RSTR:
            dbgPRINT(dbgflg_intr,5,"RSTR");
            /* Clear status bit. */
            reg32write(bd->mcu_base+USB_Status,US_RSTR);
            /* Remove cleared bit from status mask. */
            ust &= ~US_RSTR;
            /* Reset the USB module. */
            if (dbgflg_rstr >= 3) {
                (void) cdsp_do_reset(bd); // Turns off pull-up from cdsp_connect(), and
                                          // turning it back on gives another RSTR.... :(.
                cdsp_set_ep_defaults(bd);
                cdsp_enable_usb_interrupts(bd->bus,0);
            } else {
                cdsp_set_ep_defaults(bd);
                cdsp_enable_usb_interrupts(bd->bus,0);
            }
            break;
        case VI_IE0:
            dbgPRINT(dbgflg_intr,5,"IE0");
            break;
        case VI_OE0:
            dbgPRINT(dbgflg_intr,5,"OE0");
            break;
        case VI_TxDMA:
            dbgPRINT(dbgflg_intr,5,"TxDMA");
            break;
        case VI_RxDMA:
            dbgPRINT(dbgflg_intr,5,"RxDMA");
            break;
        case VI_HxDMA:
            dbgPRINT(dbgflg_intr,5,"HxDMA");
            break;
        default:
            dbgPRINT(dbgflg_intr,1,"unknown interrupt vector #%02x",val);
            if (val > VI_unkn) {
                val = VI_unkn;
            }
        }
        dbg_uicount[val>>1] += tcount;
    } while (icount < 26 && val != VI_Done);
    /* Sanity check, clear anything we missed. */
    if (0 != ust) {
        reg32write(bd->mcu_base+USB_Status,ust);
    }
    prev_USB_status = ost;
    if (1 <= dbgflg_intr) {
        /* Try to keep to one line of output. */
        char *mp = imsg_buff;
        char *me = mp + sizeof(imsg_buff) - 12;
        memcpy(mp,((0==x)?"tick":"REAL"),4);
        mp += 4;
        memcpy(mp," status#",8);
        mp += 8;
        *mp++ = tox[0xf & (ost >> 4)];
        *mp++ = tox[0xf & ost];
        if (0 != ust) {
            memcpy(mp," MISSED#",8);
            mp += 8;
            *mp++ = tox[0xf & (ust >> 4)];
            *mp++ = tox[0xf & ust];
        }
        for (x = 0; x < 48 && mp < me; x++) {
            if ((val = dbg_uicount[x]) > 0) {
                dbg_uicount[x] = 0;
                dbg_uitotal[x] += val;
                *mp++ = ' ';
                memcpy(mp,dbg_iname[x],4);
                mp += 4;
                if (1 < val) {
                    *mp++ = '(';
                    *mp++ = tox[0xf & (val >> 8)];
                    *mp++ = tox[0xf & (val >> 4)];
                    *mp++ = tox[val&0xf];
                    *mp++ = ')';
                }
            }
        }
        *mp++ = 0;
        if (!repeating || repeat_count <= 1) {
            dbgPRINT(dbgflg_intr,1,"%u %c D#%02x M#%02x %s",icount,(repeating?'R':'-'),
                     stuck_disabled,reg32read(bd->mcu_base+USB_Interrupt_Mask),imsg_buff);
        }
    }
    /* Development only - remove for real life :). */
    if (interrupt_count > 3000) {
        /* Probably have a stuck interrupt, bail. */
        dbgPRINT(dbgflg_intr,0,"Interrupt count too high for development SW, bailing out.");
        (void) cdsp_do_reset(bd);
    }
}

static void remove_cdsp_interrupt(bi_data *data)
{
    /* Disable USB interrupts. */
    cdsp_disable_usb_interrupts(data->bus,0);
    // Remove interrupt handler (disables interrupt)
    free_irq(LNXINTNUM(AVALANCHE_USB_INT),data);
}

static int install_cdsp_interrupt(bi_data *data)
{
    // Install interrupt handler (request_irq() enables interrupt)
    // struct sead_ictrl_regs *icregs = data->sead_hw0_icregs;
    unsigned long irq_flags = SA_INTERRUPT;
    //u32 b1, b2, a1, a2;
    u32 b3, b4;
    /* Enable USB interrupts. */
    cdsp_enable_usb_interrupts(data->bus,rstie);
#ifdef JIMK_INT_CTRLR
    dbgPRINT(dbgflg_init,1,"bi_data#%08x",(u32)(void*)data);
    b3 = JIMK_INT_STATUS;
    b4 = JIMK_INT_MASK;
#else
    //dbgPRINT(dbgflg_init,1,"INTERS1@%08x INTERS2@%08x",
    //       (AV_INTR_BASE+AVI_INTER1),(AV_INTR_BASE+AVI_INTER2));
    //b1 = reg32read(AV_INTR_BASE+AVI_INTER1); // inters1
    //b2 = reg32read(AV_INTR_BASE+AVI_INTER2); // inters2
    dbgPRINT(dbgflg_init,1,"inters1@%08x inters2@%08x bi_data#%08x",
             (u32)(void*)(&icregs->inters1),(u32)(void*)(&icregs->inters2),(u32)(void*)data);
    b3 = icregs->inters1;
    b4 = icregs->inters2;
#endif
    if (0 != request_irq(LNXINTNUM(AVALANCHE_USB_INT),cdsp_interrupt,
                         irq_flags,"CDSP",data)) {
        dbgPRINT(dbgflg_init,1,"could not register USB interrupt.");
        printk(KERN_ERR "cdsp_bi: could not register USB interrupt.\n");
        return(-ENODEV);
    }
    //a1 = reg32read(AV_INTR_BASE+AVI_INTER1); // inters1
    //a2 = reg32read(AV_INTR_BASE+AVI_INTER1); // inters2
    //dbgPRINT(dbgflg_init,1,"interrupt handler installed #%08x #%08x -> #%08x #%08x",b1,b2,a1,a2);
    // 2000 Dec 29 - Victor Wells indicated that some of the interrupt logic
    //               is missing (for space reasons) from the RTL on the SEAD board,
    //               so this will not show the correct values.
#ifdef JIMK_INT_CTRLR
    dbgPRINT(dbgflg_init,1,"interrupt handler installed s#%08x m#%08x -> #%08x #%08x",
             b3,b4,JIMK_INT_STATUS,JIMK_INT_MASK);
    b3 = read_32bit_cp0_register(CP0_STATUS);
    dbgPRINT(dbgflg_init,1,"MIPS CP0 status#%08x",b3);
    /* Fake an interrupt, to see if anything happens. */
    // 2000 Dec 29 - Victor Wells indicated that some of the interrupt logic
    //               is missing (for space reasons) from the RTL on the SEAD board,
    //               so this will not generate an interrupt or show the correct values.
    if (dbgflg_intr >= 5) {
        dbgPRINT(dbgflg_intr,1,"about to test interrupt s#%08x m#%08x",JIMK_INT_STATUS,JIMK_INT_MASK);
        JIMK_INT_STATUS = 0xffffffff;
        dbgPRINT(dbgflg_intr,1,"1st reading interrupt status and mask s#%08x m#%08x",
                 JIMK_INT_STATUS,JIMK_INT_MASK);
        JIMK_INT_STATUS = 0x1;
        dbgPRINT(dbgflg_intr,1,"2nd reading interrupt status and mask s#%08x m#%08x",
                 JIMK_INT_STATUS,JIMK_INT_MASK);
        for (b3 = 0;  b3 < 10;  b3++) {
             if (0 != JIMK_INT_STATUS) {
                 // Clear it.
                 JIMK_INT_STATUS = 0x1;
             } else {
                 break;
             }
        }
        dbgPRINT(dbgflg_intr,1,"3rd reading interrupt status and mask s#%08x m#%08x",
                 JIMK_INT_STATUS,JIMK_INT_MASK);
    }
#else
    dbgPRINT(dbgflg_init,5,"interrupt handler installed #%08x #%08x -> #%08lx #%08lx",
             b3,b4,icregs->inters1,icregs->inters2);

    /* Fake an interrupt, to see if anything happens. */
    // 2000 Dec 29 - Victor Wells indicated that some of the interrupt logic
    //               is missing (for space reasons) from the RTL on the SEAD board,
    //               so this will not generate an interrupt or show the correct values.
    dbgPRINT(dbgflg_intr,5,"about to test interrupt...");
    // icregs->intsr1 = (0x1 << AVALANCHE_USB_INT);
    icregs->intsr1 = 0xffffffff;
    dbgPRINT(dbgflg_intr,5,"about to read raw interrupt status...");
    dbgPRINT(dbgflg_intr,5,"raw interrupt status #%08lx",icregs->intcr1);
#endif
    return(0);
}

static int cdsp_init(bi_data *bd)
{
#ifdef FUTURE
    u32 val;
    cdsp_endpoint_descriptor *ep;
#endif
    int n;

    if (0 != (n = cdsp_do_reset(bd))) {
        return(n);
    }
    /* Enable internal reset. */
    cdsp_enable_usb_reset(bd->bus);
    /* Configure default endpoints. */
    cdsp_set_ep_defaults(bd);

#ifdef FUTURE
    data = bd;
    /* Set the USB registers */
    val = (u32)(void *) (&data->tx_fifo.pointers);
    reg32write(data->usb_base+TxFIFO_Pointers,val);
    val = (u32)(void *) (&data->tx_fifo.octet[0]);
    reg32write(data->usb_base+TxFIFO_Base,val);
    val = DMA_FIFO_SIZE / USB_BUF_SIZE;  /* length in buffers */
    reg32write(data->usb_base+TxFIFO_Length,val);
    /* Use interrupt mechanism when FIFO full. */
    reg32write(data->usb_base+TxFIFO_Poll_Rate,0);
    val = (u32)(void *) (&data->rx_fifo.pointers);
    reg32write(data->usb_base+RxFIFO_Pointers,val);
    val = (u32)(void *) (&data->rx_fifo.octet[0]);
    reg32write(data->usb_base+RxFIFO_Base,val);
    val = DMA_FIFO_SIZE / USB_BUF_SIZE;  /* length in buffers */
    reg32write(data->usb_base+RxFIFO_Length,val);
    /* Use interrupt mechanism when FIFO full. */
    reg32write(data->usb_base+RxFIFO_Poll_Rate,0);
    /* Set up Host Interrupt, but leave disabled. */
    val = (u32) (void *) (&data->hx_intr_data);
    reg32write(data->usb_base+Hx_Interrupt_Addr,val);
    //
    val = UCC_Stream | UCC_Tx_Enable | UCC_Rx_Enable;
    reg32write(data->usb_base+U2V_Ctrlr_Config,val);
    /* Set the MCU registers */
    val = U2V_DMA_Enable | 1; // EP1 is "Out"
    reg32write(data->mcu_base+U2V_TxDMA_Config,val);
    reg32write(data->mcu_base+U2V_TxDMA_Status,0);
    val = U2V_DMA_Enable | 2; // EP2 is "In"
    reg32write(data->mcu_base+U2V_RxDMA_Config,val);
    reg32write(data->mcu_base+U2V_RxDMA_Status,0);
    /* For now, disable Host I/F. */
    reg32write(data->mcu_base+U2V_HxDMA_Config_Read,0);
    reg32write(data->mcu_base+U2V_HxDMA_Config_Write,0);
    reg32write(data->mcu_base+U2V_HxDMA_Config_Intr,0);
    reg32write(data->mcu_base+U2V_HxDMA_Status,0);
    /* Configure EndPoints.
       Note that general USB device terminology refers to 
       "In" as from device to host, and
       "Out" as from host to device.
       (A host centric view of the world.) */
    /* Configure EndPoint 0 for In and Out */
    val = EC_USB_Intr_Enable | EC_UBM_Enable;
    reg32write(data->mcu_base+InEP0_Config,val);
    reg32write(data->mcu_base+OutEP0_Config,val);
    val = 0;
    reg32write(data->mcu_base+InEP0_Byte_Count,val);
    reg32write(data->mcu_base+OutEP0_Byte_Count,val);
    /* Configure EndPoints 1-7 */
    /* EP1 is bulk data received from Host by this device (ie "Out"). */
    ep = (cdsp_endpoint_descriptor *)(void *)(data->mcu_base+InEP_Descrip(1));
    endpoint_disable(ep);
    ep = (cdsp_endpoint_descriptor *)(void *)(data->mcu_base+OutEP_Descrip(1));
    ep->config = EC_USB_Intr_Enable | EC_DBuf_Enable | EC_UBM_Enable;
    ep->x_buffer_base = EB64A(0);
    ep->x_buffer_byte_count = 0;
    ep->y_buffer_base = EB64A(1);
    ep->y_buffer_byte_count = 0;
    ep->xy_buffer_size = USB_BUF_SIZE;
    /* EP2 is bulk data to be sent to Host by this device (ie "In"). */
    ep = (cdsp_endpoint_descriptor *)(void *)(data->mcu_base+InEP_Descrip(2));
    ep->config = EC_USB_Intr_Enable | EC_DBuf_Enable | EC_UBM_Enable;
    ep->x_buffer_base = EB64A(2);
    ep->x_buffer_byte_count = 0;
    ep->y_buffer_base = EB64A(3);
    ep->y_buffer_byte_count = 0;
    ep->xy_buffer_size = USB_BUF_SIZE;
    ep = (cdsp_endpoint_descriptor *)(void *)(data->mcu_base+OutEP_Descrip(2));
    endpoint_disable(ep);
    /* EP3-7 are disabled */
    for (n = 3; n <= 7; n++) {
        ep = (cdsp_endpoint_descriptor *)(void *)(data->mcu_base+InEP_Descrip(n));
        endpoint_disable(ep);
        ep = (cdsp_endpoint_descriptor *)(void *)(data->mcu_base+OutEP_Descrip(n));
        endpoint_disable(ep);
    }
#endif
    return(0);
}

//static void tick_tasklet(unsigned long data)
static void tick_tsklet(void *data)
{
    bi_data *bd = (bi_data *) (void *) data;
    u32 new_USB_status;
    u32 new_AVA_status;
    u32 st;
    int i,new;
    //u32 new_USB_vect;
    if (NULL == bd) {
        return;
    }
    if (3 <= dbgflg_tick) {
        if (old_AVA_status != (new_AVA_status = JIMK_INT_STATUS)) {
            st = read_32bit_cp0_register(CP0_STATUS);
            dbgPRINT(dbgflg_tick,1,"AVA_Status #%08x->#%08x msk#%08x cp0#%08x",
                     old_AVA_status,new_AVA_status,JIMK_INT_MASK,st);
            old_AVA_status = new_AVA_status;
            if (0 != new_AVA_status && dbgflg_tick >= 2) {
                // Clear the status, so we can see it changing...
                JIMK_INT_STATUS = new_AVA_status;
                new_AVA_status = JIMK_INT_STATUS;
                dbgPRINT(dbgflg_tick,1,"AVA_Status cleared ->#%08x",new_AVA_status);
                old_AVA_status = new_AVA_status;
            }
        }
    }
    if (1 <= dbgflg_tick &&
        old_USB_status != (new_USB_status = reg32read(bd->mcu_base+USB_Status))) {
        // Clear the interrupts
        // reg32write(bd->mcu_base+USB_Status,new_USB_status);
        st = new_USB_status;
        new = 0;
        for (i = 0; i < 8; i++) {
            if ((st & 0x1) && ++st_count[i] >= st_freq[i]) {
                new = 1;
                st_count[i] = 0;
            }
            st >>= 1;
        }
        if (new && 3 <= dbgflg_tick) {
            dbgPRINT(dbgflg_tick,1,"USB_Status #%02x->#%02x",old_USB_status,new_USB_status);
            /* extract the vectored interrupts */
            cdsp_interrupt(0, (void *) bd, NULL);
        }
        old_USB_status = new_USB_status;
        tick_count = 0;
    }
    if (frste) {
        /* Internal USB reset may have cleared interrupt mask, check and restore. */
        unsigned long flags;
        save_and_cli(flags); // MIPS specific, different name in x86
        if (bd->desired_ui_mask != reg32read(bd->mcu_base+USB_Interrupt_Mask)) {
            reg32write(bd->mcu_base+USB_Interrupt_Mask,bd->desired_ui_mask);
            if (frste > 2) {
                /* Fake an interrupt. */
                cdsp_interrupt(0, (void *) bd, NULL);
            }
        }
        restore_flags(flags);
    }
#if OLD
    if (old_USB_vect != (new_USB_vect = reg32read(bd->mcu_base+Vector_Intr))) {
        printk(KERN_INFO "USB_Vector #%02x->#%02x\n",old_USB_vect,new_USB_vect);
        old_USB_vect = new_USB_vect;
        tick_count = 0;
        if (old_USB_vect != 0) {
            reg32write(bd->mcu_base+Vector_Intr,0);
            old_USB_vect = 0;
        }
    }
#endif
    if (++tick_count >= 200) {
        if (3 <= dbgflg_tick) {
            printk(KERN_INFO "tick\n");
        }
        tick_count = 0;
    }
    queue_task(&clk_tsk,&tq_timer);
}

/* Module Init ******************************************************************************* */

/* bi_init - commission bus interface driver
 */
static int __init bi_init(void)
{
    struct usb_bus_instance *bus;
    struct usb_device_instance *device;
    struct bi_data *data;
    int rc;

    if (0 != scan_debug_options("cdsp_bi",dbg_table,dbg)) {
        return(-EINVAL);
    }

    printk(KERN_INFO "cdsp_bi: %s loading\n", VERSION);

    // allocate a bus structure

    if ((data = bi_alloc("test"))==NULL) {
        return(-ENOMEM);
    }
    data->mcu_base = USB_MCU_BASE;
    data->usb_base = USB_BASE;
    data->sead_hw0_icregs = (struct sead_ictrl_regs *) AV_INTR_BASE;

    // Setup tick_tsk.
    clk_tsk.next = NULL;
    clk_tsk.sync = 0;
    clk_tsk.routine = tick_tsklet;
    clk_tsk.data = data;

    // register with usb device layer
    if ((bus = usb_register_bus(&bi_driver))==NULL) {
        // deregister from usb device support
        bi_dealloc(data);
        return(-ENOMEM);  // QQQ check reasonable value?
    }
    bus->privdata = data;
    data->bus = bus;
    if ((device = usb_register_device(NULL, bus, 8))==NULL) {
        // failed - disconnect
        usb_deregister_bus(bus);
        bi_dealloc(data);
        return(-ENOMEM);  // QQQ check reasonable value?
    }
    bus->device = device;
    device_array[0] = device;

    // Initialize cdsp "chip" and install the interrupt handler.
    if (0 != (rc = cdsp_init(data)) ||
        0 != (rc = install_cdsp_interrupt(data))) {
        // de-register from usb device layer
        usb_deregister_bus(bus);
        // deregister from usb device support
        bi_dealloc(data);
        return(rc);
    }

    if (frste || dbgflg_tick > 0) {
        // Install tick_tsk.
        queue_task(&clk_tsk,&tq_timer);
    }

    usb_device_event(device, DEVICE_CREATE);
    usb_device_event(device, DEVICE_HUB_CONFIGURED);
    // Fake a RSTR
    usb_device_event(device, DEVICE_RESET);

    // enable upstream port, this will result in the host hub applying power and enumerating us
    cdsp_connect(bus);

    // XXX this may move to an interrupt
    // emulate power on - this will create a device instance
    // bi_power_on(bus);

    usb_proc_init();

    return 0;
}

/* bi_exit - decommission bus interface driver
 */
static void __exit bi_exit(void)
{
    struct usb_device_instance *device;
    struct usb_bus_instance *bus = NULL;
    struct bi_data *data = NULL;

    // remove proc filesystem entry
    usb_proc_term();

    // XXX should iterate across bus list

    // Disable tick_tsk()
    clk_tsk.data = NULL;

    if (NULL != (device = device_array[0]) && NULL != (bus = device->bus)) {
        data = bus->privdata;
    }
    // XXXX FUTURE - straighten out resource alloc/free

    // disable upstream port, this will result in the host disconnection from us
    cdsp_disconnect(bus);
    
    // XXX this may move to an interrupt
    // emulate power off, this will destroy device instance
    // bi_power_off(bus);

    usb_device_event(device, DEVICE_RESET);
    usb_device_event(device, DEVICE_POWER_INTERRUPTION);
    usb_device_event(device, DEVICE_HUB_RESET);
    usb_device_event(device, DEVICE_DESTROY);

    // Remove interrupt handler
    remove_cdsp_interrupt(data);

    device_array[0] = NULL;
    bus->device = NULL;
    bus->privdata = NULL;

    usb_deregister_device(device);

    // de-register from usb device layer
    usb_deregister_bus(bus);

    // deregister from usb device support
    bi_dealloc(data);

    printk(KERN_INFO "cdsp_bi: %s unloaded\n", VERSION);
}


module_init(bi_init);
module_exit(bi_exit);


