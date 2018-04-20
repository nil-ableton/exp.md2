#ifndef MD1_SUPPORT
#define MD1_SUPPORT

#include <stdbool.h>
#include <stdint.h>

typedef struct MD1_Song
{
  uint64_t id;
  uint64_t* audioclip_ids;
  size_t audioclip_ids_n;
  uint16_t scene_n;
  uint64_t* audiotracks_ids;
  size_t audiotracks_ids_n;
} MD1_Song;

typedef struct MD1_SongTrack
{
  uint64_t id;
  uint64_t* audioclip_id_in_session_slots;
  size_t audioclip_id_in_session_slots_n;
  uint64_t playing_session_slot_index;
} MD1_SongTrack;

typedef enum MD1_ClipWarpMode {
  MD1_ClipWarpMode_None = 0,
  MD1_ClipWarpMode_Repitch = 1,
} MD1_ClipWarpMode;

typedef struct MD1_AudioClip
{
  uint64_t id;
  uint64_t file_id;
  MD1_ClipWarpMode clip_warp_mode;
  float duration_in_bars;
} MD1_AudioClip;

typedef struct MD1_File
{
  uint64_t id;
  char* path;
  size_t path_n;
} MD1_File;

enum
{
  MD1_FileVersion_None = 0,
  MD1_FileVersion_First = 1,
  MD1_FileVersion_AddClipWarpMode,
  MD1_FileVersion_AddClipDurationInBars,
  MD1_FileVersionLast,
  MD1_FileVersionCurrent = MD1_FileVersionLast - 1,
};

typedef struct MD1_EntityCatalog
{
  MD1_Song* songs;
  size_t songs_n;
  MD1_SongTrack* songtracks;
  size_t songtracks_n;
  MD1_AudioClip* audioclips;
  size_t audioclips_n;
  MD1_File* files;
  size_t files_n;
} MD1_EntityCatalog;

typedef struct MD1_Loader
{
  uint64_t version;
  struct IOBuffer* input;
  bool error;
  char* error_text;
  char error_buffer[256];
} MD1_Loader;

bool md1_load(MD1_Loader* loader, MD1_EntityCatalog* dest);
void md1_free(MD1_EntityCatalog* catalog);

#endif
