/*
 * usb-ohci-sa1111.h
 *
 * definitions and special code for Intel SA-1111 USB OHCI host controller
 *
 * 06/05/01 Brad Parker <brad@heeltoe.com>
 * updated for new usb-ohci.c; moved h/w specific code to
 * arch/arm/mach-sa1100/sa1111.c
 *
 * 10/24/00 Brad Parker <brad@heeltoe.com>
 * added memory allocation code
 *
 * 09/26/00 Brad Parker <brad@heeltoe.com>
 * init code for the SA-1111 ohci controller
 * special dma map/unmap code to compensate for SA-1111 h/w bug
 * 
 */

#define OHCI_NON_PCI_BASE        SA1111_p2v(_SA1111(0x0400))
#define OHCI_NON_PCI_EXTENT      512
#define OHCI_NON_PCI_IRQ         NIRQHCIM

/* arch/arm/mach-sa1100/sa1111.c */
void sa1111_ohci_hcd_cleanup(void);

static void
ohci_non_pci_cleanup(void)
{
	sa1111_ohci_hcd_cleanup();
}

/* Make the remaining of the code happy */
#ifndef CONFIG_PCI
#define CONFIG_PCI
#endif

