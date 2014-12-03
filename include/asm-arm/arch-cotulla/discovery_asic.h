/*
 * linux/arm/arch-cotulla/discovery_asic.h :  This file contains Discovery-specific tweaks.
 *
 * Copyright (C) 2002 Richard Rau (richard.rau@msa.hinet.net)
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
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 * May be copied or modified under the terms of the GNU General Public
 * License.  See linux/COPYING for more information.
 *
 * ChangeLog:
 *  04-15-2002 Steve Lin, modified for physical address
 */
#ifndef _INCLUDE_DISCOVERY_ASIC_H_
#define _INCLUDE_DISCOVERY_ASIC_H_

/********* GPIO **********/

#define _DISCOVERY_ASIC1_GPIO_Base	    0xfC000000
#define _DISCOVERY_ASIC1_GPIO_Base_P	    0x0C000000

#define DISCOVERY_ASIC1_OFFSET(s,x)   \
     (*((volatile s *) (_DISCOVERY_ASIC1_GPIO_Base + (_DISCOVERY_ASIC1_GPIO_ ## x))))

#define DISCOVERY_ASIC1_OFFSET_VALUE(s,x)   \
     (_DISCOVERY_ASIC1_GPIO_Base + (_DISCOVERY_ASIC1_GPIO_ ## x))

#define DISCOVERY_ASIC1_OFFSET_P(s,x)   \
     (*((volatile s *) (_DISCOVERY_ASIC1_GPIO_Base_P + (_DISCOVERY_ASIC1_GPIO_ ## x))))

#define _DISCOVERY_ASIC1_GPIO_Mask          0x60    /* R/W 0:don't mask, 1:mask interrupt */
#define _DISCOVERY_ASIC1_GPIO_Direction     0x64    /* R/W 0:input, 1:output              */
#define _DISCOVERY_ASIC1_GPIO_Out           0x68    /* R/W 0:output low, 1:output high    */
#define _DISCOVERY_ASIC1_GPIO_TriggerType   0x6c    /* R/W 0:level, 1:edge                */
#define _DISCOVERY_ASIC1_GPIO_EdgeTrigger   0x70    /* R/W 0:falling, 1:rising            */
#define _DISCOVERY_ASIC1_GPIO_LevelTrigger  0x74    /* R/W 0:low, 1:high level detect     */
#define _DISCOVERY_ASIC1_GPIO_LevelStatus   0x78    /* R/W 0:none, 1:detect               */
#define _DISCOVERY_ASIC1_GPIO_EdgeStatus    0x7c    /* R/W 0:none, 1:detect               */
#define _DISCOVERY_ASIC1_GPIO_State         0x80    /* R   See masks below  (default 0)         */
#define _DISCOVERY_ASIC1_GPIO_Reset         0x84    /* R/W See masks below  (default 0x04)      */
#define _DISCOVERY_ASIC1_GPIO_SleepMask     0x88    /* R/W 0:don't mask, 1:mask trigger in sleep mode  */
#define _DISCOVERY_ASIC1_GPIO_SleepDir      0x8c    /* R/W direction 0:input, 1:ouput in sleep mode    */
#define _DISCOVERY_ASIC1_GPIO_SleepOut      0x90    /* R/W level 0:low, 1:high in sleep mode           */
#define _DISCOVERY_ASIC1_GPIO_Status        0x94    /* R   Pin status                                  */
#define _DISCOVERY_ASIC1_GPIO_BattFaultDir  0x98    /* R/W direction 0:input, 1:output in batt_fault   */
#define _DISCOVERY_ASIC1_GPIO_BattFaultOut  0x9c    /* R/W level 0:low, 1:high in batt_fault           */


#define DISCOVERY_ASIC1_GPIO_MASK_VALUE            DISCOVERY_ASIC1_OFFSET_VALUE( u16, Mask )

#define DISCOVERY_ASIC1_GPIO_MASK            DISCOVERY_ASIC1_OFFSET( u16, Mask )
#define DISCOVERY_ASIC1_GPIO_DIR             DISCOVERY_ASIC1_OFFSET( u16, Direction )
#define DISCOVERY_ASIC1_GPIO_OUT             DISCOVERY_ASIC1_OFFSET( u16, Out )
#define DISCOVERY_ASIC1_GPIO_LEVELTRI        DISCOVERY_ASIC1_OFFSET( u16, TriggerType )
#define DISCOVERY_ASIC1_GPIO_RISING          DISCOVERY_ASIC1_OFFSET( u16, EdgeTrigger )
#define DISCOVERY_ASIC1_GPIO_LEVEL           DISCOVERY_ASIC1_OFFSET( u16, LevelTrigger )
#define DISCOVERY_ASIC1_GPIO_LEVEL_STATUS    DISCOVERY_ASIC1_OFFSET( u16, LevelStatus )
#define DISCOVERY_ASIC1_GPIO_EDGE_STATUS     DISCOVERY_ASIC1_OFFSET( u16, EdgeStatus )
#define DISCOVERY_ASIC1_GPIO_STATE           DISCOVERY_ASIC1_OFFSET(  u8, State )
#define DISCOVERY_ASIC1_GPIO_RESET           DISCOVERY_ASIC1_OFFSET(  u8, Reset )
#define DISCOVERY_ASIC1_GPIO_SLEEP_MASK      DISCOVERY_ASIC1_OFFSET( u16, SleepMask )
#define DISCOVERY_ASIC1_GPIO_SLEEP_DIR       DISCOVERY_ASIC1_OFFSET( u16, SleepDir )
#define DISCOVERY_ASIC1_GPIO_SLEEP_OUT       DISCOVERY_ASIC1_OFFSET( u16, SleepOut )
#define DISCOVERY_ASIC1_GPIO_STATUS          DISCOVERY_ASIC1_OFFSET( u16, Status )
#define DISCOVERY_ASIC1_GPIO_BATT_FAULT_DIR  DISCOVERY_ASIC1_OFFSET( u16, BattFaultDir )
#define DISCOVERY_ASIC1_GPIO_BATT_FAULT_OUT  DISCOVERY_ASIC1_OFFSET( u16, BattFaultOut )

#define DISCOVERY_ASIC1_GPIO_MASK_P            DISCOVERY_ASIC1_OFFSET_P( u16, Mask )
#define DISCOVERY_ASIC1_GPIO_DIR_P             DISCOVERY_ASIC1_OFFSET_P( u16, Direction )
#define DISCOVERY_ASIC1_GPIO_OUT_P             DISCOVERY_ASIC1_OFFSET_P( u16, Out )
#define DISCOVERY_ASIC1_GPIO_LEVELTRI_P        DISCOVERY_ASIC1_OFFSET_P( u16, TriggerType )
#define DISCOVERY_ASIC1_GPIO_RISING_P          DISCOVERY_ASIC1_OFFSET_P( u16, EdgeTrigger )
#define DISCOVERY_ASIC1_GPIO_LEVEL_P           DISCOVERY_ASIC1_OFFSET_P( u16, LevelTrigger )
#define DISCOVERY_ASIC1_GPIO_LEVEL_STATUS_P    DISCOVERY_ASIC1_OFFSET_P( u16, LevelStatus )
#define DISCOVERY_ASIC1_GPIO_EDGE_STATUS_P     DISCOVERY_ASIC1_OFFSET_P( u16, EdgeStatus )
#define DISCOVERY_ASIC1_GPIO_STATE_P           DISCOVERY_ASIC1_OFFSET_P(  u8, State )
#define DISCOVERY_ASIC1_GPIO_RESET_P           DISCOVERY_ASIC1_OFFSET_P(  u8, Reset )
#define DISCOVERY_ASIC1_GPIO_SLEEP_MASK_P      DISCOVERY_ASIC1_OFFSET_P( u16, SleepMask )
#define DISCOVERY_ASIC1_GPIO_SLEEP_DIR_P       DISCOVERY_ASIC1_OFFSET_P( u16, SleepDir )
#define DISCOVERY_ASIC1_GPIO_SLEEP_OUT_P       DISCOVERY_ASIC1_OFFSET_P( u16, SleepOut )
#define DISCOVERY_ASIC1_GPIO_STATUS_P          DISCOVERY_ASIC1_OFFSET_P( u16, Status )
#define DISCOVERY_ASIC1_GPIO_BATT_FAULT_DIR_P  DISCOVERY_ASIC1_OFFSET_P( u16, BattFaultDir )
#define DISCOVERY_ASIC1_GPIO_BATT_FAULT_OUT_P  DISCOVERY_ASIC1_OFFSET_P( u16, BattFaultOut )

#define DISCOVERY_ASIC1_GPIO_STATE_MASK            (1 << 0)
#define DISCOVERY_ASIC1_GPIO_STATE_DIRECTION       (1 << 1)
#define DISCOVERY_ASIC1_GPIO_STATE_OUT             (1 << 2)
#define DISCOVERY_ASIC1_GPIO_STATE_TRIGGER_TYPE    (1 << 3)
#define DISCOVERY_ASIC1_GPIO_STATE_EDGE_TRIGGER    (1 << 4)
#define DISCOVERY_ASIC1_GPIO_STATE_LEVEL_TRIGGER   (1 << 5)

#define DISCOVERY_ASIC1_GPIO_RESET_SOFTWARE        (1 << 0)
#define DISCOVERY_ASIC1_GPIO_RESET_AUTO_SLEEP      (1 << 1)
#define DISCOVERY_ASIC1_GPIO_RESET_FIRST_PWR_ON    (1 << 2)


#define AS1_GPIO_WAKE_UP		(1 << 0)
#define AS1_GPIO_CF_IOIS16		(1 << 1)
#define AS1_GPIO_SD_DETECT		(1 << 2)
#define AS1_GPIO_CF_DETECT		(1 << 3)
#define AS1_GPIO_CF_CHG_EN		(1 << 4)
#define AS1_GPIO_KEY_IN			(1 << 5)
#define AS1_GPIO_KEY_INTERRUPT		(1 << 6)
#define AS1_GPIO_HEADPHONE		(1 << 7)
#define AS1_GPIO_AC_IN			(1 << 8)
#define AS1_GPIO_PWR_ON			(1 << 9)
#define AS1_GPIO_COM_DCD		(1 << 10)
#define AS1_GPIO_CONN60_4		(1 << 11)
#define AS1_GPIO_CONN60_5		(1 << 12)
#define AS1_GPIO_BACKPART_DETECT	(1 << 13)
#define AS1_GPIO_CF_BATT_FAULT    	(1 << 14)

#define GPIO1_WAKE_UP			AS1_GPIO_WAKE_UP
#define GPIO1_CF_IOIS16			AS1_GPIO_CF_IOIS16
#define GPIO1_SD_DETECT			AS1_GPIO_SD_DETECT
#define GPIO1_CF_DETECT			AS1_GPIO_CF_DETECT
#define GPIO1_CF_CHG_EN			AS1_GPIO_CF_CHG_EN
#define GPIO1_KEY_IN			AS1_GPIO_KEY_IN
#define GPIO1_KEY_INTERRUPT		AS1_GPIO_KEY_INTERRUPT
#define GPIO1_HEADPHONE			AS1_GPIO_HEADPHONE
#define GPIO1_AC_IN			AS1_GPIO_AC_IN
#define GPIO1_PWR_ON			AS1_GPIO_PWR_ON
#define GPIO1_COM_DCD			AS1_GPIO_COM_DCD
#define GPIO1_CONN60_4			AS1_GPIO_CONN60_4
#define GPIO1_CONN60_5			AS1_GPIO_CONN60_5
#define GPIO1_BACKPART_DETECT		AS1_GPIO_BACKPART_DETECT
#define GPIO1_CF_BATT_FAULT		AS1_GPIO_CF_BATT_FAULT

#endif /* _INCLUDE_DISCOVERY_ASIC_H_ */
