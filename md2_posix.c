#include <sys/stat.h>
#include <sys/types.h>

#if defined(_WIN32)
#define stat(...) _stat(__VA_ARGS__)
#define S_IFDIR _S_IFDIR

#endif

bool posix_is_dir(char const* path)
{
  struct stat result;
  if (0 != stat(path, &result))
  {
    return false;
  }
  return result.st_mode & S_IFDIR;
}

#if defined(_WIN32)
#undef stat
#undef S_IFDIR
#endif
