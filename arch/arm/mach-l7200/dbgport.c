/* ====================================================
 *   debug-port utility for Iris
 * ==================================================== */

static unsigned char debug_port_data = 0;

#include <asm/system.h>
#include <asm/io.h>
#include <asm/irq.h>
#include <asm/uaccess.h>
#include <asm/bitops.h>
#include <asm/arch/hardware.h>
#include <asm/arch/gpio.h>

#define DEBUG_IO    IO_IRIS1_DEBUG_LED

int get_debug_port_data(void)
{
  //printk("debug port : %x\n",debug_port_data);
  return(debug_port_data);
}

void clear_debug_port_data(void)
{
  debug_port_data = 0;
  /* DEBUG_IO = debug_port_data; */
}

void add_debug_port_data(unsigned char pin)
{
  debug_port_data |= pin;
  /* DEBUG_IO = debug_port_data; */
}

void del_debug_port_data(unsigned char pin)
{
  debug_port_data &= ~pin;
  /* DEBUG_IO = debug_port_data; */
}

void set_debug_port_data(unsigned char pin)
{
  debug_port_data = pin;
  /* DEBUG_IO = debug_port_data; */
}
