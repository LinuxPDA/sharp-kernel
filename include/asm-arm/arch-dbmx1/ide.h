/*
 * Copyright (c) 2003 Lineo uSolutions, Inc.
 *
 * include/asm-arm/arch-dbmx1/ide.h
 *
 */
#include <asm/irq.h>

static __inline__ void
ide_init_hwif_ports(hw_regs_t *hw, int data_port, int ctrl_port, int *irq)
{
	ide_ioreg_t reg;
	int i;
	int regincr = 1;
	
	memset(hw, 0, sizeof(*hw));

	reg = (ide_ioreg_t)data_port;

	for (i = IDE_DATA_OFFSET; i <= IDE_STATUS_OFFSET; i++) {
		hw->io_ports[i] = reg;
		reg += regincr;
	}
	
	hw->io_ports[IDE_CONTROL_OFFSET] = (ide_ioreg_t) ctrl_port;
	
	if (irq)
		*irq = 0;
}

static __inline__ void
ide_init_default_hwifs(void)
{
}
