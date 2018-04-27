#ifndef IOBUFFER
#define IOBUFFER

#include <stdint.h>

typedef enum IOBufferError {
  IOBufferError_None = 0,
  IOBufferError_PastTheEnd = 1,
  IOBufferError_IO = 2,
} IOBufferError;

typedef struct IOBuffer
{
  uint8_t* bytes_f;
  uint8_t* bytes_i;
  uint8_t* bytes_l;

  IOBufferError error;
  IOBufferError (*refill_)(struct IOBuffer*);
} IOBuffer;

IOBufferError iobuffer_refill(struct IOBuffer* x);

IOBuffer iobuffer_from_memory_size(uint8_t* bytes, size_t bytes_n);

IOBuffer iobuffer_file_writer(char const* path);
void iobuffer_file_writer_close(IOBuffer* iobuffer);

IOBuffer iobuffer_file_reader(char const* path);
void iobuffer_file_reader_close(IOBuffer* iobuffer);


#endif
