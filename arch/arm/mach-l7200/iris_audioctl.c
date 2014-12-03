/*
 *  linux/arch/arm/mach-l7200/iris_audioctl.c
 *
 *  Driver for Audio Ctrl I/O Ports On SHARP PDA Iris
 *
 */

#include <linux/config.h>
#include <linux/module.h>
#include <linux/errno.h>
#include <linux/signal.h>
#include <linux/sched.h>
#include <linux/timer.h>
#include <linux/interrupt.h>
#include <linux/major.h>
#include <linux/string.h>
#include <linux/fcntl.h>
#include <linux/ptrace.h>
#include <linux/ioport.h>
#include <linux/mm.h>
#include <linux/malloc.h>
#include <linux/init.h>
#include <linux/delay.h>
#include <linux/console.h>
#include <linux/poll.h>

#include <asm/system.h>
#include <asm/uaccess.h>
#include <asm/io.h>
#include <asm/irq.h>
#include <asm/bitops.h>
#include <asm/arch/hardware.h>

#include <asm/sharp_char.h>
#include <asm/arch/fpga.h>
#include <asm/arch/gpio.h>

#define DEBUGPRINT(s)   /* printk s */

/*
 * GPOs...
 */

#if 0 /* Audio Filter is removed from IRIS1 QA3 */
#define AUDIO_FILTER_BIT  (1 << 8)
#endif /* Audio Filter is removed from IRIS1 QA3 */
#define DAC_OUTPUT_SW     (bitPA7)

/*
 * operations....
 */

int iris_audioctl_open(struct inode *inode, struct file *filp)
{
  return 0;
}

int iris_audioctl_release(struct inode *inode, struct file *filp)
{
  return 0;
}

extern void Iris_AMP_off_externally(void);
extern void Iris_AMP_on_externally(void);

extern int iris_ctrl_buzzer_sw;

int iris_audioctl_ioctl(struct inode *inode,
			struct file *filp,
			unsigned int command,
			unsigned long arg)
{
  int error;
  switch( command ){
  case SHARP_IRIS_AUFIL_GETVAL:
#if 0 /* Audio Filter is removed from IRIS1 QA3 */
    {
      unsigned long* puser = (unsigned long*)arg;
      unsigned long val = 0;
      if( IO_FPGA_GPO & AUDIO_FILTER_BIT ) val |= SHARP_IRIS_AUFIL_FILTERON;
      error = put_user(val,&(puser));
      if( error ) return error;
    }
#endif /* Audio Filter is removed from IRIS1 QA3 */
    return 0;
    break;
  case SHARP_IRIS_AUFIL_SETVAL:
#if 0 /* Audio Filter is removed from IRIS1 QA3 */
    {
      unsigned long* puser = (unsigned long*)arg;
      if( ( *puser & SHARP_IRIS_AUFIL_FILTERON ) ){
	IO_FPGA_GPO |= AUDIO_FILTER_BIT;
      }else{
	IO_FPGA_GPO &= ~(AUDIO_FILTER_BIT);
      }
    }
#endif /* Audio Filter is removed from IRIS1 QA3 */
    return 0;
    break;
  case SHARP_IRIS_AMP_EXT_ON:
    {
      //printk("exton\n");
      Iris_AMP_on_externally();
      return 0;
    }
    break;
  case SHARP_IRIS_AMP_EXT_OFF:
    {
      //printk("extoff\n");
      Iris_AMP_off_externally();
      return 0;
    }
    break;
#if 1  /* switch OutputPort */
#define SHARP_IRIS_DAC_OUTPUT_BUZZER   0x10
#define SHARP_IRIS_DAC_OUTPUT_SPEAKER  0x11
  case SHARP_IRIS_DAC_OUTPUT_BUZZER:
    {
      printk("DAC to buzzer\n");
      SET_PA_OUT(DAC_OUTPUT_SW);
      SET_PADR_HI(DAC_OUTPUT_SW);
      iris_ctrl_buzzer_sw = 1;
    }
    break;
  case SHARP_IRIS_DAC_OUTPUT_SPEAKER:
    {
      printk("DAC to speaker\n");
      SET_PA_OUT(DAC_OUTPUT_SW);
      SET_PADR_LO(DAC_OUTPUT_SW);
      iris_ctrl_buzzer_sw = 0;
    }
    break;
#endif

#if 1  /* switch OutputPort */
#define SHARP_IRIS_DAC_OUTPUT_BUZON   0x12
#define SHARP_IRIS_DAC_OUTPUT_BUZOFF  0x13
  case SHARP_IRIS_DAC_OUTPUT_BUZON:
    {
      extern int IrisSoundCtl_Dsp_Buzzer_On(void);
      printk("DAC BUZZER ON\n");
      return IrisSoundCtl_Dsp_Buzzer_On();
    }
    break;
  case SHARP_IRIS_DAC_OUTPUT_BUZOFF:
    {
      extern int IrisSoundCtl_Dsp_Buzzer_Off(void);
      printk("DAC BUZZER OFF\n");
      return IrisSoundCtl_Dsp_Buzzer_Off();
    }
    break;
#endif

  default:
    return -EINVAL;
  }
  return -EINVAL;
}

/*
 *   end of source
 */
