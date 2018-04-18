#ifndef MD2_UI
#define MD2_UI

#include <float.h>

typedef struct MD2_Point2
{
  float x, y;
} MD2_Point2;

typedef struct MD2_Rect2
{
  float x0, y0, x1, y1;
} MD2_Rect2;

struct Mu;
struct NVGcontext;

typedef struct MD2_UserInterface
{
  struct Mu* mu;                 // input & low-level output
  struct NVGcontext* vg;         // nvg canvas
  struct NVGcontext* overlay_vg; // overlay for drag images
  uint64_t hot_element;
  uint64_t active_element;

  struct Map* elements_map;
} MD2_UserInterface;

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

void* md2_ui_alloc(size_t size);
void md2_ui_free(void* ptr);
uint64_t md2_ui_element_allocate(MD2_ElementAllocator* a);
MD2_ElementAllocator* md2_ui_scope(MD2_ElementAllocator* parent);
MD2_ElementAllocator* md2_ui_scope_end(MD2_ElementAllocator* scope);

// Geometry

static inline float min_f(float a, float b)
{
  return a < b ? a : b;
}

static inline float max_f(float a, float b)
{
  return a < b ? b : a;
}

static inline float min_f_unit()
{
  return FLT_MAX;
}

static inline float max_f_unit()
{
  return FLT_MIN;
}

static inline MD2_Rect2 rect_make(MD2_Point2 a, MD2_Point2 b)
{
  return (MD2_Rect2){.x0 = min_f(a.x, b.x),
                     .x1 = max_f(a.x, b.x),
                     .y0 = min_f(a.y, b.y),
                     .y1 = max_f(a.y, b.y)};
}

static inline MD2_Point2 rect_min_point(MD2_Rect2 aabb2)
{
  return (MD2_Point2){.x = aabb2.x0, .y = aabb2.y0};
}

static inline MD2_Point2 rect_max_point(MD2_Rect2 aabb2)
{
  return (MD2_Point2){.x = aabb2.x1, .y = aabb2.y1};
}

static inline bool rect_intersects(MD2_Rect2 aabb2, MD2_Point2 point)
{
  if (point.x < aabb2.x0 || point.x >= aabb2.y1)
    return false;
  if (point.y < aabb2.y0 || point.y >= aabb2.y1)
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
