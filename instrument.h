#pragma once

#include <stdint.h>
#include <stdbool.h>

typedef struct voice {
    uint64_t phases[8];
    uint32_t phase_increment[8];
    float filter_state[16]; // Enough for a 8 band equalizer

    float start_time;
    float end_time; // If unknown, set to start_time + large value.
    float velocity;
    bool active;
} voice;

typedef void (*instrument_fn)(voice* v, float current_time, float time_per_sample, float* write_to, int num_samples, float* freq_modu, float freq_mod_effect);

void pad(voice* v, float current_time, float time_per_sample, float* write_to, int num_samples, float* freq_modu, float freq_mod_effect);
void bass(voice* v, float current_time, float time_per_sample, float* write_to, int num_samples, float* freq_modu, float freq_mod_effect);
void pluck(voice* v, float current_time, float time_per_sample, float* write_to, int num_samples, float* freq_modu, float freq_mod_effect);
void tri_bass(voice* v, float current_time, float time_per_sample, float* write_to, int num_samples, float* freq_modu, float freq_mod_effect);
void epiano(voice* v, float current_time, float time_per_sample, float* write_to, int num_samples, float* freq_modu, float freq_mod_effect);
void kick(voice* v, float current_time, float time_per_sample, float* write_to, int num_samples, float* freq_modu, float freq_mod_effect);

void voice_init(voice* v, instrument_fn instrument);
float instrument_note_to_frequency(instrument_fn instrument, int note);