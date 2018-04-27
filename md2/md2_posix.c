#include "md2_posix.h"

#include <sys/stat.h>
#include <sys/types.h>

#if defined(_WIN32)
#define stat(...) _stat(__VA_ARGS__)
#define S_IFDIR _S_IFDIR

typedef struct _stat64i32 posix_stat_struct;

#else

typedef struct stat posix_stat_struct;

#endif

bool posix_is_dir(char const* path)
{
  posix_stat_struct result;
  if (0 != stat(path, &result))
  {
    return false;
  }
  return result.st_mode & S_IFDIR;
}

#if !defined(_WIN32)

#include <unistd.h>

char* posix_realpath(char const* path, bool* d_error, char* error_buffer, size_t error_buffer_size)
{
	char* rp = _realpath(path, NULL);
	if (!rp) {
		*d_error = true;
		strerror_r(errno, error_buffer, error_buffer_size);
		return NULL;
	}
	*d_error = false;
	return rp;
}
#endif

#if defined(_WIN32)
#undef stat
#undef S_IFDIR
#endif
