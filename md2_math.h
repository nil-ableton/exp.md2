#ifndef MD2_MATH
#define MD2_MATH

#include <float.h>

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

static inline intmax_t max_i(intmax_t a, intmax_t b)
{
  return a < b ? b : a;
}

static inline uintptr_t round_up_multiple_of_pot_uintptr(uintptr_t x, uintptr_t pot)
{
  return (x + pot - 1) & ~(pot - 1);
}

static inline float interpolate_linear_f(float a, float b, float coeff)
{
  return (1.0f - coeff) * a + coeff * b;
}

#endif
