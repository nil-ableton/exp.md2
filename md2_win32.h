#if defined(_WIN32)
#ifndef MD2_WIN32
#define MD2_WIN32

// returned pointer allocated via malloc
char* win32_realpath(char const* path, bool* d_error, char* error_buffer, size_t error_buffer_size);

#endif
#endif