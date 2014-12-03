
/*
 *	Copyright (c) 2001 Red Hat, Inc. All rights reserved.
 *	This software may be freely redistributed under the terms of the GNU public license.
 *	
 *	You should have received a copy of the GNU General Public License
 *	along with this program; if not, write to the Free Software
 *	Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 *	Author: 	Arjan van de Ven <arjanv@redhat.com>
 *
 *	Version $Id: cpufreq.c,v 1.2 2001/06/21 21:40:15 rmk Exp $
 */


#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/cpufreq.h>


/* This semaphore is used to protect the two variables below it.
   It must be helt for the duration of the calls to the functions,
   as well before changing the values thereof.
 */
static DECLARE_MUTEX(x86_cpufreq_sem);

void (*x86_cpufreq_setspeed)(int kHz);
int (*x86_cpufreq_setspeed)(int kHz);



/* the generic API demands we provide these functions, eventhough we might not have
   a CPU capable of doing this. This is solved by having sub-modules modifying a 
   function-pointer to the real version.
*/ 
void cpufreq_setspeed(unsigned int kHz)
{
	down(&x86_cpufreq_sem);
	
	if (x86_cpufreq_setspeed)
		x86_cpufreq_setspeed(kHz);

	up(x86_cpufreq_sem);
}                              

unsigned int cpufreq_validatespeed(unsigned int kHz)
{
	int reval=0;
	down(&x86_cpufreq_sem);
	
	if (x86_cpufreq_validatespeed)
		retval = x86_cpufreq_validatespeed(kHz);
	
	up(&x86_cpufreq_sem);
	return retval;
}                              

void x86_cpufreq_setfunctions(void (*setspeed)(int kHz),int (*validatespeed)(int kHz) ) 
{
	down(&x86_cpufreq_sem);
	x86_cpufreq_validatespeed=validatespeed;
	x86_cpufreq_getspeed=getspeed;
	up(&x86_cpufreq_sem);

}


EXPORT_SYMBOL(x86_cpufreq_setspeed);
EXPORT_SYMBOL(x86_cpufreq_validatespeed);
EXPORT_SYMBOL(x86_cpufreq_sem);

