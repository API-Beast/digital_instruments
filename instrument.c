#include "instrument.h"
#include <SDL3/SDL_log.h>
#include <math.h>

static inline float saw(uint32_t phase)     { return -1.0f + (phase / (float)(UINT32_MAX)) * 2.0f; }
static inline float square(uint32_t phase)  { return -1.0f + (phase > (UINT32_MAX/2)) * 2.0f;  }
static inline float triangle(uint32_t phase)
{
    if(phase < UINT32_MAX/2)
        return saw(phase * 2);
    return -saw((phase - UINT32_MAX/2) * 2);
}

static inline float sine(uint32_t phase)
{
    float x = (phase / (float)UINT32_MAX) * 2.0f - 1.0f;
    
    // 11th-order polynomial approximation using Horner's method
    const float coeffs[] = {
        -3.1415926444234477f,   // x
         2.0261194642649887f,   // x^3
        -0.5240361513980939f,   // x^5
         0.0751872634325299f,   // x^7
        -0.006860187425683514f, // x^9
         0.000385937753182769f, // x^11
    };
    
    float x2 = x * x;
    float p11 = coeffs[5];
    float p9  = p11 * x2 + coeffs[4];
    float p7  = p9 * x2  + coeffs[3];
    float p5  = p7 * x2  + coeffs[2];
    float p3  = p5 * x2  + coeffs[1];
    float p1  = p3 * x2  + coeffs[0];
    
    return (x - 1.0f) * (x + 1.0f) * p1 * x;
}

static inline float rectified_sine(uint32_t phase)
{
    float s = sine(phase);
    return s < 0.0f ? -s : s; 
}

static inline uint32_t xxhash32(uint32_t x, uint32_t y, uint32_t z, uint32_t w)
{
    const uint32_t PRIME32_2 = 2246822519U, PRIME32_3 = 3266489917U;
	const uint32_t PRIME32_4 = 668265263U, PRIME32_5 = 374761393U;
	uint32_t h32 =  w + PRIME32_5 + x*PRIME32_3;
	h32 = PRIME32_4*((h32 << 17) | (h32 >> (32 - 17)));
	h32 += y * PRIME32_3;
	h32 = PRIME32_4*((h32 << 17) | (h32 >> (32 - 17)));
	h32 += z * PRIME32_3;
	h32 = PRIME32_4*((h32 << 17) | (h32 >> (32 - 17)));
    h32 = PRIME32_2*(h32^(h32 >> 15));
    h32 = PRIME32_3*(h32^(h32 >> 13));
    return h32^(h32 >> 16);
}

static inline float noise(uint32_t seed, int sample)
{
    static const int NUM_SAMPLES_PER_CYCLE = 4;
    return -1.0f + (xxhash32(seed, sample, 0, 0) / (float)(UINT32_MAX)) * 2.0f;
}

static inline float noise_osc(uint64_t phase, uint32_t seed)
{
    static const int NUM_SAMPLES_PER_CYCLE = 4;
    return -1.0f + (xxhash32(seed, phase >> 32, phase / (UINT32_MAX / NUM_SAMPLES_PER_CYCLE), 0) / (float)(UINT32_MAX)) * 2.0f;
}

static inline float min(float a, float b){ return a < b ? a : b; }

static inline float clamped_exp_ease_in(float a, float b, float f)
{
    if(f < 0.0f) f = 0.0f; else if(f > 1.0f) f = 1.0f;
    f = powf(2.0, 10.0f * f - 10.0f);
    return a * (1.0f - f) + b * f;
}

static inline float clamped_exp_ease_out(float a, float b, float f)
{
    if(f < 0.0f) f = 0.0f; else if(f > 1.0f) f = 1.0f;
    f = 1.0f - powf(2.0f, -10.0f * f);
    return a * (1.0f - f) + b * f;
}

static inline float clamped_lerp(float a, float b, float f)
{
    if(f < 0.0f) f = 0.0f; else if(f > 1.0f) f = 1.0f;
    return a * (1.0f - f) + b * f;
}

static inline float _envelope_attack(float time_since_press, float attack_time, float decay_time, float sustain)
{
    return clamped_lerp(0.0f, 1.0f, min(time_since_press / (attack_time + 0.00001f), 1.0f)) * clamped_lerp(1.0f, sustain, min((time_since_press - attack_time) / (decay_time + 0.00001f), 1.0f));
}

static inline float _envelope_release(float time_since_release, float release_time)
{
    return clamped_lerp(1.0f, 0.0f, min(time_since_release / (release_time + 0.00001f), 1.0f));
}

static inline float lowpass(float *low, float signal, float cutoff)
{
    *low = cutoff * signal + (1.0f - cutoff) * (*low);
    return *low;
}

static inline float highpass(float *state, float signal, float cutoff)
{
    lowpass(state, signal, cutoff);
    return signal - *state;
}

static inline float bandpass(float state[2], float signal, float center, float Q)
{   
    float bandwidth = center / Q;
    float factor = 2.0f * sin(M_PI * center);
    float damping = 1.0f / Q;
    
    // High pass = Signal - Last Midpass - Last Lowpass
    float hp = (signal - state[0] * (1.0f + damping * factor) - state[1]) / (1.0f + factor * damping + factor * factor);
    state[0] = factor * hp + state[0]; // Midpass moving average
    state[1] = factor * state[0] + state[1]; // Lowpass moving average
    return state[0];
}

static inline float equalizer(float signal, float state[16], float time_per_sample, float gains[8])
{
    const float center_freqs[] = {63.0f,   125.0f,  250.0f,  500.0f, 1000.0f, 2000.0f, 4000.0f, 8000.0f};
    
    float output = 0.0f;
    output +=  lowpass(&state[ 0], signal, center_freqs[0] * time_per_sample) * gains[0];
    output += bandpass(&state[ 1], signal, center_freqs[1] * time_per_sample, 1.0f) * gains[1];
    output += bandpass(&state[ 3], signal, center_freqs[2] * time_per_sample, 1.0f) * gains[2];
    output += bandpass(&state[ 5], signal, center_freqs[3] * time_per_sample, 1.0f) * gains[3];
    output += bandpass(&state[ 7], signal, center_freqs[4] * time_per_sample, 1.0f) * gains[4];
    output += bandpass(&state[ 9], signal, center_freqs[5] * time_per_sample, 1.0f) * gains[5];
    output += bandpass(&state[11], signal, center_freqs[6] * time_per_sample, 1.0f) * gains[6];
    output += highpass(&state[13], signal, center_freqs[7] * time_per_sample) * gains[7];
    return output;
}

void pad(voice* v, float current_time, float time_per_sample, float* write_to, int num_samples, float* freq_modu, float freq_mod_effect)
{
    float play_time = current_time - v->start_time;
    float release_time = current_time - v->end_time;
    for(int i = 0; i<num_samples; i++)
    {
        v->phases[0] += v->phase_increment[0] + (freq_modu ? (uint32_t)(freq_modu[i] * freq_mod_effect) * v->phase_increment[0] : 0);
        float sample_attack_time = play_time + time_per_sample * i;
        float sample_release_time = release_time + time_per_sample * i;
        write_to[i] = saw(v->phases[0]) * _envelope_attack(sample_attack_time, 0.5f, 0.2f, 0.6f) * _envelope_release(sample_release_time, 0.5f);
    }
    v->active = (release_time + time_per_sample * num_samples) < 0.5f;
}

void bass(voice* v, float current_time, float time_per_sample, float* write_to, int num_samples, float* freq_modu, float freq_mod_effect)
{
    float play_time = current_time - v->start_time;
    float release_time = current_time - v->end_time;
    for(int i = 0; i<num_samples; i++)
    {
        v->phases[0] += v->phase_increment[0] + (freq_modu ? (uint32_t)(freq_modu[i] * freq_mod_effect) * v->phase_increment[0] : 0);
        float sample_attack_time = play_time + time_per_sample * i;
        float sample_release_time = release_time + time_per_sample * i;
        write_to[i] = saw(v->phases[0]) * _envelope_attack(sample_attack_time, 0.0f, 0.05f, 0.5f) * _envelope_release(sample_release_time, 0.1f);
    }
    v->active = (release_time + time_per_sample * num_samples) < 0.1f;
}

void tri_bass(voice* v, float current_time, float time_per_sample, float* write_to, int num_samples, float* freq_modu, float freq_mod_effect)
{
    float play_time = current_time - v->start_time;
    float release_time = current_time - v->end_time;
    for(int i = 0; i<num_samples; i++)
    {
        v->phases[0] += v->phase_increment[0] + (freq_modu ? (uint32_t)(freq_modu[i] * freq_mod_effect) * v->phase_increment[0] : 0);
        float sample_attack_time = play_time + time_per_sample * i;
        float sample_release_time = release_time + time_per_sample * i;
        write_to[i] = triangle(v->phases[0]) * _envelope_attack(sample_attack_time, 0.0f, 0.05f, 0.5f) * _envelope_release(sample_release_time, 0.1f);
    }
    v->active = (release_time + time_per_sample * num_samples) < 0.1f;
}

void pluck(voice* v, float current_time, float time_per_sample, float* write_to, int num_samples, float* freq_modu, float freq_mod_effect)
{
    float play_time = current_time - v->start_time;
    float release_time = current_time - v->end_time;
    for(int i = 0; i<num_samples; i++)
    {
        v->phases[0] += v->phase_increment[0] + (freq_modu ? (uint32_t)(freq_modu[i] * freq_mod_effect) * v->phase_increment[0] : 0);
        float sample_attack_time = play_time + time_per_sample * i;
        float sample_release_time = release_time + time_per_sample * i;
        write_to[i] = square(v->phases[0]) * _envelope_attack(sample_attack_time, 0.0f, 0.05f, 0.5f) * _envelope_release(sample_release_time, 0.1f);
    }
    v->active = (release_time + time_per_sample * num_samples) < 0.1f;
}

void epiano(voice* v, float current_time, float time_per_sample, float* write_to, int num_samples, float* freq_modu, float freq_mod_effect)
{
    float play_time = current_time - v->start_time;
    float release_time = current_time - v->end_time;
    for(int i = 0; i<num_samples; i++)
    {
        v->phases[0] += v->phase_increment[0] + (freq_modu ? (uint32_t)(freq_modu[i] * freq_mod_effect) * v->phase_increment[0] : 0);
        v->phases[1] += v->phase_increment[1] + (freq_modu ? (uint32_t)(freq_modu[i] * freq_mod_effect) * v->phase_increment[1] : 0);
        v->phases[2] += v->phase_increment[2] + (freq_modu ? (uint32_t)(freq_modu[i] * freq_mod_effect) * v->phase_increment[2] : 0);
        float sample_attack_time = play_time + time_per_sample * i;
        float sample_release_time = release_time + time_per_sample * i;
        float osc1 = triangle(v->phases[0]);
        float osc2 = triangle(v->phases[1]) * 0.5f;
        float osc3 = sine(v->phases[2]) * 0.3f;
        write_to[i] = (osc1 + osc2 + osc3) * _envelope_attack(sample_attack_time, 0.01f, 0.1f, 0.3f) * _envelope_release(sample_release_time, 0.4f);
    }
    v->active = (release_time + time_per_sample * num_samples) < 0.4f;
}

void kick(voice* v, float current_time, float time_per_sample, float* write_to, int num_samples, float* freq_modu, float freq_mod_effect)
{
    float play_time = current_time - v->start_time;
    float release_time = current_time - v->end_time;
    
    for(int i = 0; i < num_samples; i++)
    {
        float sample_time = play_time + time_per_sample * i;
        float sample_release_time = release_time + time_per_sample * i;
        
        float noise_env = clamped_exp_ease_out(1.0f, 0.0f, sample_time / 0.5f);
        float noise = noise_osc(v->phases[1], 42) * noise_env;
        
        float pitch_env = clamped_exp_ease_out(1.0f, 0.0f, sample_time / 0.08f);
        float amp_env = clamped_exp_ease_out(1.0f, 0.0f, sample_time / 0.6f);
        float release_env = _envelope_release(sample_release_time, 0.5f);
        
        v->phases[0] += v->phase_increment[0] * (1.0f + noise * 6.0f) * (1.0f + pitch_env * 12.0f);
        v->phases[1] += v->phase_increment[0] * 64.0f;
        float signal = (sine(v->phases[0]) * 0.85f + sine(v->phases[0] / 2.0f) * 0.40f + noise_osc(v->phases[0] * 20.0f, 7734) * 0.1f) * amp_env * 3.0f * release_env;
        
        write_to[i] = equalizer(signal, &v->filter_state[0], time_per_sample, (float[8]){4.5f, 6.0f, 3.0f, 1.0f, 0.5f, 0.8f, 1.5f, 0.0f});
    }
    v->active = (release_time + time_per_sample * num_samples) < 0.2f;
}

void voice_init(voice* v, instrument_fn instrument)
{
    if(instrument == epiano)
    {
        v->phase_increment[1] = v->phase_increment[0] * 1.005f;
        v->phase_increment[2] = v->phase_increment[0] * 0.995f;
    }
}

float instrument_note_to_frequency(instrument_fn instrument, int note)
{
    // "Atonal" instruments, limited pitch variation.
    if(instrument == kick) return 55.0 * pow(2, note/48.0);
    // Bass instruments. 2 octaves lower.
    if(instrument == bass || instrument == tri_bass) return 110.0 * pow(2, note/12.0);
    return 440.0 * pow(2, note/12.0);
}