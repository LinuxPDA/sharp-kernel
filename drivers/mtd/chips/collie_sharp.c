/*
 * MTD chip driver for non-CFI Sharp flash chips
 *
 * Copyright 2000,2001 David A. Schleef <ds@schleef.org>
 *           2000,2001 Lineo, Inc.
 *
 * $Id: sharp.c,v 1.4 2001/04/29 16:21:17 dwmw2 Exp $
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
#include <asm/hardware.h>

#define CMD_RESET		0xffffffff
#define CMD_READ_ID		0x90909090
#define CMD_READ_STATUS		0x70707070
#define CMD_CLEAR_STATUS	0x50505050
#define CMD_BLOCK_ERASE_1	0x20202020
#define CMD_BLOCK_ERASE_2	0xd0d0d0d0
#define CMD_BYTE_WRITE		0x40404040
#define CMD_SUSPEND		0xb0b0b0b0
#define CMD_RESUME		0xd0d0d0d0
#define CMD_SET_BLOCK_LOCK_1	0x60606060
#define CMD_SET_BLOCK_LOCK_2	0x01010101
#define CMD_SET_MASTER_LOCK_1	0x60606060
#define CMD_SET_MASTER_LOCK_2	0xf1f1f1f1
#define CMD_CLEAR_BLOCK_LOCKS_1	0x60606060
#define CMD_CLEAR_BLOCK_LOCKS_2	0xd0d0d0d0

#define SR_READY		0x80808080 // 1 = ready
#define SR_ERASE_SUSPEND	0x40404040 // 1 = block erase suspended
#define SR_ERROR_ERASE		0x20202020 // 1 = error in block erase or clear lock bits
#define SR_ERROR_WRITE		0x10101010 // 1 = error in byte write or set lock bit
#define	SR_VPP			0x08080808 // 1 = Vpp is low
#define SR_WRITE_SUSPEND	0x04040404 // 1 = byte write suspended
#define SR_PROTECT		0x02020202 // 1 = lock bit set
#define SR_RESERVED		0x01010101

#define SR_ERRORS (SR_ERROR_ERASE|SR_ERROR_WRITE|SR_VPP|SR_PROTECT)

#define	BLOCK_MASK		0xfffe0000
/* Configuration options */

//#undef AUTOUNLOCK  /* automatically unlocks blocks before erasing */
#define AUTOUNLOCK
struct mtd_info *collie_probe(struct map_info *);

static int collie_probe_map(struct map_info *map,struct mtd_info *mtd);

static int collie_read(struct mtd_info *mtd, loff_t from, size_t len,
	size_t *retlen, u_char *buf);
static int collie_write(struct mtd_info *mtd, loff_t from, size_t len,
	size_t *retlen, const u_char *buf);
static int collie_erase(struct mtd_info *mtd, struct erase_info *instr);
static void collie_sync(struct mtd_info *mtd);
static int collie_suspend(struct mtd_info *mtd);
static void collie_resume(struct mtd_info *mtd);
static void collie_destroy(struct mtd_info *mtd);

static int collie_write_oneword(struct map_info *map, struct flchip *chip,
	unsigned long adr, __u32 datum);
static int collie_erase_oneblock(struct map_info *map, struct flchip *chip,
	unsigned long adr);
#ifdef AUTOUNLOCK
static inline void collie_unlock_oneblock(struct map_info *map, struct flchip *chip,
	unsigned long adr);
#endif


struct collie_info{
	struct flchip *chip;
	int bogus;
	int chipshift;
	int numchips;
	struct flchip chips[1];
};

struct mtd_info *collie_probe(struct map_info *map);
static void collie_destroy(struct mtd_info *mtd);

static struct mtd_chip_driver collie_chipdrv = {
	probe: collie_probe,
	destroy: collie_destroy,
	name: "collie",
	module: THIS_MODULE
};
static void collie_udelay(unsigned long i){
	if (in_interrupt()) {
		udelay(i);
	} else {
		schedule();
	}

}
struct mtd_info *collie_probe(struct map_info *map)
{
	struct mtd_info *mtd = NULL;
	struct collie_info *collie = NULL;
	int width;

	mtd = kmalloc(sizeof(*mtd), GFP_KERNEL);
	if(!mtd)
		return NULL;

	collie = kmalloc(sizeof(*collie), GFP_KERNEL);
	if(!collie)
		return NULL;

	memset(mtd, 0, sizeof(*mtd));

	width = collie_probe_map(map,mtd);
	if(!width){
		kfree(mtd);
		kfree(collie);
		return NULL;
	}
	//MSC0 = 0xfff8e352;
        MSC0 = 0xfff8fff8;

	mtd->priv = map;
	mtd->type = MTD_NORFLASH;
	mtd->erase = collie_erase;
	mtd->read = collie_read;
	mtd->write = collie_write;
	mtd->sync = collie_sync;
	mtd->suspend = collie_suspend;
	mtd->resume = collie_resume;
	mtd->flags = MTD_CAP_NORFLASH;
	mtd->name = map->name;

	memset(collie, 0, sizeof(*collie));

	//collie->chipshift = 23;
	collie->chipshift = 24;
	collie->numchips = 1;
	collie->chips[0].start = 0;
	collie->chips[0].state = FL_READY;
	collie->chips[0].mutex = &collie->chips[0]._spinlock;
	collie->chips[0].word_write_time = 0;
	init_waitqueue_head(&collie->chips[0].wq);
	spin_lock_init(&collie->chips[0]._spinlock);

	map->fldrv = &collie_chipdrv;
	map->fldrv_priv = collie;

	MOD_INC_USE_COUNT;
	return mtd;
}

static int collie_probe_map(struct map_info *map,struct mtd_info *mtd)
{
	unsigned long tmp;
	unsigned long base = 0;
	u32 read0, read4;
	int width = 4;

	tmp = map->read32(map, base+0);

	map->write32(map, CMD_READ_ID, base+0);

	read0=map->read32(map, base+0);
	read4=map->read32(map, base+4);
	//if(read0 == 0x89898989){
	if(read0 == 0x00b000b0){
		//printk("Looks like collie flash\n");
		switch(read4){
		case 0xaaaaaaaa:
		case 0xa0a0a0a0:
			/* aa - LH28F016SCT-L95 2Mx8, 32 64k blocks*/
			/* a0 - LH28F016SCT-Z4  2Mx8, 32 64k blocks*/
			mtd->erasesize = 0x10000 * width;
			mtd->size = 0x200000 * width;
			return width;
		case 0xa6a6a6a6:
			/* a6 - LH28F008SCT-L12 1Mx8, 16 64k blocks*/
			/* a6 - LH28F008SCR-L85 1Mx8, 16 64k blocks*/
			mtd->erasesize = 0x10000 * width;
			mtd->size = 0x100000 * width;
			return width;
		case 0x00b000b0:
			/* a6 - LH28F640BFHE 8 64k * 2 chip blocks*/
			mtd->erasesize = 0x10000 * width / 2;
			mtd->size = 0x800000 * width / 2;
			return width;
#if 0
		case 0x00000000: /* unknown */
			/* XX - LH28F004SCT 512kx8, 8 64k blocks*/
			mtd->erasesize = 0x10000 * width;
			mtd->size = 0x100000 * width;
			return width;
#endif
		default:
			printk("Sort-of looks like collie flash, 0x%08x 0x%08x\n",
				read0,read4);
		}
	}else if((map->read32(map, base+0) == CMD_READ_ID)){
		/* RAM, probably */
		printk("Looks like RAM\n");
		map->write32(map, tmp, base+0);
	}else{
		printk("Doesn't look like collie flash, 0x%08x 0x%08x\n",
			read0,read4);
	}

	return 0;
}

/* This function returns with the chip->mutex lock held. */
static inline int collie_wait(struct map_info *map, struct flchip *chip)
{
	__u32 status;
	unsigned long timeo = jiffies + HZ;
	DECLARE_WAITQUEUE(wait, current);
	int adr = 0;

	//timeo = jiffies + HZ * 10;
retry:
	spin_lock_bh(chip->mutex);

	switch(chip->state){
	case FL_READY:
		map->write32(map,CMD_READ_STATUS,adr);
		chip->state = FL_STATUS;
	case FL_STATUS:
		status = map->read32(map,adr);
		if((status & SR_READY)==SR_READY)
			break;
		//printk(".status=%08x\n",status);
		spin_unlock_bh(chip->mutex);
		if (time_after(jiffies, timeo)) {
			printk("Waiting for chip to be ready timed out in erase\n");
			return -EIO;	
		}
		collie_udelay(1);
		goto retry;
	default:
		//printk("Waiting for chip\n");

		set_current_state(TASK_INTERRUPTIBLE);
		add_wait_queue(&chip->wq, &wait);

		spin_unlock_bh(chip->mutex);

		//schedule();
		collie_udelay(1);

		set_current_state(TASK_RUNNING);
		remove_wait_queue(&chip->wq, &wait);

		if(signal_pending(current))
			return -EINTR;

		timeo = jiffies + HZ;

		goto retry;
	}

	map->write32(map,CMD_RESET, adr);

	chip->state = FL_READY;

	return 0;
}

static void collie_release(struct flchip *chip)
{
	wake_up(&chip->wq);
	spin_unlock_bh(chip->mutex);
}

static int collie_read(struct mtd_info *mtd, loff_t from, size_t len,
	size_t *retlen, u_char *buf)
{
	struct map_info *map = mtd->priv;
	struct collie_info *collie = map->fldrv_priv;
	int chipnum;
	int ret = 0;
	int ofs = 0;

	chipnum = (from >> collie->chipshift);
	ofs = from & ((1 << collie->chipshift)-1);

	*retlen = 0;

	while(len){
		unsigned long thislen;

		if(chipnum>=collie->numchips)
			break;

		thislen = len;
		if(ofs+thislen >= (1<<collie->chipshift))
			thislen = (1<<collie->chipshift) - ofs;

		ret = collie_wait(map,&collie->chips[chipnum]);
		if(ret<0)
			break;

		map->copy_from(map,buf,ofs,thislen);

		collie_release(&collie->chips[chipnum]);

		*retlen += thislen;
		len -= thislen;
		buf += thislen;

		ofs = 0;
		chipnum++;
	}
	return ret;
}

static int collie_write(struct mtd_info *mtd, loff_t to, size_t len,
	size_t *retlen, const u_char *buf)
{
	struct map_info *map = mtd->priv;
	struct collie_info *collie = map->fldrv_priv;
	int ret = 0;
	int i,j;
	int chipnum;
	unsigned long ofs;
	union { u32 l; unsigned char uc[4]; } tbuf;

	*retlen = 0;

	while(len){
		tbuf.l = 0xffffffff;
		chipnum = to >> collie->chipshift;
		ofs = to & ((1<<collie->chipshift)-1);

		j=0;
		for(i=ofs&3;i<4 && len;i++){
			tbuf.uc[i] = *buf;
			buf++;
			to++;
			len--;
			j++;
		}
		collie_write_oneword(map, &collie->chips[chipnum], ofs&~3, tbuf.l);
		if(ret<0)
			return ret;
		(*retlen)+=j;
	}

	return 0;
}

static int collie_write_oneword(struct map_info *map, struct flchip *chip,
	unsigned long adr, __u32 datum)
{
	int ret;
	int try;
	int i;
	u32 status = 0;

	ret = collie_wait(map,chip);
	if(ret<0)
		return ret;

	for(try=0;try<10;try++){
		map->write32(map,CMD_BYTE_WRITE,adr);
		/* cpu_to_le32 -> hack to fix the writel be->le conversion */
		map->write32(map,cpu_to_le32(datum),adr);

		chip->state = FL_WRITING;

		map->write32(map,CMD_READ_STATUS,adr);

		for(i=0;i<100;i++){
			status = map->read32(map,adr);
			if((status & SR_READY)==SR_READY)
				break;
		}

#ifdef AUTOUNLOCK
		if (status & SR_PROTECT){ /* lock block */
#if 0
			map->write32(map,CMD_CLEAR_BLOCK_LOCKS_1,adr);
			map->write32(map,CMD_CLEAR_BLOCK_LOCKS_2,adr);
#else
			map->write32(map,CMD_CLEAR_STATUS,adr);
			//map->write32(map,CMD_RESET,adr);

			collie_unlock_oneblock(map,chip,adr);

			map->write32(map,CMD_CLEAR_STATUS,adr);
			map->write32(map,CMD_RESET,adr);

#endif
			continue;
		}
#endif
		if(i==100){
			printk("collie: timed out writing\n");
		}

		if(!(status&SR_ERRORS))
			break;

		printk("collie: error writing byte at addr=%08lx status=%08x\n",adr,status);

		map->write32(map,CMD_CLEAR_STATUS,adr);
	}
	map->write32(map,CMD_RESET,adr);
	chip->state = FL_READY;

	collie_release(chip);

	return 0;
}

static int collie_erase(struct mtd_info *mtd, struct erase_info *instr)
{
	struct map_info *map = mtd->priv;
	struct collie_info *collie = map->fldrv_priv;
	unsigned long adr,len;
	int chipnum, ret=0;

//printk("collie_erase()\n");
	if(instr->addr & (mtd->erasesize - 1))
		return -EINVAL;
	if(instr->len & (mtd->erasesize - 1))
		return -EINVAL;
	if(instr->len + instr->addr > mtd->size)
		return -EINVAL;

	chipnum = instr->addr >> collie->chipshift;
	adr = instr->addr & ((1<<collie->chipshift)-1);
	len = instr->len;
//printk("--+* erase adr %08x [%08x]\n",adr,len);
	while(len){
		ret = collie_erase_oneblock(map, &collie->chips[chipnum], adr);
		if(ret)return ret;
		if (adr >= 0xfe0000) {
			adr += mtd->erasesize / 8;
			len -= mtd->erasesize / 8;
		} else {

			adr += mtd->erasesize;
			len -= mtd->erasesize;
		}
		if(adr >> collie->chipshift){
			adr = 0;
			chipnum++;
			if(chipnum>=collie->numchips)
				break;
		}
	}
	instr->state = MTD_ERASE_DONE;
	if(instr->callback)
		instr->callback(instr);
//printk("--+* erase end %08x \n\n",adr);

	return 0;
}

static inline int collie_do_wait_for_ready(struct map_info *map, struct flchip *chip,
	unsigned long adr)
{
	int ret;
	int timeo;
	int status;
	DECLARE_WAITQUEUE(wait, current);

	map->write32(map,CMD_READ_STATUS,adr);
	status = map->read32(map,adr);

	timeo = jiffies + HZ * 10;

	while(jiffies<timeo){
		map->write32(map,CMD_READ_STATUS,adr);
		status = map->read32(map,adr);
		if((status & SR_READY)==SR_READY){
			ret = 0;
			goto out;
		}
		set_current_state(TASK_INTERRUPTIBLE);
		add_wait_queue(&chip->wq, &wait);

		spin_unlock_bh(chip->mutex);

		schedule_timeout(1);
		schedule();

		spin_lock_bh(chip->mutex);

		remove_wait_queue(&chip->wq, &wait);
		set_current_state(TASK_RUNNING);
#if 0
		if (signal_pending(current)){
			ret = -EINTR;
			goto out;
		}
#endif
		
	}
	ret = -ETIME;
out:
	return ret;
}

static int collie_erase_oneblock(struct map_info *map, struct flchip *chip,
	unsigned long adr)
{
	int ret;
	//int timeo;
	int status;
	//int i;

	//printk("collie_erase_oneblock()\n");

	ret = collie_wait(map,chip);
	if (ret <0)
		return ret;
#ifdef AUTOUNLOCK
	/* This seems like a good place to do an unlock */
	collie_unlock_oneblock(map,chip,adr);
#endif

	map->write32(map,CMD_BLOCK_ERASE_1,adr);
	map->write32(map,CMD_BLOCK_ERASE_2,adr);

	chip->state = FL_ERASING;

	ret = collie_do_wait_for_ready(map,chip,adr);
	if(ret<0){
		spin_unlock_bh(chip->mutex);
		return ret;
	}
	map->write32(map,CMD_READ_STATUS,adr);
	status = map->read32(map,adr);
	if(!(status&SR_ERRORS)){
		map->write32(map,CMD_RESET,adr);
		chip->state = FL_READY;
		spin_unlock_bh(chip->mutex);
		return 0;
	}

	printk("collie: error erasing block at addr=%08lx status=%08x\n",adr,status);
	map->write32(map,CMD_CLEAR_STATUS,adr);

	//wake_up(&chip->wq);
	//spin_unlock_bh(chip->mutex);
	
	collie_release(chip);
	
	return -EIO;
}

#ifdef AUTOUNLOCK
static inline void collie_unlock_oneblock(struct map_info *map, struct flchip *chip,
	unsigned long adr)
{
	u32 status;

	map->write32(map,CMD_CLEAR_BLOCK_LOCKS_1,adr & BLOCK_MASK);
	map->write32(map,CMD_CLEAR_BLOCK_LOCKS_2,adr & BLOCK_MASK);

	collie_do_wait_for_ready(map,chip,adr);

	status = map->read32(map,adr);
	if(!(status&SR_ERRORS)){
		map->write32(map,CMD_RESET,adr);
		chip->state = FL_READY;
		return;
	}

	printk("collie: error unlocking block at addr=%08lx status=%08x\n",adr,status);
	map->write32(map,CMD_CLEAR_STATUS,adr);
}
#endif

static void collie_sync(struct mtd_info *mtd)
{
	//printk("collie_sync()\n");
}

static int collie_suspend(struct mtd_info *mtd)
{
	struct map_info *map = mtd->priv;
	struct collie_info *collie = map->fldrv_priv;
	int i;
	struct flchip *chip;
	int ret = 0;

	//printk("collie_suspend()\n");
#if 1
	for (i=0; !ret && i<collie->numchips; i++) {
                chip = &collie->chips[i];
		ret = collie_wait(map,chip);


		if (ret){
			ret = -EAGAIN;
		} else {
			chip->state = FL_PM_SUSPENDED;
			spin_unlock_bh(chip->mutex);
		}
	}
#else
	for (i=0; !ret && i<collie->numchips; i++) {
                chip = &collie->chips[i];

                spin_lock_bh(chip->mutex);

                switch(chip->state) {
		case FL_READY:
		case FL_STATUS:
			chip->oldstate = chip->state;
			chip->state = FL_PM_SUSPENDED;
			/* No need to wake_up() on this state change -
			 * as the whole point is that nobody can do anything
			 * with the chip now anyway.
			 */
		case FL_PM_SUSPENDED:
			break;

		default:
			ret = -EAGAIN;
			break;
		}		
		spin_unlock_bh(chip->mutex);
	}

	/* Unlock the chips again */
	if (ret) {
		for (i--; i >=0; i--) {
			chip = &collie->chips[i];
			
			spin_lock_bh(chip->mutex);
			if (chip->state == FL_PM_SUSPENDED) {
				chip->state = chip->oldstate;
				wake_up(&chip->wq);
			}
			spin_unlock_bh(chip->mutex);
		}
	}
#endif	
	return ret;

}

static void collie_resume(struct mtd_info *mtd)
{
	struct map_info *map = mtd->priv;
	struct collie_info *collie = map->fldrv_priv;
	int i;
	struct flchip *chip;
	
	//printk("collie_resume()\n");
#if 0
	for (i=0; i<collie->numchips; i++) {
		chip = &collie->chips[i];
		if (chip->state == FL_PM_SUSPENDED) {
			map->write32(map,CMD_RESET,chip->start);
			chip->state = FL_READY;
			collie_release(chip);
		}
	}
#else	
	for (i=0; i<collie->numchips; i++) {
		chip = &collie->chips[i];
	
		spin_lock_bh(chip->mutex);	
		
		if (chip->state == FL_PM_SUSPENDED) {
			/* We need to force it back to a known state. */
			//cfi_write(map, CMD(0xff), 0);
			map->write32(map,CMD_RESET,chip->start);
			chip->state = FL_READY;
			wake_up(&chip->wq);
		}
		
		spin_unlock_bh(chip->mutex);
	}
#endif
}

static void collie_destroy(struct mtd_info *mtd)
{
	struct map_info *map = mtd->priv;
	struct collie_info *collie = map->fldrv_priv;
	
	//printk("collie_destroy()\n");
	//kfree(collie->cmdset_priv);
	kfree(collie);

}

#if LINUX_VERSION_CODE < 0x020212 && defined(MODULE)
#define collie_probe_init init_module
#define collie_probe_exit cleanup_module
#endif

int __init collie_probe_init(void)
{
	printk("MTD Sharp chip driver <ds@lineo.com>\n");

	register_mtd_chip_driver(&collie_chipdrv);

	return 0;
}

static void __exit collie_probe_exit(void)
{
	unregister_mtd_chip_driver(&collie_chipdrv);
}

module_init(collie_probe_init);
module_exit(collie_probe_exit);

