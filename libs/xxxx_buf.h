#ifndef XXXX_BUF
#define XXXX_BUF

#include <assert.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>

enum
{
  BufHeader_Magic = 'BUFX',
};

typedef struct BufHeader
{
  size_t cap;
  size_t len;
  uint32_t magic;
  size_t element_size;
  char bytes[1];
} BufHeader;

char* buf__printf(char* b, char const* fmt, ...);
char* buf__vprintf(char* b, char const* fmt, va_list args);
void* buf__grow(void* buf, size_t element_n, size_t element_size);
void buf__free(void* buf);

#define buf__cast(type__, x__) ((type__)x__)
#define buf__hdr(b__)                                                                    \
  buf__cast(BufHeader*, (buf__cast(char*, b__) - offsetof(BufHeader, bytes)))
#define buf__assert(b__)                                                                 \
  assert(buf__isnull(b__) || buf__hdr(b__)->magic == BufHeader_Magic)
#define buf__assign(b__, expr__) ((b__) = (expr__), buf__assert(b__))
#define buf__isnull(b__) (NULL == &((b__)[0]))

#define buf_fit(b__, added_n__) buf__assign(b__, buf__grow(b__, added_n__, sizeof *b__))

#define buf_free(b__)                                                                    \
  (buf__isnull(b__) ? 0 : (buf__assert(b__), buf__free(b__), buf__assign(b__, NULL), 0))
#define buf_cap(b__) (buf__isnull(b__) ? 0 : (buf__hdr(b__)->cap))
#define buf_len(b__) (buf__isnull(b__) ? 0 : (buf__hdr(b__)->len))
#define buf_end(b__) (buf__isnull(b__) ? NULL : ((&(b__)[0]) + buf_len(b__)))
#define buf_push(b__, ...)                                                               \
  (buf_fit(b__, 1), (b__[buf_len(b__)] = __VA_ARGS__), buf__hdr(b__)->len++)
#define buf_printf(b__, ...) buf__assign(b__, buf__printf((b__), __VA_ARGS__))
#define buf_vprintf(b__, ...) buf__assign(b__, buf__vprintf((b__), __VA_ARGS__))

#endif
