#include "md1_support.h"
#include "md2_posix.h"
#include "md2_ui.h"

#include "libs/xxxx_buf.h"
#include "libs/xxxx_iobuffer.h"
#include "libs/xxxx_map.h"
#include "libs/xxxx_mu.h"

#if defined(_WIN32)
#include "libs/xxxx_mu_win32.h"
#endif

#include "deps/gl.h"
#define NANOVG_GL3_IMPLEMENTATION
#include "deps/nanovg/src/nanovg.h"
#include "deps/nanovg/src/nanovg_gl.h"


#include <assert.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int test_buf(int, char const**);
int test_map(int, char const**);
int test_iobuffer(int, char const**);
int test_serialisation(int argc, char const** argv);
int test_ui(int, char const**);

static printf_line(int indent, char const* fmt, ...)
{
  va_list args;
  va_start(args, fmt);
  printf("% *s", indent, "");
  vprintf(fmt, args);
  va_end(args);
  printf("\n");
}

int comp_uint64(const void* a_ptr, const void* b_ptr)
{
  uint64_t a, b;
  memcpy(&a, a_ptr, sizeof a);
  memcpy(&b, b_ptr, sizeof b);
  return a < b ? -1 : (a > b ? +1 : 0);
}

int test_main(int argc, char const* argv[])
{
  IOBuffer reader = iobuffer_file_reader("test_assets/md1_songs.bin");
  MD1_Loader loader = {
    .input = &reader,
  };
  MD1_EntityCatalog entities = {0};
  assert(md1_load(&loader, &entities));
  iobuffer_file_reader_close(&reader);

  // Print some of the document + do some validation

  Map unused_files = {0};
  for (MD1_File *file_i = &entities.files[0], *file_l = &file_i[entities.files_n];
       file_i < file_l; file_i++)
  {
    map_put(&unused_files, file_i->id, (void*)((intptr_t)1));
  }
  assert(unused_files.len == entities.files_n);

  int indent = 0;
  for (MD1_Song *song_i = entities.songs, *song_l = &song_i[entities.songs_n];
       song_i < song_l; song_i++)
  {
    printf_line(indent, "Song {"), indent += 4;
    printf_line(indent, "id = %llu,", song_i->id);
    printf_line(indent, "scenes_n = %llu,", song_i->scene_n);

    printf_line(indent, "tracks = {"), indent += 4;
    for (uint64_t *track_id_i = &song_i->audiotracks_ids[0],
                  *track_id_l = &track_id_i[song_i->audiotracks_ids_n];
         track_id_i < track_id_l; track_id_i++)
    {
      if (*track_id_i == 0)
        continue;
      printf_line(indent, "Track {"), indent += 4;
      MD1_SongTrack* track = &entities.songtracks[*track_id_i - 1];
      assert(track->id == *track_id_i);
      printf_line(indent, "id = %llu,", track->id);
      printf_line(
        indent, "playing_session_slot_index = %llu,", track->playing_session_slot_index);
      printf_line(
        indent, "@derived clip_id_in_playing_session_slot = %llu,",
        track->audioclip_id_in_session_slots[track->playing_session_slot_index]);
      printf_line(indent, "audioclips = {"), indent += 4;
      for (uint64_t *clip_id_i = &track->audioclip_id_in_session_slots[0],
                    *clip_id_l = &clip_id_i[track->audioclip_id_in_session_slots_n];
           clip_id_i < clip_id_l; clip_id_i++)
      {
        if (*clip_id_i == 0)
          continue;
        printf_line(indent, "AudioClip {"), indent += 4;
        MD1_AudioClip* clip = &entities.audioclips[*clip_id_i - 1];
        assert(clip->id == *clip_id_i);
        printf_line(indent, "id = %llu,", clip->id);
        printf_line(indent, "file_id = %llu,", clip->file_id);
        map_remove(&unused_files, clip->file_id);
        indent -= 4, printf_line(indent, "},");
      }
      indent -= 4, printf_line(indent, "},");

      indent -= 4, printf_line(indent, "},");
    }
    indent -= 4, printf_line(indent, "},");

    indent -= 4, printf_line(indent, "},");
  }

  uint64_t* files = NULL;
  for (uint64_t *file_i = &unused_files.keys[0],
                *file_l = &unused_files.keys[unused_files.cap];
       file_i < file_l; file_i++)
  {
    if (*file_i == 0)
      continue;
    buf_push(files, *file_i);
  }

  qsort(&files[0], buf_len(files), sizeof files[0], comp_uint64);
  for (uint64_t *file_i = &files[0], *file_l = buf_end(files); file_i < file_l; file_i++)
  {
    printf("Unused file: %llu\n", *file_i);
  }


  return 0;
}

static void md2_fatal(char const* fmt, ...)
{
  va_list args;
  va_start(args, fmt);
  printf("ERROR: ");
  vprintf(fmt, args);
  printf("\n");
  va_end(args);
  assert(0);
  exit(1);
}

typedef struct WindowMetrics
{
  float device_px_per_ref_points;
  struct Mu_Int2 window_size;
  float dpi;
} WindowMetrics;

static WindowMetrics get_window_metrics(struct Mu* mu)
{
  WindowMetrics metrics = {
    .device_px_per_ref_points = 1.0f,
    .window_size = mu->window.size,
    .dpi = 96.0,
  };
#if defined(_WIN32)
  UINT Dpi = GetDpiForWindow(mu->win32->window);
  metrics.device_px_per_ref_points = Dpi / 96.0;
  metrics.dpi = Dpi;
#endif
  return metrics;
}

static char* str_dup_range(char* f, char* l)
{
  char* r = calloc(1, l - f + 1);
  memcpy(&r[0], &f[0], l - f);
  r[l - f] = '\0';
  return r;
}

static char* get_exe_dir()
{
#if defined(_WIN32)
  char* buffer = NULL;
  char* buffer_l = NULL;
  buf_fit(buffer, MAX_PATH);
  while (1)
  {
    DWORD size = GetModuleFileNameA(NULL, buffer, buf_cap(buffer));
    if (GetLastError() == ERROR_INSUFFICIENT_BUFFER || size == 0)
    {
      buf_fit(buffer, 256);
    }
    else
    {
      buffer_l = buffer + size;
      break;
    }
  }
  assert(*buffer_l == 0);
  char* last_dir_separator = NULL;
  for (char* buffer_i = buffer_l; buffer_i > buffer; buffer_i--)
  {
    if (buffer_i[-1] == '\\')
    {
      last_dir_separator = &buffer_i[-1];
      break;
    }
  }
  if (!last_dir_separator)
    md2_fatal("can't get installation directory");
  char* result = str_dup_range(buffer, last_dir_separator);
  buf_free(buffer);
  return result;
#else
#error Implement me
#endif
}

void load_fonts(NVGcontext* vg)
{
  char* exe_dir = get_exe_dir();
  {
    char* path = NULL;
    buf_printf(path, "%s/assets/Inter-UI-Regular.ttf", exe_dir);

    if (nvgCreateFont(vg, "fallback", path) == -1)
      md2_fatal("could not load fallback font: %s\n", path);
    buf_free(path), path = NULL;
  }
  free(exe_dir), exe_dir = NULL;
}

int main(int argc, char const* argv[])
{
// @todo @platform{win32}
#if defined(_WIN32)
  SetThreadDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE);
#endif

  test_buf(argc, argv);
  test_map(argc, argv);
  test_iobuffer(argc, argv);
  test_serialisation(argc, argv);
  test_main(argc, argv);
  test_ui(argc, argv);

  char const* user_library_path = "";
  for (char const **arg = &argv[0], **argl = &argv[argc]; arg != argl;)
  {
    if (0 == strcmp(*arg, "--quit"))
    {
      exit(0);
    }
    else if (0 == strcmp(*arg, "--user-library"))
    {
      arg++;
      user_library_path = *arg;
    }
    arg++;
  }

  if (!posix_is_dir(user_library_path))
  {
    md2_fatal("--user-library <dir> expected (%s not recognized as directory)",
              user_library_path);
  }

  struct Mu mu = {
    .window.title = "Minidaw2",
  };
  if (!Mu_Initialize(&mu))
    md2_fatal("Init: %s", mu.error);

  if (!gladLoadGL())
    md2_fatal("Init: GL loader");

  MD2_UserInterface ui = {
    .mu = &mu,
    .vg = nvgCreateGL3(NVG_ANTIALIAS | NVG_STENCIL_STROKES),
    .overlay_vg = nvgCreateGL3(NVG_ANTIALIAS | NVG_STENCIL_STROKES),
  };
  if (!ui.vg || !ui.overlay_vg)
    md2_fatal("Init: NanoVG");

  {
    struct NVGcontext* vg_all[] = {
      ui.vg,
      ui.overlay_vg,
    };
    size_t vg_all_n = sizeof vg_all / sizeof vg_all[0];
    for (struct NVGcontext **vg_i = &vg_all[0], **vg_l = &vg_all[vg_all_n]; vg_i < vg_l;
         vg_i++)
    {
      load_fonts(
        *vg_i); // @todo @performance wasted due to multiple loading of th same font?
    }
  }

  bool is_first_frame = true;
  while (Mu_Push(&mu), Mu_Pull(&mu))
  {
    float px_ratio = get_window_metrics(&mu).device_px_per_ref_points;
    glViewport(0, 0, mu.window.size.x, mu.window.size.y);
    glClearColor(0.5f, 0.0f, 1.0f, 0.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);

    ui.mu = &mu;
    ui.pixel_ratio = px_ratio;
    ui.size = (MD2_Vec2){
      .x = mu.window.size.x / ui.pixel_ratio, .y = mu.window.size.y / ui.pixel_ratio};
    md2_ui_update(&ui);

    NVGcontext* vg = ui.vg;
    {
      nvgFontFace(vg, "fallback");
      nvgFontSize(vg, 16);
      nvgFillColor(vg, nvgRGBA(255, 192, 0, 255));
      md2_ui_textf(&ui, (MD2_RenderingOptions){.layer_index = 1},
                   (MD2_Rect2){.x0 = 20, .y1 = 20}, "User Library %s%s",
                   user_library_path,
                   !posix_is_dir(user_library_path) ? " (offline)" : "");


      nvgBeginPath(vg);
      nvgRect(vg, 100, 100, 120, 30);
      nvgCircle(vg, 120, 120, 5);
      nvgPathWinding(vg, NVG_HOLE); // Mark circle as a hole.
      nvgFillColor(vg, nvgRGBA(255, 192, 0, 255));
      nvgFill(vg);

      nvgFontFace(vg, "fallback");
      nvgFontSize(vg, 16);
      md2_ui_textf(&ui, (MD2_RenderingOptions){.layer_index = 0},
                   (MD2_Rect2){.x0 = 100, .y1 = 100}, "hello, world %d %d %f",
                   mu.window.size.x, mu.window.size.y, px_ratio);

      nvgFontFace(ui.overlay_vg, "fallback");
      nvgFontSize(ui.overlay_vg, 24);
      md2_ui_textf(&ui, (MD2_RenderingOptions){.layer_index = 1},
                   (MD2_Rect2){.x0 = ui.pointer_position.x, .y1 = ui.pointer_position.y},
                   "hello from (drag-image) overlay");
    }
    is_first_frame = false;
  }

  return 0;
}
