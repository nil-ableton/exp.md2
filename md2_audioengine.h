#ifndef MD2_AUDIOENGINE
#define MD2_AUDIOENGINE

typedef struct MD2_AudioTimeSync
{
  uint64_t tick;
  double beat;
  double beats_per_minute;
} MD2_AudioTimeSync;

typedef struct MD2_AudioTime
{
  uint64_t ticks;
  uint64_t microseconds;
  uint64_t samples;
  double seconds;
  double beats;
  double samples_per_second;
} MD2_AudioTime;

typedef struct MD2_Audio_Float2
{
  union
  {
    struct
    {
      float left, right;
    };
    float values[2];
  };
} MD2_Audio_Float2;

typedef struct MD2_Audio_Range
{
  size_t f, l;
} MD2_Audio_Range;

typedef struct MD2_Audio_StereoClipPlayer
{
  MD2_Audio_Float2* stereo_frames;
  size_t stereo_frames_n;
  MD2_Audio_Range loop;
  double phase_increment;
  double phase;
} MD2_Audio_StereoClipPlayer;

typedef struct MD2_AudioState
{
  float global_gain;
  MD2_AudioTime time;
  MD2_AudioTimeSync sync;

  MD2_Audio_StereoClipPlayer preview_clip;
  bool preview_clip_is_playing;
} MD2_AudioState;

struct MD2_AudioEngine;

struct MD2_AudioEngine* md2_audioengine_init(void);
void md2_audioengine_deinit(struct MD2_AudioEngine*);

void md2_audioengine_mu_audiocallback(struct MD2_AudioEngine*, struct Mu_AudioBuffer*);

// \pre must be called only from one thread at a time
void md2_audioengine_update(struct MD2_AudioEngine*, MD2_AudioState* audio_state);

#endif
