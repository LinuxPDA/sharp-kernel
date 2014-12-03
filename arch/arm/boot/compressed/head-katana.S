#include <asm/mach-types.h>

		.section	".start", "ax"
		mov	r7, #0xFF
		add	r7, r7, #MACH_TYPE_KATANA-0xFF
