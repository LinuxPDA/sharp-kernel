/* 
 * Copyright (C) 2000 Jeff Dike (jdike@karaya.com)
 * Licensed under the GPL
 */

#include <unistd.h>
#include <signal.h>
#include <sched.h>
#include <errno.h>
#include <termios.h>
#include <sys/select.h>
#include <sys/time.h>
#include <sys/ioctl.h>
#include "stdio_console.h"
#include "user_util.h"
#include "kern_util.h"
#include "user.h"

struct termios cooked_tt;

void save_console_flags(void)
{
	if(isatty(0)) tcgetattr(0, &cooked_tt);
	else if(isatty(1)) tcgetattr(1, &cooked_tt);
	else tcgetattr(2, &cooked_tt);
}

void cooked(int fd)
{
	tcsetattr(fd, TCSADRAIN, &cooked_tt);
}

void update_console_size(int fd, unsigned short *rows_out, 
			 unsigned short *cols_out)
{
	struct winsize size;

	if(ioctl(fd, TIOCGWINSZ, &size) == 0){
		*rows_out = size.ws_row;
		*cols_out = size.ws_col;
	}
}

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
