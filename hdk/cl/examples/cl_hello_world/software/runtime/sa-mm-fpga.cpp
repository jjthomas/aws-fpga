// http://stackoverflow.com/questions/17761704/suffix-array-algorithm
// clang++ -std=c++11 bw.cpp
#include <sys/time.h>
#include <fstream>
#include <vector>
#include <limits.h>
#include <algorithm>
#include <cassert>
#include <immintrin.h>

// #define FPGA 1
#ifdef FPGA
#include <stdio.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <poll.h>

extern "C" {
#include <fpga_pci.h>
#include <fpga_mgmt.h>
#include <utils/lcd.h>
}

static uint16_t pci_vendor_id = 0x1D0F; /* Amazon PCI Vendor ID */
static uint16_t pci_device_id = 0xF000;

const struct logger *logger = &logger_stdout;
int fd = -1;

static int 
check_slot_config()
{
    int rc;
    int slot_id = 0;
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

static int fpga_setup() {
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

  rc = check_slot_config();
  fail_on(rc, out, "slot config is not correct");

  fd = open("/dev/edma0_queue_0", O_RDWR);
  if(fd<0){
    printf("Cannot open device file /dev/edma0_queue_0.\nMaybe the EDMA "
           "driver isn't installed, isn't modified to attach to the PCI ID of "
           "your CL, or you're using a device file that doesn't exist?\n"
           "See the edma_install manual at <aws-fpga>/sdk/linux_kernel_drivers/edma/edma_install.md\n"
           "Remember that rescanning your FPGA can change the device file.\n"
           "To remove and re-add your edma driver and reset the device file mappings, run\n"
           "`sudo rmmod edma-drv && sudo insmod <aws-fpga>/sdk/linux_kernel_drivers/edma/edma-drv.ko`\n");
    fail_on((rc = (fd < 0)? 1:0), out, "unable to open DMA queue.");
  }

out:
  return rc;
}
#endif

static inline uint64_t
rdtsc(void)
{
	uint32_t eax = 0, edx;

	__asm__ __volatile__("cpuid;"
			     "rdtsc;"
				: "+a" (eax), "=d" (edx)
				:
				: "%rcx", "%rbx", "memory");

	__asm__ __volatile__("xorl %%eax, %%eax;"
			     "cpuid;"
				:
				:
				: "%rax", "%rbx", "%rcx", "%rdx", "memory");

	return (((uint64_t)edx << 32) | eax) / 1000;
}

using namespace std;

typedef struct __attribute__((packed)) {
  unsigned int second: 20;
  unsigned int first: 20;
  unsigned int i: 20;
  unsigned int: 4;
} p;

p *data1;
p *merge_buf;
p *sort_buf;
int buf_size;
int num_lists;

#define TUP_MASK (0x000000FFFFFFFFFFL)

#define LIST_BITS 9
#define LIST_SIZE (1 << LIST_BITS)
#define LG_LISTS_PER_BLOCK (12 - LIST_BITS)

#ifdef FPGA

#define OFF_MASK ((1 << LG_LISTS_PER_BLOCK) - 1)
#define LOOKUP(l, e) (data1 + (((l) >> LG_LISTS_PER_BLOCK) << 12) + ((l) & OFF_MASK) + ((e) << LG_LISTS_PER_BLOCK))

#define GLOB_OFF_MASK (LIST_SIZE - 1)
#define LOOKUP_GLOB(e) LOOKUP((e) >> LIST_BITS, (e) & GLOB_OFF_MASK)

#else

#define LOOKUP(l, e) (data1 + (l << LIST_BITS) + e)
#define LOOKUP_GLOB(e) (data1 + e)
#endif

int compare(const void *a, const void *b) {
  p *a_p = (p *)a;
  p *b_p = (p *)b;
  if (a_p->first != b_p->first) {
    return a_p->first - b_p->first;
  } else {
    return a_p->second - b_p->second;
  }
};

bool verify_sorted(int lower, int upper) {
  for (int i = lower + 1; i < upper; i++) {
    if (compare(LOOKUP_GLOB(i), LOOKUP_GLOB(i - 1)) < 0) {
      return false;
    }
  }
  return true;
}

#ifdef FPGA
void do_full_sort() {
  for (int i = 0; i < buf_size; i += 4096) { 
    pwrite(fd, data1 + i, 4096 * sizeof(p), 0);
    fsync(fd);
    pread(fd, data1 + i, 4096 * sizeof(p), 0);
  }
}
#else
void do_full_sort() {
  for (int i = 0; i < num_lists; i++) {
    for (int j = 0; j < LIST_SIZE; j++) {
      sort_buf[j] = *LOOKUP(i, j);
    }
    qsort(sort_buf, LIST_SIZE, sizeof(p), compare);
    for (int j = 0; j < LIST_SIZE; j++) {
      *LOOKUP(i, j) = sort_buf[j];
    }
  }
}
#endif

void merge_lists(int lower, int upper) {
  int lower_list = lower >> LIST_BITS;
  int upper_list = (upper - 1) >> LIST_BITS;
  if (lower_list == upper_list) {
    return;
  } else {
    // two or more lists
    int first_ptr = lower;
    int first_bound = upper_list << LIST_BITS;
    int second_ptr = first_bound;
    if (upper_list - lower_list > 1) {
      int mid_list = (lower_list + upper_list) / 2;
      first_bound = mid_list << LIST_BITS;
      second_ptr = first_bound;
      merge_lists(lower, first_bound);
      merge_lists(first_bound, upper);
    }
    int merge_ptr = 0;
    while (first_ptr < first_bound && second_ptr < upper) {
      p *first_el = LOOKUP_GLOB(first_ptr);
      p *second_el = LOOKUP_GLOB(second_ptr);
      if (first_el->second < second_el->second) {
        merge_buf[merge_ptr++] = *first_el;
        first_ptr++;
      } else {
        merge_buf[merge_ptr++] = *second_el;
        second_ptr++;
      }
    }
    while (first_ptr < first_bound) {
      merge_buf[merge_ptr++] = *LOOKUP_GLOB(first_ptr);
      first_ptr++;
    }
    while (second_ptr < upper) {
      merge_buf[merge_ptr++] = *LOOKUP_GLOB(second_ptr);
      second_ptr++;
    }
    for (int i = lower; i < upper; i++) {
      *LOOKUP_GLOB(i) = merge_buf[i - lower];
    }
  }
}

int main(int argc, char **argv) {
#ifdef FPGA
  int rc = fpga_setup();
  if (rc > 0) {
    printf("setup failed...\n");
    if (fd > 0) {
      close(fd);
    }
  }
#endif
  int CHARS = atoi(argv[1]);
  
  ifstream infile(argv[2]);
  string line;

  int chars = 0;
  char *buf = new char[CHARS];
  while (getline(infile, line)) {
    if (chars + line.length() > CHARS) {
      break;
    }
    memcpy(buf + chars, line.c_str(), line.length());
    chars += line.length();
  }

  buf_size = ((chars - 1) / 4096 + 1) * 4096;
  num_lists = buf_size / LIST_SIZE;
  data1 = (p *)malloc(sizeof(p) * buf_size);
  int *ranks = (int *)calloc(2 * chars, sizeof(int));
  int *new_idxs = (int *)malloc(sizeof(int) * chars);
  merge_buf = (p *)malloc(sizeof(p) * buf_size);
  // only for CPU-only version
  sort_buf = (p *)malloc(sizeof(p) * LIST_SIZE);
  for (int i = 0; i < chars; i++) {
    // TODO we can save an iteration and set gap back to 2 if we set both first and second here
    p *cur = LOOKUP_GLOB(i);
    cur->first = 0;
    cur->second = buf[i];
    cur->i = i;
  }
  for (int i = chars; i < buf_size; i++) {
    p *cur = LOOKUP_GLOB(i);
    cur->first = 1048575;
    cur->second = 1048575;
    cur->i = 1048575;
  }

  int gap = 1;

  struct timeval start, end, diff;
  gettimeofday(&start, 0);
  uint64_t total_start = rdtsc();
  uint64_t sort_time = 0;
  uint64_t update_time = 0;
  uint64_t merge_time = 0;
  while (true) {
    uint64_t sort_start = rdtsc();
    do_full_sort();
    sort_time += rdtsc() - sort_start;
    int el_to_check = LIST_SIZE;
    uint64_t merge_start = rdtsc();
    while (el_to_check < buf_size) {
      p *prev = LOOKUP_GLOB(el_to_check - 1);
      p *next = LOOKUP_GLOB(el_to_check);
      if (prev->first == next->first) {
	int lower_bound = el_to_check - 1;
	int upper_bound = el_to_check;
        while (lower_bound >= 0 && LOOKUP_GLOB(lower_bound)->first == prev->first) {
          lower_bound--;
	}
	lower_bound++;
        while (upper_bound < buf_size && LOOKUP_GLOB(upper_bound)->first == prev->first) {
          upper_bound++;
	}
	// printf("%d %d %d %d\n", upper_bound, buf_size, LOOKUP_GLOB(upper_bound)->first, prev->first);
	// printf("%d %d\n", lower_bound, upper_bound);
        merge_lists(lower_bound, upper_bound);
	// TODO remove
	// assert(verify_sorted(lower_bound, upper_bound));
	el_to_check = ((upper_bound >> LIST_BITS) + 1) << LIST_BITS;
      } else {
        el_to_check += LIST_SIZE;
      }
    }
    merge_time += rdtsc() - merge_start;

    uint64_t update_start = rdtsc();
    int cur_char = 1;
    __m256i zero = _mm256_set1_epi32(0);
    ranks[LOOKUP_GLOB(0)->i] = cur_char;
    for (int i = 1; i < chars; i++) {
      uint64_t cur = *(uint64_t *)LOOKUP_GLOB(i);
      uint64_t prev = *(uint64_t *)LOOKUP_GLOB(i - 1);
      cur_char += ((cur & TUP_MASK) != (prev & TUP_MASK));
      ranks[LOOKUP_GLOB(i)->i] = cur_char;
    }
    update_time += rdtsc() - update_start;
    if (cur_char == chars) {
      // update_time += rdtsc() - update_start;
      break;
    }
    for (int i = 0; i < chars; i++) {
      p *cur = LOOKUP_GLOB(i);
      cur->first = ranks[cur->i];
      cur->second = ranks[cur->i + gap];
    }
    // update_time += rdtsc() - update_start;
    gap *= 2;
    // printf("new gap %d\n", gap);
  }
  gettimeofday(&end, 0);
  timersub(&end, &start, &diff);
  printf("gettimeofday: %ld.%06ld\n", (long)diff.tv_sec, (long)diff.tv_usec);
  printf("%lld/%lld/%lld/%lld\n", sort_time, update_time, merge_time, rdtsc() - total_start);
  printf("%d\n", ranks[0]);
  printf("%d\n", gap);
#ifdef FPGA
  if (fd >= 0) {
    close(fd);
  }
#endif
  return 0;
}
