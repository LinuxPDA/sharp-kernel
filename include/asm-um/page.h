#ifndef __UM_PAGE_H
#define __UM_PAGE_H

struct page;

#include "asm/arch/page.h"

#undef BUG
#undef PAGE_BUG
#undef __pa
#undef __va
#undef virt_to_page
#undef VALID_PAGE

#ifndef __ASSEMBLY__

extern void stop(void);

#define BUG() do { \
	printk("kernel BUG at %s:%d!\n", __FILE__, __LINE__); \
	stop(); \
} while (0)

#define PAGE_BUG(page) do { \
	BUG(); \
} while (0)

#endif /* __ASSEMBLY__ */

extern unsigned long physmem;

#define __va_space (8*1024*1024)

#define __pa(x)	((unsigned long) (x) - (physmem))
#define __va(x)	((void *) ((unsigned long) (x) + (physmem)))

#define virt_to_page(kaddr)	(mem_map + (__pa(kaddr) >> PAGE_SHIFT))
#define VALID_PAGE(page)	((page - mem_map) < max_mapnr)

#endif
