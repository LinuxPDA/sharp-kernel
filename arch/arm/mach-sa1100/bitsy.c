/*
 * linux/arch/arm/mach-sa1100/bitsy.c
 */

#include <linux/config.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/tty.h>

#include <asm/hardware.h>
#include <asm/setup.h>

#include <asm/mach/arch.h>
#include <asm/mach/map.h>
#include <asm/mach/serial_sa1100.h>
#include <linux/serial_core.h>

#include "generic.h"


/*
 * Bitsy has extended, write-only memory-mapped GPIO's
 */

static int bitsy_egpio = EGPIO_BITSY_RS232_ON;

void clr_bitsy_egpio(unsigned long x)
{
	bitsy_egpio &= ~x;
	BITSY_EGPIO = bitsy_egpio;
}

void set_bitsy_egpio(unsigned long x)
{
	bitsy_egpio |= x;
	BITSY_EGPIO = bitsy_egpio;
}

EXPORT_SYMBOL(clr_bitsy_egpio);
EXPORT_SYMBOL(set_bitsy_egpio);


/*
 * low-level UART features
 */
static void bitsy_uart_pm(struct uart_port *port, u_int state, u_int oldstate)
{
	if (port->base == (u_int)&Ser2UTCR0) {
		if (state == 0) {
			Ser2UTCR4 = UTCR4_HSE;
			Ser2HSCR0 = 0;
			Ser2HSSR0 = HSSR0_EIF | HSSR0_TUR |
				    HSSR0_RAB | HSSR0_FRE;
			set_bitsy_egpio(EGPIO_BITSY_IR_ON);
		} else {
			clr_bitsy_egpio(EGPIO_BITSY_IR_ON);
		}
	}
}

static struct sa1100_port_fns bitsy_port_fns __initdata = {
	pm:	bitsy_uart_pm,
};

static struct map_desc bitsy_io_desc[] __initdata = {
 /* virtual     physical    length      domain     r  w  c  b */
  { 0xe8000000, 0x00000000, 0x02000000, DOMAIN_IO, 1, 1, 0, 0 }, /* Flash bank 0 */
  { 0xf0000000, 0x49000000, 0x01000000, DOMAIN_IO, 1, 1, 0, 0 }, /* EGPIO 0 */
  { 0xf1000000, 0x10000000, 0x02000000, DOMAIN_IO, 1, 1, 0, 0 }, /* static memory bank 2 */
  { 0xf3000000, 0x40000000, 0x02000000, DOMAIN_IO, 1, 1, 0, 0 }, /* static memory bank 4 */
  LAST_DESC
};

static void __init bitsy_map_io(void)
{
	sa1100_map_io();
	iotable_init(bitsy_io_desc);

	sa1100_register_uart_fns(&bitsy_port_fns);
	sa1100_register_uart(0, 3);
	sa1100_register_uart(1, 1); /* isn't this one driven elsewhere? */
	sa1100_register_uart(2, 2);
}

MACHINE_START(BITSY, "Compaq iPAQ")
	BOOT_MEM(0xc0000000, 0x80000000, 0xf8000000)
	BOOT_PARAMS(0xc0000100)
	MAPIO(bitsy_map_io)
	INITIRQ(sa1100_init_irq)
MACHINE_END
