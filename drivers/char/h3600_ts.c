/*
*
* Driver for the h3600 Touch Screen and other Atmel controlled devices.
*
* Copyright 2000 Compaq Computer Corporation.
*
* Use consistent with the GNU GPL is permitted,
* provided that this copyright notice is
* preserved in its entirety in all copies and derived works.
*
* COMPAQ COMPUTER CORPORATION MAKES NO WARRANTIES, EXPRESSED OR IMPLIED,
* AS TO THE USEFULNESS OR CORRECTNESS OF THIS CODE OR ITS
* FITNESS FOR ANY PARTICULAR PURPOSE.
*
* Author: Charles Flynn.
*
*/

#include <linux/module.h>
#include <linux/version.h>

#include <linux/init.h>
#include <linux/fs.h>
#include <linux/delay.h>
#include <linux/poll.h>
#include <asm/uaccess.h>        /* get_user,copy_to_user */
#include <linux/string.h>
#include <linux/interrupt.h>
#include <linux/sysctl.h>

#include <linux/tqueue.h>
#include <linux/sched.h>
#include <linux/pm.h>
/* SA1100 serial defines */
#include <asm/arch/hardware.h>
#include <asm/arch/serial_reg.h>
#include <asm/arch/irqs.h>

#include <linux/kbd_ll.h>
#include <linux/h3600_ts.h>
#include <linux/apm_bios.h>

#define _INLINE_ inline
/* Note this define only works if TXBUF_MAX is a power of 2 */
#define INCBUF(x,mod) ( (++x) & ( mod - 1) )
#define DECBUF(x,mod) ( (--x) & ( mod - 1) )

/* ++++++++++++local function defines ++++++++++++*/
static int checkDevice(struct inode *pInode);

static int initSerial( void );

/* These are the touch screen functions */
static int tsEvent(ID *);	/* event handler */
static TS_EVENT tsFilter(TS_EVENT);     /* event fitler */
static int tsRead(ID *);	/* file operations _read() */
static int tsReadRaw(ID *);	/* file operations _read() */
static int tsIoctl(void * , unsigned int , unsigned long );
static int tsInit(ID *);

static int keyEvent(ID *);	/* buttons event handler */
static int keyRead(ID *);	/* file operations _read() */

static int ackRes(ID *);	/* generic cmd Acknowledgement */
static int verRes(ID * );	/* Version response handler */
static int epromRdRes(ID * );   /* read eeprom response handler */
static int iicRdRes(ID * );     /* read iic response handler*/
static int battRdEvent(ID * );  /* read power/battery info */

/* ++++++++++++local function defines ++++++++++++*/


/* +++++++++++++globals ++++++++++++++++*/
int gMajor = 0;			/* TODO dynamic major for now */
/* This is the pointer to the UART receiving the ATMEL serial data */
volatile unsigned long *serial_port = (volatile unsigned long *)&Ser1UTCR0;


static VER_RET verRet;		/* The structure returned to the client */

static KEY_DEV keyDev;		/*  bit7=1=on bit7=0=off bits0:3=key num */
unsigned char keyRet;

static TS_DEV tsDev;
static TS_RET tsRet = {0,0,0,0};

static EEPROM_READ epromRdRet;

static BAT_DEV battRet;

static SPI_READ iicRdRet;	/* TODO change iicFuncs() to spiFuncs() */

static unsigned char flite_brightness=25; 
static unsigned char touchScreenSilenced = 0;
static int h3600_ts_ready = 0;

/*
	++++++++++++++ start of driver structures ++++++++++++++
	Each Atmel microcontroller device is represented in this array.
*/
static EVENT g_Events[MAX_MINOR] = {
        { processRead: tsRead, 	   processIoctl: tsIoctl, id: TOUCHS_ID },	/* TS minor = 0 */
        { processRead: tsReadRaw,  processIoctl: tsIoctl, id: TOUCHS_ID },	/* TSRAW minor = 1 */
        { processRead: keyRead,    id: KEYBD_ID }    		                /* KEY minor = 2 */
};

static ID g_Ids[MAX_ID] = {

        {
                0, (void *)&verRet, verRes,0,0,sizeof(VER_RET),0
        }, /*ID=0 VERSION*/

        { 0,0,0,0,0,0,0 },	/* ID=1 N/A */

        {	/* ID=2 keybd */
                (void *)&keyDev,(void *)&keyRet,keyEvent,0,sizeof(KEY_DEV),sizeof(keyRet),0
        },

        {	/* ID=3 Touch Screen */
                (void *)&tsDev,(void *)&tsRet,tsEvent,tsInit,sizeof(TS_DEV),sizeof(tsRet),0
        },

        {	/* ID=4 EEPROM READ */
                0, (void *)&epromRdRet,epromRdRes,0, 0,sizeof(epromRdRet),0
        },

        {	/* ID=5 EEPROM WRITE */
                0, 0,ackRes,0, 0,0,0
        },

        { 0,0,0,0,0,0,0 },	/* ID=6 N/A */
        { 0,0,0,0,0,0,0 },	/* ID=7 N/A */

        { 0, 0, ackRes ,0, 0,0,0 }, /* ID=8 LED */

        {	/* ID=9 Power/Battery Status */
                0, (void *)&battRet,battRdEvent,0, 0,sizeof(battRet),0
        },

        { 0,0,0,0,0,0,0 },	/* ID=10 N/A */

        {	/* ID=11 SPI Read TODO rename all iicXXXFuncs to spiXXXFuncts*/
                0, (void *)&iicRdRet,iicRdRes,0, 0,sizeof(iicRdRet),0
        },

        {	/* ID=12 SPI WRITE */
                0, 0,ackRes,0, 0,0,0
        },

        { 0, 0, ackRes ,0, 0,0,0 } /* ID=13 Front Light */

};

/* ++++++++++++++ end driver structures ++++++++++++++*/

static RXDEV rxdev;		/* receive ISR state */
static TXDEV txdev;		/* send */
/* +++++++++++++globals ++++++++++++++++*/


/* +++++++++++++File operations ++++++++++++++*/
static int h3600_fasync(int fd, struct file *filp, int mode);
static ssize_t h3600_read(struct file * , char * , size_t , loff_t * l);
static int h3600_open(struct inode * inode, struct file * filp);
static int h3600_release(struct inode * inode, struct file * filp);

int h3600_ioctl(struct inode * inode, struct file *filp, unsigned int cmd , unsigned long arg);
static unsigned int h3600_poll(struct file * filp, struct poll_table_struct * wait);

static void eventIsr(int , void *, struct pt_regs *);	/* ISR :*/

struct file_operations ts_fops = {
	read:           h3600_read,
        poll:           h3600_poll,
	ioctl:		h3600_ioctl,
        fasync:         h3600_fasync,
	open:		h3600_open,
	release:	h3600_release,
};

/*
	This function implements a simple block and wait mechanism
	instead of the interruptible_sleep_on_timeout()

	Return 0 for success 1 for timeout
*/
static _INLINE_ int wait_for_ack(ID * pid )
{
	unsigned int loops = 100;
	unsigned int esn = pid->sn+1;	/* expected sequence number */
	/* Block - wait on the response - should take ~1.5 m/s */
	while( (pid->sn != esn) && --loops )
		udelay(100);
	return ( pid->sn == esn ) ? 1 : 0;
}

/*
	sendBytes
*/
static _INLINE_ void sendBytes(const char * buff, size_t count,int id)
{
        unsigned char chksum;
        unsigned int space,head,tail;

        /* TODO TODO TODO (event << 4) compiles even though event is NOT defined */
        chksum = ( (unsigned char)((id << 4) | count ) );

        while(1)	/* this loop guarantees the payload is sent */
        {
                /* check if enough space - if not wait until the ISR frees some */
                head = txdev.head;
                tail = txdev.tail;
                /* size is the number of bytes waiting to be transmitted */
                space = ( head > tail ) ? head - tail : tail - head;
                space = TXBUF_MAX - space;
                if ( space >= ( count + FRAME_OVERHEAD ) )
                {
                        unsigned int cr3;
                        unsigned cnt = count;
                        txdev.buf[head]= (unsigned char)CHAR_SOF;
                        head=INCBUF(head,TXBUF_MAX);
                        txdev.buf[head]= chksum;
                        head=INCBUF(head,TXBUF_MAX);
                        while(cnt--)
                        {
                                txdev.buf[head]= *buff;	
                                head=INCBUF(head,TXBUF_MAX);
                                chksum += (unsigned char)(*buff++);
                        }
                        txdev.buf[head]=chksum;
                        txdev.head=INCBUF(head,TXBUF_MAX);
#if 0
                        {
			    unsigned i;
			    printk("\nsendBytes: head=%d tail=%d\n",
				txdev.head,txdev.tail);
			    for( i=tail ; i != txdev.head ; i = INCBUF(i,TXBUF_MAX) )
			    {
				printk("%d:%02x\n",i,txdev.buf[i]);
			    }
			}
#endif
                        /* if not enabled then enable Tx IRQs */
                        cr3 = serial_port[UTCR3];
                        if ( !( cr3 & UTCR3_TIE) )
                                serial_port[UTCR3] = cr3 | UTCR3_TIE;
                        break;	/* all done */	
                }
        }
}

/* +++++++++++++File operations ++++++++++++++*/

static int h3600_fasync(int fd, struct file *filp, int mode)
{
        /* TODO TODO put this data into file private data */
        int minor = checkDevice( filp->f_dentry->d_inode);
        if ( minor == - 1)
                return -ENODEV;
#if 0
        printk("fasync:called minor=%d\n",minor);
#endif

        return( fasync_helper(fd, filp, mode, &g_Events[minor].aq) );
}


int h3600_ioctl(struct inode * inode, struct file *filp,
                unsigned int cmd , unsigned long arg)
{
        unsigned char buf[28]; /* TODO optimal dimension for this buffer */
        ID * pid;
        int minor;
        unsigned rxcnt=0,txcnt=0,sn,id,offset=0;
        int status = 0;	/* TODO success */
	unsigned char tmp;


        minor = checkDevice( inode );
        if ( minor == - 1)
                return -ENODEV;

#if 0
        printk("h3600_ioctl: minor=%08x cmd=%d\n",minor,cmd);
#endif


        /*
	  We copy rxcnt bytes from the user. This buffer contains the
	  command sequence as well as storage for an (optional) response.

	  We transmit txcnt bytes starting at buf[XXX_CMD_OFFSET]

          We return 'pid->lenRet' bytes in 'pid->pReturn' to the user.

        */
        switch (cmd)
        {
	case LED_ON:
                rxcnt=txcnt=sizeof(LED_IN);
		id=NOTIFY_LED_ID;
                break;
	case GET_VERSION:
                id=VERSION_ID;
                break;
	case READ_EEPROM:
		txcnt=EEPROM_READ_CMD_LEN;  /*# of bytes sent to the Atmel */
		rxcnt=sizeof(READ_EEPROM);  /*# of bytes read from user */
		id=EEPROM_READ_ID;
                break;
#if defined EEPROM_WRITE_ENABLED
	case WRITE_EEPROM:
		rxcnt=sizeof(EEPROM_WRITE);
		offset=EEPROM_WRITE_CMD_OFFSET;
		id=EEPROM_WRITE_ID;
                break;
#endif
	case READ_SPI:
		txcnt=SPI_READ_CMD_LEN;
		rxcnt = sizeof(SPI_READ);
		offset=SPI_READ_CMD_OFFSET;
		id=SPI_READ_ID;
                break;
	case WRITE_SPI:
		rxcnt=sizeof(SPI_WRITE);
		offset=SPI_WRITE_CMD_OFFSET;
		id=SPI_WRITE_ID;
                break;
        case GET_THERMAL:
		id=THERMAL_ID;
		break;
        case GET_BATTERY_STATUS:
		id=BATTERY_ID;
		break;
	case FLITE_ON:
		rxcnt=txcnt=FLITE_CMD_LEN;
                id=FLITE_ID;
                break;
	default:
	{
                EVENT * pev = &g_Events[minor];
                ID * pid = &g_Ids[pev->id];
                printk("ts_ioctl: default minor=%d cmd=%d\n", minor, cmd);
                if( pev->processIoctl )
                        return ( (*pev->processIoctl)(pid->pdev,cmd,arg) );
		else
		{
			printk("Unknown IOCTL cmd=%d\n",cmd);
			return -EINVAL;
		}
                return 0;
	}
        }

	/* Copy rxcnt bytes from the user */
        pid = &g_Ids[id];
        if( rxcnt )
                __copy_from_user(buf, (char *)arg, rxcnt );

	/* Now check lengths and txcnts now we have read the user buffer */
        switch (cmd)
	{
        case READ_EEPROM:
		/* Check the length is within limits */
		if( ((EEPROM_READ *)buf)->len > EEPROM_RD_BUFSIZ )
			return -EIO;
		break;
        case WRITE_EEPROM:
		txcnt = ((EEPROM_WRITE *)buf)->len;
		if(txcnt > EEPROM_WR_BUFSIZ )
		{
		    printk("EEPROM WRITE bad len=%d\n",txcnt);
		    return -EIO;
		}
		txcnt <<= 1;	/* convert words to bytes */
		++txcnt;	/* adjust for 8bit addr field */		
		break;
        case READ_SPI:
		/* Check the length is within limits */
		if( ((SPI_READ *)buf)->len > SPI_RD_BUFSIZ )
			return -EIO;
		break;
        case WRITE_SPI:
		txcnt = ((SPI_WRITE *)buf)->len;
		if(txcnt > SPI_WR_BUFSIZ)
		{
		    printk("SPI WRITE bad len=%d\n",txcnt);
		    return -EIO;
		}
		txcnt +=2;		/* Add the 16bit addr field */
		break;
	case FLITE_ON:
		tmp = ((FLITE_IN *)buf)->brightness;
		tmp >>=3;	/* attenuate brightness to save power */ 
                flite_brightness = tmp;
		((FLITE_IN *)buf)->brightness = tmp;
		break;
        }

	/* Send txcnt bytes from calculated offset */
	sendBytes( buf + offset, txcnt, id );

        /* now wait on response */
        sn = pid->sn;
	rxcnt = pid->lenRet;	/* reuse this variable to return data to user*/	
        interruptible_sleep_on_timeout(&pid->wq, (2 * HZ) ); /*wait 2 secs*/
        if ( pid->sn == ( sn + 1 ) )
        {
                if( rxcnt)
                        __copy_to_user((char *)arg,(char *)pid->pReturn,rxcnt);
        }
        else
                status = -EIO;

        return status;
}

unsigned int h3600_flite_control(enum flite_pwr pwr, unsigned char brightness)
{
        if (h3600_ts_ready) {
                FLITE_IN req = { FLITE_MODE1, pwr, brightness >> 3 };

		/* Attenuate the brightness to save battery */
                flite_brightness = brightness >> 3;

                sendBytes( (char*)(char*)&req, sizeof(req), FLITE_ID);

		/* Block - wait on the response - should take ~1.5 m/s */
		if( !wait_for_ack(&g_Ids[FLITE_ID]) )
		{
			printk("h3600_flite_control: timeout\n");
			return 0;
		}
                return 1;
        } else {
                return 0;
        }
}

/*
 * h3600_flite_power: enables or disables power to frontlight, using last brightness setting.
 */
unsigned int h3600_flite_power(enum flite_pwr pwr)
{
        if (h3600_ts_ready) {
                FLITE_IN req = { FLITE_MODE1, pwr, ((pwr == FLITE_PWR_OFF) ? 0 : flite_brightness) };

                if (pwr == FLITE_PWR_OFF) 
                        touchScreenSilenced = 1;
                else
                        touchScreenSilenced = 0;

                if (0) printk("h3600_flite_power: pwr=%d brightness=%d sizeof(req)=%d\n", req.pwr, req.brightness, sizeof(FLITE_IN));

                sendBytes( (char*)&req, sizeof(req), FLITE_ID);
		
		/* Block - wait on the response - should take ~1.5 m/s */
		if( !wait_for_ack(&g_Ids[FLITE_ID]) )
		{
			printk("h3600_flite_power: timeout\n");
			return 1;
		}

                return 0;

        } else {
                return 1;
        }
}

#ifdef CONFIG_PM
static int suspended = 0;
static int h3600_ts_pm_callback(struct pm_dev *pm_dev, pm_request_t req, void *data)
{
        printk("  " __FUNCTION__ ": req=%d\n", req);
	switch (req) {
	case PM_SUSPEND: /* enter D1-D3 */
                suspended = 1;
                h3600_flite_power(FLITE_PWR_OFF);
                break;
	case PM_BLANK:
                if (!suspended) 
                        h3600_flite_power(FLITE_PWR_OFF);
		break;
	case PM_RESUME:  /* enter D0 */
	case PM_UNBLANK:
                if (suspended) {
                        initSerial();
                        suspended = 0;
                }
		h3600_flite_power(FLITE_PWR_ON);
                break;
	}
	return 0;
}
#endif

EXPORT_SYMBOL(h3600_apm_get_power_status);
unsigned int h3600_apm_get_power_status(u_char *ac_line_status,
                                        u_char *battery_status, u_char *battery_flag, u_char *battery_percentage, u_short *battery_life)
{
        if (h3600_ts_ready) {
                ID *pid = &g_Ids[BATTERY_ID];
                BAT_DEV *pbatt = (BAT_DEV *)pid->pReturn;
                int percentage;

                if (pbatt == NULL)
                        return 0;

                sendBytes( NULL, 0, BATTERY_ID);

		/* Block - wait on the response - should take ~1.5 m/s */
		if( !wait_for_ack( pid ) )
		{
			printk("h3600_flite_power: timeout\n");
			return 0;
		}


                if (0) printk(__FUNCTION__ ": status=%x raw_voltage=%d voltage=%dmV\n",
                              pbatt->batt1_status, 
                              pbatt->batt1_voltage,
                              (1000 * pbatt->batt1_voltage) / 228);
                /* now translate result into APM terms */
                if (ac_line_status != NULL) {
                        u_char stat = APM_AC_UNKNOWN;
                        switch (pbatt->ac_status) {
                        case H3600_AC_STATUS_AC_OFFLINE:
                                stat = APM_AC_OFFLINE;
                                break;
                        case H3600_AC_STATUS_AC_ONLINE:
                                stat = APM_AC_ONLINE;
                                break;
                        case H3600_AC_STATUS_AC_BACKUP:
                                stat = APM_AC_BACKUP;
                                break;
                        }
                        *ac_line_status = stat;
                }
                if (battery_status != NULL) {
                        u_char stat = APM_BATTERY_STATUS_UNKNOWN;
                        switch (pbatt->batt1_status) {
                        case H3600_BATT_STATUS_HIGH:
                                stat = APM_BATTERY_STATUS_HIGH;
                                break;
                        case H3600_BATT_STATUS_LOW:
                                stat = APM_BATTERY_STATUS_LOW;
                                break;
                        case H3600_BATT_STATUS_CRITICAL:
                                stat = APM_BATTERY_STATUS_CRITICAL;
                                break;
                        case H3600_BATT_STATUS_CHARGING:
                                stat = APM_BATTERY_STATUS_CHARGING;
                                break;
                        case H3600_BATT_STATUS_NOBATT:
                                stat = APM_BATTERY_STATUS_UNKNOWN;
                                break;

                        }
                }
                percentage = (425 * pbatt->batt1_voltage) / 1000 - 298;
                if (battery_percentage != NULL) {
                        *battery_percentage = percentage;
                }
                /* assuming C/5 discharge rate */
                if (battery_life != NULL) {
                        *battery_life = 300 * percentage / 100;
                }
                        
                return 1;
        } else {
                return 0;
        }
}

unsigned int h3600_microcontroller_request(int id, void *req, int len)
{
        if (h3600_ts_ready) {

                sendBytes( req, len, id);

		/* Block - wait on the response - should take ~1.5 m/s */
		if( !wait_for_ack(&g_Ids[id]) )
		{
			printk("h3600_microcontroller_request: timeout\n");
			return 1;
		}
		
                return 0;
        } else {
                return 1;
        }
}

unsigned int h3600_eeprom_read(unsigned char address, char *data, unsigned char len)
{
        int status;
        epromRdRet.addr = address; epromRdRet.len = len;
        status = h3600_microcontroller_request(EEPROM_READ_ID, (char*)&epromRdRet, len + 2);
        memcpy(data, &epromRdRet.buff, len);
        return 0;
}

/* TODO change this to h3600_spi_read */
unsigned int h3600_iic_read(unsigned char address, char *data, unsigned char len)
{
        int status;
        iicRdRet.addr = address; iicRdRet.len = len;
        printk(__FUNCTION__ ": calling microcontroller\n");
        status = h3600_microcontroller_request(SPI_READ_ID, (char*)&iicRdRet, len + 2);
        printk(__FUNCTION__ ": req.len=%d\n", iicRdRet.len);
        memcpy(data, &iicRdRet.buff, len);
        return 0;
}

EXPORT_SYMBOL(h3600_eeprom_read);
EXPORT_SYMBOL(h3600_iic_read);

/*
	Generic poll
*/
unsigned int h3600_poll(struct file * filp, struct poll_table_struct * wait)
{
        int minor;
        EVENT * pev;
        ID * pid;

        minor = checkDevice( filp->f_dentry->d_inode);
        if ( minor == - 1)
                return -ENODEV;

#if 0
        printk("h3600_poll:\n");
#endif

        pev = &g_Events[minor];
        pid = &g_Ids[pev->id];
        poll_wait(filp,&pid->wq,wait);
        return ( pev->head == pev->tail) ? 0 : (POLLIN | POLLRDNORM);
}

/*
	Generic read operation.	
*/

ssize_t h3600_read(struct file * filp, char * buf, size_t count, loff_t * l)
{
        int nonBlocking = filp->f_flags & O_NONBLOCK;
        int minor;
        EVENT * pev;
        ID * pid;


        minor = checkDevice( filp->f_dentry->d_inode );
        if ( minor == - 1)
                return -ENODEV;


        pev = &g_Events[minor];
        pid = &g_Ids[pev->id];

#if 0
        printk("h3600_read: minor=%d id=%d\n",minor,pev->id);
#endif

        if( nonBlocking )
        {
                if( ( pev->head != pev->tail ) && pev->processRead )
                {
                        int count;
                        count = (*pev->processRead)(pid);
                        /* returns length to be copied otherwise errno -Exxx */
                        if( count > 0 )
                                /* TODO TODO use __copy or copy ? */
                                __copy_to_user(buf,(char *)pid->pReturn,count );
                        return count;
                }
                else
                        return -EINVAL;

        }
        else
        {
                /* check , when woken, there is a complete event to read */
        retry:
                if( ( pev->head != pev->tail ) && pev->processRead )
                {
                        int rcount;
                        rcount = (*pev->processRead)(pid);
                        /* returns length to be copied otherwise errno -Exxx */
                        if( rcount > 0 )
                        {
                                __copy_to_user(buf,(char *)pid->pReturn,rcount );
                        }
                        return rcount;	/* TODO fmtLen if success -Exxx if error */
                }
                else
                {
                        interruptible_sleep_on(&pid->wq);
                        goto retry;
                }

	
        }
}

int h3600_open(struct inode * inode, struct file * filp)
{
        int minor;
        EVENT * pev;

        minor = checkDevice( inode );
        if ( minor == - 1)
                return -ENODEV;

#if 0
        printk("h3600_open:minor=%d\n",minor);
#endif
	MOD_INC_USE_COUNT;

        /* initialise event buffer */
        pev = &g_Events[minor];
        pev->tail = pev->head;

        return 0;
}

int h3600_release(struct inode * inode, struct file * filp)
{

#if 0
        printk("h3600_release:minor=%d\n",minor);
#endif
	MOD_DEC_USE_COUNT;

        return 0;
}



/*
	Returns the Minor number from the inode.
*/
static int checkDevice(struct inode *pInode)
{
	int minor;
	kdev_t dev = pInode->i_rdev;

	if( MAJOR(dev) != gMajor)
	{
		printk("checkDevice bad major = %d\n",MAJOR(dev) );
		return -1;
	}
	minor = MINOR(dev);

	if ( minor < MAX_MINOR )
		return minor;
	else
	{
		printk("checkDevice bad minor = %d\n",minor );
		return -1;
	}
}

/* +++++++++++++++++ Start of ISR code ++++++++++++++++++++++++++ */


/*
	Sets up serial port 1
	baud rate = 115k
	8 bits no parity 1 stop bit
*/

int initSerial( )
{
        unsigned int status;

        /* TODO registers read in 32bit chunks - need to do this for the sa1110? */

        /* Clean up CR3 for now */
        serial_port[UTCR3]= 0;

        /* 8 bits no parity 1 stop bit */
        serial_port[UTCR0] = 0x08;

        /* Set baud rate MS nibble to zero. */
        serial_port[UTCR1] = 0;

        /* Set baud rate LS nibble to 115K bits/sec */
        serial_port[UTCR2]=0x1;

        /*
	  SR0 has sticky bits, must write ones to them to clear.
          the only way you can clear a status reg is to write a '1' to clear.
        */
        serial_port[UTSR0]=0xff;

        /*
          enable rx fifo interrupt (SAM 11-116/7)
          If RIE=0 then sr0.RFS and sr0.RID are ignored ELSE
          if RIE=1 then whenever sro.RFS OR sr0.RID are high an
          interrupt will be generated.
          Also enable the transmitter and receiver.
        */
        status = UTCR3_TXE | UTCR3_RXE | UTCR3_RIE;
        serial_port[UTCR3]=status;


        /*
          initialize Serial channel protocol frame
        */
        rxdev.state = STATE_SOF;
        rxdev.event = rxdev.len = 0;
        memset(&rxdev.buf[0],0,sizeof(rxdev.buf));
        txdev.head = txdev.tail=0;
        return 0;	/* TODO how do we denote an error */

} // End of function initSerial

static  _INLINE_ void ClearInterrupt(void)
{
	/* SR0 has sticky bits, must write ones to them to clear */
	serial_port[UTSR0]=0xff;
}


/*
	This function takes a single byte and detects the frame
*/
static _INLINE_ void processChar(unsigned char rxchar)
{
        switch ( rxdev.state )
        {
		/*
                  We are looking for the SOF in this state.
		*/
        case STATE_SOF:
                if ( rxchar == CHAR_SOF )
                {
				/* found it - next byte is the id and len */
                        rxdev.state=STATE_ID;
                }
                break;
		/*
                  We now expect the id and len byte
		*/
        case STATE_ID:
                rxdev.event = (rxchar & 0xf0) >> 4 ;
                rxdev.len = (rxchar & 0x0f);
                rxdev.idx=0;
                if (rxdev.event >= MAX_ID )
                {
				/* increment error counter 1 */
                        printk("Error1\n");
                        ++rxdev.counter[0];
                        rxdev.state = STATE_SOF;
                        break;
                }
                rxdev.chksum = rxchar;
                rxdev.state=( rxdev.len > 0 ) ? STATE_DATA : STATE_EOF;
                break;
		/*
                  We now expect 'len' data bytes.
		*/
        case STATE_DATA:
                rxdev.chksum += rxchar;
                rxdev.buf[rxdev.idx]= rxchar;
                if( ++rxdev.idx == rxdev.len )
                {
				/* look for the EOF char next */
                        rxdev.state = STATE_EOF;
                }
                break;
		/*
                  We now expect to find the STATE_EOF
                  TODO TODO not sure if this is an STATE_EOF marker
                  or simply the checksum byte
		*/
        case STATE_EOF:
                rxdev.state = STATE_SOF;
                if (rxchar == CHAR_EOF || rxchar == rxdev.chksum )
                {
                        ID * pid = &g_Ids[(unsigned)rxdev.event];
                        if( pid->processEvent )
                        {
                                /*printk(" [%d]\n",(unsigned)rxdev.event);*/
                                if ( (*pid->processEvent)(pid) == 0 )
                                        wake_up_interruptible(&pid->wq);
                        }
                }
                else
                {
                        ++rxdev.counter[1];
                        printk("\nbadFrame: event=%d ", rxdev.event);
                }
                break;
        default:
                ++rxdev.counter[2];
                printk("Error3\n");
                break;

        }	/* end switch */

}  /* end of in-line function */


/*

	Frame format
  byte	  1	  2		  3 		 len + 4
	+-------+---------------+---------------+--=------------+
	|SOF	|id	|len	| len bytes	| Chksum	|
	+-------+---------------+---------------+--=------------+
  bit	0     7  8    11 12   15 16

	+-------+---------------+-------+
	|SOF	|id	|0	|Chksum	| - Note Chksum does not include SOF
	+-------+---------------+-------+
  bit	0     7  8    11 12   15 16

*/

static void eventIsr(int irq, void *dev_id, struct pt_regs *regs)
{
        unsigned status0;	/* UTSR0 */
        int rxval;

        /* TODO check if we need to share */
        if ( dev_id != eventIsr)
                goto exit;

        /* ascertain reason for interrupt and handle accordingly */
        status0 = serial_port[UTSR0];
        if( (status0 & UTSR0_RID) || (status0 & UTSR0_RFS) )
        {
                /* Rx idle or RxFIFO above high water mark and needs service */
                unsigned status1;
#if 0
                printk("IRQ Rx - ");
#endif
                do
                {
                        /* process all bytes in Rx FIFO */
                        rxval = serial_port[UART_RX];
                        if (!(rxval & 0x700)) 
                        {
#ifdef DEBUG
                                printk("%02x",(unsigned char)rxval );
#else
                                processChar( (unsigned char)rxval);
#endif
                        }
                        else
                        {
                                /* found parity,frame or overrun error */
                                printk("Bad Char\n");
                        }

                        status1 = serial_port[UTSR1];
	
                } while (status1 & UTSR1_RNE );
#ifdef DEBUG
                printk("\n");
#endif

        }
        else if ( status0 & UTSR0_TFS )
        {
                unsigned int cr3;
                unsigned int tail=txdev.tail;
                unsigned int head=txdev.head;
                /* Tx FIFO below low water mark and needs replenished */
#ifdef DEBUG
                printk("IRQ Tx - ");
#endif
                while( serial_port[UTSR1] & UTSR1_TNF )
                {
                        /* load another char into the Tx FIFO */
                        serial_port[UART_TX] = txdev.buf[tail];
#ifdef DEBUG
                        printk("%02x",txdev.buf[tail]);
#endif
                        if ( (tail=INCBUF(tail,TXBUF_MAX) ) == head )
                        {
                                /* nothing more to Tx - disable interrupts */
                                cr3 = serial_port[UTCR3] & ~UTCR3_TIE;
                                serial_port[UTCR3] = cr3;
                                /* printk(" TxIRQ off head=%d tail=%d\n",head,tail); */
                                break;
                        }
                }
                txdev.tail=tail;
#ifdef DEBUG
                printk("\n");
#endif
	
        }
        if ( status0 & UTSR0_EIF )
        {
                /* Detected a Parity,Frame or Overrun error */
                printk("IRQ EIF\n");
        }
        if ( (status0 & UTSR0_RBB) || (status0 & UTSR0_REB) ) 
        {
                /* must be a Begin/End of break that caused IRQ */
                printk("IRQ RBB | REB\n");
        }


 exit:

        ClearInterrupt();

}	//end of function

static void action_button_handler(int irq, void *dev_id, struct pt_regs *regs)
{
        int scancode = H3600_SCANCODE_ACTION;
        int down = (GPLR & GPIO_BITSY_ACTION_BUTTON) ? 0 : 1;
        if (dev_id != action_button_handler)
                return;
           
        if (0) printk(__FUNCTION__ ": scancode=%d down=%d\n", scancode, down);
        handle_scancode(scancode, down);

}

static int suspend_button_pushed = 0;
static void suspend_button_task_handler(void *data) 
{
        extern void pm_do_suspend(void);
        udelay(200); /* debounce */
        pm_do_suspend();
        suspend_button_pushed = 0;
}

static struct tq_struct suspend_button_task = {
        routine: suspend_button_task_handler
};

static int suspend_button_mode = PBM_SUSPEND;
static void npower_button_handler(int irq, void *dev_id, struct pt_regs *regs)
{
        int scancode = H3600_SCANCODE_SUSPEND;
        int down = (GPLR & GPIO_BITSY_NPOWER_BUTTON) ? 0 : 1;
        if (dev_id != npower_button_handler)
                return;

        if (suspend_button_mode == PBM_GENERATE_KEYPRESS) {
                if (0) printk(__FUNCTION__ ": scancode=%d down=%d\n", scancode, down);
                handle_scancode(scancode, down);
        } else {
                if (!suspend_button_pushed) {
                        suspend_button_pushed = 1;
                        schedule_task(&suspend_button_task);
                }
        }
}


static unsigned char scancodes[] = {
        0, /* unused */
        H3600_SCANCODE_RECORD, /* 1 -> record button */
        H3600_SCANCODE_CALENDAR, /* 2 -> calendar */
        H3600_SCANCODE_CONTACTS, /* 3 -> contact */
        H3600_SCANCODE_Q, /* 4 -> Q button */
        H3600_SCANCODE_START, /* 5 -> start menu */
        H3600_SCANCODE_UP, /* 6 -> up */
        H3600_SCANCODE_RIGHT, /* 7 -> right */
        H3600_SCANCODE_LEFT, /* 8 -> left */
        H3600_SCANCODE_DOWN, /* 9 -> down */
};

/* +++++++++++++++++ end of ISR code ++++++++++++++++++++++++++ */
/****************** start of device handlers section ***********************/

/*
	Buttons - returned as a single byte
	7 6 5 4 3 2 1 0
	S x x x N N N N

	S	switch state ( 0=pressed 1=released)
	x	Unused.
	NNNN	switch number 0-15

*/
static int keyEvent(ID * pDev )		/* touch screen event handler */
{
        KEY_DEV * pKeyDev = (KEY_DEV *)pDev->pdev;
        EVENT * pev = &g_Events[KEY_MINOR];
        unsigned char * pKey = &pKeyDev->buf[pev->head];

        *pKey=rxdev.buf[0];	/* store current key state */

        pev->head=INCBUF(pev->head,MAX_KEY_EVENTS );
        /* Send an interrupt (SIGIO) to the client's signal handler */
        if (pev->aq)
                kill_fasync(&pev->aq, SIGIO, POLL_IN);

        { 
           unsigned char key = *pKey;
           unsigned char scancode = scancodes[key&KEY_NUM];
           unsigned char down = (key&KEY_RELEASED) ? 0 : 1;

           if (0) printk(__FUNCTION__ ": key=%#x scancode=%d down=%d\n", key, scancode, down);
           handle_scancode(scancode, down);
        }
        return 0;

}

static int keyRead(ID * pDev)
{
        /* generic line for all readers */
        KEY_DEV * pKeyDev = (KEY_DEV *)pDev->pdev;
        EVENT * pev = &g_Events[KEY_MINOR];
        unsigned char * pKey = &pKeyDev->buf[pev->tail];
        keyRet = *pKey;
        pev->tail=INCBUF(pev->tail,MAX_KEY_EVENTS );
        return 1;	/* TODO make this a sizeof() */
}


/*	ID=3 TOUCHSCREEN EVENTS
	TouchScreen event data is formatted as shown below:-

	+-------+-------+-------+-------+
	| Xmsb	| Xlsb	| Ymsb	| Ylsb	|
	+-------+-------+-------+-------+
	byte 0	  1	  2	  3

*/

static int tsEvent(ID * pDev )		/* touch screen event handler */
{
        if (!touchScreenSilenced) {
                unsigned head;
                /* This is where we will store the event */
                EVENT * pev = &g_Events[TS_MINOR];
                EVENT * pevraw = &g_Events[TSRAW_MINOR];
                TS_DEV * pTsDev = ( TS_DEV * )pDev->pdev;
                TS_EVENT * pEvent;
                TS_EVENT * pFilteredEvent;

                head = pev->head;
                pEvent = &pTsDev->events[head];
                pFilteredEvent = &pTsDev->filtered_events[head];

                if( rxdev.len == TS_DATA_LEN )
                {
                        /*
                          There are 3 sub-events within the TS_EVENT
                          PEN_UP		- user has just lifted pen off screen.
                          PEN_FLEETING	- user has just put pen DOWN for the first time.
                          - we ignore this event.
                          PEN_DOWN	- this is a true sample.
                        */
                        switch(pTsDev->penStatus)
                        {
                        case PEN_FLEETING:
                                /* TODO how many fleeting before we pronounce a PEN_DOWN? */
                                pTsDev->penStatus = PEN_DOWN;
                                /* OK to use this data -  pass thru to next case*/
                        case PEN_DOWN:
                        {
                                /* store raw x,y & pressure to ts_dev.event[head] */
                                unsigned  short x,y;
                                /* Extract RAW coords from event data  */
                                /* TODO TODO check this!! */
                                x = rxdev.buf[0]; x <<= 8; x += rxdev.buf[1];
                                y = rxdev.buf[2]; y <<= 8; y += rxdev.buf[3];
                                /* dont calibrate it until the user asks for it */
                                pEvent->x = x;
                                pEvent->y = y;
                                pEvent->pressure = 1;
                                *pFilteredEvent = tsFilter(*pEvent);
                                /* increment heads */
                                pev->head = INCBUF(head,MAX_TS_EVENTS);
                                pevraw->head=pev->head;
#if 0
                                printk("PEN_DOWN: x=%d y=%d p=%d ++h=%d:%d\n",
                                       pEvent->x,pEvent->y,
                                       pEvent->pressure,pev->head,pevraw->head);
#endif
                        }
                        break;
                        case PEN_UP:
                                /* Pen was previously UP dont trust this sample */
                                pTsDev->penStatus = PEN_FLEETING;
#if 0
                                printk("PEN_FLEETING\n");
#endif
                                return 1; 
                        default:
                                pTsDev->penStatus = PEN_UP;
                                printk("PEN_UNKNOWN\n");
                                return -1;
                        }

                }
                else if (rxdev.len == 0)
                {
                        /* PEN_UP - since we have no data in the frame */
                        pTsDev->penStatus = PEN_UP;
                        pEvent->x = pEvent->y = pEvent->pressure = 0; 
                        *pFilteredEvent = tsFilter(*pEvent);
                        /* increment heads */
                        pev->head = INCBUF(head,MAX_TS_EVENTS);
                        pevraw->head=pev->head;
#if 0
                        printk("PEN_UP head=%d\n",pev->head);
#endif
                }
                else
                {
                        /* we have a length over or under run */
                        printk("LENGTH_UNKNOWN\n");
                        return -1;
                }

                /* There is an event to report - Async notification */
                if (pev->aq)
                        kill_fasync(&pev->aq, SIGIO, POLL_IN);
                if (pevraw->aq)
                        kill_fasync(&pevraw->aq, SIGIO, POLL_IN);

                return 0;	/* event to report */

        }
	else
	    return -1;	/* return error if disabled */

} /* end of tsEvent() */


/* 
        a function that will return a filtered/normalized co-ordinate
        based on the history of previous co-ordinates. 

        SHOULD ONLY BE USED FROM tsEvent.  Currently can only be used over
        one set of points as needs to keep history.  If used from two places
        it will be unpredictable.  

        Avg the value,
        Check for sudden jumps (slightly ruined by the avg)
        check for jittering (don't allow very slight movement)
        return average value.

        return 0 all if don't have enough information for filtering.
*/
#define DEFAULT_JITTER_DIST_SQR         2
#define DEFAULT_JUMP_DIST_SQR        3000
#define FLTR_BUFFER_MAX_SIZE_LOG2 	6
#define FLTR_BUFFER_MAX_SIZE    	(1 << FLTR_BUFFER_MAX_SIZE_LOG2)
#define DEFAULT_FLTR_BUFFER_SIZE_LOG2   3
#define DEFAULT_FLTR_BUFFER_SIZE    	(1 << DEFAULT_FLTR_BUFFER_SIZE_LOG2)

static int jitter_dist_sqr = DEFAULT_JITTER_DIST_SQR;
static int jump_dist_sqr = DEFAULT_JUMP_DIST_SQR;
static int fltr_buffer_size = DEFAULT_FLTR_BUFFER_SIZE;
static int fltr_buffer_size_log2 = DEFAULT_FLTR_BUFFER_SIZE_LOG2;

static TS_EVENT tsFilter(TS_EVENT raw) { 
        TS_EVENT return_val;
        static unsigned int count=0;
        static unsigned int xprev=0;
        static unsigned int yprev=0;	
        static unsigned int buffer_x[FLTR_BUFFER_MAX_SIZE];
        static unsigned int buffer_y[FLTR_BUFFER_MAX_SIZE];
        static unsigned int avg_x=0;
        static unsigned int avg_y=0;
        static int b_pos = 0;
        static unsigned int xprev_raw=0;
        static unsigned int yprev_raw=0;	
        static signed int xlast_diff=0;	
        static signed int ylast_diff=0;	

        fltr_buffer_size = (1 << fltr_buffer_size_log2);

        return_val = raw; 

        buffer_x[b_pos]= raw.x;
        avg_x += raw.x;
        buffer_y[b_pos]= raw.y;
        avg_y += raw.y;
        b_pos = INCBUF(b_pos, fltr_buffer_size);

        if (raw.pressure != 0) {
                int dx;
                int dy;

                switch(count) {
                case 0: 
                        /* first position */
                        count++;
                        /* store xprev, and yprev, but return as if 
                         * new position */
                        xprev_raw = raw.x;
                        yprev_raw = raw.y;
                        return_val.x = return_val.y 
                                = return_val.pressure = 0;
                        break;
                case 1:
                        /* second position */
                        count++;
                        xlast_diff = raw.x - xprev_raw;
                        ylast_diff = raw.y - yprev_raw;
                        xprev_raw = raw.x;
                        yprev_raw = raw.y;
                        return_val.x = return_val.y 
                                = return_val.pressure = 0;
                        break; 
                default:
                        if ((count+1) >= fltr_buffer_size) {
                                /* added more than 4, need to 
                                 * take one off */
                                return_val.x = avg_x>>fltr_buffer_size_log2;
                                return_val.y = avg_y>>fltr_buffer_size_log2;
                                avg_x -= buffer_x[b_pos];
                                avg_y -= buffer_y[b_pos];
                        } else {
                                count++;
                                xlast_diff = raw.x - xprev_raw;
                                ylast_diff = raw.y - yprev_raw;
                                xprev_raw = raw.x;
                                yprev_raw = raw.y; 
                                return_val.x = return_val.y = 
                                        return_val.pressure = 0;
                                break;
                        }
                        /* third position or later */
                        dx = raw.x - xprev_raw;
                        dy = raw.y - yprev_raw;
                        if ((((dx - xlast_diff)*(dx - xlast_diff)) +
                             ((dy - ylast_diff)*(dy - ylast_diff)))
                                        > jump_dist_sqr) { 
                                /* possible jump, lets assume so, 
                                 * fix by setting pen up.  Also, 
                                 * so can continue, need to rexet 
                                 * trap info, so reset count as well */
                                return_val.x = return_val.y = 
                                        return_val.pressure = 0;
                                count = 0;
                                avg_x = avg_y = 0;
                                break; 
                        }
                        xlast_diff = dx;
                        ylast_diff = dy;
                        xprev_raw = raw.x;
                        yprev_raw = raw.y;
                        /* stable over last three positions, 
                         * can process this */
                        dx = return_val.x - xprev;
                        dy = return_val.y - yprev;
      
                        /* process Jitter filter */
                        if(((dx*dx) + (dy*dy)) > jitter_dist_sqr) {
                                xprev=return_val.x;
                                yprev=return_val.y;
                        } else {
                                return_val.x = xprev;
                                return_val.y = yprev;
                        } 
                }
        } else { 
                /* pen up, reset data, return_val already = raw */
                count = 0; 
                avg_x = avg_y = 0;
        }
        return(return_val);
}

/*
	Read() function for the touch screen.
	IN:  raw data
	OUT: Calibrated data pointed to by pReturn.
        as part of calibration, may filter out 'noise' and report
        up if not enough data to give a 'good' value

        Works by looking back in buffer for any changes in possiton
        greater than magic number X. in this case the magic number 
        is 81.  I must stress that this is the first test of this filter
        and it will be refined as tested.  Mostly it will be made to be 
        less ugly, better code etc.
*/
static int tsRead(ID * pDev)
{
	/* generic line for all readers */
        TS_DEV * pTsDev = (TS_DEV *)pDev->pdev;
        EVENT * pev = &g_Events[TS_MINOR];
        TS_EVENT filtered = pTsDev->filtered_events[pev->tail];
        TS_RET * fmt = (TS_RET *)pDev->pReturn;

        if (filtered.pressure != 0) { 
                fmt->x=((pTsDev->cal.xscale*filtered.x)>>8)+pTsDev->cal.xtrans;
                fmt->y=((pTsDev->cal.yscale*filtered.y)>>8)+pTsDev->cal.ytrans;
                fmt->pressure = 1;
        } else {
                /* pen really up */
                fmt->x = fmt->y = fmt->pressure = 0;
        }

        /* TODO there may be other stuff to copy to the return buffer */
        pev->tail=INCBUF(pev->tail,MAX_TS_EVENTS);
#if 0
        printk("\ntsRead: h=%d ++t=%d x=%d y=%d p=%d\n",
               pDev->head,pDev->tail,fmt->x,fmt->y,fmt->pressure);
#endif

        return sizeof(TS_RET);		/* success */
}

/*
	RAW Touch screen data - used by the calibration utility.
	Read() function for the touch screen.
	IN:  raw data
	OUT: Calibrated data pointed to by pReturn.
*/
static int tsReadRaw(ID * pDev)
{
	/* generic line for all readers */
        TS_DEV * pTsDev = (TS_DEV *)pDev->pdev;
        EVENT * pev = &g_Events[TSRAW_MINOR];
        TS_EVENT raw = pTsDev->events[pev->tail];
        TS_RET * ret = (TS_RET *)pDev->pReturn;

#if 0
	printk("tsReadRaw:rawX=%d rawY=%d\n", raw.x,raw.y );
#endif
	/* Raw data including PEN_UP */
        ret->x = raw.x;
        ret->y = raw.y;
        ret->pressure = raw.pressure;

        /* TODO there may be other stuff to copy to the return buffer */

        pev->tail=INCBUF(pev->tail,MAX_TS_EVENTS);

#if 0
        printk("\ntsReadRaw:h=%d ++t=%d p=%d\n",pev->head,pev->tail,ret->pressure);
#endif

        return sizeof(TS_RET);		/* success */
}
/*
	ts_ioctl - ioctl function for touch screen device
*/
static int tsIoctl( void * pDevice, unsigned int cmd, unsigned long arg)
{
        TS_DEV * pdev = (TS_DEV *)pDevice;

        printk("tsIoctl: cmd %04x pdev=%p\n",cmd,pdev);

        switch(cmd)
        {
	case TS_GET_RATE:	/* TODO TODO */
                return pdev->rate;
                break;
	case TS_SET_RATE:	/* TODO TODO */
                pdev->rate = arg;
                break;
	case TS_GET_CAL:
                printk("TS_GET_CAL\n");
                __copy_to_user((char *)arg, (char *)&pdev->cal, sizeof(TS_CAL) );
                break;
	case TS_SET_CAL:
                printk("\n\nTS_SET_CAL: ");
                __copy_from_user( (char *)&pdev->cal,(char *)arg ,sizeof(TS_CAL) );
	    
                printk("xscale=%d xtrans=%d yscale=%d ytrans=%d\n\n",
                       pdev->cal.xscale,pdev->cal.xtrans,
                       pdev->cal.yscale,pdev->cal.ytrans);
                break;
	default:
                printk("IOCTL: unknowncmd %04x\n",cmd);
                return -EINVAL;
        }

        return 0;
}

static int tsInit(ID * pDev)
{
        TS_DEV * pTsDev = (TS_DEV *)pDev->pdev;

        /* TODO this is zero'd at init_module - need to do it again? */

        pTsDev->penStatus=PEN_DOWN;
        /* default calibration */
        pTsDev->cal.xscale = -93;
        pTsDev->cal.xtrans = 346;
        pTsDev->cal.yscale = -64;
        pTsDev->cal.ytrans = 251;

        return 0;
}

/* 
	Stub functions 
*/
#if 0
static int xxInit(ID * pdev )
{
	printk("xxInit: not implemented\n");
	return 0;
}

#endif

/*
	generic response handler - assumes no data - simply an Ack
	All it does is increment a MOD255 sequence number.
*/
static int ackRes(ID * pDev )
{
        ++pDev->sn;
#if 0
        printk("ackRes sn=%d\n",pDev->sn);
#endif
        return 0;
}

/*
	Response handler for GET version number.
	Ver Number returned as 4 bytes XX.YY
*/
static int verRes(ID * pDev )
{
        VER_RET * pVer = (VER_RET *)pDev->pReturn;
	unsigned int len = rxdev.len;
	switch(len)
	{
	    case 4:
		memcpy(pVer->host_version,rxdev.buf,4);
		pVer->host_version[4]=0;
		pVer->pack_version[0]=0;
		break;
	    case 9:
		memcpy(pVer->host_version,rxdev.buf,4);
		pVer->host_version[4]=0;
		memcpy(pVer->pack_version,&rxdev.buf[4],4);
		pVer->pack_version[4]=0;
		pVer->boot_type=rxdev.buf[8];
		break;
	    default:
		printk("verRes Unknown len = %d\n",len);
		break;
	}
#if 0
    {
	unsigned i;
	printk("ver=%s option_pack_ver=%s boot_type=%02x\n",
		pVer->host_version,pVer->pack_version,pVer->boot_type);
	for(i=0; i < rxdev.len; i++ )
        	printk("%02x:",rxdev.buf[i]);
	printk("\n");
    }
#endif
    ++pDev->sn;
    return 0;
}

static int epromRdRes(ID * pDev)
{
    EEPROM_READ *pER = (EEPROM_READ *)pDev->pReturn;
    unsigned int len = rxdev.len >> 1;	/* len = len/2 */
    unsigned int i,j;
    unsigned short x;

    if( len > EEPROM_RD_BUFSIZ )
    {
	printk("WARNING epromRdRes bad length = %d\n",len);
	return -1;
    }

    for( i=0,j=0 ; i < len ; i++, j++)
    {
	x = rxdev.buf[j];
	x <<= 8;
	x += rxdev.buf[++j];
#if 0
	printk("epromRdRes: i=%d,j=%d,x=%04x\n",i,j,x);
#endif
	pER->buff[i]=x;
    }

    pER->len=len;
    ++pDev->sn;
    return 0;

}


static int iicRdRes(ID * pDev)
{
    SPI_READ *pER = (SPI_READ *)pDev->pReturn;
    unsigned int i,len;

    len = rxdev.len;

    for( i=0 ; i < len ; i++)
    {
#if 0
	printk("iicRdRes: %02x\n",rxdev.buf[i]);
#endif
	pER->buff[i]=rxdev.buf[i];
    }

    pER->len=len;
    ++pDev->sn;
    return 0;
}


static int battRdEvent(ID * pDev)
{
        BAT_DEV *pbattinfo = (BAT_DEV *)pDev->pReturn;
        {
#if 0
                unsigned i;
                printk(__FUNCTION__ ": len=%d\n",rxdev.len);
                for (i=0 ; i < rxdev.len ; i++ )
                        printk("%02x",rxdev.buf[i]);
                printk("\n\n");
#endif

                pbattinfo->ac_status = rxdev.buf[0];
                pbattinfo->batt1_chemistry = rxdev.buf[1];
                pbattinfo->batt1_voltage = ((rxdev.buf[3]<<8)| rxdev.buf[2]);
                pbattinfo->batt1_status = rxdev.buf[4];                
        }

        ++pDev->sn;
        return 0;
}




static struct ctl_table h3600_ts_table[] = 
{
	{1, "suspend_button_mode", &suspend_button_mode, sizeof(int), 0666, NULL, &proc_dointvec},
	{2, "calibration", &tsDev.cal, sizeof(tsDev.cal), 0600, NULL, &proc_dointvec },
        {3, "jitter_dist_sqr", &jitter_dist_sqr, sizeof(int), 0600, NULL, &proc_dointvec },
        {4, "jump_dist_sqr", &jump_dist_sqr, sizeof(int), 0600, NULL, &proc_dointvec },
        {3, "fltr_buffer_size_log2", data: &fltr_buffer_size_log2, maxlen: sizeof(int), mode: 0600, proc_handler: &proc_dointvec_minmax, 
         extra1: (void*)0, extra2: (void*)FLTR_BUFFER_MAX_SIZE_LOG2 },
	{0}
};

static struct ctl_table h3600_ts_dir_table[] =
{
	{11, "ts", NULL, 0, 0555, h3600_ts_table},
	{0}
};

static struct ctl_table_header *h3600_ts_sysctl_header = NULL;



/* int __init h3600_init(void) */
int __init h3600_init_module(void)
{
	/* TODO TODO what happens if there is an error - see pdriver */
	int result;
	unsigned i;
	ID * pid;

        /* register our character device */
        printk(__FUNCTION__ ":init_module registering char device\n");
        result = register_chrdev(0, MOD_NAME, &ts_fops);
        if (result < 0) {
                printk(__FUNCTION__ ": can't get major number\n");
                return result;
        }

	if( gMajor == 0 )
		gMajor = result;

	initSerial();		/* do this first before irq management */
	
	/* Now register our IRQ - ID must be unique if sharing an IRQ line*/
	/* TODO TODO IRQ not shared tale out */

	result = request_irq(TS_IRQ, eventIsr, SA_SHIRQ | SA_INTERRUPT ,
                             "h3600_ts", eventIsr);

        set_GPIO_IRQ_edge( GPIO_BITSY_ACTION_BUTTON, GPIO_BOTH_EDGES );
        set_GPIO_IRQ_edge( GPIO_BITSY_NPOWER_BUTTON, GPIO_RISING_EDGE );

	result |= request_irq(IRQ_GPIO_BITSY_ACTION_BUTTON, action_button_handler, SA_SHIRQ | SA_INTERRUPT | SA_SAMPLE_RANDOM,
                              "h3600_action", action_button_handler);
	result |= request_irq(IRQ_GPIO_BITSY_NPOWER_BUTTON, npower_button_handler, SA_SHIRQ | SA_INTERRUPT | SA_SAMPLE_RANDOM,
                              "h3600_suspend", npower_button_handler);

	if (!result)
	{	
                printk("init_module successful init major= %d irq=%d\n",
                       gMajor,TS_IRQ);

                for(i=0 ; i < MAX_ID ; i++)
                {
                        /* Zero device specific structures */
                        pid = &g_Ids[i];
                        pid->sn=0;
                        memset(pid->pdev,0,pid->lenDev);		
                        init_waitqueue_head(&pid->wq);
                        memset(pid->pReturn,0,pid->lenRet);		
                        if( pid->initDev )
                                (*pid->initDev)( pid );
                }
		h3600_flite_control(1, 25);	/* default brightness */
#ifdef CONFIG_PM
                pm_register(PM_ILLUMINATION_DEV, PM_SYS_LIGHT, h3600_ts_pm_callback);
                printk(__FUNCTION__ " registered pm callback=%p\n", h3600_ts_pm_callback);
#endif
                h3600_ts_sysctl_header = register_sysctl_table(h3600_ts_dir_table, 0);
                h3600_ts_ready = 1;
	}
	else
		printk("init_module error from irq %d\n", result);
	return 0;
}

void h3600_cleanup_module(void)
{
	printk("cleanup_module:\n");

        flush_scheduled_tasks();
        unregister_sysctl_table(h3600_ts_sysctl_header);
	free_irq(TS_IRQ, eventIsr);
	free_irq(IRQ_GPIO_BITSY_ACTION_BUTTON, action_button_handler);
	free_irq(IRQ_GPIO_BITSY_NPOWER_BUTTON, npower_button_handler);
#ifdef CONFIG_PM
	pm_unregister_all(h3600_ts_pm_callback);
#endif
	unregister_chrdev(gMajor, MOD_NAME);
}

module_init(h3600_init_module);
module_exit(h3600_cleanup_module);
