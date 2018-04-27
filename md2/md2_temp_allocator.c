#include "md2_temp_allocator.h"

static inline bool region_fits(TempAllocatorRegion* region, size_t added_bytes_n)
{
  return added_bytes_n <= (region->bytes_n - region->bytes_allocated);
}

void temp_allocator_free(TempAllocator* temp_allocator)
{
  for (TempAllocatorRegion *region_i = &temp_allocator->regions[0],
                           *region_l = buf_end(temp_allocator->regions);
       region_i < region_l; region_i++)
  {
    free(region_i->bytes_f), region_i->bytes_f = NULL;
  }
  buf_free(temp_allocator->regions), temp_allocator->regions = NULL;
}

void* temp_calloc(TempAllocator* temp_allocator, size_t n, size_t element_size)
{
  if (element_size * n == 0)
  {
    return NULL;
  }
  assert(element_size * n > 0);
  assert(element_size < SIZE_MAX / n); // @todo compare with SQRT_SIZE_MAX

  uintptr_t alignment = 16;
  uintptr_t needed_size = element_size * n;
  uintptr_t aligned_needed_size =
    round_up_multiple_of_pot_uintptr(needed_size, alignment);

  if (buf_len(temp_allocator->regions) == 0
      || buf_end(temp_allocator->regions)[-1].bytes_allocated
           == buf_end(temp_allocator->regions)[-1].bytes_n)
  {
    size_t region_size = max_i(1024 * 1024, aligned_needed_size);
    TempAllocatorRegion new_region = {
      .bytes_f = calloc(1, region_size),
      .bytes_n = region_size,
    };
    buf_push(temp_allocator->regions, new_region);
  }
  TempAllocatorRegion* region = &buf_end(temp_allocator->regions)[-1];
  assert(region_fits(region, aligned_needed_size));
  char* ptr =
    &region->bytes_f[region->bytes_allocated] + aligned_needed_size - needed_size;
  region->bytes_allocated += aligned_needed_size;
  assert(region->bytes_n - (ptr - region->bytes_f) >= needed_size);
  return ptr;
}

static inline void* temp_memdup_range(TempAllocator* allocator,
                                      void const* first,
                                      void const* last)
{
  char const* bytes_f = first;
  char const* bytes_l = last;
  void* dst = temp_calloc(allocator, 1, bytes_l - bytes_f);
  memcpy(dst, bytes_f, bytes_l - bytes_f);
  return dst;
}
