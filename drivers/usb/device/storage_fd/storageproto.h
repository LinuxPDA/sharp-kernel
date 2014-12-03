/*
 * linux/drivers/usb/device/storage_fd/storageproto.h - mass storage protocol library header
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

#ifndef _STORAGEPROTO_H_
#define _STORAGEPROTO_H_

/******************************************************************************
** Macro Define
******************************************************************************/

/**************************************
** Class Code
**************************************/

/*
 * Class
 */

#define MASS_STORAGE_CLASS                  0x08

/*
 * SubClass
 */

#define MASS_STORAGE_SUBCLASS_RBC           0x01
#define MASS_STORAGE_SUBCLASS_SFF8020I      0x02
#define MASS_STORAGE_SUBCLASS_QIC157        0x03
#define MASS_STORAGE_SUBCLASS_UFI           0x04
#define MASS_STORAGE_SUBCLASS_SFF8070I      0x05
#define MASS_STORAGE_SUBCLASS_SCSI          0x06

/*
 * Protocol
 */

#define MASS_STORAGE_PROTO_CBI_WITH_COMP    0x00
#define MASS_STORAGE_PROTO_CBI_NO_COMP      0x01
#define MASS_STORAGE_PROTO_BULK_ONLY        0x50

/**************************************
** SCSI Command
**************************************/

#define SCSI_FORMAT_UNT                      0x04
#define SCSI_INQUIRY                         0x12
#define SCSI_START_STOP                      0x1b
#define SCSI_MODE_SELECT                     0x55
#define SCSI_MODE_SENSE                      0x1a
#define SCSI_PREVENT_ALLOW_MEDIUM_REMOVAL    0x1e
#define SCSI_READ_10                         0x28
#define SCSI_READ_12                         0xa8
#define SCSI_READ_CAPACITY                   0x25
#define SCSI_READ_FORMAT_CAPACITY            0x23
#define SCSI_REQUEST_SENSE                   0x03
#define SCSI_REZERO_UNIT                     0x01
#define SCSI_SEEK_10                         0x2b
#define SCSI_SEND_DIAGNOSTIC                 0x1d
#define SCSI_TEST_UNIT_READY                 0x00
#define SCSI_VERIFY                          0x2f
#define SCSI_WRITE_10                        0x2a
#define SCSI_WRITE_12                        0xaa
#define SCSI_WRITE_AND_VERIFY                0x2e

/**************************************
** SCSI Command Parameter
**************************************/

#define CBW_SIGNATURE           0x43425355  /* USBC */
#define CSW_SIGNATURE           0x53425355  /* USBS */

#define PRODUCT_REVISION_LEVEL  "1.00"

/**************************************
** Status
**************************************/

#define WR_PROTECT_OFF      0
#define WR_PROTECT_ON       1

#define STORAGE_IDLE        0
#define STORAGE_BULKIN      1
#define STORAGE_BULKOUT     2

#define USB_DISCONNECT      0
#define USB_CONNECT         1

#define MEDIA_EJECT         0
#define MEDIA_INSERT        1

#define MEDIA_CHANGE_OFF    0
#define MEDIA_CHANGE_ON     1

/**************************************
** Mass Storage Thread Name
**************************************/

#define STORAGE_THREAD_NAME "usbdstorage"

/**************************************
** Media Signal Delay Time(ms)
**************************************/

#define USB_EVENT_DELAY_TIM 1000

/**************************************
** Write Bulk Out Check Time(ms)
**************************************/

#define WR_BULKOUT_CHK_TIM  1000

/**************************************
** Media Check Time(ms)
**************************************/

#define MEDIA_CHECK_TIM     3000
#define MEDIA_CHECK_RETRY   3

/**************************************
** Context
**************************************/

#define CONTEXT_SCHEDULE    0
#define CONTEXT_STORAGE     1
#define CONTEXT_TIMER       2

/**************************************
** Debug Message
**************************************/
#if 0
#define DBG_STORAGE_FD(fmt, args...)    printk(fmt, ##args)
#else
#define DBG_STORAGE_FD(fmt, args...)
#endif

/******************************************************************************
** Structure Define
******************************************************************************/

/**************************************
** Command Block Wrapper / Command Status Wrapper
**************************************/

/*
 * Command Block Wrapper
 */
typedef struct{
    unsigned long   dCBWSignature;
    unsigned long   dCBWTag;
    unsigned long   dCBWDataTransferLength;
    unsigned char   bmCBWFlags;
    unsigned char   bCBWLUN:4,
                    Reserved:4;
    unsigned char   bCBWCBLength:5,
                    Reserved2:3;
    unsigned char   CBWCB[16];
} __attribute__((packed)) COMMAND_BLOCK_WRAPPER;

/*
 * Command Status Wrapper
 */
typedef struct{
    unsigned long   dCSWSignature;
    unsigned long   dCSWTag;
    unsigned long   dCSWDataResidue;
    unsigned char   bCSWStatus;
} __attribute__((packed)) COMMAND_STATUS_WRAPPER;

/**************************************
** SCSI Command
**************************************/

/*
 * INQUIRY
 */

typedef struct{
    unsigned char   OperationCode;
    unsigned char   EVPD:1,
                    Reserved1:4,
                    LogicalUnitNumber:3;
    unsigned char   PageCode;
    unsigned char   Reserved2;
    unsigned char   AllocationLength;
    unsigned char   Reserved3;
    unsigned char   Reserved4;
    unsigned char   Reserved5;
    unsigned char   Reserved6;
    unsigned char   Reserved7;
    unsigned char   Reserved8;
    unsigned char   Reserved9;
} __attribute__((packed)) SCSI_INQUIRY_COMMAND;

typedef struct{
    unsigned char   PeripheralDeviceType:5,
                    Reserved1:3;
    unsigned char   Reserved2:7,
                    RMB:1;
    unsigned char   ANSIVersion:3,
                    ECMAVersion:3,
                    ISOVersion:2;
    unsigned char   ResponseDataFormat:4,
                    Reserved3:4;
    unsigned char   AdditionalLength;
    unsigned char   Reserved4;
    unsigned char   Reserved5;
    unsigned char   Reserved6;
    unsigned char   VendorInformation[8];
    unsigned char   ProductIdentification[16];
    unsigned char   ProductRevisionLevel[4];
} __attribute__((packed)) SCSI_INQUIRY_DATA;

/*
 * READ FORMAT CAPACITY
 */

typedef struct{
    unsigned char   OperationCode;
    unsigned char   Reserved1:5,
                    LogicalUnitNumber:3;
    unsigned char   Reserved2;
    unsigned char   Reserved3;
    unsigned char   Reserved4;
    unsigned char   Reserved5;
    unsigned char   Reserved6;
    unsigned char   AllocationLength[2];
    unsigned char   Reserved7;
    unsigned char   Reserved8;
    unsigned char   Reserved9;
} __attribute__((packed)) SCSI_READ_FORMAT_CAPACITY_COMMAND;

typedef struct{
    struct{
        unsigned char   Reserved1;
        unsigned char   Reserved2;
        unsigned char   Reserved3;
        unsigned char   CapacityListLength;
    } __attribute__((packed)) CapacityListHeader;
    struct{
        unsigned char   NumberofBlocks[4];
        unsigned char   DescriptorCode:2,
                        Reserved1:6;
        unsigned char   BlockLength[3];
    } __attribute__((packed)) CurrentMaximumCapacityDescriptor;
} __attribute__((packed)) SCSI_READ_FORMAT_CAPACITY_DATA;

/*
 * READ FORMAT CAPACITY
 */

typedef struct{
    unsigned char   OperationCode;
    unsigned char   RelAdr:1,
                    Reserved1:4,
                    LogicalUnitNumber:3;
    unsigned char   LogicalBlockAddress[4];
    unsigned char   Reserved2;
    unsigned char   Reserved3;
    unsigned char   PMI:1,
                    Reserved4:7;
    unsigned char   Reserved5;
    unsigned char   Reserved6;
    unsigned char   Reserved7;
} __attribute__((packed)) SCSI_READ_CAPACITY_COMMAND;

typedef struct{
    unsigned char   LastLogicalBlockAddress[4];
    unsigned char   BlockLengthInBytes[4];
} __attribute__((packed)) SCSI_READ_CAPACITY_DATA;

/*
 * REQUEST SENSE
 */

typedef struct{
    unsigned char   OperationCode;
    unsigned char   Reserved1:5,
                    LogicalUnitNumber:3;
    unsigned char   Reserved2;
    unsigned char   Reserved3;
    unsigned char   AllocationLength;
    unsigned char   Reserved4;
    unsigned char   Reserved5;
    unsigned char   Reserved6;
    unsigned char   Reserved7;
    unsigned char   Reserved8;
    unsigned char   Reserved9;
    unsigned char   Reserved10;
} __attribute__((packed)) SCSI_REQUEST_SENSE_COMMAND;

typedef struct{
    unsigned char   ErrorCode:7,
                    Valid:1;
    unsigned char   Reserved1;
    unsigned char   SenseKey:4,
                    Reserved2:4;
    unsigned char   Information[4];
    unsigned char   AdditionalSenseLength;
    unsigned char   Reserved3[4];
    unsigned char   AdditionalSenseCode;
    unsigned char   AdditionalSenseCodeQualifier;
    unsigned char   Reserved4;
    unsigned char   Reserved5[3];
} __attribute__((packed)) SCSI_REQUEST_SENSE_DATA;

/*
 * READ(10)
 */

typedef struct{
    unsigned char   OperationCode;
    unsigned char   RelAdr:1,
                    Reserved1:2,
                    FUA:1,
                    DPO:1,
                    LogicalUnitNumber:3;
    unsigned char   LogicalBlockAddress[4];
    unsigned char   Reserved2;
    unsigned char   TransferLength[2];
    unsigned char   Reserved3;
    unsigned char   Reserved4;
    unsigned char   Reserved5;
} __attribute__((packed)) SCSI_READ_10_COMMAND;

/*
 * MODE SENSE
 */

typedef struct{
    unsigned char   OperationCode;
    unsigned char   Reserved1:3,
                    DBD:1,
                    Reserved2:1,
                    LogicalUnitNumber:3;
    unsigned char   PageCode:6,
                    PC:2;
    unsigned char   Reserved3;
    unsigned char   Reserved4;
    unsigned char   Reserved5;
    unsigned char   Reserved6;
    unsigned char   ParameterListLength[2];
    unsigned char   Reserved7;
    unsigned char   Reserved8;
    unsigned char   Reserved9;
} __attribute__((packed)) SCSI_MODE_SENSE_COMMAND;

typedef struct{
    unsigned char   ModeDataLength;
    unsigned char   MediumTypeCode;
    unsigned char   Reserved1:4,
                    DPOFUA:1,
                    Reserved2:2,
                    WP:1;
    unsigned char   Reserved3;
} __attribute__((packed)) MODE_PARAMETER_HEADER;

typedef struct{
    unsigned char   PageCode:6,
                    Reserved1:1,
                    PS:1;
    unsigned char   PageLength;
    unsigned char   DCR:1,
                    Reserved2:1,
                    PER:1,
                    Reserved3:1,
                    RC:1,
                    Reserved4:1,
                    Reserved5:1,
                    AWRE:1;
    unsigned char   ReadRetryCount;
    unsigned char   Reserved6[4];
    unsigned char   WriteRetryCount;
    unsigned char   Reserved7[3];
} __attribute__((packed)) READ_WRITE_ERROR_RECOVERY_PAGE;

typedef struct{
    unsigned char   PageCode:6,
                    Reserved1:1,
                    PS:1;
    unsigned char   PageLength;
    unsigned char   TransferRate[2];
    unsigned char   NumberofHeads;
    unsigned char   SectorsperTrack;
    unsigned char   DataBytesperSector[2];
    unsigned char   NumberofCylinders[2];
    unsigned char   Reserved2[9];
    unsigned char   MotorOnDelay;
    unsigned char   MotorOffDelay;
    unsigned char   Reserved3[7];
    unsigned char   MediumRotationRate[2];
    unsigned char   Reserved4;
    unsigned char   Reserved5;
} __attribute__((packed)) FLEXIBLE_DISK_PAGE;

typedef struct{
    unsigned char   PageCode:6,
                    Reserved1:1,
                    PS:1;
    unsigned char   PageLength;
    unsigned char   Reserved2:6,
                    SRFP:1,
                    SFLP:1;
    unsigned char   TLUN:3,
                    Reserved3:3,
                    SML:1,
                    NCD:1;
    unsigned char   Reserved4[8];
} __attribute__((packed)) REMOVABLE_BLOCK_ACCESS_CAPABILITIES_PAGE;

typedef struct{
    unsigned char   PageCode:6,
                    Reserved1:1,
                    PS:1;
    unsigned char   PageLength;
    unsigned char   Reserved2;
    unsigned char   InactivityTimeMultiplier:4,
                    Reserved3:4;
    unsigned char   SWPP:1,
                    DISP:1,
                    Reserved4:6;
    unsigned char   Reserved5;
    unsigned char   Reserved6;
    unsigned char   Reserved7;
} __attribute__((packed)) TIMER_AND_PROTECT_PAGE;

typedef struct{
    READ_WRITE_ERROR_RECOVERY_PAGE              ReadWriteErrorRecoveryPage;
    FLEXIBLE_DISK_PAGE                          FlexibleDiskPage;
    REMOVABLE_BLOCK_ACCESS_CAPABILITIES_PAGE    RemovableBlockAccessCapabilitiesPage;
    TIMER_AND_PROTECT_PAGE                      TimerAndProtectPage;
} __attribute__((packed)) MODE_ALL_PAGES;

typedef struct{
    MODE_PARAMETER_HEADER   ModeParameterHeader;
    union{
        READ_WRITE_ERROR_RECOVERY_PAGE              ReadWriteErrorRecoveryPage;
        FLEXIBLE_DISK_PAGE                          FlexibleDiskPage;
        REMOVABLE_BLOCK_ACCESS_CAPABILITIES_PAGE    RemovableBlockAccessCapabilitiesPage;
        TIMER_AND_PROTECT_PAGE                      TimerAndProtectPage;
        MODE_ALL_PAGES                              ModeAllPages;
    } __attribute__((packed)) ModePages;
} __attribute__((packed)) SCSI_MODE_SENSE_DATA;

/*
 * TEST UNIT READY
 */

typedef struct{
    unsigned char   OperationCode;
    unsigned char   Reserved1:5,
                    LogicalUnitNumber:3;
    unsigned char   Reserved2;
    unsigned char   Reserved3;
    unsigned char   Reserved4;
    unsigned char   Reserved5;
    unsigned char   Reserved6;
    unsigned char   Reserved7;
    unsigned char   Reserved8;
    unsigned char   Reserved9;
    unsigned char   Reserved10;
    unsigned char   Reserved11;
} __attribute__((packed)) SCSI_TEST_UNIT_READY_COMMAND;

/*
 * PREVENT-ALLOW MEDIUM REMOVAL
 */

typedef struct{
    unsigned char   OperationCode;
    unsigned char   Reserved1:5,
                    LogicalUnitNumber:3;
    unsigned char   Reserved2;
    unsigned char   Reserved3;
    unsigned char   Prevent:1,
                    Reserved4:7;
    unsigned char   Reserved5;
    unsigned char   Reserved6;
    unsigned char   Reserved7;
    unsigned char   Reserved8;
    unsigned char   Reserved9;
    unsigned char   Reserved10;
    unsigned char   Reserved11;
} __attribute__((packed)) SCSI_PREVENT_ALLOW_MEDIUM_REMOVAL_COMMAND;

/*
 * START-STOP UNIT
 */

typedef struct{
    unsigned char   OperationCode;
    unsigned char   IMMED:1,
                    Reserved1:4,
                    LogicalUnitNumber:3;
    unsigned char   Reserved2;
    unsigned char   Reserved3;
    unsigned char   Start:1,
                    LoEj:1,
                    Reserved4:6;
    unsigned char   Reserved5;
    unsigned char   Reserved6;
    unsigned char   Reserved7;
    unsigned char   Reserved8;
    unsigned char   Reserved9;
    unsigned char   Reserved10;
    unsigned char   Reserved11;
} __attribute__((packed)) SCSI_START_STOP_COMMAND;

/*
 * WRITE(10)
 */

typedef struct{
    unsigned char   OperationCode;
    unsigned char   RelAdr:1,
                    Reserved1:2,
                    FUA:1,
                    DPO:1,
                    LogicalUnitNumber:3;
    unsigned char   LogicalBlockAddress[4];
    unsigned char   Reserved2;
    unsigned char   TransferLength[2];
    unsigned char   Reserved3;
    unsigned char   Reserved4;
    unsigned char   Reserved5;
} __attribute__((packed)) SCSI_WRITE_10_COMMAND;

/*
 * VERIFY
 */

typedef struct{
    unsigned char   OperationCode;
    unsigned char   RelAdr:1,
                    ByteChk:1,
                    Reserved1:1,
                    Reserved2:1,
                    DPO:1,
                    LogicalUnitNumber:3;
    unsigned char   LogicalBlockAddress[4];
    unsigned char   Reserved3;
    unsigned char   VerificationLength[2];
    unsigned char   Reserved4;
    unsigned char   Reserved5;
    unsigned char   Reserved6;
} __attribute__((packed)) SCSI_VERIFY_COMMAND;

/******************************************************************************
** Global Function Prototype
******************************************************************************/

/* storage-fd.c */
void    storage_urb_send(struct usb_device_instance*, void*, int);

/* storageproto.c */
void    storageproto_urb_analysis(struct urb*);
int     storageproto_device_open_check(void);
void    storageproto_device_close(void);
void    storageproto_usb_status_check(int);
void    storageproto_media_status_check(int);
void    storageproto_usb_reset_ind(void);
ssize_t storageproto_proc_read(struct file*, char*, size_t, loff_t* pos);
void    storageproto_init(void);
void    storageproto_exit(void);

#endif  /* _STORAGEPROTO_H_ */

