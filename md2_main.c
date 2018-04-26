// @todo memory management of paths is a bit icky. strdup is a smell
// @todo iterating multiple times on directory listing entries is fishy: if I have a
// sequential domain from which to select from, then it should be convenient to scan
// through it
// @todo too much stuff is being done locally rather than globally left to the UI to
// decide
// @todo non-overlapping boxes layout at the top level

#include "md1_support.h"
#include "md2_audio.h"
#include "md2_audioengine.h"
#include "md2_math.h"
#include "md2_posix.h"
#include "md2_temp_allocator.h"
#include "md2_types.h"
#include "md2_ui.h"
#include "md2_win32.h"

#include "libs/xxxx_buf.h"
#include "libs/xxxx_iobuffer.h"
#include "libs/xxxx_map.h"
#include "libs/xxxx_mu.h"
#include "libs/xxxx_tasks.h"

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
int test_queue(int argc, char const** argv);

int test_serialisation(int argc, char const** argv);
int test_task(int argc, char const** argv);
int test_ui(int, char const**);

enum
{
  MD2_UI_WAVEFORM_SIZE = 512,
};

static void md2_exit_with_message(char const* fmt, ...)
{
  va_list args;
  va_start(args, fmt);
  printf("ERROR: ");
  vprintf(fmt, args);
  printf("\n");
  va_end(args);
  exit(1);
}

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

MD1_EntityCatalog md1_song_load(char const* path)
{
  IOBuffer reader = iobuffer_file_reader(path);
  MD1_Loader loader = {
    .input = &reader,
  };
  MD1_EntityCatalog entities = {0};
  assert(md1_load(&loader, &entities));
  iobuffer_file_reader_close(&reader);
  return entities;
}

int test_main(int argc, char const* argv[])
{
  MD1_EntityCatalog entities = md1_song_load("test_assets/md1_songs.bin");

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

  map_free(&unused_files);
  md1_free(&entities);

  return 0;
}

static char* str_dup_range(char* f, char* l)
{
  char* r = calloc(1, l - f + 1);
  memcpy(&r[0], &f[0], l - f);
  r[l - f] = '\0';
  return r;
}

#if defined(_WIN32)
static DWORD DWORD_from_size_t(size_t x)
{
  DWORD y = (DWORD)x;
  assert(((size_t)y) == x);
  if (((size_t)y) != x)
    md2_fatal("too large of a number for DWORD: %llu", x);
  return y;
}
#endif

static char* get_exe_dir()
{
#if defined(_WIN32)
  char* buffer = NULL;
  char* buffer_l = NULL;
  buf_fit(buffer, MAX_PATH);
  while (1)
  {
    DWORD size = GetModuleFileNameA(NULL, buffer, DWORD_from_size_t(buf_cap(buffer)));
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

typedef struct LoadAudioTask
{
  bool success;
  char* filename;
  WaveformData ui_waveform;
  MD2_Float2* float_stereo;
  size_t float_stereo_n;
} LoadAudioTask;

char* strdup_range(char const* f, size_t n)
{
  void* str = calloc(n + 1, 1);
  if (!str)
    md2_fatal("can't allocate");
  memcpy(str, f, n);
  return str;
}

void load_audio_file(LoadAudioTask* load_audio_task)
{
  struct Mu_AudioBuffer audiobuffer;
  bool success = Mu_LoadAudio(load_audio_task->filename, &audiobuffer);
  if (!success)
    return;

  WaveformData* d_waveform = &load_audio_task->ui_waveform;
  size_t n = d_waveform->len_pot;
  if (n > 0)
  {
    audiobuffer_compute_waveform(&audiobuffer, d_waveform);
  }

  load_audio_task->float_stereo_n =
    audiobuffer.samples_count / audiobuffer.format.channels;
  load_audio_task->float_stereo =
    calloc(load_audio_task->float_stereo_n, sizeof load_audio_task->float_stereo[0]);
  {
    MD2_Float2* d_frame = &load_audio_task->float_stereo[0];
    for (size_t sample_index = 0; sample_index < audiobuffer.samples_count;
         sample_index += audiobuffer.format.channels)
    {
      d_frame->x = audiobuffer.samples[sample_index] / 32267.0;
      d_frame->y = d_frame->x;
      if (audiobuffer.format.channels > 1)
      {
        d_frame->y = audiobuffer.samples[sample_index + 1] / 32267.0;
      }
      d_frame++;
    }
    assert(load_audio_task->float_stereo_n
           == d_frame - &load_audio_task->float_stereo[0]);
  }

  free(audiobuffer.samples);
  load_audio_task->success = success;
}

static inline char* temp_strdup(char const* src, TempAllocator* allocator)
{
  char* dst = temp_calloc(allocator, strlen(src) + 1, 1);
  strcpy(dst, src);
  return dst;
}

static inline char* temp_sprintf(TempAllocator* allocator, char const* fmt, ...)
{
  va_list args;
  va_start(args, fmt);
  size_t needed_n = vsnprintf(NULL, 0, fmt, args);
  va_end(args);
  char* y = temp_calloc(allocator, needed_n + 1, 1);
  va_start(args, fmt);
  vsnprintf(y, needed_n + 1, fmt, args);
  va_end(args);
  return y;
}

char* temp_str_nicenumber(intmax_t x, TempAllocator* temp_allocator)
{
  char* digits_only = NULL;
  buf_printf(digits_only, "%lld", x);

  char* grouping_str = u8"\u2009";
  size_t grouping_str_n = strlen(grouping_str);

  size_t digits_n = buf_len(digits_only);
  size_t groupings_n = digits_n / 3;
  size_t next_grouping_i = digits_n % 3;
  size_t str_n = digits_n + groupings_n + grouping_str_n;

  char* d_str = temp_calloc(temp_allocator, str_n, 1);
  char* d_str_l = d_str;
  char* s_str = digits_only;
  while (next_grouping_i-- && digits_n-- && d_str_l < &d_str[str_n])
  {
    *d_str_l = *s_str;
    s_str++;
    d_str_l++;
  }
  while (digits_n)
  {
    for (int i = 0; i < grouping_str_n && d_str_l < &d_str[str_n]; i++)
    {
      *d_str_l = grouping_str[i];
      d_str_l++;
    }
    next_grouping_i = 3;
    while (next_grouping_i-- && digits_n-- && d_str_l < &d_str[str_n])
    {
      *d_str_l = *s_str;
      s_str++;
      d_str_l++;
    }
  }

  buf_free(digits_only), digits_only = NULL;
  return d_str;
}

typedef struct MD2_UIRegion
{
  MD2_Rect2 bounds;
  MD2_UIElement* elements_buf;
  Map element_index_by_data;
} MD2_UIRegion;

void md2_ui_region_start(MD2_UIRegion* region)
{
  region->bounds = rect_cover_unit();
}

void md2_ui_region_end(MD2_UIRegion* region)
{
  region->bounds = rect_made_valid(region->bounds);
}

void md2_ui_region_free(MD2_UIRegion* region)
{
  buf_free(region->elements_buf);
  map_free(&region->element_index_by_data);
}

void md2_ui_region_add(MD2_UIRegion* region, MD2_UIElement element, void const* data)
{
  assert(data);
  size_t element_index = buf_len(region->elements_buf);
  buf_push(region->elements_buf, element);
  region->bounds = rect_covering(region->bounds, element.rect);
  map_put(
    &region->element_index_by_data, (intptr_t)data, (void*)(uintptr_t)element_index);
}

MD2_UIElement md2_ui_region_get_element(MD2_UIRegion const* region, void const* data)
{
  size_t element_index = (size_t)map_get(&region->element_index_by_data, (intptr_t)data);
  return region->elements_buf[element_index];
}

typedef struct MD2_UIScrollableContent
{
  float content_min_top_y;
  float content_max_top_y;
  float content_user_top_y;
  float content_top_y; // @animated from content_user_top_y
  float scrollbar_user_alpha;
  float scrollbar_alpha; // @animated from scrollbar_user_alpha
  bool scrollbar_is_being_dragged;
  MD2_Vec2 scrollbar_handle_top_relative_to_drag_point;
} MD2_UIScrollableContent;

MD2_Rect2 rect_translate_within(MD2_Rect2 bounds,
                                MD2_Rect2 rect,
                                MD2_Point2 new_origin,
                                MD2_Vec2 margin_size)
{
  MD2_Rect2 max_rect = rect_expanded(bounds, (MD2_Vec2){-margin_size.x, -margin_size.y});
  return rect_intersection(max_rect, rect_translated(rect, vec_to_point(new_origin)));
}

enum
{
  SCROLLBAR_HANDLE_SIZE_X = 10
};

void md2_ui_scroller_start(MD2_UserInterface* ui,
                           MD2_UIScrollableContent* scroller_data,
                           MD2_UIRegion* content,
                           MD2_UIElement element)
{
  struct Mu* mu = ui->mu;
  MD2_Rect2 rect = element.rect;

  float content_min_top_y = 0.0;
  float content_max_top_y =
    max_f(0.0, (content->bounds.y1 - content->bounds.y0) - (rect.y1 - rect.y0));

  if (rect_intersects(rect, ui->pointer.position))
  {
    // Input processing for the list
    scroller_data->content_user_top_y +=
      (-16.0 * mu->mouse.delta_wheel) / ui->pixel_ratio;
    if (mu->keys[MU_PAGEUP].pressed)
    {
      scroller_data->content_user_top_y -= 16.0 * 5;
    }
    if (mu->keys[MU_PAGEDOWN].pressed)
    {
      scroller_data->content_user_top_y += 16.0 * 5;
    }
    if (mu->keys[MU_HOME].pressed)
    {
      scroller_data->content_user_top_y = content_min_top_y;
    }
    if (mu->keys[MU_END].pressed)
    {
      scroller_data->content_user_top_y = content_max_top_y;
    }
  }

  scroller_data->content_min_top_y = content_min_top_y;
  scroller_data->content_max_top_y = content_max_top_y;

  // apply constraints
  scroller_data->content_user_top_y =
    max_f(scroller_data->content_user_top_y, content_min_top_y);
  scroller_data->content_user_top_y =
    min_f(scroller_data->content_user_top_y, content_max_top_y);

  // smooth lerp // @todo factorize
  float user_interaction_animation_rate = 10.0;
  scroller_data->content_top_y = interpolate_linear_f(
    scroller_data->content_user_top_y, scroller_data->content_top_y,
    exp2(-1.0 * user_interaction_animation_rate * mu->time.delta_seconds));
  if (fabs(scroller_data->content_top_y - scroller_data->content_user_top_y) < 0.1)
  {
    scroller_data->content_top_y = scroller_data->content_user_top_y;
  }

  nvgScissor(md2_ui_vg(ui, element), element.rect.x0, element.rect.y0,
             element.rect.x1 - element.rect.x0, element.rect.y1 - element.rect.y0);
}


void md2_ui_scroller_end(MD2_UserInterface* ui,
                         MD2_UIScrollableContent* scroller_data,
                         MD2_UIRegion* content,
                         MD2_UIElement element)
{
  struct Mu* mu = ui->mu;
  if (content->bounds.y1 != content->bounds.y0)
  {
    // draw the scrollbar
    MD2_UIElement scrollbar = {
      .rect = element.rect,
    };
    scrollbar.rect.x0 = scrollbar.rect.x1 - SCROLLBAR_HANDLE_SIZE_X;

    MD2_UIElement scrollbar_content_handle = scrollbar;
    float content_y_from_scrollbar_y = 1.0;
    float visible_content_size_y = element.rect.y1 - element.rect.y0;
    {
      float content_size_y = (content->bounds.y1 - content->bounds.y0);
      content_y_from_scrollbar_y =
        content_size_y / (scrollbar.rect.y1 - scrollbar.rect.y0);
      float normalized_content_top = scroller_data->content_top_y / content_size_y;
      float normalized_content_bottom =
        (scroller_data->content_top_y + (element.rect.y1 - element.rect.y0))
        / content_size_y;
      scrollbar_content_handle.rect.y0 =
        scrollbar.rect.y0
        + normalized_content_top * (scrollbar.rect.y1 - scrollbar.rect.y0);
      scrollbar_content_handle.rect.y1 =
        scrollbar.rect.y0
        + normalized_content_bottom * (scrollbar.rect.y1 - scrollbar.rect.y0);
    }

    MD2_Rect2 fade_in_rect = rect_expanded(scrollbar.rect, (MD2_Vec2){5, 0});
    MD2_Rect2 fade_out_rect = rect_expanded(scrollbar.rect, (MD2_Vec2){10, 0});

    MD2_Rect2 scrollbar_content_handle_interaction_rect =
      rect_expanded(scrollbar_content_handle.rect, (MD2_Vec2){4, 4});
    if (ui->pointer.drag.started)
    {
      if (rect_intersects(
            scrollbar_content_handle_interaction_rect, ui->pointer.position))
      {
        scroller_data->scrollbar_is_being_dragged = true;
        scroller_data->scrollbar_handle_top_relative_to_drag_point = vec_from_to(
          ui->pointer.last_press_position, rect_min_point(scrollbar_content_handle.rect));
      }
    }

    if (ui->pointer.drag.ended)
    {
      scroller_data->scrollbar_is_being_dragged = false;
    }

    if (scroller_data->scrollbar_is_being_dragged && ui->pointer.drag.running)
    {
      MD2_Point2 scrollbar_pos = point_translated(
        ui->pointer.position, scroller_data->scrollbar_handle_top_relative_to_drag_point);
      scroller_data->content_top_y =
        (scrollbar_pos.y - scrollbar.rect.y0) * content_y_from_scrollbar_y;
      // allow bounce by allowing to move slightly beyond
      scroller_data->content_top_y =
        max_f(scroller_data->content_top_y,
              scroller_data->content_min_top_y - visible_content_size_y / 2.0);
      scroller_data->content_top_y =
        min_f(scroller_data->content_top_y,
              scroller_data->content_max_top_y + visible_content_size_y / 2.0);
      scroller_data->content_user_top_y = scroller_data->content_top_y;
    }

    if (scroller_data->scrollbar_is_being_dragged)
    {
      scroller_data->scrollbar_user_alpha = 255;
    }
    else if (scroller_data->content_top_y != scroller_data->content_user_top_y)
    {
      scroller_data->scrollbar_user_alpha = 255;
    }
    else if (rect_intersects(fade_in_rect, ui->pointer.position))
    {
      scroller_data->scrollbar_user_alpha = 255;
    }
    else if (!rect_intersects(fade_out_rect, ui->pointer.position))
    {
      scroller_data->scrollbar_user_alpha = 0;
    }

    float user_interaction_animation_rate = 3.0;
    scroller_data->scrollbar_alpha = interpolate_linear_f(
      scroller_data->scrollbar_user_alpha, scroller_data->scrollbar_alpha,
      exp2(-1.0 * user_interaction_animation_rate * mu->time.delta_seconds));

    if (scroller_data->scrollbar_alpha > 0.1)
    {
      md2_ui_rect(ui, scrollbar_content_handle,
                  nvgRGBA(255, 255, 255, scroller_data->scrollbar_alpha));
    }
  }

  nvgResetScissor(md2_ui_vg(ui, element));
}

bool md2_ui_scroller_get_element(MD2_UserInterface* ui,
                                 MD2_UIScrollableContent* scroller_data,
                                 MD2_UIElement scroller_element,
                                 MD2_UIRegion const* content,
                                 void const* data,
                                 MD2_UIElement* d_element)
{
  MD2_UIElement element = md2_ui_region_get_element(content, data);
  element.rect =
    rect_translated(element.rect, (MD2_Vec2){-content->bounds.x0, -content->bounds.y0});
  element.rect =
    rect_translated(element.rect, (MD2_Vec2){0.0, -scroller_data->content_top_y});
  element.rect =
    rect_translated(element.rect, vec_to_point(rect_min_point(scroller_element.rect)));
  if (!rects_intersect(scroller_element.rect, element.rect))
    return false;
  if (content->bounds.y1 - content->bounds.y0
      > scroller_element.rect.y1 - scroller_element.rect.y0)
  {
    // reserve space for scrollbar
    element.rect = rect_translated(
      rect_expanded(element.rect, (MD2_Vec2){.x = -SCROLLBAR_HANDLE_SIZE_X / 2}),
      (MD2_Vec2){.x = -SCROLLBAR_HANDLE_SIZE_X / 2});
  }
  *d_element = element;
  return true;
}

static void atomic_store_uint32(uint32_t* d_y, uint32_t x)
{
  *d_y = x;
}

static uint32_t atomic_load_uint32(uint32_t const* s_x)
{
  return *s_x;
}

enum
{
  DirectoryListing_None,
  DirectoryListing_InProgress,
  DirectoryListing_Done,
  DirectoryListing_Cancelled,
  DirectoryListing_Error
};

// @todo make the listing task be a series of tasks working one page at a time, to allow
// concurrent listing and display as well as give a natural way to cancel and tear down
// the listing when needed.

typedef struct DirectoryListing
{
  uint32_t state;
  char* error;
  char error_buffer[256];
  uint64_t start_tick;
  uint64_t end_tick;
  TempAllocator allocator;
  char* root_abspath;
  size_t names_n;
  size_t last_dir_name_n;
  char const* const*
    names; // [0..last_dir_name_n) directories, [last_dir_name_n..names_n) files
} DirectoryListing;

void directory_listing_cleanup_on_cancellation_task(void* data_)
{
  DirectoryListing* listing = data_;
  if (atomic_load_uint32(&listing->state) == DirectoryListing_Cancelled)
  {
    // @todo cleanup memory
    temp_allocator_free(&listing->allocator);
  }
}

#if defined(_WIN32)
void win32_query_directory(DirectoryListing* listing)
{
  char const* query_fmt = "%s\\*";
  char* query = temp_sprintf(&listing->allocator, query_fmt, listing->root_abspath);

  WIN32_FIND_DATAA CurrentFileAttributes;
  HANDLE SearchHandle = FindFirstFileA(query, &CurrentFileAttributes);
  if (!SearchHandle || ((void*)(intptr_t)(-1)) == SearchHandle)
  {
    listing->state = DirectoryListing_Error;
    snprintf(listing->error_buffer, sizeof listing->error_buffer, "Got Win32 error: 0x%x",
             GetLastError());
    listing->error = listing->error_buffer;
    return;
  }
  char** files_buf = NULL;
  char** dirs_buf = NULL;
  do
  {
    DWORD attrs = CurrentFileAttributes.dwFileAttributes;
    if (attrs & FILE_ATTRIBUTE_DEVICE)
      continue;
    if (attrs & FILE_ATTRIBUTE_HIDDEN)
      continue;
    if (attrs & FILE_ATTRIBUTE_INTEGRITY_STREAM)
      continue;
    if (attrs & FILE_ATTRIBUTE_OFFLINE)
      continue;
    if (attrs & FILE_ATTRIBUTE_SYSTEM)
      continue;
    if (attrs & FILE_ATTRIBUTE_TEMPORARY)
      continue;

    if ((attrs & FILE_ATTRIBUTE_DIRECTORY))
    {
      buf_push(
        dirs_buf, temp_strdup(CurrentFileAttributes.cFileName, &listing->allocator));
    }
    else
    {
      buf_push(
        files_buf, temp_strdup(CurrentFileAttributes.cFileName, &listing->allocator));
    }
  } while (FindNextFileA(SearchHandle, &CurrentFileAttributes));
  FindClose(SearchHandle);
  size_t names_n = buf_len(dirs_buf) + buf_len(files_buf);
  char** names = temp_calloc(&listing->allocator, names_n, sizeof names[0]);
  size_t last_dir_name_n = buf_len(dirs_buf);
  memcpy(&names[0], &dirs_buf[0], buf_len(dirs_buf) * sizeof dirs_buf[0]);
  memcpy(
    &names[last_dir_name_n], &files_buf[0], buf_len(files_buf) * sizeof files_buf[0]);
  listing->names = names;
  listing->names_n = names_n;
  listing->last_dir_name_n = last_dir_name_n;
  buf_free(dirs_buf), buf_free(files_buf);
}
#endif

void directory_listing_task(void* data_)
{
  DirectoryListing* listing = data_;
  atomic_store_uint32(&listing->state, DirectoryListing_InProgress);
#if defined(_WIN32)
  win32_query_directory(listing);
#else
#error "Implement me"
#endif
  atomic_store_uint32(&listing->state, DirectoryListing_Done);
}

void directory_listing_make(DirectoryListing* listing, char const* root_dir_path)
{
  bool rp_error;
#if defined(_WIN32)
  char* rp = win32_realpath(
    root_dir_path, &rp_error, listing->error_buffer, sizeof listing->error_buffer);
#else
  char* rp = posix_realpath(
    root_dir_path, &rp_error, listing->error_buffer, sizeof listing->error_buffer);
#endif
  if (rp_error)
  {
    listing->state = DirectoryListing_Error;
    return;
  }
  listing->root_abspath = temp_strdup(rp, &listing->allocator);

  TaskHandle task = task_create(directory_listing_task, listing);
  task_depends(
    task, task_create(directory_listing_cleanup_on_cancellation_task, listing));
  task_start(task);
}

bool directory_listing_same_dir(DirectoryListing* listing, char const* other_path)
{
  bool rp_error;
#if defined(_WIN32)
  char* rp = win32_realpath(other_path, &rp_error, NULL, 0);
#else
  char* rp = posix_realpath(other_path, &rp_error, NULL, 0);
#endif
  if (rp_error)
    return false;
  return strcmp(listing->root_abspath, rp) == 0;
}

void directory_listing_free(DirectoryListing* listing)
{
  assert(listing->state != DirectoryListing_InProgress);
  if (atomic_load_uint32(&listing->state) == DirectoryListing_InProgress)
  {
    atomic_store_uint32(&listing->state, DirectoryListing_Cancelled);
    // @todo @defect interrupt and wait for task if it's running, otherwise detach if it
    // does not respond
    // @todo @defect it would be a defect for the UI to wait, so detaching the data and
    // having a cleanup task is the surest.
    return; // will crash if we free the memory
  }
  temp_allocator_free(&listing->allocator);
}

static Map /* path to DirectoryListing* */ g_directory_listings;
typedef struct DirectoryListingEntry
{
  char* directory_path_buf;
  struct DirectoryListing* listing;
  struct DirectoryListing** collisions;
} DirectoryListingEntry;

char* buf_make_strdup(char* other)
{
  char* buf = NULL;
  size_t n = strlen(other);
  buf_fit(buf, n + 1);
  memcpy(&buf[0], &other[0], n);
  buf[n] = '\0';
  return buf;
}

DirectoryListing* directory_listing_get(char const* directory_path, uint64_t start_tick)
{
  uint64_t key_hash = hash_bytes(directory_path, strlen(directory_path));
  DirectoryListingEntry* listing_entry = map_get(&g_directory_listings, key_hash);
  if (listing_entry && directory_listing_same_dir(listing_entry->listing, directory_path))
  {
    return listing_entry->listing;
  }

  DirectoryListing** d_listing;
  if (listing_entry)
  {
    DirectoryListing** collision_entry;
    for (collision_entry = &listing_entry->collisions[0];
         collision_entry < buf_end(listing_entry->collisions)
         && !directory_listing_same_dir(*collision_entry, directory_path);
         collision_entry++)
      ;
    if (collision_entry < buf_end(listing_entry->collisions))
    {
      return *collision_entry;
    }

    buf_push(listing_entry->collisions, NULL);
    d_listing = &buf_end(listing_entry->collisions)[-1];
  }
  else
  {
    listing_entry = calloc(1, sizeof *listing_entry);
    map_put(&g_directory_listings, key_hash, listing_entry);
    d_listing = &listing_entry->listing;
  }

  DirectoryListing* listing = calloc(1, sizeof *listing);
  directory_listing_make(listing, directory_path);
  listing->start_tick = start_tick;

  *d_listing = listing;

  return listing;
}

typedef struct MD2_UIList
{
  Map selection_indices_set; // all element indices belonging to the selection
  uint64_t anchor_index;
  bool in_drag_gesture;
} MD2_UIList;

// Right-Exclusive
typedef enum SelectionRangeOp {
  SelectionRangeOp_None,
  SelectionRangeOp_Replace,
  SelectionRangeOp_Merge,
} SelectionRangeOp;

typedef struct SelectionRange
{
  SelectionRangeOp op;
  size_t first_index;
  size_t last_index;
} SelectionRange;

SelectionRange ui_list_element_process_input(MD2_UserInterface* ui,
                                             MD2_UIList* list,
                                             MD2_UIElement element,
                                             size_t element_index)
{
  // @todo @defect this isn't really up to the standard spec for a multiselection
  // mechanism. that'll do for now.
  if (!rect_intersects(element.rect, ui->pointer.position))
    return (SelectionRange){0};

  SelectionRangeOp op =
    ui->mu->keys[MU_CTRL].down ? SelectionRangeOp_Merge : SelectionRangeOp_Replace;

  if (ui->mu->mouse.left_button.released)
  {
    if (ui->mu->keys[MU_SHIFT].down)
    {
      // range selection
      size_t selection_min_i = min_i(list->anchor_index, element_index);
      size_t selection_max_i = max_i(list->anchor_index, element_index);
      return (SelectionRange){op, selection_min_i, selection_max_i + 1};
    }
    else
    {
      list->anchor_index = element_index;
      return (SelectionRange){op, element_index, element_index + 1};
    }
  }

  return (SelectionRange){0};
}

void ui_directory_listing_element_draw(MD2_UserInterface* ui,
                                       MD2_UIElement element,
                                       char const* name,
                                       bool is_dir)
{
  NVGcontext* vg = md2_ui_vg(ui, element);
  nvgFillColor(vg, nvgRGBA(255, 255, 255, 255));
  nvgTextAlign(vg, NVG_ALIGN_BOTTOM);
  md2_ui_textf(ui, element, "%s%s", name, is_dir ? "/" : "");
  nvgTextAlign(vg, NVG_ALIGN_BASELINE); // back to default
}

typedef struct DirectoryListingOperation
{
  char const* next_directory_path;
} DirectoryListingOperation;

DirectoryListingOperation ui_directory_listing(MD2_UserInterface* ui,
                                               MD2_UIElement element,
                                               MD2_UIList* list,
                                               MD2_UIScrollableContent* scroller_state,
                                               char const* directory_path)
{
  DirectoryListing const* listing =
    directory_listing_get(directory_path, ui->mu->time.ticks);
  DirectoryListingOperation result = {.next_directory_path = NULL};
  bool listing_is_done = atomic_load_uint32(&listing->state) & DirectoryListing_Done;

  NVGcontext* vg = md2_ui_vg(ui, element);
  float font_size = 14.0;
  float default_font_size = 16.0; // @todo global
  MD2_UIRegion list_content = {0};

  md2_ui_region_start(&list_content);
  if (listing_is_done)
  {
    float row_y = 0.0;
    nvgFontSize(vg, font_size);
    for (size_t entry_i = 0; entry_i < listing->names_n; entry_i++)
    {
      char const* name = listing->names[entry_i];
      md2_ui_region_add(&list_content,
                        (MD2_UIElement){.rect = {.x0 = 0.0,
                                                 .x1 = element.rect.x1 - element.rect.x0,
                                                 .y0 = row_y,
                                                 .y1 = row_y + font_size}},
                        name);
      row_y += font_size;
    }
  }
  md2_ui_region_end(&list_content);

  MD2_UIElement file_list_scroller = element;
  file_list_scroller.rect.y1 -= default_font_size; // @todo status height
  file_list_scroller.rect = rect_made_valid(file_list_scroller.rect);
  // @todo we're not checking if the content escapes the element

  md2_ui_rect(ui, element, nvgRGBA(160, 160, 160, 128)); // debug
  nvgFillColor(vg, nvgRGBA(255, 255, 255, 255));
  md2_ui_scroller_start(ui, scroller_state, &list_content, file_list_scroller);
  if (listing_is_done)
  {
    for (size_t entry_i = 0; entry_i < listing->names_n; entry_i++)
    {
      char const* name = listing->names[entry_i];
      bool is_dir = entry_i < listing->last_dir_name_n;
      MD2_UIElement list_element;
      if (md2_ui_scroller_get_element(
            ui, scroller_state, file_list_scroller, &list_content, name, &list_element))
      {
        SelectionRange selection =
          ui_list_element_process_input(ui, list, list_element, entry_i);
        bool element_is_selected = map_get(&list->selection_indices_set, hash_ptr(name));
        if (element_is_selected)
        {
          md2_ui_rect(ui, list_element, nvgRGBA(160, 160, 160, 128));
        }
        ui_directory_listing_element_draw(ui, list_element, name, is_dir);
        if (list->in_drag_gesture && element_is_selected)
        {
          MD2_UIElement drag_element = list_element;
          drag_element.layer = 1;
          drag_element.rect = rect_translated(
            list_element.rect,
            vec_from_to(ui->pointer.last_press_position, ui->pointer.position));
          ui_directory_listing_element_draw(ui, drag_element, name, is_dir);
        }
        if (rect_intersects(list_element.rect, ui->pointer.position))
        {
          md2_ui_rect(ui, list_element, nvgRGBA(255, 160, 160, 128));

          if (ui->pointer.double_clicked && is_dir)
          {
            assert(result.next_directory_path == NULL);
            if (result.next_directory_path == NULL)
            {
              char* buf = NULL;
              buf_printf(buf, "%s/%s", listing->root_abspath, name);
              result.next_directory_path = _strdup(buf);
              buf_free(buf);
            }
          }

          if (ui->pointer.drag.started)
          {
            list->in_drag_gesture = true;
            if (!element_is_selected)
            {
              list->anchor_index = entry_i;
              selection.op = SelectionRangeOp_Replace;
              selection.first_index = entry_i;
              selection.last_index = entry_i + 1;
            }
          }
        }
        if (selection.op == SelectionRangeOp_Replace)
        {
          map_free(&list->selection_indices_set);
        }
        for (size_t entry_to_select_i = selection.first_index;
             entry_to_select_i < selection.last_index; entry_to_select_i++)
        {
          char const* const name = listing->names[entry_to_select_i];
          map_put(&list->selection_indices_set, hash_ptr(name), (void*)(intptr_t)1);
        }
      }
    }
  }
  md2_ui_scroller_end(ui, scroller_state, &list_content, file_list_scroller);

  if (listing_is_done)
  {
    if (list->in_drag_gesture && ui->pointer.drag.started)
    {
      char** filepath_list = NULL;
      TempAllocator* d_allocator = &ui->pointer.drag.payload_allocator;

      for (size_t entry_i = 0; entry_i < listing->names_n; entry_i++)
      {
        if (entry_i < listing->last_dir_name_n)
          continue; // ignore dirs
        char const* const name = listing->names[entry_i];
        if (map_get(&list->selection_indices_set, hash_ptr(name)))
        {
          buf_push(filepath_list,
                   temp_sprintf(d_allocator, "%s/%s", listing->root_abspath, name));
        }
      }
      if (buf_len(filepath_list) > 0)
      {
        FilepathList* d_list = temp_calloc(d_allocator, 1, sizeof *d_list);
        d_list->paths_n = buf_len(filepath_list);
        d_list->paths =
          temp_memdup_range(d_allocator, &filepath_list[0], buf_end(filepath_list));
        map_put(&ui->pointer.drag.payload_by_type, MD2_PayloadType_FilepathList, d_list);
      }
      buf_free(filepath_list);
    }
  }

  if (ui->pointer.drag.ended)
  {
    list->in_drag_gesture = false;
  }

  nvgFillColor(vg, nvgRGBA(255, 255, 255, 255));
  nvgTextAlign(vg, NVG_ALIGN_BOTTOM);
  nvgFontSize(vg, default_font_size);
  if (listing_is_done)
  {
    md2_ui_textf(
      ui, element, "Directory %s (Found %d)", listing->root_abspath, listing->names_n);
  }
  else
  {
    md2_ui_textf(ui, element, "Directory %s (Listing)", listing->root_abspath);
  }
  nvgTextAlign(vg, NVG_ALIGN_BASELINE); // back to default

  md2_ui_region_free(&list_content);
  return result;
}


#if defined(_WIN32)
typedef DPI_AWARENESS_CONTEXT /*WINAPI*/ (SetThreadDpiAwarenessContextFn)(
  DPI_AWARENESS_CONTEXT dpiContext);
typedef UINT /*WINAPI*/ (GetDpiForWindowFn)(HWND hwnd);

typedef struct Windows10DpiFunctions
{
  SetThreadDpiAwarenessContextFn* SetThreadDpiAwarenessContext;
  GetDpiForWindowFn* GetDpiForWindow;
} Windows10DpiFunctions;

static Windows10DpiFunctions g_windows10_dpi_functions;
#endif

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
  if (g_windows10_dpi_functions.GetDpiForWindow)
  {
    UINT Dpi = g_windows10_dpi_functions.GetDpiForWindow(mu->win32->window);
    metrics.device_px_per_ref_points = Dpi / 96.0;
    metrics.dpi = Dpi;
  }
#endif
  return metrics;
}

LoadAudioTask* md2_load_audio_task_start(char const* path, size_t path_n)
{
  LoadAudioTask* task_data = calloc(1, sizeof *task_data);
  task_data->filename = strdup_range(path, path_n);
  {
    // Require waveform data:
    WaveformData* wv = &task_data->ui_waveform;
    size_t n = MD2_UI_WAVEFORM_SIZE;
    wv->len_pot = n;
    wv->min = calloc(n, sizeof wv->min[0]);
    wv->max = calloc(n, sizeof wv->max[0]);
    wv->rms = calloc(n, sizeof wv->rms[0]);
  }
  task_start(task_create(load_audio_file, task_data));
  return task_data;
}

// ui definition
typedef struct MD2_UIState
{
  struct LoadAudioTask** audiofile_tasks;
  char const* user_library_path;
} MD2_UIState;


void play_preview_clip(MD2_AudioState* audio_state, bool is_playing)
{
  audio_state->preview_clip_is_playing = is_playing;
}


MD2_Audio_Float2* to_stereo_frames(MD2_Float2* bytes)
{
  return (MD2_Audio_Float2*)bytes;
}

void set_preview_clip_stereo_data_counted_range(MD2_AudioState* audio_state,
                                                MD2_Float2* stereo_frames,
                                                size_t stereo_frames_n,
                                                enum {PLAY_FORWARDS,
                                                      PLAY_BACKWARDS} direction)
{
  audio_state->preview_clip.stereo_frames = to_stereo_frames(stereo_frames);
  audio_state->preview_clip.stereo_frames_n = stereo_frames_n;
  audio_state->preview_clip.phase_increment = 1.0 / stereo_frames_n;
  if (direction == PLAY_BACKWARDS)
  {
    audio_state->preview_clip.phase_increment *= -1;
  }
}

bool is_preview_clip(MD2_AudioState* audio_state, MD2_Float2* stereo_frames)
{
  return audio_state->preview_clip.stereo_frames == to_stereo_frames(stereo_frames);
}

void md2_ui_playhead(MD2_UserInterface* ui, MD2_UIElement element, double phase)
{
  float playhead_x = element.rect.x0 + phase * (element.rect.x1 - element.rect.x0);
  MD2_UIElement playhead_element = element;
  playhead_element.rect =
    rect_from_corners((MD2_Point2){.x = playhead_x - 10 / 2, .y = element.rect.y0},
                      (MD2_Point2){.x = playhead_x + 10 / 2, .y = element.rect.y1});
  md2_ui_rect(ui, playhead_element, nvgRGBA(0, 0, 0, 255));
}


void md2_update(MD2_UserInterface* ui,
                MD2_UIState* ui_state,
                MD2_AudioState* audio_state,
                TempAllocator* perframe_allocator)
{
  size_t files_n = buf_len(ui_state->audiofile_tasks);
  size_t loaded_n = 0;
  size_t bytes_n = 0;
  for (LoadAudioTask **task_i = &ui_state->audiofile_tasks[0],
                     **task_l = &task_i[files_n];
       task_i < task_l; task_i++)
  {
    LoadAudioTask* task = *task_i;
    if (!task->success)
      continue;
    loaded_n++;
    bytes_n += sizeof(task->float_stereo[0]) * task->float_stereo_n;
  }

  float small_size_x = 10;
  float small_size_y = 10;
  MD2_Rect2 bounds = rect_expanded(ui->bounds, (MD2_Vec2){-small_size_x, -small_size_y});
  static float col01splitter_percent = 0.33f;

  float row_y = bounds.y0;
  float splitter_size_x = small_size_x;
  float col0_x = bounds.x0;
  float col01splitter_x = col0_x + col01splitter_percent * (bounds.x1 - bounds.x0);
  float col1_x = col01splitter_x + splitter_size_x;

  NVGcontext* vg = ui->vg;

  nvgFontFace(vg, "fallback");
  float font_size_y = 16;
  float line_size_y = font_size_y * 1.10;
  nvgFontSize(vg, font_size_y);
  nvgFillColor(vg, nvgRGBA(255, 192, 0, 255));
  float col_x = col0_x;
  md2_ui_textf(
    ui,
    (MD2_UIElement){.rect = {.x0 = col_x, .x1 = bounds.x1, .y1 = row_y + font_size_y}},
    "User Library %s%s", ui_state->user_library_path,
    !posix_is_dir(ui_state->user_library_path) ? " (offline)" : ""),
    row_y += line_size_y;
  md2_ui_textf(
    ui,
    (MD2_UIElement){.rect = {.x0 = col_x, .x1 = bounds.x1, .y1 = row_y + font_size_y}},
    "Audiofiles:"),
    row_y += line_size_y;

  col_x += small_size_x;
  md2_ui_textf(
    ui,
    (MD2_UIElement){.rect = {.x0 = col_x, .x1 = bounds.x1, .y1 = row_y + font_size_y}},
    "%d / %d", loaded_n, files_n),
    row_y += line_size_y;

  md2_ui_textf(
    ui,
    (MD2_UIElement){.rect = {.x0 = col_x, .x1 = bounds.x1, .y1 = row_y + font_size_y}},
    "%s bytes", temp_str_nicenumber(bytes_n, perframe_allocator)),
    row_y += font_size_y;
  col_x -= small_size_x;

  md2_ui_textf(
    ui,
    (MD2_UIElement){.rect = {.x0 = col_x, .x1 = bounds.x1, .y1 = row_y + font_size_y}},
    "audioengine: sample-rate: %f", audio_state->time.samples_per_second),
    row_y += line_size_y;

  row_y += small_size_y;

  MD2_Rect2 col0_rect = {
    .x0 = col0_x,
    .x1 = col01splitter_x,
    .y0 = row_y,
    .y1 = bounds.y1,
  };
  MD2_Rect2 splitter_rect = rect_right_abutting_size(col0_rect, splitter_size_x);
  MD2_Rect2 col1_rect = rect_right_abutting_extremity(splitter_rect, bounds.x1);

  MD2_UIElement directory_listing_element = {
    .rect = rect_intersection(bounds, col0_rect),
  };
  {
    static MD2_UIScrollableContent file_content = {0};
    static MD2_UIList file_list_state = {0};
    static char const* path = NULL;
    if (!path)
      path = ui_state->user_library_path;
    DirectoryListingOperation result = ui_directory_listing(
      ui, directory_listing_element, &file_list_state, &file_content, path);
    if (result.next_directory_path)
    {
      if (path != ui_state->user_library_path)
        free(path);
      path = result.next_directory_path;
    }
  }

  MD2_UIElement splitter_element = {
    .rect = splitter_rect,
  };

  {
    static bool splitter_is_dragged = false;
    static float splitter_x_from_press_point;
    static float splitter_user_x;
    md2_ui_rect(ui, splitter_element, nvgRGBA(128, 128, 128, 128));
    if (rect_intersects(splitter_element.rect, ui->pointer.position))
    {
      md2_ui_rect(ui, splitter_element, nvgRGBA(128, 128, 128, 128));
    }
    if (ui->pointer.drag.started
        && rect_intersects(splitter_element.rect, ui->pointer.last_press_position))
    {
      splitter_is_dragged = true;
      splitter_x_from_press_point =
        splitter_element.rect.x0 - ui->pointer.last_press_position.x;
    }
    if (ui->pointer.drag.ended)
    {
      splitter_is_dragged = false;
    }
    if (ui->pointer.drag.running && splitter_is_dragged)
    {
      splitter_user_x = ui->pointer.position.x + splitter_x_from_press_point;
      col01splitter_percent = max_f(bounds.x0 + 3 * small_size_x,
                                    min_f(splitter_user_x, bounds.x1 - 3 * small_size_x))
                              / (bounds.x1 - bounds.x0);
    }
  }

  MD2_UIElement scroller_element = {
    .rect = col1_rect,
  };
  {
    LoadAudioTask const* const* const audiofile_tasks = &ui_state->audiofile_tasks[0];
    MD2_UIRegion audiofile_list_content = {0};
    {
      float row_y = 0.0;
      md2_ui_region_start(&audiofile_list_content);
      for (size_t loaded_i = 0; loaded_i < buf_len(audiofile_tasks); loaded_i++)
      {
        LoadAudioTask const* task = audiofile_tasks[loaded_i];
        if (!task->success)
          continue;
        row_y += 10.0;
        md2_ui_region_add(&audiofile_list_content,
                          (MD2_UIElement){
                            .rect =
                              {
                                .x0 = 0,
                                .x1 = 800.0,
                                .y0 = row_y,
                                .y1 = row_y + 40.0,
                              },
                          },
                          &task->ui_waveform);
        row_y += 40.0;
      }
      md2_ui_region_end(&audiofile_list_content);
    }

    static MD2_UIScrollableContent audiofile_list = {0};

    md2_ui_rect(ui, scroller_element, nvgRGBA(128, 128, 128, 128));
    md2_ui_rect(ui, scroller_element, nvgRGBA(160, 160, 160, 128)); // debug

    md2_ui_scroller_start(ui, &audiofile_list, &audiofile_list_content, scroller_element);
    for (size_t loaded_i = 0; loaded_i < buf_len(audiofile_tasks); loaded_i++)
    {
      LoadAudioTask const* task = audiofile_tasks[loaded_i];
      if (!task->success)
        continue;
      MD2_UIElement element;
      if (md2_ui_scroller_get_element(ui, &audiofile_list, scroller_element,
                                      &audiofile_list_content, &task->ui_waveform,
                                      &element))
      {
        md2_ui_waveform(ui, element, &task->ui_waveform);
        if (rect_intersects(element.rect, ui->pointer.last_click_position)
            && ui->pointer.clicked)
        {
          set_preview_clip_stereo_data_counted_range(audio_state, &task->float_stereo[0],
                                                     task->float_stereo_n,
                                                     ui->mu->keys[MU_SHIFT].down ? 1 : 0);
          play_preview_clip(audio_state, true);
        }
        if (is_preview_clip(audio_state, &task->float_stereo[0]))
        {
          md2_ui_playhead(ui, element, audio_state->preview_clip.phase);
        }
      }
    }
    md2_ui_scroller_end(ui, &audiofile_list, &audiofile_list_content, scroller_element);

    md2_ui_region_free(&audiofile_list_content);

    if (rect_intersects(scroller_element.rect, ui->pointer.position))
    {
      if (ui->pointer.drag.running)
      {
        md2_ui_rect(ui, scroller_element, nvgRGBA(128, 128, 128, 128));
      }

      if (ui->pointer.drag.ended)
      {
        FilepathList* filepath_list =
          map_get(&ui->pointer.drag.payload_by_type, MD2_PayloadType_FilepathList);
        if (filepath_list)
        {
          for (char const **path_i = &filepath_list->paths[0],
                          **path_l = &path_i[filepath_list->paths_n];
               path_i < path_l; path_i++)
          {
            buf_push(ui_state->audiofile_tasks,
                     md2_load_audio_task_start(*path_i, strlen(*path_i)));
          }
        }
      }
    }
  }
}

struct MD2_AudioEngine* g_audioengine;

void mu_audiocallback(struct Mu_AudioBuffer* buffer)
{
  md2_audioengine_mu_audiocallback(g_audioengine, buffer);
}

int main(int argc, char const* argv[])
{
// @todo @platform{win32}
#if defined(_WIN32)
  HANDLE user32_module = LoadLibraryA("user32.dll");
  g_windows10_dpi_functions.SetThreadDpiAwarenessContext =
    (SetThreadDpiAwarenessContextFn*)GetProcAddress(
      user32_module, "SetThreadDpiAwarenessContext");
  g_windows10_dpi_functions.GetDpiForWindow =
    (GetDpiForWindowFn*)GetProcAddress(user32_module, "GetDpiForWindow");
  if (g_windows10_dpi_functions.SetThreadDpiAwarenessContext)
  {
    g_windows10_dpi_functions.SetThreadDpiAwarenessContext(
      DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE);
  }
#endif

  // libs:
  test_buf(argc, argv);
  test_map(argc, argv);
  test_iobuffer(argc, argv);
  test_queue(argc, argv);
  // md2:
  test_serialisation(argc, argv);
  test_main(argc, argv);
  test_task(argc, argv);
  test_ui(argc, argv);

  char const* user_library_path = "";
  char const* md1_song_path = "";
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
    else if (0 == strcmp(*arg, "--md1-song"))
    {
      arg++;
      md1_song_path = *arg;
    }
    arg++;
  }

  task_init();

  if (!posix_is_dir(user_library_path))
  {
    md2_fatal("--user-library <dir> expected ('%s' not recognized as directory)",
              user_library_path);
  }

  g_audioengine = md2_audioengine_init();
  assert(g_audioengine);

  struct Mu mu = {
    .window.title = "Minidaw2",
    .audio.callback = mu_audiocallback,
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

  LoadAudioTask** audiofile_tasks = NULL;
  if (md1_song_path[0])
  {
    // @todo @defect @leak
    MD1_EntityCatalog entities = md1_song_load(md1_song_path);
    for (MD1_File *file_i = &entities.files[0], *file_l = &file_i[entities.files_n];
         file_i < file_l; file_i++)
    {
      buf_push(audiofile_tasks, md2_load_audio_task_start(file_i->path, file_i->path_n));
    }
  }

  bool is_first_frame = true;
  TempAllocator perframe_allocator = {0};
  MD2_UIState ui_state = {
    .audiofile_tasks = audiofile_tasks,
    .user_library_path = user_library_path,
  };
  audiofile_tasks = NULL;   // @moved_from
  user_library_path = NULL; // @moved_from

  MD2_AudioState audio_state = {
    0,
  };

  while (Mu_Push(&mu), Mu_Pull(&mu))
  {

    temp_allocator_free(&perframe_allocator);
    float px_ratio = get_window_metrics(&mu).device_px_per_ref_points;
    ui.pixel_ratio = px_ratio;

    glViewport(0, 0, mu.window.size.x, mu.window.size.y);
    glClearColor(0.5f, 0.0f, 1.0f, 0.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);
    md2_ui_update(&ui);
    md2_update(&ui, &ui_state, &audio_state, &perframe_allocator);
    md2_audioengine_update(g_audioengine, &audio_state);
    is_first_frame = false;
  }

  md2_audioengine_deinit(g_audioengine), g_audioengine = NULL;

  return 0;
}
