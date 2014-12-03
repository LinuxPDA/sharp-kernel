#include <stdarg.h>

#define UART3_THR (long *)0x40700000
#define UART3_DLL (long *)0x40700000
#define UART3_IER (long *)0x40700004
#define UART3_FCR (long *)0x40700008
#define UART3_LCR (long *)0x4070000c
#define UART3_LSR (long *)0x40700014
#define UART3_MSR (long *)0x40700018
#define UART3_SCR (long *)0x4070001c

#define CKEN    (unsigned long*)0x41300004
#define GPDR1   (unsigned long*)0x40e00010
#define GPFR1_L (unsigned long*)0x40e0005c

#define BPS_9600   0x60 /* 14745600 / (16 * 9600) */
#define BPS_115200 0x08 /* 14745600 / (16 * 115200) */

void start_UART(void);
void icache_enable(void);
void dcache_enable(void);
void init_UART(void);
void putc(char);
int writestring(char *, int);

void start_UART(void)
{
    /* clock enable */
    *CKEN = *CKEN | 0x00000020;
    /* pin direction */
    *GPDR1 = *GPDR1 | 0x00008000;
    /* alternate function select */
    *GPFR1_L = (*GPFR1_L & 0x0fffffff) | 0x60000000;
    
    init_UART();
    icache_enable();
    putc('S');
    dcache_enable();
    //writestring("hello!\n", 7);
}

void icache_enable(void)
{
  __asm("mrc p15, 0, r0, c1, c0, 0 \n \
         orr r0, r0, #0x1000 \n \
         mcr p15, 0, r0, c1, c0, 0");
}

void dcache_enable(void)
{
  __asm("mrc p15, 0, r0, c7, c10, 4 \n \
         mrc p15, 0, r0, c1, c0, 0 \n \
         orr r0, r0, #0x4 \n \
         mcr p15, 0, r0, c1, c0, 0");
}

void init_UART(void)
{
	volatile unsigned long *uart_scr = UART3_SCR;
	volatile unsigned long *uart_fcr = UART3_FCR;
	volatile unsigned long *uart_msr = UART3_MSR;
	volatile unsigned long *uart_dll = UART3_DLL;
	volatile unsigned long *uart_ier = UART3_IER;
	volatile unsigned long *uart_lcr = UART3_LCR;

	*uart_scr = 0x00;
	*uart_fcr = 0x07; /* FIFO reset */
	*uart_msr = 0x00;

	*uart_lcr |= 0x83; /* DLAB ON Parity none, stop bit 1, 8bit char */
	*uart_dll = BPS_9600;
	//*uart_dll = BPS_115200;
	*uart_lcr &= 0x7f; /* DLAB off */
	*uart_ier |= 0x40; /* UART unit enable */
	*uart_fcr = 0xc9; /* FIFO enable, DMA mode 1, Triger level 14 bytes */
}

void putc(char c)
{
    volatile unsigned long *uart_lsr  = UART3_LSR;
    volatile unsigned long *uart_xmit = UART3_THR;

    while ((*uart_lsr & 0x40) == 0)
       ;
    *uart_xmit = c;
}

int writestring(char *ptr, int nb)
{
    int i;
    volatile unsigned long *uart_lsr  = UART3_LSR;
    volatile unsigned long *uart_xmit = UART3_THR;

    for (i = 0; i < nb; ++i) {


        /* wait for transmit reg (possibly fifo) to empty) */
        while ((*uart_lsr & 0x40) == 0)
            ;

        *uart_xmit = (ptr[i] & 0xff);


        if (ptr[i] == '\n') {

            /* add a carriage return */

            /* wait for transmit reg (possibly fifo) to empty) */
            while ((*uart_lsr & 0x40) == 0)
                ;

            *uart_xmit = '\r';
        }
    }

    return nb;
}
