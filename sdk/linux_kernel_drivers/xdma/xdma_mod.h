#ifndef __XDMA_MODULE_H__
#define __XDMA_MODULE_H__

#include <linux/types.h>
#include <linux/module.h>
#include <linux/cdev.h>
#include <linux/dma-mapping.h>
#include <linux/delay.h>
#include <linux/fb.h>
#include <linux/fs.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/io.h>
#include <linux/jiffies.h>
#include <linux/kernel.h>
#include <linux/mm.h>
#include <linux/mm_types.h>
#include <linux/poll.h>
#include <linux/pci.h>
#include <linux/sched.h>
#include <linux/slab.h>
#include <linux/vmalloc.h>
#include <linux/workqueue.h>
#include <linux/aio.h>
#include <linux/splice.h>
#include <linux/version.h>
#include <linux/uio.h>

#define XDMA_MINOR_BASE (0)
#define XDMA_MINOR_COUNT (255)

#define MAGIC_ENGINE	0xEEEEEEEEUL
#define MAGIC_DEVICE	0xDDDDDDDDUL
#define MAGIC_CHAR	0xCCCCCCCCUL
#define MAGIC_BITSTREAM 0xBBBBBBBBUL

/* SECTION: Enum definitions */
enum cdev_type {
	CHAR_USER,
	CHAR_CTRL,
	CHAR_EVENTS,
	CHAR_XDMA_H2C,
	CHAR_XDMA_C2H,
	CHAR_BYPASS_H2C,
	CHAR_BYPASS_C2H,
	CHAR_BYPASS
};

struct xdma_cdev {
	unsigned long magic;		/* structure ID for sanity checks */
	struct xdma_pci_dev *xpdev;
	struct xdma_dev *xdev;
	dev_t cdevno;			/* character device major:minor */
	struct cdev cdev;		/* character device embedded struct */
	int bar;			/* PCIe BAR for HW access, if needed */
	struct xdma_engine *engine;	/* engine instance, if needed */
	struct xdma_user_irq *user_irq;	/* IRQ value, if needed */
	struct device *sys_device;	/* sysfs device */
	spinlock_t lock;
};

enum xpdev_flags_bits {
	XDF_CDEV_USER,
	XDF_CDEV_CTRL,
	XDF_CDEV_EVENT,
	XDF_CDEV_SG,
	XDF_CDEV_BYPASS,
};

/* XDMA PCIe device specific book-keeping */
struct xdma_pci_dev {
	unsigned long magic;		/* structure ID for sanity checks */
	struct pci_dev *pdev;	/* pci device struct from probe() */
	struct xdma_dev *xdev;
	int major;		/* major number */
	int instance;		/* instance number */
	int user_max;
	int channel_max;

	unsigned int flags;
	/* character device structures */
	struct xdma_cdev ctrl_cdev;
	struct xdma_cdev sgdma_c2h_cdev[XDMA_CHANNEL_NUM_MAX];
	struct xdma_cdev sgdma_h2c_cdev[XDMA_CHANNEL_NUM_MAX];
	struct xdma_cdev events_cdev[16];

	struct xdma_cdev user_cdev;
	struct xdma_cdev bypass_c2h_cdev[XDMA_CHANNEL_NUM_MAX];
	struct xdma_cdev bypass_h2c_cdev[XDMA_CHANNEL_NUM_MAX];
	struct xdma_cdev bypass_cdev_base;

	void *data;
};

static inline void xpdev_flag_set(struct xdma_pci_dev *xpdev,
				enum xpdev_flags_bits fbit)
{
	xpdev->flags |= 1 << fbit;
}

static inline void xcdev_flag_clear(struct xdma_pci_dev *xpdev,
				enum xpdev_flags_bits fbit)
{
	xpdev->flags &= ~(1 << fbit);
}

static inline int xpdev_flag_test(struct xdma_pci_dev *xpdev,
				enum xpdev_flags_bits fbit)
{
	return xpdev->flags & (1 << fbit);
}

struct xdma_io_cb {
	void __user *buf;
	size_t len;
	unsigned int pages_nr;
	struct sg_table sgt;
	struct page **pages;
};

#endif /* ifndef __XDMA_MODULE_H__ */
