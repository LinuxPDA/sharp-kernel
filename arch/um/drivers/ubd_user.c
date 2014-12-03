/* 
 * Copyright (C) 2000 Jeff Dike (jdike@karaya.com)
 * Copyright (C) 2001 Ridgerun,Inc (glonnon@ridgerun.com)
 * Licensed under the GPL
 */

#include <stddef.h>
#include <unistd.h>
#include <errno.h>
#include <sched.h>
#include <signal.h>
#include <string.h>
#include <netinet/in.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/fcntl.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/param.h>
#include "user_util.h"
#include "kern_util.h"
#include "user.h"
#include "ubd_user.h"

#include <endian.h>
#include <byteswap.h>
#if __BYTE_ORDER == __BIG_ENDIAN
# define ntohll(x) (x)
# define htonll(x) (x)
#elif __BYTE_ORDER == __LITTLE_ENDIAN
# define ntohll(x)  bswap_64(x)
# define htonll(x)  bswap_64(x)
#else
#error "__BYTE_ORDER not defined"
#endif

extern void panic(char *fmt, ...);

#define PATH_LEN_V1 256

struct cow_header_v1 {
	int magic;
	int version;
	char backing_file[PATH_LEN_V1];
	time_t mtime;
	__u64 size;
	int sectorsize;
};

#define PATH_LEN_V2 MAXPATHLEN

struct cow_header_v2 {
	unsigned long magic;
	unsigned long version;
	char backing_file[PATH_LEN_V2];
	time_t mtime;
	__u64 size;
	int sectorsize;
};

union cow_header {
	struct cow_header_v1 v1;
	struct cow_header_v2 v2;
};

#define COW_MAGIC 0x4f4f4f4d  /* MOOO */
#define COW_VERSION 2

static void sizes(__u64 size, int sectorsize, int bitmap_offset, 
		  int *bitmap_len_out, int *data_offset_out)
{
	*bitmap_len_out = (size + sectorsize - 1) / (8 * sectorsize);

	*data_offset_out = bitmap_offset + *bitmap_len_out;
	*data_offset_out = (*data_offset_out + sectorsize - 1) / sectorsize;
	*data_offset_out *= sectorsize;
}

static int read_cow_header(int fd, int *magic_out, char **backing_file_out, 
			   time_t *mtime_out, __u64 *size_out, 
			   int *sectorsize_out, int *bitmap_offset_out)
{
	union cow_header *header;
	char *file;
	int err, n;
	unsigned long version, magic;

	header = um_kmalloc(sizeof(*header));
	if(header == NULL){
		printk("read_cow_header - Failed to allocate header\n");
		return(-ENOMEM);
	}
	err = -EINVAL;
	n = read(fd, header, sizeof(*header));
	if(n < offsetof(typeof(header->v1), backing_file)){
		printk("read_cow_header - short header\n");
		goto out;
	}

	err = 0;
	magic = header->v1.magic;
	if(magic == COW_MAGIC) {
		version = header->v1.version;
	}
	else if(magic == ntohl(COW_MAGIC)){
		version = ntohl(header->v1.version);
	}
	else goto out;

	*magic_out = COW_MAGIC;

	err = -EINVAL;
	if(version == 1){
		if(n < sizeof(header->v1)){
			printk("read_cow_header - failed to read V1 header\n");
			goto out;
		}
		*mtime_out = header->v1.mtime;
		*size_out = header->v1.size;
		*sectorsize_out = header->v1.sectorsize;
		*bitmap_offset_out = sizeof(header->v1);
		file = header->v1.backing_file;
	}
	else if(version == 2){
		if(n < sizeof(header->v2)){
			printk("read_cow_header - failed to read V2 header\n");
			goto out;
		}
		*mtime_out = ntohl(header->v2.mtime);
		*size_out = ntohll(header->v2.size);
		*sectorsize_out = ntohl(header->v2.sectorsize);
		*bitmap_offset_out = sizeof(header->v2);
		file = header->v2.backing_file;
	}
	else {
		printk("read_cow_header - invalid COW version\n");
		goto out;
	}
	err = -ENOMEM;
	*backing_file_out = uml_strdup(file);
	if(*backing_file_out == NULL){
		printk("read_cow_header - failed to allocate backing file\n");
		goto out;
	}
	err = 0;
 out:
	kfree(header);
	return(err);
}

int open_ubd_file(char *file, int *openflags, char **backing_file_out, 
		  int *bitmap_offset_out, int *bitmap_len_out, 
		  int *data_offset_out)
{
	struct stat64 buf;
	time_t mtime;
	__u64 size;
        int fd, err, sectorsize, magic, mode = 0644;

	if(backing_file_out != NULL) *backing_file_out = NULL;
        if((fd = open64(file, *openflags, mode)) < 0){
                if(((*openflags & O_ACCMODE) != O_RDWR) || 
                   ((errno != EROFS) && (errno != EACCES))) return(-errno);
		*openflags &= ~O_ACCMODE;
                *openflags |= O_RDONLY;
                if((fd = open64(file, *openflags, mode)) < 0) return(-errno);
        }
	if(backing_file_out == NULL) return(fd);

	err = read_cow_header(fd, &magic, backing_file_out, &mtime, &size, 
			      &sectorsize, bitmap_offset_out);
	if(err) goto error;
	
	if(magic != COW_MAGIC){
  		*backing_file_out = NULL;
  		return(fd);
  	}

  	err = stat64(*backing_file_out, &buf);
  	if(err) goto error;

  	err = -EINVAL;
  	if(buf.st_size != size){
		printk("Size mismatch (%ld vs %ld) of COW header vs backing "
		       "file\n", buf.st_size, size);
		goto error;
	}
	if(buf.st_mtime != mtime){
		printk("mtime mismatch (%ld vs %ld) of COW header vs backing "
		       "file\n", buf.st_mtime, mtime);
		goto error;
	}

	sizes(size, sectorsize, *bitmap_offset_out, bitmap_len_out, 
	      data_offset_out);

        return(fd);
 error:
	close(fd);
	return(err);
}

int read_cow_bitmap(int fd, void *buf, int offset, int len)
{
	int err;

	err = lseek64(fd, offset, SEEK_SET);
	if(err != offset) return(-errno);
	err = read(fd, buf, len);
	if(err < 0) return(-errno);
	return(0);
}

static int absolutize(char *to, int size, char *from)
{
	char save_cwd[256], *slash;
	int remaining;

	if(getcwd(save_cwd, sizeof(save_cwd)) == NULL) {
		printk("absolutize : unable to get cwd - errno = %d\n", errno);
		return(-1);
	}
	slash = strrchr(from, '/');
	if(slash != NULL){
		*slash = '\0';
		if(chdir(from)){
			*slash = '/';
			printk("absolutize : Can't cd to '%s' - errno = %d\n",
			       from, errno);
			return(-1);
		}
		*slash = '/';
		if(getcwd(to, size) == NULL){
			printk("absolutize : unable to get cwd of '%s' - "
			       "errno = %d\n", from, errno);
			return(-1);
		}
		remaining = size - strlen(to);
		if(strlen(slash) + 1 > remaining){
			printk("absolutize : unable to fit '%s' into %d "
			       "chars\n", from, size);
			return(-1);
		}
		strcat(to, slash);
	}
	else {
		if(strlen(save_cwd) + 1 + strlen(from) + 1 > size){
			printk("absolutize : unable to fit '%s' into %d "
			       "chars\n", from, size);
			return(-1);
		}
		strcpy(to, save_cwd);
		strcat(to, "/");
		strcat(to, from);
	}
	chdir(save_cwd);
	return(0);
}

int create_cow_file(char *cow_file, char *backing_file, int sectorsize, 
		    int *bitmap_offset_out, int *bitmap_len_out, 
		    int *data_offset_out)
{
        struct cow_header_v2 *header;
	struct stat64 buf;
	__u64 blocks;
	long zero;
	int err, fd, i, flags;
	__u64 size;

	flags = O_RDWR | O_CREAT;
	fd = open_ubd_file(cow_file, &flags, NULL, NULL, NULL, NULL);
	if(fd < 0) return(fd);

	err = -ENOMEM;
	header = um_kmalloc(sizeof(*header));
	if(header == NULL){
		printk("Failed to allocate COW V2 header\n");
		goto out_close;
	}
	header->magic = htonl(COW_MAGIC);
	header->version = htonl(COW_VERSION);

	err = -EINVAL;
	if(strlen(backing_file) > sizeof(header->backing_file) - 1){
		printk("Backing file name \"%s\" is too long - names are "
		       "limited to %d characters\n", backing_file, 
		       sizeof(header->backing_file) - 1);
		goto out_free;
	}

	if(absolutize(header->backing_file, sizeof(header->backing_file), 
		      backing_file))
		goto out_free;

	err = stat64(header->backing_file, &buf);
	if(err < 0) goto out_free;

	header->mtime = htonl(buf.st_mtime);
	header->size = htonll(buf.st_size);
	header->sectorsize = htonl(sectorsize);
	size = buf.st_size;

	err = write(fd, header, sizeof(*header));
	if(err != sizeof(*header)) goto out_free;

	blocks = (size + sectorsize - 1) / sectorsize;
	blocks = (blocks + sizeof(long) * 8 - 1) / (sizeof(long) * 8);
	zero = 0;
	for(i = 0; i < blocks; i++){
		err = write(fd, &zero, sizeof(zero));
		if(err != sizeof(zero)) goto out_free;
	}

	sizes(size, sectorsize, sizeof(struct cow_header_v2), 
	      bitmap_len_out, data_offset_out);
	*bitmap_offset_out = sizeof(struct cow_header_v2);

	kfree(header);
	return(fd);

 out_free:
	kfree(header);
 out_close:
	close(fd);
	return(err);
}

int read_ubd_fs(int fd, void *buffer, int len)
{
	return(read(fd, buffer, len));
}

int write_ubd_fs(int fd, char *buffer, int len)
{
	return(write(fd, buffer, len));
}

int ubd_is_dir(char *file)
{
	struct stat64 buf;

	if(stat64(file, &buf) < 0) return(0);
	return(S_ISDIR(buf.st_mode));
}

__u64 last_offset = -1;

int do_io(struct io_thread_req *req)
{
	char *buf;
	unsigned long len;
	int n, nsectors, start, end, bit;
       __u64 off;

	nsectors = req->length / req->sectorsize;
	start = 0;
	do {
		bit = ubd_test_bit(start, &req->sector_mask);
		end = start;
		while((end < nsectors) && 
		      (ubd_test_bit(end, &req->sector_mask) == bit))
			end++;

		if(end != nsectors)
			printk("end != nsectors\n");
		off = req->offset + req->offsets[bit];
		len = (end - start) * req->sectorsize;
		buf = &req->buffer[start * req->sectorsize];
		last_offset = off;

		if(lseek64(req->fds[bit], off, SEEK_SET) != off){
			printk("do_io - lseek failed : errno = %d\n",
			       errno);
			return(-1);
		}
		if(req->read){
			n = read(req->fds[bit], buf, len);
			if(n <= 0){
				printk("do_io - read returned %d : "
				       "errno = %d fd = %d\n", n, 
				       errno, req->fds[bit]);
				return(-1);
			}
			else if(n != len) memset(&buf[n], 0, len - n);
		}
		else {
			n = write(req->fds[bit], buf, len);
			if(n != len){
				printk("do_io - write returned %d : "
				       "errno = %d fd = %d\n", n, 
				       errno, req->fds[bit]);
				return(-1);
			}
		}

		start = end;
	} while(start < nsectors);

	if(req->bitmap_start != -1){
		off = req->bitmap_start * sizeof(req->bitmap[0]);
		off += req->bitmap_offset;
		if(lseek64(req->fds[1], off, SEEK_SET) != off){
			printk("do_io - bitmap lseek failed : errno = %d\n",
			       errno);
			return(-1);
		}
		len = (req->bitmap_end - req->bitmap_start) * 
			sizeof(req->bitmap[0]);
		n = write(req->fds[1], &req->bitmap[req->bitmap_start], len);
		if(n != len){
			printk("do_io - bitmap update returned %d : "
			       "errno = %d fd = %d\n", n, errno, req->fds[1]);
			return(-1);
		}
	}
	return(0);
}

int kernel_fds[2] = { -1, -1 };

int io_count = 0;

int io_thread(void *arg)
{
	struct io_thread_req req;
	int n;

	while(1){
		n = read(kernel_fds[0], &req, sizeof(req));
		if(n < 0) printk("io_thread - read returned %d, errno = %d\n",
				 n, errno);
		else if(n < sizeof(req)){
			printk("io_thread - short read : length = %d\n", n);
			continue;
		}
		io_count++;
		if(do_io(&req)) req.error = 1;
		if(write(kernel_fds[1], &req, sizeof(req)) != sizeof(req))
			printk("io_thread - write failed, errno = %d\n",
			       errno);
	}
}

int start_io_thread(unsigned long sp, int *fds_out)
{
	int pid;

	if((kernel_fds[0] = get_pty()) < 0) return(-1);
	raw(kernel_fds[0], 1);
	if((fds_out[0] = open(ptsname(kernel_fds[0]), O_RDWR)) < 0){
		printk("Couldn't open tty for IO\n");
		return(-1);
	}
	fds_out[1] = fds_out[0];
	kernel_fds[1] = kernel_fds[0];
	pid = clone(io_thread, (void *) sp, CLONE_VM | CLONE_FILES | SIGCHLD,
		    NULL);
	if(pid < 0){
		printk("start_io_thread - clone failed : errno = %d\n", errno);
		return(-errno);
	}
	return(pid);
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
