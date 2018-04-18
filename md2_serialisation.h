#ifndef MD2_SERIALISATION
#define MD2_SERIALISATION

#include <stdint.h>

struct IOBuffer;

bool read_uint8_n(struct IOBuffer* in, uint8_t* dest, size_t n);
bool read_uint64(struct IOBuffer* in, uint64_t* dest);
bool read_uint32(struct IOBuffer* in, uint32_t* dest);
bool read_uint16(struct IOBuffer* in, uint16_t* dest);
bool read_uint8(struct IOBuffer* in, uint8_t* dest);
bool read_float32(struct IOBuffer* in, float* dest);
bool read_float64(struct IOBuffer* in, double* dest);
bool write_uint8_n(struct IOBuffer* out, uint8_t* src, size_t n);
bool write_uint64(struct IOBuffer* out, uint64_t* src);
bool write_uint32(struct IOBuffer* out, uint32_t* src);
bool write_uint16(struct IOBuffer* out, uint16_t* src);
bool write_uint8(struct IOBuffer* out, uint8_t* src);

#endif
