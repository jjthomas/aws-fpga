/*
 * Amazon FPGA Hardware Development Kit
 *
 * Copyright 2018 Amazon.com, Inc. or its affiliates. All Rights Reserved.
 *
 * Licensed under the Amazon Software License (the "License"). You may not use
 * this file except in compliance with the License. A copy of the License is
 * located at
 *
 *    http://aws.amazon.com/asl/
 *
 * or in the "license" file accompanying this file. This file is distributed on
 * an "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, express or
 * implied. See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <stdio.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <poll.h>

#include "fpga_pci.h"
#include "fpga_mgmt.h"
#include "fpga_dma.h"
#include "utils/lcd.h"

#include "test_dram_dma_common.h"

#define NUM_WORDS_IN_LINE 16
#define NUM_BYTES_IN_LINE (NUM_WORDS_IN_LINE * 8)
#define NUM_CORES 512

typedef struct __attribute__((packed)) {
  uint64_t data[NUM_WORDS_IN_LINE];
} cache_line;

#define	MEM_16G              (1ULL << 34)
#define USER_INTERRUPTS_MAX  (16)

/* use the standard out logger */
static const struct logger *logger = &logger_stdout;

void usage(const char* program_name);
int dma_example(int slot_i, uint32_t num_buffers, uint8_t **buffers, uint32_t *lengths,
  float max_output_expansion);

int main(int argc, char **argv) {
    int rc;
    int slot_id = 0;
    int interrupt_n;

    switch (argc) {
    case 1:
        break;
    case 3:
        sscanf(argv[2], "%x", &slot_id);
        break;
    default:
        usage(argv[0]);
        return 1;
    }

    /* setup logging to print to stdout */
    rc = log_init("test_dram_dma");
    fail_on(rc, out, "Unable to initialize the log.");
    rc = log_attach(logger, NULL, 0);
    fail_on(rc, out, "%s", "Unable to attach to the log.");

    /* initialize the fpga_plat library */
    rc = fpga_mgmt_init();
    fail_on(rc, out, "Unable to initialize the fpga_mgmt library");

    /* check that the AFI is loaded */
    log_info("Checking to see if the right AFI is loaded...");
#ifndef SV_TEST
    rc = check_slot_config(slot_id);
    fail_on(rc, out, "slot config is not correct");
#endif

    // extracts "ad_id" and "ad_type"
    uint8_t seq_confs[] = {1, 34, 2, 97, 3, 100, 4, 95, 5, 105, 6, 100, 200, 34, 8, 121, 9, 112, 10, 101, 200, 34};
    uint8_t split_confs[] = {4, 7, 116};

    int CHARS = atoi(argv[1]);
    FILE *fp = fopen("kafka-json.txt", "r");
    char *line = NULL;
    size_t len = 0;
    ssize_t read;

    uint32_t buf_size = sizeof(seq_confs) / 2 * 3 + 3 + sizeof(split_confs) + 3 + CHARS;
    uint8_t *buf = (uint8_t *)malloc(buf_size * sizeof(uint8_t));
    int chars = 0;
    for (int i = 0; i < sizeof(seq_confs); i += 2) {
      buf[chars] = seq_confs[i];
      buf[chars + 1] = seq_confs[i + 1];
      chars += 3;
    }
    for (int i = 0; i < 3; i++) {
      buf[chars++] = 255; // sentinel
    }
    for (int i = 0; i < sizeof(split_confs); i++) {
      buf[chars++] = split_confs[i];
    }
    for (int i = 0; i < 3; i++) {
      buf[chars++] = 255; // sentinel
    }
    while ((read = getline(&line, &len, fp)) != -1) {
      if (chars + read > buf_size) {
        break;
      }
      memcpy(buf + chars, line, read);
      chars += read;
    }

    uint8_t **buffers = (uint8_t **)malloc(NUM_CORES * sizeof(uint8_t *));
    uint32_t *lengths = (uint32_t *)malloc(NUM_CORES * sizeof(uint32_t));
    for (int i = 0; i < NUM_CORES; i++) {
      buffers[i] = buf;
      lengths[i] = chars / 4 * 4;
    }

    rc = dma_example(slot_id, NUM_CORES, buffers, lengths, 1.0);
    fail_on(rc, out, "DMA example failed");

out:
    log_info("TEST %s", (rc == 0) ? "PASSED" : "FAILED");
    return rc;
}

void usage(const char* program_name) {
    printf("usage: %s [--slot <slot>]\n", program_name);
}

/**
 * This example fills a buffer with random data and then uses DMA to copy that
 * buffer into each of the 4 DDR DIMMS.
 */
// max_output_expansion is a growth factor like 1.5
int dma_example(int slot_id, uint32_t num_buffers, uint8_t **buffers, uint32_t *lengths,
  float max_output_expansion) {
    assert(num_buffers % 4 == 0);
    int rc, channel;
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
    assert(write_buffer_size < 1000000000); // only 32 bits available on FPGA for addresses
    assert(read_buffer_size < 1000000000);

    int read_fd = fpga_dma_open_queue(FPGA_DMA_XDMA, slot_id,
        /*channel*/ 0, /*is_read*/ true);
    fail_on((rc = (read_fd < 0) ? -1 : 0), out, "unable to open read dma queue");

    int write_fd = fpga_dma_open_queue(FPGA_DMA_XDMA, slot_id,
        /*channel*/ 0, /*is_read*/ false);
    fail_on((rc = (write_fd < 0) ? -1 : 0), out, "unable to open write dma queue");

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
      rc = fpga_dma_burst_write(write_fd, write_buffer, write_buffer_size,
          input_mem_offset + channel * MEM_16G);
      fail_on(rc, out, "DMA write failed on DIMM: %d", channel);
    }

    pci_bar_handle_t pci_bar_handle = PCI_BAR_HANDLE_INIT;
    rc = fpga_pci_attach(0, 0, 0, 0, &pci_bar_handle);
    fail_on(rc, out, "Unable to attach to the AFI on slot id %d", 0);
    rc = fpga_pci_poke(pci_bar_handle, 0x600, 1);
    if (rc < 0) {
      fail_on((rc = (rc < 0)? errno:0), out, "call to fpga_pci_poke failed.");
    }
    uint32_t reg_peek;
    do {
      rc = fpga_pci_peek(pci_bar_handle, 0x600, &reg_peek);
      if (rc < 0) {
        fail_on((rc = (rc < 0)? errno:0), out, "call to fpga_pci_peek failed.");
      }
      usleep(10000);
    } while (reg_peek != 0);

    fpga_pci_peek(pci_bar_handle, 0x700, &reg_peek);
    printf("Number of cycles for streaming: %d\n", reg_peek);

    int total_consumed_bytes = 0;
    for (int i = 0; i < num_buffers; i++) {
      total_consumed_bytes += lengths[i];
    }
    printf("Throughput for streaming: %.2f GBps\n", total_consumed_bytes / (reg_peek * 8.0));

    for (channel=0; channel < 4; channel++) {
      rc = fpga_dma_burst_read(read_fd, read_buffer, read_buffer_size,
          output_mem_offset + channel * MEM_16G);
      fail_on(rc, out, "DMA read failed on DIMM: %d", channel);
      printf("Length of output from first core of channel %d: %d\n", channel, (int)(cl_rb[0].data[0] / 8));
    }

out:
    if (write_buffer != NULL) {
        free(write_buffer);
    }
    if (read_buffer != NULL) {
        free(read_buffer);
    }
    if (write_fd >= 0) {
        close(write_fd);
    }
    if (read_fd >= 0) {
        close(read_fd);
    }
    /* if there is an error code, exit with status 1 */
    return (rc != 0 ? 1 : 0);
}
