#pragma once

struct instr_string
{
    // 44100Hz sample rate / 4096 samples = 10.7 Hz lowest possible pitch.
    // You can increase this if you need even lower pitched strings than that.
    float displacement[4096];
    float velocity[4096];
    // How many elements of the delay line we actually use, determining the fundamental pitch of the instr_string.
    int string_length; // = Sample Rate / Frequency
    int current_offset;

    float filter_state[16]; // Enough for a 8 band equalizer
};

void model_string_vibration(instr_string* str, float* write_to, int num_samples)
{

}