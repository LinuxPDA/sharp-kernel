/*
 * SA11x0 GPIO management
 * (C) 2000 Nicolas Pitre
 * This code is GPL.
 * 
 * This should eventually serve as a GPIO ressource manager.
 * For now we only have useful debugging stuff.
 */

#include <linux/config.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/proc_fs.h>
#include <linux/string.h>

#include <asm/hardware.h>



static int proc_gpio_read(char *page, char **start, off_t off,
			  int count, int *eof, void *data)
{
	char *p = page;
	int len, i;
	long gpdr = GPDR, gplr = GPLR;

	for (i = 0; i <= 27; i++)
		p += sprintf(p, "%d\t%s\t%s\n", i, 
				(gpdr & GPIO_GPIO(i)) ? "out" : "in",
				(gplr & GPIO_GPIO(i)) ? "set" : "clear");


	len = (p - page) - off;
	if (len < 0)
		len = 0;

	*eof = (len <= count) ? 1 : 0;
	*start = page + off;

	return len;
}


static struct proc_dir_entry *proc_gpio;

static int __init gpio_init(void)
{
	proc_gpio = create_proc_entry("gpio", 0, 0);
	if (proc_gpio)
		proc_gpio->read_proc = proc_gpio_read;
	return 0;
}

static int __exit gpio_exit(void)
{
	if (proc_gpio)
		remove_proc_entry( "gpio", 0);
	return 0;

}


module_init(gpio_init);
module_exit(gpio_exit);

