/* 
 * Copyright (C) 2000 Jeff Dike (jdike@karaya.com)
 * Licensed under the GPL
 */

#ifndef __SYS_SIGCONTEXT_I386_H
#define __SYS_SIGCONTEXT_I386_H

#define UM_ALLOCATE_SC(name) struct sigcontext name
#define SC_FAULT_ADDR(sc) ((sc)->cr2)
#define SC_FAULT_WRITE(sc) (((sc)->err) & 2)
#define SC_IP(sc) ((sc)->eip)
#define SC_SP(sc) ((sc)->esp_at_signal)
#define SEGV_IS_FIXABLE(sc) (((sc)->trapno == 14) || ((sc)->trapno == 13))

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
