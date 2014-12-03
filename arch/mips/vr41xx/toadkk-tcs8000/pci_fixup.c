/*
 * FILE NAME
 *	arch/mips/vr41xx/nec-eagle/pci_fixup.c
 *
 * BRIEF MODULE DESCRIPTION
 *	The NEC Eagle/Hawk Board specific PCI fixups.
 *
 * Author: Yoichi Yuasa
 *         yyuasa@mvista.com or source@mvista.com
 *
 * Copyright 2001,2002 MontaVista Software Inc.
 *
 *  This program is free software; you can redistribute it and/or modify it
 *  under the terms of the GNU General Public License as published by the
 *  Free Software Foundation; either version 2 of the License, or (at your
 *  option) any later version.
 *
 *  THIS SOFTWARE IS PROVIDED ``AS IS'' AND ANY EXPRESS OR IMPLIED
 *  WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 *  MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 *  IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 *  INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 *  BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS
 *  OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 *  ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR
 *  TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE
 *  USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 *  You should have received a copy of the GNU General Public License along
 *  with this program; if not, write to the Free Software Foundation, Inc.,
 *  675 Mass Ave, Cambridge, MA 02139, USA.
 */
/*
 * Changes:
 *  MontaVista Software Inc. <yyuasa@mvista.com> or <source@mvista.com>
 *  - Moved mips_pci_channels[] to arch/mips/vr41xx/vr4122/eagle/setup.c.
 *  - Added support for NEC Hawk.
 *
 *  Paul Mundt <lethal@chaoticdreams.org>
 *  - Fix empty break statements, remove useless CONFIG_PCI.
 *
 *  MontaVista Software Inc. <yyuasa@mvista.com> or <source@mvista.com>
 *  - New creation, NEC Eagle is supported.
 */
#include <linux/config.h>
#include <linux/init.h>
#include <linux/pci.h>

#include <asm/vr41xx/toadkk-tcs8000.h>

#if 0 /*@@@@@*/
#define TRACE printk(KERN_ERR __FUNCTION__ ": %d\n", __LINE__ )
#else
#define TRACE /**/
#endif

void __init pcibios_fixup_resources(struct pci_dev *dev)
{
}

void __init pcibios_fixup(void)
{
}

void __init pcibios_fixup_irqs(void)
{
	struct pci_dev *dev;
	u8 slot, func, pin;

	TRACE;

	pci_for_each_dev(dev) {
		slot = PCI_SLOT(dev->devfn);
		func = PCI_FUNC(dev->devfn);
		printk ( KERN_ERR __FUNCTION__ ": slot = %d\n", slot );
		//pci_read_config_byte(dev, PCI_INTERRUPT_PIN, &pin);
		dev->irq = 0;

		switch (slot) {
		case 29:
			dev->irq = VR4181A_AC97U_INT;
			break;
		case 30:
			dev->irq = VR4181A_USBFU_INT;
			break;
		case 31:
			dev->irq = VR4181A_USBHU_INT;
#if 1 /*@@@@@*/
			{
				u16 cmd;
				
				if ( pci_read_config_word(dev, PCI_COMMAND, &cmd ) == PCIBIOS_SUCCESSFUL ) {
					cmd |= PCI_COMMAND_MEMORY|PCI_COMMAND_MASTER;
					pci_write_config_word ( dev,PCI_COMMAND,cmd );
				}
			}
#endif
			break;
		default:
			continue;
		}

		pci_write_config_byte(dev, PCI_INTERRUPT_LINE, dev->irq);
	}
}

unsigned int pcibios_assign_all_busses(void)
{
	return 0;
}
