/* 
 * Copyright (C) 2000, 2001 Jeff Dike (jdike@karaya.com)
 * Licensed under the GPL
 */

#include <unistd.h>
#include <stdio.h> 
#include <stdlib.h>
#include <termios.h>
#include <string.h>
#include <signal.h>
#include <sys/resource.h>
#include <sys/mman.h>
#include <sys/user.h>
#include "user_util.h"
#include "kern_util.h"
#include "mem_user.h"
#include "user.h"

unsigned long stacksizelim;

char *linux_prog;

#define PGD_BOUND (4 * 1024 * 1024)
#define STACKSIZE (8 * 1024 * 1024)
#define THREAD_NAME_LEN (256)

char padding[THREAD_NAME_LEN] = { [ 0 ...  THREAD_NAME_LEN - 2] = ' ', '\0' };

static void set_stklim(void)
{
	struct rlimit lim;

	if(getrlimit(RLIMIT_STACK, &lim) < 0){
		perror("getrlimit");
		exit(1);
	}
	if((lim.rlim_cur == RLIM_INFINITY) || (lim.rlim_cur > STACKSIZE)){
		lim.rlim_cur = STACKSIZE;
		if(setrlimit(RLIMIT_STACK, &lim) < 0){
			perror("setrlimit");
			exit(1);
		}
	}
	stacksizelim = (lim.rlim_cur + PGD_BOUND - 1) & ~(PGD_BOUND - 1);
}

unsigned long host_task_size;
unsigned long task_size;

static void set_task_sizes(int arg)
{
	/* Round up to the nearest 4M */
	host_task_size = ROUND_4M((unsigned long) &arg);
	task_size = host_task_size - 0x20000000;
}

int main(int argc, char **argv, char **envp)
{
	struct termios tt;
	int ret, i;
	char **new_argv;
	
	/* Allocate memory for thread command lines */
	if(argc < 2 || strlen(argv[1]) < THREAD_NAME_LEN - 1){
		new_argv = malloc((argc + 2) * sizeof(char*));
		if(!new_argv) {
			perror("Allocating extended argv");
			exit(1);
		}	
		
		new_argv[0] = argv[0];
		new_argv[1] = padding;
		
		for(i = 2; i <= argc; i++)
			new_argv[i] = argv[i - 1];
		new_argv[argc + 1] = NULL;
		
#ifdef PROFILING
		disable_profile_timer();
#endif
		execvp(new_argv[0], new_argv);
		perror("execing with extended args");
		exit(1);
	}	

	linux_prog = argv[0];

	set_stklim();
	set_task_sizes(0);

	if(isatty(0)) tcgetattr(0, &tt);
	else if(isatty(1)) tcgetattr(1, &tt);
	else tcgetattr(2, &tt);

	if((new_argv = malloc((argc + 1) * sizeof(char *))) == NULL){
		perror("Mallocing argv");
		exit(1);
	}
	for(i=0;i<argc;i++){
		if((new_argv[i] = strdup(argv[i])) == NULL){
			perror("Mallocing an arg");
			exit(1);
		}
	}
	new_argv[argc] = NULL;

	ret = linux_main(argc, argv);
	
	/* Reboot */
	if(ret){
		printf("\n");
		tcsetattr(0, TCSADRAIN, &tt);
		execve(new_argv[0], new_argv, envp);
		perror("Failed to exec kernel");
		ret = 1;
	}
	if(isatty(0)) tcsetattr(0, TCSADRAIN, &tt);
	else if(isatty(1)) tcsetattr(1, TCSADRAIN, &tt);
	else tcsetattr(2, TCSADRAIN, &tt);
	printf("\n");
	return(ret);
}

int allocating_monbuf = 0;

#ifdef PROFILING
extern void __real___monstartup (unsigned long, unsigned long);

void __wrap___monstartup (unsigned long lowpc, unsigned long highpc)
{
	allocating_monbuf = 1;
	__real___monstartup(lowpc, highpc);
	allocating_monbuf = 0;
	get_profile_timer();
}
#endif

extern void *__real_malloc(int);
extern unsigned long host_task_size;

static void *gmon_buf = NULL;

void *__wrap_malloc(int size)
{
	if(allocating_monbuf){
		unsigned long start, end;
		int fd = create_mem_file(size);

		/* Calculate this here because linux_main hasn't run yet
		 * and host_task_size figures in STACK_TOP, which figures
		 * in kmem_end.
		 */
		set_task_sizes(0);

		/* Same with stacksizelim */
		set_stklim();

		end = get_kmem_end();
		start = (end - size) & PAGE_MASK;
		gmon_buf = mmap((void *) start, size, PROT_READ | PROT_WRITE,
			    MAP_SHARED | MAP_FIXED, fd, 0);
		if(gmon_buf != (void *) start){
			perror("Creating gprof buffer");
			exit(1);
		}
		set_kmem_end(start);
		return(gmon_buf);
	}
	if(kmalloc_ok) return(um_kmalloc(size));
	else return(__real_malloc(size));
}

void *__wrap_calloc(int n, int size)
{
	void *ptr = __wrap_malloc(n * size);

	if(ptr == NULL) return(NULL);
	memset(ptr, 0, n * size);
	return(ptr);
}

extern void __real_free(void *);

void __wrap_free(void *ptr)
{
	/* Could maybe unmap the gmon buffer, but we're just about to
	 * exit anyway
	 */
	if(ptr == gmon_buf) return;
	if(kmalloc_ok) kfree(ptr);
	else __real_free(ptr);
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
