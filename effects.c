#include "effects.h"
#include <math.h>

static inline float min(float a, float b) { return a < b ? a : b; }
static inline float max(float a, float b) { return a > b ? a : b; }
static inline float absf(float x) { return x < 0 ? -x : x; }

void reverb_init(reverb* r, float sample_rate)
{
    for(int i = 0; i < 4096; i++) r->buffer[i] = 0.0f;
    r->write_pos = 0;
    
    // Set read positions for different delay times (in samples)
    r->read_pos[0] = (uint32_t)(sample_rate * 0.03f);  // 30ms
    r->read_pos[1] = (uint32_t)(sample_rate * 0.037f); // 37ms  
    r->read_pos[2] = (uint32_t)(sample_rate * 0.043f); // 43ms
    r->read_pos[3] = (uint32_t)(sample_rate * 0.051f); // 51ms
    
    r->feedback = 0.7f;
    r->wet_dry_mix = 0.3f;
}

void reverb_process(reverb* r, float* samples, int num_samples)
{
    for(int i = 0; i < num_samples; i++)
    {
        float input = samples[i];
        float output = 0.0f;
        
        // Read from delay lines (multiple echoes)
        for(int j = 0; j < 4; j++)
        {
            output += r->buffer[(r->write_pos - r->read_pos[j]) & 4095] * 0.25f;
        }
        
        // Write input + feedback to delay line
        r->buffer[r->write_pos] = input + output * r->feedback;
        r->write_pos = (r->write_pos + 1) & 4095;
        
        // Mix dry and wet signals
        samples[i] = input * (1.0f - r->wet_dry_mix) + output * r->wet_dry_mix;
    }
}

void compressor_init(compressor* c)
{
    c->envelope = 0.0f;
    c->threshold = 0.5f;    // -6dB approx
    c->ratio = 4.0f;        // 4:1 ratio
    c->attack_time = 0.01f; // 10ms attack
    c->release_time = 0.1f; // 100ms release
}

void compressor_process(compressor* c, float* samples, int num_samples, float samplerate)
{
    float attack_coeff = expf(-1.0f / (c->attack_time * samplerate));
    float release_coeff = expf(-1.0f / (c->release_time * samplerate));
    
    for(int i = 0; i < num_samples; i++)
    {
        // Envelope follower
        float input_abs = absf(samples[i]);
        float coeff = (input_abs > c->envelope) ? attack_coeff : release_coeff;
        c->envelope = input_abs + coeff * (c->envelope - input_abs);
        
        // Gain computation
        float gain = 1.0f;
        if(c->envelope > c->threshold)
        {
            float over_threshold = c->envelope - c->threshold;
            float compression = over_threshold / c->ratio;
            gain = (c->threshold + compression) / c->envelope;
        }
        
        samples[i] *= gain;
    }
}