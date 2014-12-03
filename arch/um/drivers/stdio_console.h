/* 
 * Copyright (C) 2000 Jeff Dike (jdike@karaya.com)
 * Licensed under the GPL
 */

#ifndef __STDIO_CONSOLE_H
#define __STDIO_CONSOLE_H

extern int stdio_console_thread(unsigned long sp);
extern int stdio_start_thread(int pid);
extern void open_vt(int n, int *pid_out, int *in_fd_out, int *out_fd_out);
extern void update_console_size(int fd, unsigned short *rows_out, 
				unsigned short *cols_out);
extern void save_console_flags(void);
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
