#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h>
#include <sys/stat.h>
#include <stdlib.h>
#include <dirent.h>
#include <signal.h>
#include "user.h"
#include "umid.h"

#define UMID_LEN 64

static char umid[UMID_LEN] = { 0 };

static int umid_inited = 0;

int umid_file_name(char *name, char *buf, int len)
{
	int n;

	if(!umid_inited && set_umid(NULL)) return(-1);

	n = sizeof("/tmp/uml") + strlen(umid) + sizeof("/") + strlen(name) + 1;
	if(n > len){
		printk("umid_file_name : buffer too short\n");
		return(-1);
	}

	sprintf(buf, "/tmp/uml/%s/%s", umid, name);
	return(0);
}

extern int tracing_pid;

static void create_pid_file(void)
{
	char file[sizeof("/tmp/uml/") + UMID_LEN + sizeof("/pid\0")];
	char pid[sizeof("nnnnn\0")];
	int fd;

	if(umid_file_name("pid", file, sizeof(file))) return;

	if((fd = open(file, O_RDWR | O_CREAT | O_EXCL, 0644)) < 0){
		printk("Open of machine pid file \"%s\" failed - "
		       "errno = %d\n", file, errno);
		return;
	}

	sprintf(pid, "%d\n", (tracing_pid == -1) ?  getpid() : tracing_pid);
	if(write(fd, pid, strlen(pid)) != strlen(pid))
		printk("Write of pid file failed - errno = %d\n", errno);
	close(fd);
}

static int actually_do_remove(char *dir)
{
	DIR *directory;
	struct dirent *ent;
	int len;
	char file[256];

	if((directory = opendir(dir)) == NULL){
		printk("actually_do_remove : couldn't open directory '%s', "
		       "errno = %d", dir, errno);
		return(1);
	}
	while((ent = readdir(directory)) != NULL){
		if(!strcmp(ent->d_name, ".") || !strcmp(ent->d_name, ".."))
			continue;
		len = strlen(dir) + sizeof("/") + strlen(ent->d_name) + 1;
		if(len > sizeof(file)){
			printk("Not deleting '%s' from '%s' - name too long\n",
			       ent->d_name, dir);
			continue;
		}
		sprintf(file, "%s/%s", dir, ent->d_name);
		if(unlink(file) < 0){
			printk("actually_do_remove : couldn't remove '%s' "
			       "from '%s', errno = %d\n", ent->d_name, dir, 
			       errno);
			return(1);
		}
	}
	if(rmdir(dir) < 0){
		printk("actually_do_remove : couldn't rmdir '%s', "
		       "errno = %d\n", dir, errno);
		return(1);
	}
	return(0);
}

void remove_umid_dir(void)
{
	char dir[sizeof("/tmp/uml/") + UMID_LEN + 1];
	if(!umid_inited) return;

	sprintf(dir, "/tmp/uml/%s", umid);
	actually_do_remove(dir);
}

char *get_umid(void)
{
	return(umid);
}

static int not_dead_yet(char *dir)
{
	char file[sizeof("/tmp/uml/") + UMID_LEN + sizeof("/pid\0")];
	char pid[sizeof("nnnnn\0")], *end;
	int dead, fd, p;

	sprintf(file, "%s/pid", dir);
	dead = 0;
	if((fd = open(file, O_RDONLY)) < 0){
		if(errno != ENOENT){
			printk("not_dead_yet : couldn't open pid file '%s', "
			       "errno = %d\n", file, errno);
			return(1);
		}
		dead = 1;
	}
	if(fd > 0){
		if(read(fd, pid, sizeof(pid)) < 0){
			printk("not_dead_yet : couldn't read pid file '%s', "
			       "errno = %d\n", file, errno);
			return(1);
		}
		p = strtoul(pid, &end, 0);
		if(end == pid){
			printk("not_dead_yet : couldn't parse pid file '%s', "
			       "errno = %d\n", file, errno);
			dead = 1;
		}
		if((kill(p, 0) < 0) && (errno == ESRCH)) dead = 1;
	}
	if(!dead) return(1);
	return(actually_do_remove(dir));
	return(0);
}

int set_umid(char *name)
{
	int fd, err;
	char tmp[sizeof("/tmp/uml/") + UMID_LEN + 1];

	if(umid_inited){
		printk("Unique machine name can't be set twice\n");
		return(-1);
	}

	if((mkdir("/tmp/uml", 0777) < 0) && (errno != EEXIST)){
		printk("Failed to create /tmp/uml - errno = %s\n", errno);
		return(-1);
	}

	strcpy(tmp, "/tmp/uml/");

	if(name == NULL){
		strcat(tmp, "XXXXXX");
		fd = mkstemp(tmp);
		if(fd < 0){
			printk("set_umid - mkstemp failed, errno = %d\n",
			       errno);
			return(-1);
		}

		close(fd);
		/* There's a nice tiny little race between this unlink and
		 * the mkdir below.  It'd be nice if there were a mkstemp
		 * for directories.
		 */
		unlink(tmp);
		name = &tmp[strlen("/tmp/uml/")];
	}

	if(strlen(name) > UMID_LEN - 1)
		printk("Unique machine name is being truncated to %s "
		       "characters\n", UMID_LEN);
	strncpy(umid, name, UMID_LEN - 1);
	umid[UMID_LEN - 1] = '\0';
	
	sprintf(tmp, "/tmp/uml/%s", umid);

	if((err = mkdir(tmp, 0777)) < 0){
		if(errno == EEXIST){
			if(not_dead_yet(tmp)){
				printk("umid '%s' is in use\n", umid);
				return(-1);
			}
			err = mkdir(tmp, 0777);
		}
	}
	if(err < 0){
		printk("Failed to create %s - errno = %d\n", umid, errno);
		return(-1);
	}

	umid_inited = 1;
	create_pid_file();
	return(0);
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
