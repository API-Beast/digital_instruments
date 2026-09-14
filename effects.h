#pragma once

#include <stdint.h>
#include <stdbool.h>

typedef struct reverb {
    float buffer[4096];  // Delay line buffer
    uint32_t write_pos;
    uint32_t read_pos[4]; // Multiple read positions for echoes
    float feedback;
    float wet_dry_mix;
} reverb;

typedef struct compressor {
    float envelope;
    float threshold;
    float ratio;
    float attack_time;
    float release_time;
} compressor;

void reverb_init(reverb* r, float sample_rate);
void reverb_process(reverb* r, float* samples, int num_samples);

void compressor_init(compressor* c);
void compressor_process(compressor* c, float* samples, int num_samples, float sample_rate);