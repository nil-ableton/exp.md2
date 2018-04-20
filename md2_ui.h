#ifndef MD2_UI
#define MD2_UI

#include <float.h>

typedef struct MD2_Point2
{
  float x, y;
} MD2_Point2;

typedef struct MD2_Vec2
{
  float x, y;
} MD2_Vec2;

typedef struct MD2_Rect2
{
  float x0, y0, x1, y1;
} MD2_Rect2;

struct Mu;
struct NVGcontext;

typedef struct MD2_UserInterface
{
  // User supplied on update:
  MD2_Vec2 size;
  float pixel_ratio;

  MD2_Point2 pointer_position;
  MD2_Rect2 bounds;
  bool frame_started;
  uint64_t hot_element;
  uint64_t active_element;
  struct Map* elements_map;

  struct Mu* mu;                 // input & low-level output
  struct NVGcontext* vg;         // nvg canvas
  struct NVGcontext* overlay_vg; // overlay for drag images
} MD2_UserInterface;

typedef struct MD2_UIElement
{
  int layer;
  MD2_Rect2 rect;
} MD2_UIElement;

struct NVGcontext* md2_ui_vg(MD2_UserInterface* ui, MD2_UIElement element);
void md2_ui_rect(MD2_UserInterface* ui, MD2_UIElement element, struct NVGcolor color);
void md2_ui_textf(MD2_UserInterface* ui, MD2_UIElement element, char* fmt, ...);
void md2_ui_vtextf(MD2_UserInterface* ui, MD2_UIElement rect, char* fmt, va_list args);
void md2_ui_waveform(MD2_UserInterface* ui,
                     MD2_UIElement element,
                     WaveformData const* waveform);

typedef struct MD2_ElementAllocator
{
  uint64_t id;
  uint64_t id_l;
  struct MD2_ElementAllocator* parent;
  enum
  {
    MD2_ElementAllocator_None,
    MD2_ElementAllocator_InChild,
  } state;
} MD2_ElementAllocator;

void md2_ui_update(MD2_UserInterface* ui);

void* md2_ui_alloc(size_t size);
void md2_ui_free(void* ptr);
uint64_t md2_ui_element_allocate(MD2_ElementAllocator* a);
MD2_ElementAllocator* md2_ui_scope(MD2_ElementAllocator* parent);
MD2_ElementAllocator* md2_ui_scope_end(MD2_ElementAllocator* scope);

// Geometry

static inline MD2_Vec2 to_point_from_origin(MD2_Point2 a)
{
  return (MD2_Vec2){a.x, a.y};
}

static inline MD2_Rect2 rect_make(MD2_Point2 a, MD2_Point2 b)
{
  return (MD2_Rect2){.x0 = min_f(a.x, b.x),
                     .x1 = max_f(a.x, b.x),
                     .y0 = min_f(a.y, b.y),
                     .y1 = max_f(a.y, b.y)};
}

static inline MD2_Rect2 rect_make_valid(MD2_Rect2 a)
{
  if (a.x0 > a.x1)
    a.x0 = a.x1;
  if (a.y0 > a.y1)
    a.y0 = a.y1;
  return a;
}

static inline MD2_Rect2 rect_make_point_size(MD2_Point2 point, MD2_Vec2 size)
{
  return rect_make(point, (MD2_Point2){point.x + size.x, point.y + size.y});
}

static inline MD2_Rect2 rect_translate(MD2_Rect2 rect, MD2_Vec2 delta)
{
  rect.x0 += delta.x;
  rect.x1 += delta.x;
  rect.y0 += delta.y;
  rect.y1 += delta.y;
  return rect;
}


// @todo translated/expanded and the likes is a better name for things which aren't action
static inline MD2_Rect2 rect_expand(MD2_Rect2 rect, MD2_Vec2 margin)
{
  rect.x0 -= margin.x;
  rect.x1 += margin.x;
  rect.y0 -= margin.y;
  rect.y1 += margin.y;
  return rect_make_valid(rect);
}

static inline MD2_Point2 rect_min_point(MD2_Rect2 aabb2)
{
  return (MD2_Point2){.x = aabb2.x0, .y = aabb2.y0};
}

static inline MD2_Point2 rect_max_point(MD2_Rect2 aabb2)
{
  return (MD2_Point2){.x = aabb2.x1, .y = aabb2.y1};
}

static inline MD2_Rect2 rect_intersection(MD2_Rect2 a, MD2_Rect2 b)
{
  return (MD2_Rect2){
    .x0 = max_f(a.x0, b.x0),
    .x1 = min_f(a.x1, b.x1),
    .y0 = max_f(a.y0, b.y0),
    .y1 = min_f(a.y1, b.y1),
  };
}

static inline bool rect_intersects(MD2_Rect2 aabb2, MD2_Point2 point)
{
  assert(aabb2.x0 <= aabb2.x1 && aabb2.y0 <= aabb2.y1);
  if (point.x < aabb2.x0 || point.x >= aabb2.x1)
    return false;
  if (point.y < aabb2.y0 || point.y >= aabb2.y1)
    return false;
  return true;
}

static inline bool rects_intersect(MD2_Rect2 a, MD2_Rect2 b)
{
  assert(a.x0 <= a.x1 && a.y0 <= a.y1);
  assert(b.x0 <= b.x1 && b.y0 <= b.y1);
  if (b.x0 >= a.x1)
    return false;
  if (a.x0 >= b.x1)
    return false;
  if (b.y0 >= a.y1)
    return false;
  if (a.y0 >= b.y1)
    return false;
  return true;
}

static inline MD2_Rect2 rect_cover_unit()
{
  return (MD2_Rect2){
    .x0 = min_f_unit(),
    .x1 = max_f_unit(),
    .y0 = min_f_unit(),
    .y1 = max_f_unit(),
  };
}

static inline MD2_Rect2 rect_cover_point(MD2_Rect2 aabb2, MD2_Point2 point)
{
  return (MD2_Rect2){
    .x0 = min_f(aabb2.x0, point.x),
    .x1 = max_f(aabb2.x1, point.x),
    .y0 = min_f(aabb2.y0, point.y),
    .y1 = max_f(aabb2.y1, point.y),
  };
}

static inline MD2_Rect2 rect_cover(MD2_Rect2 a, MD2_Rect2 b)
{
  return (MD2_Rect2){
    .x0 = min_f(a.x0, b.x0),
    .x1 = max_f(a.x1, b.x1),
    .y0 = min_f(a.y0, b.y0),
    .y1 = max_f(a.y1, b.y1),
  };
}

#endif
