
#ifndef _AWS_MGT_PF_H_
#define _AWS_MGT_PF_H_

#include <linux/cdev.h>
#include <asm/io.h>

#define DRV_NAME "awsmgmt"

enum AWSMGMT_BARS {
    AWSMGMT_MAIN_BAR = 0,
    AWSMGMT_MAILBOX_BAR,
    AWSMGMT_MAX_BAR
};

struct awsmgmt_ioc_info;

struct awsmgmt_bitstream_container {
  /* MAGIC_BITSTREAM == 0xBBBBBBBBUL */
  unsigned long magic;
  char *clear_bitstream;
  u32 clear_bitstream_length;
};

struct awsmgmt_dev {
	/* MAGIC_DEVICE == 0xAAAAAAAA */
	unsigned long magic;
	/* the kernel pci device data structure provided by probe() */
	struct pci_dev *pci_dev;
	struct pci_dev *user_pci_dev;
	int instance;
	void *__iomem bar[AWSMGMT_MAX_BAR];
	resource_size_t bar_map_size[AWSMGMT_MAX_BAR];
	struct awsmgmt_char *user_char_dev;
	struct awsmgmt_bitstream_container stash;
	u64 feature_id;
	unsigned short ocl_frequency[4];
	u64 unique_id_last_bitstream;
};

struct awsmgmt_char {
	struct awsmgmt_dev *lro;
	struct cdev cdev;
	struct device *sys_device;
	int bar;
};

struct awsmgmt_ocl_clockwiz {
	/* target frequency */
	unsigned short ocl;
	/* config0 register */
	unsigned long config0;
	/* config2 register */
	unsigned short config2;
};

#define AWSMGMT_DRIVER_MAJOR 2017
#define AWSMGMT_DRIVER_MINOR 1
#define AWSMGMT_DRIVER_PATCHLEVEL 2

#define AWSMGMT_MINOR_BASE (0)
#define AWSMGMT_MINOR_COUNT (16)
#define AWSMGMT_INPUT_FREQ 125

#define VERSION_BASE           0x0
#define PRISOLATION_BASE       0xfc
#define PF1_TUNNEL_BASE        0x300
#define HWICAP_OFFSET          0x1500
#define DDRA_CALIBRATION_BASE  0x1800
#define DDRB_CALIBRATION_BASE  0x1900
#define DDRC_CALIBRATION_BASE  0x1a00
#define DDRD_CALIBRATION_BASE  0x1b00

#define PROTECTION_LOGIC_BASE  0x218

#define DDR_STATUS_OFFSET      0x8
#define DDR_CONFIG_OFFSET      0xc

int bitstream_ioctl(struct awsmgmt_dev *lro, const void __user *arg);
int bitstream_ioctl_axlf(struct awsmgmt_dev *lro, const void __user *arg);
int ocl_freqscaling_ioctl(struct awsmgmt_dev *lro, const void __user *arg);
void freezeAXIGate(struct awsmgmt_dev *lro);
void freeAXIGate(struct awsmgmt_dev *lro);
void fill_frequency_info(struct awsmgmt_dev *lro, struct awsmgmt_ioc_info *obj);
void device_info(struct awsmgmt_dev *lro, struct awsmgmt_ioc_info *obj);
long load_boot_firmware(struct awsmgmt_dev *lro);
long ocl_freqscaling(struct awsmgmt_dev *lro, bool force);
int enable_ddrs(const struct awsmgmt_dev *lro);

#endif
