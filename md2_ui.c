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

NVGcontext* md2_ui_vg(MD2_UserInterface* ui, MD2_UIElement element)
{
  return element.layer ? ui->overlay_vg : ui->vg;
}

void md2_ui_vtextf(MD2_UserInterface* ui, MD2_UIElement element, char* fmt, va_list args)
{
  MD2_Rect2 rect = element.rect;
  if (!rect.x1)
    rect.x1 = max_f(rect.x0, ui->bounds.x1);
  if (!rect.x0)
    rect.x0 = min_f(rect.x0, ui->bounds.x0);

  if (!rects_intersect(ui->bounds, rect))
  {
    return;
  }


  // @todo left truncation (this only does right truncation)

  NVGcontext* vg = md2_ui_vg(ui, element);
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

void md2_ui_textf(MD2_UserInterface* ui, MD2_UIElement element, char* fmt, ...)
{
  va_list args;
  va_start(args, fmt);
  md2_ui_vtextf(ui, element, fmt, args);
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

static void test_rect_exhaustive()
{
  MD2_Rect2 rect = {
    .x0 = 0.0f,
    .x1 = 1.0f,
    .y0 = 2.0f,
    .y1 = 3.0f,
  };
  for (float y = rect.y0; y < rect.y1; y += 0.001f)
  {
    for (float x = rect.x0; x < rect.x1; x += 0.001f)
    {
      MD2_Point2 p = {x, y};
      assert(rect_intersects(rect, p));
    }
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


void md2_ui_rect(MD2_UserInterface* ui, MD2_UIElement element, NVGcolor color)
{
  NVGcontext* vg = md2_ui_vg(ui, element);
  MD2_Rect2 rect = element.rect;
  nvgBeginPath(vg);
  nvgRect(vg, rect.x0, rect.y0, rect.x1 - rect.x0, rect.y1 - rect.y0);
  nvgFillColor(vg, color);
  nvgFill(vg);
}

void md2_ui_waveform(MD2_UserInterface* ui,
                     MD2_UIElement element,
                     WaveformData const* waveform)
{
  if (!rects_intersect(ui->bounds, element.rect))
    return;
  float top_y = element.rect.y0;
  float left_x = element.rect.x0;
  float size_x = element.rect.x1 - element.rect.x0;
  float size_y = element.rect.y1 - element.rect.y0;
  float halfsize_y = size_y / 2.0f;
  float inc_x = size_x / waveform->len_pot;
  float mid_y = top_y + halfsize_y;


  NVGcontext* vg = md2_ui_vg(ui, element);

  NVGcolor color = nvgRGBA(255, 192, 255, 160);
  NVGcolor red_color = nvgRGBA(255, 40, 60, 160);
  NVGcolor rms_color = nvgRGBA(255, 200, 255, 160);

  NVGpaint min_gradient =
    nvgLinearGradient(vg, left_x, mid_y, left_x, mid_y + halfsize_y, color, red_color);

  NVGpaint max_gradient =
    nvgLinearGradient(vg, left_x, mid_y, left_x, mid_y - halfsize_y, color, red_color);

  float c_x;
  c_x = left_x;

  MD2_Rect2 intersecting_rect = {0};
  size_t intersecting_chunk_index;
  for (size_t chunk_index = 0; chunk_index < waveform->len_pot;
       chunk_index++, c_x += inc_x)
  {
    MD2_Rect2 rect = {.x0 = c_x,
                      .x1 = max_f(c_x + 1, c_x + inc_x),
                      .y0 = element.rect.y0,
                      .y1 = element.rect.y1};
    if (rect_intersects(rect, ui->pointer_position))
    {
      intersecting_chunk_index = chunk_index;
      intersecting_rect = rect;
    }
    if (c_x > ui->pointer_position.x)
      break;
  }

  if (intersecting_rect.x0 != intersecting_rect.x1)
  {
    md2_ui_rect(
      ui, (MD2_UIElement){.rect = intersecting_rect}, nvgRGBA(255, 255, 255, 128));
    md2_ui_textf(
      ui,
      (MD2_UIElement){.layer = 1, .rect = {.x0 = intersecting_rect.x0 + 10, .y1 = mid_y}},
      "[%f, %f]", waveform->min[intersecting_chunk_index],
      waveform->max[intersecting_chunk_index]);
  }

  nvgBeginPath(vg);
  c_x = left_x;
  for (size_t chunk_index = 0; chunk_index < waveform->len_pot;
       chunk_index++, c_x += inc_x)
  {
    float size_y = -halfsize_y * waveform->min[chunk_index];
    nvgRect(vg, c_x, mid_y, inc_x, size_y);
  }
  nvgFillPaint(vg, min_gradient);
  nvgFill(vg);

  nvgBeginPath(vg);
  c_x = left_x;
  for (size_t chunk_index = 0; chunk_index < waveform->len_pot;
       chunk_index++, c_x += inc_x)
  {
    float size_y = -halfsize_y * waveform->max[chunk_index];
    nvgRect(vg, c_x, mid_y, inc_x, size_y);
  }
  nvgFillPaint(vg, max_gradient);
  nvgFill(vg);

  nvgBeginPath(vg);
  c_x = left_x;
  for (size_t chunk_index = 0; chunk_index < waveform->len_pot;
       chunk_index++, c_x += inc_x)
  {
    if (waveform->min[chunk_index] < -1.0f || waveform->max[chunk_index] > 1.0f)
    {
      float min_y = mid_y + -halfsize_y * waveform->max[chunk_index];
      float max_y = mid_y + -halfsize_y * waveform->min[chunk_index];
      nvgRect(vg, c_x, max_y, inc_x, min_y - max_y);
    }
  }
  nvgFillColor(vg, red_color);
  nvgFill(vg);

  nvgBeginPath(vg);
  c_x = left_x;
  for (size_t chunk_index = 0; chunk_index < waveform->len_pot;
       chunk_index++, c_x += inc_x)
  {
    float min_y = halfsize_y * waveform->rms[chunk_index];
    float max_y = -halfsize_y * waveform->rms[chunk_index];
    nvgRect(vg, c_x, mid_y + min_y, inc_x, max_y - min_y);
  }
  nvgFillColor(vg, rms_color);
  nvgFill(vg);
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

  map_free(&seen_ids);
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
  test_rect_exhaustive();
  test_element_allocator();
  test_truncate();
  return 0;
}
