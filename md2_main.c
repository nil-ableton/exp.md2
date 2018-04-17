#include "md1_support.h"

#include "libs/xxxx_buf.h"
#include "libs/xxxx_iobuffer.h"
#include "libs/xxxx_map.h"

#include <assert.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int test_buf(int, char const**);
int test_map(int, char const**);
int test_iobuffer(int, char const**);
int test_serialisation(int argc, char const** argv);

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

int main(int argc, char const* argv[])
{
  test_buf(argc, argv);
  test_map(argc, argv);
  test_iobuffer(argc, argv);
  test_serialisation(argc, argv);
  test_main(argc, argv);
  return 0;
}
