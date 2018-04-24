#ifndef POSIX_H
#define POSIX_H

#include <stdbool.h>

bool posix_is_dir(char const* path);

// returned pointer allocated via malloc
char* posix_realpath(char const* path, bool* d_error, char* error_buffer, size_t error_buffer_size);

#endif
