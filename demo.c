#define SDL_MAIN_USE_CALLBACKS

#include <SDL3/SDL_init.h>
#include <SDL3/SDL_surface.h>
#include <SDL3/SDL_timer.h>
#include <SDL3/SDL_main.h>
#include <SDL3/SDL_audio.h>
#include <SDL3/SDL_log.h>
#include <SDL3/SDL_video.h>
#include <SDL3/SDL_render.h>
#include "instrument.h"
#include "effects.h"

#include <math.h>
#include <stdio.h>

typedef struct voice_ext voice_ext;
struct voice_ext
{
    struct voice;

    SDL_Scancode   input_id_key;
    SDL_KeyboardID input_id_device;
    instrument_fn instrument;
};

voice_ext voices[16] = {};

bool is_recording;
typedef struct track track;
struct track
{
    float record_time;
    float play_time;
    voice notes[256];
    int note_index;
    bool is_playing;
};

track tracks[10];

enum
{
    EPiano,
    Bass,
    TriBass,
    Kick,
    Pad,
};


#define CONFIG_LIST \
HEADER("General") \
INT_LIST(instrument, EPiano, EPiano, Bass, TriBass, Kick, Pad) \
FLOAT_SLIDER(volume, 1.0f, 0.0f, 1.0f, 0.01f, 1.0f)


#include "common/config_ui.h"

config_data config;

reverb r;
compressor c;

SDL_Scancode grid[4][10] = {
{SDL_SCANCODE_1, SDL_SCANCODE_2, SDL_SCANCODE_3, SDL_SCANCODE_4, SDL_SCANCODE_5, SDL_SCANCODE_6, SDL_SCANCODE_7, SDL_SCANCODE_8, SDL_SCANCODE_9, SDL_SCANCODE_0},
{SDL_SCANCODE_Q, SDL_SCANCODE_W, SDL_SCANCODE_E, SDL_SCANCODE_R, SDL_SCANCODE_T, SDL_SCANCODE_Y, SDL_SCANCODE_U, SDL_SCANCODE_I, SDL_SCANCODE_O, SDL_SCANCODE_P},
{SDL_SCANCODE_A, SDL_SCANCODE_S, SDL_SCANCODE_D, SDL_SCANCODE_F, SDL_SCANCODE_G, SDL_SCANCODE_H, SDL_SCANCODE_J, SDL_SCANCODE_K, SDL_SCANCODE_L, SDL_SCANCODE_SEMICOLON},
{SDL_SCANCODE_Z, SDL_SCANCODE_X, SDL_SCANCODE_C, SDL_SCANCODE_V, SDL_SCANCODE_B, SDL_SCANCODE_N, SDL_SCANCODE_M, SDL_SCANCODE_COMMA, SDL_SCANCODE_PERIOD, 0}
};

uint64_t previous_processed_samples = 0;
uint64_t previous_block_start_time_ns = 0;
uint64_t processed_samples = 0;
int   samplerate = 44100;
SDL_Renderer *renderer;
float samples[512] = {};

static instrument_fn instruments[] = {epiano, bass, tri_bass, kick, pad};

void NoteOn(int note, SDL_Scancode code, SDL_KeyboardID keyboard_id, uint64_t timestamp_ns, float velocity)
{
    // Find inactive voice
    int v = -1;
    for(int i = 0; i < 16; i++)
    if(voices[i].active == false)
    {
        v = i;
        break;
    }

    // If no voice is inactive, chose the voice that is furthest along it's release envelope.
    float first_end_time = -1e9;
    if(v == -1)
    for(int i = 0; i < 16; i++)
    {
        if(voices[i].end_time < first_end_time)
        {
            v = i;
            first_end_time = voices[i].end_time;
        }
    }

    // Fallback
    if(v == -1) v = code % 16;

    float last_block_start_time = (previous_processed_samples + 1) / (double)(samplerate);
    uint64_t offset_to_last_block_start_ns = timestamp_ns - previous_block_start_time_ns;
    float time = last_block_start_time + 128.0f / samplerate;

    voices[v] = (voice_ext){};
    voices[v].instrument      = instruments[config.instrument];
    voices[v].phase_increment[0] = (uint32_t)(instrument_note_to_frequency(voices[v].instrument, note) * UINT32_MAX / samplerate);
    voices[v].active          = true;
    voices[v].start_time      = time;
    voices[v].end_time        = time + 1e3;
    voices[v].input_id_key    = code;
    voices[v].input_id_device = keyboard_id;
    voices[v].velocity        = velocity * config.volume;
    voice_init((voice*)&voices[v], voices[v].instrument);
}

void NoteOff(int note, SDL_Scancode code, SDL_KeyboardID keyboard_id, uint64_t timestamp_ns)
{
    float last_block_start_time = (previous_processed_samples + 1) / (double)(samplerate);
    uint64_t offset_to_last_block_start_ns = timestamp_ns - previous_block_start_time_ns;
    float time = last_block_start_time + 128.0f / samplerate;
    for(int i = 15; i >= 0; i--)
    {
        if(voices[i].active == true && voices[i].input_id_key == code && voices[i].input_id_device == keyboard_id)
            voices[i].end_time = time;
    }
}

static inline void ProcessVoices(float* buffer, int samples)
{
    memset(buffer, 0, samples * sizeof(float));
    float time_per_sample = 1.0f / (float)samplerate;
    float start_time = processed_samples / (float)samplerate;
    float end_time = (processed_samples + samples) / (float)samplerate;
    for(int i = 0; i < 16; i++)
    {
        if(!voices[i].active || voices[i].instrument == NULL || voices[i].start_time > end_time)
            continue;

        float tmp[samples] = {};
        voices[i].instrument((voice*)&voices[i], start_time, time_per_sample, tmp, samples, NULL, 0.0f);
        for(int j = 0; j < samples; j++)
            buffer[j] += tmp[j] * voices[i].velocity;
    } 
    previous_processed_samples = processed_samples;
    processed_samples += samples;
}

void SDLCALL AudioPostmix_UpdateOffset(void *userdata, const SDL_AudioSpec *spec, float *buffer, int buflen)
{
    previous_block_start_time_ns = SDL_GetTicksNS();

    // Capture the last 512 samples for oscilloscope display
    int num_samples = buflen / sizeof(float);
    if (num_samples >= 512) {
        memcpy(samples, buffer + (num_samples - 512), 512 * sizeof(float));
    }
}

void SDL_ProcessAudio(void *userdata, SDL_AudioStream *stream, int additional_amount, int total_amount)
{
    int num_samples = additional_amount / sizeof(float);
    float buffer[num_samples];
    ProcessVoices(buffer, num_samples);
    reverb_process(&r, buffer, num_samples);
    compressor_process(&c, buffer, num_samples, samplerate);
    SDL_PutAudioStreamData(stream, buffer, additional_amount);
}

SDL_AudioStream *stream;
SDL_Window *window;

void draw_hexagon(SDL_Renderer *renderer, float cx, float cy, float r) {
    for(int i = 0; i < 6; i++) {
        float angle1 = (i * 60 + 30) * M_PI / 180.0f;
        float angle2 = ((i + 1) * 60 + 30) * M_PI / 180.0f;
        int x1 = (int)(cx + r * cosf(angle1));
        int y1 = (int)(cy + r * sinf(angle1));
        int x2 = (int)(cx + r * cosf(angle2));
        int y2 = (int)(cy + r * sinf(angle2));
        SDL_RenderLine(renderer, x1, y1, x2, y2);
    }
}

static inline int grid_to_note(int x, int y)
{
    return 7*(x-4) + 4*y - 12 * (x/4);
}

const char* frequency_to_note(double freq) {
    static const char* notes[] = {"C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"};
    static char buffer[32];
    double semitones = 12.0 * log2(freq / 440.0);
    int nearest = (int)round(semitones);
    double cents_offset = 100.0 * (semitones - nearest);

    int note_num = 69 + nearest;
    int pitch_class = (note_num % 12 + 12) % 12;
    int octave = note_num / 12 - 1;
    const char* note_name = notes[pitch_class];

    if (fabs(cents_offset) > 10.0) {
        int cents = (int)round(cents_offset);
        sprintf(buffer, "%s%d%s%dc", note_name, octave, cents >= 0 ? "+" : "-", abs(cents));
    } else {
        sprintf(buffer, "%s%d", note_name, octave);
    }

    return buffer;
}

char* human_readable_note(int midi) {
    static char buf[5];
    int octave = (midi / 12) - 1;
    int note = midi % 12;
    const char* names[] = {"C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"};
    sprintf(buf, "%s%d", names[note], octave);
    //sprintf(buf, "%d", midi);
    return buf;
}

SDL_AppResult SDL_AppInit(void** appstate, int argc, char* argv[])
{
    SDL_AudioSpec spec = {.freq = samplerate, .format = SDL_AUDIO_F32, .channels = 1};

    SDL_Init(SDL_INIT_AUDIO | SDL_INIT_VIDEO);

    reverb_init(&r, samplerate);
    compressor_init(&c);
    config_init(&config);

    // Create window for oscilloscope display
    window = SDL_CreateWindow("Synthesizer", 900, 500, 0);
    if (!window) {
        SDL_Log("Failed to create window: %s", SDL_GetError());
        return SDL_APP_FAILURE;
    }
    renderer = SDL_CreateRenderer(window, NULL);
    if (!renderer) {
        SDL_Log("Failed to create renderer: %s", SDL_GetError());
        return SDL_APP_FAILURE;
    }
    stream = SDL_OpenAudioDeviceStream(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK,  &spec, SDL_ProcessAudio, NULL);

    if(stream)
    {
        SDL_AudioDeviceID audio_device = SDL_GetAudioStreamDevice(stream);
        SDL_SetAudioPostmixCallback(audio_device, AudioPostmix_UpdateOffset, NULL);
        SDL_ResumeAudioStreamDevice(stream);
    }
    else
    {
        SDL_Log("Failed to open audio stream: %s", SDL_GetError());
        return SDL_APP_FAILURE;
    }

    return SDL_APP_CONTINUE;
}

SDL_AppResult SDL_AppIterate(void* appstate)
{
    // Clear the screen
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
    SDL_RenderClear(renderer);

    // Draw the waveform
    SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
    for (int i = 0; i < 511; i++) {
        int x1 = i * 700 / 512;
        int y1 = 150 + (int)(samples[i] * 150.0f);
        int x2 = (i + 1) * 700 / 512;
        int y2 = 150 + (int)(samples[i + 1] * 150.0f);
        SDL_RenderLine(renderer, x1, y1, x2, y2);
    }

    // Draw hexagonal keyboard grid
    float dy = 200.0f / 4.0f;
    float r = dy / 1.4f - 2.0f;
    float dx = r * 1.732f + 2.0f; // sqrt(3)
    float time = processed_samples / (float)samplerate;
    for(int y = 0; y < 4; y++) {
        for(int x = 0; x < 10; x++) {
            SDL_Scancode sc = grid[y][x];
            if(sc == 0) continue;
            float cx = 50.0f + x * dx + (y % 2) * dx * 0.5f + (y / 2) * dx;
            float cy = 300.0f + y * dy;
            bool is_active = false;
            for(int i = 0; i < 16; i++) {
                if(voices[i].active && time < voices[i].end_time && voices[i].input_id_key == sc) {
                    is_active = true;
                    break;
                }
            }
            if(is_active) {
                SDL_SetRenderDrawColor(renderer, 255, 0, 0, 255);
            } else {
                SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
            }
            draw_hexagon(renderer, cx, cy, r - 2.0f);

            // Draw MIDI note text
            int note = grid_to_note(x, y);
            float freq = instrument_note_to_frequency(instruments[config.instrument], note);
            char* note_str = frequency_to_note(freq);
            int width = stb_easy_font_width(note_str);
            float text_x = cx - width / 2.0f;
            float text_y = cy - 6.0f;
            draw_text(renderer, text_x, text_y, note_str, (unsigned char[]){255, 255, 255, 255});
        }
    }

    config_render_ui(renderer, &config, 700, 0, 200, 500);

    // Present the rendered frame
    SDL_RenderPresent(renderer);
    config_reset_modified(&config);
    return SDL_APP_CONTINUE;
}

SDL_AppResult SDL_AppEvent(void* appstate, SDL_Event* event)
{
    if(config_handle_input(event, &config, 700, 0, 200, 500))
        return SDL_APP_CONTINUE;
    if(event->type == SDL_EVENT_QUIT || event->type ==  SDL_EVENT_WINDOW_CLOSE_REQUESTED || event->type == SDL_EVENT_WINDOW_DESTROYED)
        return SDL_APP_SUCCESS;
    if(event->type == SDL_EVENT_KEY_DOWN || event->type == SDL_EVENT_KEY_UP)
    {
        if(event->key.repeat) return 0;
        int x = 0; int y = 0;
        switch(event->key.scancode)
        {
            case SDL_SCANCODE_1: x = 0; y = 0; break;
            case SDL_SCANCODE_2: x = 1; y = 0; break;
            case SDL_SCANCODE_3: x = 2; y = 0; break;
            case SDL_SCANCODE_4: x = 3; y = 0; break;
            case SDL_SCANCODE_5: x = 4; y = 0; break;
            case SDL_SCANCODE_6: x = 5; y = 0; break;
            case SDL_SCANCODE_7: x = 6; y = 0; break;
            case SDL_SCANCODE_8: x = 7; y = 0; break;
            case SDL_SCANCODE_9: x = 8; y = 0; break;
            case SDL_SCANCODE_0: x = 9; y = 0; break;

            case SDL_SCANCODE_Q: x = 0; y = 1; break;
            case SDL_SCANCODE_W: x = 1; y = 1; break;
            case SDL_SCANCODE_E: x = 2; y = 1; break;
            case SDL_SCANCODE_R: x = 3; y = 1; break;
            case SDL_SCANCODE_T: x = 4; y = 1; break;
            case SDL_SCANCODE_Y: x = 5; y = 1; break;
            case SDL_SCANCODE_U: x = 6; y = 1; break;
            case SDL_SCANCODE_I: x = 7; y = 1; break;
            case SDL_SCANCODE_O: x = 8; y = 1; break;
            case SDL_SCANCODE_P: x = 9; y = 1; break;

            case SDL_SCANCODE_A: x = 0; y = 2; break;
            case SDL_SCANCODE_S: x = 1; y = 2; break;
            case SDL_SCANCODE_D: x = 2; y = 2; break;
            case SDL_SCANCODE_F: x = 3; y = 2; break;
            case SDL_SCANCODE_G: x = 4; y = 2; break;
            case SDL_SCANCODE_H: x = 5; y = 2; break;
            case SDL_SCANCODE_J: x = 6; y = 2; break;
            case SDL_SCANCODE_K: x = 7; y = 2; break;
            case SDL_SCANCODE_L: x = 8; y = 2; break;
            case SDL_SCANCODE_SEMICOLON: x = 9; y = 2; break;
        
            case SDL_SCANCODE_Z: x = 0; y = 3; break;
            case SDL_SCANCODE_X: x = 1; y = 3; break;
            case SDL_SCANCODE_C: x = 2; y = 3; break;
            case SDL_SCANCODE_V: x = 3; y = 3; break;
            case SDL_SCANCODE_B: x = 4; y = 3; break;
            case SDL_SCANCODE_N: x = 5; y = 3; break;
            case SDL_SCANCODE_M: x = 6; y = 3; break;
            case SDL_SCANCODE_COMMA: x = 7; y = 3; break;
            case SDL_SCANCODE_PERIOD: x = 8; y = 3; break;

            default: return SDL_APP_CONTINUE;
        }
        int note = grid_to_note(x, y);
        if(event->key.down) {
            float velocity = (SDL_GetModState() & (SDL_KMOD_LSHIFT | SDL_KMOD_RSHIFT | SDL_KMOD_CAPS)) ? 1.0f : 0.65f;
            NoteOn(note, event->key.scancode, event->key.which, event->key.timestamp, velocity);
        }
        else NoteOff(note, event->key.scancode, event->key.which, event->key.timestamp);
    }
    return SDL_APP_CONTINUE;
}

void SDL_AppQuit(void* appstate, SDL_AppResult result)
{
    if(renderer) SDL_DestroyRenderer(renderer);
    if(window) SDL_DestroyWindow(window);
    SDL_Quit();
}
