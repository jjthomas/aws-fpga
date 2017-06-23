// http://stackoverflow.com/questions/17761704/suffix-array-algorithm
// g++ -O3 -std=c++11 -march=native sa-mm-fpga.cpp
// scl enable devtoolset-4 bash
#include <sys/time.h>
#include <fstream>
#include <vector>
#include <limits.h>
#include <algorithm>
#include <cassert>
#include <immintrin.h>
#include <string.h>

// #define FPGA 1
#define AVX512 1
#ifdef FPGA
#include <stdio.h>
#include <fcntl.h>
#include <errno.h>
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

typedef struct {
  unsigned int *second;
  unsigned int *first;
  unsigned int *i;
} s;

s data1;
s merge_buf;
p *sort_buf;
int buf_size;
int num_lists;

#define TUP_MASK (0x000000FFFFFFFFFFL)
#define I_MASK (0x0FFFFF0000000000L)

#define LIST_BITS 9
#define LIST_SIZE (1 << LIST_BITS)
#define LG_LISTS_PER_BLOCK (12 - LIST_BITS)

#ifdef FPGA

#define OFF_MASK ((1 << LG_LISTS_PER_BLOCK) - 1)
#define LOOKUP(l, e) (data1 + (((l) >> LG_LISTS_PER_BLOCK) << 12) + ((l) & OFF_MASK) + ((e) << LG_LISTS_PER_BLOCK))

#define GLOB_OFF_MASK (LIST_SIZE - 1)
#define LOOKUP_GLOB(e) LOOKUP((e) >> LIST_BITS, (e) & GLOB_OFF_MASK)

#else

#define LOOKUP(l, e) (data1 + ((l) << LIST_BITS) + (e))
#define LOOKUP_GLOB(e) (data1 + (e))
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
  data1.first = (unsigned int *)malloc(sizeof(unsigned int) * buf_size);
  data1.second = (unsigned int *)malloc(sizeof(unsigned int) * buf_size);
  data1.i = (unsigned int *)malloc(sizeof(unsigned int) * buf_size);
  s data2;
  data2.first = (unsigned int *)malloc(sizeof(unsigned int) * buf_size);
  data2.second = (unsigned int *)malloc(sizeof(unsigned int) * buf_size);
  data2.i = (unsigned int *)malloc(sizeof(unsigned int) * buf_size);
  int *ranks = (int *)calloc(2 * chars, sizeof(int));
  merge_buf.first = (unsigned int *)malloc(sizeof(unsigned int) * buf_size);
  merge_buf.second = (unsigned int *)malloc(sizeof(unsigned int) * buf_size);
  merge_buf.i = (unsigned int *)malloc(sizeof(unsigned int) * buf_size);
  // only for CPU-only version
  sort_buf = (p *)malloc(sizeof(p) * LIST_SIZE);
  for (int i = 0; i < chars; i++) {
    data1.first[i] = buf[i] + 1;
    data1.second[i] = (i < chars - 1) ? buf[i + 1] + 1 : 0;
    data1.i[i] = i;
  }
  for (int i = chars; i < buf_size; i++) {
    data1.first[i] = 1048575;
    data1.second[i] = 1048575;
    data1.i[i] = 1048575;
  }

  int *prefix_sums = (int *)malloc(sizeof(int) * 65536 * 16);
  for (int i = 0; i < 65536; i++) {
    int cur_sum = 0;
    int cur_val = i;
    for (int j = 0; j < 16; j++) {
      cur_sum += (cur_val & 0x01);
      prefix_sums[i * 16 + j] = cur_sum;
      cur_val >>= 1;
    }
  }

  int gap = 2;

  struct timeval start, end, diff;
  gettimeofday(&start, 0);
  uint64_t total_start = rdtsc();
  uint64_t sort_time = 0;
  uint64_t update_time = 0;
  uint64_t merge_time = 0;

  uint64_t radix_start = rdtsc();
  int *counts = (int *)calloc(257, sizeof(int));
  for (int i = 0; i < chars; i++) {
    counts[data1.second[i]]++;
  }
  int sum = 0;
  for (int i = 0; i < 257; i++) {
    int new_sum = sum + counts[i];
    counts[i] = sum;
    sum = new_sum;
  }
  for (int i = 0; i < chars; i++) {
    data2.first[counts[data1.second[i]]] = data1.first[i];
    data2.second[counts[data1.second[i]]] = data1.second[i];
    data2.i[counts[data1.second[i]]] = data1.i[i];
    counts[data1.second[i]]++;
  }
  // radix pass 2
  for (int i = 0; i < 257; i++) {
    counts[i] = 0;
  }
  for (int i = 0; i < chars; i++) {
    counts[data2.first[i]]++;
  }
  sum = 0;
  for (int i = 0; i < 257; i++) {
    int new_sum = sum + counts[i];
    counts[i] = sum;
    sum = new_sum;
  }
  for (int i = 0; i < chars; i++) {
    data1.first[counts[data2.first[i]]] = data2.first[i];
    data1.second[counts[data2.first[i]]] = data2.second[i];
    data1.i[counts[data2.first[i]]] = data2.i[i];
    counts[data2.first[i]]++;
  }
  merge_time += rdtsc() - radix_start;
  while (true) {
    uint64_t update_start = rdtsc();
    int cur_char = 1;
#ifdef AVX512
    int vector_bound = (chars - 1) / 8 * 8 + 1;
    ranks[LOOKUP_GLOB(0)->i] = cur_char;
    for (int i = 1; i < vector_bound; i += 8) {
      __m512i cur = _mm512_loadu_si512(LOOKUP_GLOB(i));
      __m512i prev = _mm512_loadu_si512(LOOKUP_GLOB(i - 1));
      __m512i cur_masked = _mm512_and_epi64(cur, tup_mask);
      __m512i prev_masked = _mm512_and_epi64(prev, tup_mask);
      __mmask8 cmp_res = _mm512_cmpneq_epi64_mask(cur_masked, prev_masked);
      __m256i cur_char_broad = _mm256_set1_epi32(cur_char);
      __m256i pref_sum = _mm256_loadu_si256((const __m256i *)(prefix_sums + (cmp_res * 8)));
      __m256i new_ranks = _mm256_add_epi32(cur_char_broad, pref_sum);
      __m512i cur_is = _mm512_srli_epi64(_mm512_and_epi64(cur, i_mask), 40);
      _mm512_i64scatter_epi32(ranks, cur_is, new_ranks, 4);
      cur_char += prefix_sums[(cmp_res * 8) + 7];
    }
#else
    int vector_bound = 1;
#endif
    for (int i = vector_bound; i < chars; i++) {
      p *cur = LOOKUP_GLOB(i);
      uint64_t prev = *(uint64_t *)LOOKUP_GLOB(i - 1);
      cur_char += ((*((uint64_t *)cur) & TUP_MASK) != (prev & TUP_MASK));
      ranks[cur->i] = cur_char;
    }
    if (cur_char == chars) {
      update_time += rdtsc() - update_start;
      break;
    }
#ifdef AVX512
    vector_bound = chars / 8 * 8;
    __m512i gap_broad = _mm512_set1_epi64(gap);
    for (int i = 0; i < vector_bound; i += 8) {
      __m512i cur = _mm512_loadu_si512(LOOKUP_GLOB(i));
      __m512i cur_is_orig = _mm512_and_epi64(cur, i_mask);
      __m512i cur_is = _mm512_srli_epi64(cur_is_orig, 40);
      __m512i cur_is_gap = _mm512_add_epi64(cur_is, gap_broad);
      __m512i firsts = _mm512_cvtepu32_epi64(_mm512_i64gather_epi32(cur_is, ranks, 4));
      __m512i seconds = _mm512_cvtepu32_epi64(_mm512_i64gather_epi32(cur_is_gap, ranks, 4));
      __m512i tup = _mm512_or_epi64(seconds, _mm512_slli_epi64(firsts, 20));
      __m512i result = _mm512_or_epi64(cur_is_orig, tup);
      _mm512_storeu_si512(LOOKUP_GLOB(i), result);
    }
#else
    vector_bound = 0;
#endif
    for (int i = vector_bound; i < chars; i++) {
      p *cur = LOOKUP_GLOB(i);
      cur->first = ranks[cur->i];
      cur->second = ranks[cur->i + gap];
    }
    update_time += rdtsc() - update_start;
    gap *= 2;
    // printf("new gap %d\n", gap);

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
