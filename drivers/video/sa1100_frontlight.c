/*************************************************************
 *  StrongARM 11xx LCD Frontlight Device
 *
 *  linux/drivers/video/sa1100_frontlight.c 
 *************************************************************
 *
 * Initial Version by: Nicholas Mistry (nmistry@lhsl.com)
 *
 * This driver provides basic functionality for the frontlight
 * provided with the Assabet board.   It will enable and disable
 * the light.  
 * 
 * The module grabs a dynamically assigned Major, so you will have
 * to do a mknod before using the device.  I typically use /dev/sa1100-fl
 * 
 * Because the assabet does not come with its own power supply for the
 * one can be obtained/made.  Below is a list of the parts i used to 
 * enable mine. 
 * 
 * **   The parts/cable described below assume that your assabet 
 * **   is using the Sharp LQ039Q2DS54 Screen. 
 * 
 * Parts List:
 * ------------
 *   1 ERG power 8m122375 DC to AC inverter.
 *     obtain from: Endicott Research Group Inc. (http://www.ergpower.com)
 * 
 *   Assabet Side Connector:
 *     1 Minitek 90312-005 Housing 
 *     3 Minitek 77138-101 Pins
 * 	obtain both from:  FCI Framatome Group (http://www.fciconnect.com)
 * 	* Samples Available *  
 * 
 *   Inverter Side Connector:
 *     1 Standard 0.1" 4 pin Housing
 *     3 Standard 0.1" Pins  (crimp is preferred)
 * 
 *   3 pieces of 26 AWG Stranded Wire each aprox 4"-6" long 
 * 	(3 colors optional: red,black,white)
 * 
 * Cable Diagram:
 * --------------
 * 
 * Inverter:				Assabet:
 *  
 *  _____                                   _______
 *       |      ______     /---------------|  |1  [|  (red wire)
 *  V+  1|---- |]   1 |---/   /------------|_ |2  [|  (black wire)
 *  GND 2|---- |]   2 |------(------\      |_||3  [|
 *  CTL 3|---- |]   3 |-----/        \-----|  |4  [|  (white wire)
 *  NE  4|---- |]___4_|                    |__|5__[|
 *  _____|
 * 
 * 
 * Example Program: 
 * ----------------
 * 
 * // This following example program will blink on and off the  
 * //  frontlight 5 times w/ an one second delay.  	     
 * 
 * #include <stdio.h>
 * #include <stdlib.h>
 * #include <sys/ioctl.h>
 * #include <sys/types.h>
 * #include <fcntl.h>
 * #include <unistd.h>
 * #include <errno.h>
 * 
 * #include <video/sa1100_frontlight.h>
 * 
 * int main(void) {
 * 
 *   int i, fd, retval;
 * 
 *   fd = open ("/dev/sa1100-fl", O_WRONLY);
 * 
 *   if (fd ==  -1) {
 *     perror("/dev/sa1100-fl");
 *     exit(errno);
 *   }
 *   
 *   printf("\n\t\t\tFrontlight Driver Test Example.\n\n");
 *   
 *   for (i=0; i<5; i++) {
 *     printf("Turning OFF Frontlight...\n");
 *     retval = ioctl(fd, _SA1100_FL_OFF, 0);
 *     if (retval == -1) {
 *       perror("ioctl");
 *       exit(errno);
 *     }
 *     
 *     sleep(1);
 *     
 *     printf("Turning ON Frontlight...\n");
 *     retval = ioctl(fd, _SA1100_FL_ON, 0);
 *     if (retval == -1) {
 *       perror("ioctl");
 *       exit(errno);
 *     }
 *     
 *     sleep(1);
 *     
 *   }
 *   
 *   printf("Turning OFF Frontlight...\n");
 *   retval = ioctl(fd, _SA1100_FL_OFF, 0);
 *   if (retval == -1) {
 *     perror("ioctl");
 *     exit(errno);
 *   }
 * 
 *   fprintf(stderr, "Done.\n");
 * 
 * }
 * 
 *************************************************************
 * Revision History:
 * 	07/07/2000 - Nicholas Mistry (nmistry@lhsl.com)
 *		Initial release for assabet, no dimming support, 
 *		can enable and disable the frontlight via ioctl.
 *		Initial	framework for dimming included.
 *
 *  30/1/2001 - Chester Kuo (chester@linux.or.tw)
 *		Add the backlight/contrast control of SA-1110 board.
 *		Test with FreeBird-1.1 board,but i think  it's useful 
 *		for other SA-11x0 based board.
 *		
 *	4/3/2001 - Tim Wu <timwu@coventive.com)
 *	    Make physical backlight in hardware consistent with counters   
 *
 * Change Log
 *	12-Nov-2001 Lineo Japan, Inc.
 *
 *************************************************************/

#include <linux/config.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/sched.h>
#include <linux/errno.h>
#include <linux/string.h>
#include <linux/ctype.h>
#include <linux/mm.h>
#include <linux/tty.h>
#include <linux/malloc.h>
#include <linux/init.h>
#include <linux/fb.h>
#include <linux/delay.h>

#include <asm/system.h>
#include <asm/hardware.h>
#include <asm/io.h>
#include <asm/irq.h>
#include <asm/uaccess.h>
#include <asm/proc/pgtable.h>

#include <video/sa1100_frontlight.h>
#include <linux/pm.h>

#ifdef CONFIG_PM
  static struct pm_dev *lcd_pm_dev; /* registered PM device */
#endif


#ifdef MODULE

MODULE_AUTHOR("Nicholas N. Mistry (nmistry@lhsl.com)");
MODULE_DESCRIPTION("SA1100 LCD light Driver");

#endif

#ifdef CONFIG_SA1100_FREEBIRD
#define MAX_BRIGHTNESS 31
#define MAX_CONTRAST   31
#define DEFAULT_BRIGHTNESS  27
#define DEFAULT_CONTRAST    18
#else
#define MAX_BRIGHTNESS  100
#define MAX_CONTRAST   100
#endif


/* Backlight conter */
static int counter_bl = 1;
/* LCD contrast counter */
static int counter_contrast = 1;
static int UP = 1;
static int DOWN = 0;
//------------------------------------------------------------

static int sa1100_fl_ioctl(struct inode * inode, struct file *filp,
			   unsigned int cmd , unsigned long arg);

static int sa1100_fl_open(struct inode *inode, struct file *filp);
static int sa1100_fl_close(struct inode *inode, struct file *filp);
//------------------------------------------------------------


static struct file_operations sa1100_fl_fops = {
	ioctl:		sa1100_fl_ioctl,
	open:		sa1100_fl_open,
	release:	sa1100_fl_close,
};

//------------------------------------------------------------

static int sa1100_frontlight_major;


/*------------------------------------------------------------
 * Set the output intensity of the frontlight.  
 *   intensity is a value between 0..100  
 *   where: 0 = full off  and    100 = full on 
 */

static int sa1100_fl_setintensity(int intensity)
{

  if(machine_is_assabet()) {
    printk(KERN_WARNING "sa1100_frontlight: intensity(%d) not available"
	   " for assabet!\n", intensity);
    return 0;
  }


				/* if the intensity is set to 0, turn off */
  if(intensity == 0) 
    sa1100_fl_disable();

				/* otherwise set the intensity and turn */
				/* on the frontlight */

				/* TODO:  Add code for dimming here */

  printk(KERN_WARNING "sa1100_frontlight: intensety(%d) not implemented yet\n",
	 intensity);

  sa1100_fl_enable();

  return 0;

}
static int sa1100_bl_setting(int need_value)
{
	if(!machine_is_freebird())
	{
		printk(KERN_WARNING	"backlight control not available!\n");
		return 0;
	}

#if CONFIG_SA1100_FREEBIRD
	if(need_value > counter_bl) {
			//BCR_set(BCR_FREEBIRD_LCD_LIGHT_DU);
			BCR_clear(BCR_FREEBIRD_LCD_LIGHT_DU);
			sa1100_bl_function(UP);
	}
	else {
			BCR_set(BCR_FREEBIRD_LCD_LIGHT_DU);
			//BCR_clear(BCR_FREEBIRD_LCD_LIGHT_DU);
			sa1100_bl_function(DOWN);
	}
#endif
	return counter_bl;
}
static int sa1100_contrast_setting(int need_value)
{
	if(!machine_is_freebird())
	{
		return 0;
	}
#if CONFIG_SA1100_FREEBIRD
	if(need_value > counter_contrast) {
			BCR_clear(BCR_FREEBIRD_LCD_DU);
			sa1100_contrast_function(UP);
	}
	else {
			BCR_set(BCR_FREEBIRD_LCD_DU);
			sa1100_contrast_function(DOWN);
	}
#endif	
	return counter_contrast;
}
static int sa1100_bl_function(int up)
{

#if CONFIG_SA1100_FREEBIRD
	if (up) { 
	    /* then send a pulse to it */	
		BCR_clear(BCR_FREEBIRD_LCD_LIGHT_INC);
		BCR_set(BCR_FREEBIRD_LCD_LIGHT_INC);
		udelay(1000);
		BCR_clear(BCR_FREEBIRD_LCD_LIGHT_INC);
		++counter_bl;
	}
	else {
		/* send a pulse */
		BCR_clear(BCR_FREEBIRD_LCD_LIGHT_INC);
		BCR_set(BCR_FREEBIRD_LCD_LIGHT_INC);
		udelay(1000);
		BCR_clear(BCR_FREEBIRD_LCD_LIGHT_INC);
		--counter_bl;
	}
#endif
	return counter_bl;
}
static int sa1100_contrast_function(int up)
{
#if CONFIG_SA1100_FREEBIRD
	if (up) {
		/* send a pluse */
		BCR_clear(BCR_FREEBIRD_LCD_INC);
		BCR_set(BCR_FREEBIRD_LCD_INC);
		udelay(1000);
		BCR_clear(BCR_FREEBIRD_LCD_INC);
		++counter_contrast;
	}
	else {
		/* send a pluse */
		BCR_clear(BCR_FREEBIRD_LCD_INC);
		BCR_set(BCR_FREEBIRD_LCD_INC);
		udelay(1000);
		BCR_clear(BCR_FREEBIRD_LCD_INC);
		--counter_contrast;
	}
#endif
	return counter_contrast;
}
		
		

static int sa1100_fl_enable(void)
{
#if CONFIG_SA1100_ASSABET
  BCR_set(BCR_LIGHT_ON);
#elif CONFIG_SA1100_FREEBIRD
  BCR_set(BCR_FREEBIRD_LCD_BACKLIGHT /*| BCR_FREEBIRD_LCD_DISP*/);
#endif
  return 0;
}

static int sa1100_fl_disable(void)
{
#if CONFIG_SA1100_ASSABET
  BCR_clear(BCR_LIGHT_ON);
#elif CONFIG_SA1100_FREEBIRD
  BCR_clear(BCR_FREEBIRD_LCD_BACKLIGHT);
#endif
  return 0;
}

#ifdef CONFIG_SA1100_FREEBIRD
/* Make physical backlight in hardware consistent with counters we
 * maintain */
static void sync_backlight(void){
	int i;
	
	BCR_set(BCR_FREEBIRD_LCD_LIGHT_DU);
	for(i=0;i<MAX_BRIGHTNESS;i++){
	  	sa1100_bl_function(DOWN);
  	}
  	BCR_set(BCR_FREEBIRD_LCD_DU);
  	for(i=0;i<MAX_CONTRAST;i++){
   		sa1100_contrast_function(DOWN);
  	}
  	counter_bl = 0;
  	counter_contrast = 0;
}

static void set_brightness_contrast( int bl, int contrast){
	BCR_clear(BCR_FREEBIRD_LCD_LIGHT_DU);
  	while(counter_bl!=bl)
    	sa1100_bl_function(UP);

  	BCR_clear(BCR_FREEBIRD_LCD_DU);
  	while(counter_contrast!=contrast)
   	sa1100_contrast_function(UP);
}
#endif

static int sa1100_fl_open(struct inode *inode, struct file *filp)
{
    MOD_INC_USE_COUNT;
    return 0;
}

static int sa1100_fl_close(struct inode *inode, struct file *filp)
{
	MOD_DEC_USE_COUNT;
    return 0;
}


//------------------------------------------------------------

static int sa1100_fl_ioctl(struct inode * inode, struct file *filp,
				   unsigned int cmd , unsigned long arg)
{
	int ret;
	ret = (-EINVAL);
	
	switch(cmd) 
	  {
	  case _SA1100_FL_IOCTL_ON:
	    ret = sa1100_fl_enable();
	    break;
	  case _SA1100_FL_IOCTL_OFF:
	    ret = sa1100_fl_disable();
	    break;
	  case _SA1100_FL_IOCTL_INTENSITY:
	    if ((arg >=0) && (arg <= 100)) 
	      ret = sa1100_fl_setintensity(arg);
	    break;
	  case _SA1100_FL_IOCTL_BACKLIGHT:
	    if ((arg >=0) && (arg <= MAX_BRIGHTNESS))
		  ret = sa1100_bl_setting(arg);
		break;
	  case _SA1100_FL_IOCTL_CONTRAST:
	    if ((arg >=0) && (arg <= MAX_CONTRAST))
		  ret = sa1100_contrast_setting(arg);
		break;
	  case _SA1100_FL_IOCTL_GET_BACKLIGHT:
	       ret = counter_bl;
	    break;
	   case _SA1100_FL_IOCTL_GET_CONTRAST:
	       ret = counter_contrast;
	   break;
	  default:
	    break;
	  }
	
	return ret;
}



#ifdef CONFIG_PM
static int lcd_pm_callback(struct pm_dev *pm_dev, pm_request_t req, void *data)
{
	static int back_bl, back_cr;
	switch (req) {
    		case PM_SUSPEND:
			back_bl = counter_bl;
			back_cr = counter_contrast;
#ifdef CONFIG_SA1100_FREEBIRD
			sync_backlight();
#endif
			sa1100_fl_disable();
			break;
		case PM_RESUME:
			sa1100_fl_enable();
#ifdef CONFIG_SA1100_FREEBIRD
			set_brightness_contrast( back_bl, back_cr);	    
#endif
			break;
	}
	return 0;
}
#endif


int __init sa1100frtlight_init(void)
{
  int result;

#if 0
  if ( !(machine_is_assabet() || machine_is_freebird()))
    return -ENODEV;
#endif

  sa1100_frontlight_major = 0;	/* request a dynamic major */

#ifdef CONFIG_SA1100_FREEBIRD
  result = register_chrdev(FL_MAJOR, FL_NAME, &sa1100_fl_fops);
#else
  result = register_chrdev(sa1100_frontlight_major, "sa1100-fl", &sa1100_fl_fops);
#endif

  
  if (result < 0) {
    printk(KERN_WARNING "sa1100_frontlight: cant get major %d\n", 
	   sa1100_frontlight_major);
    return result;
  }
  
  if (sa1100_frontlight_major == 0)  sa1100_frontlight_major = result;

#ifdef CONFIG_PM
     lcd_pm_dev = pm_register(PM_SYS_DEV, 0, lcd_pm_callback);
#endif

  sa1100_fl_enable();
#ifdef CONFIG_SA1100_FREEBIRD
	sync_backlight();
	set_brightness_contrast( DEFAULT_BRIGHTNESS, DEFAULT_CONTRAST);
#endif

  printk("Frontlight Driver Initialized...\n");
  
  return 0;

}
module_init(sa1100frtlight_init);

//------------------------------------------------------------

void __exit sa1100frtlight_exit(void)
{


#ifdef CONFIG_SA1100_FREEBIRD
  unregister_chrdev(FL_MAJOR, FL_NAME);
#else
  unregister_chrdev(sa1100_frontlight_major, "sa1100-fl");
#endif


  printk("Frontlight Driver Unloaded\n");
}

module_exit(sa1100frtlight_exit);


