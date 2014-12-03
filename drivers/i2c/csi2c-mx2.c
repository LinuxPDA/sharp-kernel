/*
 * linux/drivers/i2c/csi2c-mx2.c
 *
 * Copyright (C) 2003 Lineo Solutions, Inc.
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file "COPYING" in the main directory of this archive
 * for more details.
 * 
 * Based on:
 * Motorola i.MX21ADS BSP elinux v0.2
 * mx21_rel_0.2/Drivers/iMagic/csi2c.c
 */
/*
	For Smart Sense iMagic IM8803 VGA
*/
#include <linux/config.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/compatmac.h>
#include <linux/hdreg.h>
#include <linux/vmalloc.h>
#include <linux/fs.h>
#include <linux/module.h>
#include <linux/blkpg.h>
//#include <asm/arch/hardware.h>
#include <linux/i2c.h>
#include <linux/i2c-algo-bit.h>
#include <linux/i2c-id.h>
#include <linux/slab.h>
#include <asm/io.h>
//#include <asm/proc-armv/cache.h>
#include <linux/mm.h>
#include <linux/wrapper.h>
#include <asm/dma.h>
#include <linux/miscdevice.h>
#include <linux/proc_fs.h>
#include <linux/delay.h>

#include "csi2c-mx2.h"
#include "asm/arch/mx2.h"
#include "asm/arch/pll.h"
#include "asm/arch/platform.h"
//#include "asm/arch/dma.h"
#include "asm/dma.h"

//#define DEBUG_CSI2C

#ifdef DEBUG_CSI2C
#define dprintcsi2c(str...) printk("<"__FUNCTION__"> "str)
#else
#define dprintcsi2c(str...)	// nothing
#endif

#define BUFFER_SIZE    (PAGE_SIZE<<7)
#define IRQ_FIFO_FULL  0x00040000
#define IRQ_SOF		   0x00010000
#define IRQ_EOF		   0x20000000
#define STATUS_EOF 0x00020000
#define IRQ_OVER_RUN   0x01000000
#define IRQ_ALL        (IRQ_OVER_RUN| IRQ_EOF|IRQ_SOF|IRQ_FIFO_FULL)
#define CSI_CTL_PRP     0x10000000

#define I2C_DRIVERID_I2CCSI	0xF000

/* Define of Massage Flag */
#define EMBEDDED_REGISTER		0x01
#define I2C_M_READ				0x02
#define I2C_M_WT				0x04

#define I2C_CLKDIV		0x17		// 0x17 => /960 => 100k I2C clock for 96 MHz system clock
//#define I2C_CLKDIV		0x13		// 0x13 => /480 => 100k I2C clock for 48 MHz system clock

//
//	SENSOR read/write command
//
#define SENSOR_I2C_ADDR  (u32)0xB8	//01100110b, last bit 0 => WRITE

// functions and interface
static int csi2c_open(struct inode *inode, struct file *filp);
static int csi2c_release(struct inode *inode, struct file *filp);
static ssize_t csi2c_read(struct file *filp, char *buf, size_t size, loff_t *l);
static ssize_t csi2C_write(struct file *filp, const char *buf, size_t size, loff_t *l);
static int csi2c_ioctl (struct inode *inode, struct file *file, unsigned int cmd, unsigned long arg);
static void I2C_init(void);
static int I2C_read_16(u32 reg, u32 * _data);
static int I2C_write_16(u32 reg, u16 data);
static int i2c_csi_attach_adapter (struct i2c_adapter * adap);
static int malloc_buffer(void);
static void free_buffer(void);
static void dump_reg(void);
static int I2C_write_16_check(u32 reg, u32 data);

static int	gMajor=243;
static int	result=0;
static u32 *g_csi_data_buf=0;
static u32 *g_buff_current=0;
static u32  g_bad_frame=0;
static DECLARE_WAIT_QUEUE_HEAD(g_capture_wait);

static u32 _gCSIBase=0;
static u32 openprp=0;
static u32 base=0x80000000;
static u32 irq=INT_CSI;
static int _gDmaChanel=-1;

static u32 g_IRQ_SOF_count=0;
static u32 g_IRQ_FULL_count=0;
static u32 g_IRQ_EOF_count=0;
static u32 g_all_data_count=0;

static struct proc_dir_entry * g_proc_dir;

#ifdef CONFIG_ARCH_MX2ADS
#define _reg_CSI_EXP_IO (*( (volatile u16*)(MX2ADS_PER_IOBASE+(4<<21)) ))
#endif

typedef struct _tag_CSI_REG
{
	volatile u32 CTRL_REG1;
	volatile u32 CTRL_REG2;
	volatile u32 STS_REG;
	volatile u32 STAT_FIFO;
	volatile u32 RX_FIFO;
	volatile u32 RX_COUNT; 
}CSI_REG;

CSI_REG *_gCSI_REG;

static struct i2c_driver i2c_csi_driver = {
	name:					"i2c-csi client driver",
	id:					I2C_DRIVERID_I2CCSI,
	flags:				I2C_DF_DUMMY | I2C_DF_NOTIFY,
	attach_adapter:	i2c_csi_attach_adapter,
	detach_client:		NULL,	/*i2c_csi_detach_client,*/
	command:				NULL
};

static struct i2c_client i2c_csi2c_client = {
	name:		"i2c-csi client",
	id:		2,
	flags:	0,
	addr:		-1,
	adapter:	NULL,
	driver:	&i2c_csi_driver,
	data:		NULL
};

struct file_operations csi2c_fops = {
	open:          csi2c_open,
	release:       csi2c_release,
	read:          csi2c_read,
	write:			csi2C_write,
	ioctl:			csi2c_ioctl,
};



static int csi_init(void)
{
	u32 addr;
	dprintcsi2c("Begin Config CSI\n");
	
	_gCSI_REG->CTRL_REG1=0; 			//module reset
		
	_gCSI_REG->CTRL_REG1 |= 0x2;	//latch on rising edge
	_gCSI_REG->CTRL_REG1 |= 0x10;	//gated clock mode
	_gCSI_REG->CTRL_REG1 |= 0x800;	//hsync active mode
	_gCSI_REG->CTRL_REG1 |= 0x100;	//sync FIFO clear
	_gCSI_REG->CTRL_REG1 |= 0x2200;	//MCLK = 24
	_gCSI_REG->CTRL_REG1 |= 0x40000000;	//SOF rising edge
	_gCSI_REG->CTRL_REG1 |= 0x20000;	//SOF rising edge
	_gCSI_REG->CTRL_REG1 |= 0x10000;	//SOF INT
	
	_gCSI_REG->CTRL_REG1 |= 0x40000;   //RXFF int
	_gCSI_REG->CTRL_REG1 |= 0x100000;  //RXFF level =16;
	_gCSI_REG->CTRL_REG1 |= 0x1000000; //RXFF overflow int
	
	_gCSI_REG->CTRL_REG1 |= 0x80000000;//2 byte of 4 byte swap
	_gCSI_REG->CTRL_REG1 |= 0x80;     //big endian
	
	_gCSI_REG->RX_COUNT=320*240*2;
	
	dump_reg();
	
	
	dprintcsi2c("End Config CSI\n");
	
	return 0;
}

static void csi_enable_irq(int mask)
{
	_gCSI_REG->CTRL_REG1 |= mask;
}
static void csi_disable_irq(int mask)
{
	_gCSI_REG->CTRL_REG1&=~mask;
}
int wait_d190_ready(void)
{   
	u32 count=0;
	u32 status;
    while(1)	//poll until update completed
    {
    	count++;
    	if(count>10000)
    	{
    		printk("Time Out Error\n");
    		return -1;
    	}
	    I2C_read_16(0xe8, &status);
	    if(!status)
	    	break;
	}
	return 0;
}
int SetWindowSize(short width, short height)
{
    _gCSI_REG->RX_COUNT=width*height*2;
    
    I2C_write_16(0x63, width>>8);
	I2C_write_16(0x64, width&0xFF);
	I2C_write_16(0x65, height>>8);
	I2C_write_16(0x66, height&0xFF);
}
int IM8803_QVGA_init(void)
{
	u32 status;
	u32 data;
	
	dprintcsi2c("Begin Conifg IM8803\n");
	
	data=_reg_CSI_EXP_IO;
	data&=(~0x40);
	_reg_CSI_EXP_IO=data; ///disable standby
	
	udelay(20);
	
	
	//reset IM8803
	data&=~0x20;
	_reg_CSI_EXP_IO=data; 
	
	udelay(200);
	data|=0x20;
	_reg_CSI_EXP_IO=data;
		
//soft reset
	I2C_write_16_check(0x01, 0x0004);	//select IC register
	I2C_write_16_check(0x0D, 0x0001);	//reset IC
	I2C_write_16_check(0x0D, 0x0000);
	I2C_write_16_check(0x01, 0x0001);	//select IFP register
	I2C_write_16_check(0x07, 0x0001);	//reset IFP
	I2C_write_16_check(0x07, 0x0000);

//decimation control
	I2C_write_16_check(0x01, 0x0001);	//select IFP register
	I2C_write_16_check(0x46, 0x0001);	//QVGA

//read mode control
	I2C_write_16_check(0x01, 0x0004);	//select IC register
	I2C_write_16(0x20, 0xD0A1);	//read from top to bottom (rotate by 180 deg)

//workaround for read mode control
	I2C_write_16_check(0x01, 0x0001);	//select IFP register
	I2C_write_16_check(0x37, 0x0100);	//fix for flickering

//select RGB565 output
	I2C_write_16_check(0x01, 0x0001);	//select IFP register
	I2C_read_16(0x08, &data);
	
	dprintcsi2c("data is %d",data);
	data |= 0x1000;	//set bit 12
	I2C_write_16_check(0x08, data);

	dprintcsi2c("IM8803 Config Finish ***\n");
	return 0;
}
static void gpio_init(void)
{
	_reg_GPIO_GIUS(GPIOB)&=~0x3FFC00;//disable GPIO
}
static void DmaFinish(void * arg)
{
	dprintcsi2c("*** DmaFinish ***\n");

	wake_up_interruptible(&g_capture_wait);
}
static int csi2c_open(struct inode *inode, struct file *filp)
{
	dma_request_t dma;

	dprintcsi2c("*** open ***\n");
	
	if(malloc_buffer())
	{
		return -1;
	}
	gpio_init();
	csi_init();
	//csi_disable_irq(IRQ_ALL);
	
	IM8803_QVGA_init();
	
	//SetWindowSize(320,240);
	
	if(dbmx2_request_dma(&_gDmaChanel,"CSI"))
	{
		printk("Request Dma Fail\n");
		free_buffer();
		return -1;
	}else
	{
		printk("Dma Channel is %d\n",_gDmaChanel);

		memset(&dma,0,sizeof(dma_request_t));
		dma.sourceType=DMA_TYPE_FIFO;
		
		dma.sourceAddr=0x80000010;

		dma.request = DMA_REQ_CSI_RX;

		dma.destAddr=(u32*)__virt_to_phys((u_long)g_csi_data_buf);	
		dma.destType=DMA_TYPE_LINEAR;
		dma.destPort=DMA_MEM_SIZE_32;
		dma.count=_gCSI_REG->RX_COUNT;
		dma.burstLength=64;
		dma.callback=DmaFinish;
		
		dbmx2_dma_set_config(_gDmaChanel,&dma);	
		#ifdef	DEBUG_CSI2C
		dbmx2_dump_dma_register(_gDmaChanel);
		#endif
	}
        MOD_INC_USE_COUNT;
 	return 0;
}

static void dump_reg(void)
{
	printk("Reg1   %X\n", _gCSI_REG->CTRL_REG1);
	printk("Reg2   %X\n", _gCSI_REG->CTRL_REG2);
	printk("Status %x\n", _gCSI_REG->STS_REG);
	printk("Count  %x\n",_gCSI_REG->RX_COUNT);
}
static int csi2c_release(struct inode *inode, struct file *filp)
{
	dprintcsi2c("*** close ***\n");
	MOD_DEC_USE_COUNT;
	
	//_gCSI_REG->CTRL_REG1=0x40000000;
	csi_disable_irq(IRQ_ALL);
	
	if(_gDmaChanel>=0)
	{
		dbmx2_free_dma(_gDmaChanel);
		_gDmaChanel=-1;
	}
	free_buffer();
	
	return 0;
}
static void start_capture(void)
{
	g_buff_current=g_csi_data_buf;
	csi_disable_irq(IRQ_ALL);
	csi_enable_irq(IRQ_SOF);
}

static void poll_get_data(void)
{
	int i=0;
	//start_capture();
	g_buff_current=g_csi_data_buf;
	dprintcsi2c("*** wait SOF *** %d\n",_gCSI_REG);
	_gCSI_REG->STS_REG=IRQ_SOF;
	while(! (_gCSI_REG->STS_REG&IRQ_SOF) );
	_gCSI_REG->STS_REG=IRQ_SOF;
	
	if(_gCSI_REG->STS_REG& 0x1000000)
		_gCSI_REG->STS_REG = 0x1000000;

	while(1)
	{	
		if(_gCSI_REG->STS_REG& 0x1000000)
		{
			printk("CSI overflow\n");
			_gCSI_REG->STS_REG = 0x1000000;
		}
		if( _gCSI_REG->STS_REG & 0x40000)
		{
			for(i=0;i<16;i++)
			{
				*g_buff_current=_gCSI_REG->RX_FIFO;
				g_buff_current++;
				g_all_data_count+=sizeof(u32);
			}
			if( ((g_buff_current-g_csi_data_buf)*sizeof(u32))>=320*240*2 )
			{
				_gCSI_REG->STS_REG=STATUS_EOF;
				dprintcsi2c("End Frame\n");
				g_IRQ_EOF_count++;
				break;
			}
		}
	}

}
static ssize_t csi2c_read(struct file *filp, char *buf, size_t size, loff_t *l)
{
	dprintcsi2c("*** read ***\n");
	if(openprp)
		return -1;
	if(_gDmaChanel>=0)
	{
		start_capture();
		if(!interruptible_sleep_on_timeout(&g_capture_wait,500))
		{
			dprintcsi2c("read time out\n");
			return -1;
		}
		if(size>_gCSI_REG->RX_COUNT)
			size=_gCSI_REG->RX_COUNT;
	}
	else
	{	
		poll_get_data();
	}
	copy_to_user(buf,g_csi_data_buf,size);
	dprintcsi2c("%d copy to user\n",size);
	
	return size;
}

static ssize_t csi2C_write(struct file *filp, const char *buf, size_t size, loff_t *l)
{
	dprintcsi2c("*** write ***\n");
	return -1;
}

static int malloc_buffer(void) 
{
	if ((g_csi_data_buf = (u32 *)__get_free_pages(GFP_KERNEL, 7)))	// 128 pages = 128x4K = 512K
	{
		dprintcsi2c("Buffer start: 0x%08x", (int)g_csi_data_buf);
	}
	else
	{
		printk("*** ERROR: cannot allocate buffer memory for driver ! ***\n");
		return -1;
	}
	return 0;
}

static void free_buffer(void)
{
	dprintcsi2c("free buffer\n");
	if (g_csi_data_buf)
	{	__free_pages((void *)g_csi_data_buf,7);		// 512K
		g_csi_data_buf=0;
	}
}

static void	setSubsample(int subsample)
{
	u32 value;
	
	switch (subsample)
	{
		case 2:		// divide by 2
			value = 0x11;
			break;
		case 3:		// divide by 4
			value = 0x22;
			break;
		case 4:		// divide by 8
			value = 0x33;
			break;
		default:		// i.e. full subsampling
			value = 0;		// REMARK: "cm" bit must be cleared for full-sampling
	}
}

static int csi2c_ioctl (struct inode *inode, struct file *file, unsigned int cmd, unsigned long arg)
{
	dprintcsi2c("cmd: 0x%08x, arg: 0x%08x\n", cmd, (int)arg);
	switch(cmd)
	{
		case IOCTL_ENABLE_PRP:
			openprp=1;
			_gCSI_REG->CTRL_REG1|=CSI_CTL_PRP;	
			break;
		case IOCTL_DISABLE_PRP:
			openprp=0;
			_gCSI_REG->CTRL_REG2&=~CSI_CTL_PRP;
			break;
	}
	return 0;
}

static int i2c_csi_attach_adapter (struct i2c_adapter * adap)
{
	dprintcsi2c("adap %x\n",adap);
	/* find out the adapter for the I2C module in the DBMX1*/
	
	if (memcmp(adap->name, "DBMX I2C Adapter", 16) != 0 )
		return -ENODEV;

	/* store the adapter to the client driver */
	dprintcsi2c("Hear");
	i2c_csi2c_client.adapter = adap;

	return 0;
}

static void I2C_init(void)
{
	dprintcsi2c("Being I2C_init\n");
	/* 
	 * set the address of the CMOS sensor to the client
	 */
	i2c_csi2c_client.addr = SENSOR_I2C_ADDR;

	/* 
	 * call the i2c_add_driver() to register the driver 
	 * to the Linux I2C system
	 */
	if (i2c_add_driver( &i2c_csi_driver ) != 0)
	{
		printk("*** i2c_add_driver() failed ***\n");
		return;
	}
	
	dprintcsi2c("i2c_add_driver\n");
	/* 
	 * attach the client to the adapter by calling the i2c_attach_client() 
	 * function of the Linux I2C system
	 */
	if (i2c_attach_client(&i2c_csi2c_client ) != 0)
	{	
		printk("*** i2c_attach_client() failed ***\n");
		i2c_del_driver(&i2c_csi_driver);
		return;
	}

	dprintcsi2c("i2c_attach_driver\n");
	i2c_csi2c_client.adapter->inc_use( i2c_csi2c_client.adapter );
	dprintcsi2c("End I2C_int\n");
}

static void I2C_cleanup(void)
{
    i2c_csi2c_client.adapter->dec_use(i2c_csi2c_client.adapter);
	i2c_detach_client(&i2c_csi2c_client );
	i2c_del_driver(&i2c_csi_driver);
}

//-------------------------------
//
//	Port enable for CSI signals
//
//-------------------------------
static void port_init_CSI(void)
{
	return;
}

//---------------------------
//
//	SENSOR Register Write
//
//---------------------------
//#define DEBUG_I2CWR
#ifdef DEBUG_I2CWR
#define dprinti2cwr(str...) printk("<"__FUNCTION__"> "str)
#else
#define dprinti2cwr(str...)	// nothing
#endif

static int I2C_write_16_check(u32 reg, u32 data)
{
	u32 d;
	
	if(I2C_write_16(reg,data)<0)
	{
		printk("I2C Write Error\n");
		return -1;
	}
	
	if(I2C_read_16(reg,&d)<0)
	{
		printk("I2C Read Error\n");
		return -1;
	}
	
	if(data!=d)
	{
		printk("I2C Varify Error expect %X : %X\n",data,d);
		return -1;
	}
	return 0;
}
static int I2C_write_16(u32 reg, u16 data)
{
	struct i2c_msg msg;
	char buf[3];
	int ret;
	/* 
	 * store the register value to the first address of the buffer 
	 * the adapter/algorithm driver will regard the first byte 
	 * as the register value 
	 */
	buf[0] = (char)reg;
	buf[1] = (char)((data&0xFF00)>>8);
	buf[2] = (char)(data&0xFF);
	
	/* 
	 * initialize the message structure
	 */
	msg.addr = i2c_csi2c_client.addr;
	msg.flags = EMBEDDED_REGISTER | I2C_M_WT;
	msg.len = 3;
	msg.buf = buf;
		
	ret=i2c_transfer( i2c_csi2c_client.adapter, &msg, 1 );
	dprintcsi2c("I2C Write %x %x\n", reg, data);
	return ret;
}

//---------------------------
//
//	SENSOR Register Read
//
//---------------------------
static int I2C_read_16(u32 reg, u32 * _data)
{
	struct i2c_msg msg[2];
	char buf[3];
	int ret;
	
	/* 
	 * store the register value to the first address of the buffer 
	 * the adapter/algorithm driver will regard the first byte 
	 * as the register value 
	 */
	buf[0] = (char)reg;

	/* 
	 * initialize the message structure
	 */
	msg[0].addr =  i2c_csi2c_client.addr;
	msg[0].flags = EMBEDDED_REGISTER | I2C_M_WT;
	msg[0].len = 2;
	msg[0].buf = buf;

	msg[1].addr =  i2c_csi2c_client.addr;
	msg[1].flags = EMBEDDED_REGISTER | I2C_M_READ;
	msg[1].len = 2;
	msg[1].buf = buf;
	
	ret=i2c_transfer( i2c_csi2c_client.adapter, msg, 2 );
	*_data = (buf[0]<<8)|(buf[1]);
	
	dprintcsi2c("ret=%d reg:%x, value:%x\n",ret,reg,*_data);
	
	return ret;
}

//-------------------------------
//
//	Port enable for SENSOR signals
//
//-------------------------------
static void port_init_SENSOR(void)
{
#ifdef CONFIG_ARCH_MX1ADS

#endif
}

static void csi_intr_handler(int irq, void *dev_id, struct pt_regs *regs)
{
	int i;
#ifdef SWAP_2_OF_4
	register u32 data,data2;
#endif
	if(_gCSI_REG->STS_REG&IRQ_SOF)
	{
		_gCSI_REG->STS_REG=IRQ_ALL;
		csi_disable_irq(IRQ_ALL);
		if(_gDmaChanel>=0)
			dbmx2_dma_start(_gDmaChanel);
		//dprintcsi2c("SOF\n");
		g_IRQ_SOF_count++;
		return;
	}

	if(_gCSI_REG->STS_REG&IRQ_OVER_RUN);
	{
		_gCSI_REG->STS_REG=IRQ_ALL;
		csi_disable_irq(IRQ_ALL);
		//csi_enable_irq(IRQ_SOF); //give up this frame and wait next frame;
		g_bad_frame++;
		//dprintcsi2c("over run\n");
		return;
	}

	_gCSI_REG->STS_REG=_gCSI_REG->STS_REG;//Clear All irpt;
	dprintcsi2c("Unknow interrupt\n");
}

static devfs_handle_t devfs_handle;


#define DEBUG_I2C

#ifdef DEBUG_I2C
int init_csi_testi2c(void)
{	
	_gCSI_REG->RX_COUNT = 0x9600;
	_gCSI_REG->CTRL_REG1=0x31171302;
	
	*(volatile u32 *)0xf300100c|=0x8;
	*(volatile u32 *)0xf300100c&=~0x8;
	udelay(100);
	*(volatile u32 *)0xf300100c|=0x8;
	
	*(volatile u32 *)0xf300100c&=~0x4;

	return 0;
}
#endif
 
static int get_csi_list(char *buf)
{
	char *p = buf;
	p+=sprintf(p,"CSI:\n");
	p+=sprintf(p,"\tBad Frame:\t%d\n", g_bad_frame);
	p+=sprintf(p,"\tSOF:\t %d\n",g_IRQ_SOF_count);
	p+=sprintf(p,"\tEOF:\t %d\n",g_IRQ_EOF_count);
	p+=sprintf(p,"\tFULL:\t %d\n",g_IRQ_FULL_count);
	p+=sprintf(p,"\tData have Read:\t %d\n",g_all_data_count);
	p+=sprintf(p,"\tPrp Link %s\n", _gCSI_REG->CTRL_REG1&CSI_CTL_PRP?"Enable":"Disable");
		
	return p - buf;
}

/*
 * DMA interface functions
 */
static int mx2_get_csi_list(char *page, char **start, off_t off,
				 		int count, int *eof, void *data)
{
	int len = get_csi_list(page);
	
	if (len <= off+count) *eof = 1;
	*start = page + off;
	len -= off;
	if (len>count) len = count;
	if (len<0) len = 0;
	
	return len;
};
int csi2c_init(void)
{
	u32 * addr;
	
	dprintcsi2c("Begin Init Modlue\n");
	
	printk("CSI driver "__DATE__" / "__TIME__"\n");
	/* register our character device */
	
	addr=ioremap_nocache(base,0x1000);
	
	mx_module_clk_open(HCLK_MODULE_CSI); //It must be enable before access CSI register
	
	if(addr==0)
	{
		printk("IO Map ERROR\n");
		return -1;
	}
		
	_gCSI_REG=(CSI_REG*)addr;
	I2C_init();
	
	printk("Address is 0x%X  VAddr %X irq is 0x%X\n",base,_gCSI_REG,irq);
	dprintcsi2c("buffer size %d\n",BUFFER_SIZE);
			
 	result = devfs_register_chrdev(gMajor, "csi2c", &csi2c_fops);
 	if ( result < 0 )
 	{
		printk("csi2c driver: Unable to register driver\n");
		return -ENODEV;
	}
	devfs_handle = devfs_register(NULL, "csi2c", DEVFS_FL_DEFAULT,
				      gMajor, 0,
				      S_IFCHR | S_IRUSR | S_IWUSR,
				      &csi2c_fops, NULL);   	
	
	if (request_irq(irq, csi_intr_handler, SA_INTERRUPT, "CSI", NULL))
		printk("*** Cannot register interrupt handler for CSI ! ***\n");
	else
		printk("CSI interrupt handler registered\n");
	
	//*((volatile u32 *)0xf0400008)=0x800;
	
	g_proc_dir=create_proc_entry("CSI", 0, NULL);
	g_proc_dir->read_proc=mx2_get_csi_list;
	g_proc_dir->data=NULL;
	
	
	if(openprp)
	{
	    csi_init();
	    csi_disable_irq(IRQ_ALL);
	
	    IM8803_QVGA_init();
	    _gCSI_REG->CTRL_REG1|=CSI_CTL_PRP;
	}
	
   	return 0;
}

void csi2c_cleanup(void)
{    
    	if(openprp)
    	{
       	 _gCSI_REG->CTRL_REG1&=~CSI_CTL_PRP;
        	openprp=0;
    	}
    
   	if (result > 0)
   	{
		devfs_unregister_chrdev(gMajor, "csi2c");
		devfs_unregister(devfs_handle);
	}
	
	csi_disable_irq(IRQ_ALL);	
	free_irq(irq, 0);
	I2C_cleanup();
	iounmap(_gCSIBase);
	free_buffer();	
	remove_proc_entry("CSI",g_proc_dir);
	printk("Say goodbye to csi2c\n");
}

module_init(csi2c_init);
module_exit(csi2c_cleanup);

MODULE_PARM(base,"i");
MODULE_PARM(irq,"i");
MODULE_PARM(dmabase,"i");
MODULE_PARM(openprp, "i");
