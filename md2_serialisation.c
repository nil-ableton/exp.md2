#include "md2_serialisation.h"

#include "libs/xxxx_iobuffer.h"

#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>

bool read_uint8_n(IOBuffer* in, uint8_t* dest, size_t n)
{
  while (n && in->error == IOBufferError_None)
  {
    if (in->bytes_i == in->bytes_l)
      iobuffer_refill(in);
    while (n && in->bytes_i < in->bytes_l)
    {
      *dest = *in->bytes_i;
      dest++;
      in->bytes_i++;
      n--;
    }
  }
  return n == 0;
}

inline uint64_t shift_uint64(uint64_t b, int offset)
{
  return b << offset;
}

inline uint32_t shift_uint32(uint32_t b, int offset)
{
  return b << offset;
}

inline uint16_t shift_uint16(uint16_t b, int offset)
{
  return b << offset;
}

inline uint8_t unpack8_uint64(uint64_t x, int offset)
{
  return (x >> offset) & 0xff;
}

inline uint8_t unpack8_uint32(uint32_t x, int offset)
{
  return (x >> offset) & 0xff;
}

inline uint8_t unpack8_uint16(uint16_t x, int offset)
{
  return (x >> offset) & 0xff;
}

bool read_uint64(IOBuffer* in, uint64_t* dest)
{
  uint8_t bytes[sizeof *dest];
  if (!read_uint8_n(in, bytes, sizeof bytes))
    return false;
  *dest = bytes[0] | shift_uint64(bytes[1], 8) | shift_uint64(bytes[2], 16)
          | shift_uint64(bytes[3], 24) | shift_uint64(bytes[4], 32)
          | shift_uint64(bytes[5], 40) | shift_uint64(bytes[6], 48)
          | shift_uint64(bytes[7], 56);
  return true;
}

bool read_uint32(IOBuffer* in, uint32_t* dest)
{
  uint8_t bytes[sizeof *dest];
  if (!read_uint8_n(in, bytes, sizeof bytes))
    return false;
  *dest = bytes[0] | shift_uint32(bytes[1], 8) | shift_uint32(bytes[2], 16)
          | shift_uint32(bytes[3], 24);
  return true;
}

bool read_uint16(IOBuffer* in, uint16_t* dest)
{
  uint8_t bytes[sizeof *dest];
  if (!read_uint8_n(in, bytes, sizeof bytes))
    return false;
  *dest = bytes[0] | shift_uint16(bytes[1], 8);
  return true;
}

bool read_uint8(IOBuffer* in, uint8_t* dest)
{
  return read_uint8_n(in, dest, 1);
}

bool write_uint8_n(IOBuffer* out, uint8_t* src, size_t n)
{
  while (n && out->error == IOBufferError_None)
  {
    if (out->bytes_i == out->bytes_l)
      iobuffer_refill(out);
    while (n && out->bytes_i < out->bytes_l)
    {
      *out->bytes_i = *src;
      src++;
      out->bytes_i++;
      n--;
    }
  }
  return n == 0;
}


bool write_uint64(IOBuffer* out, uint64_t* src)
{
  uint64_t x = *src;
  uint8_t bytes[sizeof *src] = {
    unpack8_uint64(x, 0),  unpack8_uint64(x, 8),  unpack8_uint64(x, 16),
    unpack8_uint64(x, 24), unpack8_uint64(x, 32), unpack8_uint64(x, 40),
    unpack8_uint64(x, 48), unpack8_uint64(x, 56),
  };
  return write_uint8_n(out, bytes, sizeof bytes);
}

bool write_uint32(IOBuffer* out, uint32_t* src)
{
  uint32_t x = *src;
  uint8_t bytes[sizeof *src] = {
    unpack8_uint32(x, 0),
    unpack8_uint32(x, 8),
    unpack8_uint32(x, 16),
    unpack8_uint32(x, 24),
  };
  return write_uint8_n(out, bytes, sizeof bytes);
}

bool write_uint16(IOBuffer* out, uint16_t* src)
{
  uint32_t x = *src;
  uint8_t bytes[sizeof *src] = {
    unpack8_uint16(x, 0),
    unpack8_uint16(x, 8),
  };
  return write_uint8_n(out, bytes, sizeof bytes);
}

bool write_uint8(IOBuffer* out, uint8_t* src)
{
  return write_uint8_n(out, &src[0], 1);
}

int test_serialisation(int argc, char const** argv)
{
  uint8_t data_le[] = {0xff, 0x00, 0xfe, 0x01, 0xfd, 0x02, 0xfc, 0x03, 0xfb,
                       0x04, 0xfa, 0x05, 0xf9, 0x06, 0xf8, 0x07, 0xf7};

  IOBuffer buffer = {
    .bytes_f = &data_le[0],
    .bytes_i = &data_le[0],
    .bytes_l = &data_le[0] + sizeof data_le,
  };

  uint64_t y64;
  assert(read_uint64(&buffer, &y64));
  assert(y64 == 0x03fc02fd01fe00ff);
  uint32_t y32;
  assert(read_uint32(&buffer, &y32));
  assert(y32 == 0x05fa04fb);
  uint16_t y16;
  assert(read_uint16(&buffer, &y16));
  assert(y16 == 0x06f9);
  uint8_t y8;
  assert(read_uint8(&buffer, &y8));
  assert(y8 == 0xf8);

  uint8_t output[8 + 4 + 2 + 1] = {
    0,
  };
  IOBuffer out = {
    .bytes_f = &output[0],
    .bytes_i = &output[0],
    .bytes_l = &output[sizeof output],
  };
  uint64_t x64 = 0x03fc02fd01fe00ff;
  write_uint64(&out, &x64);
  uint32_t x32 = 0x05fa04fb;
  write_uint32(&out, &x32);
  uint16_t x16 = 0x06f9;
  write_uint16(&out, &x16);
  uint8_t x8 = 0xf8;
  write_uint8(&out, &x8);
  assert(sizeof output < sizeof data_le);
  assert(0 == memcmp(&data_le[0], &output, sizeof output));

  for (int n = 8; n > 0;)
  {
    n--;
    IOBuffer test_buffer = {
      .bytes_f = &data_le[0],
      .bytes_l = &data_le[n],
      .bytes_i = &data_le[0],
    };
    if (n < 1)
    {
      buffer = test_buffer;
      assert(read_uint8(&buffer, &y8) == false);
    }
    if (n < 2)
    {
      buffer = test_buffer;
      assert(read_uint16(&buffer, &y16) == false);
    }
    if (n < 4)
    {
      buffer = test_buffer;
      assert(read_uint32(&buffer, &y32) == false);
    }
    if (n < 8)
    {
      buffer = test_buffer;
      assert(read_uint64(&buffer, &y64) == false);
    }
  }
  return 0;
}

// derived

bool read_int64(IOBuffer* in, int64_t* dest)
{
  uint64_t y;
  if (!read_uint64(in, &y))
    return false;
  memcpy(dest, &y, sizeof *dest);
  return true;
}

bool read_int32(IOBuffer* in, int64_t* dest)
{
  uint32_t y;
  if (!read_uint32(in, &y))
    return false;
  memcpy(dest, &y, sizeof *dest);
  return true;
}

bool read_int16(IOBuffer* in, int64_t* dest)
{
  uint16_t y;
  if (!read_uint16(in, &y))
    return false;
  memcpy(dest, &y, sizeof *dest);
  return true;
}

bool read_int8(IOBuffer* in, int64_t* dest)
{
  int8_t y;
  if (!read_uint8(in, &y))
    return false;
  memcpy(dest, &y, sizeof *dest);
  return true;
}

bool read_float32(IOBuffer* in, float* dest)
{
  uint32_t y;
  if (!read_uint32(in, &y))
    return false;
  memcpy(dest, &y, sizeof *dest);
  return true;
}

bool read_float64(IOBuffer* in, double* dest)
{
  uint64_t y;
  if (!read_uint64(in, &y))
    return false;
  memcpy(dest, &y, sizeof *dest);
  return true;
}
