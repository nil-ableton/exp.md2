#ifndef MD2_AUDIO
#define MD2_AUDIO

typedef struct WaveformData
{
  float* min;
  float* max;
  float* rms;
  size_t len_pot;
} WaveformData;

void audiobuffer_compute_waveform(struct Mu_AudioBuffer* audiobuffer,
                                  WaveformData* d_waveform);

#endif
