/*
 *  TC35143
 *
 * (C) Copyright 2001 Lineo Japan, Inc.
 *
 * May be copied or modified under the terms of the GNU General Public
 * License.  See linux/COPYING for more information.
 *
 */

#ifndef __ASM_ARCH_TC35143_H
#define __ASM_ARCH_TC35143_H 1

#include <asm/hardware.h>
#include <asm/delay.h>



/*
 * TC35143 control register
 */
#define TC35143_CONTROL_REG_A	0x0007
#define TC35143_CONTROL_REG_B	0x0008
#define TC35143_MODE_REG	0x000D

/*
 * TC35143 Audio control register A Set up data
 */
#define TC35143_VINSEL_MASK	(0x3 << 14)
#define TC35143_VINSEL_MUTE	(0x0 << 14)
#define TC35143_VINSEL_VBIN1	(0x1 << 14)
#define TC35143_VINSEL_VBIN2	(0x2 << 14)
#define TC35143_VOUT2_EN	(0x1 << 13)
#define TC35143_VGAIN_MASK	(0x3 << 11)
#define TC35143_VGAIN_0DB	(0x0 << 11)
#define TC35143_VGAIN_6DB	(0x1 << 11)
#define TC35143_VGAIN_12DB	(0x2 << 11)
#define TC35143_VGAIN_18DB	(0x3 << 11)
#define TC35143_VAMP_MASK	(0xf << 7)
#define TC35143_VDIV_MASK	(0x7f)
#define TC35143_VDIV_8KHZ	(0x24)
#define TC35143_VDIV_11KHZ	(0x1a)
#define TC35143_VDIV_16KHZ	(0x12)
#define TC35143_VDIV_22KHZ	(0x0d)

/*
 * TC35143 Audio control register B Set up data
 */
#define TC35143_VOUT1_EN	(0x1 << 15)
#define TC35143_VIN_EN		(0x1 << 14)
#define TC35143_VADMUTE		(0x1 << 13)
#define TC35143_VHSMUTE		(0x1 << 12)
#define TC35143_VHSATT_MASK	(0x7 << 9)
#define TC35143_VLOOP_EN	(0x1 << 8)
#define TC35143_VDAMUTE		(0x1 << 7)
#define TC35143_VCLP_CLR	(0x1 << 6)
#define TC35143_VSPMUTE		(0x1 << 5)
#define TC35143_VDA_ATT_MASK	(0x3 << 3)
#define TC35143_VSP_ATT_MASK	(0x7)
#define TC35143_ALL_MUTE        (TC35143_VADMUTE|TC35143_VHSMUTE|TC35143_VDAMUTE|TC35143_VSPMUTE)

/*
 * TC35143 mode register Set up data
 */
#define TC35143_VOFFCAN			(0x1 << 13)
#define TC35143_DFLG_EN			(0x1 << 12)
#define TC35143_VCOF_MASK		(0x7 << 8)
#define TC35143_VCOF_11_OR_8KHZ		(0x1 << 8)
#define TC35143_VCOF_22_OR_16KHZ	(0x4 << 8)

/*
 * TC35143 GPIO Set up data
 */
#define TC35143_GPIO_VERSION0    0	/* GPIO0=Version                 */
#define TC35143_GPIO_TBL_CHK     1	/* GPIO1=TBL_CHK                 */
#define TC35143_GPIO_VPEN_ON     2	/* GPIO2=VPNE_ON                 */
#define TC35143_GPIO_IR_ON       3	/* GPIO3=IR_ON                   */
#define TC35143_GPIO_AMP_ON      4	/* GPIO4=AMP_ON                  */
#define TC35143_GPIO_VERSION1    5	/* GPIO5=Version                 */
#define TC35143_GPIO_FS8KLPF     5	/* GPIO5=fs 8k LPF               */
//#ifdef CONFIG_COLLIE_TR0
#define TC35143_GPIO_CHRG_ON     6	/* GPIO6=CHRG_ON                 */
//#else
#define TC35143_GPIO_BUZZER_BIAS 6	/* GPIO6=BUZZER BIAS             */
//#endif
#define TC35143_GPIO_MBAT_ON     7	/* GPIO7=MBAT_ON                 */
#define TC35143_GPIO_BBAT_ON     8	/* GPIO8=BBAT_ON                 */
#define TC35143_GPIO_TMP_ON      9	/* GPIO9=TMP_ON                  */

#define TC35143_ADC_BAT_TMP      ADC_INPUT_AD0	/* BAT TMP               */
#define TC35143_ADC_BAT_VOL      ADC_INPUT_AD1	/* MAIN/BACKUP BAT VOL   */
#define TC35143_ADC_TC_PRESSURE  ADC_INPUT_AD2 	/* TC PRESSURE           */
#define TC35143_ADC_REMOCON_KEY  ADC_INPUT_AD3	/* REMOCON KEY           */

#define TC35143_IODIR_OUTPUT	 1	/* set up output mode            */
#define TC35143_IODIR_INPUT	 0	/* set up input  mode            */

#define TC35143_IODAT_LOW	 0	/* set up fs 8k LPF on data      */
#define TC35143_IODAT_HIGH	 1	/* set up fs 8k LPF off data     */

#endif
