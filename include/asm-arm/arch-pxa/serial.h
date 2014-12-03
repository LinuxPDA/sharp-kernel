/*
 *  linux/include/asm-arm/arch-pxa/serial.h
 * 
 * Author:	Nicolas Pitre
 * Copyright:	(C) 2001 MontaVista Software Inc.
 * 
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * ChangLog:
 *	12-Dec-2002 Lineo Japan, Inc.
 */


#ifndef CONFIG_ARCH_SHARP_SL
#define BAUD_BASE	921600

/* Standard COM flags */
#define STD_COM_FLAGS (ASYNC_SKIP_TEST)

#if defined(CONFIG_SABINAL_DISCOVERY)

#define STD_SERIAL_PORT_DEFNS	\
	{	\
		type:			PORT_PXA,	\
		xmit_fifo_size:		32,		\
		baud_base:		BAUD_BASE,	\
		iomem_base:		(void *)&BTUART,\
		iomem_reg_shift:	2,		\
		io_type:		SERIAL_IO_MEM32,\
		irq:			IRQ_BTUART,	\
		flags:			STD_COM_FLAGS,	\
	}, {	\
		type:			PORT_PXA,	\
		xmit_fifo_size:		32,		\
		baud_base:		BAUD_BASE,	\
		iomem_base:		(void *)&FFUART,\
		iomem_reg_shift:	2,		\
		io_type:		SERIAL_IO_MEM32,\
		irq:			IRQ_FFUART,	\
		flags:			STD_COM_FLAGS,	\
	}, {	\
		type:			PORT_PXA,	\
		xmit_fifo_size:		32,		\
		baud_base:		BAUD_BASE,	\
		iomem_base:		(void *)&STUART,\
		iomem_reg_shift:	2,		\
		io_type:		SERIAL_IO_MEM32,\
		irq:			IRQ_STUART,	\
		flags:			STD_COM_FLAGS,	\
	}

#elif defined(CONFIG_ARCH_PXA_POODLE)

#define STD_SERIAL_PORT_DEFNS	\
	{	\
		type:			PORT_PXA,	\
		xmit_fifo_size:		32,		\
		baud_base:		BAUD_BASE,	\
		iomem_base:		(void *)&FFUART,\
		iomem_reg_shift:	2,		\
		io_type:		SERIAL_IO_MEM32,\
		irq:			IRQ_FFUART,	\
		flags:			STD_COM_FLAGS,	\
	}, {	\
		type:			PORT_PXA,	\
		xmit_fifo_size:		32,		\
		baud_base:		BAUD_BASE,	\
		iomem_base:		(void *)&BTUART,\
		iomem_reg_shift:	2,		\
		io_type:		SERIAL_IO_MEM32,\
		irq:			IRQ_BTUART,	\
		flags:			STD_COM_FLAGS,	\
	}, {	\
		type:			PORT_PXA,	\
		xmit_fifo_size:		32,		\
		baud_base:		BAUD_BASE,	\
		iomem_base:		(void *)&STUART,\
		iomem_reg_shift:	2,		\
		io_type:		SERIAL_IO_MEM32,\
		irq:			IRQ_STUART,	\
		flags:			STD_COM_FLAGS,	\
	}, {	\
		type:			0,		\
		xmit_fifo_size:		0,		\
		baud_base:		BAUD_BASE,	\
		iomem_base:		0,		\
		iomem_reg_shift:	0,		\
		io_type:		0,		\
		irq:			0,		\
		flags:			STD_COM_FLAGS,	\
	}

#else

#define STD_SERIAL_PORT_DEFNS	\
	{	\
		type:			PORT_PXA,	\
		xmit_fifo_size:		32,		\
		baud_base:		BAUD_BASE,	\
		iomem_base:		(void *)&FFUART,\
		iomem_reg_shift:	2,		\
		io_type:		SERIAL_IO_MEM32,\
		irq:			IRQ_FFUART,	\
		flags:			STD_COM_FLAGS,	\
	}, {	\
		type:			PORT_PXA,	\
		xmit_fifo_size:		32,		\
		baud_base:		BAUD_BASE,	\
		iomem_base:		(void *)&BTUART,\
		iomem_reg_shift:	2,		\
		io_type:		SERIAL_IO_MEM32,\
		irq:			IRQ_BTUART,	\
		flags:			STD_COM_FLAGS,	\
	}, {	\
		type:			PORT_PXA,	\
		xmit_fifo_size:		32,		\
		baud_base:		BAUD_BASE,	\
		iomem_base:		(void *)&STUART,\
		iomem_reg_shift:	2,		\
		io_type:		SERIAL_IO_MEM32,\
		irq:			IRQ_STUART,	\
		flags:			STD_COM_FLAGS,	\
	}

#endif

#define RS_TABLE_SIZE 8

#else
#define BASE_BAUD ( 1843200 / 16 )

/* Standard COM flags */
#define STD_COM_FLAGS (ASYNC_BOOT_AUTOCONF | ASYNC_SKIP_TEST)

#define RS_TABLE_SIZE 4


/*
 * Rather empty table...
 * Hardwired serial ports should be defined here.
 * PCMCIA will fill it dynamically.
 */
#define STD_SERIAL_PORT_DEFNS	\
       /* UART	CLK     	PORT		IRQ	FLAGS		*/ \
	{ 0,	BASE_BAUD,	0, 		0,	STD_COM_FLAGS },   \
	{ 0,	BASE_BAUD,	0, 		0,	STD_COM_FLAGS },   \
	{ 0,	BASE_BAUD,	0, 		0,	STD_COM_FLAGS },   \
	{ 0,	BASE_BAUD,	0, 		0,	STD_COM_FLAGS }
#endif

#define EXTRA_SERIAL_PORT_DEFNS

