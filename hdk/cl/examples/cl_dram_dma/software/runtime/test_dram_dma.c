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
#include <sys/time.h>
#include <math.h>
#include <assert.h>

#include <fpga_pci.h>
#include <fpga_mgmt.h>
#include <utils/lcd.h>

#define NUM_WORDS_IN_LINE 8
#define NUM_BYTES_IN_LINE (NUM_WORDS_IN_LINE * 8)
#define NUM_CORES 512

typedef struct __attribute__((packed)) {
  uint64_t data[NUM_WORDS_IN_LINE];
} cache_line;

static uint16_t pci_vendor_id = 0x1D0F; /* Amazon PCI Vendor ID */
static uint16_t pci_device_id = 0xF001;
/* */

#define	MEM_16G		(1ULL << 34)

/* use the stdout logger */
const struct logger *logger = &logger_stdout;

int dma_example(int slot_i, uint32_t num_buffers, uint8_t **buffers, uint32_t *lengths,
  float max_output_expansion);
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

  int CHARS = atoi(argv[1]);
  FILE *fp = fopen("kafka-json.txt", "r");
  char *line = NULL;
  size_t len = 0;
  ssize_t read;

  int chars = 0;
  uint8_t *buf = (uint8_t *)malloc(CHARS * sizeof(uint8_t));
  while ((read = getline(&line, &len, fp)) != -1) {
    if (chars + read > CHARS) {
      break;
    }
    memcpy(buf + chars, line, read);
    chars += read;
  }

  uint8_t **buffers = (uint8_t **)malloc(NUM_CORES * sizeof(uint8_t *));
  uint32_t *lengths = (uint32_t *)malloc(CHARS * sizeof(uint32_t));
  for (int i = 0; i < NUM_CORES; i++) {
    buffers[i] = buf;
    lengths[i] = chars;
  }

  rc = dma_example(slot_id, NUM_CORES, buffers, lengths, 1.0);
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

// max_output_expansion is a growth factor like 1.5
int dma_example(int slot_id, uint32_t num_buffers, uint8_t **buffers, uint32_t *lengths,
  float max_output_expansion) {
    assert(num_buffers % 4 == 0);
    int fd, rc;
    char device_file_name[256];
    char *write_buffer, *read_buffer;
    const int num_cores = num_buffers;
    static const int input_mem_offset = 0;
    static const int output_mem_offset = 1000000000;
    size_t input_buffer_length = 0;
    for (int i = 0; i < num_buffers; i++) {
      if (lengths[i] > input_buffer_length) {
        input_buffer_length = lengths[i];
      }
    }
    const size_t rounded_input_buffer_length = (input_buffer_length + NUM_BYTES_IN_LINE - 1)
      / NUM_BYTES_IN_LINE * NUM_BYTES_IN_LINE;
    const size_t write_buffer_size = (num_cores/4) * (rounded_input_buffer_length + NUM_BYTES_IN_LINE);
    const size_t rounded_output_buffer_length = ((size_t)ceil(input_buffer_length * max_output_expansion) +
      NUM_BYTES_IN_LINE - 1) / NUM_BYTES_IN_LINE * NUM_BYTES_IN_LINE;
    const size_t read_buffer_size = (num_cores/4) * (rounded_output_buffer_length + NUM_BYTES_IN_LINE);
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

    write_buffer = (char *)malloc(write_buffer_size);
    read_buffer = (char *)malloc(read_buffer_size);
    if (write_buffer == NULL || read_buffer == NULL) {
        rc = ENOMEM;
        goto out;
    }

    cache_line *cl_wb = (cache_line *)write_buffer;
    cache_line *cl_rb = (cache_line *)read_buffer;

    for (channel = 0; channel < 4; channel++) {
      for (int i = 0; i < num_cores/4; i++) {
        int core_start_offset = NUM_BYTES_IN_LINE * num_cores/4 + i * rounded_input_buffer_length;
        cl_wb[i].data[0] = input_mem_offset + core_start_offset;
        cl_wb[i].data[1] = lengths[channel * num_cores/4 + i] * 8; // expressed in bits
        cl_wb[i].data[2] = output_mem_offset + (NUM_BYTES_IN_LINE + rounded_output_buffer_length) * i;

        memcpy(write_buffer + core_start_offset, buffers[channel * num_cores/4 + i], lengths[channel * num_cores/4 + i]);
      }
      size_t write_offset = 0;
      while (write_offset < write_buffer_size) {
        if (write_offset != 0) {
          printf("Partial write by driver, trying again with remainder of buffer (%lu bytes)\n",
            write_buffer_size - write_offset);
        }
        rc = pwrite(fd,
          write_buffer + write_offset,
          write_buffer_size - write_offset,
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
      while (read_offset < read_buffer_size) {
        if (read_offset != 0) {
          printf("Partial read by driver, trying again with remainder of buffer (%lu bytes)\n",
            read_buffer_size - read_offset);
        }
        rc = pread(fd,
          read_buffer + read_offset,
          read_buffer_size - read_offset,
          output_mem_offset + channel*MEM_16G + read_offset);
        if (rc < 0) {
          fail_on((rc = (rc < 0)? errno:0), out, "call to pread failed.");
        }
        read_offset += rc;
      }
      printf("Length of output from first core of channel %d: %d\n", channel, (int)(cl_rb[0].data[0] / 8));
    }


out:
    if (write_buffer != NULL) {
        free(write_buffer);
    }
    if (read_buffer != NULL) {
        free(read_buffer);
    }
    if (fd >= 0) {
        close(fd);
    }
    /* if there is an error code, exit with status 1 */
    return (rc != 0 ? 1 : 0);
}
