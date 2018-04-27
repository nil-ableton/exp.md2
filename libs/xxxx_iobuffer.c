#include "xxxx_iobuffer.h"

#include <assert.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define IOBUFFER_INTERNAL static

#define IOBUFFER_CAST(ptype, pexpr) ((ptype)(pexpr))
#define IOBUFFER_MEMBER_TO_BASE(t, mn, p) ((p)-offsetof(t, mn))

#define IOBUFFER_PCAST_FROM_MEMBER(dt, mn, p)                                            \
  IOBUFFER_CAST(dt*, IOBUFFER_MEMBER_TO_BASE(dt, mn, p))

IOBUFFER_INTERNAL enum IOBufferError iobuffer_refill_empty(IOBuffer* x)
{
  x->bytes_f = x->bytes_i = x->bytes_l = 0;
  x->refill_ = iobuffer_refill_empty;
  return x->error =
           (x->error == IOBufferError_None ? IOBufferError_PastTheEnd : x->error);
}

IOBUFFER_INTERNAL enum IOBufferError iobuffer_fail(IOBuffer* x, enum IOBufferError error)
{
  iobuffer_refill_empty(x);
  return x->error = error;
}

enum IOBufferError iobuffer_refill(IOBuffer* x)
{
  if (!x->refill_)
  {
    x->refill_ = iobuffer_refill_empty;
  }
  return x->refill_(x);
}

IOBUFFER_INTERNAL enum IOBufferError iobuffer_memory_range_refill(IOBuffer *x)
{
    return iobuffer_fail(x, IOBufferError_PastTheEnd);
}

IOBuffer iobuffer_from_memory_size(uint8_t* bytes, size_t bytes_n)
{
    IOBuffer buffer = {
        .bytes_f = &bytes[0],
        .bytes_l = &bytes[bytes_n],
    };
    buffer.bytes_i = buffer.bytes_f;
    buffer.refill_ = iobuffer_memory_range_refill;
    return buffer;
}


struct FileIOBuffer
{
  FILE* file;
  uint8_t buffer[4096];
  int is_writing;
};

IOBUFFER_INTERNAL enum IOBufferError iobuffer_stdio_fail(IOBuffer* x,
                                                         enum IOBufferError error)
{
  struct FileIOBuffer* file_io_buffer =
    IOBUFFER_PCAST_FROM_MEMBER(struct FileIOBuffer, buffer, x->bytes_f);
  fclose(file_io_buffer->file);
  free(file_io_buffer);
  return iobuffer_fail(x, error);
}

IOBUFFER_INTERNAL enum IOBufferError iobuffer_stdio_refill(IOBuffer* x)
{
  struct FileIOBuffer* file_io_buffer =
    IOBUFFER_PCAST_FROM_MEMBER(struct FileIOBuffer, buffer, x->bytes_f);
  if (file_io_buffer->is_writing)
  {
    size_t n = x->bytes_i - x->bytes_f;
    if (n == 0)
      return iobuffer_stdio_fail(x, IOBufferError_PastTheEnd);
    size_t fwrite_n = fwrite(x->bytes_f, n, 1, file_io_buffer->file);
    if (fwrite_n != n)
    {
      return iobuffer_stdio_fail(x, IOBufferError_IO);
    }
  }
  else
  {
    size_t fread_n = fread(
      &file_io_buffer->buffer[0], 1, sizeof file_io_buffer->buffer, file_io_buffer->file);
    if (fread_n == 0 && ferror(file_io_buffer->file))
    {
      return iobuffer_stdio_fail(x, IOBufferError_IO);
    }
    else if (fread_n == 0)
    {
      assert(feof(file_io_buffer->file));
      return iobuffer_stdio_fail(x, IOBufferError_PastTheEnd);
    }
    x->bytes_l = x->bytes_f + fread_n;
  }
  x->bytes_i = x->bytes_f;
  return x->error = IOBufferError_None;
}

IOBUFFER_INTERNAL void iobuffer_stdio_write_and_close(IOBuffer* x)
{
  struct FileIOBuffer* file_io_buffer =
    IOBUFFER_PCAST_FROM_MEMBER(struct FileIOBuffer, buffer, x->bytes_f);

  assert(file_io_buffer->is_writing);

  size_t fwrite_n = fwrite(x->bytes_f, x->bytes_i - x->bytes_f, 1, file_io_buffer->file);
  fclose(file_io_buffer->file);
  free(file_io_buffer);
  iobuffer_refill_empty(x);
  if (fwrite_n != 1)
  {
    x->error = IOBufferError_IO;
  }
}

IOBUFFER_INTERNAL void iobuffer_stdio_close(IOBuffer* x)
{
  struct FileIOBuffer* file_io_buffer =
    IOBUFFER_PCAST_FROM_MEMBER(struct FileIOBuffer, buffer, x->bytes_f);
  fclose(file_io_buffer->file);
  free(file_io_buffer);
  iobuffer_refill_empty(x);
}

IOBUFFER_INTERNAL struct FileIOBuffer* iobuffer_stdio_init(IOBuffer* iobuffer, FILE* file)
{
  memset(iobuffer, 0, sizeof *iobuffer);
  if (!file)
  {
    iobuffer_refill_empty(iobuffer);
    return NULL;
  }
  struct FileIOBuffer* file_io_buffer = calloc(sizeof *file_io_buffer, 1);
  file_io_buffer->file = file;
  iobuffer->bytes_f = iobuffer->bytes_i = &file_io_buffer->buffer[0];
  iobuffer->bytes_l = iobuffer->bytes_f;
  iobuffer->refill_ = iobuffer_stdio_refill;
  return file_io_buffer;
}

IOBuffer iobuffer_file_reader(char const* path)
{
  IOBuffer iobuffer;
  struct FileIOBuffer* file_io_buffer = iobuffer_stdio_init(&iobuffer, fopen(path, "rb"));
  if (file_io_buffer)
  {
    iobuffer.bytes_l = iobuffer.bytes_f;
    file_io_buffer->is_writing = 0;
  }
  iobuffer_refill(&iobuffer);
  return iobuffer;
}

IOBuffer iobuffer_file_writer(char const* path)
{
  IOBuffer iobuffer;
  struct FileIOBuffer* file_io_buffer = iobuffer_stdio_init(&iobuffer, fopen(path, "wb"));
  if (file_io_buffer)
  {
    iobuffer.bytes_l = &file_io_buffer->buffer[sizeof file_io_buffer->buffer];
    file_io_buffer->is_writing = 1;
  }
  return iobuffer;
}

void iobuffer_file_reader_close(IOBuffer* iobuffer)
{
  if (iobuffer->refill_ == iobuffer_stdio_refill)
  {
    iobuffer_stdio_close(iobuffer);
  }
}

void iobuffer_file_writer_close(IOBuffer* iobuffer)
{
  if (iobuffer->refill_ == iobuffer_stdio_refill)
  {
    iobuffer_stdio_write_and_close(iobuffer);
  }
}

int test_iobuffer(int argc, char const** argv)
{
  (void)argc, (void)argv;

  for (IOBuffer out = iobuffer_file_writer("iobuffer_test.bin");
       out.error == IOBufferError_None;)
  {
    char const* input = "hello, world";
    while (out.bytes_i < out.bytes_l && *input)
    {
      *out.bytes_i = *input;
      input++;
      out.bytes_i++;
    }
    assert(out.bytes_i > out.bytes_f);
    iobuffer_file_writer_close(&out);
    break;
  }

  for (IOBuffer in = iobuffer_file_reader("iobuffer_test.bin");
       assert(in.error == IOBufferError_None), in.error == IOBufferError_None;)
  {
    char const* sentinel = "hello, world";
    while (in.bytes_i < in.bytes_l && *sentinel)
    {
      assert(*in.bytes_i == *sentinel);
      sentinel++;
      in.bytes_i++;
    }
    assert(in.bytes_i == in.bytes_l);
    iobuffer_file_reader_close(&in);
    break;
  }
  return 0;
}
