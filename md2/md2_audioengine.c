#include "md2_audioengine.h"

// @todo remove all memory allocations from audio engine

#include "md2_temp_allocator.h"

#include "libs/xxxx_buf.h"
#include "libs/xxxx_map.h"
#include "libs/xxxx_mu.h"
#include "libs/xxxx_queue.h"

#include <assert.h>
#include <math.h>
#include <string.h>

// @note: must be copyiable
typedef struct MD2_AudioEngineState
{
  struct Mu_AudioFormat output_format;
  MD2_AudioState client_state;
} MD2_AudioEngineState;

typedef struct MD2_AudioEngine
{
  MD2_AudioEngineState* state; // current
  double preview_clip_phase;

  QueueWriter to_client_writer;
  QueueReader from_client_reader;
  Queue to_client_queue;
  Queue from_client_queue;
  Map entities;

  // Client
  MD2_AudioEngineState nursery[2];
  uint32_t engine_nursery_index;

  QueueReader client_reader;
  QueueWriter client_writer;
} MD2_AudioEngine;

struct MD2_AudioEngine* md2_audioengine_init()
{
  MD2_AudioEngine* engine = calloc(1, sizeof *engine);
  engine->state = &engine->nursery[0];
  engine->to_client_writer.queue = &engine->to_client_queue;
  engine->client_reader.queue = &engine->to_client_queue;
  engine->from_client_reader.queue = &engine->from_client_queue;
  engine->client_writer.queue = &engine->from_client_queue;

  map_grow(&engine->entities, 1024);

  return engine;
}

void md2_audioengine_deinit(struct MD2_AudioEngine* engine)
{
  queue_reader_free(&engine->client_reader);
  queue_writer_free(&engine->client_writer);
  queue_writer_free(&engine->to_client_writer);
  queue_free(&engine->to_client_queue);
  queue_reader_free(&engine->from_client_reader);
  queue_free(&engine->from_client_queue);
  free(engine);
}

static md2_audioengine__push_to_client(MD2_AudioEngine* engine)
{
  QueueWriter* writer = &engine->to_client_writer;
  queue_flush(writer);
  if (buf_len(writer->writer_buffer) > 0)
  {
    for (void **d_msg_i = &writer->writer_buffer[0], **d_msg_l = buf_end(d_msg_i);
         d_msg_i < d_msg_l; d_msg_i++)
    {
      *d_msg_i = engine->state;
    }
  }
  else
  {
    queue_push(writer, engine->state);
  }
}

static void md2_audioengine__pull_from_client(MD2_AudioEngine* engine,
                                              struct Mu_AudioBuffer* output)
{
  for (void* src; src = queue_pull_next(&engine->from_client_reader);)
  {
    engine->state = src;
  }
}


static void clip_player_mixdown(MD2_Audio_StereoClipPlayer const* player,
                                double* phase_ptr,
                                struct Mu_AudioBuffer* output)
{
  double phase = *phase_ptr;
  int16_t* d_frame = &output->samples[0];
  for (size_t output_frame_i = 0,
              output_frame_n = output->samples_count / output->format.channels;
       output_frame_i < output_frame_n;
       output_frame_i++, phase += player->phase_increment)
  {
    if (player->stereo_frames_n == 0)
    {
      d_frame += output->format.channels;
      continue;
    }
    phase = fmod(phase, 1.0);
    while (phase < 0.0)
      phase += 1.0;
    double input_frame_i =
      fmod(interpolate_linear_f(0.0f, player->stereo_frames_n, phase),
           player->stereo_frames_n);
    assert(input_frame_i >= 0);
    assert(input_frame_i < player->stereo_frames_n);

    for (size_t c = 0, c_n = output->format.channels; c < c_n; c++)
    {
      if (c < 2)
      {
        d_frame[0] = 8000 * player->stereo_frames[(size_t)input_frame_i].values[c];
      }
      d_frame++;
    }
  }
  assert(d_frame == output->samples + output->samples_count);
  *phase_ptr = phase;
}

static void reference_tone_n(int16_t* stereo_frames, int frame_count)
{
  float const reference_hz = 1000;
  float const reference_amp = 0.5;
  static double phase;

  double phase_delta = reference_hz / 44100.0;
  for (int i = 0; i < frame_count; ++i)
  {
    float y = reference_amp * sin(2 * 3.141592 * phase);
    stereo_frames[2 * i] = stereo_frames[2 * i + 1] = 8000 * y;
    phase += phase_delta;
  }
}

void md2_audioengine_mu_audiocallback(struct MD2_AudioEngine* engine,
                                      struct Mu_AudioBuffer* output)
{
  md2_audioengine__pull_from_client(engine, output);
  engine->state->output_format = output->format;

  memset(&output->samples[0], 0, output->samples_count * sizeof output->samples[0]);
  if (engine->state->client_state.preview_clip_is_playing)
  {
    assert(output->format.channels == 2);
    reference_tone_n(
      &output->samples[0], output->samples_count / output->format.channels);
    clip_player_mixdown(
      &engine->state->client_state.preview_clip, &engine->preview_clip_phase, output);
    engine->state->client_state.preview_clip.phase = engine->preview_clip_phase;
  }
  md2_audioengine__push_to_client(engine);
}

void md2_audioengine_update(struct MD2_AudioEngine* engine, MD2_AudioState* audio_state)
{
  bool engine_state_updated = false;
  for (void* src; src = queue_pull_next(&engine->client_reader);)
  {
    MD2_AudioEngineState* engine_state_ptr = src;
    audio_state->preview_clip.phase = engine_state_ptr->client_state.preview_clip.phase;
    audio_state->time.samples_per_second =
      engine_state_ptr->output_format.samples_per_second;
    engine->engine_nursery_index = engine_state_ptr - &engine->nursery[0];
  }

  MD2_AudioEngineState* next_state = &engine->nursery[1 - engine->engine_nursery_index];
  next_state->client_state = *audio_state;
  queue_push(&engine->client_writer, next_state);
}
