#include "md1_support.h"
#include "md2_audio.h"
#include "md2_math.h"
#include "md2_posix.h"
#include "md2_temp_allocator.h"
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
  IOBuffer reader = iobuffer_file_reader("test_assets/md1_songs.bin");
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
  struct Mu_AudioBuffer audiobuffer;
  WaveformData ui_waveform;
} LoadAudioTask;

char* strdup_range(char const* f, size_t n)
{
  void* str = calloc(n + 1, 1);
  if (!str)
    md2_fatal("can't allocate");
  memcpy(str, f, n);
  return str;
}

void load_md1_file(LoadAudioTask* load_audio_task)
{
  bool success = Mu_LoadAudio(load_audio_task->filename, &load_audio_task->audiobuffer);
  if (!success)
    return;

  WaveformData* d_waveform = &load_audio_task->ui_waveform;
  size_t n = d_waveform->len_pot;
  if (n > 0)
  {
    audiobuffer_compute_waveform(&load_audio_task->audiobuffer, d_waveform);
  }
  load_audio_task->success = success;
}

static inline char* strdup_temp(char const* src, TempAllocator* allocator)
{
  char* dst = temp_calloc(allocator, strlen(src) + 1, 1);
  strcpy(dst, src);
  return dst;
}

static inline void* temp_memdup_range(TempAllocator* allocator,
                                      void const* first,
                                      void const* last)
{
  char const* bytes_f = first;
  char const* bytes_l = last;
  void* dst = temp_calloc(allocator, 1, bytes_l - bytes_f);
  memcpy(dst, bytes_f, bytes_l - bytes_f);
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
  region->bounds = rect_make_valid(region->bounds);
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
  region->bounds = rect_cover(region->bounds, element.rect);
  map_put(
    &region->element_index_by_data, (intptr_t)data, (void*)(uintptr_t)element_index);
}

MD2_UIElement md2_ui_region_get_element(MD2_UIRegion* region, void const* data)
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
} MD2_UIScrollableContent;

MD2_Rect2 rect_translate_within(MD2_Rect2 bounds,
                                MD2_Rect2 rect,
                                MD2_Point2 new_origin,
                                MD2_Vec2 margin_size)
{
  MD2_Rect2 max_rect = rect_expand(bounds, (MD2_Vec2){-margin_size.x, -margin_size.y});
  return rect_intersection(
    max_rect, rect_translate(rect, to_point_from_origin(new_origin)));
}

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
    scrollbar.rect.x0 = scrollbar.rect.x1 - 10;

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

    MD2_Rect2 fade_in_rect = rect_expand(scrollbar.rect, (MD2_Vec2){5, 0});
    MD2_Rect2 fade_out_rect = rect_expand(scrollbar.rect, (MD2_Vec2){10, 0});

    MD2_Rect2 scrollbar_content_handle_interaction_rect =
      rect_expand(scrollbar_content_handle.rect, (MD2_Vec2){4, 4});
    if (mu->mouse.left_button.down)
    {
      if (rect_intersects(
            scrollbar_content_handle_interaction_rect, ui->pointer.position))
      {
        scroller_data->scrollbar_is_being_dragged = true;
      }
    }
    else
    {
      scroller_data->scrollbar_is_being_dragged = false;
    }

    if (scroller_data->scrollbar_is_being_dragged)
    {
      scroller_data->content_top_y =
        (ui->pointer.position.y - scrollbar.rect.y0) * content_y_from_scrollbar_y;
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
                                 MD2_UIRegion* content,
                                 void const* data,
                                 MD2_UIElement* d_element)
{
  MD2_UIElement element = md2_ui_region_get_element(content, data);
  element.rect =
    rect_translate(element.rect, (MD2_Vec2){0.0, -scroller_data->content_top_y});
  element.rect = rect_translate(
    element.rect, to_point_from_origin(rect_min_point(scroller_element.rect)));
  if (!rects_intersect(scroller_element.rect, element.rect))
    return false;
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
  size_t files_n;
  char** files;
  size_t dirs_n;
  char** dirs;
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
    return;
  char** files_buf = NULL;
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

    if (!(attrs & FILE_ATTRIBUTE_DIRECTORY))
    {
      buf_push(
        files_buf, strdup_temp(CurrentFileAttributes.cFileName, &listing->allocator));
    }
  } while (FindNextFileA(SearchHandle, &CurrentFileAttributes));
  FindClose(SearchHandle);
  listing->files_n = buf_len(files_buf);
  listing->files =
    temp_memdup_range(&listing->allocator, &files_buf[0], buf_end(files_buf));
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
  listing->root_abspath = strdup_temp(rp, &listing->allocator);

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

DirectoryListing* directory_listing_get(char const* directory_path, uint64_t start_tick)
{
  uint64_t key_hash = hash_bytes(directory_path, strlen(directory_path));
  DirectoryListing* listing = map_get(&g_directory_listings, key_hash);
  if (!listing || !directory_listing_same_dir(listing, directory_path))
  {
    if (listing)
    {
      directory_listing_free(listing);
      free(listing), listing = NULL;
    }
    listing = calloc(1, sizeof *listing);
    directory_listing_make(listing, directory_path);
    listing->start_tick = start_tick;
    map_put(&g_directory_listings, key_hash, listing);
  }
  return listing;
}

typedef struct MD2_UIList
{
  Map selection_indices_set; // all element indices belonging to the selection
  uint64_t anchor_index;
  MD2_Point2 press_point;
  MD2_Rect2 press_element_rect;
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
                                       char const* filename)
{
  NVGcontext* vg = md2_ui_vg(ui, element);
  nvgFillColor(vg, nvgRGBA(255, 255, 255, 255));
  nvgTextAlign(vg, NVG_ALIGN_BOTTOM);
  md2_ui_textf(ui, element, "%s", filename);
  nvgTextAlign(vg, NVG_ALIGN_BASELINE); // back to default
}

void ui_directory_listing(MD2_UserInterface* ui,
                          MD2_UIElement element,
                          MD2_UIList* list,
                          MD2_UIScrollableContent* scroller_state,
                          char const* directory_path)
{
  DirectoryListing const* listing =
    directory_listing_get(directory_path, ui->mu->time.ticks);
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
    for (size_t file_i = 0; file_i < listing->files_n; file_i++)
    {
      char const* filename = listing->files[file_i];
      md2_ui_region_add(&list_content,
                        (MD2_UIElement){.rect = {.x0 = 0.0,
                                                 .x1 = element.rect.x1 - element.rect.x0,
                                                 .y0 = row_y,
                                                 .y1 = row_y + font_size}},
                        filename);
      row_y += font_size;
    }
  }
  md2_ui_region_end(&list_content);

  MD2_UIElement file_list_scroller = element;
  file_list_scroller.rect.y1 -= default_font_size; // @todo status height
  file_list_scroller.rect = rect_make_valid(file_list_scroller.rect);
  // @todo we're not checking if the content escapes the element

  md2_ui_rect(ui, element, nvgRGBA(160, 160, 160, 128)); // debug
  nvgFillColor(vg, nvgRGBA(255, 255, 255, 255));
  md2_ui_scroller_start(ui, scroller_state, &list_content, file_list_scroller);
  if (listing_is_done)
  {
    for (size_t file_i = 0; file_i < listing->files_n; file_i++)
    {
      char const* filename = listing->files[file_i];
      MD2_UIElement list_element;
      if (md2_ui_scroller_get_element(ui, scroller_state, file_list_scroller,
                                      &list_content, filename, &list_element))
      {
        SelectionRange selection =
          ui_list_element_process_input(ui, list, list_element, file_i);
        bool element_is_selected =
          map_get(&list->selection_indices_set, hash_ptr(filename));
        if (element_is_selected)
        {
          md2_ui_rect(ui, list_element, nvgRGBA(160, 160, 160, 128));
        }
        ui_directory_listing_element_draw(ui, list_element, filename);
        if (list->in_drag_gesture && element_is_selected)
        {
          MD2_UIElement drag_element = list_element;
          drag_element.layer = 1;
          drag_element.rect = rect_translate(
            list_element.rect, vec_make(list->press_point, ui->pointer.position));
          ui_directory_listing_element_draw(ui, drag_element, filename);
        }
        if (rect_intersects(list_element.rect, ui->pointer.position))
        {
          md2_ui_rect(ui, list_element, nvgRGBA(255, 160, 160, 128));

          if (ui->mu->mouse.left_button.pressed)
          {
            list->press_point = ui->pointer.position; // @todo @ux project that point into
                                                      // the box, so that it's magnetized
                                                      // to it
            list->press_element_rect = list_element.rect;
          }

          if (ui->pointer.drag.started)
          {
            list->in_drag_gesture = true;
            assert(buf_len(ui->pointer.drag.payload_allocator.regions) == 0);
            if (!element_is_selected)
            {
              list->anchor_index = file_i;
              selection.op = SelectionRangeOp_Replace;
              selection.first_index = file_i;
              selection.last_index = file_i + 1;
            }
          }
        }
        if (selection.op == SelectionRangeOp_Replace)
        {
          map_free(&list->selection_indices_set);
        }
        for (size_t file_to_select_i = selection.first_index;
             file_to_select_i < selection.last_index; file_to_select_i++)
        {
          char const* filename = listing->files[file_to_select_i];
          map_put(&list->selection_indices_set, hash_ptr(filename), (void*)(intptr_t)1);
        }
      }
    }

    if (list->in_drag_gesture && ui->pointer.drag.started)
    {
      MD2_DragGesture* drag = &ui->pointer.drag;
      char** filepath_list = NULL;
      for (size_t file_i = 0; file_i < listing->files_n; file_i++)
      {
        char const* filename = listing->files[file_i];
        if (map_get(&list->selection_indices_set, hash_ptr(filename)))
        {
          buf_push(filepath_list, temp_memdup_range(&drag->payload_allocator, filename,
                                                    filename + strlen(filename) + 1));
        }
      }

      map_put(&drag->payload_by_type, MD2_PayloadType_FilePathList,
              temp_memdup_range(
                &drag->payload_allocator, filepath_list, buf_end(filepath_list)));
      buf_free(filepath_list);
    }
  }
  md2_ui_scroller_end(ui, scroller_state, &list_content, file_list_scroller);

  if (list->in_drag_gesture && ui->mu->mouse.left_button.released)
  {
    list->in_drag_gesture = false;
  }

  nvgFillColor(vg, nvgRGBA(255, 255, 255, 255));
  nvgTextAlign(vg, NVG_ALIGN_BOTTOM);
  nvgFontSize(vg, default_font_size);
  if (listing_is_done)
  {
    md2_ui_textf(
      ui, element, "Directory %s (Found %d)", listing->root_abspath, listing->files_n);
  }
  else
  {
    md2_ui_textf(ui, element, "Directory %s (Listing)", listing->root_abspath);
  }
  nvgTextAlign(vg, NVG_ALIGN_BASELINE); // back to default

  md2_ui_region_free(&list_content);
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

  LoadAudioTask** audiofile_tasks = NULL;
  if (!md1_song_path[0])
  {
    // @todo @defect @leak
    MD1_EntityCatalog entities = md1_song_load(md1_song_path);
    for (MD1_File *file_i = &entities.files[0], *file_l = &file_i[entities.files_n];
         file_i < file_l; file_i++)
    {
      LoadAudioTask* task_data = calloc(1, sizeof *task_data);
      task_data->filename = strdup_range(file_i->path, file_i->path_n);
      {
        // Require waveform data:
        WaveformData* wv = &task_data->ui_waveform;
        size_t n = MD2_UI_WAVEFORM_SIZE;
        wv->len_pot = n;
        wv->min = calloc(n, sizeof wv->min[0]);
        wv->max = calloc(n, sizeof wv->max[0]);
        wv->rms = calloc(n, sizeof wv->rms[0]);
      }
      task_start(task_create(load_md1_file, task_data));
      buf_push(audiofile_tasks, task_data);
    }
  }

  bool is_first_frame = true;
  while (Mu_Push(&mu), Mu_Pull(&mu))
  {
    TempAllocator temp_allocator = {0};
    float px_ratio = get_window_metrics(&mu).device_px_per_ref_points;
    glViewport(0, 0, mu.window.size.x, mu.window.size.y);
    glClearColor(0.5f, 0.0f, 1.0f, 0.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);

    ui.mu = &mu;
    ui.pixel_ratio = px_ratio;
    ui.size = (MD2_Vec2){
      .x = mu.window.size.x / ui.pixel_ratio, .y = mu.window.size.y / ui.pixel_ratio};
    md2_ui_update(&ui);

    size_t files_n = buf_len(audiofile_tasks);
    size_t loaded_n = 0;
    size_t bytes_n = 0;
    for (LoadAudioTask **task_i = &audiofile_tasks[0], **task_l = &task_i[files_n];
         task_i < task_l; task_i++)
    {
      LoadAudioTask* task = *task_i;
      if (!task->success)
        continue;
      loaded_n++;
      bytes_n +=
        task->audiobuffer.samples_count * task->audiobuffer.format.bytes_per_sample;
    }

    NVGcontext* vg = ui.vg;
    {
      nvgFontFace(vg, "fallback");
      float font_size = 16;
      nvgFontSize(vg, font_size);
      nvgFillColor(vg, nvgRGBA(255, 192, 0, 255));
      float row_y = font_size + 20;
      float col_x = 20;
      md2_ui_textf(&ui, (MD2_UIElement){.rect = {.x0 = col_x, .y1 = row_y}},
                   "User Library %s%s", user_library_path,
                   !posix_is_dir(user_library_path) ? " (offline)" : ""),
        row_y += font_size;
      md2_ui_textf(
        &ui, (MD2_UIElement){.rect = {.x0 = col_x, .y1 = row_y}}, "Audiofiles:"),
        row_y += font_size;

      col_x += 10;
      md2_ui_textf(&ui, (MD2_UIElement){.rect = {.x0 = col_x, .y1 = row_y}}, "%d / %d",
                   loaded_n, files_n),
        row_y += font_size;

      md2_ui_textf(&ui, (MD2_UIElement){.rect = {.x0 = col_x, .y1 = row_y}}, "%s bytes",
                   temp_str_nicenumber(bytes_n, &temp_allocator)),
        row_y += font_size;
      col_x -= 10;

      row_y += 10;

      MD2_Rect2 inner_bounds = rect_expand(ui.bounds, (MD2_Vec2){-10, -10});
      MD2_UIElement directory_listing_element = {
        .rect = rect_intersection(
          inner_bounds,
          (MD2_Rect2){
            .x0 = col_x, .x1 = col_x + 320, .y0 = row_y, .y1 = inner_bounds.y1}),
      };
      {
        static MD2_UIScrollableContent file_content = {0};
        static MD2_UIList file_list_state = {0};
        ui_directory_listing(&ui, directory_listing_element, &file_list_state,
                             &file_content, user_library_path);
      }

      col_x = directory_listing_element.rect.x1 + 10;
      {
        MD2_UIRegion audiofile_list_content = {0};
        {
          float row_y = 0.0;
          md2_ui_region_start(&audiofile_list_content);
          for (size_t loaded_i = 0; loaded_i < loaded_n; loaded_i++)
          {
            row_y += 10.0;
            LoadAudioTask* task = audiofile_tasks[loaded_i];
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

        MD2_UIElement scroller_element = {
          .rect = rect_translate_within(ui.bounds, audiofile_list_content.bounds,
                                        (MD2_Point2){col_x, row_y}, (MD2_Vec2){10, 10})};
        scroller_element.rect.x1 = max_f(
          min_f(scroller_element.rect.x0 + 40, ui.bounds.x1), scroller_element.rect.x1);
        scroller_element.rect.y1 = ui.bounds.y1 - 10;
        md2_ui_rect(&ui, scroller_element, nvgRGBA(128, 128, 128, 128));
        md2_ui_rect(&ui, scroller_element, nvgRGBA(160, 160, 160, 128)); // debug

        md2_ui_scroller_start(
          &ui, &audiofile_list, &audiofile_list_content, scroller_element);
        for (size_t loaded_i = 0; loaded_i < loaded_n; loaded_i++)
        {
          LoadAudioTask* task = audiofile_tasks[loaded_i];
          MD2_UIElement element;
          if (md2_ui_scroller_get_element(&ui, &audiofile_list, scroller_element,
                                          &audiofile_list_content, &task->ui_waveform,
                                          &element))
          {
            md2_ui_waveform(&ui, element, &task->ui_waveform);
          }
        }
        md2_ui_scroller_end(
          &ui, &audiofile_list, &audiofile_list_content, scroller_element);

        md2_ui_region_free(&audiofile_list_content);

        if (ui.pointer.drag.running
            && rect_intersects(scroller_element.rect, ui.pointer.position))
        {
          md2_ui_rect(&ui, scroller_element, nvgRGBA(128, 128, 128, 128));
        }
      }

      temp_allocator_free(&temp_allocator);
    }
    is_first_frame = false;
  }

  return 0;
}
