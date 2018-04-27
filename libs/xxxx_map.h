#ifndef MAP
#define MAP

/* hashtable */

#include <stddef.h>
#include <stdint.h>

typedef struct Map
{
  uint64_t* keys;
  void** ptrs;
  size_t cap;
  size_t len;
} Map;

void map_grow(Map* map, size_t size);
void map_free(Map* map);

// returns value at key
void* map_get(Map const* map, uint64_t key);

// grows the map if necessary
void map_put(Map* map, uint64_t key, void* data);

void map_remove(Map* map, uint64_t key);

static inline uint64_t hash_uint64(uint64_t x)
{
  x *= 0xff51afd7ed558ccd;
  x ^= x >> 32;
  return x;
}

static inline uint64_t hash_bytes(const char* buf, size_t len)
{
  uint64_t x = 0xcbf29ce484222325;
  for (size_t i = 0; i < len; i++)
  {
    x ^= buf[i];
    x *= 0x100000001b3;
    x ^= x >> 32;
  }
  return x;
}

static inline uint64_t hash_ptr(void const* ptr)
{
  return hash_uint64((uintptr_t)ptr);
}

#endif
