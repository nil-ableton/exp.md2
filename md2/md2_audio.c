#include "md2_audio.h"

#include "md2_math.h"

#include "libs/xxxx_mu.h"

void audiobuffer_compute_waveform(struct Mu_AudioBuffer* audiobuffer,
                                  WaveformData* d_waveform)
{
  assert(d_waveform->min);
  assert(d_waveform->max);
  assert(d_waveform->rms);
  assert(d_waveform->len_pot);
  size_t n_pot = d_waveform->len_pot;
  size_t chan_n = audiobuffer->format.channels;
  size_t frame_n = audiobuffer->samples_count / chan_n;
  size_t chunk_size = round_up_multiple_of_pot_uintptr(frame_n, n_pot) / n_pot;

  size_t s_frame_i = 0;
  size_t s_frame_n = frame_n;
  for (size_t chunk_index = 0; chunk_index < n_pot; chunk_index++)
  {
    float* d_min = &d_waveform->min[chunk_index];
    float* d_max = &d_waveform->max[chunk_index];
    float* d_rms = &d_waveform->rms[chunk_index];
    *d_min = min_f_unit();
    *d_max = max_f_unit();
    *d_rms = 0.0f;

    size_t s_frame_chunk_l = s_frame_i + chunk_size;
    for (; s_frame_i < s_frame_n && s_frame_i < s_frame_chunk_l; s_frame_i++)
    {
      for (size_t chan_i = 0; chan_i < chan_n; chan_i++)
      {
        float x = audiobuffer->samples[s_frame_i * chan_n + chan_i] / 32268.0f;
        *d_min = min_f(*d_min, x);
        *d_max = max_f(*d_max, x);
        *d_rms += x * x;
      }
    }
    for (; s_frame_i < s_frame_chunk_l; s_frame_i++)
    {
      *d_min = min_f(*d_min, 0.0f);
      *d_max = max_f(*d_min, 0.0f);
      *d_rms += 0.0f;
    }
    *d_rms /= (chunk_size * chan_n);
  }
  assert(s_frame_i >= s_frame_n);
}
