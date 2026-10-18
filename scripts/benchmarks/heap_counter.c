/*
 * Counts the peak of live heap bytes in the process it is preloaded into, and
 * writes it at exit to the file named by HEAP_COUNTER_OUT:
 *
 *   LD_PRELOAD=./heap_counter.so HEAP_COUNTER_OUT=heap.txt ./program
 *
 * Unlike peak RSS, the count does not depend on the kernel or on what else runs
 * on the machine, so for a fixed input and seed it repeats exactly. Sizes are what
 * the allocator hands out (malloc_usable_size), so a request rounded up to the
 * allocator's size class counts at its rounded size.
 *
 * glibc only: the replacements forward to glibc's own exported __libc_*
 * functions, which avoids looking them up with dlsym from inside malloc.
 */
#define _GNU_SOURCE
#include <errno.h>
#include <malloc.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>

extern void* __libc_malloc(size_t size);
extern void __libc_free(void* ptr);
extern void* __libc_calloc(size_t count, size_t size);
extern void* __libc_realloc(void* ptr, size_t size);
extern void* __libc_memalign(size_t alignment, size_t size);
extern void* __libc_valloc(size_t size);
extern void* __libc_pvalloc(size_t size);

static size_t live_bytes;
static size_t peak_bytes;
static size_t allocations;

static void* count_allocation(void* ptr)
{
  if (ptr == NULL)
    return ptr;
  size_t live = __atomic_add_fetch(&live_bytes, malloc_usable_size(ptr), __ATOMIC_RELAXED);
  __atomic_add_fetch(&allocations, 1, __ATOMIC_RELAXED);
  size_t peak = __atomic_load_n(&peak_bytes, __ATOMIC_RELAXED);
  while (live > peak && !__atomic_compare_exchange_n(&peak_bytes, &peak, live, 1, __ATOMIC_RELAXED, __ATOMIC_RELAXED)) {
  }
  return ptr;
}

static void count_free(void* ptr)
{
  if (ptr != NULL)
    __atomic_sub_fetch(&live_bytes, malloc_usable_size(ptr), __ATOMIC_RELAXED);
}

void* malloc(size_t size) { return count_allocation(__libc_malloc(size)); }

void free(void* ptr)
{
  count_free(ptr);
  __libc_free(ptr);
}

void* calloc(size_t count, size_t size) { return count_allocation(__libc_calloc(count, size)); }

void* realloc(void* ptr, size_t size)
{
  size_t old_size = ptr != NULL ? malloc_usable_size(ptr) : 0;
  void* result = __libc_realloc(ptr, size);
  // A failed realloc leaves the old block allocated; realloc(ptr, 0) frees it.
  if (result != NULL || size == 0) {
    __atomic_sub_fetch(&live_bytes, old_size, __ATOMIC_RELAXED);
  }
  return count_allocation(result);
}

void* memalign(size_t alignment, size_t size) { return count_allocation(__libc_memalign(alignment, size)); }

void* aligned_alloc(size_t alignment, size_t size) { return count_allocation(__libc_memalign(alignment, size)); }

int posix_memalign(void** out, size_t alignment, size_t size)
{
  if (alignment == 0 || alignment % sizeof(void*) != 0 || (alignment & (alignment - 1)) != 0)
    return EINVAL;
  void* ptr = __libc_memalign(alignment, size);
  if (ptr == NULL)
    return ENOMEM;
  *out = count_allocation(ptr);
  return 0;
}

void* valloc(size_t size) { return count_allocation(__libc_valloc(size)); }

void* pvalloc(size_t size) { return count_allocation(__libc_pvalloc(size)); }

__attribute__((destructor)) static void write_peak(void)
{
  const char* path = getenv("HEAP_COUNTER_OUT");
  if (path == NULL)
    return;
  FILE* file = fopen(path, "w");
  if (file == NULL)
    return;
  fprintf(file, "peak_heap_bytes %zu\nallocations %zu\n", peak_bytes, allocations);
  fclose(file);
}
