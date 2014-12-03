/*
 * MTD chip driver for pre-CFI Sharp flash chips
 *
 * Tested On Iris , LHF32J04
 *
 * ------------------
 * Based On....
 *
 * Copyright 2000,2001 David A. Schleef <ds@schleef.org>
 *           2000,2001 Lineo, Inc.
 *
 * $Id: sharp16.c,v 1.7 2001/07/09 05:44:43 yata Exp $
 *
 * Devices supported:
 *   LH28F016SCT Symmetrical block flash memory, 2Mx8
 *   LH28F008SCT Symmetrical block flash memory, 1Mx8
 *
 * Documentation:
 *   http://www.sharpmeg.com/datasheets/memic/flashcmp/
 *   http://www.sharpmeg.com/datasheets/memic/flashcmp/01symf/16m/016sctl9.pdf
 *   016sctl9.pdf
 *
 * Limitations:
 *   This driver only supports 4x1 arrangement of chips.
 *   Not tested on anything but PowerPC.
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/version.h>
#include <linux/types.h>
#include <linux/sched.h>
#include <linux/errno.h>
#include <linux/mtd/map.h>
#include <linux/mtd/cfi.h>
#include <linux/delay.h>

#define CMD_RESET		0xff
#define CMD_READ_ID		0x90
#define CMD_READ_STATUS		0x70
#define CMD_CLEAR_STATUS	0x50
#define CMD_BLOCK_ERASE_1	0x20
#define CMD_BLOCK_ERASE_2	0xd0
#define CMD_BYTE_WRITE		0x40
#define CMD_SUSPEND		0xb0
#define CMD_RESUME		0xd0
#define CMD_SET_BLOCK_LOCK_1	0x60
#define CMD_SET_BLOCK_LOCK_2	0x01
#define CMD_SET_MASTER_LOCK_1	0x60
#define CMD_SET_MASTER_LOCK_2	0xf1
#define CMD_CLEAR_BLOCK_LOCKS_1	0x60
#define CMD_CLEAR_BLOCK_LOCKS_2	0xd0

#define SR_READY		0x80 // 1 = ready
#define SR_ERASE_SUSPEND	0x40 // 1 = block erase suspended
#define SR_ERROR_ERASE		0x20 // 1 = error in block erase or clear lock bits
#define SR_ERROR_WRITE		0x10 // 1 = error in byte write or set lock bit
#define	SR_VPP			0x08 // 1 = Vpp is low
#define SR_WRITE_SUSPEND	0x04 // 1 = byte write suspended
#define SR_PROTECT		0x02 // 1 = lock bit set
#define SR_RESERVED		0x01

#define SR_ERRORS (SR_ERROR_ERASE|SR_ERROR_WRITE|SR_VPP|SR_PROTECT)

/* Configuration options */

#define AUTOUNLOCK  /* automatically unlocks blocks before erasing */

struct mtd_info *sharp16_probe(struct map_info *);

static int sharp_probe_map(struct map_info *map,struct mtd_info *mtd);

static int sharp_read(struct mtd_info *mtd, loff_t from, size_t len,
	size_t *retlen, u_char *buf);
static int sharp_write(struct mtd_info *mtd, loff_t from, size_t len,
	size_t *retlen, const u_char *buf);
static int sharp_erase(struct mtd_info *mtd, struct erase_info *instr);
static void sharp_sync(struct mtd_info *mtd);
static int sharp_suspend(struct mtd_info *mtd);
static void sharp_resume(struct mtd_info *mtd);
static void sharp_destroy(struct mtd_info *mtd);

static int sharp_write_oneword(struct map_info *map, struct flchip *chip,
	unsigned long adr, __u16 datum);
static int sharp_erase_oneblock(struct map_info *map, struct flchip *chip,
	unsigned long adr);
#ifdef AUTOUNLOCK
static void sharp_unlock_oneblock(struct map_info *map, struct flchip *chip,
	unsigned long adr);
#endif


struct sharp_info{
	struct flchip *chip;
	int bogus;
	int chipshift;
	int numchips;
	struct flchip chips[1];
};


#if 0
#define DEBUGPRINT(s)   printk s
#define DPRINTK(x, args...)  printk(__FUNCTION__ ": " x,##args)
#else
#define DEBUGPRINT(s)   
#define DPRINTK(x, args...)
#endif

static struct mtd_chip_driver sharp_chipdrv = {
        probe: sharp16_probe,
        destroy: sharp_destroy,
        name: "sharp16",
        module: THIS_MODULE
};


struct mtd_info *sharp16_probe(struct map_info *map)
{
	struct mtd_info *mtd = NULL;
	struct sharp_info *sharp = NULL;
	int width;

	mtd = kmalloc(sizeof(*mtd), GFP_KERNEL);
	if(!mtd)
		return NULL;

	sharp = kmalloc(sizeof(*sharp), GFP_KERNEL);
	if(!sharp)
		return NULL;

	memset(mtd, 0, sizeof(*mtd));

	width = sharp_probe_map(map,mtd);
	if(!width){
		kfree(mtd);
		kfree(sharp);
		return NULL;
	}

	mtd->priv = map;
	mtd->type = MTD_NORFLASH;
	mtd->erase = sharp_erase;
	mtd->read = sharp_read;
	mtd->write = sharp_write;
	mtd->sync = sharp_sync;
	mtd->suspend = sharp_suspend;
	mtd->resume = sharp_resume;
	mtd->flags = MTD_CAP_NORFLASH;
	mtd->name = map->name;

	memset(sharp, 0, sizeof(*sharp));
	sharp->chipshift = 23;
	sharp->numchips = 1;
	sharp->chips[0].start = 0;
	sharp->chips[0].state = FL_READY;
	sharp->chips[0].mutex = &sharp->chips[0]._spinlock;
	sharp->chips[0].word_write_time = 0;
	init_waitqueue_head(&sharp->chips[0].wq);
	spin_lock_init(&sharp->chips[0]._spinlock);

	map->fldrv = &sharp_chipdrv;
	map->fldrv_priv = sharp;

	MOD_INC_USE_COUNT;
	return mtd;
}

static int sharp_probe_map(struct map_info *map,struct mtd_info *mtd)
{
	unsigned long tmp;
	unsigned long base = 0;
	u32 read0, read2;
	int width = 4;

	tmp = map->read16(map, base+0);

	map->write16(map, CMD_READ_ID, base+0);

	read0=map->read16(map, base+0);
	read2=map->read16(map, base+2);
	if(read0 == 0xb0 ){
	  switch(read2){
	  case 0xe2: /* LH28F320BJHE */
	    printk("Looks like sharp flush LH28F320BJHE\n");
	    mtd->erasesize = 0x4000 * width;
	    mtd->size = 0xfc000 * width;
	    return width;
	  case 0xb0: /* LH28F640BX */
	    printk("Looks like sharp flush LH28F640BX\n");
	    mtd->erasesize = 0x4000 * width;
	    /* mtd->size = 0x1fc000 * width; */
	    mtd->size = 0x200000 * width;
	    map->write16(map,CMD_RESET,base+0);
	    return width;
	  default:
	    printk("Sort-of looks like sharp flash, 0x%08x 0x%08x\n",read0,read2);
	    break;
	  }
	}else if((map->read16(map, base+0) == CMD_READ_ID)){
	  /* RAM, probably */
	  printk("Looks like RAM\n");
	  map->write16(map, tmp, base+0);
	}else{
	  printk("Doesn't look like sharp flash, 0x%08x 0x%08x\n",
		 read0,read2);
	}

	return 0;
}

/* This function returns with the chip->mutex lock held. */
static int sharp_wait(struct map_info *map, struct flchip *chip)
{
	__u16 status;
	unsigned long timeo = jiffies + HZ;
	DECLARE_WAITQUEUE(wait, current);
	int adr = 0;

retry:

	DPRINTK("retry\n");

	spin_lock_bh(chip->mutex);

	switch(chip->state){
	case FL_READY:
	DPRINTK("FL_READY\n");
		map->write16(map,CMD_READ_STATUS,adr);
		chip->state = FL_STATUS;
	case FL_STATUS:
	DPRINTK("FL_STATUS\n");
		status = map->read16(map,adr);
//printk("status=%08x\n",status);

		udelay(100);
		if((status & SR_READY)!=SR_READY){
//printk(".status=%08x\n",status);
			udelay(100);
		}
		break;
	default:
	DPRINTK("default\n");
	//		printk("Waiting for chip\n");

		set_current_state(TASK_INTERRUPTIBLE);
		add_wait_queue(&chip->wq, &wait);

		spin_unlock_bh(chip->mutex);

	DPRINTK("schedule enter\n");

		schedule();

	DPRINTK("schedule leave\n");

		remove_wait_queue(&chip->wq, &wait);

		//		printk("remove_wait_queue\n");

		if(signal_pending(current))
			return -EINTR;

		timeo = jiffies + HZ;

	DPRINTK("timeo\n");

		goto retry;
	}

	DPRINTK("exit\n");

	map->write16(map,CMD_RESET, adr);

	chip->state = FL_READY;

	return 0;
}

static void sharp_release(struct flchip *chip)
{
	DPRINTK("enter\n");
	wake_up(&chip->wq);
	spin_unlock_bh(chip->mutex);
	DPRINTK("leave\n");
}

static int sharp_read(struct mtd_info *mtd, loff_t from, size_t len,
	size_t *retlen, u_char *buf)
{
	struct map_info *map = mtd->priv;
	struct sharp_info *sharp = map->fldrv_priv;
	int chipnum;
	int ret = 0;
	int ofs = 0;

	chipnum = (from >> sharp->chipshift);
	ofs = from & ((1 << sharp->chipshift)-1);

	*retlen = 0;

	DPRINTK("enter\n");

	while(len){
		unsigned long thislen;

		if(chipnum>=sharp->numchips)
			break;

		thislen = len;
		if(ofs+thislen >= (1<<sharp->chipshift))
			thislen = (1<<sharp->chipshift) - ofs;

		DPRINTK("wait enter\n");

		ret = sharp_wait(map,&sharp->chips[chipnum]);

		DPRINTK("wait leave\n");

		if(ret<0)
			break;

		DPRINTK("copy from enter\n");

		map->copy_from(map,buf,ofs,thislen);

		DPRINTK("copy from leave\n");

		sharp_release(&sharp->chips[chipnum]);

		DPRINTK("release\n");

		*retlen += thislen;
		len -= thislen;
		buf += thislen;

		ofs = 0;
		chipnum++;
	}

	DPRINTK("leave\n");

	return ret;
}

static int sharp_write(struct mtd_info *mtd, loff_t to, size_t len,
	size_t *retlen, const u_char *buf)
{
	struct map_info *map = mtd->priv;
	struct sharp_info *sharp = map->fldrv_priv;
	int ret = 0;
	int chipnum;
	unsigned long ofs;
	union { u16 w; unsigned char uc[2]; } tbuf;

	*retlen = 0;

	DPRINTK("enter\n");

	while(len){
		tbuf.w = 0xffff;
		chipnum = to >> sharp->chipshift;
		ofs = to & ((1<<sharp->chipshift)-1);

		tbuf.uc[0] = *buf;
		buf++;
		len--;
		to++;
		tbuf.uc[1] = *buf;
		buf++;
		len--;
		to++;

	DPRINTK("oneword enter\n");

		sharp_write_oneword(map, &sharp->chips[chipnum], ofs&~1, tbuf.w);

	DPRINTK("oneword leave\n");

		if(ret<0)
			return ret;
		(*retlen)+=2;
	}

	DPRINTK("leave\n");

	return 0;
}

static int sharp_write_oneword(struct map_info *map, struct flchip *chip,
	unsigned long adr, __u16 datum)
{
	int ret;
	int timeo;
	int try;
	int i;
	int status = 0;

	DPRINTK("enter\n");

	ret = sharp_wait(map,chip);

	DPRINTK("wait\n");

	for(try=0;try<10;try++){
		map->write16(map,CMD_BYTE_WRITE,adr);
		/* cpu_to_le32 -> hack to fix the writel be->le conversion */
		map->write16(map,datum,adr);

		chip->state = FL_WRITING;

		timeo = jiffies + (HZ/2);

		map->write16(map,CMD_READ_STATUS,adr);
		for(i=0;i<100;i++){
			status = map->read16(map,adr);
			if((status & SR_READY)==SR_READY)
				break;
		}
		if(i==100){
			printk("sharp: timed out writing\n");
		}

		if(!(status&SR_ERRORS))
			break;

		printk("sharp: error writing byte at addr=%08lx status=%08x\n",adr,status);

		map->write16(map,CMD_CLEAR_STATUS,adr);
	}

	DPRINTK("done\n");

	map->write16(map,CMD_RESET,adr);
	chip->state = FL_READY;

	DPRINTK("wake up\n");

	wake_up(&chip->wq);
	spin_unlock_bh(chip->mutex);

	DPRINTK("leave\n");

	return 0;
}

static int sharp_erase(struct mtd_info *mtd, struct erase_info *instr)
{
	struct map_info *map = mtd->priv;
	struct sharp_info *sharp = map->fldrv_priv;
	unsigned long adr,len;
	int chipnum, ret=0;

	DPRINTK("enter\n");

	//	printk("sharp_erase()\n");
	if(instr->addr & (mtd->erasesize - 1))
	  return -EINVAL;
	if(instr->len & (mtd->erasesize - 1))
	  return -EINVAL;
	if(instr->len + instr->addr > mtd->size)
	  return -EINVAL;

	DPRINTK("do\n");

	chipnum = instr->addr >> sharp->chipshift;
	adr = instr->addr & ((1<<sharp->chipshift)-1);
	len = instr->len;

	while(len){
	  DPRINTK("one enter\n");
		ret = sharp_erase_oneblock(map, &sharp->chips[chipnum], adr);
	  DPRINTK("one leave\n");
		if(ret) return ret;

		adr += mtd->erasesize;
		len -= mtd->erasesize;
		if(adr >> sharp->chipshift){
			adr = 0;
			chipnum++;
			if(chipnum>=sharp->numchips)
				break;
		}
	}

	DPRINTK("callback\n");

	if(instr->callback)
		instr->callback(instr);

	DPRINTK("done\n");

	return 0;
}

static int sharp_do_wait_for_ready(struct map_info *map, struct flchip *chip,
	unsigned long adr)
{
	int ret;
	int timeo;
	int status;
	DECLARE_WAITQUEUE(wait, current);

	DPRINTK("enter\n");

	map->write16(map,CMD_READ_STATUS,adr);
	status = map->read16(map,adr);

	timeo = jiffies + HZ*10;

	while(jiffies<timeo){
		map->write16(map,CMD_READ_STATUS,adr);
		status = map->read16(map,adr);
		if((status & SR_READY)==SR_READY){
			ret = 0;
			goto out;
		}

		//		printk("wait for ready\n");

		DPRINTK("add_wait_queue\n");

		set_current_state(TASK_INTERRUPTIBLE);
		add_wait_queue(&chip->wq, &wait);

		//spin_unlock_bh(chip->mutex);

		schedule_timeout(1);
		schedule();
		remove_wait_queue(&chip->wq, &wait);

		DPRINTK("remove wait queue\n");

		//		printk("wait remove wait queue\n");

		//spin_lock_bh(chip->mutex);
		
		if (signal_pending(current)){
			ret = -EINTR;
			goto out;
		}
		
	}
	ret = -ETIME;
out:

	DPRINTK("done\n");

	return ret;
}

static int sharp_erase_oneblock(struct map_info *map, struct flchip *chip,
	unsigned long adr)
{
	int ret;
	//int timeo;
	int status;
	//int i;

	//	printk("sharp_erase_oneblock()\n");

#ifdef AUTOUNLOCK
	/* This seems like a good place to do an unlock */
	sharp_unlock_oneblock(map,chip,adr);
#endif

	DPRINTK("enter\n");

	map->write16(map,CMD_BLOCK_ERASE_1,adr);
	map->write16(map,CMD_BLOCK_ERASE_2,adr);

	chip->state = FL_ERASING;

	DPRINTK("wait\n");

	ret = sharp_do_wait_for_ready(map,chip,adr);

	DPRINTK("ready\n");

	if(ret<0) return ret;


	map->write16(map,CMD_READ_STATUS,adr);
	status = map->read16(map,adr);

	if(!(status&SR_ERRORS)){
		map->write16(map,CMD_RESET,adr);
		chip->state = FL_READY;
		//spin_unlock_bh(chip->mutex);
		return 0;
	}

	printk("sharp: error erasing block at addr=%08lx status=%08x\n",adr,status);
	map->write16(map,CMD_CLEAR_STATUS,adr);

	//spin_unlock_bh(chip->mutex);

	DPRINTK("done\n");

	return -EIO;
}

#ifdef AUTOUNLOCK
static void sharp_unlock_oneblock(struct map_info *map, struct flchip *chip,
	unsigned long adr)
{
	int i;
	int status;

	map->write16(map,CMD_CLEAR_BLOCK_LOCKS_1,adr);
	map->write16(map,CMD_CLEAR_BLOCK_LOCKS_2,adr);

	udelay(100);

	status = map->read16(map,adr);
	//	printk("status=%08x\n",status);

	DPRINTK("enter\n");


	for(i=0;i<1000;i++){
		//map->write16(map,CMD_READ_STATUS,adr);
		status = map->read16(map,adr);
		if((status & SR_READY)==SR_READY)
			break;
		udelay(100);
	}
	if(i==1000){
		printk("sharp: timed out unlocking block\n");
	}

	if(!(status&SR_ERRORS)){
		map->write16(map,CMD_RESET,adr);
		chip->state = FL_READY;
		return;
	}

	printk("sharp: error unlocking block at addr=%08lx status=%08x\n",adr,status);
	map->write16(map,CMD_CLEAR_STATUS,adr);

	DPRINTK("done\n");

}
#endif

static void sharp_sync(struct mtd_info *mtd)
{
	printk("sharp_sync()\n");
}

static int sharp_suspend(struct mtd_info *mtd)
{
	printk("sharp_suspend()\n");
	return 0;
	//return -EINVAL;
}

static void sharp_resume(struct mtd_info *mtd)
{
	printk("sharp_resume()\n");
	
}

static void sharp_destroy(struct mtd_info *mtd)
{
	printk("sharp_destroy()\n");

}

#if LINUX_VERSION_CODE < 0x020212 && defined(MODULE)
#define sharp_probe_init init_module
#define sharp_probe_exit cleanup_module
#endif

int __init sharp_probe_init(void)
{
	printk("MTD Sharp 16 chip driver\n");

	register_mtd_chip_driver(&sharp_chipdrv);

	return 0;
}

static void __exit sharp_probe_exit(void)
{
	unregister_mtd_chip_driver(&sharp_chipdrv);
}

module_init(sharp_probe_init);
module_exit(sharp_probe_exit);

