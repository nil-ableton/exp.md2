#if !defined(_CRT_SECURE_NO_WARNINGS)
#define _CRT_SECURE_NO_WARNINGS
#endif

#include "md1_support.c"
#include "md2_audio.c"
#include "md2_audioengine.c"
#include "md2_main.c"
#include "md2_posix.c" // platform support, comes last
#include "md2_serialisation.c"
#include "md2_temp_allocator.c"
#include "md2_ui.c"
#if defined(_WIN32)
#include "md2_win32.c"
#endif

#include "deps/glad/src/glad.c"
#include "deps/nanovg/src/nanovg.c"

#include "libs/xxxx_buf.c"
#include "libs/xxxx_iobuffer.c"
#include "libs/xxxx_map.c"
#include "libs/xxxx_queue.c"
#include "libs/xxxx_tasks.c"
