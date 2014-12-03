/*
 * Serial interface GDB stub
 *
 * Written (hacked together) by David Grothe (dave@gcom.com)
 *
 * Modified by Scott Foehner (sfoehner@engr.sgi.com) to allow connect
 * on boot-up
 *
 */

#include <linux/module.h>
#include <linux/errno.h>
#include <linux/signal.h>
#include <linux/sched.h>
#include <linux/timer.h>
#include <linux/interrupt.h>
#include <linux/tty.h>
#include <linux/tty_flip.h>
#include <linux/serial.h>
#include <linux/serial_reg.h>
#include <linux/serialP.h>
#include <linux/config.h>
#include <linux/major.h>
#include <linux/string.h>
#include <linux/fcntl.h>
#include <linux/termios.h>
#include <linux/gdb.h>

#include <asm/system.h>
#include <asm/io.h>
#include <asm/segment.h>
#include <asm/bitops.h>
#include <asm/system.h>
#include <asm/irq.h>
#include <asm/atomic.h>

#include "serial_dbmx1.h"

#define	GDB_BUF_SIZE	512		/* power of 2, please */

static char	gdb_buf[GDB_BUF_SIZE] ;
static int	gdb_buf_in_inx ;
static atomic_t	gdb_buf_in_cnt ;
static int	gdb_buf_out_inx ;

extern void	set_debug_traps(void) ;		/* GDB routine */
extern void	breakpoint(void) ;		/* GDB routine */
extern struct serial_state *	dbmx1_gdb_serial_setup(int ttyS, int baud);
extern void	dbmx1_shutdown_for_gdb(struct async_struct * info) ;
						/* in serial.c */
int gdb_irq   = 0;
unsigned int gdb_port  = 0;
int gdb_ttyS  = 0;	/* Default: ttyS0 */
int gdb_baud  = 115200;
int gdb_enter = 0;	/* Default: do not do gdb_hook on boot */
int gdb_pid   = 0;	/* PID of gdbstart */

int dbmx1_kgdb_in = 0;

/*
 * Get a byte from the hardware data buffer and return it
 */
static int	read_data_bfr(void)
{
    int c = inw(gdb_port + URXDn(0));
    if (c & URXDn_CHARRDY)
	return c & 0xff;

    return( -1 ) ;

} /* read_data_bfr */


/*
 * Get a char if available, return -1 if nothing available.
 * Empty the receive buffer first, then look at the interface hardware.
 */

int	read_char(void)
{
    if (atomic_read(&gdb_buf_in_cnt) != 0)	/* intr routine has q'd chars */
    {
	int		chr ;

	chr = gdb_buf[gdb_buf_out_inx++] ;
	gdb_buf_out_inx &= (GDB_BUF_SIZE - 1) ;
	atomic_dec(&gdb_buf_in_cnt) ;
	return(chr) ;
    }

    return(read_data_bfr()) ;	/* read from hardware */

} /* read_char */

/*
 * Wait until the interface can accept a char, then write it.
 */
void	write_char(int chr)
{
    while (!(inw(gdb_port + USR1) & USR1_TRDY))
	;
    outb(chr, gdb_port + UTXnD(0));

} /* write_char */

/*
 * This is the receiver interrupt routine for the GDB stub.
 * It will receive a limited number of characters of input
 * from the gdb  host machine and save them up in a buffer.
 *
 * When the gdb stub routine getDebugChar() is called it
 * draws characters out of the buffer until it is empty and
 * then reads directly from the serial port.
 *
 * We do not attempt to write chars from the interrupt routine
 * since the stubs do all of that via putDebugChar() which
 * writes one byte after waiting for the interface to become
 * ready.
 *
 * The debug stubs like to run with interrupts disabled since,
 * after all, they run as a consequence of a breakpoint in
 * the kernel.
 *
 * Perhaps someone who knows more about the tty driver than I
 * care to learn can make this work for any low level serial
 * driver.
 */
void gdb_interrupt(int irq, void *dev_id, struct pt_regs * regs)
{
    int			 chr ;
    int			 iir ;

    do
    {
	chr = read_data_bfr() ;
	iir = inw(gdb_port + USR2);
	if (chr < 0) continue ;

        if (chr == 3)                   /* Ctrl-C means remote interrupt */
        {
            breakpoint();
            continue ;
        }

	if (atomic_read(&gdb_buf_in_cnt) >= GDB_BUF_SIZE)
	{				/* buffer overflow, clear it */
	    gdb_buf_in_inx = 0 ;
	    atomic_set(&gdb_buf_in_cnt, 0) ;
	    gdb_buf_out_inx = 0 ;
	    break ;
	}

	gdb_buf[gdb_buf_in_inx++] = chr ;
	gdb_buf_in_inx &= (GDB_BUF_SIZE - 1) ;
	atomic_inc(&gdb_buf_in_cnt) ;
    }
    while (iir & USR2_RDR);

} /* gdb_interrupt */

/*
 * Just a NULL routine for testing.
 */
void gdb_null(void)
{
  /* dbmx1_kgdb_in = 1; */
} /* gdb_null */


int     gdb_hook(void)
{
    int         retval ;
    struct serial_state *ser;

    /*
     * Call first time just to get the ser ptr
     */
    if((ser = dbmx1_gdb_serial_setup(gdb_ttyS, gdb_baud)) == 0) {
        printk ("dbmx1_gdb_serial_setup() error");
        return(-1);
    }

    gdb_port = ser->port;
    gdb_irq = ser->irq;
    gdb_pid = current->pid;
write_char('a');

#if 0
    if (ser->info != NULL)
    {
	dbmx1_shutdown_for_gdb(ser->info) ;
	/*
	 * Call second time to do the setup now that we have
	 * shut down the previous user of the interface.
	 */
	dbmx1_gdb_serial_setup(gdb_ttyS, gdb_baud) ;
    }
#endif

    retval = request_irq(gdb_irq,
                         gdb_interrupt,
                         SA_INTERRUPT,
                         "GDB-stub", NULL);
    if (retval == 0) 
    {
	/*
	 * Now we need to undo some side effects of the earlier shutdown. If we 
	 * don't then the IRQ just installed won't be released when the TIOCGDB
	 * requesting program exits. If it isn't freed then we'll never be able 
	 * to use the port again. We also mark the port as in use for remote
	 * debugging so that we'll know which IRQ handler to release.
	 */
#if 0
	if (ser->info != NULL) {
	    ser->info->flags |= (ASYNC_INITIALIZED | ASYNC_GDB);
	}
#endif
    }
    else
    {
	printk("gdb_hook: request_irq(irq=%d) failed: %d\n", gdb_irq, retval);
    }

    /*
     * Call GDB routine to setup the exception vectors for the debugger
     */

    set_debug_traps() ;

    /*
     * Call the breakpoint() routine in GDB to start the debugging
     * session.
     */
    printk("Waiting for connection from remote gdb... ") ;
    breakpoint() ;
    gdb_null() ;

    printk("Connected.\n");

    return(0) ;

} /* gdb_hook_interrupt2 */

/*
 * getDebugChar
 *
 * This is a GDB stub routine.  It waits for a character from the
 * serial interface and then returns it.  If there is no serial
 * interface connection then it returns a bogus value which will
 * almost certainly cause the system to hang.
 */
int	getDebugChar(void)
{
    volatile int	chr ;

    while ( (chr = read_char()) < 0 ) ;

    return(chr) ;

} /* getDebugChar */

/*
 * putDebugChar
 *
 * This is a GDB stub routine.  It waits until the interface is ready
 * to transmit a char and then sends it.  If there is no serial
 * interface connection then it simply returns to its caller, having
 * pretended to send the char.
 */
void	putDebugChar(int chr)
{
    write_char(chr) ;	/* this routine will wait */

} /* putDebugChar */
