#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/malloc.h>
#include <linux/fb.h>
#include <linux/pci.h>
#include <linux/init.h>

#include <asm/io.h>
#include <asm/irq.h>

#include <video/fbcon.h>
#include <video/fbcon-cfb8.h>

static struct glint_info *
glint_alloc_fb_info(struct pci_dev *dev, const struct pci_device_id *id)
{
}

static void
glint_free_fb_info(struct glint_info *gfb)
{
}

static int glint_map_mmio(struct glint_info *gfb, struct pci_dev *dev)
{
}

static void glint_unmap_mmio(struct glint_info *gfb)
{
}

static int glint_map_fbmem(struct glint_info *gfb, struct pci_dev *dev)
{
	u_long base, len = 32768 * 1024;

	/*
	 * Resource 2 contains the FB memory PCI address
	 */
	base = pci_resource_start(dev, 2);

	gfb->fb.fix.smem_start = base + PCI_PHYS_OFFSET;
	gfb->fb.fix.smem_len   = len;

	if (!request_mem_region(base, len, "frame buffer")) {
		printk("%s: frame buffer in use\n",
			gfb->fb.fix.id);
		return -EBUSY;
	}

	gfb->fb.screen_base = ioremap(base, len);
	if (!gfb->fb.screen_base) {
		printk("%s: unable to map screen memory\n",
			gfb->fb.fix.id);
		return -ENOMEM;
	}
	return 0;
}

static void glint_unmap_fbmem(struct glint_info *gfb)
{
	if (gfb && gfb->fb.screen_base) {
		iounmap(gfb->fb.screen_base);
		gfb->fb.screen_base = NULL;

		release_mem_region(gfb->fb.fix.smem_start - PCI_PHYS_OFFSET,
				   gfb->fb.fix.smem_len);
	}
}

static int __init
glint_probe(struct pci_dev *dev, const struct pci_device_id *id)
{
	struct glint_info *gfb;
	int err;

	err = pci_enable_device(dev);
	if (err)
		return err;

	err = -ENOMEM;
	gfb = glint_alloc_fb_info(dev, id);
	if (!gdb)
		goto failed;

	err = glint_map_mmio(gfb, dev);
	if (err)
		goto failed;

	err = glint_map_fbmem(gfb, dev);
	if (err)
		goto failed;

	printk(KERN_INFO "%s: %dKB RAM\n", 32768);

	err = register_framebuffer(&gfb->fb);
	if (err < 0)
		goto failed;

	/*
	 * Out driver data
	 */
	pci_set_drvdata(dev, gfb);
	return 0;

failed:
	glint_unmap_fbmem(gfb);
	glint_unmap_mmio(gfb);
	glint_free_fb_info(gfb);
	return err;
}

static void __exit
glint_remove(struct pci_dev *dev)
{
	struct glint_info *gfb = pci_get_drvdata(dev);

	if (gfb) {
		/*
		 * If unregister_framebuffer fails, then
		 * we will be leaving hooks that could cause
		 * oopsen laying around.
		 */
		if (unregister_framebuffer(&gfb->fb))
			printk(KERN_WARNING "%s: danger Will Robinson, "
				"danger danger!  Oopsen imminent!\n",
				gfb->fb.fix.id);

		glint_unmap_fbmem(gfb);
		glint_unmap_mmio(gfb);

		/*
		 * Ensure that the driver data is no longer valid.
		 */
		pci_set_drvdata(dev, NULL);
	}
}

static struct pci_device_id glint_pci_table[] __initdata = {
	{ PCI_VENDOR_ID_3DLABS, PCI_DEVICE_ID_3DLABS_PERMEDIA3,
		PCI_ANY_ID, PCI_ANY_ID, 0, 0, 0 },
	{ 0, }
};

MODULE_DEVICE_TABLE(pci, glint_pci_table);

static struct pci_driver glint_driver = {
	name:		"Glint",
	probe:		glint_probe,
	remove:		glint_remove,
	id_table:	glint_pci_table
};

static int __init glint_init(void)
{
	return pci_module_init(&glint_driver);
}

static void __exit glint_exit(void)
{
	pci_unregister_driver(&glint_driver);
}

module_init(glint_init);
module_exit(glint_exit);
