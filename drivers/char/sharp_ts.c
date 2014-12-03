/*
 *  linux/drivers/char/sharp_ts.c
 *
 *  Driver for TouchPanel devices On SHARP PDA
 *
 *  ChangeLog:
 *
 */

#include <linux/config.h>
#include <linux/init.h>
#include <linux/poll.h>
#include <asm/segment.h>
#include <asm/uaccess.h>
#include <asm/arch/irqs.h>
#include <asm/irq.h>
#include <asm/delay.h>
#include <asm/hardware.h>

#include <asm/arch/sib.h>
#include <asm/arch/fpga.h>

#include <asm/arch/touchscr_iris.h>
#include <asm/arch/sspads_iris.h>

#include <asm/sharp_ts.h>

/*****************************************************************************************
	Tablet sampling data queue operations
*****************************************************************************************/

#define DPRINTK(x, args...)  /* printk(__FUNCTION__ ": " x,##args) */

static void init_sample_queue(void);
static void release_sample_queue(void);
static int cmp_sample(tsEvent *sample0, tsEvent *sample1);
static int get_current_sample_id(void);
static int get_next_sample_id(void);
static int wait_for_queue_acceessable_without_sche(void);
static int get_sample_from_queue(tsEvent *sample);
int set_sample_to_queue(tsEvent *sample);

static ssize_t ts_read(struct file *file, char *buffer, size_t count, loff_t *ppos);
static uint ts_poll(struct file *filep, poll_table *wait);
static int ts_ioctl(struct inode *inode,struct file *file,unsigned int command,unsigned long arg);
static int ts_fasync(int fd, struct file *filep, int mode);
static int ts_open(struct inode *inode, struct file *file);
static int ts_release(struct inode *inode, struct file *file);


#define QUEUE_IS_UNLOCKED	0
#define QUEUE_IS_LOCKED		1

#ifdef CONFIG_IRIS
#define PENDOWN_TH 9000
#else
#error "not implemented"
#endif

#define SAMPLE_PEN_IS_DOWN(sample)  (((sample)->pressure) < PENDOWN_TH && ((sample)->pressure) > 0)

static tsSampleQueue	sample_queue;
static DECLARE_WAIT_QUEUE_HEAD(wq_in);
static struct fasync_struct	*fasyncptr;

static void init_sample_queue(void)
{
  const tsEvent	start_pendown_readbuf = {0, 0, 0 , 0};

  memset(&sample_queue, 0, sizeof(sample_queue));

  memcpy(&sample_queue.last_pendown_buf, &start_pendown_readbuf, sizeof(tsEvent));
}


static void release_sample_queue(void)
{
  DPRINTK("1\n");
  wake_up_interruptible(&wq_in);
  DPRINTK("done\n");
}


static int cmp_sample(tsEvent *sample0, tsEvent *sample1)
{
  DPRINTK("1\n");

  if (	sample0->x == sample1->x
	&& sample0->y == sample1->y
	&& ( ( SAMPLE_PEN_IS_DOWN(sample0) && SAMPLE_PEN_IS_DOWN(sample1) )
	     || ( ! SAMPLE_PEN_IS_DOWN(sample0) && ! SAMPLE_PEN_IS_DOWN(sample1) ) ) ){
    DPRINTK("done 1\n");
    return 0;
  }
  DPRINTK("done 2\n");
  return (-1);
}


static int get_current_sample_id(void)
{
  int	current_id = (-1);

  DPRINTK("1\n");

  current_id = sample_queue.head+sample_queue.count-1;
  if (current_id >= 0) {
    if (current_id >= SAMPLE_BUF_SIZE) {
      current_id -= SAMPLE_BUF_SIZE;
      if (current_id >= sample_queue.head) {
	current_id = (-1);
      }
    }
  }

  DPRINTK("done\n");
  
  return current_id;
}

static int get_next_sample_id(void)
{
  int	next_id;

  DPRINTK("1\n");

  next_id = sample_queue.head+sample_queue.count;
  if (next_id >= 0) {
    if (next_id >= SAMPLE_BUF_SIZE) {
      next_id -= SAMPLE_BUF_SIZE;
      if (next_id >= sample_queue.head) {
	next_id = (-1);
      }
    }
  }

  DPRINTK("done\n");
  
  return next_id;
}

static int wait_for_queue_acceessable_without_sche(void)
{
  unsigned int	loop_cnt;

  if( sample_queue.is_locked == QUEUE_IS_LOCKED ){
    return -1;
  }else{
    return 0;
  }

  loop_cnt = 0;
  while (QUEUE_IS_LOCKED == sample_queue.is_locked) {
    if (loop_cnt >= UINT_MAX) {
      return (-1);	// Timeout
    }
    loop_cnt++;
  }

  sample_queue.is_locked = QUEUE_IS_LOCKED;

  return 0;
}

static int get_sample_from_queue(tsEvent *sample)
{
  int	ret = 0;
  struct timeval	tv;

  DPRINTK("1\n");

  if (0 > wait_for_queue_acceessable_without_sche()) {
    return 0;
  }

  DPRINTK("2\n");

  if (sample_queue.count > 0) {
    memcpy(sample, &sample_queue.buf[sample_queue.head], sizeof(tsEvent));
    sample_queue.count--;
    if (0 == sample_queue.count) {
      sample_queue.head = 0;
    }
    else {
      sample_queue.head++;
      if (SAMPLE_BUF_SIZE <= sample_queue.head) {
	sample_queue.head = 0;
      }
    }
    ret = 1;
  }
  else {
    sample->x = sample_queue.last_pendown_buf.x;
    sample->y = sample_queue.last_pendown_buf.y;
    sample->pressure = sample_queue.last_pendown_buf.pressure;
    do_gettimeofday(&tv);
    sample->timestump
      = (long long)tv.tv_sec*(long long)1E6+(long long)tv.tv_usec;
  }
  sample_queue.is_locked = QUEUE_IS_UNLOCKED;
  DPRINTK("done\n");
  return ret;
}

int set_sample_to_queue(tsEvent *sample)
{
  int	cur_sample_id;
  int	next_sample_id;
  int	ret = 0;

  DPRINTK("1\n");

  if (0 > wait_for_queue_acceessable_without_sche()) {
    return 0;
  }

  DPRINTK("2\n");

  if( SAMPLE_PEN_IS_DOWN(sample) ){
    memcpy(&sample_queue.last_pendown_buf, sample, sizeof(tsEvent));
  }else{
    if( SAMPLE_PEN_IS_DOWN(&sample_queue.last_pendown_buf) ){
      sample->x = sample_queue.last_pendown_buf.x;
      sample->y = sample_queue.last_pendown_buf.y;
      //sample->pressure = sample_queue.last_pendown_buf.pressure;
    }
  }

  if (sample_queue.count < SAMPLE_BUF_SIZE) {
    if (sample_queue.count == 0) {
      memcpy(&sample_queue.buf[0], sample, sizeof(tsEvent));
      sample_queue.count++;
      ret = 1;
      if (fasyncptr) {
	kill_fasync(&fasyncptr, SIGIO, POLL_IN);
      }
      wake_up_interruptible(&wq_in);
    }
    else {
      cur_sample_id = get_current_sample_id();
      if (0 != cmp_sample(&sample_queue.buf[cur_sample_id], sample)) {
	if (0 <= (next_sample_id = get_next_sample_id())) {
	  memcpy(&sample_queue.buf[next_sample_id], sample, sizeof(tsEvent));
	  sample_queue.count++;
	  ret = 1;
	  if (fasyncptr) {
	    kill_fasync(&fasyncptr, SIGIO, POLL_IN);
	  }
	}
      }
    }
  }
  sample_queue.is_locked = QUEUE_IS_UNLOCKED;

  DPRINTK("done\n");
  return ret;
}

/*****************************************************************************************
       file operations
*****************************************************************************************/

static int ts_ref_count = 0;


static ssize_t ts_read(struct file *file, char *buffer, size_t count, loff_t *ppos)
{
  tsEvent	read_pos;
  ssize_t	ret_size;
  int retval;

  DPRINTK("1\n");

  if( count < sizeof(tsEvent) ){
    return 0;
  }
  
  if (0 == (file->f_flags & O_NONBLOCK)) {
    DPRINTK("1-1\n");
    while (0 == sample_queue.count) {
      interruptible_sleep_on_timeout(&wq_in, HZ);
    }
    DPRINTK("1-2\n");
  }
  retval = get_sample_from_queue(&read_pos);
  if( ! retval ) return -ENODATA;
  DPRINTK("2\n");
  copy_to_user(buffer, &read_pos, sizeof(tsEvent));

  ret_size = sizeof(tsEvent);

#if 0
  if ( ! SAMPLE_PEN_IS_DOWN(&read_pos) ){
    ret_size = -ENODATA;
  }
  else  {
    ret_size = sizeof(tsEvent);
  }
#endif
  DPRINTK("done\n");
  return ret_size;
}


static uint ts_poll(struct file *filep, poll_table *wait)
{
  uint	mask = 0;

  if (sample_queue.count) {
    mask = POLLIN|POLLRDNORM;
    goto END;
  }

  poll_wait(filep, &wq_in, wait);

  if (sample_queue.count)
    mask = POLLIN|POLLRDNORM;

END:
  return mask;
}

static int ts_ioctl(struct inode *inode,
		    struct file *file,
		    unsigned int command,
		    unsigned long arg)
{
  switch( command ) {
  default:
    return -EINVAL;
  }
  return 0;
}

static int ts_fasync(int fd, struct file *filep, int mode)
{
  int	ret;

  ret = fasync_helper((int)fd, filep, mode, &fasyncptr);

  if (ret < 0) {
    return ret;
  }
  return 0;
}


extern int iris_ts_open_arch(struct inode *inode, struct file *file);
extern int iris_ts_release_arch(struct inode *inode, struct file *file);

static int ts_open(struct inode *inode, struct file *file)
{
  int error;

  if( ! ts_ref_count ){
    /* initialize arch-dependent part */
    error = iris_ts_open_arch(inode,file);
    if( error ) return error;
    ts_fasync(-1, file, 0);
    init_sample_queue();
  }

  ts_ref_count++;

  return 0;
}

static int ts_release(struct inode *inode, struct file *file)
{
  int error;
  ts_ref_count--;
  if( ! ts_ref_count ){
    /* release arch-dependent part */
    error = iris_ts_release_arch(inode,file);
    if( error ) return error;
    release_sample_queue();
  }
  return 0;
}


struct file_operations ts_fops = {
  read:	   ts_read,	/* read */
  poll:	   ts_poll,	/* poll */
  ioctl:   ts_ioctl,	/* ioctl */
  open:    ts_open,	/* open */
  release: ts_release,	/* release */
  fasync:  ts_fasync,	/* fasync */
};



/*****************************************************************************************
       initialize operations
*****************************************************************************************/

extern int __init iris_ts_arch_init(void);

int __init sharp_ts_init(void)
{

  /* init archtecture-dependent part */
  iris_ts_arch_init();

  fasyncptr = NULL;
	
  if ( register_chrdev(TS_MAJOR,"ts",&ts_fops) )
    printk("unable to get major %d for touch screen\n", TS_MAJOR);
  
  return 0;
}

module_init(sharp_ts_init);

/*
 *   end of source
 */
