#ifndef _ASM_ARCH_UCB_H
#define _ASM_ARCH_UCB_H

/* ucb register Address */
#define UCB_IO_DATA     0x0000
#define UCB_IO_DIR      0x0001
#define UCB_RE_IE       0x0002
#define UCB_FE_IE       0x0003
#define UCB_INT_CS      0x0004
#define UCB_TEL_CSA     0x0005
#define UCB_TEL_CSB     0x0006
#define UCB_AUD_CSA     0x0007
#define UCB_AUD_CSB     0x0008
#define UCB_TCH_CR      0x0009
#define UCB_ADC_CR      0x000A
#define UCB_ADC_DATA    0x000B
#define UCB_ID_REG      0x000C
#define UCB_MODE        0x000D
#define UCB_RESERVED	0x000E
#define UCB_NULL	0x000F

/* ucb touch screen cr bits */
#define UCB_TSMX_POW    (0x00000001 << 0)
#define UCB_TSPX_POW    (0x00000001 << 1)
#define UCB_TSMY_POW    (0x00000001 << 2)
#define UCB_TSPY_POW    (0x00000001 << 3)
#define UCB_TSMX_GND    (0x00000001 << 4)
#define UCB_TSPX_GND    (0x00000001 << 5)
#define UCB_TSMY_GND    (0x00000001 << 6)
#define UCB_TSPY_GND    (0x00000001 << 7)
#define UCB_MODE_IE     (0x00000000 << 8)
#define UCB_MODE_PS     (0x00000001 << 8)
#define UCB_MODE_PO     (0x00000003 << 8)
#define UCB_BIAS_ENA    (0x00000001 << 11)
#define UCB_TSPX_LOW    (0x00000001 << 12)
#define UCB_TSMX_LOW    (0x00000001 << 13)

/* ucb adc cr bits */
#define UCB_ADC_SYNC_DIS	0
#define UCB_ADC_SYNC_ENA        (0x00000001 << 0)
#define UCB_VREFBYP_CON         (0x00000001 << 1)
#define UCB_ADC_INPUT(n)        ((n)<<2)
#define UCB_ADC_INPUT_TSPX      UCB_ADC_INPUT(0)
#define UCB_ADC_INPUT_TSMX      UCB_ADC_INPUT(1)
#define UCB_ADC_INPUT_TSPY      UCB_ADC_INPUT(2)
#define UCB_ADC_INPUT_TSMY      UCB_ADC_INPUT(3)
#define UCB_ADC_INPUT_AD0       UCB_ADC_INPUT(4)
#define UCB_ADC_INPUT_AD1       UCB_ADC_INPUT(5)
#define UCB_ADC_INPUT_AD2       UCB_ADC_INPUT(6)
#define UCB_ADC_INPUT_AD3       UCB_ADC_INPUT(7)
#define UCB_EXT_REF_ENA         (0x00000001 << 5)
#define UCB_ADC_START           (0x00000001 << 7)
#define UCB_ADC_ENA             (0x00000001 << 15)

/* ucb adc data */
#define UCB_ADC_DATA_VALUE(n)   (((n)>>5)&0x3ff)
#define UCB_ADC_DAT_VAL         (0x00000001 << 15)

extern __inline__ int ucb_read(int addr)
{
        ulong flags;
        int data;

        /* critical section */
        save_flags_cli(flags);
        {
                IO_MCDR2 = MCDR2_ADDR(addr);
		while(!( IO_MCSR & MCSR_CRC) );
                data = MCDR2_VAL(IO_MCDR2);
        }
        restore_flags(flags);

        return(data);
}

extern __inline__ void ucb_write(int addr, int data)
{
        ulong flags;

        /* critical section */
        save_flags_cli(flags);
        {
                IO_MCDR2 = MCDR2_ADDR(addr) | MCDR2_RW | MCDR2_VAL(data);
		while(!(IO_MCSR & MCSR_CWC) );
        }
        restore_flags(flags);
	udelay(200);
}
	
#endif
