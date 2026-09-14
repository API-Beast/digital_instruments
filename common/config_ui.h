#include <SDL3/SDL_render.h>
#include "stb_easy_font.h"
#include <stdio.h>

/* EXAMPLE
#define CONFIG_LIST\
INT_SLIDER(bpm, 60, 180, 5)\
BOOL(step_recording)\
INT_LIST(step, 1, 2, 4, 8, 16)\
INT_LIST(instrument, EPiano, Bass, Violin, Kick, HiHat)\
INT(current_track, 0, 10)\
BOOL(record)\
BOOL(play)\
FLOAT_SLIDER(volume, 0.0f, 1.0f, 0.01f)
*/

#define FOREACH_STRINGIFY_1(a)       #a
#define FOREACH_STRINGIFY_2(a, ...)  #a, FOREACH_STRINGIFY_1(__VA_ARGS__)
#define FOREACH_STRINGIFY_3(a, ...)  #a, FOREACH_STRINGIFY_2(__VA_ARGS__)
#define FOREACH_STRINGIFY_4(a, ...)  #a, FOREACH_STRINGIFY_3(__VA_ARGS__)
#define FOREACH_STRINGIFY_5(a, ...)  #a, FOREACH_STRINGIFY_4(__VA_ARGS__)
#define FOREACH_STRINGIFY_6(a, ...)  #a, FOREACH_STRINGIFY_5(__VA_ARGS__)
#define FOREACH_STRINGIFY_7(a, ...)  #a, FOREACH_STRINGIFY_6(__VA_ARGS__)
#define FOREACH_STRINGIFY_8(a, ...)  #a, FOREACH_STRINGIFY_7(__VA_ARGS__)
#define FOREACH_STRINGIFY_9(a, ...)  #a, FOREACH_STRINGIFY_8(__VA_ARGS__)
#define FOREACH_STRINGIFY_N(_9,_8,_7,_6,_5,_4,_3,_2,_1,N,...) \
        FOREACH_STRINGIFY##N
#define FOREACH_STRINGIFY(...)  \
        FOREACH_STRINGIFY_N(__VA_ARGS__,_9,_8,_7,_6,_5,_4,_3,_2,_1) \
        (__VA_ARGS__)



typedef struct config_data config_data;
struct config_data
{
    #define BOOL(name, default) bool name;
    #define INT_SLIDER(name, default, min, max, period) int name;
    #define INT_LIST(name, default, ...) int name;
    #define INT(name, default, min, max) int name;
    #define FLOAT_SLIDER(name, default, min, max, period, power) float name;
    #define HEADER(label)

    CONFIG_LIST

    #undef BOOL
    #undef INT_SLIDER
    #undef INT_LIST
    #undef INT
    #undef FLOAT_SLIDER
    #undef HEADER
    #define BOOL(name, default) bool name ## _has_changed;
    #define INT_SLIDER(name, default, min, max, period) bool name ## _has_changed;
    #define INT_LIST(name, default, ...) bool name ## _has_changed;
    #define INT(name, default, min, max) bool name ## _has_changed;
    #define FLOAT_SLIDER(name, default, min, max, period, power) bool name ## _has_changed;
    #define HEADER(label)

    bool any_changed;
    CONFIG_LIST
};

#undef BOOL
#undef INT_SLIDER
#undef INT_LIST
#undef INT
#undef FLOAT_SLIDER
#undef HEADER

void draw_text(SDL_Renderer *renderer, float x, float y, char* text, unsigned char color[4]) {
    char buffer[99999];
    y += 3.0f;
    int num_quads = stb_easy_font_print(x, y, text, color, buffer, sizeof(buffer));
    for(int q = 0; q < num_quads; q++) {
        SDL_Vertex verts[4];
        for(int v = 0; v < 4; v++) {
            char* ptr = buffer + (q * 4 + v) * 16;
            float vx = *(float*)(ptr);
            float vy = *(float*)(ptr + 4);
            uint8_t r = ptr[12], g = ptr[13], b = ptr[14], a = ptr[15];
            verts[v].position = (SDL_FPoint){vx, vy};
            verts[v].color = (SDL_FColor){r / 255.0f, g / 255.0f, b / 255.0f, a / 255.0f};
            verts[v].tex_coord = (SDL_FPoint){0, 0};
        }
        int indices[6] = {0, 1, 2, 0, 2, 3};
        SDL_RenderGeometry(renderer, NULL, verts, 4, indices, 6);
    }
}

static inline void config_init(config_data* c)
{   
    *c = (config_data){};
    c->any_changed = false;

    /* Redefine the macros for initialization */
    #define BOOL(name, default) c->name = default;
    #define INT_SLIDER(name, default, min, max, period) c->name = default;
    #define INT_LIST(name, default, ...) c->name = default;
    #define INT(name, default, min, max) c->name = default;
    #define FLOAT_SLIDER(name, default, min, max, period, power) c->name = default;
    #define HEADER(label) // Header: label

    /* Expand the CONFIG_LIST with our initialization macros */
    CONFIG_LIST

    /* Clean up the macros */
    #undef BOOL
    #undef INT_SLIDER
    #undef INT_LIST
    #undef INT
    #undef FLOAT_SLIDER
    #undef HEADER
}

static inline void config_reset_modified(config_data* c)
{   
    /* Redefine the macros for initialization */
    #define BOOL(name, default) c->name ## _has_changed = false;
    #define INT_SLIDER(name, default, min, max, period) c->name ## _has_changed = false;
    #define INT_LIST(name, default, ...) c->name ## _has_changed = false;
    #define INT(name, default, min, max) c->name ## _has_changed = false;
    #define FLOAT_SLIDER(name, default, min, max, period, power) c->name ## _has_changed = false;
    #define HEADER(label) // Header: label

    /* Expand the CONFIG_LIST with our initialization macros */
    c->any_changed = false;
    CONFIG_LIST

    /* Clean up the macros */
    #undef BOOL
    #undef INT_SLIDER
    #undef INT_LIST
    #undef INT
    #undef FLOAT_SLIDER
    #undef HEADER
}

static inline void config_render_ui(SDL_Renderer *r, config_data* c, float x, float y, float w, float h)
{
    void render_slider(float current_y, float factor)
    {
        SDL_SetRenderDrawColor(r, 200, 200, 200, 255);
        SDL_RenderFillRect(r, &(SDL_FRect){x, current_y, factor * w, 12});
    };

    int y_offset = 0;
    int y_increment = 16;

    /* fill the area with off white */
    SDL_SetRenderDrawColor(r, 240, 240, 240, 255);
    SDL_RenderFillRect(r, &(SDL_FRect){x, y, w, h});

    /* Redefine the macros for rendering */
    #define BOOL(name, default) \
        {\
            float current_y = y + y_offset; \
            if (current_y >= y && current_y <= y + h) { \
                /* render small empty black box */ \
                SDL_SetRenderDrawColor(r, 0, 0, 0, 255); \
                SDL_RenderRect(r, &(SDL_FRect){x + 5, current_y, 12, 12}); \
                /* if enabled, render green box inside */ \
                if (c->name) { \
                    SDL_SetRenderDrawColor(r, 0, 255, 0, 255); \
                    SDL_RenderFillRect(r, &(SDL_FRect){x + 6, current_y + 1, 10, 10}); \
                } \
                /* render label */ \
                char bool_text[64]; \
                snprintf(bool_text, sizeof(bool_text), "%s: %s", #name, c->name ? "ON" : "OFF"); \
                draw_text(r, x + 20, current_y, bool_text, (unsigned char[]){0, 0, 0, 255}); \
            } \
            y_offset += y_increment; \
        }

    #define INT_SLIDER(name, default, min, max, period) \
        {\
            float current_y = y + y_offset; \
            if (current_y >= y && current_y <= y + h) { \
                render_slider(current_y, (float)(c->name - min) / (max - min));\
                /* render text */ \
                char slider_text[64]; \
                snprintf(slider_text, sizeof(slider_text), "%s: %d", #name, c->name); \
                SDL_SetRenderDrawColor(r, 0, 0, 0, 255); \
                draw_text(r, x + 5, current_y, slider_text, (unsigned char[]){0, 0, 0, 255}); \
            } \
            y_offset += y_increment; \
        }

    #define RENDER_PREV_NEXT_BUTTONS(label, value) \
        /* Calculate positions for right alignment */ \
        float button_group_width = 20 + 60 + 20; /* < button (20) + value (60) + > button (20) */ \
        float button_group_x = x + w - button_group_width - 5; /* 5 pixels from right edge */ \
        /* render name on the left */ \
        draw_text(r, x + 5, current_y, label, (unsigned char[]){0, 0, 0, 255}); \
        /* render < button */ \
        SDL_SetRenderDrawColor(r, 180, 180, 180, 255); \
        SDL_RenderRect(r, &(SDL_FRect){button_group_x, current_y, 20, 14}); \
        draw_text(r, button_group_x + 5, current_y, "<", (unsigned char[]){0, 0, 0, 255}); \
        /* render current value */ \
        float text_width = stb_easy_font_width(value); \
        draw_text(r, button_group_x + 20 + 30 - (text_width/2), current_y, value, (unsigned char[]){0, 0, 0, 255}); \
        /* render > button */ \
        SDL_RenderRect(r, &(SDL_FRect){button_group_x + 20 + 60, current_y, 20, 14}); \
        draw_text(r, button_group_x + 20 + 60 + 5, current_y, ">", (unsigned char[]){0, 0, 0, 255})

    #define INT_LIST(name, default, ...) \
        {\
            float current_y = y + y_offset; \
            if (current_y >= y && current_y <= y + h) { \
                /* get the list of values from variadic macro */ \
                int values[] = {__VA_ARGS__}; \
                char* names[] = {FOREACH_STRINGIFY(__VA_ARGS__)};\
                int count = sizeof(values) / sizeof(values[0]); \
                /* find current index */ \
                int current_index = 0; \
                for (int i = 0; i < count; i++) { \
                    if (values[i] == c->name) { \
                        current_index = i; \
                        break; \
                    } \
                } \
                /* render prev/next buttons */ \
                RENDER_PREV_NEXT_BUTTONS(#name, names[current_index]); \
            } \
            y_offset += y_increment; \
        }

    #define INT(name, default, min, max) \
        {\
            float current_y = y + y_offset; \
            if (current_y >= y && current_y <= y + h) { \
                /* render current value */ \
                char value_text[32]; \
                snprintf(value_text, sizeof(value_text), "%d", c->name); \
                /* render prev/next buttons */ \
                RENDER_PREV_NEXT_BUTTONS(#name, value_text); \
            } \
            y_offset += y_increment; \
        }

    #define FLOAT_SLIDER(name, default, min, max, period, power) \
        {\
            float current_y = y + y_offset; \
            if (current_y >= y && current_y <= y + h) { \
                render_slider(current_y, powf((float)(c->name - min) / (max - min), 1.0f/power));\
                /* render text */ \
                char slider_text[64]; \
                snprintf(slider_text, sizeof(slider_text), "%s: %f", #name, c->name); \
                SDL_SetRenderDrawColor(r, 0, 0, 0, 255); \
                draw_text(r, x + 5, current_y, slider_text, (unsigned char[]){0, 0, 0, 255}); \
            } \
            y_offset += y_increment; \
        }

    #define HEADER(label) \
    { \
        if(y_offset != 0) y_offset += y_increment; \
        float current_y = y + y_offset; \
        draw_text(r, x + 5, current_y,  "# "label, (unsigned char[]){0, 0, 0, 255}); \
        y_offset += y_increment; \
    }

    /* Expand the CONFIG_LIST with our rendering macros */
    CONFIG_LIST

    /* Clean up the macros */
    #undef BOOL
    #undef INT_SLIDER
    #undef INT_LIST
    #undef INT
    #undef FLOAT_SLIDER
    #undef HEADER
}


static inline bool config_handle_input(SDL_Event* e, config_data* c, float x, float y, float w, float h)
{
    int mod(int a, int b)
    {
        int r = a % b;
        return r < 0 ? r + b : r;
    }

    float clamp(float val, float min, float max)
    {
        if(val < min) return min;
        if(val > max) return max;
        return val;
    }

    int id = 0;
    static int current_focus = -1;

    if(e->type == SDL_EVENT_MOUSE_BUTTON_UP)
    {
        current_focus = -1;
    }

    if (e->type != SDL_EVENT_MOUSE_BUTTON_DOWN && e->type != SDL_EVENT_MOUSE_MOTION)
    {
        return false;
    }
    float mouse_x = e->button.x;
    float mouse_y = e->button.y;
    bool is_press = true;
    if(e->type == SDL_EVENT_MOUSE_MOTION)
    {
        mouse_x = e->motion.x;
        mouse_y = e->motion.y;
        if((e->motion.state & SDL_BUTTON_LMASK) == 0)
            return false;
        is_press = false;
    }
    else if(e->type == SDL_EVENT_MOUSE_BUTTON_DOWN)
    {
        is_press = true;
        current_focus = -1;
    }
    
    // Check if click is within the config panel
    if (current_focus == -1)
    if (mouse_x < x || mouse_x > x + w || mouse_y < y || mouse_y > y + h) {
        return false;
    }
    
    int y_offset = 0;
    int y_increment = 16;
    bool handled = false;

    float handle_slider(float min, float max, float period, float power)
    {
        float factor = powf(clamp((mouse_x - x) / w, 0.0f, 1.0f), 1.0f / power);
        if(factor >= 0.0f && factor <= 1.0f)
        {
            float value = min + factor * (max - min);
            /* Quantize to period */
            if (period > 0) {
                float steps = (max - min) / period;
                value = min + roundf((value - min) / period) * period;
            }
            handled = true;
            return value;
        }
        return min;
    }

    #define BOOL(name, default) \
        {\
            id++; \
            float current_y = y + y_offset; \
            if (mouse_y >= current_y && mouse_y <= current_y + y_increment && is_press) { \
                /* Check if click is within the checkbox area */ \
                if (mouse_x >= x + 5 && mouse_x <= x + 17) { \
                    c->name = !c->name; \
                    c->name ## _has_changed = true; \
                    c->any_changed = true; \
                    handled = true; \
                } \
            } \
            y_offset += y_increment; \
        }

    #define INT_SLIDER(name, default, min, max, period) \
        {\
            id++; \
            float current_y = y + y_offset; \
            if (current_focus == id || current_focus == -1) \
            if ((mouse_y >= current_y && mouse_y <= current_y + y_increment) || current_focus == id) { \
                current_focus = id; \
                float val = handle_slider(min, max, period, 1.0f); \
                if(handled)\
                { \
                    c->name = val; \
                    c->name ## _has_changed = true;\
                    c->any_changed = true; \
                } \
            } \
            y_offset += y_increment; \
        }

    #define HANDLE_PREV_NEXT_BUTTONS(name, prev_val, next_val) \
    { \
        id++; \
        float button_group_width = 20 + 60 + 20; \
        float button_group_x = x + w - button_group_width - 5; \
        float left_button_x = button_group_x; \
        float right_button_x = button_group_x + 20 + 60; \
        /* Check left button */ \
        if (mouse_x >= left_button_x && mouse_x <= left_button_x + 20 && \
            mouse_y >= current_y && mouse_y <= current_y + 14) { \
            c->name = prev_val; \
            c->name ## _has_changed = true;\
            c->any_changed = true; \
            handled = true; \
        } \
        /* Check right button */ \
        else if (mouse_x >= right_button_x && mouse_x <= right_button_x + 20 && \
                mouse_y >= current_y && mouse_y <= current_y + 14) { \
            c->name = next_val; \
            c->name ## _has_changed = true;\
            c->any_changed = true; \
            handled = true; \
        } \
    }

    #define INT_LIST(name, default, ...) \
        {\
            id++; \
            float current_y = y + y_offset; \
            if (mouse_y >= current_y && mouse_y <= current_y + y_increment && is_press) { \
                int values[] = {__VA_ARGS__}; \
                int count = sizeof(values) / sizeof(values[0]); \
                /* Find current index */ \
                int current_index = 0; \
                for (int i = 0; i < count; i++) { \
                    if (values[i] == c->name) { \
                        current_index = i; \
                        break; \
                    } \
                } \
                HANDLE_PREV_NEXT_BUTTONS(name, values[mod(current_index-1, count)], values[mod(current_index-1, count)]) \
            } \
            y_offset += y_increment; \
        }

    #define INT(name, default, min, max) \
        {\
            id++; \
            float current_y = y + y_offset; \
            if (mouse_y >= current_y && mouse_y <= current_y + y_increment && is_press) { \
                HANDLE_PREV_NEXT_BUTTONS(name, min + mod(c->name - min - 1, max - min), min + mod(c->name - min + 1, max - min)) \
            } \
            y_offset += y_increment; \
        }

    #define FLOAT_SLIDER(name, default, min, max, period, power) \
        {\
            id++; \
            float current_y = y + y_offset; \
            if (current_focus == id || current_focus == -1) \
            if ((mouse_y >= current_y && mouse_y <= current_y + y_increment) || current_focus == id) { \
                current_focus = id; \
                float val = handle_slider(min, max, period, 1.0f/power); \
                if(handled)\
                { \
                    c->name = val; \
                    c->name ## _has_changed = true; \
                    c->any_changed = true; \
                } \
            } \
            y_offset += y_increment; \
        }

    #define HEADER(label) \
    { \
        id++; \
        if(y_offset != 0) y_offset += y_increment; \
        y_offset += y_increment; \
    }

    /* Expand the CONFIG_LIST with our input handling macros */
    CONFIG_LIST

    /* Clean up the macros */
    #undef BOOL
    #undef INT_SLIDER
    #undef INT_LIST
    #undef INT
    #undef FLOAT_SLIDER
    #undef HEADER

    return handled;
}