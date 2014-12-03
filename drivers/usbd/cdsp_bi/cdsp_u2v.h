/*
 * linux/drivers/usbd/cdsp_bi/cdsp_u2v.h - USB device CDSP bus interface
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

#ifndef CDSP_U2V_H
#define CDSP_U2V_H

/* Register offsets and bit masks for the USB CDSP U2V Sub-Module */

/* MCU Registers ----------------------------------------------------------- */
// The MCU registers are defined as offsets from bi_data->mcu_base
/* Note: all 5 config registers have the same bitmask definitions,
         and all 3 status registers have the same masks as well.  */

#define U2V_TxDMA_Config        0x1E80
#define U2V_RxDMA_Config        0x1EC0
#define U2V_HxDMA_Config_Read   0x1F00
#define U2V_HxDMA_Config_Write  0x1F04
#define U2V_HxDMA_Config_Intr   0x1F08

    #define U2V_DMA_Enable    0x80
    #define U2V_DMA_EP_MSK    0x07

#define U2V_TxDMA_Status        0x1E84
#define U2V_RxDMA_Status        0x1ED4
#define U2V_HxDMA_Status        0x1F0C

    #define U2V_DMA_Timeout   0x01
    #define U2V_DMA_TO_MSK    0xF0
    #define U2V_DMA_TO_none   0x00
    #define U2V_DMA_TO_15us   0x10
    #define U2V_DMA_TO_31us   0x20
    #define U2V_DMA_TO_62us   0x30
    #define U2V_DMA_TO_125us  0x40
    #define U2V_DMA_TO_250us  0x50
    #define U2V_DMA_TO_500us  0x60
    #define U2V_DMA_TO_750us  0x70
    #define U2V_DMA_TO_1_00ms 0x80
    #define U2V_DMA_TO_1_25us 0x90
    #define U2V_DMA_TO_1_50ms 0xA0
    #define U2V_DMA_TO_1_75ms 0xB0
    #define U2V_DMA_TO_2_00ms 0xC0
    #define U2V_DMA_TO_2_25ms 0xD0
    #define U2V_DMA_TO_2_50ms 0xE0
    #define U2V_DMA_TO_2_73ms 0xF0

/* VBus access through MCU
   (not used, VBus accessed directly through "VBus Registers" defined below) */
#define U2V_VB_Data_07_00        0x1F80
#define U2V_VB_Data_15_08        0x1F84
#define U2V_VB_Data_23_16        0x1F88
#define U2V_VB_Data_31_24        0x1F8C
#define U2V_VB_Addr_07_00        0x1F90
#define U2V_VB_Addr_15_08        0x1F94
#define U2V_VB_Addr_23_16        0x1F98
#define U2V_VB_Addr_31_24        0x1F9C
#define U2V_VB_Control           0x1FA0
    #define U2V_VB_RDY           0x80
    #define U2V_VB_REQ           0x40
    #define U2V_VB_READ          0x20
    #define U2V_VB_WENABLE_MSK   0x0F

/* USB Configuration Registers --------------------------------------------- */

#define Function_Addr            0x1FFC
    #define FA_Mask              0x7f
#define USB_Status               0x1FF8
    #define US_STPOW             0x01
    #define US_Setup_Rcvd        0x04
    #define US_RESR              0x20
    #define US_SUSR              0x40
    #define US_RSTR              0x80
#define USB_Interrupt_Mask       0x1FF4
    #define UI_STPOW_Enable      0x01
    #define UI_Setup_Enable      0x04
    #define UI_RESR_Enable       0x20
    #define UI_SUSR_Enable       0x40
    #define UI_RSTR_Enable       0x80
#define USB_Control              0x1FF0
    #define UC_Setup_Dir_IN      0x01
    #define UC_Setup_Int_Srvc    0x02
    #define UC_Fn_Reset_Conn     0x10
    #define UC_Rmt_Wkup          0x20
    #define UC_Fn_Enable         0x40
    #define UC_Connect           0x80

#define Output_Endpoint_Intr     0x1E50
#define Input_Endpoint_Intr      0x1E4C
#define Vector_Intr              0x1E48
    #define VI_Done                0x00
    #define VI_OE1                 0x12
    #define VI_OE2                 0x14
    #define VI_OE3                 0x16
    #define VI_OE4                 0x18
    #define VI_OE5                 0x1A
    #define VI_OE6                 0x1C
    #define VI_OE7                 0x1E
    #define VI_IE1                 0x22
    #define VI_IE2                 0x24
    #define VI_IE3                 0x26
    #define VI_IE4                 0x28
    #define VI_IE5                 0x2A
    #define VI_IE6                 0x2C
    #define VI_IE7                 0x2E
    #define VI_STPOW               0x30
    #define VI_SETUP               0x32
    #define VI_RESR                0x38
    #define VI_SUSR                0x3A
    #define VI_RSTR                0x3C
    #define VI_IE0                 0x44
    #define VI_OE0                 0x46
    #define VI_TxDMA               0x50
    #define VI_RxDMA               0x52
    #define VI_HxDMA               0x54

/* Endpoint Config register bit masks */
#define EC_USB_Intr_Enable    0x04
#define EC_Stall              0x08
#define EC_DBuf_Enable        0x10
#define EC_DBuf_Toggle_State  0x20
#define EC_ISO                0x40
#define EC_UBM_Enable         0x80

/* Endpoint Base Address shift amount */
#define EBA_Shift             3

/* Endpoint Byte Count register masks */
#define EBC_Count_Mask        0x7F
#define EBC_Valid             0x80

/* EndPoint 0 registers */
#define OutEP0_Byte_Count        0x1E0C
#define OutEP0_Config            0x1E08
#define InEP0_Byte_Count         0x1E04
#define InEP0_Config             0x1E00

/* EndPoints 1-7 (EP0 is different, see above) */
typedef struct cdsp_endpoint_descriptor {
        u32 config;
        u32 x_buffer_base;
        u32 x_buffer_byte_count;
        u32 spare_3;
        u32 spare_4;
        u32 y_buffer_base;
            // Uses same shift as X buffer
        u32 y_buffer_byte_count;
            // Uses same flag as X buffer
        u32 xy_buffer_size;
    } cdsp_endpoint_descriptor;

#define InEP_Descrip_Base         0x1D00
#define InEP_Descrip(n)           (InEP_Descrip_Base + (n * sizeof(cdsp_endpoint_descriptor))) 

#define OutEP_Descrip_Base        0x1C00
#define OutEP_Descrip(n)          (OutEP_Descrip_Base + (n * sizeof(cdsp_endpoint_descriptor)))

#define Setup_Packet             0x1C00 /* 8 u32s, 1 u8 each */
#define InEP0_Buffer             0x1BE0 /* 8 u32s, 1 u8 each */
#define OutEP0_Buffer            0x1BC0 /* 8 u32s, 1 u8 each */

#define EP_Buffer_Base           0x0000 /* 1 u8 in lsb of each u32 */
// Divvy the buffer area into n buffers of 64 u32s each.
// Only the LSB of each u32 is used, so this is 64 bytes/buffer
#define EB64A(b) ((EP_Buffer_Base + (b * 64 * sizeof(u32))) >> EBA_Shift)

/* VBus Registers ---------------------------------------------------------- */
// The VBus registers are defined as offsets from bi_data->usb_base

#define U2V_Ctrlr_Config          0x0000
    #define UCC_Host_Int_Enable   (0x1<<20)
    #define UCC_Stream            (0x1<<2)
    #define UCC_Tx_Enable         (0x1<<1)
    #define UCC_Rx_Enable         (0x1<<0)

#define TxFIFO_Pointers           0x0020
#define TxFIFO_Base               0x0024
#define TxFIFO_Length             0x0028
#define TxFIFO_Poll_Rate          0x002C
#define TxFIFO_Current_Pointer    0x0030

#define RxFIFO_Pointers           0x0040
#define RxFIFO_Base               0x0044
#define RxFIFO_Length             0x0048
#define RxFIFO_Poll_Rate          0x004C
#define RxFIFO_Current_Pointer    0x0050

#define Hx_Interrupt_Addr         0x0070
#define Hx_Interrupt              0x0074

#define Mail_Box_1                0x0080
#define Mail_Box_2                0x0084

#endif
