/* 
 * Copyright (C) 2000 Jeff Dike (jdike@karaya.com)
 * Licensed under the GPL
 */

#ifndef __USER_H__
#define __USER_H__

extern void panic(char *fmt, ...);
extern int printk(char *fmt, ...);
extern void schedule(void);
extern void *kmalloc(int size, int flags);
extern void kfree(void *ptr);
extern int in_aton(char *str);

#endif

/*
 * Overrides for Emacs so that we follow Linus's tabbing style.
 * Emacs will notice this stuff at the end of the file and automatically
 * adjust the settings for this buffer only.  This must remain at the end
 * of the file.
 * ---------------------------------------------------------------------------
 * Local variables:
 * c-file-style: "linux"
 * End:
 */
