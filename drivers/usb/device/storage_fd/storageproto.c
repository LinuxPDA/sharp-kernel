/*
 * linux/drivers/usb/device/storage_fd/storageproto.c - mass storage protocol library
 *
 * Copyright (c) 2003 Lineo Solutions, Inc.
 *
 * Written by Shunnosuke kabata
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

/******************************************************************************
** Include File
******************************************************************************/
#include <linux/config.h>
#include <linux/module.h>

#include "../usbd-export.h"
#include "../usbd-build.h"
#include "../usbd-module.h"

#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/list.h>
#include <linux/netdevice.h>
#include <linux/skbuff.h>
#include <linux/etherdevice.h>
#include <linux/rtnetlink.h>
#include <linux/smp_lock.h>
#include <linux/ctype.h>
#include <linux/timer.h>
#include <linux/string.h>
#include <linux/atmdev.h>
#include <linux/pkt_sched.h>
#include <linux/delay.h>
#include <linux/blkdev.h>
#include <linux/file.h>
#include <asm/uaccess.h>
#include <asm/system.h>
#include <net/arp.h>

#include <linux/autoconf.h>

#include "../usbd.h"
#include "../usbd-func.h"
#include "../usbd-bus.h"
#include "../usbd-inline.h"
#include "../usbd-arch.h"
#include "../hotplug.h"

#include "schedule_task.h"
#include "storageproto.h"

/******************************************************************************
** macro define
******************************************************************************/

#define DEVICE_BLOCK_SIZE       512

#define BLOCK_BUFFER_SIZE       (1024 * 64)

#define DEF_NUMBER_OF_HEADS     0x10
#define DEF_SECTORS_PER_TRACK   0x20

/******************************************************************************
** Structure Define
******************************************************************************/

typedef struct{
    unsigned char   scsi_command;
    unsigned char*  command_name;
    void            (*scsi_func)(struct usb_device_instance*,
                                 COMMAND_BLOCK_WRAPPER*);
} SCSI_ANALYSIS_TBL;

typedef struct{
    unsigned char   scsi_command;
    unsigned char*  command_name;
    void            (*bulkout_func)(struct usb_device_instance*,
                                    void*, int);
} SCSI_BULKOUT_ANALYSIS_TBL;

/******************************************************************************
** Variable Declaration
******************************************************************************/

/**************************************
** Module Parameters
**************************************/

static char*    storage_device = CONFIG_USBD_STORAGE_DEF_DEVICE_NAME;
MODULE_PARM(storage_device, "s");

/**************************************
** Device Information
**************************************/

static struct file*     DeviceFile      = NULL;
static unsigned int     DeviceSize      = 0;  
static unsigned int     DeviceBlockNum = 0;  //=DeviceSize/DeviceBlockSize  //for terrier-j 
static unsigned int     DeviceBlockSize = DEVICE_BLOCK_SIZE;
static int              DeviceWrProtect = WR_PROTECT_OFF;

/**************************************
** Status
**************************************/

static int  StorageStatus  = STORAGE_IDLE;
static int  UsbStatus      = USB_DISCONNECT;
static int  MediaStatus    = MEDIA_EJECT;
static int  MediaChange    = MEDIA_CHANGE_OFF;

/**************************************
** Keep Information
**************************************/

static SCSI_REQUEST_SENSE_DATA  RequestSenseData;
static COMMAND_BLOCK_WRAPPER    KeepCBW;
static unsigned long            BulkOutLength = 0;
static unsigned char            BlockBuffer[BLOCK_BUFFER_SIZE];

/**************************************
** Statistics
**************************************/
static unsigned long    StatMaxBulkInSize  = 0;
static unsigned long    StatMaxBulkOutSize = 0;
static unsigned long    StatDevWriteError  = 0;
static unsigned long    StatDevReadError   = 0;
static unsigned long    StatDevFlushError  = 0;
static unsigned long    StatWriteTimout    = 0;
static unsigned long    StatMaxWriteTime   = 0;

/**************************************
** Timer
**************************************/
static struct timer_list    BulkOutTim;
static struct timer_list    MediaCheckTim;

/******************************************************************************
** Local Function
******************************************************************************/

static void storage_bulkout_timeout(unsigned long param)
{
    /* statistics update */
    StatWriteTimout++;
    printk(KERN_INFO "storage_fd: write bulk out timeout. length '%ld/%ld'.\n",
        BulkOutLength, KeepCBW.dCBWDataTransferLength);

    return;
}

static void media_check_timeout(unsigned long param)
{
    /* media check */
    schedule_task_register(
        (SCHEDULE_TASK_FUNC)storageproto_media_status_check, CONTEXT_TIMER,
        0, 0, 0, 0);

    return;
}

static void request_sense_data_set(unsigned char sense_key, unsigned char asc,
                                   unsigned char ascq, unsigned long info)
{
    /*
     * set REQUEST SENSE DATA
     */

    memset(&RequestSenseData, 0x00, sizeof(RequestSenseData));

    RequestSenseData.ErrorCode                    = 0x70;
    RequestSenseData.Valid                        = (info) ? 1 : 0;
    RequestSenseData.SenseKey                     = sense_key;
    memcpy(RequestSenseData.Information, &info, sizeof(info));
    RequestSenseData.AdditionalSenseLength        = 0x0a;
    RequestSenseData.AdditionalSenseCode          = asc;
    RequestSenseData.AdditionalSenseCodeQualifier = ascq;

    return;
}

static void scsi_inquiry_analysis(struct usb_device_instance* device,
                                  COMMAND_BLOCK_WRAPPER* cbw)
{
    SCSI_INQUIRY_DATA       data;
    COMMAND_STATUS_WRAPPER  csw;
    unsigned long           data_len;

    /*
     * data transport
     */

    memset(&data, 0x00, sizeof(data));

    data.PeripheralDeviceType = 0x00;
    data.RMB                  = 1;
    data.ResponseDataFormat   = 0x01;
    data.AdditionalLength     = 0x1f;
    strncpy(data.VendorInformation, CONFIG_USBD_MANUFACTURER,
            sizeof(data.VendorInformation));
    strncpy(data.ProductIdentification, CONFIG_USBD_PRODUCT_NAME,
            sizeof(data.ProductIdentification));
    strncpy(data.ProductRevisionLevel, PRODUCT_REVISION_LEVEL,
            sizeof(data.ProductRevisionLevel));

    data_len = sizeof(data);
    if(cbw->dCBWDataTransferLength < data_len){
        data_len = cbw->dCBWDataTransferLength;
    }

    storage_urb_send(device, &data, data_len);

    /*
     * status transport
     */

    memset(&csw, 0x00, sizeof(csw));

    csw.dCSWSignature   = CSW_SIGNATURE;
    csw.dCSWTag         = cbw->dCBWTag;
    csw.dCSWDataResidue = cbw->dCBWDataTransferLength - data_len;
    csw.bCSWStatus      = 0;

    storage_urb_send(device, &csw, sizeof(csw));

    return;
}

static void scsi_read_format_capacity_analysis(struct usb_device_instance* device,
                                               COMMAND_BLOCK_WRAPPER* cbw)
{
    SCSI_READ_FORMAT_CAPACITY_DATA  data;
    COMMAND_STATUS_WRAPPER          csw;
    unsigned long                   block_num, data_len;
    unsigned short                  block_len;

    /*
     * data transport
     */

    block_num = 0xffffffff;
    block_len = (unsigned short)DeviceBlockSize;
    block_num = htonl(block_num);
    block_len = htons(block_len);

    memset(&data, 0x00, sizeof(data));

    data.CapacityListHeader.CapacityListLength =
        sizeof(data.CurrentMaximumCapacityDescriptor);
    memcpy(data.CurrentMaximumCapacityDescriptor.NumberofBlocks, &block_num,
           sizeof(block_num));
    data.CurrentMaximumCapacityDescriptor.DescriptorCode = 0x03;
    memcpy(data.CurrentMaximumCapacityDescriptor.BlockLength + 1, &block_len,
           sizeof(block_len));

    data_len = sizeof(data);
    if(cbw->dCBWDataTransferLength < data_len){
        data_len = cbw->dCBWDataTransferLength;
    }

    storage_urb_send(device, &data, data_len);

    /*
     * status transport
     */

    memset(&csw, 0x00, sizeof(csw));

    csw.dCSWSignature   = CSW_SIGNATURE;
    csw.dCSWTag         = cbw->dCBWTag;
    csw.dCSWDataResidue = cbw->dCBWDataTransferLength - data_len;
    csw.bCSWStatus      = 0;

    storage_urb_send(device, &csw, sizeof(csw));

    return;
}

static void scsi_read_capacity_analysis(struct usb_device_instance* device,
                                        COMMAND_BLOCK_WRAPPER* cbw)
{
    SCSI_READ_CAPACITY_DATA data;
    COMMAND_STATUS_WRAPPER  csw;
    unsigned char           data_len, status = 0;

    if(DeviceFile == NULL){
        /* 0 clear */
        memset(&data, 0x00, sizeof(data));

        /* data length set */
        data_len = cbw->dCBWDataTransferLength;

        /* status set */
        status = 1;

        /* error code save REQUEST SENSE */
        request_sense_data_set(0x02, 0x3a, 0x00, 0x00);
    }
    else
    if(MediaChange == MEDIA_CHANGE_ON){
        /* 0 clear */
        memset(&data, 0x00, sizeof(data));

        /* data length set */
        data_len = cbw->dCBWDataTransferLength;

        /* status set */
        status = 1;

        /* error code save REQUEST SENSE */
        request_sense_data_set(0x06, 0x28, 0x00, 0x00);

        /* media change flag off */
        MediaChange = MEDIA_CHANGE_OFF;
    }
    else{
        unsigned unsigned long   last_lba, block_len;

        /* 0 clear */
        memset(&data, 0x00, sizeof(data));

        /* data set */
	//for terrier-j
	//        last_lba  = (DeviceSize / DeviceBlockSize) - 1; 
	last_lba  = DeviceBlockNum - 1;
        block_len = DeviceBlockSize;
        last_lba  = htonl(last_lba);
        block_len = htonl(block_len);

        memcpy(data.LastLogicalBlockAddress, &last_lba, sizeof(last_lba));
        memcpy(data.BlockLengthInBytes, &block_len, sizeof(block_len));

        /* data length set */
        data_len = sizeof(data);

        /* status set */
        status = 0;
    }

    /*
     * data transport
     */

    if(cbw->dCBWDataTransferLength < data_len){
        data_len = cbw->dCBWDataTransferLength;
    }

    storage_urb_send(device, &data, data_len);

    /*
     * status transport
     */

    memset(&csw, 0x00, sizeof(csw));

    csw.dCSWSignature   = CSW_SIGNATURE;
    csw.dCSWTag         = cbw->dCBWTag;
    csw.dCSWDataResidue = cbw->dCBWDataTransferLength - data_len;
    csw.bCSWStatus      = status;

    storage_urb_send(device, &csw, sizeof(csw));

    return;
}

static void scsi_request_sense_analysis(struct usb_device_instance* device,
                                        COMMAND_BLOCK_WRAPPER* cbw)
{
    COMMAND_STATUS_WRAPPER  csw;
    unsigned long           data_len;

    /*
     * data transport
     */

    data_len = sizeof(RequestSenseData);
    if(cbw->dCBWDataTransferLength < data_len){
        data_len = cbw->dCBWDataTransferLength;
    }

    storage_urb_send(device, &RequestSenseData, data_len);

    /*
     * status transport
     */

    memset(&csw, 0x00, sizeof(csw));

    csw.dCSWSignature   = CSW_SIGNATURE;
    csw.dCSWTag         = cbw->dCBWTag;
    csw.dCSWDataResidue = cbw->dCBWDataTransferLength - data_len;
    csw.bCSWStatus      = 0;

    storage_urb_send(device, &csw, sizeof(csw));

    return;
}

static void scsi_read_10_analysis(struct usb_device_instance* device,
                                  COMMAND_BLOCK_WRAPPER* cbw)
{
    SCSI_READ_10_COMMAND*   command = (SCSI_READ_10_COMMAND*)cbw->CBWCB;
    COMMAND_STATUS_WRAPPER  csw;
    unsigned char           status = 0;
    unsigned short          len;
    //for terrier
    //    unsigned long           lba, size, offset;
    unsigned long           lba, size;
    long long           offset;

    memcpy(&lba, command->LogicalBlockAddress, sizeof(lba));
    memcpy(&len, command->TransferLength, sizeof(len));
    lba    = ntohl(lba);
    len    = ntohs(len);

    //change terrier-j
    //    offset = lba * DeviceBlockSize;
    offset = (long long)lba * DeviceBlockSize;
    size   = cbw->dCBWDataTransferLength;

    if(DeviceFile == NULL){
        /* command fail */
        status = 1;

        /* error code save REQUEST SENSE */
        request_sense_data_set(0x02, 0x3a, 0x00, 0x00);

        /*
         * data transport
         */

        storage_urb_send(device, NULL, size);
    }
    else
    if(MediaChange == MEDIA_CHANGE_ON){
        /* command fail */
        status = 1;

        /* error code save REQUEST SENSE */
        request_sense_data_set(0x06, 0x28, 0x00, 0x00);

        /* media change flag off */
        MediaChange = MEDIA_CHANGE_OFF;

        /*
         * data transport
         */

        storage_urb_send(device, NULL, size);
    }
    else{
        unsigned long   count, read_size;

        /*
         * data transport
         */

        /* device seek */
        DeviceFile->f_op->llseek(DeviceFile, offset, 0);

        /* device read */
        for(count = size; count; count -= read_size){
            read_size = (count > sizeof(BlockBuffer)) ?
                            sizeof(BlockBuffer) : count;
            if(DeviceFile &&
               DeviceFile->f_op->read(DeviceFile, BlockBuffer, read_size,
                                      &DeviceFile->f_pos) != read_size){

                /* command fail */
                status = 1;

                /* error code save REQUEST SENSE */
                request_sense_data_set(0x02, 0x3a, 0x00, 0x00);

                /* statistics update */
                StatDevReadError++;
                printk(KERN_INFO "storage_fd: device read error. length '%ld'.\n", read_size);
            }

            storage_urb_send(device, BlockBuffer, read_size);
        }
    }

    /*
     * status transport
     */

    memset(&csw, 0x00, sizeof(csw));

    csw.dCSWSignature   = CSW_SIGNATURE;
    csw.dCSWTag         = cbw->dCBWTag;
    csw.dCSWDataResidue = cbw->dCBWDataTransferLength - size;
    csw.bCSWStatus      = status;

    storage_urb_send(device, &csw, sizeof(csw));

    return;
}

static void scsi_mode_sense_analysis(struct usb_device_instance* device,
                                     COMMAND_BLOCK_WRAPPER* cbw)
{
    static READ_WRITE_ERROR_RECOVERY_PAGE           page_01 = {
        PageCode:0x01,
        PageLength:0x0A,
        ReadRetryCount:0x03,
        WriteRetryCount:0x80,
    };
    static FLEXIBLE_DISK_PAGE                       page_05 = {
        PageCode:0x05,
        PageLength:0x1E,
        TransferRate:{0x00, 0xFA},
        NumberofHeads:0xA0,
        SectorsperTrack:0x00,
        DataBytesperSector:{0x02, 0x00},
        NumberofCylinders:{0x00, 0x00},
        MotorOnDelay:0x05,
        MotorOffDelay:0x1E,
        MediumRotationRate:{0x01, 0x68},
    };
    static REMOVABLE_BLOCK_ACCESS_CAPABILITIES_PAGE page_1b = {
        PageCode:0x1B,
        PageLength:0x0A,
        TLUN:0x01,
    };
    static TIMER_AND_PROTECT_PAGE                   page_1c = {
        PageCode:0x1c,
        PageLength:0x06,
        InactivityTimeMultiplier:0x0A,
    };


    SCSI_MODE_SENSE_COMMAND*    command = (SCSI_MODE_SENSE_COMMAND*)cbw->CBWCB;
    SCSI_MODE_SENSE_DATA        data;
    COMMAND_STATUS_WRAPPER      csw;
    unsigned char               data_len, status = 0;
    unsigned short              cylinder, sector;
    unsigned long               size;

    /*
     * data transport
     */

    memset(&data, 0x00, sizeof(data));

    /* set write protect */
    data.ModeParameterHeader.WP = DeviceWrProtect;

    /* set Flexible Disk Page */
    if(DeviceFile == NULL){
        sector   = (unsigned short)DeviceBlockSize;
        cylinder = 0;
        sector   = htons(sector);
        cylinder = htons(cylinder);

        page_05.NumberofHeads      = 0;
        page_05.SectorsperTrack    = 0;
        memcpy(page_05.DataBytesperSector, &sector, sizeof(sector));
        memcpy(page_05.NumberofCylinders, &cylinder, sizeof(cylinder));
    }
    else{
        sector   = (unsigned short)DeviceBlockSize;
        size     = DEF_NUMBER_OF_HEADS * DEF_SECTORS_PER_TRACK * sector;
	//        cylinder = DeviceSize / size;
	cylinder = DeviceBlockNum /(DEF_NUMBER_OF_HEADS * DEF_SECTORS_PER_TRACK) ;
        sector   = htons(sector);
        cylinder = htons(cylinder);

        page_05.NumberofHeads      = DEF_NUMBER_OF_HEADS;
        page_05.SectorsperTrack    = DEF_SECTORS_PER_TRACK;
        memcpy(page_05.DataBytesperSector, &sector, sizeof(sector));
        memcpy(page_05.NumberofCylinders, &cylinder, sizeof(cylinder));
    }

    if(command->PC == 0 && command->PageCode == 0x01){
        data_len = sizeof(MODE_PARAMETER_HEADER) + sizeof(page_01);
        data.ModeParameterHeader.ModeDataLength = data_len - 1;
        memcpy(&data.ModePages, &page_01, sizeof(page_01));
    }
    else
    if(command->PC == 0 && command->PageCode == 0x05){
        data_len = sizeof(MODE_PARAMETER_HEADER) + sizeof(page_05);
        data.ModeParameterHeader.ModeDataLength = data_len - 1;
        memcpy(&data.ModePages, &page_05, sizeof(page_05));
    }
    else
    if(command->PC == 0 && command->PageCode == 0x1b){
        data_len = sizeof(MODE_PARAMETER_HEADER) + sizeof(page_1b);
        data.ModeParameterHeader.ModeDataLength = data_len - 1;
        memcpy(&data.ModePages, &page_1b, sizeof(page_1b));
    }
    else
    if(command->PC == 0 && command->PageCode == 0x1c){
        data_len = sizeof(MODE_PARAMETER_HEADER) + sizeof(page_1c);
        data.ModeParameterHeader.ModeDataLength = data_len - 1;
        memcpy(&data.ModePages, &page_1c, sizeof(page_1c));
    }
    else
    if(command->PC == 0 && command->PageCode == 0x3f){
        data_len = sizeof(MODE_PARAMETER_HEADER) + sizeof(MODE_ALL_PAGES);
        data.ModeParameterHeader.ModeDataLength = data_len - 1;
        memcpy(&data.ModePages.ModeAllPages.ReadWriteErrorRecoveryPage,
               &page_01, sizeof(page_01));
        memcpy(&data.ModePages.ModeAllPages.FlexibleDiskPage,
               &page_05, sizeof(page_05));
        memcpy(&data.ModePages.ModeAllPages.RemovableBlockAccessCapabilitiesPage,
               &page_1b, sizeof(page_1b));
        memcpy(&data.ModePages.ModeAllPages.TimerAndProtectPage,
               &page_1c, sizeof(page_1c));
    }
    else{
        /* command fail */
        status = 1;
        data_len = cbw->dCBWDataTransferLength;

        /* error code save REQUEST SENSE */
        request_sense_data_set(0x05, 0x24, 0x00, 0x00);
    }

    if(cbw->dCBWDataTransferLength < data_len){
        data_len = cbw->dCBWDataTransferLength;
    }

    storage_urb_send(device, &data, data_len);

    /*
     * status transport
     */

    memset(&csw, 0x00, sizeof(csw));

    csw.dCSWSignature   = CSW_SIGNATURE;
    csw.dCSWTag         = cbw->dCBWTag;
    csw.dCSWDataResidue = cbw->dCBWDataTransferLength - data_len;
    csw.bCSWStatus      = status;

    storage_urb_send(device, &csw, sizeof(csw));

    return;
}

static void scsi_test_unit_ready_analysis(struct usb_device_instance* device,
                                          COMMAND_BLOCK_WRAPPER* cbw)
{
    COMMAND_STATUS_WRAPPER      csw;
    unsigned char               status = 0;

    if(DeviceFile == NULL){
        /* command fail */
        status = 1;

        /* error code save REQUEST SENSE */
        request_sense_data_set(0x02, 0x3a, 0x00, 0x00);
    }
    else
    if(MediaChange == MEDIA_CHANGE_ON){
        /* command fail */
        status = 1;

        /* error code save REQUEST SENSE */
        request_sense_data_set(0x06, 0x28, 0x00, 0x00);

        /* media change flag off */
        MediaChange = MEDIA_CHANGE_OFF;
    }

    /*
     * status transport
     */

    memset(&csw, 0x00, sizeof(csw));

    csw.dCSWSignature   = CSW_SIGNATURE;
    csw.dCSWTag         = cbw->dCBWTag;
    csw.dCSWDataResidue = cbw->dCBWDataTransferLength;
    csw.bCSWStatus      = status;

    storage_urb_send(device, &csw, sizeof(csw));

    return;
}

static void scsi_prevent_allow_medium_removal_analysis(struct usb_device_instance* device,
                                                       COMMAND_BLOCK_WRAPPER* cbw)
{
    SCSI_PREVENT_ALLOW_MEDIUM_REMOVAL_COMMAND*  command = (SCSI_PREVENT_ALLOW_MEDIUM_REMOVAL_COMMAND*)cbw->CBWCB;
    COMMAND_STATUS_WRAPPER                      csw;
    unsigned char                               status = 0;

    if(command->Prevent){
        /* command fail */
        status = 1;

        /* error code save REQUEST SENSE */
        request_sense_data_set(0x05, 0x24, 0x00, 0x00);
    }

    /*
     * status transport
     */

    memset(&csw, 0x00, sizeof(csw));

    csw.dCSWSignature   = CSW_SIGNATURE;
    csw.dCSWTag         = cbw->dCBWTag;
    csw.dCSWDataResidue = cbw->dCBWDataTransferLength;
    csw.bCSWStatus      = status;

    storage_urb_send(device, &csw, sizeof(csw));

    return;
}

static void scsi_start_stop_analysis(struct usb_device_instance* device,
                                     COMMAND_BLOCK_WRAPPER* cbw)
{
    SCSI_START_STOP_COMMAND*    command = (SCSI_START_STOP_COMMAND*)cbw->CBWCB;
    COMMAND_STATUS_WRAPPER      csw;
    unsigned char               status = 0;

    if(DeviceFile == NULL){
        /* command fail */
        status = 1;

        /* error code save REQUEST SENSE */
        request_sense_data_set(0x02, 0x3a, 0x00, 0x00);
    }
    else
    if(MediaChange == MEDIA_CHANGE_ON){
        /* command fail */
        status = 1;

        /* error code save REQUEST SENSE */
        request_sense_data_set(0x06, 0x28, 0x00, 0x00);

        /* media change flag off */
        MediaChange = MEDIA_CHANGE_OFF;
    }
    else
    if(command->Start && command->LoEj){
        /* command fail */
        status = 1;

        /* error code save REQUEST SENSE */
        request_sense_data_set(0x05, 0x24, 0x00, 0x00);
    }

    /* device buffer flush */
    if(DeviceFile && DeviceFile->f_op->ioctl(DeviceFile->f_dentry->d_inode,
                                             DeviceFile, BLKFLSBUF, 0) != 0){
        /* statistics update */
        StatDevFlushError++;
        printk(KERN_INFO "storage_fd: device flush error.\n");
    }

    /*
     * status transport
     */

    memset(&csw, 0x00, sizeof(csw));

    csw.dCSWSignature   = CSW_SIGNATURE;
    csw.dCSWTag         = cbw->dCBWTag;
    csw.dCSWDataResidue = cbw->dCBWDataTransferLength;
    csw.bCSWStatus      = status;

    storage_urb_send(device, &csw, sizeof(csw));

    return;
}

static void scsi_write_10_analysis(struct usb_device_instance* device,
                                   COMMAND_BLOCK_WRAPPER* cbw)
{
    /* status set */
    BulkOutLength = 0;
    StorageStatus = STORAGE_BULKOUT;

    /* timer set */
    del_timer(&BulkOutTim);
    BulkOutTim.expires  = jiffies + ((WR_BULKOUT_CHK_TIM * HZ) / 1000);
    BulkOutTim.data     = 0;
    BulkOutTim.function = storage_bulkout_timeout;
    add_timer(&BulkOutTim);

    return;
}

static void scsi_verify_analysis(struct usb_device_instance* device,
                                 COMMAND_BLOCK_WRAPPER* cbw)
{
    COMMAND_STATUS_WRAPPER      csw;
    unsigned char               status = 0;

    if(DeviceFile == NULL){
        /* command fail */
        status = 1;

        /* error code save REQUEST SENSE */
        request_sense_data_set(0x02, 0x3a, 0x00, 0x00);
    }
    else
    if(MediaChange == MEDIA_CHANGE_ON){
        /* command fail */
        status = 1;

        /* error code save REQUEST SENSE */
        request_sense_data_set(0x06, 0x28, 0x00, 0x00);

        /* media change flag off */
        MediaChange = MEDIA_CHANGE_OFF;
    }

    /*
     * status transport
     */

    memset(&csw, 0x00, sizeof(csw));

    csw.dCSWSignature   = CSW_SIGNATURE;
    csw.dCSWTag         = cbw->dCBWTag;
    csw.dCSWDataResidue = cbw->dCBWDataTransferLength;
    csw.bCSWStatus      = status;

    storage_urb_send(device, &csw, sizeof(csw));

    return;
}

static void scsi_unsupport_analysis(struct usb_device_instance* device,
                                    COMMAND_BLOCK_WRAPPER* cbw)
{
    COMMAND_STATUS_WRAPPER csw;
    unsigned char          data_len = 0;

    if(((cbw->bmCBWFlags & 0x80) == 0x00) && (cbw->dCBWDataTransferLength)){
        /* BLKOUT */

        /* status set */
        BulkOutLength = 0;
        StorageStatus = STORAGE_BULKOUT;
    }
    else{
        /* BLKIN */

        if(cbw->dCBWDataTransferLength){
            data_len = cbw->dCBWDataTransferLength;
            storage_urb_send(device, NULL, data_len);
        }

        /*
         * status transport
         */

        memset(&csw, 0x00, sizeof(csw));

        csw.dCSWSignature   = CSW_SIGNATURE;
        csw.dCSWTag         = cbw->dCBWTag;
        csw.dCSWDataResidue = cbw->dCBWDataTransferLength - data_len;
        csw.bCSWStatus      = 0x01;
        storage_urb_send(device, &csw, sizeof(csw));

    }

    /* error code save REQUEST SENSE */
    request_sense_data_set(0x05, 0x20, 0x00, 0x00);

    return;
}

static void scsi_bulkout_write_10_analysis(struct usb_device_instance* device,
                                           void* buffer, int length)
{
    COMMAND_STATUS_WRAPPER  csw;
    unsigned char           status = 0;
    unsigned long           buff_used_len, buff_offset;
    unsigned long           s_tick, e_tick, wr_tick;

    buff_offset = BulkOutLength % sizeof(BlockBuffer);

    memcpy(BlockBuffer + buff_offset, buffer, length);
    BulkOutLength += length;

    buff_used_len = BulkOutLength % sizeof(BlockBuffer);
    if(buff_used_len == 0) buff_used_len = sizeof(BlockBuffer);

    /* delete timer */
    if(BulkOutLength >= KeepCBW.dCBWDataTransferLength){
        del_timer(&BulkOutTim);
    }

    if(buff_used_len >= sizeof(BlockBuffer) ||
       BulkOutLength >= KeepCBW.dCBWDataTransferLength){

        /* buffer full */
        SCSI_WRITE_10_COMMAND*  command = (SCSI_WRITE_10_COMMAND*)&KeepCBW.CBWCB;
        unsigned short          len;
	//for terrier
	//        unsigned long           lba, offset;
        unsigned long           lba;
        long long           offset;

        memcpy(&lba, command->LogicalBlockAddress, sizeof(lba));
        memcpy(&len, command->TransferLength, sizeof(len));
        lba = ntohl(lba);
        len = ntohs(len);

	//for terrier-j
	//        offset = (lba * DeviceBlockSize) +
        //         (sizeof(BlockBuffer) * ((BulkOutLength - 1) / sizeof(BlockBuffer)));
        offset = ((long long)lba * DeviceBlockSize) +
                 (sizeof(BlockBuffer) * ((BulkOutLength - 1) / sizeof(BlockBuffer)));

        /* device check */
        if(DeviceFile &&
           MediaChange != MEDIA_CHANGE_ON &&
           DeviceWrProtect != WR_PROTECT_ON){

            /* write before jiffies get */
            s_tick = jiffies;

            /* device seek */
            DeviceFile->f_op->llseek(DeviceFile, offset, 0);

            /* device write */
            if(DeviceFile->f_op->write(DeviceFile, BlockBuffer, buff_used_len,
                                       &DeviceFile->f_pos) != buff_used_len){
                /* statistics update */
                StatDevWriteError++;
                printk(KERN_INFO "storage_fd: device write error. length '%ld'.\n", buff_used_len);
            }

            /* write after jiffies get */
            e_tick = jiffies;

            /* statistics update */
            wr_tick = e_tick - s_tick;
            if(wr_tick > StatMaxWriteTime){
                StatMaxWriteTime = wr_tick;
            }
        }
    }

    if(BulkOutLength >= KeepCBW.dCBWDataTransferLength){
        if(DeviceFile == NULL){
            /* command fail */
            status = 1;

            /* error code save REQUEST SENSE */
            request_sense_data_set(0x02, 0x3a, 0x00, 0x00);
        }
        else
        if(MediaChange == MEDIA_CHANGE_ON){
            /* command fail */
            status = 1;

            /* error code save REQUEST SENSE */
            request_sense_data_set(0x06, 0x28, 0x00, 0x00);

            /* media change flag off */
            MediaChange = MEDIA_CHANGE_OFF;
        }
        else
        if(DeviceWrProtect == WR_PROTECT_ON){
            /* command fail */
            status = 1;

            /* error code save REQUEST SENSE */
            request_sense_data_set(0x07, 0x27, 0x00, 0x00);

            /* media change flag off */
            MediaChange = MEDIA_CHANGE_OFF;
        }

        /*
         * status transport
         */

        memset(&csw, 0x00, sizeof(csw));

        csw.dCSWSignature   = CSW_SIGNATURE;
        csw.dCSWTag         = KeepCBW.dCBWTag;
        csw.dCSWDataResidue = KeepCBW.dCBWDataTransferLength - BulkOutLength;
        csw.bCSWStatus      = status;

        storage_urb_send(device, &csw, sizeof(csw));

        /* status reset */
        BulkOutLength = 0;
        StorageStatus = STORAGE_IDLE;

        /* flush before jiffies get */
        s_tick = jiffies;

        /* device buffer flush */
        if(DeviceFile && DeviceFile->f_op->ioctl(DeviceFile->f_dentry->d_inode,
                                                 DeviceFile, BLKFLSBUF, 0) != 0){
            /* statistics update */
            StatDevFlushError++;
            printk(KERN_INFO "storage_fd: device flush error.\n");
        }

        /* flush after jiffies get */
        e_tick = jiffies;

        /* statistics update */
        wr_tick = e_tick - s_tick;
        if(wr_tick > StatMaxWriteTime){
            StatMaxWriteTime = wr_tick;
        }

    }

    return;
}

static void scsi_bulkout_unsupport_analysis(struct usb_device_instance* device,
                                            void* buffer, int length)
{
    COMMAND_STATUS_WRAPPER  csw;

    BulkOutLength += length;

    if(BulkOutLength >= KeepCBW.dCBWDataTransferLength){
        /*
         * status transport
         */

        memset(&csw, 0x00, sizeof(csw));

        csw.dCSWSignature   = CSW_SIGNATURE;
        csw.dCSWTag         = KeepCBW.dCBWTag;
        csw.dCSWDataResidue = KeepCBW.dCBWDataTransferLength - BulkOutLength;
        csw.bCSWStatus      = 1;

        storage_urb_send(device, &csw, sizeof(csw));

        /* error code save REQUEST SENSE */
        request_sense_data_set(0x05, 0x20, 0x00, 0x00);

        /* status reset */
        BulkOutLength = 0;
        StorageStatus = STORAGE_IDLE;
    }

    return;
}

/******************************************************************************
** Global Function
******************************************************************************/

static SCSI_ANALYSIS_TBL    ScsiAnalysisTbl[] = {
    {SCSI_FORMAT_UNT,                   "SCSI_FORMAT_UNT",                   NULL},
    {SCSI_INQUIRY,                      "SCSI_INQUIRY",                      scsi_inquiry_analysis},
    {SCSI_START_STOP,                   "SCSI_START_STOP",                   scsi_start_stop_analysis},
    {SCSI_MODE_SELECT,                  "SCSI_MODE_SELECT",                  NULL},
    {SCSI_MODE_SENSE,                   "SCSI_MODE_SENSE",                   scsi_mode_sense_analysis},
    {SCSI_PREVENT_ALLOW_MEDIUM_REMOVAL, "SCSI_PREVENT_ALLOW_MEDIUM_REMOVAL", scsi_prevent_allow_medium_removal_analysis},
    {SCSI_READ_10,                      "SCSI_READ_10",                      scsi_read_10_analysis},
    {SCSI_READ_12,                      "SCSI_READ_12",                      NULL},
    {SCSI_READ_CAPACITY,                "SCSI_READ_CAPACITY",                scsi_read_capacity_analysis},
    {SCSI_READ_FORMAT_CAPACITY,         "SCSI_READ_FORMAT_CAPACITY",         scsi_read_format_capacity_analysis},
    {SCSI_REQUEST_SENSE,                "SCSI_REQUEST_SENSE",                scsi_request_sense_analysis},
    {SCSI_REZERO_UNIT,                  "SCSI_REZERO_UNIT",                  NULL},
    {SCSI_SEEK_10,                      "SCSI_SEEK_10",                      NULL},
    {SCSI_SEND_DIAGNOSTIC,              "SCSI_SEND_DIAGNOSTIC",              NULL},
    {SCSI_TEST_UNIT_READY,              "SCSI_TEST_UNIT_READY",              scsi_test_unit_ready_analysis},
    {SCSI_VERIFY,                       "SCSI_VERIFY",                       scsi_verify_analysis},
    {SCSI_WRITE_10,                     "SCSI_WRITE_10",                     scsi_write_10_analysis},
    {SCSI_WRITE_12,                     "SCSI_WRITE_12",                     NULL},
    {SCSI_WRITE_AND_VERIFY,             "SCSI_WRITE_AND_VERIFY",             NULL}
};

static SCSI_BULKOUT_ANALYSIS_TBL    ScsiBlkOutAnalysisTbl[] = {
    {SCSI_WRITE_10,                     "SCSI_WRITE_10",                     scsi_bulkout_write_10_analysis},
};

void storageproto_urb_analysis(struct urb* urb)
{
    COMMAND_BLOCK_WRAPPER*  cbw = (COMMAND_BLOCK_WRAPPER*)urb->buffer;
    int                     i;

    /* status BLKOUT check */
    if(StorageStatus == STORAGE_BULKOUT){
        for(i = 0;
            i < sizeof(ScsiBlkOutAnalysisTbl) / sizeof(SCSI_ANALYSIS_TBL);
            i++){
            if(ScsiBlkOutAnalysisTbl[i].scsi_command == KeepCBW.CBWCB[0]){
                if(ScsiBlkOutAnalysisTbl[i].bulkout_func){
                    ScsiBlkOutAnalysisTbl[i].bulkout_func(urb->device,
                                                          urb->buffer,
                                                          urb->actual_length);
                    goto RETURN_LABEL;
                }
                break;
            }
        }
        scsi_bulkout_unsupport_analysis(urb->device, urb->buffer, urb->actual_length);
        goto RETURN_LABEL;
    }

    /* signature check */
    if(cbw->dCBWSignature != CBW_SIGNATURE){
        printk(KERN_INFO "storage_fd: signature error. '0x%08lx'.\n",
            cbw->dCBWSignature);
        goto RETURN_LABEL;
    }

    /* statistics set */
    if(cbw->dCBWDataTransferLength){
        if(((cbw->bmCBWFlags & 0x80) == 0x00) && (cbw->dCBWDataTransferLength)){
            /* BULK OUT */
            if(StatMaxBulkOutSize < cbw->dCBWDataTransferLength){
                StatMaxBulkOutSize = cbw->dCBWDataTransferLength;
            }
        }
        else{
            /* BULK IN */
            if(StatMaxBulkInSize < cbw->dCBWDataTransferLength){
                StatMaxBulkInSize = cbw->dCBWDataTransferLength;
            }
        }
    }

    /* save CBW and set storage status */
    memcpy(&KeepCBW, cbw, sizeof(KeepCBW));

    /* UFI command analysis */
    for(i = 0; i < sizeof(ScsiAnalysisTbl) / sizeof(SCSI_ANALYSIS_TBL); i++){
        if(ScsiAnalysisTbl[i].scsi_command == cbw->CBWCB[0]){
            if(ScsiAnalysisTbl[i].scsi_func){
                ScsiAnalysisTbl[i].scsi_func(urb->device, cbw);
                goto RETURN_LABEL;
            }
            break;
        }
    }

    scsi_unsupport_analysis(urb->device, cbw);
    printk(KERN_INFO "storage_fd: SCSI command error. '0x%02x'.\n",
        cbw->CBWCB[0]);
    goto RETURN_LABEL;

RETURN_LABEL:

    /* URB free */
    usbd_recycle_urb(urb);

    return;
}

int storageproto_device_open_check(void)
{
    struct file*    file;
    struct inode*   inode;
    kdev_t          dev;
    int             read_org;

    /* device already open check */
    if(DeviceFile){
        storageproto_device_close();
    }

    /* device open */
    file = filp_open(storage_device, O_RDWR, 0000);
    if(IS_ERR(file)){
        file = filp_open(storage_device, O_RDONLY, 0000);
        if(IS_ERR(file)){
            storageproto_device_close();
            return MEDIA_EJECT;
        }
        DeviceWrProtect = WR_PROTECT_ON;
    }

    file->f_op->llseek(file, 0, 0);
    if(file->f_op->read(file, (void*)&read_org, sizeof(read_org), &file->f_pos)
                        != sizeof(read_org)){
        filp_close(file, NULL);
        storageproto_device_close();
        return MEDIA_EJECT;
    }

    /* struct file pointer save */
    DeviceFile = file;

    /* device information */
    inode = file->f_dentry->d_inode;
    dev = inode->i_rdev;

    if (blk_size[MAJOR(dev)]){
      //for  terrier-j
      //        DeviceSize = blk_size[MAJOR(dev)][MINOR(dev)] << BLOCK_SIZE_BITS;
      DeviceBlockNum = blk_size[MAJOR(dev)][MINOR(dev)]*(BLOCK_SIZE/DEVICE_BLOCK_SIZE);
    }
    else{
      //        DeviceSize = INT_MAX << BLOCK_SIZE_BITS;
        DeviceBlockNum = INT_MAX*(BLOCK_SIZE/DEVICE_BLOCK_SIZE) ;
    }

    if(blksize_size[MAJOR(dev)] && blksize_size[MAJOR(dev)][MINOR(dev)]){
        DeviceBlockSize = DEVICE_BLOCK_SIZE;
    }
    else{
        DeviceBlockSize = DEVICE_BLOCK_SIZE;
    }

    return MEDIA_INSERT;
}

void storageproto_device_close(void)
{
    if(DeviceFile){
        filp_close(DeviceFile, NULL);
        DeviceFile      = NULL;
        DeviceSize      = 0;
        DeviceBlockSize = DEVICE_BLOCK_SIZE;
        DeviceWrProtect = WR_PROTECT_OFF;
    }

    return;
}

void storageproto_usb_status_check(int status)
{
    static int  Is1stCheck = 1;
    /* USB status check */
    if(Is1stCheck){
        Is1stCheck = 0;
    }
    else{
        if(UsbStatus == status) goto RETURN_LABEL;
    }

    /* set status */
    UsbStatus = status;

    switch(UsbStatus){
    case USB_CONNECT:
        /* media status check */
        storageproto_media_status_check(CONTEXT_SCHEDULE);

        switch(MediaStatus){
        case MEDIA_EJECT:
            break;

        case MEDIA_INSERT:
            hotplug("usbdstorage", storage_device, "umount");
            break;

        default:
            break;
        }
        hotplug("usbdstorage", storage_device, "connect");
        break;

    case USB_DISCONNECT:
        /* media status check */
        storageproto_media_status_check(CONTEXT_SCHEDULE);

        /* device close */
        storageproto_device_close();

        switch(MediaStatus){
        case MEDIA_EJECT:
            break;

        case MEDIA_INSERT:
            hotplug("usbdstorage", storage_device, "mount");
            break;

        default:
            break;
        }
        hotplug("usbdstorage", storage_device, "disconnect");
        break;

    default:
        break;
    }

RETURN_LABEL:
    DBG_STORAGE_FD(KERN_INFO "storage_fd: USB check '%s' '%s' 'file:%p'.\n",
        (UsbStatus) ? "USB_CONNECT" : "USB_DISCONNECT",
        (MediaStatus) ? "MEDIA_INSERT" : "MEDIA_EJECT", DeviceFile);

    return;
}

void storageproto_media_status_check(int call_context)
{
    static unsigned long    RetryCount = 0;
    int status;

    /* media open check */
    status = storageproto_device_open_check();

    /* media status check retry */
    if(status == MEDIA_EJECT){
        switch(call_context){
        case CONTEXT_STORAGE:

            /* retry counter init */
            RetryCount = 0;

            /* timer set */
            del_timer(&MediaCheckTim);
            MediaCheckTim.expires  = jiffies + ((MEDIA_CHECK_TIM * HZ) / 1000);
            MediaCheckTim.data     = 0;
            MediaCheckTim.function = media_check_timeout;
            add_timer(&MediaCheckTim);

            break;

        case CONTEXT_TIMER:

            /* retry counter update */
            RetryCount++;

            /* retry counter check */
            if(RetryCount >= MEDIA_CHECK_RETRY) break;

            /* timer set */
            del_timer(&MediaCheckTim);
            MediaCheckTim.expires  = jiffies + ((MEDIA_CHECK_TIM * HZ) / 1000);
            MediaCheckTim.data     = 0;
            MediaCheckTim.function = media_check_timeout;
            add_timer(&MediaCheckTim);

            break;

        default:
            break;
        }
    }
    else
    if(status == MEDIA_INSERT){
        /* delete timer */
        del_timer(&MediaCheckTim);
    }

    /* media status check */
    if(status == MediaStatus){
        if(UsbStatus == USB_DISCONNECT){
            storageproto_device_close();
        }
        if(MediaStatus == MEDIA_INSERT){
            if(call_context == CONTEXT_STORAGE || call_context == CONTEXT_TIMER){
                MediaChange = MEDIA_CHANGE_ON;
            }
        }
        goto RETURN_LABEL;
    }

    /* set status */
    MediaStatus = status;

    switch(MediaStatus){
    case MEDIA_INSERT:
        /* set status */
        MediaChange = MEDIA_CHANGE_ON;

        switch(UsbStatus){
        case USB_DISCONNECT:
            storageproto_device_close();
            break;

        case USB_CONNECT:
            break;

        default:
            break;
        }
        hotplug("usbdstorage", storage_device, "insert");
        break;

    case MEDIA_EJECT:
        switch(UsbStatus){
        case USB_DISCONNECT:
            storageproto_device_close();
            break;

        case USB_CONNECT:
            break;

        default:
            break;
        }
        hotplug("usbdstorage", storage_device, "eject");
        break;

    default:
        break;
    }

RETURN_LABEL:
    DBG_STORAGE_FD(KERN_INFO "storage_fd: media check. '%s' '%s' 'file:%p' '%s'.\n",
        (UsbStatus) ? "USB_CONNECT" : "USB_DISCONNECT",
        (MediaStatus) ? "MEDIA_INSERT" : "MEDIA_EJECT", DeviceFile,
        (call_context == CONTEXT_SCHEDULE) ? "CONTEXT_SCHEDULE" : 
        (call_context == CONTEXT_STORAGE)  ? "CONTEXT_STORAGE" : "CONTEXT_TIMER");

    return;
}

void storageproto_usb_reset_ind(void)
{
    /* status reset */
    BulkOutLength = 0;
    StorageStatus = STORAGE_IDLE;

    DBG_STORAGE_FD(KERN_INFO "storage_fd: storage protocol reset.\n");

    return;
}

void storageproto_init(void)
{
    /* timer init */
    init_timer(&BulkOutTim);

    /* timer init */
    init_timer(&MediaCheckTim);

    return;
}

void storageproto_exit(void)
{
    /* device close */
    storageproto_device_close();

    /* delete timer */
    del_timer(&BulkOutTim);

    /* delete timer */
    del_timer(&MediaCheckTim);

    return;
}

ssize_t storageproto_proc_read(struct file* file, char* buf, size_t count,
                               loff_t* pos)
{
    char    string[1024];
    int     len = 0;

    len += sprintf(string + len, "Protocol status:%s\n",
            (StorageStatus == STORAGE_IDLE)   ? "STORAGE_IDLE" :
            (StorageStatus == STORAGE_BULKIN) ? "STORAGE_BULKIN" :
                                                "STORAGE_BULKOUT");

    len += sprintf(string + len, "USB status:%s\n",
            (UsbStatus == USB_DISCONNECT) ? "USB_DISCONNECT" :
                                            "USB_CONNECT");

    len += sprintf(string + len, "Media status:%s\n",
            (MediaStatus == MEDIA_EJECT) ? "MEDIA_EJECT" :
                                           "MEDIA_INSERT");

    len += sprintf(string + len, "Media chage:%s\n",
            (MediaChange == MEDIA_CHANGE_OFF) ? "MEDIA_CHANGE_OFF" :
                                                "MEDIA_CHANGE_ON");

    len += sprintf(string + len, "Device name:%s\n",
            storage_device);

    len += sprintf(string + len, "Device file descriptor:0x%p\n",
            DeviceFile);
    /*
    len += sprintf(string + len, "Device size:0x%d(%08x)\n",
            DeviceSize,DeviceSize);
    */
    len += sprintf(string + len, "Device Block Num:0x%d(%08x)\n",
            DeviceBlockNum,DeviceBlockNum);

    len += sprintf(string + len, "Device block size:0x%d\n",
            DeviceBlockSize);

    len += sprintf(string + len, "Device write protect:%s\n",
            (DeviceWrProtect == WR_PROTECT_OFF) ? "WR_PROTECT_OFF":
                                                  "WR_PROTECT_ON");

    len += sprintf(string + len, "Bulk in max size:%ld\n",
            StatMaxBulkInSize);

    len += sprintf(string + len, "Bulk out max size:%ld\n",
            StatMaxBulkOutSize);

    len += sprintf(string + len, "device write error:%ld\n",
            StatDevWriteError);

    len += sprintf(string + len, "device read error:%ld\n",
            StatDevReadError);

    len += sprintf(string + len, "device flush error:%ld\n",
            StatDevFlushError);

    len += sprintf(string + len, "write data bulk out timout:%ld\n",
            StatWriteTimout);

    len += sprintf(string + len, "device write max time:%ld msec\n",
            (StatMaxWriteTime * 1000) / HZ);

    *pos += len;
    if(len > count){
        len = -EINVAL;
    }
    else
    if(len > 0 && copy_to_user(buf, string, len)) {
        len = -EFAULT;
    }

    return len;
}

