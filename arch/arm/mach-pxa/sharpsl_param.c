/*
 * arch/arm/mach-pxa/sharpsl_param.c
 *
 *  paramater func for SL (SHARP)
 *
 * Copyright (C) 2002  SHARP
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * ChangeLog:
 * 
 */
#include <linux/types.h>

#if defined(CONFIG_ARCH_PXA_POODLE) || defined(CONFIG_ARCH_PXA_CORGI)
#include <asm/arch/sharpsl_param.h>
#endif


sharpsl_flash_param_info sharpsl_flash_param;


#if defined(CONFIG_ARCH_PXA_POODLE) || defined(CONFIG_ARCH_PXA_CORGI)
int sharpsl_get_comadj()
{
  if ( sharpsl_flash_param.comadj_keyword == FLASH_COMADJ_MAJIC ) {
    return sharpsl_flash_param.comadj;
  } else {
    return -1;
  }
}
#endif

#if defined(CONFIG_ARCH_PXA_CORGI)
int sharpsl_get_phadadj()
{
  if ( sharpsl_flash_param.phad_keyword == FLASH_PHAD_MAJIC ) {
    return sharpsl_flash_param.phadadj;
  } else {
    return -1;
  }
}
#endif


#if defined(CONFIG_ARCH_PXA_POODLE) || defined(CONFIG_ARCH_PXA_CORGI)
void sharpsl_get_param(void)
{
   // get comadj
	sharpsl_flash_param.comadj_keyword = (unsigned int) FLASH_DATA(FLASH_COMADJ_MAGIC_ADR);
	sharpsl_flash_param.comadj = (unsigned int) FLASH_DATA(FLASH_COMADJ_DATA_ADR);

  // get phad
	sharpsl_flash_param.phad_keyword = (unsigned int) FLASH_DATA(FLASH_PHAD_MAGIC_ADR);
	sharpsl_flash_param.phadadj = (unsigned int) FLASH_DATA(FLASH_PHAD_DATA_ADR);
}
EXPORT_SYMBOL(sharpsl_get_param);

#endif
