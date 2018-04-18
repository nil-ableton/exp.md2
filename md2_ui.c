#include "md2_ui.h"

#include "libs/xxxx_map.h"

void* md2_ui_alloc(size_t size)
{
  void* ptr = malloc(size);
  if (!ptr)
    md2_fatal("could not allocate");
  return ptr;
}

void md2_ui_free(void* ptr)
{
  free(ptr);
}

static void test_rect()
{
  MD2_Rect2 rect = rect_cover_unit();

  rect = rect_cover_point(rect, (MD2_Point2){2, 3});
  assert(rect.x0 == 2);
  assert(rect.y0 == 3);
  assert(rect.x1 == 2);
  assert(rect.y1 == 3);

  rect = rect_cover_point(rect, (MD2_Point2){0, 5});
  assert(rect.x0 == 0);
  assert(rect.y0 == 3);
  assert(rect.x1 == 2);
  assert(rect.y1 == 5);

  {
    MD2_Rect2 rect0 = rect;
    rect = rect_cover(rect_cover_unit(), rect);
    assert(rect.x0 == rect0.x0);
    assert(rect.y0 == rect0.y0);
    assert(rect.x1 == rect0.x1);
    assert(rect.y1 == rect0.y1);
  }

  {
    MD2_Rect2 rect0 = rect;
    rect = rect_cover(rect, rect_cover_unit());
    assert(rect.x0 == rect0.x0);
    assert(rect.y0 == rect0.y0);
    assert(rect.x1 == rect0.x1);
    assert(rect.y1 == rect0.y1);
  }
}

uint64_t md2_ui_element_allocate(MD2_ElementAllocator* a)
{
  uint64_t id = a->id_l;
  a->id_l++;
  return id;
}

MD2_ElementAllocator* md2_ui_scope(MD2_ElementAllocator* parent)
{
  assert(parent->state == MD2_ElementAllocator_None);
  MD2_ElementAllocator child = {
    .id = md2_ui_element_allocate(parent),
    .parent = parent,
  };
  child.id_l = child.id + 1;
  MD2_ElementAllocator* result = md2_ui_alloc(sizeof *result);
  *result = child;
  parent->state = MD2_ElementAllocator_InChild;
  return result;
}

MD2_ElementAllocator* md2_ui_scope_end(MD2_ElementAllocator* scope)
{
  assert(scope->parent);
  assert(scope->parent->state == MD2_ElementAllocator_InChild);
  MD2_ElementAllocator* parent = scope->parent;
  parent->id_l = scope->id_l;
  parent->state = MD2_ElementAllocator_None;
  md2_ui_free(scope);
  return parent;
}

static void test_allocate_many_recursive(MD2_ElementAllocator* scope,
                                         int n,
                                         int depth,
                                         Map* seen_ids)
{
  if (depth == 0)
    return;

  scope = md2_ui_scope(scope);
  for (int i = 0; i < n; i++)
  {
    uint64_t element_id = md2_ui_element_allocate(scope);
    assert(map_get(seen_ids, element_id) == NULL /* ids should be unique */);
    map_put(seen_ids, element_id, (void*)(intptr_t)1);
  }
  test_allocate_many_recursive(scope, n, depth - 1, seen_ids);
  scope = md2_ui_scope_end(scope);
}

static void test_element_allocator()
{
  MD2_ElementAllocator root = {
    0,
  };

  Map seen_ids = {0};

  MD2_ElementAllocator* scope = &root;
  test_allocate_many_recursive(scope, 8, 3, &seen_ids);
  assert(root.state == MD2_ElementAllocator_None);
  assert(scope->id_l - scope->id == 8 * 3 + 3);
}

int test_ui(int argc, char const** argv)
{
  test_rect();
  test_element_allocator();
  return 0;
}
