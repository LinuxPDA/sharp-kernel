/*
 * linux/arch/arm/boot/compressed/head-at91rm9200dk.S
 *
 *  Copyright (C) 2002 ATMEL Rousset
 *
 * This is merged into head.S by the linker.
 *
 */
#include <asm/mach-types.h>

		.section	".start", "ax"
		mov	r7,#0xFF
		add	r7,r7,#MACH_TYPE_AT91RM9200DK-0xFF
