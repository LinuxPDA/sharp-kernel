/*
 *  linux/include/asm-sh/ide.h
 *
 *  Copyright (C) 1994-1996  Linus Torvalds & authors
 */

/*
 *  This file contains the i386 architecture specific IDE code.
 *  In future, SuperH code.
 */

#ifndef __ASM_SH_IDE_H
#define __ASM_SH_IDE_H

#ifdef __KERNEL__

#include <linux/config.h>
#include <asm/machvec.h>
#include <asm/irq.h>

#if defined(CONFIG_SH_MS7727RP)
#include <asm/ms7727rp.h>
#elif defined(CONFIG_SH_MS7290CP)
#include <asm/ms7290cp.h>
#elif defined(CONFIG_SH_MS7710SE)
#include <asm/ms7710se.h>
#elif defined(CONFIG_SH_MS7720RP)
#include <asm/ms7720rp.h>
#elif defined(CONFIG_SH_SOLUTION_ENGINE)
//#include <asm/hitachi_se.h>
#include <asm/mrshpc.h>
#elif defined(CONFIG_SH_RTS7751R2D)
#include <asm/renesas_rts7751r2d.h>
#elif defined(CONFIG_SH_7760_SOLUTION_ENGINE)
#include <asm/mrshpc.h>
#endif

#ifndef MAX_HWIFS
#if defined(CONFIG_SH_SOLUTION_ENGINE) || \
    defined(CONFIG_SH_MS7727RP) || \
    defined(CONFIG_SH_MS7710SE) || \
    defined(CONFIG_SH_MS7720RP) || \
    defined(CONFIG_SH_MS7290CP)
#define MAX_HWIFS	1
#elif defined(CONFIG_SH_7760_SOLUTION_ENGINE)

#if defined(CONFIG_CF_ENABLER)
#define MAX_HWIFS       2
#else
#define MAX_HWIFS       1
#endif
#else
/* Should never have less than 2, ide-pci.c(ide_match_hwif) requires it */
#define MAX_HWIFS	2
#endif
#endif

#define ide__sti()	__sti()

#if defined(CONFIG_SH_7760_SOLUTION_ENGINE)
static __inline__ int ide_default_irq_ms7760cp(ide_ioreg_t base)
{
        switch (base) {
#if defined(CONFIG_CF_ENABLER)
                case PA_MRSHPC_IO + 0x01f0: return 10;  /* SIRQ0# */
                case 0x0170: return 7;  /* IRQ1# */
#else
                case 0x01f0: return 7;  /* IRQ1# */
#endif
                default:
                        return 0;
        }
}
#endif

#if defined(CONFIG_SH_RTS7751R2D)
static __inline__ int ide_default_irq_rts7751r2d(ide_ioreg_t base)
{
	switch (base) {
	case 0x1f0: return IRQ_CFCARD;
	case 0x170: return IRQ_PCMCIA;
	default:
		return 0;
	}
}
#endif

static __inline__ int ide_default_irq_hp600(ide_ioreg_t base)
{
	switch (base) {
		case 0x01f0: return 93;
		case 0x0170: return 94;
		default:
			return 0;
	}
}

static __inline__ int ide_default_irq(ide_ioreg_t base)
{
#if defined(CONFIG_SH_MS7727RP) || defined(CONFIG_SH_MS7290CP) || defined(CONFIG_SH_MS7710SE) || defined(CONFIG_SH_MS7720RP)
	return MRSHPC_IRQ;
#elif defined(CONFIG_SH_SOLUTION_ENGINE)
	return MRSHPC_IRQ;
#elif defined(CONFIG_SH_7760_SOLUTION_ENGINE)
        return ide_default_irq_ms7760cp(base);
#else
	if (MACH_HP600) {
		return ide_default_irq_hp600(base);
	}

#if defined(CONFIG_SH_RTS7751R2D)
	if (MACH_RTS7751R2D) {
		return ide_default_irq_rts7751r2d(base);
	}
#endif

	switch (base) {
		case 0x01f0: return 14;
		case 0x0170: return 15;
		default:
			return 0;
	}
#endif
}

static __inline__ ide_ioreg_t ide_default_io_base_hp600(int index)
{
	switch (index) {
		case 0:	
			return 0x01f0;
		case 1:	
			return 0x0170;
		default:
			return 0;
	}
}

static __inline__ ide_ioreg_t ide_default_io_base(int index)
{
#if defined(CONFIG_SH_MS7727RP) || defined(CONFIG_SH_MS7290CP) || defined(CONFIG_SH_MS7710SE) || defined(CONFIG_SH_MS7720RP)
	return (MRSHPC_IO_BASE + 0x01f0);
#elif defined(CONFIG_SH_SOLUTION_ENGINE)
	return (PA_MRSHPC_IO + 0x01f0);
#elif defined(CONFIG_SH_7760_SOLUTION_ENGINE)

        switch (index) {
                case 0:
#if !defined(CONFIG_CF_ENABLER)
						return 0x1f0;
#else
                        return (PA_MRSHPC_IO + 0x01f0);
#endif
                case 1:
                        return 0x170;
                default:
                        return 0;
        }
#else
	if (MACH_HP600) {
		return ide_default_io_base_hp600(index);
	}
	switch (index) {
		case 0:	
#if defined(CONFIG_SH_RTS7751R2D)
		{
			unsigned short *wk = (unsigned short *)PA_IRLMON;
			if (*wk & 0x0100){
				return 0x1f0;	/* CF detect */
			}else{
				return 0;	/* CF not detect */
			}
		}
#else
			return 0x1f0;
#endif
		case 1:	
			return 0x170;
		default:
			return 0;
	}
#endif
}

static __inline__ void ide_init_hwif_ports(hw_regs_t *hw, ide_ioreg_t data_port, ide_ioreg_t ctrl_port, int *irq)
{
	ide_ioreg_t reg = data_port;
	int i;

	for (i = IDE_DATA_OFFSET; i <= IDE_STATUS_OFFSET; i++) {
		hw->io_ports[i] = reg;
		reg += 1;
	}
	if (ctrl_port) {
		hw->io_ports[IDE_CONTROL_OFFSET] = ctrl_port;
	} else {
		hw->io_ports[IDE_CONTROL_OFFSET] = hw->io_ports[IDE_DATA_OFFSET] + 0x206;
	}
	if (irq != NULL)
		*irq = 0;
	hw->io_ports[IDE_IRQ_OFFSET] = 0;
}

static __inline__ void ide_init_default_hwifs(void)
{
#if !defined(CONFIG_BLK_DEV_IDEPCI) || defined(CONFIG_SH_RTS7751R2D)
	hw_regs_t hw;
	int index;

#ifndef CONFIG_BLK_DEV_IDEPCI
	for(index = 0; index < MAX_HWIFS; index++) {
#else
	for(index = 0; index < 1; index++) {
#endif
		ide_init_hwif_ports(&hw, ide_default_io_base(index), 0, NULL);
		hw.irq = ide_default_irq(ide_default_io_base(index));
		ide_register_hw(&hw, NULL);
	}
#endif /* CONFIG_BLK_DEV_IDEPCI */
}

typedef union {
	unsigned all			: 8;	/* all of the bits together */
	struct {
		unsigned head		: 4;	/* always zeros here */
		unsigned unit		: 1;	/* drive select number, 0 or 1 */
		unsigned bit5		: 1;	/* always 1 */
		unsigned lba		: 1;	/* using LBA instead of CHS */
		unsigned bit7		: 1;	/* always 1 */
	} b;
} select_t;

typedef union {
	unsigned all			: 8;	/* all of the bits together */
	struct {
		unsigned bit0		: 1;
		unsigned nIEN		: 1;	/* device INTRQ to host */
		unsigned SRST		: 1;	/* host soft reset bit */
		unsigned bit3		: 1;	/* ATA-2 thingy */
		unsigned reserved456	: 3;
		unsigned HOB		: 1;	/* 48-bit address ordering */
	} b;
} control_t;

#define ide_request_irq(irq,hand,flg,dev,id)	request_irq((irq),(hand),(flg),(dev),(id))
#define ide_free_irq(irq,dev_id)		free_irq((irq), (dev_id))
#define ide_check_region(from,extent)		check_region((from), (extent))
#define ide_request_region(from,extent,name)	request_region((from), (extent), (name))
#define ide_release_region(from,extent)		release_region((from), (extent))

/*
 * The following are not needed for the non-m68k ports
 */
#define ide_ack_intr(hwif)		(1)
#if defined(__LITTLE_ENDIAN__)
#define ide_fix_driveid(id)		do {} while (0)
#else
#include <linux/hdreg.h>
static __inline__ void ide_fix_driveid(struct hd_driveid *id)
{
	unsigned short* sc = (unsigned short*)id;
        int i;

	for (i = 0; i < sizeof(struct hd_driveid)/2; i++) {
	        sc[i] = __le16_to_cpu(sc[i]);
	}
}
#endif
#define ide_release_lock(lock)		do {} while (0)
#define ide_get_lock(lock, hdlr, data)	do {} while (0)

#endif /* __KERNEL__ */

#endif /* __ASM_SH_IDE_H */
