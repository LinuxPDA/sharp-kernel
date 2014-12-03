/* ------------------------------------------------------------
 * driver/media/video/imx21video.c:
 * 	iMagic IM88023B1 video for linux
 * 
 * Author:	Tsuyoshi Yamada
 *
 * 	Copyright (C) 2004, Lineo Solutions, Inc.
 * ------------------------------------------------------------ */

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
#include <linux/slab.h>
#include <linux/mm.h>
#include <linux/wrapper.h>
#include <linux/poll.h>
#include <linux/signal.h>
#include <linux/miscdevice.h>
#include <linux/proc_fs.h>
#include <linux/delay.h>
#include <linux/sched.h>
#include <linux/types.h>
#include <linux/interrupt.h>
#include <linux/kmod.h>
#include <linux/vmalloc.h>
#include <linux/pagemap.h>
#include <linux/videodev.h>

#include <linux/i2c.h>
#include <linux/i2c-algo-bit.h>
#include <linux/i2c-id.h>

#include <asm/io.h>
#include <asm/dma.h>
#include <asm/pgtable.h>
#include <asm/page.h>
#include <asm/arch/mx2.h>
#include <asm/arch/pll.h>
#include <asm/arch/platform.h>
#include "imx21video.h"

//#define DEBUG

#ifdef DEBUG
#define DPRINTK(f, arg...) printk("<%s> " f, __FUNCTION__, ##arg)
#else
#define DPRINTK(str...)	// nothing
#endif

#define IRQ_FIFO_FULL	0x00040000
#define IRQ_SOF		0x00010000
#define IRQ_EOF		0x20000000
#define STATUS_EOF	0x00020000
#define IRQ_OVER_RUN	0x01000000
#define IRQ_ALL		(IRQ_OVER_RUN| IRQ_EOF|IRQ_SOF|IRQ_FIFO_FULL)
#define CSI_CTL_PRP	0x10000000

#define I2C_DRIVERID_I2CCSI	0xF000

/* Define of Massage Flag */
#define EMBEDDED_REGISTER	0x01
#define I2C_M_READ		0x02
#define I2C_M_WT		0x04

#define I2C_CLKDIV		0x17	/* 0x17 => /960 => 100k I2C clock
					 * for 96 MHz system clock */
#if 0
#define I2C_CLKDIV		0x13	/* 0x13 => /480 => 100k I2C clock
					 * for 48 MHz system clock */
#endif

/*
 *	SENSOR read/write command
 */
#define SENSOR_I2C_ADDR	(u32)0xB8	/* 01100110b, last bit 0 => WRITE */

/* functions and interface */
static int imx21video_open(struct video_device *, int);
static void imx21video_close(struct video_device *);
static long imx21video_read(struct video_device *,
		char *, unsigned long, int);
static long imx21video_write(struct video_device*,
		const char*, unsigned long, int);
static int imx21video_ioctl(struct video_device *, unsigned int, void *);
static int imx21video_mmap(struct video_device *, const char *, unsigned long);

static void I2C_init(void);
static int I2C_read_16(u32, u32 *);
static int I2C_write_16(u32, u16);
static int i2c_csi_attach_adapter (struct i2c_adapter *);
static int malloc_buffer(void);
static void free_buffer(void);
static int I2C_write_16_check(u32, u32);
static void csi_intr_handler(int, void *, struct pt_regs *);
static void I2C_cleanup(void);
static void start_capture(void);
static void poll_get_data(void);

static u32 *g_csi_data_buf = 0;
static u32 *g_buff_current = 0;
static u32 g_bad_frame = 0;
static DECLARE_WAIT_QUEUE_HEAD(g_capture_wait);

static u32 _gCSIBase = 0;
static u32 openprp = 0;
static u32 base = 0x80000000;
static u32 irq = INT_CSI;
static int _gDmaChanel = -1;

static u32 g_IRQ_SOF_count = 0;
static u32 g_IRQ_EOF_count = 0;
static u32 g_all_data_count = 0;

static imx21video_dev_t devs[MAX_IMX21VIDEO];
static imx21video_win_t winsize;
static void *cur_vbuf = NULL;
static struct imx21video_gbuf *cur_gbuf = NULL;

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
} CSI_REG;

static CSI_REG *_gCSI_REG;

static struct i2c_driver i2c_csi_driver = {
    name:		"i2c-csi client driver",
    id:			I2C_DRIVERID_I2CCSI,
    flags:		I2C_DF_DUMMY | I2C_DF_NOTIFY,
    attach_adapter:	i2c_csi_attach_adapter,
    detach_client:	NULL,	/*i2c_csi_detach_client,*/
    command:		NULL
};

static struct i2c_client i2c_imx21video_client = {
    name:	"i2c-csi client",
    id:		2,
    flags:	0,
    addr:	SENSOR_I2C_ADDR,
    adapter:	NULL,
    driver:	&i2c_csi_driver,
    data:	NULL
};

static struct video_device imx21video_vdev =
{
    owner:	THIS_MODULE,
    name:	"iMagic",
    type:	VID_TYPE_CAPTURE | VID_TYPE_SCALES,
    hardware:	VID_HARDWARE_IM8803,
    open:	imx21video_open,
    close:	imx21video_close,
    read:	imx21video_read,
    write:	imx21video_write,
    ioctl:	imx21video_ioctl,
    mmap:	imx21video_mmap,
    minor:	-1,
};

/* ------------------------------------------------------------
 * 	Memory management functions
 * ------------------------------------------------------------ */
static inline unsigned long kvirt_to_bus(unsigned long adr)
{
    unsigned long kva;

    kva = (unsigned long)page_address(vmalloc_to_page((void *)adr));
    kva |= adr & (PAGE_SIZE-1); /* restore the offset */
    return virt_to_bus((void *)kva);
}

static inline unsigned long kvirt_to_pa(unsigned long adr)
{
    unsigned long kva;

    kva = (unsigned long)page_address(vmalloc_to_page((void *)adr));
    kva |= adr & (PAGE_SIZE-1); /* restore the offset */
    return __pa(kva);
}

static void * rvmalloc(signed long size)
{
    struct page *page;
    void * mem;
    unsigned long adr;

    mem=vmalloc_32(size);
    if (NULL == mem)
	printk(KERN_INFO "%s: vmalloc_32(%ld) failed\n",DEVNAME, size);
    else {
	/* Clear the ram out, no junk to the user */
	memset(mem, 0, size);
	adr=(unsigned long) mem;
	while (size > 0) {
	    page = vmalloc_to_page((void *)adr);
	    mem_map_reserve(page);
	    adr+=PAGE_SIZE;
	    size-=PAGE_SIZE;
	}
    }
    return mem;
}

static void rvfree(void * mem, signed long size)
{
    struct page *page;
    unsigned long adr;

    if (mem) {
	adr=(unsigned long) mem;
	while (size > 0) {
	    page = vmalloc_to_page((void *)adr);
	    mem_map_unreserve(page);
	    adr+=PAGE_SIZE;
	    size-=PAGE_SIZE;
	}
	vfree(mem);
    }
}

static int fbuffer_alloc(struct imx21video_dev *dev)
{
    if (!dev->fbuffer)
	dev->fbuffer=(unsigned char *) rvmalloc(FBUFFERS * FBUF_SIZE);
    else
	printk(KERN_ERR "%s%d: Double alloc of fbuffer!\n", DEVNAME, dev->nr);
    if (!dev->fbuffer)
	    return -ENOBUFS;
    return 0;
}

/* ------------------------------------------------------------ */

static int vgrab(struct imx21video_dev *dev, struct video_mmap *mp)
{
    unsigned long count = 0;
    
    DPRINTK("video_mmap: frame(%d), h(%d), w(%d), frm(%d)\n",
		mp->frame, mp->height, mp->width, mp->format);
    if (!dev->fbuffer) {
	if (fbuffer_alloc(dev))
	    return -ENOBUFS;
    }
    if (mp->frame >= FBUFFERS || mp->frame < 0)
	return -EINVAL;
    if(mp->height < dev->minwin.h || mp->width < dev->minwin.w)
	return -EINVAL;
    if (mp->format != VIDEO_PALETTE_RGB565)
	return -EINVAL;
    if (RGB565_FORMAT(mp->height, mp->width) > FBUF_SIZE)
	return -EINVAL;
    if (cur_vbuf || cur_gbuf)
	return -EBUSY;
    /*
     * Load up
     */
    dev->gbuf[mp->frame].stat = GBUFFER_GRABBING;
    count = RGB565_FORMAT(mp->height, mp->width);
    cur_vbuf = (void *)(dev->fbuffer + FBUF_SIZE * mp->frame);
    cur_gbuf = &dev->gbuf[mp->frame];
    if (_gDmaChanel>=0) {
	start_capture();
    } else {
	poll_get_data();
	dev->gbuf[mp->frame].stat = GBUFFER_DONE;
    }
    return 0;
}

/* ------------------------------------------------------------ */

static int csi_init(void)
{
    DPRINTK("Begin Config CSI\n");
	
    _gCSI_REG->CTRL_REG1=0; 			//module reset
		
    _gCSI_REG->CTRL_REG1 |= 0x2;	//latch on rising edge
    _gCSI_REG->CTRL_REG1 |= 0x10;	//gated clock mode
    _gCSI_REG->CTRL_REG1 |= 0x800;	//hsync active mode
    _gCSI_REG->CTRL_REG1 |= 0x100;	//sync FIFO clear
    _gCSI_REG->CTRL_REG1 |= 0x2200;	//MCLK = 24
    _gCSI_REG->CTRL_REG1 |= 0x40000000;	//SOF rising edge
    _gCSI_REG->CTRL_REG1 |= 0x20000;	//SOF rising edge
    _gCSI_REG->CTRL_REG1 |= 0x10000;	//SOF INT
    _gCSI_REG->CTRL_REG1 |= 0x40000;	//RXFF int
    _gCSI_REG->CTRL_REG1 |= 0x100000;	//RXFF level =16;
    _gCSI_REG->CTRL_REG1 |= 0x1000000;	//RXFF overflow int
    _gCSI_REG->CTRL_REG1 |= 0x80000000;	//2 byte of 4 byte swap
    _gCSI_REG->CTRL_REG1 |= 0x80;	//big endian
	
    DPRINTK("End Config CSI\n");
    return 0;
}

static inline void csi_enable_irq(int mask)
{
    _gCSI_REG->CTRL_REG1 |= mask;
}

static inline void csi_disable_irq(int mask)
{
    _gCSI_REG->CTRL_REG1&=~mask;
}

int wait_d190_ready(void)
{   
    u32 count = 0, status;

    while (1) {	//poll until update completed
	count++;
    	if (count > 10000) {
    	    printk("Time Out Error\n");
	    return -1;
    	}
	I2C_read_16(0xe8, &status);
	if (!status) break;
    }
    return 0;
}

static int set_win_size(short width, short height)
{
    winsize.w = width;
    winsize.h = height;
    
    _gCSI_REG->RX_COUNT=width*height*2;
    
    I2C_write_16(0x63, width>>8);
    I2C_write_16(0x64, width&0xFF);
    I2C_write_16(0x65, height>>8);
    I2C_write_16(0x66, height&0xFF);
}

static int IM8803_QVGA_init(void)
{
    u32 data;
	
    DPRINTK("Begin Conifg IM8803\n");
	
    data = _reg_CSI_EXP_IO;
    data &= (~0x40);
    _reg_CSI_EXP_IO = data;	//disable standby
	
    udelay(20);
	
	
    /* reset IM8803 */
    data &= ~0x20;
    _reg_CSI_EXP_IO = data; 
	
    udelay(200);
    data |= 0x20;
    _reg_CSI_EXP_IO = data;
		
    /* soft reset */
    I2C_write_16_check(0x01, 0x0004);	//select IC register
    I2C_write_16_check(0x0D, 0x0001);	//reset IC
    I2C_write_16_check(0x0D, 0x0000);
    I2C_write_16_check(0x01, 0x0001);	//select IFP register
    I2C_write_16_check(0x07, 0x0001);	//reset IFP
    I2C_write_16_check(0x07, 0x0000);

    /* decimation control */
    I2C_write_16_check(0x01, 0x0001);	//select IFP register
    I2C_write_16_check(0x46, 0x0001);	//QVGA

    /* read mode control */
    I2C_write_16_check(0x01, 0x0004);	//select IC register
    I2C_write_16(0x20, 0xD0A1);	//read from top to bottom (rotate by 180 deg)

    /* workaround for read mode control */
    I2C_write_16_check(0x01, 0x0001);	//select IFP register
    I2C_write_16_check(0x37, 0x0100);	//fix for flickering

    /* select RGB565 output */
    I2C_write_16_check(0x01, 0x0001);	//select IFP register
    I2C_read_16(0x08, &data);
	
    DPRINTK("data is %d",data);
    data |= 0x1000;	//set bit 12
    I2C_write_16_check(0x08, data);

    DPRINTK("IM8803 Config Finish ***\n");
    return 0;
}

static inline void gpio_init(void)
{
    _reg_GPIO_GIUS(GPIOB)&=~0x3FFC00;	//disable GPIO
}

static void DmaFinish(void *arg)
{
    DPRINTK("*** DmaFinish ***\n");

    if (_gDmaChanel < 0)
	return;

    if (cur_vbuf && cur_gbuf) {
	int i, j;
	short *pbuf = (short *)cur_vbuf, *p = (short *)g_csi_data_buf;
#if 0
	memcpy(cur_vbuf, g_csi_data_buf, FBUF_SIZE);
#else
	for (i = 0; i < winsize.w; i++) {
	    for (j = 0; j < winsize.h; j++) {
		pbuf[j*winsize.w+i] = *p++;
	    }
	}
#endif
	cur_gbuf->stat = GBUFFER_DONE;
	cur_vbuf = NULL;
	cur_gbuf = NULL;
	wake_up_interruptible(&g_capture_wait);
    } else {
	wake_up_interruptible(&g_capture_wait);
    }
}

static int imx21video_open(struct video_device *v, int flags)
{
    dma_request_t dma;
    u32 * addr;
    struct imx21video_dev *dev = (struct imx21video_dev *)v->priv;
    int i;
	
    DPRINTK("Begin Init Modlue\n");
	
    /* register our character device */
	
    if (!(addr = ioremap_nocache(base,0x1000))) {
	printk("IO Map ERROR\n");
	return -1;
    }
	
    mx_module_clk_open(HCLK_MODULE_CSI);	/* It must be enable before 
						   access CSI register */
	
    _gCSI_REG=(CSI_REG*)addr;
    I2C_init();
	
    DPRINTK("Address is 0x%lX  VAddr %lX irq is 0x%lX\n",
	(long)base, (long)_gCSI_REG, (long)irq);
    DPRINTK("buffer size %ld\n", (long)FBUF_SIZE);
			
    if (request_irq(irq, csi_intr_handler, SA_INTERRUPT, "CSI", NULL))
	printk("*** Cannot register interrupt handler for CSI ! ***\n");
	
    if(openprp) {
	csi_init();
	csi_disable_irq(IRQ_ALL);
	
	IM8803_QVGA_init();
	_gCSI_REG->CTRL_REG1|=CSI_CTL_PRP;
    }
	
    DPRINTK("*** open ***\n");
    if(malloc_buffer()) {
	return -1;
    }
    gpio_init();
    csi_init();
	
    IM8803_QVGA_init();
	
    set_win_size(dev->maxwin.w, dev->maxwin.h);
	
    for (i = 0; i < FBUFFERS; i++)
	dev->gbuf[i].stat = GBUFFER_UNUSED;

    cur_vbuf = NULL;
    cur_gbuf = NULL;
    
    if(dbmx2_request_dma(&_gDmaChanel,"CSI")) {
	printk("%s%d: Request Dma Fail\n", DEVNAME, dev->nr);
	free_buffer();
	return -1;
    } else {
	memset(&dma, 0, sizeof(dma_request_t));
	dma.sourceType = DMA_TYPE_FIFO;
	dma.sourceAddr = (u32*)0x80000010;
	dma.request = DMA_REQ_CSI_RX;
	dma.destAddr = (u32*)__virt_to_phys((u_long)g_csi_data_buf);
	dma.destType = DMA_TYPE_LINEAR;
	dma.destPort = DMA_MEM_SIZE_32;
	dma.count = _gCSI_REG->RX_COUNT;
	dma.burstLength = 64;
	dma.callback = DmaFinish;
	dbmx2_dma_set_config(_gDmaChanel, &dma);	
    }
    
    MOD_INC_USE_COUNT;
    return 0;
}

static void imx21video_close(struct video_device *v)
{
    struct imx21video_dev *dev = (struct imx21video_dev *)v->priv;
    
    DPRINTK("*** close ***\n");
    MOD_DEC_USE_COUNT;
	
    csi_disable_irq(IRQ_ALL);
	
    if (_gDmaChanel >= 0) {
	dbmx2_disable_dma(_gDmaChanel);
	dbmx2_free_dma(_gDmaChanel);
	_gDmaChanel = -1;
    }
    free_buffer();

    rvfree((void *) dev->fbuffer, FBUFFERS * FBUF_SIZE);
    dev->fbuffer = 0;
	
    if(openprp) {
	_gCSI_REG->CTRL_REG1&=~CSI_CTL_PRP;
	openprp=0;
    }
    
    csi_disable_irq(IRQ_ALL);
    free_irq(irq, 0);
    I2C_cleanup();
    iounmap((void *)_gCSIBase);
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
	DPRINTK("*** wait SOF *** %ld\n", (long)_gCSI_REG);
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
				DPRINTK("End Frame\n");
				g_IRQ_EOF_count++;
				break;
			}
		}
	}

}

static long imx21video_read(struct video_device *v, char *buf,
		unsigned long count, int nonblock)
{
    //struct imx21video_dev *dev = (struct imx21video_dev *)v->priv;

    DPRINTK("*** read ***\n");
    if (openprp) return -1;
    if (_gDmaChanel>=0) {
	start_capture();
	if(!interruptible_sleep_on_timeout(&g_capture_wait,500)) {
	    DPRINTK("read time out\n");
	    return -1;
	}
	if(count > _gCSI_REG->RX_COUNT)
	    count = _gCSI_REG->RX_COUNT;
    } else {	
	poll_get_data();
    }
    copy_to_user(buf, g_csi_data_buf, count);
    DPRINTK("%ld copy to user\n", count);
	
    return count;
}

static long imx21video_write(struct video_device *v, const char *buf,
		unsigned long count, int nonblock)
{
    DPRINTK("*** write ***\n");
    return -1;
}

static int malloc_buffer(void) 
{
    /* 128 pages = 128x4K = 512K */
    if ((g_csi_data_buf = (u32 *)__get_free_pages(GFP_KERNEL, FBUF_PAGES))) {
	DPRINTK("Buffer start: 0x%08x", (int)g_csi_data_buf);
    } else {
	printk("%s: cannot allocate buffer memory for driver !\n", DEVNAME);
	return -ENOMEM;
    }
    return 0;
}

static void free_buffer(void)
{
    DPRINTK("free buffer\n");
    if (g_csi_data_buf) {
	__free_pages((void *)g_csi_data_buf, FBUF_PAGES);	// 512K
	g_csi_data_buf=0;
    }
}

#define IOCTL_ENABLE_PRP  0
#define IOCTL_DISABLE_PRP 1
static int imx21video_ioctl (struct video_device *v,
		unsigned int cmd, void *arg)
{
    struct imx21video_dev *dev = (struct imx21video_dev *)v->priv;
    int ret = 0, i;
    
    switch(cmd) {
    case VIDIOCGCAP:
    {
	struct video_capability b;
	DPRINTK("VIDIOCGCAP: cmd: 0x%08x, arg: 0x%08x\n", cmd, (int)arg);
	strcpy(b.name, dev->vdev.name);
	b.type = VID_TYPE_CAPTURE | VID_TYPE_SCALES;
	b.channels = dev->channels;
	b.audios = dev->audios;
	b.maxwidth = dev->maxwin.w;
	b.maxheight = dev->maxwin.h;
	b.minwidth = dev->minwin.w;
	b.minheight = dev->minwin.h;
	if(copy_to_user(arg, &b, sizeof(b)))
	    return -EFAULT;
	return 0;
    }
    case VIDIOCGMBUF:
    {
	struct video_mbuf vm;
	DPRINTK("VIDIOCGMBUF: cmd: 0x%08x, arg: 0x%08x\n", cmd, (int)arg);
	memset(&vm, 0, sizeof(vm));
	vm.size = FBUF_SIZE * FBUFFERS;
	vm.frames = FBUFFERS;
	for (i = 0; i < FBUFFERS; i++)
	    vm.offsets[i] = i * FBUF_SIZE;
	if (copy_to_user((void *)arg, (void *)&vm, sizeof(vm)))
	    return -EFAULT;
	return 0;							
    }
    case VIDIOCMCAPTURE:
    {
	struct video_mmap vm;
	DPRINTK("VIDIOCMCAPTURE: cmd: 0x%08x, arg: 0x%08x\n", cmd, (int)arg);
	if (copy_from_user((void *) &vm, (void *) arg, sizeof(vm)))
	    return -EFAULT;
	down(&dev->lock);
	ret = vgrab(dev, &vm);
	up(&dev->lock);
	return ret;
    }
    case VIDIOCSYNC:
    {
    	DECLARE_WAITQUEUE(wait, current);
	DPRINTK("VIDIOCSYNC: cmd: 0x%08x, arg: 0x%08x\n", cmd, (int)arg);
	i = (int)arg;
	if (i < 0 || i >= FBUFFERS) {
	    DPRINTK("frame over\n");
	    return -EINVAL;
	}
	switch (dev->gbuf[i].stat) {
	case GBUFFER_UNUSED:
	    DPRINTK("buffer unused\n");
	    ret = -EINVAL;
	    break;
	case GBUFFER_GRABBING:
	    add_wait_queue(&g_capture_wait, &wait);
	    current->state = TASK_INTERRUPTIBLE;
	    while (dev->gbuf[i].stat == GBUFFER_GRABBING) {
		DPRINTK("%s%d: cap sync: sleep on %d\n", DEVNAME, dev->nr, i);
		schedule();
		if (signal_pending(current)) {
		    remove_wait_queue(&g_capture_wait, &wait);
		    current->state = TASK_RUNNING;
		    DPRINTK("pending error\n");
		    return -EINTR;
		}
	    }
	    remove_wait_queue(&g_capture_wait, &wait);
	    current->state = TASK_RUNNING;
	case GBUFFER_DONE:
	case GBUFFER_ERROR:
	    ret = (dev->gbuf[i].stat == GBUFFER_ERROR) ? -EIO : 0;
	    DPRINTK("%s%d: cap sync: buffer %d, retval %d\n",
		DEVNAME, dev->nr, i, ret);
	    dev->gbuf[i].stat = GBUFFER_UNUSED;
	}
	return ret;
    }
    case IOCTL_ENABLE_PRP:
	openprp = 1;
	_gCSI_REG->CTRL_REG1|=CSI_CTL_PRP;	
	break;
    case IOCTL_DISABLE_PRP:
	openprp = 0;
	_gCSI_REG->CTRL_REG2&=~CSI_CTL_PRP;
	break;
    }
    return 0;
}

static int do_imx21video_mmap(struct imx21video_dev *dev,
		const char *adr, unsigned long size)
{
    unsigned long start = (unsigned long)adr;
    unsigned long page,pos;

    DPRINTK("adr(0x%08lx), size(%d)\n", (unsigned long)adr, (int)size);
    if (size > FBUFFERS * FBUF_SIZE) {
	DPRINTK("size over\n");
	return -EINVAL;
    }
    if (!dev->fbuffer) {
	if(fbuffer_alloc(dev)) {
	    DPRINTK("fbuffer_alloc failed\n");
	    return -EINVAL;
	}
    }
    pos = (unsigned long)dev->fbuffer;
    while (size > 0) {
	page = kvirt_to_pa(pos);
	if (remap_page_range(start, page, PAGE_SIZE, PAGE_SHARED)) {
	    DPRINTK("remap_page_range failed\n");
	    return -EAGAIN;
	}
	start+=PAGE_SIZE;
	pos+=PAGE_SIZE;
	size-=PAGE_SIZE;
    }
    return 0;
}

static int imx21video_mmap(struct video_device *v, 
		const char *adr, unsigned long size)
{
    struct imx21video_dev *dev = (struct imx21video_dev *)v->priv;
    int ret;

    down(&dev->lock);
    ret = do_imx21video_mmap(dev, adr, size);
    up(&dev->lock);
    return ret;
}

static int i2c_csi_attach_adapter (struct i2c_adapter * adap)
{
    DPRINTK("adap %lx\n", (long)adap);
    /* find out the adapter for the I2C module in the DBMX1*/
	
    if (memcmp(adap->name, "DBMX I2C Adapter", 16) != 0 )
	return -ENODEV;

    /* store the adapter to the client driver */
    DPRINTK("Hear");
    i2c_imx21video_client.adapter = adap;

    return 0;
}

static void I2C_init(void)
{
    DPRINTK("Being I2C_init\n");
    /* 
     * call the i2c_add_driver() to register the driver 
     * to the Linux I2C system
     */
    if (i2c_add_driver( &i2c_csi_driver ) != 0) {
	printk("*** i2c_add_driver() failed ***\n");
	return;
    }
	
    DPRINTK("i2c_add_driver\n");
    /* 
     * attach the client to the adapter by calling the i2c_attach_client() 
     * function of the Linux I2C system
     */
    if (i2c_attach_client(&i2c_imx21video_client ) != 0) {	
	printk("*** i2c_attach_client() failed ***\n");
	i2c_del_driver(&i2c_csi_driver);
	return;
    }

    DPRINTK("i2c_attach_driver\n");
    i2c_imx21video_client.adapter->inc_use( i2c_imx21video_client.adapter );
    DPRINTK("End I2C_int\n");
}

static void I2C_cleanup(void)
{
    i2c_imx21video_client.adapter->dec_use(i2c_imx21video_client.adapter);
    i2c_detach_client(&i2c_imx21video_client );
    i2c_del_driver(&i2c_csi_driver);
}

//---------------------------
//
//	SENSOR Register Write
//
//---------------------------
static int I2C_write_16_check(u32 reg, u32 data)
{
    u32 d;
	
    if (I2C_write_16(reg,data) < 0) {
	printk("I2C Write Error\n");
	return -1;
    }
	
    if (I2C_read_16(reg,&d) < 0) {
	printk("I2C Read Error\n");
	return -1;
    }
	
    if (data != d) {
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
    msg.addr = i2c_imx21video_client.addr;
    msg.flags = EMBEDDED_REGISTER | I2C_M_WT;
    msg.len = 3;
    msg.buf = buf;
		
    ret = i2c_transfer(i2c_imx21video_client.adapter, &msg, 1);
    DPRINTK("I2C Write %x %x\n", reg, data);
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
    msg[0].addr =  i2c_imx21video_client.addr;
    msg[0].flags = EMBEDDED_REGISTER | I2C_M_WT;
    msg[0].len = 2;
    msg[0].buf = buf;

    msg[1].addr =  i2c_imx21video_client.addr;
    msg[1].flags = EMBEDDED_REGISTER | I2C_M_READ;
    msg[1].len = 2;
    msg[1].buf = buf;
	
    ret = i2c_transfer(i2c_imx21video_client.adapter, msg, 2);
    *_data = (buf[0]<<8)|(buf[1]);
	
    DPRINTK("ret=%d reg:%x, value:%x\n",ret,reg,*_data);
    return ret;
}

static void csi_intr_handler(int irq, void *dev_id, struct pt_regs *regs)
{
#ifdef SWAP_2_OF_4
    register u32 data,data2;
#endif
    if (_gCSI_REG->STS_REG&IRQ_SOF) {
	_gCSI_REG->STS_REG=IRQ_ALL;
	csi_disable_irq(IRQ_ALL);
	if (_gDmaChanel >= 0)
	    dbmx2_dma_start(_gDmaChanel);
	g_IRQ_SOF_count++;
	return;
    }

    if(_gCSI_REG->STS_REG&IRQ_OVER_RUN);
    {
	_gCSI_REG->STS_REG=IRQ_ALL;
	csi_disable_irq(IRQ_ALL);
	g_bad_frame++;
	return;
    }

    _gCSI_REG->STS_REG=_gCSI_REG->STS_REG;//Clear All irpt;
    DPRINTK("Unknow interrupt\n");
}

int imx21video_init(void)
{
    int ret, i;

    for (i = 0; i < MAX_IMX21VIDEO; i++) {
	imx21video_dev_t *d = (imx21video_dev_t *)&devs[i];
	memset(d, 0, sizeof(imx21video_dev_t));
	d->nr = i;
	d->slock = SPIN_LOCK_UNLOCKED;
	init_MUTEX(&d->lock);
	d->channels = 1;
	d->audios = 0;
	d->maxwin.w = 240;
	d->maxwin.h = 320;
	d->minwin.w = 32;
	d->minwin.h = 48;
	d->fbuffer = NULL;
	init_waitqueue_head(&d->capq);
	memcpy(&d->vdev, &imx21video_vdev, sizeof(struct video_device));	
	d->vdev.priv = d;
	ret = video_register_device(&d->vdev, VFL_TYPE_GRABBER, 0);
	if (ret < 0) {
	     printk("%s%d: Unable to register driver\n", DEVNAME, i);
	     return -ENODEV;
	}
    }
    return 0;
}

void imx21video_cleanup(void)
{
    return;
}

module_init(imx21video_init);
module_exit(imx21video_cleanup);

MODULE_PARM(base,"i");
MODULE_PARM(irq,"i");
MODULE_PARM(dmabase,"i");
MODULE_PARM(openprp, "i");
