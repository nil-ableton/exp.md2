#include "md1_support.h"

#include "libs/xxxx_buf.h"
#include "libs/xxxx_iobuffer.h"

#include <limits.h>
#include <stdarg.h>
#include <stdlib.h>

static bool loader_error(MD1_Loader* loader, char* fmt, ...)
{
  if (!loader->error)
  {
    va_list args;
    va_start(args, fmt);
    size_t n = vsnprintf(loader->error_buffer, sizeof loader->error_buffer, fmt, args);
    if (n < sizeof loader->error_buffer - 1)
    {
      loader->error_text = loader->error_buffer;
    }
    else
    {
      assert(0);
      loader->error = "truncated error";
    }
    va_end(args);
    loader->error = true;
  }
  return false;
}

static void* loader_calloc(MD1_Loader* loader, size_t n, size_t elem_size)
{
  void* ptr = calloc(n, elem_size);
  if (!ptr)
  {
    return loader_error(
             loader, "could not allocate %llu elements of size %llu\n", n, elem_size),
           NULL;
  }
  return ptr;
}

#define LOADER_PROLOGUE(loader, version_f, version_l)                                    \
  if (loader->error || loader->version < version_f || loader->version >= version_l)      \
    return !loader->error;

static bool load_uint8_n(
  MD1_Loader* loader, int field_version_f, int field_version_l, uint8_t* dest, size_t n)
{
  LOADER_PROLOGUE(loader, field_version_f, field_version_l);
  if (!read_uint8_n(loader->input, dest, n))
  {
    return loader_error(loader, "expected %d uint8", n);
  }
  return !loader->error;
}

static bool load_uint8(MD1_Loader* loader,
                       int field_version_f,
                       int field_version_l,
                       uint8_t* dest)
{
  return load_uint8_n(loader, field_version_f, field_version_l, dest, 1);
}

static bool load_uint16(MD1_Loader* loader,
                        int field_version_f,
                        int field_version_l,
                        uint16_t* dest)
{
  LOADER_PROLOGUE(loader, field_version_f, field_version_l);
  if (!read_uint16(loader->input, dest))
  {
    return loader_error(loader, "expected uint16");
  }
  return !loader->error;
}

static bool load_uint64(MD1_Loader* loader,
                        int field_version_f,
                        int field_version_l,
                        uint64_t* dest)
{
  LOADER_PROLOGUE(loader, field_version_f, field_version_l);
  if (!read_uint64(loader->input, dest))
  {
    return loader_error(loader, "expected uint64");
  }
  return !loader->error;
}

static bool load_float32(MD1_Loader* loader,
                         int field_version_f,
                         int field_version_l,
                         float* dest)
{
  LOADER_PROLOGUE(loader, field_version_f, field_version_l);
  if (!read_float32(loader->input, dest))
    return loader_error(loader, "expected float32");
  return !loader->error;
}

static bool load_str(MD1_Loader* loader,
                     int field_version_f,
                     int field_version_l,
                     char** d_chars,
                     size_t* d_chars_n)
{
  LOADER_PROLOGUE(loader, field_version_f, field_version_l);
  uint64_t len;
  load_uint64(loader, field_version_f, field_version_l, &len);
  uint8_t* chars = loader_calloc(loader, 1, len + 1);
  if (!load_uint8_n(loader, field_version_f, field_version_l, chars, len))
  {
    free(chars), chars = NULL;
    d_chars_n = 0;
  }
  *d_chars = chars;
  *d_chars_n = len;
  return !loader->error;
}

static bool expect_str_n(MD1_Loader* loader,
                         int field_version_f,
                         int field_version_l,
                         char const* string,
                         size_t n)
{
  LOADER_PROLOGUE(loader, field_version_f, field_version_l);
  IOBuffer* in = loader->input;
  for (uint8_t y; !loader->error && n && read_uint8(loader->input, &y); n--, string++)
  {
    if (y != (uint8_t)*string)
      return loader_error(loader, "expected char %c got %c", *string, y);
  }
  if (n > 0)
    return loader_error(loader, "expected string: %s, abruptly ended", string);
  return !loader->error;
}

static bool load_uint64_array(MD1_Loader* loader,
                              int field_version_f,
                              int field_version_l,
                              uint64_t** array_ptr,
                              size_t* array_n_ptr)
{
  uint64_t values_n;
  if (!load_uint64(loader, field_version_f, field_version_l, &values_n))
    return loader_error(loader, "expected array count");

  uint64_t* values = loader_calloc(loader, values_n, sizeof values[0]);
  for (size_t i = 0; !loader->error && i < values_n; i++)
  {
    load_uint64(loader, 1, INT_MAX, &values[i]);
  }
  if (loader->error)
  {
    free(values), values = NULL;
  }
  else
  {
    *array_ptr = values;
    *array_n_ptr = values_n;
  }
  return !loader->error;
}

bool md1_load_song(MD1_Loader* loader, MD1_Song* d_song, uint64_t song_id)
{
  MD1_Song song = {
    .id = song_id,
  };
  expect_str_n(loader, 1, INT_MAX, "SONG\0\0\0\0", 8);
  load_uint16(loader, 1, INT_MAX, &song.scene_n);
  load_uint64_array(loader, 1, INT_MAX, &song.audioclip_ids, &song.audioclip_ids_n);
  load_uint64_array(loader, 1, INT_MAX, &song.audiotracks_ids, &song.audiotracks_ids_n);
  if (!loader->error)
  {
    *d_song = song;
  }
  return !loader->error;
}


bool md1_load_songtrack(MD1_Loader* loader,
                        MD1_SongTrack* d_songtrack,
                        uint64_t songtrack_id)
{
  MD1_SongTrack songtrack = {
    .id = songtrack_id,
  };
  load_uint64_array(loader, 1, INT_MAX, &songtrack.audioclip_id_in_session_slots,
                    &songtrack.audioclip_id_in_session_slots_n);
  load_uint64(loader, 1, INT_MAX, &songtrack.playing_session_slot_index);
  if (songtrack.playing_session_slot_index > songtrack.audioclip_id_in_session_slots_n)
    return loader_error(loader, "unexpected playing_session_slot_index %d",
                        songtrack.playing_session_slot_index);
  if (!loader->error)
  {
    *d_songtrack = songtrack;
  }
  return !loader->error;
}

bool md1_load_audioclip(MD1_Loader* loader, MD1_AudioClip* d_clip, uint64_t clip_id)
{
  MD1_AudioClip clip = {
    .id = clip_id,
  };
  load_uint64(loader, 1, INT_MAX, &clip.file_id);
  uint8_t warp_mode = MD1_ClipWarpMode_Repitch;
  load_uint8(loader, MD1_FileVersion_AddClipWarpMode, INT_MAX, &warp_mode);
  clip.clip_warp_mode = (MD1_ClipWarpMode)warp_mode;
  float duration_in_bars = 1.0;
  load_float32(loader, MD1_FileVersion_AddClipDurationInBars, INT_MAX, &duration_in_bars);
  clip.duration_in_bars = duration_in_bars;
  if (!loader->error)
  {
    *d_clip = clip;
  }
  return !loader->error;
}

bool md1_load_file(MD1_Loader* loader, MD1_File* d_file, uint64_t file_id)
{
  MD1_File file = {
    .id = file_id,
  };
  load_str(loader, 1, INT_MAX, &file.path, &file.path_n);
  *d_file = file;
  return !loader->error;
}

bool md1_load(MD1_Loader* loader, MD1_EntityCatalog* dest)
{
  IOBuffer* in = loader->input;
  loader->version = 1;
  expect_str_n(loader, 1, INT_MAX, "ABL", 3);
  expect_str_n(loader, 1, INT_MAX, "MINI", 4);
  uint64_t version = 0;
  read_uint64(in, &version);
  if (version < MD1_FileVersion_First || version >= MD1_FileVersionLast)
    return loader_error(loader, "unknown version %d", version);

  loader->version = version;
  /* files */ {
    expect_str_n(loader, 1, INT_MAX, "XHTP\0\0\0\0", 8);
    if (load_uint64(loader, 1, INT_MAX, &dest->files_n))
    {
      MD1_File* files = loader_calloc(loader, dest->files_n, sizeof files[0]);
      for (size_t file_i = 0; !loader->error && file_i < dest->files_n; ++file_i)
      {
        md1_load_file(loader, &files[file_i], file_i + 1);
      }
      if (loader->error)
      {
        free(files), files = NULL;
      }
      dest->files = files;
    }
  }
  /* clips */ {
    expect_str_n(loader, 1, INT_MAX, "XPLC\0\0\0\0", 8);
    if (load_uint64(loader, 1, INT_MAX, &dest->audioclips_n))
    {
      MD1_AudioClip* audioclips =
        loader_calloc(loader, dest->audioclips_n, sizeof audioclips[0]);
      for (size_t clip_i = 0; !loader->error && clip_i < dest->audioclips_n; ++clip_i)
      {
        md1_load_audioclip(loader, &audioclips[clip_i], clip_i + 1);
      }
      if (loader->error)
      {
        free(audioclips), audioclips = NULL;
      }
      dest->audioclips = audioclips;
    }
  }
  /* songs */ {
    expect_str_n(loader, 1, INT_MAX, "XGNS\0\0\0\0", 8);
    if (load_uint64(loader, 1, INT_MAX, &dest->songs_n))
    {
      MD1_Song* songs = loader_calloc(loader, dest->songs_n, sizeof songs[0]);
      for (size_t song_i = 0; !loader->error && song_i < dest->songs_n; ++song_i)
      {
        md1_load_song(loader, &songs[song_i], song_i + 1);
      }
      if (loader->error)
      {
        free(songs), songs = NULL;
      }
      dest->songs = songs;
    }
  }
  /* song tracks */ {
    expect_str_n(loader, 1, INT_MAX, "TRCK\0\0\0\0", 8);
    if (load_uint64(loader, 1, INT_MAX, &dest->songtracks_n))
    {
      MD1_SongTrack* songtracks =
        loader_calloc(loader, dest->songtracks_n, sizeof songtracks[0]);
      for (size_t track_i = 0; !loader->error && track_i < dest->songtracks_n; track_i++)
      {
        md1_load_songtrack(loader, &songtracks[track_i], track_i + 1);
      }
      if (loader->error)
      {
        free(songtracks), songtracks = NULL;
      }
      dest->songtracks = songtracks;
    }
  }
  if (in->bytes_i != in->bytes_l)
  {
    return loader_error(loader, "expected EOF");
  }
  return !loader->error;
}

  // @todo how to do garbage collection of MD1 documents?

#undef LOADER_PROLOGUE
