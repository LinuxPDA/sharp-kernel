#include "linux/kernel.h"
#include "linux/module.h"
#include "asm/page.h"
#include "asm/processor.h"
#include "sysrq.h"

extern int _stext, _etext;

 /*
  * If the address is either in the .text section of the
  * kernel, or in the vmalloc'ed module regions, it *may* 
  * be the address of a calling routine
  */
 
#ifdef CONFIG_MODULES

extern struct module *module_list;
extern struct module kernel_module;

static inline int kernel_text_address(unsigned long addr)
{
	int retval = 0;
	struct module *mod;

	if (addr >= (unsigned long) &_stext &&
	    addr <= (unsigned long) &_etext)
		return 1;

	for (mod = module_list; mod != &kernel_module; mod = mod->next) {
		/* mod_bound tests for addr being inside the vmalloc'ed
		 * module area. Of course it'd be better to test only
		 * for the .text subset... */
		if (mod_bound(addr, 0, mod)) {
			retval = 1;
			break;
		}
	}

	return retval;
}

#else

static inline int kernel_text_address(unsigned long addr)
{
	return (addr >= (unsigned long) &_stext &&
		addr <= (unsigned long) &_etext);
}

#endif

void show_trace(unsigned long * stack)
{
        int i;
        unsigned long addr;

        if (!stack)
                stack = (unsigned long*) &stack;

        printk("Call Trace: ");
        i = 1;
        while (((long) stack & (THREAD_SIZE-1)) != 0) {
                addr = *stack++;
		if (kernel_text_address(addr)) {
			if (i && ((i % 6) == 0))
				printk("\n   ");
			printk("[<%08lx>] ", addr);
			i++;
                }
        }
        printk("\n");
}
