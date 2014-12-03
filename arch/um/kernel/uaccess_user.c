/* 
 * Copyright (C) 2001 Chris Emerson (cemerson@chiark.greenend.org.uk)
 * Licensed under the GPL
 */

#include <setjmp.h>
#include <string.h>
#include "user_util.h"

static unsigned long __do_user_copy(void *to, const void *from, int n,
				    void **fault_addr, void **fault_catcher,
				    void (*op)(void *to, const void *from,
					       int n), int *faulted_out)
{
	unsigned long *faddrp = (unsigned long *) fault_addr, ret;

	jmp_buf jbuf;
	*fault_catcher = &jbuf;
	if(setjmp(jbuf) == 0){
		(*op)(to, from, n);
		ret = 0;
		*faulted_out = 0;
	} 
	else {
		ret = *faddrp;
		*faulted_out = 1;
	}
	*fault_addr = NULL;
	*fault_catcher = NULL;
	return ret;
}

static void __do_copy(void *to, const void *from, int n)
{
	memcpy(to, from, n);
}	

int __do_copy_from_user(void *to, const void *from, int n,
			void **fault_addr, void **fault_catcher)
{
	unsigned long fault;
	int faulted;

	fault = __do_user_copy(to, from, n, fault_addr, fault_catcher,
			       __do_copy, &faulted);
	if(!faulted) return(0);
	else return(n - (fault - (unsigned long) from));
}


int __do_copy_to_user(void *to, const void *from, int n,
		      void **fault_addr, void **fault_catcher)
{
	unsigned long fault;
	int faulted;

	fault = __do_user_copy(to, from, n, fault_addr, fault_catcher,
			       __do_copy, &faulted);
	if(!faulted) return(0);
	else return(n - (fault - (unsigned long) to));
}

static void __do_strncpy(void *dst, const void *src, int count)
{
	strncpy(dst, src, count);
}	

int __do_strncpy_from_user(char *dst, const char *src, unsigned long count,
			   void **fault_addr, void **fault_catcher)
{
	unsigned long fault;
	int faulted;

	fault = __do_user_copy(dst, src, count, fault_addr, fault_catcher,
			       __do_strncpy, &faulted);
	if(!faulted) return(strlen(dst));
	else return(-1);
}

static void __do_clear(void *to, const void *from, int n)
{
	memset(to, 0, n);
}	

int __do_clear_user(void *mem, unsigned long len,
		    void **fault_addr, void **fault_catcher)
{
	unsigned long fault;
	int faulted;

	fault = __do_user_copy(mem, NULL, len, fault_addr, fault_catcher,
			       __do_clear, &faulted);
	if(!faulted) return(0);
	else return(len - (fault - (unsigned long) mem));
}

int __do_strnlen_user(const char *str, unsigned long n,
		      void **fault_addr, void **fault_catcher)
{
	int ret;
	unsigned long *faddrp = (unsigned long *)fault_addr;
	jmp_buf jbuf;

	*fault_catcher = &jbuf;
	if(setjmp(jbuf) == 0){
		ret = strlen(str) + 1;
	} 
	else {
		ret = *faddrp - (unsigned long) str;
	}
	*fault_addr = NULL;
	*fault_catcher = NULL;
	return ret;
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
