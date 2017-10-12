// Amazon FPGA Hardware Development Kit
//
// Copyright 2016 Amazon.com, Inc. or its affiliates. All Rights Reserved.
//
// Licensed under the Amazon Software License (the "License"). You may not use
// this file except in compliance with the License. A copy of the License is
// located at
//
//    http://aws.amazon.com/asl/
//
// or in the "license" file accompanying this file. This file is distributed on
// an "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, express or
// implied. See the License for the specific language governing permissions and
// limitations under the License.

#include <stdio.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <poll.h>

#include <fpga_pci.h>
#include <fpga_mgmt.h>
#include <utils/lcd.h>

typedef struct __attribute__((packed)) {
  uint64_t data[8];
} cache_line;

static uint16_t pci_vendor_id = 0x1D0F; /* Amazon PCI Vendor ID */
static uint16_t pci_device_id = 0xF001;
/* */

#define	MEM_16G		(1ULL << 34)

/* use the stdout logger */
const struct logger *logger = &logger_stdout;

int dma_example(int slot_i);
int interrupt_example(int slot_id, int interrupt_number);

int main(int argc, char **argv) {
    int rc;
    int slot_id;

    /* setup logging to print to stdout */
    rc = log_init("test_dram_dma");
    fail_on(rc, out, "Unable to initialize the log.");
    rc = log_attach(logger, NULL, 0);
    fail_on(rc, out, "%s", "Unable to attach to the log.");

    /* initialize the fpga_plat library */
    rc = fpga_mgmt_init();
    fail_on(rc, out, "Unable to initialize the fpga_mgmt library");

    slot_id = 0;

    rc = dma_example(slot_id);
    fail_on(rc, out, "DMA example failed");

out:
    return rc;
}

static int
check_slot_config(int slot_id)
{
    int rc;
    struct fpga_mgmt_image_info info = {0};

    /* get local image description, contains status, vendor id, and device id */
    rc = fpga_mgmt_describe_local_image(slot_id, &info, 0);
    fail_on(rc, out, "Unable to get local image information. Are you running as root?");

    /* check to see if the slot is ready */
    if (info.status != FPGA_STATUS_LOADED) {
        rc = 1;
        fail_on(rc, out, "Slot %d is not ready", slot_id);
    }

    /* confirm that the AFI that we expect is in fact loaded */
    if (info.spec.map[FPGA_APP_PF].vendor_id != pci_vendor_id ||
        info.spec.map[FPGA_APP_PF].device_id != pci_device_id) {
        rc = 1;
        printf("The slot appears loaded, but the pci vendor or device ID doesn't "
               "match the expected values. You may need to rescan the fpga with \n"
               "fpga-describe-local-image -S %i -R\n"
               "Note that rescanning can change which device file in /dev/ a FPGA will map to.\n"
               "To remove and re-add your edma driver and reset the device file mappings, run\n"
               "`sudo rmmod edma-drv && sudo insmod <aws-fpga>/sdk/linux_kernel_drivers/edma/edma-drv.ko`\n",
               slot_id);
        fail_on(rc, out, "The PCI vendor id and device of the loaded image are "
                         "not the expected values.");
    }

out:
    return rc;
}


/* 
 * Write 4 identical buffers to the 4 different DRAM channels of the AFI
 * using fsync() between the writes and read to insure order
 */

int dma_example(int slot_id) {
    int fd, rc;
    char device_file_name[256];
    char *write_buffer, *read_buffer, *expected_read_buffer;
    static const int num_cores = 512;
    static const int num_data_blocks = 100;
    static const int input_mem_offset = 0;
    static const int output_mem_offset = 1000000000;
    const size_t buffer_size = 64 * (num_data_blocks + 1) * (num_cores / 4);
    int channel=0;

    read_buffer = NULL;
    write_buffer = NULL;
    fd = -1;

    rc = sprintf(device_file_name, "/dev/edma%i_queue_0", slot_id);
    fail_on((rc = (rc < 0)? 1:0), out, "Unable to format device file name.");


    /* make sure the AFI is loaded and ready */
    rc = check_slot_config(slot_id);
    fail_on(rc, out, "slot config is not correct");

    fd = open(device_file_name, O_RDWR);
    if(fd<0){
        printf("Cannot open device file %s.\nMaybe the EDMA "
               "driver isn't installed, isn't modified to attach to the PCI ID of "
               "your CL, or you're using a device file that doesn't exist?\n"
               "See the edma_install manual at <aws-fpga>/sdk/linux_kernel_drivers/edma/edma_install.md\n"
               "Remember that rescanning your FPGA can change the device file.\n"
               "To remove and re-add your edma driver and reset the device file mappings, run\n"
               "`sudo rmmod edma-drv && sudo insmod <aws-fpga>/sdk/linux_kernel_drivers/edma/edma-drv.ko`\n",
               device_file_name);
        fail_on((rc = (fd < 0)? 1:0), out, "unable to open DMA queue. ");
    }

    write_buffer = (char *)malloc(buffer_size);
    read_buffer = (char *)malloc(buffer_size);
    expected_read_buffer = (char *)malloc(buffer_size);
    if (write_buffer == NULL || read_buffer == NULL || expected_read_buffer == NULL) {
        rc = ENOMEM;
        goto out;
    }

    cache_line *cl_wb = (cache_line *)write_buffer; 
    cache_line *cl_rb = (cache_line *)expected_read_buffer; 
    for (int i = 0; i < num_cores/4; i++) { 
      cl_wb[i].data[0] = input_mem_offset + 64 * (num_cores/4 + i * num_data_blocks);
      cl_wb[i].data[1] = 512 * num_data_blocks;
      cl_wb[i].data[2] = output_mem_offset + 64 * (num_data_blocks + 1) * i;

      cl_rb[i * (num_data_blocks + 1)].data[0] = 512 * num_data_blocks;
      for (int j = 1; j < 8; j++) {
        cl_rb[i * (num_data_blocks + 1)].data[j] = 0;
      }
    }
    for (int i = num_cores/4; i < num_cores/4 * (num_data_blocks + 1); i++) { 
      for (int j = 0; j < 8; j++) {
        cl_wb[i].data[j] = i + j;
        int offset = i - num_cores/4;
        cl_rb[offset / num_data_blocks * (num_data_blocks + 1) + (offset % num_data_blocks) + 1].data[j] = i + j;
      }
    }


    for (channel=0; channel < 4; channel++) {
        size_t write_offset = 0;
        while (write_offset < buffer_size) {
            if (write_offset != 0) {
                printf("Partial write by driver, trying again with remainder of buffer (%lu bytes)\n",
                    buffer_size - write_offset);
            }
            rc = pwrite(fd,
                write_buffer + write_offset,
                buffer_size - write_offset,
                input_mem_offset + channel*MEM_16G + write_offset);
            if (rc < 0) {
                fail_on((rc = (rc < 0)? errno:0), out, "call to pwrite failed.");
            }
            write_offset += rc;
        }
        rc = 0;
    }

    /* fsync() will make sure the write made it to the target buffer 
     * before read is done
     */

    fsync(fd);

    pci_bar_handle_t pci_bar_handle = PCI_BAR_HANDLE_INIT;
    rc = fpga_pci_attach(0, 0, 0, 0, &pci_bar_handle);
    fail_on(rc, out, "Unable to attach to the AFI on slot id %d", 0);
    rc = fpga_pci_poke(pci_bar_handle, 0x500, 1);
    if (rc < 0) {
      fail_on((rc = (rc < 0)? errno:0), out, "call to fpga_pci_poke failed.");
    }
    uint32_t reg_peek;
    do {
      rc = fpga_pci_peek(pci_bar_handle, 0x500, &reg_peek);
      if (rc < 0) {
        fail_on((rc = (rc < 0)? errno:0), out, "call to fpga_pci_peek failed.");
      }
      usleep(10000);
    } while (reg_peek != 0);

    fpga_pci_peek(pci_bar_handle, 0x600, &reg_peek);
    printf("Number of cycles for streaming: %d\n", reg_peek);

    for (channel=0; channel < 4; channel++) {
        size_t read_offset = 0;
        while (read_offset < buffer_size) {
            if (read_offset != 0) {
                printf("Partial read by driver, trying again with remainder of buffer (%lu bytes)\n",
                    buffer_size - read_offset);
            }
            rc = pread(fd,
                read_buffer + read_offset,
                buffer_size - read_offset,
                output_mem_offset + channel*MEM_16G + read_offset);
            if (rc < 0) {
                fail_on((rc = (rc < 0)? errno:0), out, "call to pread failed.");
            }
            read_offset += rc;
        }
        rc = 0;

    	if (memcmp(expected_read_buffer, read_buffer, buffer_size) == 0) {
        	printf("DRAM DMA read the correct string for channel %d (it worked correctly!)\n", channel);
    	} else {
            /*
            int i;
            printf("Numbers expected from channel %d:\n", channel);
            for (i = 0; i < buffer_size/8; ++i) {
                printf("%" PRIu64 " ", ((uint64_t *)expected_read_buffer)[i]);
            }

            printf("\n\n");

            printf("Numbers read:\n");
            for (i = 0; i < buffer_size/8; ++i) {
                printf("%" PRIu64 " ", ((uint64_t *)read_buffer)[i]);
            }
            printf("\n\n");
            */
         
            rc = 1; 
            fail_on(rc, out, "Data read from DMA did not match data written with DMA. Was there an fsync() between the read and write?");
    	}
    }

out:
    if (write_buffer != NULL) {
        free(write_buffer);
    }
    if (read_buffer != NULL) {
        free(read_buffer);
    }
    if (expected_read_buffer != NULL) {
        free(expected_read_buffer);
    }
    if (fd >= 0) {
        close(fd);
    }
    /* if there is an error code, exit with status 1 */
    return (rc != 0 ? 1 : 0);
}
