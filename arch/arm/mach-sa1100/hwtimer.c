/*
 * Code to multiplex the SA1100 Timer 
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
 * Authors: Deborah A. Wallach and Lawrence S. Brakmo
 *
 */

#include <linux/init.h>
#include <linux/module.h>
#include <linux/sched.h>
#include <asm/hardware/hwtimer.h>

#include <asm/io.h>
#include <linux/config.h>

#include <asm/arch/irqs.h>

#include <net/tcp.h>

#ifdef CONFIG_POWERMGR
#include <linux/powermgr.h>
#endif

/*************************************************************************/

static int timer2_in_use = 0;

static __inline__ void clear_timer2 (void)
{
  OSSR = 0x4;
}

static  __inline__ void set_timer2 (unsigned long when)
{
  unsigned long now;

  OSMR2 = when;
  if (!timer2_in_use) {
    unsigned long i = OIER;
    i |= 4;
    OIER = i;
    OSSR = 0x4;
    timer2_in_use = 1;
  }
  if ((now=OSCR) > when  &&  (now-when) < 36864000) { /* LSB */
    printk("set_timer2, when: 0x%lx, now: 0x%lx\n", when, now);
    OSMR2 = now+2000;
  }
}

extern __inline__ void cleanup_timer2 (void)
{
  unsigned long i;
  i = OIER;
  i &= ~0x4;
  OIER = i;
  timer2_in_use = 0;
  OSSR = 0x4;
}

/*************************************************************************/

volatile struct hwtimer_list *head = NULL;
volatile int inside_timer2_interrupts=0;

static inline int islt(unsigned long a, unsigned long b)
{
  return(before(a, b));
}

static inline int isle(unsigned long a, unsigned long b)
{
  if (a==b) return(1);
  else return(islt(a, b));
}

static inline void internal_add_hwtimer(struct hwtimer_list *timer)
{
  if ((!head) || islt(timer->expires, head->expires)) {
    timer->next = (struct hwtimer_list *)head;
    head = timer;
    if (!inside_timer2_interrupts)
      set_timer2(timer->expires);
  }
  else {
    struct hwtimer_list *ptr1 = (struct hwtimer_list *)head;
    struct hwtimer_list *ptr2 = (struct hwtimer_list *)head->next;
    while ((ptr2 != NULL) && islt(ptr2->expires, timer->expires)) {
      ptr1 = ptr2;
      ptr2 = ptr2->next;
    }
    ptr1->next = timer;
    timer->next = ptr2;
  }
}

void add_hwtimer(struct hwtimer_list *timer, int delay) 
{
  unsigned long flags;
  unsigned long now;

  MOD_INC_USE_COUNT;

  /*   printk("add_hwtimer OSCR:0x%x delay:%d data:%d inUse:%d head:%p\n",  */
  /* 	 (int) OSCR, */
  /* 	 delay, (int) timer->data, timer2_in_use, head); */
  save_flags_cli(flags);
  now = OSCR;
  timer->expires = now + delay;
  internal_add_hwtimer(timer);
  restore_flags(flags);
  /*   printk("set_timer2 OSMR2:0x%x OSCR: 0x%x OIER:%x OSSR:0x%x\n",  */
  /* 	 (int) OSMR2, (int) OSCR, */
  /* 	 (int) OIER, (int) OSSR); */
}

static inline int detach_hwtimer(struct hwtimer_list *timer)
{
  struct hwtimer_list *ptr = (struct hwtimer_list *)head;
  if (!head) return(0);
  if (head == timer) {
    head = timer->next;
    if (head == NULL) cleanup_timer2();
    else if (timer->expires != head->expires) {
      set_timer2(head->expires);
    }
    return(1);
  }
  while((ptr != NULL) && (ptr->next != timer)) {
    ptr = ptr->next;
  }
  if (ptr==NULL) return(0);
  ptr->next = timer->next;
  return(1);
}

int del_hwtimer(struct hwtimer_list * timer)
{
	int ret;
	unsigned long flags;
	/* 	printk("del_hwtimer data:%d\n", (int) timer->data); */
	/* 	printk(" OSMR2:0x%x OSCR: 0x%x OIER:0x%x OSSR:0x%x ICMR:0x%x\n",  */
	/* 	 (int) OSMR2, (int) OSCR, */
	/* 	 (int) OIER, (int) OSSR, (int) *IRQ_ICMR); */
	save_flags(flags);
	cli();
	ret = detach_hwtimer(timer);
	timer->next = 0;
	restore_flags(flags);
	if (ret)
		MOD_DEC_USE_COUNT;
	return ret;
}

static void timer2_interrupt(int irq, void *dev_id, struct pt_regs *regs)
{
  unsigned long now;
  unsigned long flags;
  struct hwtimer_list *ptr;

  save_flags_cli(flags);
  inside_timer2_interrupts = 1;
  now = OSCR;
  clear_timer2();
  while((head != NULL) && isle(head->expires, now)) {  
    void (*fn)(int, void *, struct pt_regs *, unsigned long) = head->function;
    unsigned long data = head->data;
    ptr = (struct hwtimer_list *)head;
    head = head->next;
    ptr->next = NULL;
    /*     sti(); */
    /*     restore_flags(flags); */
    fn(irq, dev_id, regs, data);
    /*     printk("calling timer2 with data: %d\n", (int) data); */
    cli();
    now = OSCR;  
  }
  inside_timer2_interrupts = 0;
  if (head != NULL) {
    set_timer2(head->expires);
  }
  else {
    cleanup_timer2();
  }
  restore_flags(flags);
}

unsigned int us_check_hwtimer(void)
{
  unsigned long now;
  if (!head) return(0xffffffff);
  else { 
    now = OSCR;  
    return(head->expires-now);
  }
}

#ifdef ITSY_CONFIG_POWERMGR
/* This code is only works with the Itsy powermanagement scheme */

static unsigned long OSMR2_saved, OIER_saved;

extern unsigned long ticks_slept; /* defined in rtc.c */

static int
suspend (void *unused, int suspend_flags)
{
  OSMR2_saved = OSMR2;
  OIER_saved = OIER;
  return 1;
}

static int
resume (void *unused, int resume_flags)
{
  struct hwtimer_list *ptr = head;
  unsigned long now = OSCR;  

  while (ptr != NULL) { 
    if (ticks_slept == 0xffffffff) ptr->expires = now; 
    else { 
      ptr->expires -= ticks_slept; 
      if (islt(ptr->expires, now)) ptr->expires = now; 
    } 
    ptr = ptr->next;
  }
  if (head != NULL) OSMR2 = head->expires+5000;
  else OSMR2 =  OSMR2_saved; /* XXX this else clause is necessary to prevent triggering
				    * a compiler bug on the version of the compiler on the alphas
				    * egcs-2.91.55 19980824 (gcc2 ss-980609 experimental)
				    */

  if (OIER_saved & 0x4) {
    unsigned long i = OIER;
    i |= 4;
    OIER = i;
  }
  *IRQ_ICMR |= (1 << IRQ_OST2);
  /*   printk("hwtimer resume OIER:0x%x, OSSR:0x%x, ICMR:0x%x\n", */
  /* 	 (int) OIER, (int) OSSR, (int) *IRQ_ICMR); */
  /*   { */
  /*     unsigned long i = IRQ_ICMR; */
  /*     i |= IRQ_OST2; */
  /*     IRQ_ICMR = i; */
  /*   } */
  return 1;
}

static const powermgr_client hwtimer_powermgr = {
  /* callback functions */
  NULL,
  suspend,
  resume,
  NULL,
  POWERMGR_ID_INVALID,
  "hwtimer",
  0,
0
};

#endif /* ITSY_CONFIG_POWERMGR */
    
void init_hwtimer(struct hwtimer_list *timer)
{
  timer->next = NULL;
}


int __init hwtime_setup(void)
{
  int ret;
  
  ret = request_irq(IRQ_OST2, timer2_interrupt, SA_INTERRUPT, "OST2 - gp", NULL);
  if (ret) {
    printk("IRQ initialization failed!\n");
    return ret;
  }
  return 0;
}

module_init(hwtime_setup);

void __exit hwtime_cleanup(void)
{
  free_irq(IRQ_OST2, NULL);
}

module_exit(hwtime_cleanup);


EXPORT_SYMBOL(init_hwtimer);
EXPORT_SYMBOL(add_hwtimer);
EXPORT_SYMBOL(del_hwtimer);

