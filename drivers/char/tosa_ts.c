/*
 * drivers/char/tosa_ts.c
 *
 * (C) Copyright 2004 Lineo Solutions, Inc.
 *
 * May be copied or modified under the terms of the GNU General Public
 * License.  See linux/COPYING for more information.
 *
 * Based on:
 *   cotulla_ts.c
 *
 */

/***************************************************************************/

#include <linux/config.h>
#include <linux/init.h>
#include <linux/poll.h>
#include <linux/delay.h>
#include <linux/ioctl.h>
#include <linux/ac97_codec.h>
#include <linux/sched.h>
#include <linux/tqueue.h>
#include <linux/module.h>
#include <linux/proc_fs.h>

#include <asm/segment.h>
#include <asm/uaccess.h>
#include <asm/arch/hardware.h>
#include <asm/arch/irqs.h>
#include <asm/arch/tosa_wm9712.h>
#include <asm/sharp_apm.h>

#define TS_MAJOR	11
#define ADD_TS_TIMER	(HZ / 120)
/* AC97 Touch Screen channel */
#define ADCH_TPX	0x01
#define ADCH_TPY	0x02
/* AC97 Touch Screen bits */
#define TSR1_POLL	(1<<15)
#define TSR1_COO	(1<<11)
#define TSR1_CTC	(1<<10)
#define TSR1_SLEN	(1<< 3)
#define TSR2_RPR	(1<<13)
#define TSR2_45W	(1<<12)
#define TSR2_PDEN	(1<<11)
#define TSR2_WAIT	(1<< 9)
#define TSR2_PIL	(1<< 8)
#define CODEC_PENDOWN	(1<< 3)

#define TS_REG1_POLL	0x8000
#define TS_REG1_DEFVAL	0x0000
#define TS_REG2_DEFVAL	0xe030

#define DBG_LEVEL 0

#define DBG_L1		1
#define DBG_L2		2
#define DBG_L3		3

#define DEBUG(level, x, args...) \
	if( level <= DBG_LEVEL ) printk("%s: " x, __FUNCTION__ , ## args)

#define DEV_NAME	"ts"

int ts_type = 0;

static DECLARE_WAIT_QUEUE_HEAD(ts_proc);

static DECLARE_WAIT_QUEUE_HEAD(pd_mloop);

static struct timer_list tp_main_timer;
static unsigned char tp_timer_on = 0;
static unsigned char ac97_on = 0;
static unsigned char penup_irq = 1;

int ac97_ad_input(int, unsigned short *);
static void ts_interrupt(int, void*, struct pt_regs*);

#define BUFSIZE 128

typedef struct {
  unsigned short xd;
  unsigned short yd;
  unsigned char  pressure;
} ts_pos_t;
 
typedef struct {
   short pressure;
   short x;
   short y;
   short millisecs;
} TS_EVENT;

static DECLARE_WAIT_QUEUE_HEAD(queue);
struct fasync_struct *fasync;
static TS_EVENT tbuf[BUFSIZE], tc;
static int raw_max_x, raw_max_y;
static int res_x, res_y;
static int raw_min_x, raw_min_y;
static int cal_ok, x_rev, y_rev, xyswap;
static int head = 0, tail = 0;

#ifdef CONFIG_PM
static int tp_suspend = 0;
#endif	/* CONFIG_PM */

#define REMOVE_TP_NOISE_BY_SW
/* #define REMOVE_TP_NOISE_BY_HW */

#if 1 // 2003/9/4 Noise reduction
#define TOUCH_PANEL_AVERAGE 5
#define DEFAULT_TP_WAIT_VGA	25
#define DEFAULT_TP_WAIT_QVGA	44
#define SIMPLE_AVERAGE

#define CCNT(a)		asm("mrc p14, 0, %0, C1, C0, 0" : "=r"(a))
#define CCNT_ON()	{int pmnc = 1;asm("mcr p14, 0, %0, C0, C0, 0" : : "r"(pmnc));}
#define CCNT_OFF()	{int pmnc = 0;asm("mcr p14, 0, %0, C0, C0, 0" : : "r"(pmnc));}
static unsigned long g_TpWaitVga = DEFAULT_TP_WAIT_VGA;
static unsigned long g_TpWaitQvga = DEFAULT_TP_WAIT_QVGA;
static unsigned short XBuf[TOUCH_PANEL_AVERAGE];
static unsigned short YBuf[TOUCH_PANEL_AVERAGE];
static int TpBufIdx = -1;
extern int tc6393fb_isblank; // no H-Sync while blanking

#define PROC_DIRNAME	"ts"

struct proc_dir_entry *proc_ts = NULL;
typedef struct tosa_ts_entry {
  unsigned long*		addr;
  unsigned long			def_value;
  char*		name;
  char*		description;
  unsigned short	low_ino;
} tosa_ts_entry_t;
static tosa_ts_entry_t tosa_ts_params[] = {
/*  { addr,	    def_value,			name,	    description }*/
  { &g_TpWaitVga,   DEFAULT_TP_WAIT_VGA,	"wait_vga","pen wait for VGA Screen" },
  { &g_TpWaitQvga,  DEFAULT_TP_WAIT_QVGA,	"wait_qvga","pen wait for QVGA Screen"}
};
#define NUM_OF_TOSA_TS_ENTRY	(sizeof(tosa_ts_params)/sizeof(tosa_ts_entry_t))

static ssize_t tosa_ts_read_params(struct file *file, char *buf,
				      size_t nbytes, loff_t *ppos)
{
  int i_ino = (file->f_dentry->d_inode)->i_ino;
  char outputbuf[15];
  int count;
  int i;
  tosa_ts_entry_t	*current_param = NULL;
  if (*ppos>0) /* Assume reading completed in previous read*/
    return 0;
  for (i=0; i<NUM_OF_TOSA_TS_ENTRY; i++) {
    if (tosa_ts_params[i].low_ino==i_ino) {
      current_param = &tosa_ts_params[i];
      break;
    }
  }
  if (current_param==NULL) {
    return -EINVAL;
  }
  count = sprintf(outputbuf, "%d\n",
		  *((volatile unsigned long *) current_param->addr));
  *ppos += count;
  if (count>nbytes)	/* Assume output can be read at one time */
    return -EINVAL;
  if (copy_to_user(buf, outputbuf, count))
    return -EFAULT;
  return count;
}

static ssize_t tosa_ts_write_params(struct file *file, const char *buf,
				       size_t nbytes, loff_t *ppos)
{
  int			i_ino = (file->f_dentry->d_inode)->i_ino;
  tosa_ts_entry_t	*current_param = NULL;
  int			i;
  unsigned long		param;
  char			*endp;

  for (i=0; i<NUM_OF_TOSA_TS_ENTRY; i++) {
    if(tosa_ts_params[i].low_ino==i_ino) {
      current_param = &tosa_ts_params[i];
      break;
    }
  }
  if (current_param==NULL) {
    return -EINVAL;
  }

  param = simple_strtoul(buf,&endp,0);
  if (param == -1) {
    *current_param->addr = current_param->def_value;
  } else {
    *current_param->addr = param;
  }
  return nbytes+endp-buf;
}

static struct file_operations proc_params_operations = {
	read:	tosa_ts_read_params,
	write:	tosa_ts_write_params,
};

static int init_procinfo(void)
{
  int	i;
  struct proc_dir_entry *entry;
  
  proc_ts = proc_mkdir("driver/ts", NULL);
  if (proc_ts == NULL) {
    printk(KERN_ERR "ts: can't create /proc/driver/ts\n");
    return -ENOMEM;
  }
  for (i=0; i<NUM_OF_TOSA_TS_ENTRY; i++) {
    entry = create_proc_entry(tosa_ts_params[i].name,
			      S_IWUSR |S_IRUSR | S_IRGRP | S_IROTH,
			      proc_ts);
    if (entry) {
      tosa_ts_params[i].low_ino = entry->low_ino;
      entry->proc_fops = &proc_params_operations;
    } else {
      int	j;
      for (j=0; j<i; j++) {
	remove_proc_entry(tosa_ts_params[i].name,
			  proc_ts);
      }
      remove_proc_entry("driver/ts", &proc_root);
      proc_ts = 0;
      return -ENOMEM;
    }
  }

  return 0;
}

static void cleanup_procinfo(void)
{
  int	i;
  for (i=0; i<NUM_OF_TOSA_TS_ENTRY; i++) {
    remove_proc_entry(tosa_ts_params[i].name, proc_ts);
  }
  remove_proc_entry("driver/ts", NULL);
  proc_ts = 0;
}
#endif

static int read_xydata(ts_pos_t *tp)
{
#if defined(REMOVE_TP_NOISE_BY_SW) || defined(REMOVE_TP_NOISE_BY_HW)
  unsigned short dx, dy;
  unsigned long tx, ty;
  int i, ret = 0, penstate = 0;

  if (TpBufIdx<0) {
    for (i=0; i<TOUCH_PANEL_AVERAGE; i++) {
      if( (ret = ac97_ad_input(ADCH_TPX, &dx)) < 0 ) return ret;
      XBuf[i] = dx & 0x0fff;
      if( (ret = ac97_ad_input(ADCH_TPY, &dy)) < 0 ) return ret;
      YBuf[i] = dy & 0x0fff;
    }
    TpBufIdx = 0;
  } else {
    if( (ret = ac97_ad_input(ADCH_TPX, &dx)) < 0 ) return ret;
    XBuf[TpBufIdx] = dx & 0x0fff;
    if( (ret = ac97_ad_input(ADCH_TPY, &dy)) < 0 ) return ret;
    YBuf[TpBufIdx] = dy & 0x0fff;
    if (++TpBufIdx>=TOUCH_PANEL_AVERAGE) TpBufIdx = 0;
  }

  /* If pen Up or Down */
  penstate = (dy & 0x8000)?1:0;

#if TOUCH_PANEL_AVERAGE<3
#define SIMPLE_AVERAGE
#endif
  {
#ifndef SIMPLE_AVERAGE
    unsigned short max_dx=0,max_dy=0;
    unsigned short min_dx=0x0fff,min_dy=0x0fff;
#endif

    for(i = 0, ty = 0, tx = 0; i < (TOUCH_PANEL_AVERAGE); i++) {
      dx = XBuf[i];
      dy = YBuf[i];
#ifndef SIMPLE_AVERAGE
      if(dx > max_dx) max_dx = dx;
      if(dx < min_dx) min_dx = dx;
      if(dy > max_dy) max_dy = dy;
      if(dy < min_dy) min_dy = dy;
#endif
      tx += dx;
      ty += dy;
    }
#ifndef SIMPLE_AVERAGE
    tp->xd = (tx - max_dx - min_dx)/ (TOUCH_PANEL_AVERAGE-2);
    tp->yd = (ty - max_dy - min_dy)/ (TOUCH_PANEL_AVERAGE-2);
#else
    tp->xd = tx/ TOUCH_PANEL_AVERAGE;
    tp->yd = ty/ TOUCH_PANEL_AVERAGE;
#endif
  }
#else
  unsigned short dx, dy, tx, ty;
  int i, ret = 0;

  for(i = 0, ty = 0, tx = 0; i < TOUCH_PANEL_AVERAGE; i++) {
    /* X */
    if( (ret = ac97_ad_input(ADCH_TPX, &dx)) < 0 ) return ret;
    /* Y */
    if( (ret = ac97_ad_input(ADCH_TPY, &dy)) < 0 ) return ret;

    tx += (dx & 0x0fff);
    ty += (dy & 0x0fff);
  }
  
  /* If pen Up or Down */
  penstate = (dy & 0x8000)?1:0;

  tp->xd = tx / TOUCH_PANEL_AVERAGE;
  tp->yd = ty / TOUCH_PANEL_AVERAGE;
#endif
  return penstate;
}

static void print_par(void)
{
  printk(" Kernel ==> cal_ok    = %d\n",cal_ok);
  printk(" Kernel ==> raw_max_x = %d\n",raw_max_x);
  printk(" Kernel ==> raw_max_y = %d\n",raw_max_y);
  printk(" Kernel ==> res_x     = %d\n",res_x);
  printk(" Kernel ==> res_y     = %d\n",res_y);
  printk(" Kernel ==> raw_min_x = %d\n",raw_min_x);
  printk(" Kernel ==> raw_min_y = %d\n",raw_min_y);
  printk(" Kernel ==> xyswap    = %d\n",xyswap);
  printk(" Kernel ==> x_rev     = %d\n",x_rev);
  printk(" Kernel ==> y_rev     = %d\n",y_rev);
}

void ts_clear(void)
{
  int i;
	
  for (i = 0; i < BUFSIZE; i++) {
    tbuf[i].pressure = 0;
    tbuf[i].x = 0;
    tbuf[i].y = 0;
    tbuf[i].millisecs = 0;
  }
  head = tail =  0;
}

static TS_EVENT get_data(void)
{
   int last = tail;
   if (++tail == BUFSIZE) { tail = 0; }
   return tbuf[last];
}

static void new_data(void)
{
  tc.millisecs = jiffies;
  tbuf[head++] = tc;
  if( head >= BUFSIZE )
    head = 0;
  if( head == tail && ++tail >= BUFSIZE )
    tail = 0;
  if( fasync )
    kill_fasync(&fasync, SIGIO, POLL_IN);
  wake_up_interruptible(&queue);
}

static void ts_timer(unsigned long irq);

static void ts_timer_clear(void)
{
  if ( tp_timer_on )
    del_timer(&tp_main_timer);
  tp_timer_on = 0;
}

static void ts_timer_set(void)
{
  ts_timer_clear();
  init_timer(&tp_main_timer);
  tp_main_timer.data = IRQ_GPIO_TP_INT;
  tp_main_timer.function = ts_timer;
  tp_main_timer.expires = jiffies + ADD_TS_TIMER;
  add_timer(&tp_main_timer);
  tp_timer_on = 1;
}

static void ts_timer(unsigned long irq)
{
  wake_up(&ts_proc);
}

static int ts_pendown(void *unuse)
{
	int penstate;
	ts_pos_t pos_dt;

	while ( 1 ) {
		interruptible_sleep_on(&pd_mloop);

#ifdef CONFIG_PM
		lock_FCS(POWER_MODE_TOUCH, 1);	// not enter FCS mode.
#endif /* CONFIG_PM */
		wm9712_power_mode_ts(WM9712_PWR_TP_CONV);
		while ( 1 ) {
#ifdef CONFIG_PM
		    if ( !tp_suspend )
#endif	/* CONFIG_PM */
			{
				if ( (penstate = read_xydata(&pos_dt)) >= 0 ) {
					if ( !penstate ) 	//penup
						penup_irq = 1;
					tc.x  = pos_dt.xd;
					tc.y  = pos_dt.yd;
					tc.pressure = 1;
					new_data();
					DEBUG(DBG_L3, "pen position (%d,%d,%d)\n",
					(int)tc.y, (int)tc.x, tc.pressure);
				}
      						/* wait */
				if ( !penup_irq ) {
					ts_timer_set();
					interruptible_sleep_on(&ts_proc);
				}
				if ( penup_irq ){
					ts_timer_clear();
    						/* setting interrupt edge */
					set_GPIO_IRQ_edge(GPIO_TP_INT, GPIO_RISING_EDGE);
					GEDR(GPIO_TP_INT) = GPIO_bit(GPIO_TP_INT);	//Clear detect
#ifdef CONFIG_PM
					lock_FCS(POWER_MODE_TOUCH, 0);	// not enter FCS mode.
#endif /* CONFIG_PM */
					wm9712_power_mode_ts(WM9712_PWR_TP_WAIT);
					TpBufIdx = -1;
					if ( tc.pressure ){
						tc.pressure = 0;
						new_data();
					}
					break;
				}
			}
#ifdef CONFIG_PM
			else {
				DEBUG(DBG_L1, "suspend...\n");
#ifdef CONFIG_PM
					lock_FCS(POWER_MODE_TOUCH, 0);	// enter FCS mode.
#endif /* CONFIG_PM */
				break;
			}
#endif
		}
	}
	
}

static void ts_interrupt(int irq, void *dev_id, struct pt_regs *regs)
{
  if( (GPLR(GPIO_TP_INT) & GPIO_bit(GPIO_TP_INT)) == GPIO_bit(GPIO_TP_INT) ) {
    /* pen down irq */
    penup_irq = 0;
    
    /* setting interrupt edge */
    set_GPIO_IRQ_edge(GPIO_TP_INT, GPIO_FALLING_EDGE);
    GEDR(GPIO_TP_INT) = GPIO_bit(GPIO_TP_INT); 	//Clear detect
	wake_up(&pd_mloop);
  } else {
    /* setting interrupt edge */
    set_GPIO_IRQ_edge(GPIO_TP_INT, GPIO_RISING_EDGE);
    GEDR(GPIO_TP_INT) = GPIO_bit(GPIO_TP_INT);	//Clear detect
    /* pen up irq */
    penup_irq = 1;
    wake_up(&ts_proc);
  }
  return;
}

static int ts_open(struct inode *, struct file *);
static int ts_release(struct inode *, struct file *);
static int ts_fasync(int fd, struct file *filp, int on);
static ssize_t ts_read(struct file *, char *, size_t, loff_t *);
static unsigned int ts_poll(struct file *, poll_table *);
static int ts_ioctl(struct inode *, struct file *, unsigned int, unsigned long);
struct file_operations ts_fops = {
	open:		ts_open,
	release:	ts_release,
	fasync:		ts_fasync,
	read:		ts_read,
	poll:		ts_poll,
	ioctl:		ts_ioctl,
};

static void ts_exit(void)
{
  ts_timer_clear();
  ts_clear();
  free_irq(IRQ_GPIO_TP_INT, NULL);
  wm9712_power_mode_ts(WM9712_PWR_OFF);
  
  pxa_ac97_put(&ac97_on);
}

#ifdef MODULE
static void __exit ac97_ts_cleanup(void)
{
  ts_exit();
  unregister_chrdev(TS_MAJOR, "ts");
}
module_exit(ac97_ts_cleanup);
#endif	/* MODULE */

extern int tc6393fb_lcdMode;

int ac97_ad_input(int ch, unsigned short *dat)
{
  unsigned short val = 0;
  unsigned short adsel = (ch << 12) & 0x7000;
  unsigned long timeo;
  volatile unsigned long t1=0,t2=0;
  volatile int isVga=(tc6393fb_lcdMode==1)?1:0;
  volatile unsigned long tpwait=200;


  if (ch >= 4){
  	/* Conversion start */
    ac97_write(AC97_TS_REG1, (adsel | TS_REG1_DEFVAL|TS_REG1_POLL));

    for(timeo = 0x10000; timeo > 0; timeo--) {
      val = ac97_read(AC97_TS_REG1);
      //udelay(100);
      if( val & TS_REG1_POLL ) continue;
      val = ac97_read(AC97_TS_READBACK);
      if( (val & 0x7000) == adsel ) break;
    }
    if( !timeo ) return -EBUSY;
  
    // *dat = val & 0xfff;
    *dat = val;
    return 0;
  }


#if defined(REMOVE_TP_NOISE_BY_SW) || defined(REMOVE_TP_NOISE_BY_HW)

#ifndef REMOVE_TP_NOISE_BY_HW
  tpwait *= (isVga)?g_TpWaitVga:g_TpWaitQvga;
  CCNT_ON();
  if (!tc6393fb_isblank) {
    /* Sync */
    while((GPLR(GPIO_VGA_LINE) & GPIO_bit(GPIO_VGA_LINE)) == 0)
      ;
    while((GPLR(GPIO_VGA_LINE) & GPIO_bit(GPIO_VGA_LINE)) != 0)
      ;
  }
  if (tpwait>0) {
    CCNT(t1);
    CCNT(t2);
    while((t2-t1)<tpwait) {
      CCNT(t2);
    }
  }
#endif

  /* Conversion start */
  ac97_write(AC97_TS_REG1, (adsel | TS_REG1_DEFVAL|TS_REG1_POLL));
  
#ifndef REMOVE_TP_NOISE_BY_HW
  CCNT_OFF();
#endif

  /* Busy wait */
  for(timeo = 0x10000; timeo > 0; timeo--) {
      val = ac97_read(AC97_TS_REG1);
      if((val & TS_REG1_POLL) == 0) break;
  }
  if( !timeo ) return -EBUSY;

  /* Read AD */
  val = ac97_read(AC97_TS_READBACK);

  if( (val & 0x8000) == 0) return -EBUSY;

  if( (val & 0x7000) == adsel ) {
      *dat = val;
      return 0;
  }
  return -EBUSY;
#else
  /* Conversion start */
  ac97_write(AC97_TS_REG1, (adsel | TS_REG1_DEFVAL|TS_REG1_POLL));

  for(timeo = 0x10000; timeo > 0; timeo--) {
    val = ac97_read(AC97_TS_REG1);
    //udelay(100);
    if( val & TS_REG1_POLL ) continue;
    val = ac97_read(AC97_TS_READBACK);
    if( (val & 0x7000) == adsel ) break;
  }
  if( !timeo ) return -EBUSY;
  
  *dat = val;
  return 0;
#endif
}

static int ts_init(void)
{
  pxa_ac97_get(&codec, &ac97_on);

  wm9712_power_mode_ts(WM9712_PWR_TP_WAIT);
  /* Touch Screen Initialize */
#ifdef REMOVE_TP_NOISE_BY_HW
  ac97_write(AC97_TS_REG2, 0xe0f0);
#else
  ac97_write(AC97_TS_REG2, TS_REG2_DEFVAL);
#endif
  ac97_write(AC97_TS_REG1, TS_REG1_DEFVAL);
  ac97_bit_set(AC97_ADITFUNC1, (1<<1));
  
  /* GPIO3/PENDOWN wakeup */
  ac97_bit_set(AC97_GPIO_WAKE_UP, CODEC_PENDOWN);

  ts_clear();

  /* Init queue */
 //X	kernel_thread(ts_pendown, NULL, CLONE_FS | CLONE_FILES | CLONE_SIGHAND | SIGCHLD);

  /* Interrupt setting */
  set_GPIO_mode(GPIO32_SDATA_IN1_AC97_MD);
  set_GPIO_IRQ_edge(GPIO_TP_INT, GPIO_RISING_EDGE);
  GEDR(GPIO_TP_INT) = GPIO_bit(GPIO_TP_INT);
  if( request_irq(IRQ_GPIO_TP_INT, ts_interrupt, SA_INTERRUPT, "ts", NULL) ) {
    DEBUG(DBG_L1, "IRQ error %d\n", IRQ_GPIO_TP_INT);
    return -EBUSY;
  }

  //  printk(KERN_INFO "Tosa Touch Screen driver initialized\n");

  return 0;
}

static int __init ac97_ts_init(void)
{
  ac97_on = 0;

  raw_max_x = 1024;
  raw_max_y = 1024;
  raw_min_x = 0;
  raw_min_y = 0;
  res_x = 480;
  res_y = 640;
  cal_ok = 0;
  x_rev = 0;
  y_rev = 0;
  xyswap = 0;

  ts_clear();
  
  if( register_chrdev(TS_MAJOR,DEV_NAME, &ts_fops) ) {
    printk("unable to get major %d for touch screen\n", TS_MAJOR);
    ts_exit();
  }

  init_procinfo();

  kernel_thread(ts_pendown, NULL, CLONE_FS | CLONE_FILES | CLONE_SIGHAND | SIGCHLD);

  return 0;
}

module_init(ac97_ts_init);

/*
 * Driver functions
 */
static int ts_open(struct inode *inode, struct file *file)
{
  if( ts_init() < 0 ) {
    ts_exit();
    return -EINVAL;
  }

  MOD_INC_USE_COUNT;
  return 0;
}


static int ts_release(struct inode *inode, struct file *file)
{
  ts_exit();

  MOD_DEC_USE_COUNT;
  return 0;
}

static int ts_fasync(int fd, struct file *filp, int on)
{
  int retval;

  retval = fasync_helper(fd, filp, on, &fasync);
  if( retval < 0 ) return retval;
  return 0;
}

static ssize_t ts_read(struct file *file, char *buffer,
		size_t count, loff_t *ppos)
{
  DECLARE_WAITQUEUE(wait, current);
  TS_EVENT t, r;
  int i;
	
  if( head == tail ) {
    if( file->f_flags & O_NONBLOCK )
      return -EAGAIN;
    add_wait_queue(&queue, &wait);
    current->state = TASK_INTERRUPTIBLE;
    while( (head == tail)&& !signal_pending(current) ) {
      schedule();
      current->state = TASK_INTERRUPTIBLE;
    }
    current->state = TASK_RUNNING;
    remove_wait_queue(&queue, &wait);
  }
	
  for(i = count ; i >= sizeof(TS_EVENT);
      i -= sizeof(TS_EVENT), buffer += sizeof(TS_EVENT)) {
    if( head == tail ) break;
    t = get_data();
    if( xyswap ) {
      short tmp = t.x;
      t.x = t.y;
      t.y = tmp;
    }
    if( cal_ok ) {
      r.x = (x_rev) ?
	((raw_max_x - t.x) * res_x) / (raw_max_x - raw_min_x) :
	((t.x - raw_min_x) * res_x) / (raw_max_x - raw_min_x);
      r.y = (y_rev) ?
	((raw_max_y - t.y) * res_y) / (raw_max_y - raw_min_y) :
	((t.y - raw_min_y) * res_y) / (raw_max_y - raw_min_y);
    } else {
      r.x = t.x;
      r.y = t.y;
    }

    r.pressure = t.pressure;
    r.millisecs = t.millisecs;
    DEBUG(DBG_L2, "ts_read(x, y, p) = (%d, %d, %d)\n",
	(unsigned int)r.x, (unsigned int)r.y, (unsigned int)r.pressure);
    
    copy_to_user(buffer,&r,sizeof(TS_EVENT));
  }
  return count - i;
}

static unsigned int ts_poll(struct file *filp, poll_table *wait)
{
  poll_wait(filp, &queue, wait);
  if( head != tail )
    return POLLIN | POLLRDNORM;
  return 0;
}

static int ts_ioctl(struct inode *inode, struct file *file,
		unsigned int cmd, unsigned long arg)
{
  switch(cmd) {
  case 3:  raw_max_x = arg;	break;
  case 4:  raw_max_y = arg;	break;
  case 5:  res_x = arg;		break;
  case 6:  res_y = arg;		break;
  case 10: raw_min_x = arg;	break;
  case 11: raw_min_y = arg;	break;
  case 12: xyswap = arg;	//New attribute for portrait modes
  case 13: cal_ok = arg;	//0 = Enable calibration ; 1 = Calibration OK
  case 14: ts_clear();		break;	//Clear all buffer data
  case 15: x_rev = arg;		break;	//X axis reversed setting
  case 16: y_rev = arg;		break;	//Y axis reversed setting
  case 17: print_par();		break;
  default: return -EINVAL;
  }

  return 0;
}

#ifdef CONFIG_PM
void tosa_ts_suspend(void)
{
  DEBUG(DBG_L1, "in\n");
  tp_suspend = 1;
  ts_exit();
  DEBUG(DBG_L1, "out\n");
}

void tosa_ts_resume(void)
{
  DEBUG(DBG_L1, "in\n");
  tp_suspend = 0;
  ts_init();
  DEBUG(DBG_L1, "out\n");
}
#endif	/* CONFIG_PM */
