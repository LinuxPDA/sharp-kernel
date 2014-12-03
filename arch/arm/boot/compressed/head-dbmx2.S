#include <asm/mach-types.h>

		.section	".start", "ax"
#ifdef CONFIG_ARCH_MX2ADS
		mov	r7, #0xFF
		add	r7, r7, #MACH_TYPE_MX2ADS-0xFF
#endif
