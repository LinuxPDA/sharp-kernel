/* Power Managment Test Module.  RTSoft Co. 2002 */ 

/* The necessary header files */

/* Standard in kernel modules */
#include <linux/kernel.h>   /* We're doing kernel work */
#include <linux/module.h>   /* Specifically, a module */
#define CONFIG_PM
#include <linux/pm.h>


static int pmdata = -1;

/* Initialize the module - register the proc file  */

int init_module()
{
  pm_send_all(PM_SUSPEND,&pmdata);    
  return(0);
}


/* Cleanup - unregister our file from /proc */
void cleanup_module()
{
  pm_send_all(PM_RESUME,NULL);
}

MODULE_LICENSE( "GPL" );
