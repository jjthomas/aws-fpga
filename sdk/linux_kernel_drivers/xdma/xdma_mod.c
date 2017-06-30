/*
 * Driver for XDMA for Xilinx XDMA IP core
 *
 * Copyright (C) 2007-2017 Sidebranch
 * Copyright (C) 2007-2017 Xilinx, Inc.
 *
 * Leon Woestenberg <leon@sidebranch.com>
 * Richard Tobin <richard.tobin@xilinx.com>
 * Sonal Santan <sonal.santan@xilinx.com>
 * Karen Xie <karen.xie@xilinx.com>
 */

#define pr_fmt(fmt)	KBUILD_MODNAME ":%s: " fmt, __func__

#include <linux/ioctl.h>
#include <linux/types.h>
#include <linux/errno.h>
#include <asm/cacheflush.h>
#include <linux/aer.h>
/* include early, to verify it depends only on the headers above */
#include "libxdma_api.h"
#include "libxdma.h"
#include "xdma_mod.h"
#include "xdma_ioctl.h"

#define XDMA_DRIVER_MAJOR 2017
#define XDMA_DRIVER_MINOR 1
#define XDMA_DRIVER_PATCHLEVEL 6

#define DRV_MODULE_NAME   "xdma"
#define DRV_MODULE_DESC   "Xilinx XDMA Classic Driver"

#if 1
#define DRV_MODULE_VERSION __stringify(XDMA_DRIVER_MAJOR) "." \
                           __stringify(XDMA_DRIVER_MINOR) "." \
                     __stringify(XDMA_DRIVER_PATCHLEVEL)
static char version[] =
  DRV_MODULE_DESC " " DRV_MODULE_NAME
  " v" DRV_MODULE_VERSION "\n";
#else
#define DRV_MODULE_VERSION  "1.0"
#define DRV_MODULE_RELDATE  "Feb. 2017"
static char version[] =
  DRV_MODULE_DESC " " DRV_MODULE_NAME
  " v" DRV_MODULE_VERSION " (" DRV_MODULE_RELDATE ")\n";
#endif
MODULE_AUTHOR("Xilinx, Inc.");
MODULE_DESCRIPTION(DRV_MODULE_DESC);
MODULE_LICENSE("GPL v2");
MODULE_VERSION(DRV_MODULE_VERSION);

/* SECTION: Module global variables */
static int xpdev_cnt = 0;

struct class *g_xdma_class;	/* sys filesystem */

static const struct pci_device_id pci_ids[] = {
	{ PCI_DEVICE(0x10ee, 0x6D30), },
	{ PCI_DEVICE(0x10ee, 0x4A28), },
	{ PCI_DEVICE(0x10ee, 0x4B28), },
	{ PCI_DEVICE(0x10ee, 0x6830), },
	{ PCI_DEVICE(0x10ee, 0x6930), },
	{ PCI_DEVICE(0x10ee, 0x6A30), },
	{ PCI_DEVICE(0x10ee, 0x4908), },
	{ PCI_DEVICE(0x10ee, 0x4828), },
	{ PCI_DEVICE(0x10ee, 0x4808), },
	{ PCI_DEVICE(0x10ee, 0x2808), },
	{ PCI_DEVICE(0x1d0f, 0x1042), },
	{0,}
};
MODULE_DEVICE_TABLE(pci, pci_ids);

#define NODE_PREFIX	"xdma"

static const char * const devnode_names[] = {
	NODE_PREFIX "%d_user",
	NODE_PREFIX "%d_control",
	NODE_PREFIX "%d_events_%d",
	NODE_PREFIX "%d_h2c_%d",
	NODE_PREFIX "%d_c2h_%d"
};

ssize_t show_device_numbers(struct device *dev, struct device_attribute *attr,
				 char *buf)
{
	struct xdma_pci_dev *xpdev = (struct xdma_pci_dev *)dev_get_drvdata(dev);

	return snprintf(buf, PAGE_SIZE, "%d\t%d\n",
			xpdev->major, xpdev->xdev->idx);
}

static DEVICE_ATTR(xdma_dev_instance, S_IRUGO, show_device_numbers, NULL);

static int config_kobject(struct xdma_cdev *xcdev, enum cdev_type type)
{
	int rv = -EINVAL;
	struct xdma_dev *xdev = xcdev->xdev;
	struct xdma_engine *engine = xcdev->engine;

	switch (type) {
	case CHAR_XDMA_H2C:
	case CHAR_XDMA_C2H:
		BUG_ON(!engine);
		rv = kobject_set_name(&xcdev->cdev.kobj, devnode_names[type],
			xdev->idx, engine->channel);
		break;
	case CHAR_USER:
	case CHAR_CTRL:
		rv = kobject_set_name(&xcdev->cdev.kobj, devnode_names[type],
			xdev->idx);
		break;
	case CHAR_EVENTS:
		rv = kobject_set_name(&xcdev->cdev.kobj, devnode_names[type],
			xdev->idx, xcdev->bar);
		break;
	default:
		break;
	}

	if (rv)
		pr_err("type 0x%x, failed %d.\n", type, rv);
	return rv;
}

static int create_sys_device(struct xdma_cdev *xcdev, enum cdev_type type)
{
	struct xdma_dev *xdev = xcdev->xdev;
	struct xdma_engine *engine = xcdev->engine;
	int last_param;

	if (type == CHAR_EVENTS)
		last_param = xcdev->bar;
	else
		last_param = engine ? engine->channel : 0;

	xcdev->sys_device = device_create(g_xdma_class, &xdev->pdev->dev,
		xcdev->cdevno, NULL, devnode_names[type], xdev->idx,
		last_param);

	if (!xcdev->sys_device) {
		pr_err("device_create(%s) failed\n", devnode_names[type]);
		return -1;
	}

	return 0;
}


/* create_xcdev() -- create a character device interface to data or control bus
 *
 * If at least one SG DMA engine is specified, the character device interface
 * is coupled to the SG DMA file operations which operate on the data bus. If
 * no engines are specified, the interface is coupled with the control bus.
 */

static const struct file_operations *select_file_ops(enum cdev_type type);

static int destroy_xcdev(struct xdma_cdev *cdev)
{
	if (!cdev) {
		pr_warn("cdev NULL.\n");
		return 0;
	}
	if (cdev->magic != MAGIC_CHAR) {
		pr_warn("cdev 0x%p magic mismatch 0x%lx\n", cdev, cdev->magic);
		return 0;
	}
	BUG_ON(!cdev->xdev);
	BUG_ON(!g_xdma_class);
	BUG_ON(!cdev->sys_device);

	if (cdev->sys_device)
		device_destroy(g_xdma_class, cdev->cdevno);

	cdev_del(&cdev->cdev);
	//unregister_chrdev_region(cdev->cdevno, 1);

	return 0;
}

static int create_xcdev(struct xdma_pci_dev *xpdev, struct xdma_cdev *xcdev,
			int bar, struct xdma_engine *engine,
			enum cdev_type type)
{
	int rv;
	int minor;
	const struct file_operations *fops;
	struct xdma_dev *xdev = xpdev->xdev;
	dev_t dev;

	spin_lock_init(&xcdev->lock);
	/* new instance? */
	if (!xpdev->major) {
		/* allocate a dynamically allocated char device node */
		int rv = alloc_chrdev_region(&dev, XDMA_MINOR_BASE,
					XDMA_MINOR_COUNT, DRV_MODULE_NAME);

		if (rv) {
			pr_err("unable to allocate cdev region %d.\n", rv);
			return rv;
		}
		xpdev->major = MAJOR(dev);
pr_info("major %d.\n", xpdev->major);
	}

	switch (type) {
	case CHAR_XDMA_H2C:
		minor = 32 + engine->channel;
		break;
	case CHAR_XDMA_C2H:
		minor = 36 + engine->channel;
		break;
	case CHAR_EVENTS:
		minor = 10 + bar;
		break;
	default:
		/* minor number is type index for non-SGDMA interfaces */
		minor = type;
		break;
	}

	xcdev->cdevno = MKDEV(xpdev->major, minor);

	/*
	 * do not register yet, create kobjects and name them,
	 * re-use the name during single-minor registration
	 */

	fops = select_file_ops(type);
	if (!fops) {
		pr_err("type 0x%x, file interface selection failed\n", type);
		return -EINVAL;
	}

	cdev_init(&xcdev->cdev, fops);

	xcdev->magic = MAGIC_CHAR;
	xcdev->cdev.owner = THIS_MODULE;
	xcdev->xpdev = xpdev;
	xcdev->xdev = xdev;
	xcdev->engine = engine;
	xcdev->bar = bar;

	if (type == CHAR_EVENTS)
		xcdev->user_irq = &xdev->user_irq[bar];

	rv = config_kobject(xcdev, type);
	if (rv < 0)
		return rv;

	/* bring character device live */
	rv = cdev_add(&xcdev->cdev, xcdev->cdevno, XDMA_MINOR_COUNT);
	if (rv < 0) {
		pr_err("cdev_add failed %d, type 0x%x.\n", rv, type);
		goto unregister_region;
	}

	/* create device on our class */
	if (g_xdma_class) {
		rv = create_sys_device(xcdev, type);
		if (rv < 0)
			goto del_cdev;
	}

	return 0;

del_cdev:
	cdev_del(&xcdev->cdev);
unregister_region:
	unregister_chrdev_region(dev, XDMA_MINOR_COUNT);
	return rv;
}

static void destroy_interfaces(struct xdma_pci_dev *xpdev)
{
	int i;

	if (xpdev_flag_test(xpdev, XDF_CDEV_SG)) {
		/* iterate over channels */
		for (i = 0; i < xpdev->channel_max; i++) {
			/* remove SG DMA character device */
			destroy_xcdev(&xpdev->sgdma_c2h_cdev[i]);
			destroy_xcdev(&xpdev->sgdma_h2c_cdev[i]);
		}
	}

	if (xpdev_flag_test(xpdev, XDF_CDEV_EVENT)) {
		for (i = 0; i < xpdev->user_max; i++)
			destroy_xcdev(&xpdev->events_cdev[i]);
	}

	/* remove control character device */
	if (xpdev_flag_test(xpdev, XDF_CDEV_CTRL)) {
		destroy_xcdev(&xpdev->ctrl_cdev);
	}

	/* remove user character device */
	if (xpdev_flag_test(xpdev, XDF_CDEV_USER)) {
		destroy_xcdev(&xpdev->user_cdev);
	}

	if (xpdev->major)
		unregister_chrdev_region(MKDEV(xpdev->major, XDMA_MINOR_BASE), XDMA_MINOR_COUNT);
}

static int create_interfaces(struct xdma_pci_dev *xpdev)
{
	struct xdma_dev *xdev = xpdev->xdev;
	struct xdma_engine *engine;
	int i;
	int rv = 0;

	/* initialize user character device */
	if (xdev->user_bar_idx >= 0) {
		rv = create_xcdev(xpdev, &xpdev->user_cdev, xdev->user_bar_idx,
			NULL, CHAR_USER);
		if (rv < 0) {
			pr_err("create_char(user_cdev) failed\n");
			goto fail;
		}
		xpdev_flag_set(xpdev, XDF_CDEV_USER);
	}

	/* initialize control character device */
	rv = create_xcdev(xpdev, &xpdev->ctrl_cdev, xdev->config_bar_idx,
			NULL, CHAR_CTRL);
	if (rv < 0) {
		pr_err("create_char(ctrl_cdev) failed\n");
		goto fail;
	}
	xpdev_flag_set(xpdev, XDF_CDEV_CTRL);

	/* initialize events character device */
	for (i = 0; i < xpdev->user_max; i++) {
		rv = create_xcdev(xpdev, &xpdev->events_cdev[i], i, NULL,
			CHAR_EVENTS);
		if (rv < 0) {
			pr_err("create char event %d failed, %d.\n", i, rv);
			goto fail;
		}
	}
	xpdev_flag_set(xpdev, XDF_CDEV_EVENT);

	/* iterate over channels */
	for (i = 0; i < xpdev->channel_max; i++) {
		engine = &xdev->engine_h2c[i];

		if (engine->magic != MAGIC_ENGINE)
			continue;

		rv = create_xcdev(xpdev, &xpdev->sgdma_h2c_cdev[i], i, engine,
				 CHAR_XDMA_H2C);
		if (rv < 0) {
			pr_err("create char h2c %d failed, %d.\n", i, rv);
			goto fail;
		}
	}

	for (i = 0; i < xpdev->channel_max; i++) {
		engine = &xdev->engine_c2h[i];

		if (engine->magic != MAGIC_ENGINE)
			continue;

		rv = create_xcdev(xpdev, &xpdev->sgdma_c2h_cdev[i], i, engine,
				 CHAR_XDMA_C2H);
		if (rv < 0) {
			pr_err("create char c2h %d failed, %d.\n", i, rv);
			goto fail;
		}
	}
	xpdev_flag_set(xpdev, XDF_CDEV_SG);

	/* ??? Bypass */
	return 0;

fail:
	rv = -1;
	destroy_interfaces(xpdev);
	return rv;
}

static void xpdev_free(struct xdma_pci_dev *xpdev)
{
	struct xdma_dev *xdev = xpdev->xdev;

	pr_err("xpdev 0x%p, destroy_interfaces, xdev 0x%p.\n", xpdev, xdev);
	destroy_interfaces(xpdev);
	xpdev->xdev = NULL;
	pr_err("xpdev 0x%p, xdev 0x%p xdma_device_close.\n", xpdev, xdev);
	xdma_device_close(xpdev->pdev, xdev);
	xpdev_cnt--;

	kfree(xpdev);
}

static struct xdma_pci_dev *xpdev_alloc(struct pci_dev *pdev)
{
	struct xdma_pci_dev *xpdev = kmalloc(sizeof(*xpdev), GFP_KERNEL);	

	if (!xpdev)
		return NULL;
	memset(xpdev, 0, sizeof(*xpdev));

	xpdev->magic = MAGIC_DEVICE;
	xpdev->pdev = pdev;
	xpdev->user_max = MAX_USER_IRQ;
	xpdev->channel_max = XDMA_CHANNEL_NUM_MAX;

	xpdev_cnt++;
	return xpdev;
}

static int probe_one(struct pci_dev *pdev, const struct pci_device_id *id)
{
	int rv = 0;
	struct xdma_pci_dev *xpdev = NULL;
	struct xdma_dev *xdev;
	void *hndl;

#if 0
	pr_info("device 0x%x, %02x:%02x.%x, id 0x%x.\n",
		pdev->devfn,
		PCI_BUS_NUM(pdev->devfn), PCI_SLOT(pdev->devfn),
		PCI_FUNC(pdev->devfn), id->device);
#endif

	xpdev = xpdev_alloc(pdev);
	if (!xpdev)
		return -ENOMEM;

	hndl = xdma_device_open(DRV_MODULE_NAME, pdev,
				&xpdev->user_max, &xpdev->channel_max);
	if (!hndl)
		return -EINVAL;
	pr_info("pdev 0x%p, xdev 0x%p, 0x%p, user %d, channel %d.\n",
		pdev, xpdev, hndl, xpdev->user_max, xpdev->channel_max);

	BUG_ON(xpdev->user_max > MAX_USER_IRQ);
	BUG_ON(xpdev->channel_max > XDMA_CHANNEL_NUM_MAX);

	if (!xpdev->channel_max)
		pr_warn("NO engine found!\n");

	{
		xdev = xdev_find_by_pdev(pdev);
		if (!xdev) {
			pr_warn("NO xdev found!\n");
			return -EINVAL;
		}
		BUG_ON(hndl != xdev );
	}
	xpdev->xdev = hndl;

	rv = create_interfaces(xpdev);
	if (rv)
		goto err_out;

	rv = device_create_file(&pdev->dev, &dev_attr_xdma_dev_instance);
	if(rv){
		pr_err("Failed to create device file \n");
		goto err_out;
	}

        dev_set_drvdata(&pdev->dev, xpdev);

	return 0;

err_out:	
	pr_err("pdev 0x%p, err %d.\n", pdev, rv);
	xpdev_free(xpdev);
	return rv;
}

static void remove_one(struct pci_dev *pdev)
{
	struct xdma_pci_dev *xpdev;

	if (!pdev)
		return;

	xpdev = dev_get_drvdata(&pdev->dev);
	if (!xpdev)
		return;

	pr_info("pdev 0x%p, xdev 0x%p, 0x%p.\n",
		pdev, xpdev, xpdev->xdev);
	xpdev_free(xpdev);

        dev_set_drvdata(&pdev->dev, NULL);
	device_remove_file(&pdev->dev, &dev_attr_xdma_dev_instance);
}

/*
 * character device file operations for SG DMA engine
 */
static int char_sgdma_close(struct inode *inode, struct file *file)
{
	struct xdma_cdev *xcdev = (struct xdma_cdev *)file->private_data;
        struct xdma_engine *engine;

	BUG_ON(!xcdev);
	BUG_ON(xcdev->magic != MAGIC_CHAR);

	engine = xcdev->engine;
        BUG_ON(!engine);
        BUG_ON(engine->magic != MAGIC_ENGINE);

	return 0;
}

static int char_sgdma_open(struct inode *inode, struct file *file)
{
	struct xdma_cdev *xcdev;
        struct xdma_engine *engine;

	/* pointer to containing structure of the character device inode */
	xcdev = container_of(inode->i_cdev, struct xdma_cdev, cdev);
	BUG_ON(xcdev->magic != MAGIC_CHAR);
	/* create a reference to our char device in the opened file */
	file->private_data = xcdev;

	engine = xcdev->engine;
        BUG_ON(!engine);
        BUG_ON(engine->magic != MAGIC_ENGINE);

	return 0;
}

static loff_t char_sgdma_llseek(struct file *file, loff_t off, int whence)
{
	loff_t newpos = 0;

	switch (whence) {
	case 0: /* SEEK_SET */
		newpos = off;
		break;
	case 1: /* SEEK_CUR */
		newpos = file->f_pos + off;
		break;
	case 2: /* SEEK_END, @TODO should work from end of address space */
		newpos = UINT_MAX + off;
		break;
	default: /* can't happen */
		return -EINVAL;
	}
	if (newpos < 0)
		return -EINVAL;
	file->f_pos = newpos;
	dbg_fops("char_sgdma_llseek: pos=%lld\n", (signed long long)newpos);

#if 0
	pr_err("0x%p, off 0x%lld, whence %d -> pos %lld.\n",
		file, (signed long long)off, whence, (signed long long)off);
#endif

	return newpos;
}

/* char_sgdma_read_write() -- Read from or write to the device
 *
 * @buf userspace buffer
 * @count number of bytes in the userspace buffer
 * @pos byte-address in device
 * @dir_to_device If !0, a write to the device is performed
 *
 * Iterate over the userspace buffer, taking at most 255 * PAGE_SIZE bytes for
 * each DMA transfer.
 *
 * For each transfer, get the user pages, build a sglist, map, build a
 * descriptor table. submit the transfer. wait for the interrupt handler
 * to wake us on completion.
 */

static int check_transfer_align(struct xdma_engine *engine,
	const char __user *buf, size_t count, loff_t pos, int sync)
{
	BUG_ON(!engine);

	/* AXI ST or AXI MM non-incremental addressing mode? */
	if (engine->non_incr_addr) {
		int buf_lsb = (int)((uintptr_t)buf) & (engine->addr_align - 1);
		size_t len_lsb = count & ((size_t)engine->len_granularity - 1);
		int pos_lsb = (int)pos & (engine->addr_align - 1);

		dbg_tfr("AXI ST or MM non-incremental\n");
		dbg_tfr("buf_lsb = %d, pos_lsb = %d, len_lsb = %ld\n", buf_lsb,
			pos_lsb, len_lsb);

		if (buf_lsb != 0) {
			dbg_tfr("FAIL: non-aligned buffer address %p\n", buf);
			return -EINVAL;
		}

		if ((pos_lsb != 0) && (sync)) {
			dbg_tfr("FAIL: non-aligned AXI MM FPGA addr 0x%llx\n",
				(unsigned long long)pos);
			return -EINVAL;
		}

		if (len_lsb != 0) {
			dbg_tfr("FAIL: len %d is not a multiple of %d\n",
				(int)count,
				(int)engine->len_granularity);
			return -EINVAL;
		}
		/* AXI MM incremental addressing mode */
	} else {
		int buf_lsb = (int)((uintptr_t)buf) & (engine->addr_align - 1);
		int pos_lsb = (int)pos & (engine->addr_align - 1);

		if (buf_lsb != pos_lsb) {
			dbg_tfr("FAIL: Misalignment error\n");
			dbg_tfr("host addr %p, FPGA addr 0x%llx\n", buf, pos);
			return -EINVAL;
		}
	}

	return 0;
}

/*
 * Map a user memory range into a scatterlist
 * inspired by vhost_scsi_map_to_sgl()
 * Returns the number of scatterlist entries used or -errno on error.
 */
static inline void xdma_io_cb_release(struct xdma_io_cb *cb)
{
	int i;

	for (i = 0; i < cb->pages_nr; i++)
		put_page(cb->pages[i]);

	sg_free_table(&cb->sgt);
	kfree(cb->pages);

	memset(cb, 0, sizeof(*cb));
}

static void char_sgdma_unmap_user_buf(struct xdma_io_cb *cb)
{
	int i;

	sg_free_table(&cb->sgt);

	if (!cb->pages || !cb->pages_nr)
		return;

	for (i = 0; i < cb->pages_nr; i++) {
		if (cb->pages[i])
			put_page(cb->pages[i]);
		else
			break;
	}

	if (i != cb->pages_nr)
		pr_info("sgl pages %d/%u.\n", i, cb->pages_nr);

	kfree(cb->pages);
	cb->pages = NULL;
}

static int char_sgdma_map_user_buf_to_sgl(struct xdma_io_cb *cb, int write)
{
	struct sg_table *sgt = &cb->sgt;
	unsigned long len = cb->len;
	char *buf = cb->buf;
	struct scatterlist *sg;
	unsigned int pages_nr = (((unsigned long)buf + len + PAGE_SIZE -1) -
				 ((unsigned long)buf & PAGE_MASK))
				>> PAGE_SHIFT;
	int i;
	int rv;

	if (pages_nr == 0) {
		return -EINVAL;
	}

	if (sg_alloc_table(sgt, pages_nr, GFP_KERNEL)) {
		pr_err("sgl OOM.\n");
		return -ENOMEM;
	}

	cb->pages = kcalloc(pages_nr, sizeof(struct page *), GFP_KERNEL);
	if (!cb->pages) {
		pr_err("pages OOM.\n");
		rv = -ENOMEM;
		goto err_out;
	}

	rv = get_user_pages_fast((unsigned long)buf, pages_nr, 1/* write */,
				cb->pages);
	/* No pages were pinned */
	if (rv < 0) {
		pr_err("unable to pin down %u user pages, %d.\n",
			pages_nr, rv);
		goto err_out;
	}
	/* Less pages pinned than wanted */
	if (rv != pages_nr) {
		pr_err("unable to pin down all %u user pages, %d.\n",
			pages_nr, rv);
		rv = -EFAULT;
		cb->pages_nr = rv;
		goto err_out;
	}

	for (i = 1; i < pages_nr; i++) {
		if (cb->pages[i - 1] == cb->pages[i]) {
			pr_err("duplicate pages, %d, %d.\n",
				i - 1, i);
			rv = -EFAULT;
			cb->pages_nr = pages_nr;
			goto err_out;
		}
	}

	sg = sgt->sgl;
	for (i = 0; i < pages_nr; i++, sg = sg_next(sg)) {
		//unsigned int offset = (uintptr_t)buf & ~PAGE_MASK;
		unsigned int offset = offset_in_page(buf);
		unsigned int nbytes = min_t(unsigned int, PAGE_SIZE - offset, len);

		flush_dcache_page(cb->pages[i]);
		sg_set_page(sg, cb->pages[i], nbytes, offset);
//pr_err("sg %d,0x%p, off %u, len %u, page 0x%p.\n", i, sg, offset, nbytes, cb->pages[i]);
		buf += nbytes;
		len -= nbytes;
	}

	BUG_ON(len);
	cb->pages_nr = pages_nr;
	return 0;

err_out:
	char_sgdma_unmap_user_buf(cb);

	return rv;
}

static ssize_t char_sgdma_read_write(struct file *file, char __user *buf,
		size_t count, loff_t *pos, int write)
{
	int rv;
	ssize_t res = 0;
	struct xdma_cdev *xcdev;
	struct xdma_dev *xdev;
	struct xdma_engine *engine;
	struct xdma_io_cb cb;

	/* fetch device specific data stored earlier during open */
	xcdev = (struct xdma_cdev *)file->private_data;
	BUG_ON(!xcdev);
	BUG_ON(xcdev->magic != MAGIC_CHAR);

	xdev = xcdev->xdev;
	BUG_ON(!xdev);
	BUG_ON(xdev->magic != MAGIC_DEVICE);

	engine = xcdev->engine;
	BUG_ON(!engine);
	BUG_ON(engine->magic != MAGIC_ENGINE);

	dbg_tfr("file 0x%p, priv 0x%p, buf 0x%p,%llu, pos %llu, W %d, %s.\n",
		file, file->private_data, buf, (u64)count, (u64)*pos, write,
		engine->name);

	if ((write && engine->dir != DMA_TO_DEVICE) ||
	    (!write && engine->dir != DMA_FROM_DEVICE)) {
		pr_err("r/w mismatch. W %d, dir %d.\n",
			write, engine->dir);
		return -EINVAL;
	}

	rv = check_transfer_align(engine, buf, count, *pos, 1);
	if (rv) {
		pr_info("Invalid transfer alignment detected\n");
		return rv;
	}

	memset(&cb, 0, sizeof(struct xdma_io_cb));
	cb.buf = buf;
	cb.len = count;
	rv = char_sgdma_map_user_buf_to_sgl(&cb, write);
	if (rv < 0)
		return rv;

	res = xdma_xfer_submit(xdev, engine->channel, write, *pos, &cb.sgt,
				0, 10000);	
	//pr_err("xfer_submit return=%lld.\n", (s64)res);

	//interrupt_status(xdev);

	char_sgdma_unmap_user_buf(&cb);

	return res;
}

static ssize_t char_sgdma_write(struct file *file, const char __user *buf,
                size_t count, loff_t *pos)
{
        return char_sgdma_read_write(file, (char *)buf, count, pos, 1);
}

static ssize_t char_sgdma_read(struct file *file, char __user *buf,
		size_t count, loff_t *pos)
{
        return char_sgdma_read_write(file, (char *)buf, count, pos, 0);
}

static const struct file_operations sg_interrupt_fops = {
	.owner = THIS_MODULE,
	.open = char_sgdma_open,
	.release = char_sgdma_close,
	.write = char_sgdma_write,
	.read = char_sgdma_read,
	//.unlocked_ioctl = char_sgdma_ioctl,
	.llseek = char_sgdma_llseek,
};

static int char_open(struct inode *inode, struct file *file)
{
	struct xdma_cdev *xcdev;

	/* pointer to containing structure of the character device inode */
	xcdev = container_of(inode->i_cdev, struct xdma_cdev, cdev);
	BUG_ON(xcdev->magic != MAGIC_CHAR);
	/* create a reference to our char device in the opened file */
	file->private_data = xcdev;

	return 0;
}

/*
 * Called when the device goes from used to unused.
 */
static int char_close(struct inode *inode, struct file *file)
{
	struct xdma_dev *xdev;
	struct xdma_cdev *xcdev = (struct xdma_cdev *)file->private_data;

	BUG_ON(!xcdev);
	BUG_ON(xcdev->magic != MAGIC_CHAR);

	/* fetch device specific data stored earlier during open */
	xdev = xcdev->xdev;
	BUG_ON(!xdev);
	BUG_ON(xdev->magic != MAGIC_DEVICE);

	return 0;
}

static ssize_t char_ctrl_read(struct file *file, char __user *buf, size_t count,
		loff_t *pos)
{
	u32 w;
	int ret;
	void *reg;
	struct xdma_dev *xdev;
	struct xdma_cdev *cdev = (struct xdma_cdev *)file->private_data;

	BUG_ON(!cdev);
	BUG_ON(cdev->magic != MAGIC_CHAR);
	xdev = cdev->xdev;
	BUG_ON(!xdev);
	BUG_ON(xdev->magic != MAGIC_DEVICE);

	/* only 32-bit aligned and 32-bit multiples */
	if (*pos & 3)
		return -EPROTO;
	/* first address is BAR base plus file position offset */
	reg = xdev->bar[cdev->bar] + *pos;
	//w = read_register(reg);
	w = ioread32(reg);
	dbg_sg("char_ctrl_read(@%p, count=%ld, pos=%d) value = 0x%08x\n", reg,
		(long)count, (int)*pos, w);
	ret = copy_to_user(buf, &w, 4);
	if (ret)
		dbg_sg("Copy to userspace failed but continuing\n");

	*pos += 4;
	return 4;
}

static ssize_t char_ctrl_write(struct file *file, const char __user *buf,
			size_t count, loff_t *pos)
{
	u32 w;
	int ret;
	void *reg;
	struct xdma_dev *xdev;
	struct xdma_cdev *cdev = (struct xdma_cdev *)file->private_data;

	BUG_ON(!cdev);
	BUG_ON(cdev->magic != MAGIC_CHAR);
	xdev = cdev->xdev;
	BUG_ON(!xdev);
	BUG_ON(xdev->magic != MAGIC_DEVICE);

	/* only 32-bit aligned and 32-bit multiples */
	if (*pos & 3)
		return -EPROTO;

	/* first address is BAR base plus file position offset */
	reg = xdev->bar[cdev->bar] + *pos;
	ret = copy_from_user(&w, buf, 4);
	if (ret)
		dbg_sg("Copy from user failed but continuing\n");

	dbg_sg("char_ctrl_write(0x%08x @%p, count=%ld, pos=%d)\n", w, reg,
		(long)count, (int)*pos);
	//write_register(w, reg);
	iowrite32(w, reg);
	*pos += 4;
	return 4;
}

static long version_ioctl(struct xdma_cdev *xcdev, void __user *arg)
{
	struct xdma_ioc_info obj;
	struct xdma_dev *xdev = xcdev->xdev;

	if (copy_from_user((void *)&obj, arg, sizeof(struct xdma_ioc_info)))
		return -EFAULT;
	memset(&obj, 0, sizeof(obj));
	obj.vendor = xdev->pdev->vendor;
	obj.device = xdev->pdev->device;
	obj.subsystem_vendor = xdev->pdev->subsystem_vendor;
	obj.subsystem_device = xdev->pdev->subsystem_device;
	obj.feature_id = xdev->feature_id;
	obj.driver_version = XDMA_DRIVER_MAJOR * 1000 + XDMA_DRIVER_MINOR * 100 + XDMA_DRIVER_PATCHLEVEL;
	if (copy_to_user(arg, &obj, sizeof(struct xdma_ioc_info)))
		return -EFAULT;
	return 0;
}

long char_ctrl_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
	struct xdma_dev *xdev;
	struct xdma_cdev *xcdev = (struct xdma_cdev *)filp->private_data;
	struct xdma_ioc_base ioctl_obj;
	long result = 0;

	BUG_ON(!xcdev);
	BUG_ON(xcdev->magic != MAGIC_CHAR);
	xdev = xcdev->xdev;
	BUG_ON(!xdev);
	BUG_ON(xdev->magic != MAGIC_DEVICE);

	pr_err("IOCTL Command: %x\n", cmd);

	if (_IOC_TYPE(cmd) != XDMA_IOC_MAGIC)
		return -ENOTTY;

	if (_IOC_DIR(cmd) & _IOC_READ)
		result = !access_ok(VERIFY_WRITE, (void __user *)arg, _IOC_SIZE(cmd));
	else if (_IOC_DIR(cmd) & _IOC_WRITE)
		result =  !access_ok(VERIFY_READ, (void __user *)arg, _IOC_SIZE(cmd));

	if (result)
		return -EFAULT;

	if (copy_from_user((void *)&ioctl_obj, (void *) arg,
			 sizeof(struct xdma_ioc_base))) {
		pr_err("copy_from_user failed.\n");
		return -EFAULT;
	}
	if (ioctl_obj.magic != XDMA_XCL_MAGIC) {
		pr_err("magic 0x%x !=  XDMA_XCL_MAGIC (0x%x).\n", ioctl_obj.magic, XDMA_XCL_MAGIC);
		return -ENOTTY;
	}

	switch (cmd) {
	case XDMA_IOCINFO:
		return version_ioctl(xcdev, (void __user *)arg);
	default:
		pr_err("UNKNOWN ioctl cmd 0x%x.\n", cmd);
		return -ENOTTY;
	}
	return 0;
}

/* maps the PCIe BAR into user space for memory-like access using mmap() */
static int bridge_mmap(struct file *file, struct vm_area_struct *vma)
{
	int rc;
	struct xdma_dev *xdev;
	struct xdma_cdev *xcdev = (struct xdma_cdev *)file->private_data;
	unsigned long off;
	unsigned long phys;
	unsigned long vsize;
	unsigned long psize;

	BUG_ON(!xcdev);
	BUG_ON(xcdev->magic != MAGIC_CHAR);
	xdev = xcdev->xdev;
	BUG_ON(!xdev);
	BUG_ON(xdev->magic != MAGIC_DEVICE);

	off = vma->vm_pgoff << PAGE_SHIFT;
	/* BAR physical address */
	phys = pci_resource_start(xdev->pdev, xcdev->bar) + off;
	vsize = vma->vm_end - vma->vm_start;
	/* complete resource */
	psize = pci_resource_end(xdev->pdev, xcdev->bar) -
		pci_resource_start(xdev->pdev, xcdev->bar) + 1 - off;

	dbg_sg("mmap(): xcdev = 0x%08lx\n", (unsigned long)xcdev);
	dbg_sg("mmap(): cdev->bar = %d\n", xcdev->bar);
	dbg_sg("mmap(): xdev = 0x%p\n", xdev);
	dbg_sg("mmap(): pci_dev = 0x%08lx\n", (unsigned long)xdev->pdev);

	dbg_sg("off = 0x%lx\n", off);
	dbg_sg("start = 0x%llx\n",
		(unsigned long long)pci_resource_start(xdev->pdev,
		xcdev->bar));
	dbg_sg("phys = 0x%lx\n", phys);

	if (vsize > psize)
		return -EINVAL;
	/*
	 * pages must not be cached as this would result in cache line sized
	 * accesses to the end point
	 */
	vma->vm_page_prot = pgprot_noncached(vma->vm_page_prot);
	/*
	 * prevent touching the pages (byte access) for swap-in,
	 * and prevent the pages from being swapped out
	 */
	vma->vm_flags |= VMEM_FLAGS;
	/* make MMIO accessible to user space */
	rc = io_remap_pfn_range(vma, vma->vm_start, phys >> PAGE_SHIFT,
			vsize, vma->vm_page_prot);
	dbg_sg("vma=0x%p, vma->vm_start=0x%lx, phys=0x%lx, size=%lu = %d\n",
		vma, vma->vm_start, phys >> PAGE_SHIFT, vsize, rc);

	if (rc)
		return -EAGAIN;
	return 0;
}

/*
 * character device file operations for control bus (through control bridge)
 */
static const struct file_operations ctrl_fops = {
	.owner = THIS_MODULE,
	.open = char_open,
	.release = char_close,
	.read = char_ctrl_read,
	.write = char_ctrl_write,
	.mmap = bridge_mmap,
	.unlocked_ioctl = char_ctrl_ioctl,
};


/*
 * character device file operations for events
 */
static ssize_t char_events_read(struct file *file, char __user *buf,
		size_t count, loff_t *pos)
{
	int rc;
	int ret;
	struct xdma_user_irq *user_irq;
	struct xdma_cdev *xcdev = (struct xdma_cdev *)file->private_data;
	u32 events_user;
	unsigned long flags;

	BUG_ON(!xcdev);
	BUG_ON(xcdev->magic != MAGIC_CHAR);
	user_irq = xcdev->user_irq;
	BUG_ON(!user_irq);

	if (count != 4)
		return -EPROTO;

	if (*pos & 3)
		return -EPROTO;

	/*
	 * sleep until any interrupt events have occurred,
	 * or a signal arrived
	 */
	rc = wait_event_interruptible(user_irq->events_wq,
			user_irq->events_irq != 0);
	if (rc)
		dbg_sg("wait_event_interruptible=%d\n", rc);

	/* wait_event_interruptible() was interrupted by a signal */
	if (rc == -ERESTARTSYS)
		return -ERESTARTSYS;

	/* atomically decide which events are passed to the user */
	spin_lock_irqsave(&user_irq->events_lock, flags);
	events_user = user_irq->events_irq;
	user_irq->events_irq = 0;
	spin_unlock_irqrestore(&user_irq->events_lock, flags);

	ret = copy_to_user(buf, &events_user, 4);
	if (ret)
		dbg_sg("Copy to user failed but continuing\n");

	return 4;
}

static unsigned int char_events_poll(struct file *file, poll_table *wait)
{
	struct xdma_user_irq *user_irq;
	struct xdma_cdev *xcdev = (struct xdma_cdev *)file->private_data;
	unsigned long flags;
	unsigned int mask = 0;

	BUG_ON(!xcdev);
	BUG_ON(xcdev->magic != MAGIC_CHAR);
	user_irq = xcdev->user_irq;
	BUG_ON(!user_irq);

	poll_wait(file, &user_irq->events_wq,  wait);

	spin_lock_irqsave(&user_irq->events_lock, flags);
	if (user_irq->events_irq)
		mask = POLLIN | POLLRDNORM;	/* readable */

	spin_unlock_irqrestore(&user_irq->events_lock, flags);

	return mask;
}

/*
 * character device file operations for the irq events
 */
static const struct file_operations events_fops = {
	.owner = THIS_MODULE,
	.open = char_open,
	.release = char_close,
	.read = char_events_read,
	.poll = char_events_poll,
};

static const struct file_operations *select_file_ops(enum cdev_type type)
{
	const struct file_operations *fops;

	switch (type) {
	case CHAR_XDMA_H2C:
	case CHAR_XDMA_C2H:
		fops = &sg_interrupt_fops;
		break;

	case CHAR_USER:
	case CHAR_CTRL:
		fops = &ctrl_fops;
		break;

	case CHAR_EVENTS:
		fops = &events_fops;
		break;

	default:
		dbg_init("Invalid device type specified\n");
		fops = NULL;
		break;
	}

	return fops;
}

static pci_ers_result_t xdma_error_detected(struct pci_dev *pdev,
					pci_channel_state_t state)
{
	struct xdma_pci_dev *xpdev = dev_get_drvdata(&pdev->dev);

	switch (state) {
	case pci_channel_io_normal:
		return PCI_ERS_RESULT_CAN_RECOVER;
	case pci_channel_io_frozen:
		pr_warn("dev 0x%p,0x%p, frozen state error, reset controller\n",
			pdev, xpdev);
		//xdma_dev_disable(xpdev, false);
		return PCI_ERS_RESULT_NEED_RESET;
	case pci_channel_io_perm_failure:
		pr_warn("dev 0x%p,0x%p, failure state error, req. disconnect\n",
			pdev, xpdev);
		return PCI_ERS_RESULT_DISCONNECT;
	}
	return PCI_ERS_RESULT_NEED_RESET;
}

static pci_ers_result_t xdma_slot_reset(struct pci_dev *pdev)
{
	struct xdma_pci_dev *xpdev = dev_get_drvdata(&pdev->dev);

	pr_info("0x%p restart after slot reset\n", xpdev);
	pci_restore_state(pdev);
	//queue_work(xdma_workq, &dev->reset_work);
	return PCI_ERS_RESULT_RECOVERED;
}

static void xdma_error_resume(struct pci_dev *pdev)
{
	struct xdma_pci_dev *xpdev = dev_get_drvdata(&pdev->dev);

	pr_info("dev 0x%p,0x%p.\n", pdev, xpdev);
	pci_cleanup_aer_uncorrect_error_status(pdev);
}

#if LINUX_VERSION_CODE >= KERNEL_VERSION(3,16,0)
static void xdma_reset_notify(struct pci_dev *pdev, bool prepare)
{
	struct xdma_pci_dev *xpdev = dev_get_drvdata(&pdev->dev);

	pr_info("dev 0x%p,0x%p, prepare %d.\n", pdev, xpdev, prepare);

	if (prepare)
		xdma_device_offline(pdev, xpdev->xdev);
	else
		xdma_device_online(pdev, xpdev->xdev);
}
#endif

static const struct pci_error_handlers xdma_err_handler = {
	.error_detected	= xdma_error_detected,
	.slot_reset	= xdma_slot_reset,
	.resume		= xdma_error_resume,
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3,16,0)
	.reset_notify	= xdma_reset_notify,
#endif
};

static struct pci_driver pci_driver = {
	.name = DRV_MODULE_NAME,
	.id_table = pci_ids,
	.probe = probe_one,
	.remove = remove_one,
	.err_handler = &xdma_err_handler,
};

static int __init xdma_mod_init(void)
{
	printk(KERN_INFO "%s", version);

	g_xdma_class = class_create(THIS_MODULE, DRV_MODULE_NAME);
	if (IS_ERR(g_xdma_class)) {
		dbg_init(DRV_MODULE_NAME ": failed to create class");
		return -1;
	}

	return pci_register_driver(&pci_driver);
}

static void __exit xdma_mod_exit(void)
{
	/* unregister this driver from the PCI bus driver */
	dbg_init("pci_unregister_driver.\n");
	pci_unregister_driver(&pci_driver);
	if (g_xdma_class)
		class_destroy(g_xdma_class);
}

module_init(xdma_mod_init);
module_exit(xdma_mod_exit);
