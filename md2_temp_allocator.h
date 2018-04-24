#ifndef TEMP_ALLOCATOR
#define TEMP_ALLOCATOR

typedef struct TempAllocatorRegion
{
  char* bytes_f;
  size_t bytes_n;
  size_t bytes_allocated;
} TempAllocatorRegion;

typedef struct TempAllocator
{
  TempAllocatorRegion* regions;
} TempAllocator;

void temp_allocator_free(TempAllocator* temp_allocator);
void* temp_calloc(TempAllocator* temp_allocator, size_t n, size_t element_size);

#endif