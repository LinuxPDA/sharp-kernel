/* 
 * Copyright (C) 2000, 2001 Jeff Dike (jdike@karaya.com)
 * Licensed under the GPL
 */

#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <limits.h>
#include <sys/mman.h> 
#include <sys/stat.h>
#include <sys/ptrace.h>
#include <sys/mount.h>
#include <sys/utsname.h>
#include <ctype.h>
#include <signal.h>
#include <wait.h>
#include <errno.h>
#include <stdarg.h>
#include <sched.h>
#include <termios.h>
#include <string.h>
#include "user_util.h"
#include "kern_util.h"
#include "user.h"
#include "mem_user.h"

#define COMMAND_LINE_SIZE _POSIX_ARG_MAX

char saved_command_line[COMMAND_LINE_SIZE] = { 0 };
char command_line[COMMAND_LINE_SIZE] = { 0 };

void add_arg(char *cmd_line, char *arg)
{
	if (strlen(cmd_line) + strlen(arg) + 1 > COMMAND_LINE_SIZE) {
		printf("add_arg: Too much command line!\n");
		exit(1);
	}
	if(strlen(cmd_line) > 0) strcat(cmd_line, " ");
	strcat(cmd_line, arg);
}

void *open_maps(void)
{
	void *maps;

	maps = fopen("/proc/self/maps", "r");
	if(!maps){
		perror("open_maps");
		exit(1);
	}
	return(maps);
}

int read_map(void *maps, unsigned long *start_out, unsigned long *end_out,
	     char *r_out, char *w_out, char *x_out, char *p_out)
{
	unsigned int inode;
	unsigned long long offset;
	unsigned long start, end, major, minor;
	int ret;
	char r, w, x, p;

	ret = fscanf(maps, "%lx-%lx %c%c%c%c %Lx %lx:%lx %d%*[^\n]", &start,
		     &end, &r, &w, &x, &p, &offset, &major, &minor,
		     &inode);
	if (ret == EOF)	return 0;
	else if (ret != 10){
		perror("Scanning a map line");
		exit(1);
	}
	if(start_out != NULL) *start_out = start;
	if(end_out != NULL) *end_out = end;
	if(r_out != NULL) *r_out = r;
	if(w_out != NULL) *w_out = w;
	if(x_out != NULL) *x_out = x;
	if(p_out != NULL) *p_out = p;
	return(1);
}

void close_maps(void *maps)
{
	fclose(maps);
}

void remap_data(void *segment_start, void *segment_end)
{
	void *addr;
	unsigned long start, end, size;
	void *maps;
	int data, prot;
	char r, w, x;

	maps= open_maps();
	while(read_map(maps, &start, &end, &r, &w, &x, NULL) != 0){
		if(((unsigned long) segment_start >= start) && 
		   ((unsigned long) segment_end <= end)){
			prot = PROT_READ | PROT_WRITE | PROT_EXEC;
			size = (unsigned long) segment_end - 
				(unsigned long) segment_start;
			data = create_mem_file(size);
			if((addr = mmap(NULL, size, PROT_WRITE | PROT_READ, 
					MAP_SHARED, data, 0)) < 0){
				perror("mapping new data segment");
				exit(1);
			}
			memcpy(addr, segment_start, size);
			if(switcheroo(data, prot, addr, segment_start, 
				      size) < 0){
				printf("switcheroo failed\n");
				exit(1);
			}
			close_maps(maps);
			return;
		}
	}
	fprintf(stderr, "remap_data couldn't find data segment 0x%lx - 0x%lx "
		"in /proc/self/maps\n", (unsigned long) segment_start, 
		(unsigned long) segment_end);
	exit(1);
}

__u64 file_size(char *file)
{
	struct stat64 buf;

	if(stat64(file, &buf) == -1){
		printk("Couldn't stat \"%s\" : errno = %d\n", file, errno);
		return(-errno);
	}
	if(S_ISBLK(buf.st_mode)){
		long long size;
		int fd;

		if((fd = open64(file, O_RDONLY)) < 0){
			printk("Couldn't open \"%s\", errno = %d\n", file,
			       errno);
			return(-errno);
		}
		if(ioctl(fd, BLKGETSIZE, &size) < 0){
			printk("Couldn't get the block size of \"%s\", "
			       "errno = %d\n", file, errno);
			close(fd);
			return(-errno);
		}
		size *= 512;
		close(fd);
		return(size);
	}
	return(buf.st_size);
}

int load_initrd(char *filename, void *buf, int size)
{
	int fd, n;

	if((fd = open(filename, O_RDONLY)) == -1){
		printk("Opening '%s' failed - errno = %d\n", filename, errno);
		return(-1);
	}
	if((n = read(fd, buf, size)) != size){
		printk("Read of %d bytes from '%s' returned %d, errno = %d\n",
		       size, filename, n, errno);
		return(-1);
	}
	return(0);
}

void stop(void)
{
	while(1) sleep(1000000);
}

void stack_protections(unsigned long address)
{
	int prot = PROT_READ | PROT_WRITE | PROT_EXEC;

        if(mprotect((void *) address, page_size(), prot) < 0)
		panic("protecting stack failed, errno = %d", errno);
}

void task_protections(unsigned long address)
{
	unsigned long guard = address + page_size();
	unsigned long stack = guard + page_size();
	int prot = 0;

	if(mprotect((void *) stack, page_size(), prot) < 0)
		panic("protecting guard page failed, errno = %d", errno);
	prot = PROT_READ | PROT_WRITE | PROT_EXEC;
	if(mprotect((void *) stack, 2 * page_size(), prot) < 0)
		panic("protecting stack failed, errno = %d", errno);
}

void protect(unsigned long addr, unsigned long len, int r, int w, int x)
{
	int prot;

	prot = (r ? PROT_READ : 0) | (w ? PROT_WRITE : 0) | 
		(x ? PROT_EXEC : 0);
	if(mprotect((void *) addr, len, prot) == -1)
		panic("protect failed, errno = %d", errno);
}

int wait_for_stop(int pid, int sig, int cont_type)
{
	int status, ret;

	while(1){
		if(((ret = waitpid(pid, &status, WUNTRACED)) < 0) ||
		   !WIFSTOPPED(status) || (WSTOPSIG(status) != sig)){
			if(ret < 0){
				if(errno == EINTR) continue;
				printk("wait failed, errno = %d\n", errno);
			}
			else if(WIFEXITED(status)) 
				printk("process exited with status %d\n", 
				       WEXITSTATUS(status));
			else if(WIFSIGNALED(status))
				printk("process exited with signal %d\n", 
				       WTERMSIG(status));
			else if((WSTOPSIG(status) == SIGVTALRM) ||
				(WSTOPSIG(status) == SIGALRM) ||
				(WSTOPSIG(status) == SIGIO) ||
				(WSTOPSIG(status) == SIGPROF) ||
				(WSTOPSIG(status) == SIGCHLD)){
				ptrace(cont_type, pid, 0, WSTOPSIG(status));
				continue;
			}
			else printk("process stopped with signal %d\n", 
				    WSTOPSIG(status));
			panic("wait_for_stop failed to wait for %d to stop "
			      "with %d\n", pid, sig);
		}
		return(status);
	}
}

int clone_and_wait(int (*fn)(void *), void *arg, void *sp, int flags)
{
	int pid;

	pid = clone(fn, sp, flags, arg);
 	if(pid < 0) return(-1);
	wait_for_stop(pid, SIGSTOP, PTRACE_CONT);
	return(pid);
}

struct grantpt_info {
	int fd;
	int res;
	int err;
};

static void grantpt_cb(void *arg)
{
	struct grantpt_info *info = arg;

	info->res = grantpt(info->fd);
	info->err = errno;
}

int get_pty(void)
{
	struct grantpt_info info;
	int fd;

	if((fd = open("/dev/ptmx", O_RDWR)) < 0){
		printk("get_pty : Couldn't open /dev/ptmx - errno = %d\n",
		       errno);
		return(-1);
	}
	info.fd = fd;
	tracing_cb(grantpt_cb, &info);
	if(info.res < 0){
		printk("get_pty : Couldn't grant pty - errno = %d\n", 
		       info.err);
		return(-1);
	}
	if(unlockpt(fd) < 0){
		printk("get_pty : Couldn't unlock pty - errno = %d\n", errno);
		return(-1);
	}
	return(fd);
}

int raw(int fd, int complain)
{
	struct termios tt;
	int err;

	tcgetattr(fd, &tt);
	cfmakeraw(&tt);
	err = tcsetattr(fd, TCSADRAIN, &tt);
	if((err < 0) && complain){
		printk("tcsetattr failed, errno = %d\n", errno);
		return(-errno);
	}
	return(0);
}

void setup_machinename(char *machine_out)
{
	struct utsname host;

	uname(&host);
	strcpy(machine_out, host.machine);
}

char host_info[(_UTSNAME_LENGTH + 1) * 4 + _UTSNAME_NODENAME_LENGTH + 1];

void setup_hostinfo(void)
{
	struct utsname host;

	uname(&host);
	sprintf(host_info, "%s %s %s %s %s", host.sysname, host.nodename,
		host.release, host.version, host.machine);
}

void close_fd(int fd)
{
	close(fd);
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
