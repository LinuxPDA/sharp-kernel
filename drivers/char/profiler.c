/*
 * Itsy SA1100 Statistical Profiler
 * Copyright (c) Compaq Computer Corporation, 1999
 *
 * Use consistent with the GNU GPL is permitted,
 * provided that this copyright notice is
 * preserved in its entirety in all copies and derived works.
 *
 * COMPAQ COMPUTER CORPORATION MAKES NO WARRANTIES, EXPRESSED OR IMPLIED,
 * AS TO THE USEFULNESS OR CORRECTNESS OF THIS CODE OR ITS
 * FITNESS FOR ANY PARTICULAR PURPOSE.
 *
 * Author: Carl Waldspurger
 *
 * $Log: itsy_prof.c,v $
 * Revision 2.8  2000/11/22  10:27:07  kerr
 * Port to 2.4.X Linux.  Made less Itsy specific.
 *
 * Revision 2.7  2000/06/02  19:23:21  kerr
 * Made modifications to share the general purpose timer (OST2).
 *
 * Revision 2.6  1999/12/23  01:27:56  kerr
 * Updated Copyright information for entire source tree.
 *
 * Revision 2.5  1999/08/17  01:05:45  caw
 * Bug fix: avoid invalid inode lookups.  Distinguish nomap, kernel samples.
 *
 * Revision 2.4  1999/08/13 01:28:25  caw
 * Report information about sampling period in "/proc/prof".
 *
 * Revision 2.3  1999/08/13 00:44:14  caw
 * Changed sampling rate implementation (specify samples per second).
 * Added support for ITSY_PROF_{GET,SET}_CTL ioctls.
 *
 * Revision 2.2  1999/08/11 21:59:25  caw
 * Reduced logging, other minor modifications.
 *
 * Revision 2.1  1999/08/10  02:06:37  caw
 * Initial revision.
 *
 */

/*
 * Overview
 *
 * This driver performs interrupt-driven statistical profiling
 * of the entire running system.  Since it uses a high-priority
 * OS timer, virtually all interruptible code will be profiled.
 *
 */

/*
 * debugging 
 *
 */

#define	PROF_DEBUG			(1)
#define	PROF_DEBUG_VERBOSE		(0)

/*
 * includes
 *
 */

#include <linux/config.h>

#ifdef	MODULE
#include <linux/module.h>
#include <linux/version.h>
#endif

#include <linux/types.h>
#include <linux/fs.h>
#include <linux/mm.h>
#include <linux/malloc.h>
#include <linux/sched.h>
#include <linux/major.h>
#include <linux/ctype.h>
#include <linux/wrapper.h>
#include <linux/errno.h>

#ifdef	CONFIG_PROC_FS
#include <linux/stat.h>
#include <linux/proc_fs.h>
#endif

#include <asm/segment.h>
#include <asm/io.h>
#include <asm/uaccess.h>
#include <asm/arch/irqs.h>

#include <asm/hardware/hwtimer.h>

#include <linux/profiler.h>

/*
 * constants
 *
 */

/* names */
#define	PROF_NAME		"profiler"
#define	PROF_NAME_VERBOSE	"profiling driver"
#define	PROF_VERSION_STRING	"$Revision: 2.8 $"

/* device numbers */
int gMajor = 0;

/* hardware timer */
#define PROF_TIMER_FREQ		(3686400)
#define	PROF_TIMER_NUM		(2)
#define	PROF_TIMER_MASK		(1 << PROF_TIMER_NUM)

/* sample buffer */
#define	PROF_BUF_LG_PAGES	(5)
#define	PROF_BUF_NBYTES		(PAGE_SIZE << PROF_BUF_LG_PAGES)
#define	PROF_BUF_NSAMPLES	(PROF_BUF_NBYTES / sizeof(profiler_sample))
#define	PROF_BUF_WAKEUP_MASK	(0x0f)
#define	PROF_BUF_MIN_XFER	(8)

/* randomized sampling defaults */
#define	PROF_RANDOMIZE_DEFAULT	(1)
#define	PROF_RANDOM_SEED	(101)

/* clients */
#define	PROF_CLIENT_ID_INVALID	(-1)

/*
 * types
 *
 */

/* convenient abbrevs */
typedef profiler_sample prof_sample;
typedef profiler_stats  prof_stats;
typedef profiler_pos    prof_pos;
typedef profiler_ctl    prof_ctl;

/* user-space profiling client */
typedef int prof_client_id;
typedef struct prof_client {
  /* linked list */
  struct prof_client *prev, *next;

  /* identifier */
  prof_client_id id;

  /* position (in samples) */
  prof_pos position;
  int reset;

  /* dropped samples */
  ulong dropped;
  int drop_fail;
} prof_client;

/* profiler state */
typedef struct {
  struct hwtimer_list timers;

  /* random numbers */
  ulong random_seed;

  /* control parameters */
  prof_ctl control;

  /* sampling period */
  ulong period_base;
  ulong period_rnd_mask;
  ulong period_last;

  /* interrupts */
  int handle_interrupts;

  /* samples */
  wait_queue_head_t waitq;
  profiler_sample *buf;
  prof_pos position;
  uint wakeup_mask;
  uint next_mask;

  /* clients */
  prof_client_id client_id_next;
  prof_client *clients;
  int nclients;
  
  /* statistics */
  prof_stats stats;

} prof_dev;

/*
 * globals
 *
 */

static prof_dev prof_global;

/*
 * list operations
 *
 */

static void prof_client_insert(prof_client **head, prof_client *e)
{
  prof_client *h = *head;

  /* add element to head of list */
  e->next = h;
  if (h != NULL)
    h->prev = e;
  *head = e;
  e->prev = NULL;
}

static void prof_client_remove(prof_client **head, prof_client *e)
{
  /* splice element out of list */
  if (e->prev != NULL)
    e->prev->next = e->next;
  else
    *head = e->next;
  if (e->next != NULL)
    e->next->prev = e->prev;
}

/*
 * utility operations
 *
 */
 
#ifndef	MIN
#define MIN(a,b)                (((a)<(b))?(a):(b))
#define MAX(a,b)                (((a)>(b))?(a):(b))
#endif

static int lg(int value)
{
  /*
   * modifies: nothing
   * effects:  Returns the log base 2 of value.
   *
   */

  int result = 0;

  while (value > 1)
    {
      result++;
      value >>= 1;
    }

  return(result);
}

/*
 * prof_position operations
 *
 */

static inline u32 prof_pos_low32(prof_pos pos)
{
  return((u32) (pos & 0xffffffff));
}

static inline prof_pos prof_pos_get_user(const prof_pos *ptr)
{
  /* copyin from user space */
  prof_pos value;
  if (copy_from_user((void *) &value, (const void *) ptr, sizeof(prof_pos))) return (prof_pos) -EFAULT;
  return(value);
}

static inline int prof_pos_put_user(prof_pos value, prof_pos *ptr)
{
  /* copyout to user space */
  if (copy_to_user((void *) ptr, (const void *) &value, sizeof(prof_pos))) return -EFAULT;
  return 0;
}

/*
 * procfs operations
 *
 */

#ifdef	CONFIG_PROC_FS
static int prof_get_info(char *page,
			 char **start,
			 off_t off,
			 int count, 
			 int *eof,
			 void *unused)
{
  prof_dev *dev = &prof_global;
  prof_stats *stats = &dev->stats;
  int len;
  char *out = page;

  /* shared stats */
  out += sprintf(out,
		 "major:    %8d\n"
		 "intrs:    %8lu\n"
		 "position: %8u\n"
		 "samples:  %8lu\n"
		 "  user:   %8lu\n"
		 "  kern:   %8lu\n"
		 "  nomap:  %8lu\n"
		 "clients:  %8d\n"
		 "wakeups:  %8lu\n",
		 gMajor,
		 stats->interrupts,
		 prof_pos_low32(dev->position),
		 stats->samples,
		 stats->user,
		 stats->kernel,
		 stats->nomap,
		 dev->nclients,
		 stats->wakeups);
  
  if (PROF_DEBUG)
    out += sprintf(out,
		   "\n"
		   "base per:  %8lu\n"
		   "rnd mask:  %8lx\n"
		   "last per:  %8lu\n"
		   "rnd seed:  %8lx\n",
		   dev->period_base,
		   dev->period_rnd_mask,
		   dev->period_last,
		   dev->random_seed);

  len = out - page;
  len -= off;
  if (len < count) {
    *eof = 1;
    if (len <= 0)
      return 0;
  } else
    len = count;
  
  *start = page + off;
  
  return(len);  
}
#endif	/* CONFIG_PROC_FS */

/*
 * prof_client operations
 *
 */

static void prof_client_destroy(prof_client *client)
{
  /*
   * modifies: client
   * effects:  Reclaims all storage associated with client.
   *
   */

  /* sanity check */
  if (client == NULL)
    return;

  /* deallocate container */
  kfree((void *)client);
}

static prof_client *prof_client_create(void)
{
  /*
   * modifies: nothing
   * effects:  Creates and returns a new, initialized prof_client object.
   *           Returns NULL iff unable to allocate storage.
   *
   */

  prof_client *client;

  /* allocate client, fail if unable */
  if ((client = (prof_client *) kmalloc(sizeof(prof_client), GFP_KERNEL)) == NULL)
    return(NULL);

  /* initialize client */
  memset((void *) client, 0, sizeof(prof_client));
  client->id = PROF_CLIENT_ID_INVALID;

  /* initialize reset flag */
  client->reset = 1;

  /* everything OK */
  return(client);
}

/*
 * prof_dev operations
 *
 */

static int prof_dev_set_ctl_prim(prof_dev *dev, const prof_ctl *ctl)
{
  /*
   * modifies: dev
   * effects:  Sets the control parameters for dev using ctl.
   *
   */

  ulong period_base, period_rnd, period_rnd_mask;
  prof_ctl control;
  ulong flags;

  /* ensure valid frequency range */
  if ((ctl->frequency < PROFILER_FREQ_MIN) ||
      (ctl->frequency > PROFILER_FREQ_MAX))
    return(-EINVAL);

  /* convert ctl parameters to period data */
  period_base = PROF_TIMER_FREQ / ctl->frequency;

  /* XXX random range is hardcoded as +/- 1/16 of base freq */
  period_rnd = (1 << lg(period_base >> 4));
  period_rnd_mask = (ctl->randomize ? (period_rnd - 1) : 0);

  /* install parameters (critical section) */
  save_flags_cli(flags);
  {
    dev->control = control;
    dev->period_base = period_base;
    dev->period_rnd_mask = period_rnd_mask;
  }
  restore_flags(flags);

  /* debugging */
  if (PROF_DEBUG_VERBOSE)
    printk(PROF_NAME ": prof_dev_set_ctl: base=%lu, rnd_mask=%lx\n",
	   dev->period_base,
	   dev->period_rnd_mask);

  /* everything OK */
  return(0);
}

static void prof_interrupt(int irq, void *dev_id, struct pt_regs *regs, unsigned long unused);

static void prof_dev_init(prof_dev *dev)
{
  /* initialize */
  memset((void *) dev, 0, sizeof(prof_dev));

  /* memory-mapped registers */
  init_hwtimer(&dev->timers);
  dev->timers.function = prof_interrupt;

  /* random numbers */
  dev->random_seed = PROF_RANDOM_SEED;

  /* control parameters */
  dev->control.frequency = PROFILER_FREQ_DEFAULT;
  dev->control.randomize = PROFILER_RND_DEFAULT;

  /* sampling period */
  prof_dev_set_ctl_prim(dev, &dev->control);

  /* samples */
  init_waitqueue_head(&dev->waitq);
  dev->buf = NULL;
  dev->position = 0;
  dev->wakeup_mask = PROF_BUF_WAKEUP_MASK;
  dev->next_mask = PROF_BUF_NSAMPLES - 1;

  /* clients */
  dev->clients  = NULL;
  dev->nclients = 0;

}


static prof_client *prof_dev_client_create(prof_dev *dev)
{
  /*
   * modifies: dev
   * effects:  Creates a new, initialized client of dev.
   *           Returns the new client iff successful, otherwise NULL.
   *
   */

  prof_client *client;
  ulong flags;

  /* allocate new client, fail if unable */
  if ((client = prof_client_create()) == NULL)
    return(NULL);
  
  /* generate new client id */
  client->id = dev->client_id_next++;

  /* critical section */
  save_flags_cli(flags);
  {
    /* add to client list */
    prof_client_insert(&dev->clients, client);
    dev->nclients++;
  }
  restore_flags(flags);

  if (PROF_DEBUG_VERBOSE)
    printk(PROF_NAME ": prof_dev_client_create: id=%u\n", client->id);

  /* everything OK */
  return(client);
}

static int prof_dev_client_destroy(prof_dev *dev, prof_client *client)
{
  /*
   * modifies: dev
   * effects:  Detaches client from dev, and reclaims all associated storage.
   *           Returns 0 iff successful, otherwise error code.
   *
   */

  ulong flags;

  if (PROF_DEBUG_VERBOSE)
    printk(PROF_NAME ": prof_dev_client_destroy: id=%d\n", client->id);

  /* critical section */
  save_flags_cli(flags);
  {
    /* remove from client list */
    prof_client_remove(&dev->clients, client);
    dev->nclients--;
  }
  restore_flags(flags);

  /* reclaim storage */
  prof_client_destroy(client);

  /* everything OK */
  return(0);
}

static inline prof_pos prof_dev_get_position(const prof_dev *dev)
{
  /*
   * modifies: nothing
   * effects:  Returns a consistent snapshot of the current sample
   *           position associated with dev.
   *
   */

  prof_pos pos;
  ulong flags;

  /* critical section */
  save_flags_cli(flags);
  {
    pos = dev->position;
  }
  restore_flags(flags);

  return(pos);
}

static int prof_client_get_position(prof_client *client,
				    prof_pos *pos)
{
  /*
   * requires: pos is a user-space pointer
   * modifies: pos
   * effects:  Sets pos to the current stream position (in samples)
   *           associated with client.
   *
   */

  if (PROF_DEBUG_VERBOSE)
    printk(PROF_NAME ": prof_client_get_position\n");

  /* copyout reply value */
  return(prof_pos_put_user(client->position, pos));
}

static void prof_dev_buf_destroy(prof_dev *dev)
{
  /*
   * modifies: dev
   * effects:  Reclaims sample buffer storage associated with dev.
   *
   */

  /* sanity check */
  if (dev == NULL)
    return;

  /* deallocate circular buffer */
  if (dev->buf != NULL)
    {
      free_pages((ulong) dev->buf, PROF_BUF_LG_PAGES);
      dev->buf = NULL;
    }
}

static int prof_dev_buf_create(prof_dev *dev)
{
  /*
   * modifies: nothing
   * effects:  Creates sample buffer storage associated with dev.
   *           Returns 0 iff successful, otherwise error code.
   *
   */

  /* allocate buffer */
  dev->buf = (prof_sample *) __get_dma_pages(GFP_KERNEL, PROF_BUF_LG_PAGES);
  if (dev->buf == NULL)
    {
      /* cleanup and fail */
      prof_dev_buf_destroy(dev);
      return(-ENOMEM);
    }

  /* initialize */
  memset((void *) dev->buf, 0, PROF_BUF_NBYTES);
  dev->position = 0;
  
  /* everything OK */
  return(0);
}

static int prof_dev_buf_copyout(prof_dev *dev, 
				prof_sample *to,
				prof_pos start,
				int nsamples)
{
  /*
   * requires: to is a user-space pointer
   * modifies: to
   * effects:  Transfers nsamples of profiling data to user-space buffer.
   *
   */  

  uint next, mask;
  int i;

  /* XXX grossly inefficient */
  mask = dev->next_mask;
  next = start & mask;
  for (i = 0; i < nsamples; i++, next++)
    if (copy_to_user((void *) &to[i],
		(const void *) &dev->buf[next & mask],
		     sizeof(prof_sample))) return -EFAULT;
  return 0;
}

static int prof_dev_get_ctl(prof_dev *dev, prof_ctl *ctl)
{
  /*
   * requires: ctl is a user-space pointer
   * modifies: ctl
   * effects:  Sets ctl to the current control parameters for dev.
   *
   */

  /* copyout reply value */
  if (copy_to_user((void *) ctl, (const void *) &dev->control, sizeof(prof_ctl))) return -EFAULT;
  return(0);
}

static int prof_dev_set_ctl(prof_dev *dev, const prof_ctl *ctl)
{
  /*
   * requires: ctl is a user-space pointer
   * modifies: dev
   * effects:  Sets the control parameters for dev using ctl.
   *
   */

  prof_ctl control;

  /* copy argument from user space */
  if (copy_from_user((void *) &control, (const void *) ctl, sizeof(prof_ctl))) return -EFAULT;

  /* update control parameters */
  return(prof_dev_set_ctl_prim(dev, &control));
}

static int prof_client_get_dropped(const prof_client *client,
				   prof_pos *count)
{
  /*
   * requires: count is a user-space pointer
   * modifies: count
   * effects:  Sets count to the number of samples dropped by client.
   *
   */

  /* sanity check */
  if (client == NULL)
    return(-EINVAL);

  /* copyout reply value */
  return(prof_pos_put_user(client->dropped, count));
  return(0);
}

static int prof_client_set_dropped(prof_client *client,
				   const prof_pos *count)
{
  /*
   * requires: count is a user-space pointer
   * modifies: client
   * effects:  Sets the number of samples dropped by client to count.
   *
   */

  /* sanity check */
  if (client == NULL)
    return(-EINVAL);

  /* copy argument from user space */
  client->dropped = prof_pos_get_user(count);
  return(0);
}

static int prof_client_set_drop_fail(prof_client *client, int fail)
{
  /*
   * modifies: client
   * effects:  Specifies client behavior when samples are dropped.
   *           If fail is set, dropped samples will result in EIO
   *           errors for relevant system calls.
   *
   */

  /* update client state */
  client->drop_fail = fail;

  /* everything OK */
  return(0);
}

static int prof_client_set_position(prof_dev *dev,
				    prof_client *client,
				    prof_pos *pos)
{
  /*
   * requires: pos is a user-space pointer
   * modifies: client, pos
   * effects:  Sets stream position associated with client to the
   *           current position associated with dev plus pos.
   *           Updates pos to contain the modified client value.
   *           Returns 0 iff successful, otherwise error code.
   *
   */

  prof_pos delta;

  /* sanity check */
  if (client == NULL)
    return(-EINVAL);
  
  /* copy argument from user space */
  delta = prof_pos_get_user(pos);
  
  /* update client stream position */
  client->position = prof_dev_get_position(dev) + delta;

  /* update argument value */
  prof_pos_put_user(client->position, pos);

  /* everything OK */
  return(0);
}

/*
 * interrupt processing
 *
 */

static int prof_map_pc(struct task_struct *task,
		       ulong pc,
		       prof_sample *sample)
{
  /*
   * modifies: sample
   * effects:  Maps pc to an executable image (device, inode) and
   *           the offset within that executable image.  Updates
   *           corresponding fields within sample.  Returns 0 iff
   *           successful, otherwise error code.
   *
   */

  struct vm_area_struct *map;

  /* sanity checks */
  if ((task == NULL) || (task->mm == NULL))
    return(-EINVAL);

  /* search for map containing pc */
  for (map = task->mm->mmap; map != NULL; map = map->vm_next)
    if ((pc >= map->vm_start) && (pc <= map->vm_end))
      {
	struct inode* vm_inode = map->vm_file->f_dentry->d_inode;

	/* fail if not mapped image */
	if (vm_inode == NULL)
	  return(-EINVAL);

	/* map pc -> <device, inode, offset> */
	sample->device = vm_inode->i_dev;
	sample->inode  = vm_inode->i_ino;
	sample->offset = pc - map->vm_start;
	return(0);
      }

  /* not found */
  return(-ESRCH);
}

static inline ulong prof_random(ulong seed)
{
  /* requires: "long long" is 64 bits, "long" is 32 bits
   * modifies: nothing
   * effects:  Generates the next random number in the pseudo-random
   *           sequence defined by the multiplicative linear congruential
   *           generator S' = 16807 * S mod (2^31 - 1).  This is the ACM
   *           "minimal standard random number generator".  Based on
   *           method described by D.G. Carta in CACM, January 1990.
   */

  unsigned long long product;
  ulong product_lo, product_hi;
  long test;

  product = 33614 * (unsigned long long) seed;

  product_lo = product & (0xffffffff);
  product_hi = product >> 32;
  product_lo >>= 1;

  test = product_lo + product_hi;

  if (test > 0)
    return(test);

  test &= 0x7fffffff;
  return(test + 1);
}

static void prof_interrupt(int irq, void *dev_id, struct pt_regs *regs, unsigned long unused)
{
  /*
   * modifies: prof_global
   * effects:  Handle profiling interrupt.  Records sample data in a
   *           circular buffer, wakes up clients waiting for more data,
   *           and updates statistics.
   *
   */

  prof_dev *dev = &prof_global;
  prof_stats *stats = &dev->stats;

  ulong next, pc, period;
  profiler_sample *s;

  /* update statistics */
  stats->interrupts++;

  if (PROF_DEBUG_VERBOSE)
  {
    static int count = 0;
    if ((count++ & 0xff) == 0)
      printk(PROF_NAME ": prof_interrupt: count=%d\n", count);      
  }

  /* sanity check */
  if (!dev->handle_interrupts)
    {
      if (PROF_DEBUG)
	printk(PROF_NAME ": prof_interrupt: !handle_interrupts\n");
      return;
    }

  /* locate next buffer entry, advance position */
  next = dev->position & dev->next_mask;
  dev->position++;

  /* capture sample data */
  s = &dev->buf[next];
  s->count = 1;
  s->mode = processor_mode(regs);
  s->pid = current->pid;

  /* find executable image associated with pc */
  pc = instruction_pointer(regs);
  if (user_mode(regs))
    {
      /* user-mode sample: map pc to associated image */
      stats->user++;
      if (prof_map_pc(current, pc, s) != 0)
	{
	  /* unable to map, keep pc */
	  s->device = PROFILER_DEVICE_NONE;
	  s->inode  = PROFILER_INODE_NONE;
	  s->offset = pc;
	  stats->nomap++;
	}
    }
  else
    {
      /* kernel-mode sample: raw pc */
      stats->kernel++;
      s->device = PROFILER_DEVICE_NONE;
      s->inode  = PROFILER_INODE_KERNEL;
      s->offset = pc;
    }

  /* wakeup waiters periodically */
  if ((next & dev->wakeup_mask) == 0)
    {
      /* update statistics */
      stats->wakeups++;

      /* wake up waiters */
      wake_up_interruptible(&dev->waitq);  /* XXX timeout? */
    }

  /* update statistics */
  stats->samples++;

  /* compute sampling period */
  period = dev->period_base;
  if (dev->period_rnd_mask != 0)
    {
      ulong sign_mask = dev->period_rnd_mask + 1;
      dev->random_seed = prof_random(dev->random_seed);
      if (dev->random_seed & sign_mask)
	period += (dev->random_seed & dev->period_rnd_mask);
      else
	period -= (dev->random_seed & dev->period_rnd_mask);
    }

  /* reset timer */

  add_hwtimer(&dev->timers, period);
  dev->period_last = period;
}

/*
 * file operations
 *
 */

static int prof_read_dropped(const prof_dev *dev, prof_client *client)
{
  /*
   * modifies: client
   * effects:  If client is too far behind current device position,
   *           drops entire buffer; client skips over dropped samples.
   *           Returns TRUE iff samples were dropped.
   *
   */

  prof_pos position, behind, dropped;

  /* snapshot stream position */
  position = prof_dev_get_position(dev);
  
  /* done if not too far behind */
  behind = position - client->position;
  if (behind <= PROF_BUF_NSAMPLES)
    return(0);

  /* drop entire buffer, update stats */
  dropped = behind - PROF_BUF_NSAMPLES;
  client->position = position;
  client->dropped += behind;
  
  /* debugging */
  if (PROF_DEBUG_VERBOSE)
    printk(PROF_NAME ": prof_read_dropped: dropped %u, total %u\n",
	   prof_pos_low32(behind),
	   prof_pos_low32(client->dropped));
	  
  /* dropped samples */
  return(1);
}

static ssize_t prof_read(struct file *file,
			 char *buffer,
			 size_t count,
			 loff_t *ptr)
{
  prof_client *client = (prof_client *) file->private_data;
  prof_dev *dev = &prof_global;
  int total, err, interrupted;
  ulong flags;

  if (PROF_DEBUG_VERBOSE)
    printk(PROF_NAME ": prof_read: count=%d\n", count);

  /* Can't seek (pread) on this device */
  if (ptr != &file->f_pos)
    return -ESPIPE;

  /* enable interrupt-driven input */
  save_flags_cli(flags);
  {
    if (!dev->handle_interrupts)
      {
	add_hwtimer(&dev->timers, dev->period_base);
	dev->handle_interrupts = 1;
      }
  }
  restore_flags(flags);

  /* initialize */
  interrupted = 0;
  total = 0;

  while ((count > 0) && (!interrupted))
    {
      int count_samples, chunk_bytes, chunk_samples, min_xfer;
      prof_pos position, behind;

      /* snapshot current stream position */
      position = prof_dev_get_position(dev);

      /* handle client resets */
      if (client->reset)
	{
	  /* warp current stream position */
	  client->position = position;
	  client->reset  = 0;
	}
      behind = position - client->position;

      /* debugging */
      if (PROF_DEBUG_VERBOSE)
	printk(PROF_NAME ": prof_read: dpos=%u, cpos=%u, behind=%u\n",
	       prof_pos_low32(position),
	       prof_pos_low32(client->position),
	       prof_pos_low32(behind));

      /* wait until sufficient data ready for client transfer */
      min_xfer = MIN(count, PROF_BUF_MIN_XFER);
      if (behind < min_xfer)
	{
	  /* don't wait if non-blocking */
	  if (file->f_flags & O_NONBLOCK)
	    return((total > 0) ? total : -EAGAIN);

	  if (PROF_DEBUG_VERBOSE)
	    printk(PROF_NAME ": prof_read: wait\n");

	  /* wait for more data to become available */
	  interruptible_sleep_on(&dev->waitq);

	  /* handle interrupted system call */
	  if (signal_pending(current)) 
	    interrupted = 1;
	}

      /* skip over dropped samples */
      if (prof_read_dropped(dev, client))
	{
	  /* fail with I/O error, if specified by client */
	  if (client->drop_fail)
	    return(-EIO);
	  continue;
	}

      /* snapshot current stream position */
      position = prof_dev_get_position(dev);
      behind = position - client->position;

      /* copy chunk of data to user-space */
      count_samples = count / sizeof(prof_sample);
      chunk_samples = MIN(behind, count_samples);
      chunk_bytes = chunk_samples * sizeof(prof_sample);
      if ((err = prof_dev_buf_copyout(dev, 
			   (prof_sample *) buffer,
			   client->position,
			   chunk_samples)) != 0)
	return(err);

      /* ensure no samples dropped during copy */
      if (prof_read_dropped(dev, client))
	{
	  /* fail with I/O error, if specified by client */
	  if (client->drop_fail)
	    return(-EIO);
	  continue;
	}

      /* update counts */
      total  += chunk_bytes;
      buffer += chunk_bytes;
      count  -= chunk_bytes;
      
      /* update stream position */
      file->f_pos      += chunk_bytes;
      client->position += chunk_samples;
      
      /* debugging */
      if (PROF_DEBUG_VERBOSE)
	printk(PROF_NAME ": prof_read: chunk_bytes=%d\n", chunk_bytes);
    }

  /* handle interrupted read */
  if (interrupted && (total == 0))
    return(-EINTR);

  /* read complete */
  return(total);
}

static int prof_ioctl(struct inode *inode,
		      struct file *file,
		      uint command,
		      ulong arg)
{
  prof_client *client = (prof_client *) file->private_data;
  prof_dev *dev = &prof_global;
  int size, err;

  if (PROF_DEBUG_VERBOSE)
    printk(PROF_NAME ": prof_ioctl: cmd=%d, arg=%lu\n", command, arg);

  /* sanity checks */
  if ((_IOC_TYPE(command) != PROFILER_IOCTL_TYPE) ||
      (_IOC_NR(command) > PROFILER_IOCTL_MAXNR))
    return(-EINVAL);
  if (client == NULL)
    return(-EINVAL);

  /* verify transfer memory */
  size = _IOC_SIZE(command);
  if (_IOC_DIR(command) & _IOC_READ)
    if ((err = verify_area(VERIFY_WRITE, (void *) arg, size)) != 0)
      return(err);
  if (_IOC_DIR(command) & _IOC_WRITE)
    if ((err = verify_area(VERIFY_READ,  (void *) arg, size)) != 0)
      return(err);

  /* dispatch based on command */
  switch (command) {
  case PROFILER_GET_CTL:
    return(prof_dev_get_ctl(dev, (prof_ctl *) arg));
  case PROFILER_SET_CTL:
    return(prof_dev_set_ctl(dev, (prof_ctl *) arg));

  case PROFILER_GET_POSITION:
    return(prof_client_get_position(client, (prof_pos *) arg));
  case PROFILER_SET_POSITION:
    return(prof_client_set_position(dev, client, (prof_pos *) arg));

  case PROFILER_GET_DROPPED:
    return(prof_client_get_dropped(client, (prof_pos *) arg));
  case PROFILER_SET_DROPPED:
    return(prof_client_set_dropped(client, (prof_pos *) arg));
  case PROFILER_SET_DROP_FAIL:
    return(prof_client_set_drop_fail(client, arg));
  }

  /* invalid command */
  return(-EINVAL);
}

static int prof_open(struct inode *inode, struct file *file)
{
  prof_dev *dev = &prof_global;
  prof_client *client;
  int status, nclients;

  /* debugging */
  if (PROF_DEBUG_VERBOSE)
    printk(PROF_NAME ": prof_open\n");

  /* initialize */
  client = NULL;
  status = -ENOMEM;

  /* create new client */
  nclients = dev->nclients;
  if ((client = prof_dev_client_create(dev)) == NULL)
    goto prof_open_fail;

  /* create profiling buffer, if necessary */
  if (dev->buf == NULL)
    if (prof_dev_buf_create(dev) != 0)
      goto prof_open_fail;

#ifdef	MODULE
  MOD_INC_USE_COUNT;
#endif

  /* attach client state to file */
  file->private_data = client;

  /* everything OK */
  return(0);

prof_open_fail:

  /* reclaim storage and fail */
  if (client != NULL) 
    prof_dev_client_destroy(dev, client);
  return(status);
}

static void prof_dev_shutdown(prof_dev *dev)
{
  /* cleanup if no clients */
  if (dev->nclients == 0)
    {
      /* disable interrupts */
      if (dev->handle_interrupts) {
	del_hwtimer(&dev->timers);
	dev->handle_interrupts = 0;
      }

      /* reclaim buffer */
      prof_dev_buf_destroy(dev);
    }
}

static int prof_release(struct inode *inode, struct file *file)
{
  prof_client *client = (prof_client *) file->private_data;
  prof_dev *dev = &prof_global;

  /* detach client state from file */
  file->private_data = NULL;

  /* detach client */
  if (client != NULL)
    {
      prof_dev_client_destroy(dev, client);
      prof_dev_shutdown(dev);
    }

#ifdef	MODULE
  MOD_DEC_USE_COUNT;
#endif
  return(0);
}

/*
 * initialization
 *
 */

static struct file_operations prof_fops =
{
  read: prof_read,		
  ioctl: prof_ioctl,
  open: prof_open,
  release: prof_release,
};

static char *prof_version(void)
{
  /*
   * modifies: nothing
   * effects:  Returns the current cvs revision number.
   * storage:  Return value is statically-allocated.
   *
   */

  static char version[8];
  char *revision;
  int i = 0;

  /* extract version number from cvs string */
  for (revision = PROF_VERSION_STRING; *revision != '\0'; revision++)
    if (isdigit((int) *revision) || (*revision == '.'))
      version[i++] = *revision;
  version[i] = '\0';

  return(version);
}

int profiler_init(void)
{
  prof_dev *dev = &prof_global;
  int result;

  if (PROF_DEBUG_VERBOSE)
    printk(PROF_NAME ": profiler_init\n");
  if (PROF_DEBUG_VERBOSE)
    printk(PROF_NAME ": sizeof(prof_sample)=%d\n", sizeof(prof_sample));

  /* initialize global state */
  prof_dev_init(dev);

  /* register device */
  if ((result = register_chrdev(0, PROF_NAME, &prof_fops)) < 0)
    {
      /* log error and fail */
      printk(PROF_NAME ": can't get major number\n");
      return(result);
    }
  else {
    gMajor = result;
  }

#ifdef	CONFIG_PROC_FS
  /* register procfs device */
  {
    struct proc_dir_entry *prof_proc_entry = NULL;
    prof_proc_entry = create_proc_entry(PROF_NAME, S_IFREG|S_IRUGO, &proc_root);
    if (prof_proc_entry == NULL) {
      printk("idle_dev: unable to initialise /proc/prof\n");
      return -EBUSY;   /* YYY kerr need to find right error to return */
    }
    prof_proc_entry->read_proc = prof_get_info;
  }
#endif	/* CONFIG_PROC_FS */

  /* log device registration */
  printk(PROF_NAME_VERBOSE " version %s initialized, major number %d\n", prof_version(), gMajor);

  /* everything OK */
  return(0);
}

#ifdef	MODULE
int init_module(void)
{
  if (PROF_DEBUG_VERBOSE)
    printk(PROF_NAME ": init_module\n");

  return(profiler_init());
}

void cleanup_module(void)
{
#ifdef	CONFIG_PROC_FS
  /* unregister procfs entry */
  remove_proc_entry(PROF_NAME, &proc_root);
#endif	/* CONFIG_PROC_FS */

  /* unregister driver */
  unregister_chrdev(gMajor, PROF_NAME);

  /* log device unload */
  printk(PROF_NAME_VERBOSE " unloaded\n");
}
#endif	/* MODULE */
