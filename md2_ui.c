#include "md2_ui.h"

#include "libs/xxxx_buf.h"
#include "libs/xxxx_map.h"

#include <stdarg.h>

void md2_ui_update(MD2_UserInterface* ui)
{
  ui->pointer_position = (MD2_Point2){
    .x = ui->mu->mouse.position.x / ui->pixel_ratio,
    .y = ui->mu->mouse.position.y / ui->pixel_ratio,
  };

  if (ui->frame_started)
  {
    // Stacking order:
    nvgEndFrame(ui->vg);
    nvgEndFrame(ui->overlay_vg);
  }

  nvgBeginFrame(ui->overlay_vg, ui->size.x, ui->size.y, ui->pixel_ratio);
  nvgBeginFrame(ui->vg, ui->size.x, ui->size.y, ui->pixel_ratio);
  ui->frame_started = true;

  ui->bounds = rect_make_point_size((MD2_Point2){0, 0}, ui->size);
}

typedef void(GetBoundsFn)(NVGcontext* vg,
                          float x,
                          float y,
                          char const* text,
                          char const* text_l,
                          float bounds[4]);

static char const* md2_ui__truncate_text(NVGcontext* vg,
                                         float x,
                                         float y,
                                         float width,
                                         char const* text,
                                         char const* text_l,
                                         char const* truncation_point,
                                         GetBoundsFn* get_bounds)
{
  // @todo @defect @unicode should actually truncate to the previous unicode codepoint,
  // not byte
  if (text == text_l)
    return text;
  if (text_l == truncation_point)
    return truncation_point;

  size_t size = text_l - truncation_point;
  char const* mid_point = truncation_point + (size <= 1 ? 1 : size / 2);
  float bounds[4];
  (*get_bounds)(vg, x, y, text, mid_point, bounds);
  if (bounds[2] > x + width)
  {
    truncation_point = md2_ui__truncate_text(
      vg, x, y, width, text, mid_point - 1, truncation_point, get_bounds);
  }
  else
  {
    truncation_point =
      md2_ui__truncate_text(vg, x, y, width, text, text_l, mid_point, get_bounds);
  }
  return truncation_point;
}

void md2_ui_vtextf(MD2_UserInterface* ui,
                   MD2_RenderingOptions options,
                   MD2_Rect2 rect,
                   char* fmt,
                   va_list args)
{
  if (!rect.x1)
    rect.x1 = ui->bounds.x1;
  if (!rect.x0)
    rect.x0 = ui->bounds.x0;

  if (!rects_intersect(ui->bounds, rect))
  {
    return;
  }


  // @todo left truncation (this only does right truncation)

  NVGcontext* vg = options.layer_index ? ui->overlay_vg : ui->vg;
  char* text = NULL;
  buf_vprintf(text, fmt, args);
  if (buf_len(text) == 0)
    return;

  float x = rect.x0, y = rect.y1;


  char* new_end = text
                  + (md2_ui__truncate_text(vg, x, y, rect.x1 - rect.x0, text,
                                           buf_end(text), text, nvgTextBounds)
                     - text);
  if (new_end == text)
    return;

  if (new_end < buf_end(text))
  {
    buf__hdr(text)->len = new_end - text - 2;
    buf_printf(text, u8"\u2026");
  }

  float bounds[4];
  nvgTextBounds(vg, x, y, text, buf_end(text), bounds);

  nvgText(vg, x, y, text, buf_end(text));
  buf_free(text), text = NULL;
}

void md2_ui_textf(
  MD2_UserInterface* ui, MD2_RenderingOptions options, MD2_Rect2 rect, char* fmt, ...)
{
  va_list args;
  va_start(args, fmt);
  md2_ui_vtextf(ui, options, rect, fmt, args);
  va_end(args);
}


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

static void test_truncate_get_bounds(
  NVGcontext* vg, float x, float y, char const* text, char const* text_l, float bounds[4])
{
  bounds[0] = x;
  bounds[1] = y;
  bounds[2] = x + 7 * (text_l - text);
  bounds[3] = y;
}

static void test_truncate()
{
  char const* text = "abc";
  char const* text_l = text + strlen(text);

  char const* enoughspace_l = md2_ui__truncate_text(
    NULL, 0, 3, FLT_MAX, text, text_l, text, test_truncate_get_bounds);
  assert(enoughspace_l == text_l);
  char const* justenoughspace_l = md2_ui__truncate_text(
    NULL, 0, 3, 7 * (text_l - text), text, text_l, text, test_truncate_get_bounds);
  assert(justenoughspace_l == text_l);
  char const* halfspace_l = md2_ui__truncate_text(
    NULL, 0, 3, 7 * (text_l - text) / 2, text, text_l, text, test_truncate_get_bounds);
  assert(halfspace_l == text + (text_l - text) / 2);
}

int test_ui(int argc, char const** argv)
{
  test_rect();
  test_element_allocator();
  test_truncate();
  return 0;
}
