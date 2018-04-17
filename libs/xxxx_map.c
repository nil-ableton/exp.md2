#include "xxxx_map.h"

#include <assert.h>
#include <math.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

static uint64_t pow2_ge_uint64(uint64_t x)
{
  for (unsigned int i = 0;; i++)
  {
    if (1uLL << i >= x)
      return 1uLL << i;
  }
}

static bool is_pow2_uint64(uint64_t x)
{
  int i;
  for (i = 0; i < 64; i++, x >>= 1)
  {
    if (x & 1)
      break;
  }
  if (i == 64)
    return false;
  i++, x >>= 1;
  for (; i < 64; i++, x >>= 1)
  {
    if (x & 1)
      return false;
  }
  return i == 64;
}

void map_grow(Map* map, size_t size)
{
  size = pow2_ge_uint64(size);
  Map new_map = {
    .keys = calloc(size, sizeof new_map.keys[0]),
    .ptrs = malloc(size * sizeof new_map.ptrs[0]),
    .cap = size,
  };
  if (!new_map.keys)
    exit(1);
  if (!new_map.ptrs)
    exit(1);

  for (size_t i = 0; i < map->cap; i++)
  {
    if (map->keys[i])
    {
      map_put(&new_map, map->keys[i], map->ptrs[i]);
    }
  }

  assert(is_pow2_uint64(new_map.cap));
  assert(new_map.len == map->len);

  free(map->keys);
  free(map->ptrs);

  *map = new_map;
}

// returns value at key
void* map_get(Map* map, uint64_t key)
{
  assert(map->cap - map->len >= map->len);

  size_t index = hash_uint64(key);
  size_t rest = map->len;
  while (rest > 0)
  {
    index &= (map->cap - 1);
    if (!map->keys[index])
      return NULL;
    if (map->keys[index] == key)
      return map->ptrs[index];
    index++;
    rest--;
  }
  return NULL;
}

// puts value in hasmapable, returns previous value
void map_put(Map* map, uint64_t key, void* data)
{
  if (map->cap == 0 || map->cap - map->len < map->len)
  {
    map_grow(map, 1 + 2 * map->cap);
  }

  assert(is_pow2_uint64(map->cap));
  size_t index = hash_uint64(key);
  size_t last = index;
  do
  {
    index &= map->cap - 1;
    if (!map->keys[index])
    {
      map->keys[index] = key;
      map->ptrs[index] = data;
      map->len++;
      return;
    }
    else if (map->keys[index] == key)
    {
      map->ptrs[index] = data;
      return;
    }
    index++;
  } while (index != last);
  assert(0);
  exit(1);
}

void map_remove(Map* map, uint64_t key)
{
  if (!key)
    return;
  assert(is_pow2_uint64(map->cap));
  size_t index = hash_uint64(key);
  size_t last = index;
  do
  {
    index &= map->cap - 1;
    if (map->keys[index] == key)
    {
      map->keys[index] = 0;
      map->len--;
      return;
    }
    index++;
  } while (index != last);
  assert(0);
  exit(1);
}


int test_map(int argc, char const** argv)
{
  (void)argc, (void)argv;
  Map map = {0};

  for (int i = 0; i < 8; i++)
  {
    assert(map_get(&map, i) == NULL);
  }

  map_put(&map, 6, "hello");
  map_put(&map, 7, "world");

  assert(strcmp("hello", map_get(&map, 6)) == 0);
  assert(strcmp("world", map_get(&map, 7)) == 0);
  assert(2 == map.len);

  map_put(&map, 7, "sekai");
  assert(strcmp("sekai", map_get(&map, 7)) == 0);
  assert(2 == map.len);

  map_remove(&map, 7);
  assert(1 == map.len);

  // test that it grows
  size_t old_cap = map.cap;
  for (size_t next_id = 6; old_cap == map.cap; map_put(&map, next_id++, "grow-me"))
    ;


  return 0;
}
